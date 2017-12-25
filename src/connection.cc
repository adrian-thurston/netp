#include "thread.h"

#include <openssl/ssl.h>

int Connection::read( char *data, int len )
{
	if ( tlsConnect ) {
		return thread->tlsRead( selectFd, data, len );
	}
	else {
		return ::read( selectFd->fd, data, len );
	}
}

int Connection::write( char *data, int len )
{
	return thread->tlsWrite( selectFd, data, len );
}

void Connection::initiateTls( const char *host, uint16_t port )
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

