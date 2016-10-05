#include "thread.h"
#include <stdlib.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>

long enabledRealms = 0;

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
	writer(0),
	reader(0),
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

	/* No Existing index to use. Append. */
	itWriter->id = writerVect.size();
	writerVect.push_back( itWriter );

set:
	writerList.append( itWriter );

	pthread_mutex_unlock( &mutex );
	return itWriter;
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

	if ( writer->reader->recvRequiresSignal )
		pthread_kill( writer->reader->pthread, SIGUSR1 );

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

int Thread::inetListen( uint16_t port )
{
	/* Create the socket. */
	int listenFd = socket( PF_INET, SOCK_STREAM, 0 );
	if ( listenFd < 0 ) {
		log_ERROR( "unable to allocate socket" );
		return -1;
	}

	/* Set its address to reusable. */
	int optionVal = 1;
	setsockopt( listenFd, SOL_SOCKET, SO_REUSEADDR,
			(char*)&optionVal, sizeof(int) );

	/* bind. */
	sockaddr_in sockName;
	sockName.sin_family = AF_INET;
	sockName.sin_port = htons(port);
	sockName.sin_addr.s_addr = htonl (INADDR_ANY);
	if ( bind(listenFd, (sockaddr*)&sockName, sizeof(sockName)) < 0 ) {
		log_ERROR( "unable to bind to port " << port );
		close( listenFd );
		return -1;
	}

	/* listen. */
	if ( listen( listenFd, 1 ) < 0 ) {
		log_ERROR( "unable put socket in listen mode" );
		close( listenFd );
		return -1;
	}

	return listenFd;
}

int Thread::pselectLoop( sigset_t *sigmask )
{
	/* accept loop. */
	while ( !breakLoop ) {
		fd_set readSet;
		FD_ZERO( &readSet );
		int highest = -1;
		for ( SelectFdList::Iter fd = selectFdList; fd.lte(); fd++ ) {
			if ( fd->fd > highest )
				highest = fd->fd;
			FD_SET( fd->fd, &readSet );
		}

		/* Wait no longer than a second. */
		timespec ts;
		ts.tv_nsec = 0;
		ts.tv_sec = 1;

		int result = pselect( highest+1, &readSet, 0, 0, &ts, sigmask );

		if ( result < 0 ) {
			if ( errno == EINTR )
				return -1;

			if ( errno != EAGAIN ) 
				log_FATAL( "select returned an unexpected error " << strerror(errno) );
		}

		if ( result > 0 ) {
			for ( SelectFdList::Iter fd = selectFdList; fd.lte(); fd++ ) {
				if ( FD_ISSET( fd->fd, &readSet ) ) {
					switch ( fd->type ) {
						case SelectFd::Listen: {
							sockaddr_in peer;
							socklen_t len = sizeof(sockaddr_in);

							result = ::accept( fd->fd, (sockaddr*)&peer, &len );
							if ( result >= 0 ) {
								accept( result );
							}
							else {
								log_ERROR( "failed to accept connection: " <<
										strerror(errno) );
							}
							break;
						}
						case SelectFd::Data: {
							data( fd );
							break;
						}
					}
				}
			}
		}

		poll();
	}

	/* finalTimerRun( c ); */
	/* close( listenFd ); */

	return 0;
}

int Thread::selectLoop()
{
	bool cont = true;
	while ( cont ) {
		int r = pselectLoop( 0 );
		if ( r == 0 )
			break;
	}
	return 0;
}

int Thread::inetConnect( uint16_t port )
{
	const char *host = "127.0.0.1";

	sockaddr_in servername;
	hostent *hostinfo;
	long connectRes;

	/* Create the socket. */
	int fd = socket( PF_INET, SOCK_STREAM, 0 );
	if ( fd < 0 )
		log_ERROR( "SocketConnectFailed" );

	/* Lookup the host. */
	servername.sin_family = AF_INET;
	servername.sin_port = htons(port);
	hostinfo = gethostbyname( host );
	if ( hostinfo == NULL ) {
		::close( fd );
		log_ERROR( "SocketConnectFailed" );
	}

	servername.sin_addr = *(in_addr*)hostinfo->h_addr;

	/* Connect to the listener. */
	connectRes = ::connect( fd, (sockaddr*)&servername, sizeof(servername) );
	if ( connectRes < 0 ) {
		::close( fd );
		log_ERROR( "SocketConnectFailed" );
		return -1;
	}

	return fd;
}


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
