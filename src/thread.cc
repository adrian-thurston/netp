#include "thread.h"
#include <stdlib.h>
#include <time.h>

namespace genf
{
	lfdostream *lf;
}

ItBlock::ItBlock( int size )
:
	size(size)
{
}

ItWriter::ItWriter()
:
	thread(0),
	queue(0),
	id(-1),
	head(0), tail(0),
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

ItWriter *ItQueue::registerWriter( Thread *thread )
{
	ItWriter *writer = new ItWriter;
	writer->thread = thread;
	writer->queue = this;

	/* Reigster under lock. */
	pthread_mutex_lock( &mutex );

	/* Allocate an id (index into the vector of writers). */
	for ( int i = 0; i < (int)writerVect.size(); i++ ) {
		/* If there is a free spot, use it. */
		if ( writerVect[i] == 0 ) {
			writerVect[i] = writer;
			writer->id = i;
			goto set;
		}
	}

	/* No Existing index to use. Append. */
	writer->id = writerVect.size();
	writerVect.push_back( writer );

set:
	writerList.append( writer );

	pthread_mutex_unlock( &mutex );
	return writer;
}

ItBlock *ItQueue::allocateBlock()
{
	return new ItBlock( blockSz );
}

void ItQueue::freeBlock( ItBlock *block )
{
	delete block;
}

void *ItQueue::allocBytes( ItWriter *writer, int size )
{
	if ( writer->tail == 0 ) {
		/* There are no blocks. */
		writer->head = writer->tail = allocateBlock();
		writer->hoff = writer->toff = 0;
	}
	else {
		int avail = writer->tail->size - writer->toff;

		/* Move to the next block? */
		if ( size > avail ) {
			ItBlock *block = allocateBlock();
			writer->tail->next = block;
			writer->tail = block;
			writer->toff = 0;

			/* Need to track the padding in the message length. */
			writer->mlen += avail;
		}
	}

	void *ret = writer->tail->data + writer->toff;
	writer->toff += size;
	writer->mlen += size;
	return ret;
}

void ItQueue::send( ItWriter *writer )
{
	pthread_mutex_lock( &mutex );

	/* Put on the end of the message list. */
	if ( head == 0 )
		head = tail = writer->toSend;
	else {
		tail->next = writer->toSend;
		tail = writer->toSend;
	}

	/* Notify anyone waiting. */
	pthread_cond_broadcast( &cond );

	pthread_mutex_unlock( &mutex );
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

void ItQueue::release( ItHeader *header )
{
	ItWriter *writer = writerVect[header->writerId];
	int length = header->length;

	/* Skip whole blocks. */
	int remaining = writer->head->size - writer->hoff;
	while ( length >= remaining ) {
		/* Pop the block. */
		ItBlock *pop = writer->head;
		writer->head = writer->head->next;
		writer->hoff = 0;
		freeBlock( pop );

		/* Take what was left off the length. */
		length -= remaining;

		/* Remaining is the size of the next block (always starting at 0). */
		remaining = writer->head->size;
	}

	/* Final move ahead. */
	writer->hoff += length;
};

void *thread_start_routine( void *arg )
{
	Thread *thread = (Thread*)arg;
	long r = thread->start();
	return (void*)r;
}

std::ostream &operator <<( std::ostream &out, const log_lock & )
{
	lfdostream *fdo = dynamic_cast<lfdostream*>( &out );
	if ( fdo )
		pthread_mutex_lock( &fdo->mutex );
	return out;
}

std::ostream &operator <<( std::ostream &out, const log_unlock & )
{
	lfdostream *fdo = dynamic_cast<lfdostream*>( &out );
	if ( fdo )
		pthread_mutex_unlock( &fdo->mutex );
	return out;
}

std::ostream &operator <<( std::ostream &out, const Thread::endp & )
{
	exit( 1 );
}

std::ostream &operator <<( std::ostream &out, const log_time & )
{
	time_t epoch;
	struct tm local;
	char string[64];

	epoch = time(0);
	localtime_r( &epoch, &local );
	int r = strftime( string, sizeof(string), "%Y-%m-%d %H:%M:%S", &local );
	if ( r > 0 )
		out << string;

	return out;
}

std::ostream &operator <<( std::ostream &out, const log_prefix & )
{
	out << log_time() << ": ";
	return out;
}

std::ostream &operator <<( std::ostream &out, const Thread &thread )
{
	out << log_time() << ": " << thread.type << ": ";
	return out;
}
