#include "thread.h"
#include <stdlib.h>

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
	nextWrite(0)
{
}

ItQueue::ItQueue( int blockSz )
:
	messages(0),
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

void ItQueue::wait()
{
	pthread_mutex_lock( &mutex );

	while ( messages == 0 ) {
		pthread_cond_wait( &cond, &mutex );
	}
	messages -= 1;

	pthread_mutex_unlock( &mutex );
}

void ItQueue::notify()
{
	pthread_mutex_lock( &mutex );

	messages += 1;
	pthread_cond_broadcast( &cond );

	pthread_mutex_unlock( &mutex );
}

void *thread_start_routine( void *arg )
{
	Thread *thread = (Thread*)arg;
	long r = thread->start();
	return (void*)r;
}

std::ostream &operator <<( std::ostream &out, const Thread::endp & )
{
	exit( 1 );
}
