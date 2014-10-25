#include "thread.h"
#include <stdlib.h>

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
