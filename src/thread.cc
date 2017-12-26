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
#include <signal.h>
#include <sys/time.h>
#include <fcntl.h>

Thread::RealmSet Thread::enabledRealms = 0;
pthread_key_t Thread::thisKey;

namespace genf
{
	lfdostream *lf;
}

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

char *PacketWriter::allocBytes( int nb, long &offset )
{
	if ( buf.tblk == 0 || nb <= ( buf.tblk->size - buf.toff ) ) {
		offset = buf.length() - sizeof(PacketHeader);
		return buf.append( 0, nb );
	}
	else {
		/* Need to move to block 1 or up, so we include the block header. */
		offset = buf.length() - sizeof(PacketHeader) + sizeof(PacketBlockHeader);
		char *data = buf.append( 0, sizeof(PacketBlockHeader) + nb );
		return data + sizeof(PacketBlockHeader);
	}
}

int Thread::inetListen( uint16_t port, bool transparent )
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
	
	if ( transparent ) {
		int optionVal = 1;
		/* Maybe set transparent (for accepting connections on any address, used by
		 * transparent proxy). */
		int r = setsockopt( listenFd, SOL_IP, IP_TRANSPARENT, &optionVal, sizeof(optionVal) );
		if ( r < 0 ) {
			log_ERROR( "failed to set IP_TRANSPARENT flag on socket: " << strerror(r) );
		}
	}

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

bool Thread::makeNonBlocking( int fd )
{
	/* Make FD non-blocking. */
	int flags = fcntl( fd, F_GETFL );
	if ( flags == -1 )
		return false;

	int res = fcntl( fd, F_SETFL, flags | O_NONBLOCK );
	if ( res == -1 )
		return false;

	return true;
}


static int funnelSig = 0;

void thread_funnel_handler( int s )
{
	funnelSig = s;
}

void Thread::funnelSigs( sigset_t *set )
{
	sigaddset( set, SIGHUP );
	sigaddset( set, SIGINT );
	sigaddset( set, SIGQUIT );
	// sigaddset( set, SIGKILL );
	sigaddset( set, SIGTERM );
	sigaddset( set, SIGCHLD );
}

int Thread::signalLoop( sigset_t *set, struct timeval *timer )
{
	struct timeval left, last;

	if ( timer != 0 ) {
		left = *timer;
		gettimeofday( &last, 0 );
	}

	loop = true;
	while ( loop ) {
		int sig;
		if ( timer != 0 ) {
			timespec timeout;
			timeout.tv_sec = left.tv_sec;
			timeout.tv_nsec = left.tv_usec * 1000;
			sig = sigtimedwait( set, 0, &timeout );
		}
		else {
			sig = sigwaitinfo( set, 0 );
		}

		/* Signal handling. EAGAIN means the timer expired. EINTR means
		 * a signal not in set was delivered. In our case that is a USR
		 * signal for waking up processes on msg. */
		if ( sig < 0 && errno != EAGAIN && errno != EINTR ) {
			log_ERROR( "sigwait returned: " << strerror(errno) );
			continue;
		}

		if ( sig > 0 )
			handleSignal( sig );

		/* Timer handling. */
		if ( timer != 0 ) {
			/* Find the time between the last timer execution and now.
			 * Subtract that from the timer interval to establish the
			 * amount of time left. That value will be used in the next
			 * sigwait call. If it is <= zero then execute a timer run,
			 * save the last time and reset the amount left to the full
			 * amount. */
			struct timeval now, elapsed;
			gettimeofday( &now, 0 );
			timersub( &now, &last, &elapsed );
			timersub( timer, &elapsed, &left );
			if ( left.tv_sec < 0 || ( left.tv_usec == 0 && left.tv_sec == 0 ) ) {
				handleTimer();
				left = *timer;
				last = now;
			}
		}
	}

	return 0;
}

