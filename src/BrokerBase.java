package net.colm.monitor;

import java.io.InputStream;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.concurrent.BlockingQueue;
import java.io.IOException;
import java.net.SocketTimeoutException;
import java.security.KeyManagementException;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.UnrecoverableKeyException;
import javax.net.ssl.TrustManagerFactory;
import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.SSLSocketFactory;
import java.util.Iterator;

import static java.lang.System.arraycopy;

class Packet
{
	int msgId;
	int totalLen;

	ArrayList<byte[]> blockList = new ArrayList<byte[]>();

	void clear()
	{
		blockList.clear();
	}

	byte[] allocate( int sz )
	{
		byte[] b = new byte[sz];
		blockList.add( b );
		return b;
	}
}

abstract class BrokerBase
{
	SSLSocketFactory factory = null;
	SSLSocket socket = null;
	RecordCallback callback = null;

	static final int WANT_HEAD = 101;
	static final int WANT_BLOCK = 102;
	static final String LOG_TAG = "parse-NT";

	static final int SIZEOF_PACKET_BLOCK_HEADER = 4;
	static final int SIZEOF_PACKET_HEADER = 12;

	static final int OFF_FIRST_LEN = SIZEOF_PACKET_BLOCK_HEADER + 0;
	static final int OFF_TOTAL_LEN = SIZEOF_PACKET_BLOCK_HEADER + 4;
	static final int OFF_MSG_ID    = SIZEOF_PACKET_BLOCK_HEADER + 8;

	protected int networkType;
	public static final int BRING_UP_WIFI    = 1;
	public static final int BRING_UP_MOBILE  = 2;
	public static final int BRING_UP_UNKNOWN = 3;
	public static final int BRING_DOWN = 4;

	private int state = WANT_HEAD;
	private int have = 0;
	private int need = 0;

	Packet packet = new Packet();

	private byte[] headBuf = new byte[ SIZEOF_PACKET_BLOCK_HEADER + SIZEOF_PACKET_HEADER ];
	private byte[] data;

	boolean connected() { return socket != null; }

	abstract void dispatch( Packet packet );
	abstract void timeout();

	static class PacketPos
	{
		byte[] element;
		ByteBuffer block;
		int offset;
	}

	static PacketPos pktFind( Packet packet, int pos )
	{
		// log_debug( DBG_PACKET, \"pkt find: \" << l );
		if ( pos == 0 )
			return null;

		Iterator<byte[]> itr = packet.blockList.iterator();
		while ( itr.hasNext() ) {
			byte[] element = itr.next();
			long avail = element.length;
			if ( pos < avail ) {
				PacketPos result = new PacketPos();
				result.element = element;
				result.block = ByteBuffer.wrap(element);
				result.block.order( ByteOrder.LITTLE_ENDIAN );
				result.offset = pos;
				return result;
			}
			pos -= avail;
		}
		return null;
	}

	static int packetStrLen( PacketPos pos )
	{
		int len = 0;
		for ( int i = pos.offset; i < pos.element.length && pos.element[i] != 0; i++ )
			len++;
		return len;
	}

	int read( byte[] b, int off, int len )
	{
		int result = -1;

		try {
			InputStream input = socket.getInputStream();
			result = input.read( b, off, len );
		}
		catch ( SocketTimeoutException e ) {
			result = 0;
		}
		catch ( IOException e )
		{
			result = -1;
		}

		return result;
	}

	public void parse( )
	{
		// Log.i( LOG_TAG, "parsing packet" );

		switch ( state ) {
			case WANT_HEAD: {
				final int sz = SIZEOF_PACKET_BLOCK_HEADER + SIZEOF_PACKET_HEADER;
				int len = read( headBuf, have, sz - have );
				if ( len < 0 ) {
					/* EOF. */
					// Log.i( LOG_TAG, "packet head read: closed" );
					bringDownSocket();
					return;
				}
				else if ( len == 0 ) {
					// Log.i( LOG_TAG, "packet head read: delayed" );

					timeout();

					return;
				}
				else if ( have + len < sz )  {
					// Log.i( LOG_TAG, "packet head read: is short: got "
					//	+ Integer.valueOf(len).toString() + " bytes" );

					/* Don't have it all. Need to wait for more. */
					have += len;
					return;
				}

				/* Completed read of header. */
				// Log.i( LOG_TAG, "packet: have first block headers: " + Integer.valueOf(len) );

				ByteBuffer bbHeadBuf = ByteBuffer.wrap(headBuf);
				bbHeadBuf.order( ByteOrder.LITTLE_ENDIAN );

				packet.msgId    = bbHeadBuf.getInt( OFF_MSG_ID );
				packet.totalLen = bbHeadBuf.getInt( OFF_TOTAL_LEN );

				/* Pull the size of the first block length from the header read so far. */
				// recv.head = (PacketHeader*)(recv.headBuf + sizeof(PacketBlockHeader));
				int firstLen = bbHeadBuf.getInt( OFF_FIRST_LEN );
				need = firstLen;

				/* Allocate the first block and move the header data in from the
				 * temp read space. */
				data = packet.allocate( firstLen );
				arraycopy( headBuf, 0, data, 0, sz );

				/* Reset the head pointer to the header we just coppied in. */
				// recv.head = (PacketHeader*)(recv.data + sizeof(PacketBlockHeader));

				/* Indicate we have the headers and enter into block read loop. */
				have = sz;
				state = WANT_BLOCK;

				// Log.i( LOG_TAG, "remaining need for first block: "
				//		+ Integer.valueOf( need - have ).toString() );

				/* Deliberate fall through. */
			}
		
			case WANT_BLOCK: {
				while ( true ) {
					while ( have < need ) {
						int len = read( data, have, need - have );
						if ( len < 0 ) {
							/* EOF. */
							// Log.i( LOG_TAG, "packet data read: closed" );
							bringDownSocket();
							return;
						}
						else if ( len == 0 ) {
							/* Wait. */
							// Log.i( LOG_TAG, "packet data read: delayed" );
							timeout();
							return;
						}
						else if ( have + len < need )  {
							/* Don't have it all. Need to wait for more. */
							// Log.i( LOG_TAG, "packet data read: is short: got " +
							//		Integer.valueOf(len).toString() + " bytes" );
							have += len;
							return;
						}
		
						if ( len > 0 ) {
							// Log.i( LOG_TAG, "packet data read returned: " +
							//		Integer.valueOf(len).toString() + " bytes" );
							have += len;
						}
					}
		
					ByteBuffer bbData = ByteBuffer.wrap(data);
					bbData.order( ByteOrder.LITTLE_ENDIAN );
					need = bbData.getInt( 0 );
					have = 0;
					if ( need == 0 )
						break;
		
					data = packet.allocate( need );
				}
		
				state = WANT_HEAD;
				need = 0;
				have = 0;

				System.out.println( packet.totalLen );
				dispatch( packet );
				packet.clear();
				break;
			}
		}
	}

	public void bringDownSocket()
	{
		try {
			// Log.i( LOG_TAG, "bringing down socket");
			if ( socket != null ) {
				socket.close();
				socket = null;
			}
		}
		catch ( IOException e ) {
			// Log.e( LOG_TAG, "exception: ", e );
		}
	}
}

