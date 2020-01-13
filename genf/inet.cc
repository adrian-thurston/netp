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

bool Thread::makeNonBlocking( int fd )
{
	/* Make FD non-blocking. */
	int flags = fcntl( fd, F_GETFL );
	if ( flags == -1 ) {
		log_ERROR( "fcntl(" << fd << "): F_GETFL: " << strerror(errno) );
		return false;
	}

	int res = fcntl( fd, F_SETFL, flags | O_NONBLOCK );
	if ( res == -1 ) {
		log_ERROR( "fcntl(" << fd << "): F_SETFL: " << strerror(errno) );
		return false;
	}

	return true;
}

int Thread::inetListen( uint16_t port, bool transparent )
{
	/* Create the socket. */
	int listenFd = socket( PF_INET, SOCK_STREAM, 0 );
	if ( listenFd < 0 ) {
		log_ERROR( "inet listen: unable to allocate socket: " << strerror(errno) );
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
			log_ERROR( "inet listen: failed to set "
					"IP_TRANSPARENT flag on socket: " << strerror(r) );
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
		if ( !nbres )
			log_ERROR( "inet connect: failed to make FD non-blocking" );
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

