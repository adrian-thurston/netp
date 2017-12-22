#include "thread.h"

#include <openssl/ssl.h>

void Connection::initiate( const char *host, uint16_t port )
{
	thread->initiateConnection( selectFd, host, port );
}

void Connection::close( )
{
	if ( selectFd != 0 ) {
		if ( selectFd->ssl != 0 ) {
			SSL_shutdown( selectFd->ssl );
		    SSL_free( selectFd->ssl );
		}

		if ( selectFd->fd >= 0 )
		    ::close( selectFd->fd );

		if ( onSelectList != 0 )
			thread->selectFdList.detach( selectFd );

		delete selectFd;
	}

	selectFd = 0;
	closed = true;
}

