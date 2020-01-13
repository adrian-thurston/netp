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

Thread::RealmSet Thread::enabledRealms[NUM_APP_ID];
pthread_key_t Thread::thisKey;

namespace genf
{
	lfdostream *lf;
}


void Thread::initId()
{
	tid = syscall( SYS_gettid );
	pthread_setspecific( thisKey, this );
	pthread_this = pthread_self();
	if ( pendingNotifSignal )
		pthread_kill( pthread_this, SIGUSR1 );

}

extern "C" void *genf_thread_start( void *arg )
{
	Thread *thread = (Thread*)arg;
	thread->initId();
	ares_init( &thread->ac );
	long r = thread->start();
	ares_destroy( thread->ac );
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
	struct timeval tv;
	struct tm local;
	char string[96];

	gettimeofday( &tv, 0 );
	epoch = tv.tv_sec;
	localtime_r( &epoch, &local );
	int r = strftime( string, sizeof(string), "%Y-%m-%d %H:%M:%S", &local );
	if ( r > 0 )
		out << string << '.' << std::setfill('0') << std::setw(6) << tv.tv_usec;

	return out;
}

std::ostream &operator <<( std::ostream &out, const log_prefix & )
{
	Thread *thread = Thread::getThis();
	if ( thread != 0 )
		out << *thread;
	else
		out << log_time() << ": ";
	return out;
}

std::ostream &operator <<( std::ostream &out, const Thread &thread )
{
	out << log_time() << ": " << thread.type << "(" << thread.tid << "): ";
	return out;
}

std::ostream &operator <<( std::ostream &out, const log_binary &b )
{
	const int run = 80;
	int off = 0;
	char buf[run+1];
	int lines = 0;

	while ( off < b.len ) {
		int use = ( b.len - off <= run ) ? b.len - off : run;
		memcpy( buf, b.data + off, use );
		for ( int c = 0; c < use; c++ ) {
			if ( !isprint( buf[c] ) )
				buf[c] = '.';
		}
		buf[use] = 0;

		out << std::endl << "    " << buf;

		off += use;
		lines += 1;
	}

	return out;
}

std::ostream &operator <<( std::ostream &out, const log_hex &h )
{
	unsigned char *p = (unsigned char*)h.data;
	int off = 0;

	out << std::endl;
	out << std::hex;

	while ( off < h.len ) {
		unsigned int d = p[off];

		out << ' ';
		out << std::setfill('0') << std::setw( 2 ) << d;

		off += 1;

		if ( off % 16 == 0 )
			out << std::endl;
	}

	out << std::dec;

	return out;
}
