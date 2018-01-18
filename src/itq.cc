#include "thread.h"
#include <stdlib.h>
#include <time.h>

#include <iostream>
#include <iomanip>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>

ItWriter::ItWriter()
:
	writer(0),
	reader(0),
	queue(0),
	id(-1),
	hblk(0), tblk(0),
	hoff(0), toff(0),
	toSend(0)
{
}

ItQueue::ItQueue( int blockSz )
:
	head(0), tail(0),
	blockSz(blockSz)
{
	pthread_mutex_init( &mutex, 0 );
	pthread_cond_init( &cond, 0 );

	free = 0;
}

ItWriter *ItQueue::registerWriter( Thread *writer, Thread *reader )
{
	ItWriter *itWriter = new ItWriter;
	itWriter->writer = writer;
	itWriter->reader = reader;
	itWriter->queue = this;

	/* Reigster under lock. */
	pthread_mutex_lock( &mutex );

	/* Allocate an id (index into the vector of writers). */
	for ( int i = 0; i < (int)writerVect.size(); i++ ) {
		/* If there is a free spot, use it. */
		if ( writerVect[i] == 0 ) {
			writerVect[i] = itWriter;
			itWriter->id = i;
			goto set;
		}
	}

	/* No existing index to use. Append. */
	itWriter->id = writerVect.size();
	writerVect.push_back( itWriter );

set:
	writerList.append( itWriter );

	pthread_mutex_unlock( &mutex );
	return itWriter;
}

ItBlock *ItQueue::allocateBlock( int supporting )
{
	int size = ( supporting > IT_BLOCK_SZ ) ? supporting : IT_BLOCK_SZ;
	char *bd = new char[sizeof(ItBlock) + size];
	ItBlock *block = (ItBlock*) bd;
	block->data = bd + sizeof(ItBlock);
	block->size = size;
	block->prev = 0;
	block->next = 0;
	return block;
}

void ItQueue::freeBlock( ItBlock *block )
{
	delete block;
}

void *ItQueue::allocBytes( ItWriter *writer, int size )
{
	if ( writer->tblk == 0 ) {
		/* There are no blocks. */
		writer->hblk = writer->tblk = allocateBlock( size );
		writer->hoff = writer->toff = 0;
	}
	else {
		int avail = writer->tblk->size - writer->toff;

		/* Move to the next block? */
		if ( size > avail ) {
			ItBlock *block = allocateBlock( size );
			writer->tblk->next = block;
			writer->tblk = block;
			writer->toff = 0;

			/* Need to track the padding in the message length. */
			writer->mlen += avail;
		}
	}

	void *ret = writer->tblk->data + writer->toff;
	writer->toff += size;
	writer->mlen += size;
	return ret;
}

void ItQueue::send( ItWriter *writer, bool sendSignal )
{
	pthread_mutex_lock( &mutex );

	/* Stash the total length in the header. This grows after opening as
	 * variable-length fields are populated. */
	writer->toSend->length = writer->mlen;

	/* Put on the end of the message list. */
	if ( head == 0 ) {
		head = tail = writer->toSend;
	}
	else {
		tail->next = writer->toSend;
		tail = writer->toSend;
	}
	tail->next = 0;

	/* Notify anyone waiting. */
	pthread_cond_broadcast( &cond );

	pthread_mutex_unlock( &mutex );

	if ( sendSignal || writer->reader->recvRequiresSignal ) {
		if ( writer->reader->pthread_this != 0 )
			pthread_kill( writer->reader->pthread_this, SIGUSR1 );
		else {
			/* If the this (self) hasn't been set yet, tell the thread it has
			 * to send itself the signal when it does get set. */
			writer->reader->pendingNotifSignal = true;
		}
	}
}

ItHeader *ItQueue::wait()
{
	pthread_mutex_lock( &mutex );

	while ( head == 0 )
		pthread_cond_wait( &cond, &mutex );

	ItHeader *header = head;
	head = head->next;

	pthread_mutex_unlock( &mutex );

	header->next = 0;
	return header;
}

bool ItQueue::poll()
{
	pthread_mutex_lock( &mutex );
	bool result = head != 0;
	pthread_mutex_unlock( &mutex );

	return result;
}


void ItQueue::release( ItHeader *header )
{
	ItWriter *writer = writerVect[header->writerId];
	int length = header->length;

	/* Skip whole blocks. */
	int remaining = writer->hblk->size - writer->hoff;
	while ( length > 0 && length >= remaining ) {
		/* Pop the block. */
		ItBlock *pop = writer->hblk;
		writer->hblk = writer->hblk->next;
		writer->hoff = 0;

		/* Maybe we took the list to zero size. */
		if ( writer->hblk == 0 ) {
			writer->tblk = 0;
			writer->toff = 0;
		}

		freeBlock( pop );

		/* Take what was left off the length. */
		length -= remaining;

		/* Remaining is the size of the next block, if present. Always starting
		 * at hoff 0 when we move to the next block. */
		remaining = writer->hblk != 0 ? writer->hblk->size : 0;
	}

	/* Final move ahead. */
	writer->hoff += length;
};


void *itqOpen( ItWriter *writer, int ID, size_t SZ )
{
	writer->mlen = 0;

	/* Place the header. */
	ItHeader *header = (ItHeader*) writer->queue->allocBytes(
			writer, sizeof(ItHeader) );

	header->msgId = ID;
	header->writerId = writer->id;

	/* Length will get recorded on message send, after fields are populated. */
	header->length = 0; 

	/* Place the struct. */
	void  *msg = writer->queue->allocBytes( writer, SZ );

	writer->toSend = header;
	writer->contents = msg;

	return msg;
}

void *itqRead( ItQueue *queue, ItHeader *header, size_t SZ )
{
	ItWriter *writer = queue->writerVect[header->writerId];
	ItBlock *hblk = writer->hblk;
	int offset = writer->hoff;

	if ( offset + sizeof(ItHeader) > hblk->size ) {
		hblk = hblk->next;
		offset = 0;
	}
	offset += sizeof( ItHeader );

	if ( offset + SZ > hblk->size ) {
		hblk = hblk->next;
		offset = 0;
	}
	void *msg = hblk->data + offset;
	offset += SZ;

	return msg;
}
