#include "thread.h"
#include "packet.h"

char *Thread::pktFind( Rope *rope, long l )
{
	// log_message( "pkt find: " << l );
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
	memset( msg, 0, SZ );

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

	/* Ready to go, ship out. */
	send( writer, writer->buf );

	writer->buf.empty();
}

/*
 * if in wantWrite mode then there 
 */
void GenF::Packet::send( PacketWriter *writer, Rope &blocks )
{
	Connection *c = static_cast<Connection*>(writer->fd->local);
	PacketConnection *pc = dynamic_cast<PacketConnection*>(c);

	if ( pc->selectFd->wantWrite ) {
		/* If in want write mode then there are queued blocks. Add to the queue
		 * and let the writeReady callback deal with the output. */
		for ( RopeBlock *rb = blocks.hblk; rb != 0; rb = rb->next )
			pc->queue.append( blocks.data(rb), blocks.length(rb) );
	}
	else {
		/* Not blocked on writing, attempt to write the rope. */
		for ( RopeBlock *rb = blocks.hblk; rb != 0; rb = rb->next ) {
			char *data = blocks.data(rb);
			int blockLen = blocks.length(rb);

			int res = c->write( data, blockLen );
			if ( res < 0 )
				log_FATAL( "failed to send block, erroring out" );

			if ( res < blockLen ) {
				/* Didn't write a whole block. Put the remainder of the block
				 * and the message on the queue and go into wantWrite mode. */
				log_message( "failed to send full block, queueing " <<
						(blockLen-res) << " of " << blockLen );

				pc->queue.append( data + res, blockLen - res);
				rb = rb->next;
				for ( ; rb != 0; rb = rb->next )
					pc->queue.append( blocks.data(rb), blocks.length(rb) );
				pc->selectFd->wantWrite = true;
				break;
			}

			// log_debug( DBG__PKTSEND, "packet send result: " << res );
		}
	}
}

void PacketConnection::writeReady()
{
	if ( queue.length() == 0 ) {
		/* Queue is now empty. We can exit wantWrite mode. */
		log_message( "write ready, queue empty, turning off want write" );
		selectFd->wantWrite = false;
	}
	else {
		/* Have data to send out. */
		log_message( "write ready, sending block" );
		while ( queue.length() > 0 ) {
			RopeBlock *rb = queue.hblk;
			char *data = queue.data(rb);
			int blockLen = queue.length(rb);
			int res = write( data + qho, blockLen - qho );

			log_message( " -> sent " << res << " of " << ( blockLen - qho ) << " bytes" );

			if ( res < (blockLen-qho) ) {
				/* Didn't send the entire remainder of the block. Increqment
				 * the queue head offset and wait for another write ready. */
				qho += res;
				break;
			}

			qho = 0;
			queue.ropeLen -= blockLen;
			queue.hblk = queue.hblk->next;
			if ( queue.hblk == 0 )
				queue.tblk = 0;
		}
	}
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
	// log_message( "parsing packet" );

	switch ( recv.state ) {
		case SelectFd::Recv::WantHead: {
			const int sz = sizeof(PacketBlockHeader) + sizeof(PacketHeader);
			int len = read( (char*)&recv.headBuf + recv.have, sz - recv.have );
			if ( len < 0 ) {
				/* EOF. */
				// log_debug( DBG__PKTRECV, "packet head read closed" );
				close();
				return;
			}
			else if ( len == 0 ) {
				// log_debug( DBG__PKTRECV, "packet head read delayed: : " <<
				//		strerror(errno) );
				return;
			}
			else if ( recv.have + len < sz )  {
				// log_debug( DBG__PKTRECV, "packet head read is short: " <<
				//		len << " bytes" );

				/* Don't have it all. Need to wait for more. */
				recv.have += len;
				return;
			}

			/* Completed read of header. */
			// log_message( "have first block headers" );

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
			recv.state = SelectFd::Recv::WantBlock;

			/* Deliberate fall through. */
			// log_message( "remaining need for first block: " << recv.need - recv.have );
		}
	
		case SelectFd::Recv::WantBlock: {
			while ( true ) {
				while ( recv.have < recv.need ) {
					int len = read( recv.data + recv.have, recv.need - recv.have );
					if ( len < 0 ) {
						/* EOF. */
						// log_debug( DBG__PKTRECV, "packet data read closed" );
						close();
						return;
					}
					else if ( len == 0 ) {
						/* continue. */
						// log_debug( DBG__PKTRECV, "packet data read delayed: " <<
						//		strerror(errno) );
						return;
					}
					else if ( recv.have + len < recv.need )  {
						// log_debug( DBG__PKTRECV, "packet data read is short: " <<
						//		len << " bytes" );
						/* Don't have it all. Need to wait for more. */
						recv.have += len;
						return;
					}
	
					if ( len > 0 ) {
						// log_debug( DBG__PKTRECV, "packet data read returned: " <<
						//		len << " bytes" );
						recv.have += len;
					}
				}
	
				recv.need = *((int*)recv.data);
				recv.have = 0;
				if ( recv.need == 0 )
					break;
	
				recv.data = recv.buf.appendBlock( recv.need );
			}
	
			recv.state = SelectFd::Recv::WantHead;
			recv.need = 0;
			recv.have = 0;
	
			// log_message( "writer id: " << recv.head->writerId );

			thread->dispatchPacket( fd, recv );

			recv.buf.empty();
			break;
		}
	}
}

