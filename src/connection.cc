#include "thread.h"

#include <openssl/ssl.h>
#include <errno.h>

int Connection::read( char *data, int len )
{
	if ( tlsConnect ) {
		return thread->tlsRead( selectFd, data, len );
	}
	else {
		int res = ::read( selectFd->fd, data, len );
		if ( res < 0 ) {
			if ( errno == EAGAIN || errno == EWOULDBLOCK ) {
				/* Nothing now. */
				return 0;
			}
			else {
				/* closed. */
				return -1;
			}
		}
		else if ( res == 0 ) {
			/* Normal closure. */
			return -1;
		}

		return res;
	}
}

int Connection::write( char *data, int len )
{
	if ( tlsConnect ) {
		return thread->tlsWrite( selectFd, data, len );
	}
	else {
		int res = ::write( selectFd->fd, data, len );
		if ( res < 0 ) {
			if ( errno == EAGAIN || errno == EWOULDBLOCK ) {
				/* Cannot write anything now. */
				return 0;
			}
			else {
				/* error-based closure. */
				return -1;
			}
		}
		return res;
	}
}

void Connection::initiateTls( const char *host, uint16_t port )
{
	selectFd = new SelectFd( thread, 0, 0 );
	selectFd->local = this;

	tlsConnect = true;
	selectFd->type = SelectFd::Connection;
	selectFd->state = SelectFd::Lookup;
	selectFd->port = port;

	thread->asyncLookupHost( selectFd, host );
}

void Connection::initiatePkt( const char *host, uint16_t port )
{
	selectFd = new SelectFd( thread, -1, 0 );
	selectFd->local = this;

	tlsConnect = false;
	selectFd->type = SelectFd::Connection;
	selectFd->state = SelectFd::Lookup;
	selectFd->port = port;

	thread->asyncLookupHost( selectFd, host );
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

		selectFd->closed = true;
	}

	selectFd = 0;
	closed = true;
}

void PacketConnection::readReady()
{
	thread->parsePacket( selectFd );
}