int Thread::pselectLoop( sigset_t *sigmask, timeval *timer, bool wantPoll )
{
	timeval left, last;

	if ( timer != 0 ) {
		left = *timer;
		gettimeofday( &last, 0 );
	}

	loop = true;
	while ( loop ) {
		/* Construct event sets. */
		fd_set readSet, writeSet;
		FD_ZERO( &readSet );
		FD_ZERO( &writeSet );

		int nfds = ares_fds( ac, &readSet, &writeSet );
		
		for ( SelectFdList::Iter fd = selectFdList; fd.lte(); fd++ ) {
			if ( fd->closed )
				continue;

			// log_message( "state: " << fd->state );
			bool wantRead =
					( !fd->tlsEstablished ) ?
					fd->wantRead :
					( fd->tlsWantRead || ( fd->tlsWantWrite & fd->tlsWriteWantsRead ) );

			bool wantWrite =
					( !fd->tlsEstablished ) ?
					fd->wantWrite :
					( fd->tlsWantWrite || ( fd->tlsWantRead & fd->tlsReadWantsWrite ) );

			if ( ( wantRead || wantWrite ) && fd->fd >= nfds )
				nfds = fd->fd + 1;

			if ( wantRead )
				FD_SET( fd->fd, &readSet );

			if ( wantWrite )
				FD_SET( fd->fd, &writeSet );

			/* Use this opportunity to reset the round abort flag. */
			fd->abortRound = false;
		}

		/* Use what's left on the genf timer, or select a default for breaking
		 * out of select. */
		timeval tv, avs, *pvs = 0;
		if ( timer != 0 ) {
			tv.tv_sec = left.tv_sec;
			tv.tv_usec = left.tv_usec;
			pvs = &tv;
		}

		if ( selectTimeout > 0 && pvs == 0 ) {
			/* Wait no longer than selectTimeout when there is no timer. This
			 * can allow us to read messages when msg-signaling is turned off. */
			tv.tv_sec = selectTimeout;
			tv.tv_usec = 0;
			pvs = &tv;
		}

		/* Factor in the ares timeout. */
		pvs = ares_timeout( ac, pvs, &avs );

		/* Convert to timespec. */
		timespec ts, *pts = 0;
		if ( pvs != 0 ) {
			ts.tv_sec = pvs->tv_sec;
			ts.tv_nsec = pvs->tv_usec * 1000;
			pts = &ts;
		}

		/* If the nothing was added to any select loop then nfds will be zero
		 * and the file descriptor sets will be empty. This is is a portable
		 * sleep (with signal handling) according to select manpage. */
		int result = pselect( nfds, &readSet, &writeSet, 0, pts, sigmask );

		/*
		 * Signal handling first.
		 */
		if ( result < 0 ) {
			if ( errno == EINTR ) {
				if ( wantPoll )
					while ( poll() ) {}

				handleSignal( funnelSig );
				continue;
			}

			if ( errno != EAGAIN ) 
				log_FATAL( "select returned an unexpected error " << strerror(errno) );
		}
		else {
			/* If a signal is pending and a file descriptor is ready then the
			 * signal is not delivered. This makes it possible for file
			 * descriptors to prevent signals from ever being delivered.
			 * Therefore we need to poll signals when file descriptors are
			 * ready.
			 *
			 * This covers us for the funnelled signals, but not for SIGUSR
			 * (since they are always unmasked and don't get passed in here).
			 * But that is okay because they are only used to break from the
			 * select call.
			 */
			if ( sigmask != 0 ) {
				sigset_t check = *sigmask;
				sigpending( &check );
				while ( !sigisemptyset( &check ) ) {
					int r, sig;
					r = sigwait( &check, &sig );
					if ( r != 0 ) {
						log_ERROR( "sigwait returned: " << strerror(r) );
						continue;
					}
		
					handleSignal( sig );
					sigdelset( &check, sig );
				}
			}
		}

		/*
		 * Timers
		 */
		if ( timer != 0 ) {
			/* Find the time between the last timer execution and now.
			 * Subtract that from the timer interval to establish the
			 * amount of time left. That value will be used in the next
			 * pselect call. If it is <= zero then execute a timer run,
			 * save the last time and reset the amount left to the full
			 * amount. */
			struct timeval now, elapsed;
			gettimeofday( &now, 0 );
			timersub( &now, &last, &elapsed );
			timersub( timer, &elapsed, &left );
			if ( left.tv_sec < 0 || ( left.tv_usec == 0 && left.tv_sec == 0 ) ) {
				handleTimer();
				left = *timer;
				last = now;
			}
		}

		/* Ares library */
		ares_process( ac, &readSet, &writeSet );

		/*
		 * Handle file descriptors.
		 */
		if ( result > 0 ) {
			for ( SelectFdList::Iter fd = selectFdList; fd.lte(); ) {
				/* Prepare for possibility that the file descriptor will be
				 * removed from the select list. */
				SelectFdList::Iter next = fd.next();

				/* Check for round abort on the FD. */
				if ( !fd->abortRound ) {
					uint8_t readyField = 0;
					if ( FD_ISSET( fd->fd, &readSet ) )
						readyField |= READ_READY;
					if ( FD_ISSET( fd->fd, &writeSet ) )
						readyField |= WRITE_READY;

					if ( readyField )
						_selectFdReady( fd, readyField );
				}

				fd = next;
			}
		}

		if ( wantPoll )
			while ( poll() ) {}
	}

	/* finalTimerRun( c ); */
	/* close( listenFd ); */

	return 0;
}

