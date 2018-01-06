#include "thread.h"
#include "packet.h"

void Thread::closeForPacket( SelectFd *fd )
{
	::close( fd->fd );
	fd->closed = true;
}   

char *Thread::pktFind( Rope *rope, long l )
{
	log_message( "pkt find: " << l );
	RopeBlock *rb = rope->hblk;

	while ( rb != 0 ) {
		long avail = rope->length( rb );
		if ( l < avail )
			return rope->data( rb ) + l;
		
		rb = rb->next;
		l -= avail;
	}

	return 0;
}


/*
 * Allocates space in the rope buffer with the added effect that whenever a new
 * block in the rope is made a header automatically added to the head of the
 * block.
 */
char *PacketWriter::allocBytes( int nb, long &offset )
{
	if ( buf.tblk == 0 || nb <= ( buf.tblk->size - buf.toff ) ) {
		offset = buf.length() - sizeof(PacketHeader);
		return buf.append( 0, nb );
	}
	else {
		/* Need to move to block 1 or up, so we include the block header. */
		offset = buf.length() - sizeof(PacketHeader) + sizeof(PacketBlockHeader);
		char *data = buf.append( 0, sizeof(PacketBlockHeader) + nb );
		return data + sizeof(PacketBlockHeader);
	}
}

void *GenF::Packet::open( PacketWriter *writer, int ID, int SZ )
{
	writer->reset();

	/* Place the header. */
	PacketHeader *header = (PacketHeader*) writer->buf.append(
			0, sizeof(PacketHeader) + sizeof(PacketBlockHeader) );
	
	log_message( "header: " << header << " rb.head: " << (PacketHeader*)writer->buf.data(writer->buf.hblk) );

	header->msgId = ID;
	header->writerId = 934223439; // writer->id;

	/* Place the struct. */
	long offset = 0;
	void *msg = writer->allocBytes( SZ, offset );

	header->length = 0;
	writer->toSend = header;
	writer->content = msg;

	return msg;
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
	
			log_message( "writer id: " << fd->recv.head.writerId );
			dispatchPacket( fd );

			fd->recv.buf.empty();
			break;
		}
	}
}

