#include "thread.h"
#include "packet.h"

char *Thread::pktFind( Rope *rope, long l )
{
	// log_debug( DBG_PACKET, "pkt find: " << l );
	if ( l == 0 )
		return 0;

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
char *PacketWriter::allocBytes( int nb, uint32_t &offset )
{
	if ( buf.tblk != 0 && nb <= buf.available( buf.tblk ) ) {
		offset = buf.length();
		log_debug( DBG_PACKET, "alloc bytes offset: " << offset );
		return buf.append( 0, nb );
	}
	else {
		/* Need to move to block 1 or up, so we include the block header. */
		offset = buf.length() + sizeof(PacketBlockHeader);
		char *data = buf.append( 0, sizeof(PacketBlockHeader) + nb );
		log_debug( DBG_PACKET, "alloc bytes offset: " << offset );
		return data + sizeof(PacketBlockHeader);
	}
}

void *PacketBase::open( PacketWriter *writer, int ID, int SZ )
{
	writer->reset();

	if ( writer->usingItWriter() ) {
		/* Open up a passthrough message and stash it in writer. */
		writer->pp = (Message::PacketPassthru*)itqOpen( writer->itw,
				Message::PacketPassthru::ID, sizeof(Message::PacketPassthru) );
	}

	/* Place the header. */
	uint32_t offset = 0;
	PacketHeader *header = (PacketHeader*)writer->allocBytes(
			sizeof(PacketHeader), offset );
	
	log_debug( DBG_PACKET, "header: " << header << " rb.head: " <<
			(PacketHeader*)writer->buf.data(writer->buf.hblk) );

	header->msgId = ID;

	/* Place the struct. */
	void *msg = writer->allocBytes( SZ, offset );
	memset( msg, 0, SZ );

	header->totalLen = 0;
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
void PacketBase::send( PacketWriter *writer )
{
	RopeBlock *rb = writer->buf.hblk;

	/* Length of first block goes into the header. */
	writer->toSend->totalLen = writer->length();
	writer->toSend->firstLen = writer->buf.length(rb);

	log_debug( DBG_PACKET, "first len: " << writer->toSend->firstLen );

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

	log_debug( DBG_PACKET, "block list len: " << blocks );

	/* Ready to go, ship out. */
	if ( writer->usingItWriter() ) {
		/* Transfer the packet data to the Itc message. */
		Rope *r = new Rope;
		r->transfer( writer->buf );
		writer->pp->rope = r;

		/* Send, without using a signal. */
		writer->itw->queue->send( writer->itw, false );
	}
	else {
		send( writer, writer->buf, false );
		writer->buf.empty();
	}
}

int PacketConnection::bufWrite( SelectFd *selectFd, char *data, int len )
{
	return Connection::write( data, len );
}

void PacketConnection::bufClose( SelectFd *selectFd )
{
	log_debug( DBG_PACKET, "pipe close" );
	this->close();
	this->packetClosed();
}

/*
 * if in wantWrite mode then there 
 */
void PacketBase::send( PacketWriter *writer, Rope &blocks, bool canConsume )
{
	PacketConnection *pc = writer->pc;
	pc->writeBuffer.send( pc->selectFd, blocks, canConsume );
}

void PacketConnection::writeReady()
{
	writeBuffer.writeReady( this->selectFd );
}

void PacketConnection::readReady()
{
	parsePacket( selectFd );
}

/* Read a fixed-size prefix of the first block into a temp space, extract the
 * first length from the headers, allocate the full first block, copy the
 * prefix we read into the block, then enter into block read loop, jumping in
 * past the headers. * */
void PacketConnection::parsePacket( SelectFd *fd )
{
	log_debug( DBG_PACKET, "parsing packet" );

	switch ( recv.state ) {
		case Recv::WantHead: {
			const int sz = sizeof(PacketBlockHeader) + sizeof(PacketHeader);
			int len = read( (char*)&recv.headBuf + recv.have, sz - recv.have );
			if ( len < 0 ) {
				/* EOF. */
				log_debug( DBG_PACKET, "packet head read: closed" );
				close();
				packetClosed();
				return;
			}
			else if ( len == 0 ) {
				log_debug( DBG_PACKET, "packet head read: delayed" );
				return;
			}
			else if ( recv.have + len < sz )  {
				log_debug( DBG_PACKET, "packet head read: is short: got " <<
						len << " bytes" );

				/* Don't have it all. Need to wait for more. */
				recv.have += len;
				return;
			}

			/* Completed read of header. */
			log_debug( DBG_PACKET, "packet: have first block headers: " << len );

			/* Pull the size of the first block length from the header read so far. */
			recv.head = (PacketHeader*)(recv.headBuf + sizeof(PacketBlockHeader));
			recv.need = recv.head->firstLen;

			/* Allocate the first block and move the header data in from the
			 * temp read space. */
			recv.data = recv.buf.appendBlock( recv.head->firstLen );
			memcpy( recv.data, recv.headBuf, sz );

			/* Reset the head pointer to the header we just coppied in. */
			recv.head = (PacketHeader*)(recv.data + sizeof(PacketBlockHeader));

			/* Indicate we have the headers and enter into block read loop. */
			recv.have = sz;
			recv.state = Recv::WantBlock;

			/* Deliberate fall through. */
			log_debug( DBG_PACKET, "remaining need for first block: " << recv.need - recv.have );
		}
	
		log_debug( DBG_PACKET, "total packet length: " << recv.head->totalLen );

		case Recv::WantBlock: {
			while ( true ) {
				while ( recv.have < recv.need ) {
					int len = read( recv.data + recv.have, recv.need - recv.have );
					if ( len < 0 ) {
						/* EOF. */
						log_debug( DBG_PACKET, "packet data read: closed" );
						close();
						packetClosed();
						return;
					}
					else if ( len == 0 ) {
						/* continue. */
						log_debug( DBG_PACKET, "packet data read: delayed" );
						return;
					}
					else if ( recv.have + len < recv.need )  {
						log_debug( DBG_PACKET, "packet data read: is short: got " <<
								len << " bytes" );
						/* Don't have it all. Need to wait for more. */
						recv.have += len;
						return;
					}
	
					if ( len > 0 ) {
						log_debug( DBG_PACKET, "packet data read returned: " <<
								len << " bytes" );
						recv.have += len;
					}
				}
	
				recv.need = *((int*)recv.data);
				recv.have = 0;

				log_debug( DBG_PACKET, "next block size: " << recv.need );
				if ( recv.need == 0 )
					break;
	
				recv.data = recv.buf.appendBlock( recv.need );
			}
	
			recv.state = Recv::WantHead;
			recv.need = 0;
			recv.have = 0;
	

			log_debug( DBG_PACKET, "dispatching packet parsing packet" );
			thread->dispatchPacket( fd, recv );

			recv.buf.empty();
			break;
		}
	}
}