int Thread::selectLoop( timeval *timer, bool wantPoll )
{
	sigset_t set;
	sigemptyset( &set );

	/* Funnel sigs go to main. For pselect we need to mask what we don't want.
	 * We leave the user sigs unmasked. */
	funnelSigs( &set );

	return pselectLoop( &set, timer, wantPoll );
}

static void lookupCallback( void *arg, int status, int timeouts, unsigned char *abuf, int alen )
{
	SelectFd *fd = static_cast<SelectFd*>(arg);
	fd->thread->_lookupCallback( fd, status, timeouts, abuf, alen );
}

void Thread::asyncLookup( SelectFd *selectFd, const char *host )
{
	if ( selectFd->remoteHost == 0 )
		selectFd->remoteHost = strdup(host);
	ares_query( ac, host, ns_c_in, ns_t_a, ::lookupCallback, selectFd );
}

void Thread::connectLookupComplete( SelectFd *fd, int status, int timeouts, unsigned char *abuf, int alen )
{
	Connection *c = static_cast<Connection*>(fd->local);

	if ( status == ARES_SUCCESS ) {
		/* Parse the reply. */
		int naddrttls = 10;
		ares_addrttl addrttls[naddrttls];
		ares_parse_a_reply( abuf, alen, 0, addrttls, &naddrttls );

		/* Pull out the first address. */
		for ( int i = 0; i < naddrttls; i++ ) {
			//log_debug( DBG_DNS, "result: " << inet_ntoa( addrttls[i].ipaddr ) << " " << addrttls[i].ttl );
		}


		if ( naddrttls > 0 ) {
			//log_debug( DBG_THREAD, "lookup succeeded, connecting to " << inet_ntoa( addrttls[0].ipaddr ) );

			sockaddr_in servername;
			servername.sin_family = AF_INET;
			servername.sin_port = htons(fd->port);
			servername.sin_addr = addrttls[0].ipaddr;

			int connFd = inetConnect( &servername, true );

			fd->typeState = SelectFd::Connect;
			fd->fd = connFd;
			fd->wantWrite = true;

			c->onSelectList = true;
			selectFdList.append( fd );
		}
	}
	else {
		c->failure( Connection::LookupFailure );
	}
}

void Thread::_lookupCallback( SelectFd *fd, int status, int timeouts, unsigned char *abuf, int alen )
{
	switch ( fd->type ) {
		case SelectFd::User:
			lookupCallback( fd, status, timeouts, abuf, alen );
			break;
		case SelectFd::Connection:
			connectLookupComplete( fd, status, timeouts, abuf, alen );
			break;
		case SelectFd::PktListen:
			break;
	}
}

int Thread::inetConnect( sockaddr_in *sa, bool nonBlocking )
{
	/* Create the socket. */
	int fd = socket( PF_INET, SOCK_STREAM, 0 );
	if ( fd < 0 ) {
		log_ERROR( "inet connect: socket creation failed: " << strerror(errno) );
		return -1;
	}

	if ( nonBlocking ) {
		bool nbres = makeNonBlocking( fd );
		if ( !nbres ) {
			log_ERROR( "failed to make FD non-blocking" );
		}
	}
	
	/* Connect to the listener. */
	int connectRes = ::connect( fd, (sockaddr*)sa, sizeof(*sa) );
	if ( connectRes < 0 && errno != EINPROGRESS ) {
		::close( fd );
		log_ERROR( "inet connect: name connect call failed: " << strerror(errno) );
		return -1;
	}

	return fd;
}

void Thread::initId()
{
	tid = syscall( SYS_gettid );
	pthread_setspecific( thisKey, this );
	pthread_this = pthread_self();
	if ( pendingNotifSignal )
		pthread_kill( pthread_this, SIGUSR1 );

}

char *Thread::pktFind( Rope *rope, long l )
{
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

	return out;
}
