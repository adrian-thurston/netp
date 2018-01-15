#include "main.h"
#include "listen.h"

#include "packet_gen.h"
#include "genf.h"

#include <errno.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/ssl.h>

#include <sys/socket.h>
#include <arpa/inet.h>

struct BpConnection
:
	public PacketConnection
{
	BpConnection( Thread *thread, SelectFd *selectFd )
		: PacketConnection( thread, selectFd ) {}

	void notifyAccept()
	{
		log_message( "test packet connection: notify accept" );

		/* Immediately try to read. Is this necessary? IE will the connect
		 * leave data on the FD or buffer it in? */
		// sslReadReady( fd );
		PacketWriter writer( this );

		for ( int i = 0; i < 100; i++ ) {
			BigPacket *msg = BigPacket::open( &writer );
			msg->set_big3( &writer, ::data3 );
			msg->set_big2( &writer, ::data2 );
			msg->set_big1( &writer, ::data1 );
			msg->l1 = ::l1;
			msg->l2 = ::l2;
			msg->l3 = ::l3;
			BigPacket::send( &writer );
		}
	}
};

struct BpListener
:
	public PacketListener
{
	BpListener( Thread *thread ) : PacketListener( thread ) {}
	virtual BpConnection *connectionFactory( Thread *thread, SelectFd *selectFd )
		{ return new BpConnection( thread, selectFd ); }
};


void ListenThread::recvShutdown( Shutdown *msg )
{
	breakLoop();
}

void ListenThread::handleTimer()
{
	log_message( "tick" );
}

int ListenThread::main()
{
	log_message("starting up" );

	BpListener *listener = new BpListener( this );

	listener->tlsAccept = true;
	SSL_CTX *sslCtx = sslCtxServerInternal();

	listener->startListen( 44726, true, sslCtx, false );

	struct timeval t;
	t.tv_sec = 1;
	t.tv_usec = 0;

	selectLoop( &t );

	::close( listener->selectFd->fd );

	log_message( "exiting" );

	return 0;
}
