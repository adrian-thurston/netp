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
char *PacketWriter::allocBytes( int nb, long &offset )
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
		writer->pp = (PacketPassthru*)itqOpen( writer->itw,
				PacketPassthru::ID, sizeof(PacketPassthru) );
	}

	/* Place the header. */
	long offset = 0;
	PacketHeader *header = (PacketHeader*)writer->allocBytes( sizeof(PacketHeader), offset );
	
	log_debug( DBG_PACKET, "header: " << header << " rb.head: " <<
			(PacketHeader*)writer->buf.data(writer->buf.hblk) );

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
void PacketBase::send( PacketWriter *writer )
{
	RopeBlock *rb = writer->buf.hblk;

	/* Length of first block goes into the header. */
	writer->toSend->length = writer->length();
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

/*
 * if in wantWrite mode then there 
 */
void PacketBase::send( PacketWriter *writer, Rope &blocks, bool canConsume )
{
	PacketConnection *pc = writer->pc;

	if ( pc->selectFd->wantWriteGet() ) {
		log_debug( DBG_PACKET, "in want write mode "
				"queueing entire rope: " <<
				pc->selectFd->tlsEstablished << " " <<
				pc->selectFd->tlsWantWrite << " " <<
				pc->selectFd->wantWrite );

		for ( RopeBlock *drb = blocks.hblk; drb != 0; drb = drb->next )
			log_debug( DBG_PACKET, "-> queueing " << blocks.length( drb ) << " bytes" );

		/* If in want write mode then there are queued blocks. Add to the queue
		 * and let the writeReady callback deal with the output. */
		if ( canConsume )
			pc->queue.append( blocks );
		else {
			for ( RopeBlock *rb = blocks.hblk; rb != 0; rb = rb->next )
				pc->queue.append( blocks.data( rb ), blocks.length( rb ) );
		}
	}
	else {
		log_debug( DBG_PACKET, "packet send, sending blocks" );

		/* Not blocked on writing, attempt to write the rope. */
		RopeBlock *rb = blocks.hblk;
		while ( true ) {

			if ( rb == 0 )
				break;

			char *data = blocks.data(rb);
			int blockLen = blocks.length(rb);
			int res = pc->write( data, blockLen );
			if ( res < 0 ) {
				log_debug( DBG_PACKET, "packet write: closed" );
				pc->close();
				pc->packetClosed();
			}

			log_debug( DBG_PACKET, " -> sent " << res << " of " << ( blockLen ) << " bytes" );

			if ( res < blockLen ) {
				/* Didn't write a whole block. Put the remainder of the block
				 * and the message on the queue and go into wantWrite mode. */
				log_debug( DBG_PACKET, "failed to send full block, queueing " <<
						(blockLen-res) << " of " << blockLen );

				for ( RopeBlock *drb = blocks.hblk; drb != 0; drb = drb->next )
					log_debug( DBG_PACKET, "-> queueing " << blocks.length( drb ) << " bytes" );

				if ( canConsume ) {
					/* Move data from blocks to the output queue. */
					rb->hoff += res;
					blocks.ropeLen -= res;


					pc->queue.append( blocks );
				}
				else {
					/* Copy data from blocks to output queue. */
					pc->queue.append( data + res, blockLen - res);
					rb = rb->next;
					for ( ; rb != 0; rb = rb->next )
						pc->queue.append( blocks.data(rb), blocks.length(rb) );
				}
				pc->selectFd->wantWriteSet( true );
				break;
			}

			if ( canConsume ) {
				/* Consume the head block. */
				blocks.ropeLen -= res;
				blocks.hblk = blocks.hblk->next;
				if ( blocks.hblk == 0 )
					blocks.tblk = 0;
				delete[] (char*)rb;
				rb = blocks.hblk;
			}
			else {
				rb = rb->next;
			}


			log_debug( DBG_PACKET, "packet send result: " << res );
		}
	}
}

void PacketConnection::writeReady()
{
	if ( queue.length() == 0 ) {
		/* Queue is now empty. We can exit wantWrite mode. */
		log_debug( DBG_PACKET, "write ready, queue empty, turning off want write" );
		selectFd->wantWriteSet( false );
		queue.empty();
	}
	else {
		/* Have data to send out. */
		log_debug( DBG_PACKET, "write ready, sending blocks" );

		while ( true ) {
			RopeBlock *rb = queue.hblk;

			if ( rb == 0 )
				break;

			char *data = queue.data(rb);
			int blockLen = queue.length(rb);
			int res = write( data, blockLen );
			if ( res < 0 ) {
				log_debug( DBG_PACKET, "packet write: closed" );
				close();
				packetClosed();
			}

			log_debug( DBG_PACKET, " -> sent " << res << " of " << ( blockLen ) << " bytes" );

			if ( res < blockLen ) {
				/* Didn't send the entire remainder of the block. Increqment
				 * the queue head offset and wait for another write ready. */
				rb->hoff += res;
				queue.ropeLen -= res;
				break;
			}

			/* Consume the head block. */
			queue.ropeLen -= res;
			queue.hblk = queue.hblk->next;
			if ( queue.hblk == 0 )
				queue.tblk = 0;
			delete[] (char*)rb;
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
	log_debug( DBG_PACKET, "parsing packet" );

	switch ( recv.state ) {
		case SelectFd::Recv::WantHead: {
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
			recv.state = SelectFd::Recv::WantBlock;

			/* Deliberate fall through. */
			log_debug( DBG_PACKET, "remaining need for first block: " << recv.need - recv.have );
		}
	
		case SelectFd::Recv::WantBlock: {
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
				if ( recv.need == 0 )
					break;
	
				recv.data = recv.buf.appendBlock( recv.need );
			}
	
			recv.state = SelectFd::Recv::WantHead;
			recv.need = 0;
			recv.have = 0;
	

			log_debug( DBG_PACKET, "dispatching packet parsing packet" );
			thread->dispatchPacket( fd, recv );

			recv.buf.empty();
			break;
		}
	}
}

