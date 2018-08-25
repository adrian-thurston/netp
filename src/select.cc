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

static int funnelSig = 0;

void thread_funnel_handler( int s )
{
	funnelSig = s;
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

void Thread::selectFdReady( SelectFd *fd, uint8_t readyMask )
{
	switch ( fd->type ) {
		case SelectFd::User:
			userFdReady( fd, readyMask );
			break;
		case SelectFd::Listen:
			listenReady( fd, readyMask );
			break;
		case SelectFd::Connection: {
			switch ( fd->state ) {
				case SelectFd::Lookup:
					/* Shouldn't happen. When in lookup state, events happen on
					 * the resolver. */
					break;
				case SelectFd::Connect:
					connConnectReady( fd, readyMask );
					break;
				case SelectFd::TlsAccept:
					connTlsAcceptReady( fd );
					break;
				case SelectFd::TlsConnect:
					connTlsConnectReady( fd );
					break;
				case SelectFd::TlsEstablished:
					connTlsEstablishedReady( fd, readyMask );
					break;
				case SelectFd::Established:
					connEstablishedReady( fd, readyMask );
					break;
			}
			break;
		}
		case SelectFd::Process: {
			Process *process = (Process*)fd->local;
			if ( readyMask & READ_READY )
				process->readReady( fd );

			if ( readyMask & WRITE_READY )
				process->writeReady( fd );
			break;
		}
	}
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
		
		for ( SelectFdList::Iter fd = selectFdList; fd.lte(); ) {
			/* May be detaching fd. */
			SelectFdList::Iter next = fd.next();

			if ( fd->closed ) {
				selectFdList.detach( fd );
			}
			else {
				// log_message( "state: " << fd->state );
				bool wantRead =
						( !fd->tlsEstablished ) ?
						fd->wantRead :
						( fd->tlsWantRead || ( fd->tlsWantWrite && fd->tlsWriteWantsRead ) );

				bool wantWrite =
						( !fd->tlsEstablished ) ?
						fd->wantWrite :
						( fd->tlsWantWrite || ( fd->tlsWantRead && fd->tlsReadWantsWrite ) );

				if ( ( wantRead || wantWrite ) && fd->fd >= nfds )
					nfds = fd->fd + 1;

				if ( wantRead ) {
					if ( FD_ISSET( fd->fd, &readSet ) )
						log_ERROR( "fd " << fd->fd << " added to read set more than once" );

					FD_SET( fd->fd, &readSet );
				}

				if ( wantWrite ) {
					if ( FD_ISSET( fd->fd, &writeSet ) )
						log_ERROR( "fd " << fd->fd << " added to write set more than once" );

					FD_SET( fd->fd, &writeSet );
				}
			}

			fd = next;
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
			for ( SelectFdList::Iter fd = selectFdList; fd.lte(); fd++ ) {
				/* Skip any closed FDs. */
				if ( fd->closed )
					continue;

				/* Check for round abort on the FD. */
				uint8_t readyField = 0;
				if ( FD_ISSET( fd->fd, &readSet ) )
					readyField |= READ_READY;
				if ( FD_ISSET( fd->fd, &writeSet ) )
					readyField |= WRITE_READY;

				if ( readyField )
					selectFdReady( fd, readyField );
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


