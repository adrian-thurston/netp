#include "thread.h"
#include "packet.h"

void Thread::closeForPacket( SelectFd *fd )
{
	::close( fd->fd );
	fd->closed = true;
}   

void Thread::parsePacket( SelectFd *fd )
{
	Connection *c = static_cast<Connection*>(fd->local);
	switch ( fd->recv.state ) {
		case SelectFd::Recv::WantHead: {
			const int sz = sizeof(fd->recv.head);
			int len = c->read( (char*)&fd->recv.head + fd->recv.have, sz - fd->recv.have );
			if ( len < 0 ) {
				/* EOF. */
				// log_debug( DBG__PKTRECV, "packet head read closed" );
				closeForPacket( fd );
				return;
			}
			else if ( len == 0 ) {
				// log_debug( DBG__PKTRECV, "packet head read delayed: : " << strerror(errno) );
				return;
			}
			else if ( fd->recv.have + len < sz )  {
				// log_debug( DBG__PKTRECV, "packet head read is short: " << len << " bytes" );

				/* Don't have it all. Need to wait for more. */
				fd->recv.have += len;
				return;
			}

			if ( len > 0 ) {
				// log_debug( DBG__PKTRECV, "packet head read returned: " << len << " bytes" );
			}

			/* Completed read of header. */
			fd->recv.nextLen = &fd->recv.head.firstLen;
			fd->recv.state = SelectFd::Recv::WantBlock;
		
			fd->recv.need = *fd->recv.nextLen;
			fd->recv.have = 0;

			/* Deliberate fall through. */
			fd->recv.data = fd->recv.buf.appendBlock( fd->recv.need );
		}
	
		case SelectFd::Recv::WantBlock: {
			while ( true ) {
				while ( fd->recv.have < fd->recv.need ) {
					int len = c->read( fd->recv.data + fd->recv.have, fd->recv.need - fd->recv.have );
					if ( len < 0 ) {
						/* EOF. */
						// log_debug( DBG__PKTRECV, "packet data read closed" );
						closeForPacket( fd );
						return;
					}
					else if ( len == 0 ) {
						/* continue. */
						// log_debug( DBG__PKTRECV, "packet data read delayed: : " << strerror(errno) );
						return;
					}
					else if ( fd->recv.have + len < fd->recv.need )  {
						// log_debug( DBG__PKTRECV, "packet data read is short: " << len << " bytes" );
						/* Don't have it all. Need to wait for more. */
						fd->recv.have += len;
						return;
					}
	
					if ( len > 0 ) {
						// log_debug( DBG__PKTRECV, "packet data read returned: " << len << " bytes" );
						fd->recv.have += len;
					}
				}
	
				fd->recv.nextLen = (int*)fd->recv.data;
				if ( *fd->recv.nextLen == 0 )
					break;
	
				fd->recv.need = *fd->recv.nextLen;
				fd->recv.have = 0;
				fd->recv.data = fd->recv.buf.appendBlock( fd->recv.need );
			}
	
			fd->recv.state = SelectFd::Recv::WantHead;
			fd->recv.need = 0;
			fd->recv.have = 0;
	
			dispatchPacket( fd );

			fd->recv.buf.empty();
			break;
		}
	}
}

/*
 * format:
 *   PacketHeader, which contains first nextLen, then ( nextLen, block )*
 * 
 * The block lengths include the size of the nextLen field (int)
 */

void GenF::Packet::send( PacketWriter *writer )
{
	RopeBlock *rb = writer->buf.hblk;
	Connection *c = static_cast<Connection*>(writer->fd->local);

	writer->toSend->length = writer->length();
	writer->toSend->firstLen = writer->buf.length(rb) - sizeof(PacketHeader);

	/* First pass: set each nextlen. */
	int n = 0;
	for ( ; rb != 0; rb = rb->next ) {
		char *data = writer->buf.data(rb);

		int *nextLen;
		if ( n == 0 ) {
			/* First block explicitly contains the packet block header. */
			nextLen = (int*)(data + sizeof(PacketHeader));
		}
		else {
			nextLen = (int*)(data);
		}

		*nextLen = rb->next != 0 ? writer->buf.length(rb->next) : 0;

		n += 1;
	}

	/* Second pass: write the blocks. */
	for ( rb = writer->buf.hblk; rb != 0; rb = rb->next ) {
		char *data = writer->buf.data(rb);
		int blockLen = writer->buf.length(rb);

		int res = c->write( data, blockLen );
		if ( res < blockLen )
			log_ERROR( "failed to send full block" );
		// log_debug( DBG__PKTSEND, "packet send result: " << res );

	}

	writer->buf.empty();
}
