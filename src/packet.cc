#include "thread.h"
#include "packet.h"

void Thread::closeForPacket( SelectFd *fd )
{
	::close( fd->fd );
	fd->closed = true;
}   

char *Thread::pktFind( Rope *rope, long l )
{
	// log_message( "pkt find: " << l );

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
	if ( buf.tblk != 0 && nb <= ( buf.tblk->size - buf.toff ) ) {
		offset = buf.length();
		// log_message( "alloc bytes offset: " << offset );
		return buf.append( 0, nb );
	}
	else {
		/* Need to move to block 1 or up, so we include the block header. */
		offset = buf.length() + sizeof(PacketBlockHeader);
		char *data = buf.append( 0, sizeof(PacketBlockHeader) + nb );
		// log_message( "alloc bytes offset: " << offset );
		return data + sizeof(PacketBlockHeader);
	}
}

void *GenF::Packet::open( PacketWriter *writer, int ID, int SZ )
{
	writer->reset();

	/* Place the header. */
	long offset = 0;
	PacketHeader *header = (PacketHeader*)writer->allocBytes( sizeof(PacketHeader), offset );
	
	// log_message( "header: " << header << " rb.head: " <<
	//		(PacketHeader*)writer->buf.data(writer->buf.hblk) );

	header->msgId = ID;
	header->writerId = 934223439; // writer->id;

	/* Place the struct. */
	void *msg = writer->allocBytes( SZ, offset );

	header->length = 0;
	writer->toSend = header;
	writer->content = msg;

	return msg;
}


/*
 * List of blocks of form ( next-len, content ), where next-len refers to the
 * size of the next block. The size of the first block is contained in the
 * packet header, which is at the head of the content. To read, take in the
 * first next-len and the packet header, then read the remainder of the first
 * block.  */
void GenF::Packet::send( PacketWriter *writer )
{
	RopeBlock *rb = writer->buf.hblk;
	Connection *c = static_cast<Connection*>(writer->fd->local);

	/* Length of first block goes into the header. */
	writer->toSend->length = writer->length();
	writer->toSend->firstLen = writer->buf.length(rb);

	// log_message( "first len: " << writer->toSend->firstLen );

	/* First pass: set each nextlen. */
	for ( ; rb != 0; rb = rb->next ) {
		char *data = writer->buf.data(rb);
		int *nextLen = (int*)(data);
		*nextLen = rb->next != 0 ? writer->buf.length(rb->next) : 0;
	}

	/* Second pass: write the blocks. */

	int blocks = 0;
	for ( rb = writer->buf.hblk; rb != 0; rb = rb->next )
		blocks += 1;

	// log_message( "block list len: " << blocks );

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

void GenF::Packet::send( PacketWriter *writer, Rope &blocks )
{
	Connection *c = static_cast<Connection*>(writer->fd->local);
	for ( RopeBlock *rb = blocks.hblk; rb != 0; rb = rb->next ) {
		char *data = blocks.data(rb);
		int blockLen = blocks.length(rb);

		int res = c->write( data, blockLen );
		if ( res < blockLen )
			log_ERROR( "failed to send full block" );

		// log_debug( DBG__PKTSEND, "packet send result: " << res );
	}
}

/* Read a fixed-size prefix of the first block into a temp space, extract the
 * first length from the headers, allocate the full first block, copy the
 * prefix we read into the block, then enter into block read loop, jumping in
 * past the headers. * */
void Thread::parsePacket( SelectFd *fd )
{
	// log_message( "parsing packet" );

	Connection *c = static_cast<Connection*>(fd->local);
	switch ( fd->recv.state ) {
		case SelectFd::Recv::WantHead: {
			const int sz = sizeof(PacketBlockHeader) + sizeof(PacketHeader);
			int len = c->read( (char*)&fd->recv.headBuf + fd->recv.have, sz - fd->recv.have );
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

			/* Completed read of header. */
			// log_message( "have first block headers" );

			/* Pull the size of the first block length from the header read so far. */
			fd->recv.head = (PacketHeader*)(fd->recv.headBuf + sizeof(PacketBlockHeader));
			fd->recv.need = fd->recv.head->firstLen;

			/* Allocate the first block and move the header data in from the
			 * temp read space. */
			fd->recv.data = fd->recv.buf.appendBlock( fd->recv.head->firstLen );
			memcpy( fd->recv.data, fd->recv.headBuf, sz );

			/* Reset the head pointer to the header we just coppied in. */
			fd->recv.head = (PacketHeader*)(fd->recv.data + sizeof(PacketBlockHeader));

			/* Indicate we have the headers and enter into block read loop. */
			fd->recv.have = sz;
			fd->recv.state = SelectFd::Recv::WantBlock;

			/* Deliberate fall through. */
			// log_message( "remaining need for first block: " << fd->recv.need - fd->recv.have );
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
	
				fd->recv.need = *((int*)fd->recv.data);
				fd->recv.have = 0;
				if ( fd->recv.need == 0 )
					break;
	
				fd->recv.data = fd->recv.buf.appendBlock( fd->recv.need );
			}
	
			fd->recv.state = SelectFd::Recv::WantHead;
			fd->recv.need = 0;
			fd->recv.have = 0;
	
			// log_message( "writer id: " << fd->recv.head->writerId );

			dispatchPacket( fd );

			fd->recv.buf.empty();
			break;
		}
	}
}

