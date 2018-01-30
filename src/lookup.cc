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

/* Static callback for ares_query. Calls into class. */
static void lookupCallbackQuery( void *arg, int status, int timeouts,
		unsigned char *abuf, int alen )
{
	SelectFd *fd = static_cast<SelectFd*>(arg);
	fd->thread->lookupCallbackQuery( fd, status, timeouts, abuf, alen );
}

/* Static callback for ares_gethostbyname. Calls into class. */
static void lookupCallbackHost( void *arg, int status, int timeouts,
		struct hostent *hostent )
{
	SelectFd *fd = static_cast<SelectFd*>(arg);
	fd->thread->lookupCallbackHost( fd, status, timeouts, hostent );
}

void Thread::asyncLookupQuery( SelectFd *selectFd, const char *host )
{
	if ( selectFd->remoteHost == 0 )
		selectFd->remoteHost = strdup(host);
	ares_query( ac, host, ns_c_in, ns_t_a, ::lookupCallbackQuery, selectFd );
}

void Thread::asyncLookupHost( SelectFd *selectFd, const char *host )
{
	if ( selectFd->remoteHost == 0 )
		selectFd->remoteHost = strdup(host);
	ares_gethostbyname( ac, host, AF_INET, ::lookupCallbackHost, selectFd );
}

void Thread::lookupCallbackQuery( SelectFd *fd, int status, int timeouts,
		unsigned char *abuf, int alen )
{
	Connection *c = static_cast<Connection*>(fd->local);

	if ( status == ARES_SUCCESS ) {
		/* Parse the reply. */
		int naddrttls = 10;
		ares_addrttl addrttls[naddrttls];
		ares_parse_a_reply( abuf, alen, 0, addrttls, &naddrttls );

		/* Pull out the first address. */
		for ( int i = 0; i < naddrttls; i++ ) {
			//log_debug( DBG_DNS, "result: " <<
			//	inet_ntoa( addrttls[i].ipaddr ) << " " << addrttls[i].ttl );
		}


		if ( naddrttls > 0 ) {
			//log_debug( DBG_THREAD, "lookup succeeded, connecting to " <<
			//	inet_ntoa( addrttls[0].ipaddr ) );

			sockaddr_in servername;
			servername.sin_family = AF_INET;
			servername.sin_port = htons(fd->port);
			servername.sin_addr = addrttls[0].ipaddr;

			int connFd = inetConnect( &servername, true );

			fd->state = SelectFd::Connect;
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

void Thread::lookupCallbackHost( SelectFd *fd, int status, int timeouts,
		struct hostent *hostent )
{
	Connection *c = static_cast<Connection*>(fd->local);

	if ( status == ARES_SUCCESS ) {
		sockaddr_in servername;
		servername.sin_family = AF_INET;
		servername.sin_port = htons(fd->port);
		memcpy( &servername.sin_addr, hostent->h_addr, sizeof(servername.sin_addr) );

		int connFd = inetConnect( &servername, true );

		fd->state = SelectFd::Connect;
		fd->fd = connFd;
		fd->wantWrite = true;

		c->onSelectList = true;
		selectFdList.append( fd );
	}
	else {
		c->failure( Connection::LookupFailure );
	}
}


