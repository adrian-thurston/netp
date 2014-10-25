#include "thread.h"
#include <stdlib.h>

ItBlock::ItBlock( int size )
:
	size(size)
{
}

ItWriter::ItWriter()
{
	head = tail = 0;
	nextWrite = 0;
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
