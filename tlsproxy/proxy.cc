#include "proxy.h"
#include "main.h"
#include <parse/pattern.h>
#include <parse/module.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <stdio.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>

#include "listen.h"
#include "genf.h"

#include <sys/mman.h>
#include <linux/if_ether.h>

#define DAEMON_DAEMON_TIMEOUT 100

const char verifyResponse[] =
	"HTTP/1.1 422 Unprocessable Entity\r\n"
	"\r\n"
	"proxy could not verify TLS connection to server\r\n";

void ProxyThread::configureContext( Context *ctx )
{    
	moduleList.proxyConfigureContext( this, ctx );
}   

ContextMap::ContextMap()
:
	nextConId(0)
{
	pthread_mutex_init( &mutex, 0 );

	String caKeyFile = String( PKGSTATEDIR "/CA/cakey.pem" );
	String caCertFile = String( PKGSTATEDIR "/CA/cacert.pem" );
	String serialFile = String( PKGSTATEDIR "/CA/serial" );

	capkey = loadKey( caKeyFile );
	cacert = loadCert( caCertFile );

	serial = loadSerial( serialFile );
}

void ContextMap::close()
{
	String serialFile = String( PKGSTATEDIR "/CA/serial" );
	saveSerial( serialFile, serial );
}

int ContextMap::keysAvail()
{
	int avail;

	pthread_mutex_lock( &mutex );
	avail = keyList.length();
	pthread_mutex_unlock( &mutex );
	return avail;
}

void ContextMap::addKeys( int amt )
{
	for ( int i = 0; i < amt; i++ ) {
		EVP_PKEY *key = genrsa( 2048 );

		pthread_mutex_lock( &mutex );
		keyList.append( key );
		pthread_mutex_unlock( &mutex );
	}
}

SSL_CTX *ContextMap::serverCtx( ProxyThread *proxyThread, std::string host )
{
	bool create = false, store = false;
	SSL_CTX *rtnVal = 0;
	BIGNUM *useSerial = 0;
	EVP_PKEY *pkey = 0;
	X509 *x509;

	/* Lock. */
	pthread_mutex_lock( &mutex );

	CtxMap::El *el = ctxMap.find( host );
	if ( el == 0 ) {
		/* We will create and store it, since we were the first to request it.
		 * */
		create = store = true;
		ctxMap.insert( host, 0, &el );
	}
	else {
		/* A record is present in the map. */
		if ( el->value != 0 ) {
			/* Value is also there, we can use it. Value is there. We can
			 * return it. */
			rtnVal = el->value;
		}
		else {
			/* If the value is null then some other thread is also creating it.
			 * it will be the one to store it. */
			create = true;
			store = false;
		}
	}

	if ( create ) {
		useSerial = BN_dup( serial );

		/* FIXME: could run out here. Need to make them when the cache is
		 * empty. */
		if ( keyList.length() > 0 ) {
			pkey = keyList.head->value;
			delete keyList.detachFirst();
		}

		BN_add_word( serial, 1 );
	}

	/* Unlock. */
	pthread_mutex_unlock( &mutex );

	if ( create ) {
		log_debug( DBG_PROXY, "creating SSL context ourselves" );

		/* Our job to create the cert. We don't do this under lock. */
		makeCert( capkey, cacert, useSerial, &pkey, &x509, host.c_str() );

		SSL_CTX *ctx = proxyThread->sslCtxServer( pkey, x509 );

		log_debug( DBG_PROXY, "finished creating SSL context" );

		/* Update the element. So new requests see the context. Afterward we
		 * can notify any other proxy threads of the completed creation. Once
		 * this critical section completes we can be sure that the notify array
		 * will not be touched again. Safe to iterate it not under lock. */
		if ( store ) {
			pthread_mutex_lock( &mutex );
			el->value = ctx;
			pthread_mutex_unlock( &mutex );
		}

		rtnVal = ctx;

		/* Notify service thread. */
		Message::CertGenerated *msg = proxyThread->sendsToService->openCertGenerated();
		msg->set_host( proxyThread->sendsToService->writer, host.c_str() );
		proxyThread->sendsToService->send();
	}

	return rtnVal;
}

long ContextMap::connId()
{
	return __sync_add_and_fetch( &nextConId, 1 );
}


int FdDesc::readAvail()
{
	if ( wbList.length() == 0 )
		return 0;

	return wbList.tail->blocklen - wbList.tail->tail;
}

char *FdDesc::readTo()
{
	return wbList.tail->data + wbList.tail->tail;
}

void FdDesc::readIn( int amt )
{
	wbList.tail->tail += amt;
}

char *FdDesc::writeFrom()
{
	return wbList.head->data + wbList.tail->head;
}

int FdDesc::writeAvail()
{
	return wbList.length() == 0 ? 0 : wbList.head->tail - wbList.head->head;
}

int FdDesc::addSpace()
{
	wbList.append( new WriteBlock );
	return wbList.tail->blocklen;
}

void FdDesc::consume( int amount )
{
	WriteBlock *head = wbList.head;
	if ( head->head + amount >= head->tail ) {
		/* Consumed the whole head. */
		wbList.detach( head );

		/* The delete will clean. */
		delete head;
	}
	else {
		/* Consume part of the head. */
		wbList.head->head += amount;
	}
}

std::ostream &operator<<( std::ostream &out, FdDesc *fdDesc )
{
	out << ( fdDesc->type == FdDesc::Client ?
			"client" : "server" ) << ": " <<
			( (void*) ( 0xffffffff & (unsigned long)fdDesc->proxyConn->selectFd ) ) <<
			'(' << fdDesc->proxyConn->selectFd->fd << ')';
	return out;
}

ProxyConnection::ProxyConnection( ProxyThread *proxyThread, int connId, FdDesc::Type type )
:
	Connection( proxyThread ),
	proxyThread( proxyThread )
{
	fdDesc = new FdDesc( connId, type, this );
}
	
ProxyListener::ProxyListener( ProxyThread *proxyThread )
:
	Listener( proxyThread ),
	proxyThread( proxyThread )
{}

ProxyConnection *ProxyListener::connectionFactory( int fd )
{
	/* Allocate an identifier for this connection. */
	long connId = proxyThread->contextMap->connId();

	ProxyConnection *serverConn = new ProxyConnection( proxyThread, connId, FdDesc::Server );

	ProxyConnection *clientConn = new ProxyConnection( proxyThread, connId, FdDesc::Client );

	/* Connect the two FdDesc ends to each other. */
	FdDesc::connect( clientConn->fdDesc, serverConn->fdDesc );

	//log_debug( DBG_PROXY, clientConn->fdDesc << ": " <<
			//serverConn->fdDesc << ": " << "proxy startup" );

	/* Extract the intended server IP address and initiate connection to the
	 * true server. */
	sockaddr_in addr;
	socklen_t len = sizeof(addr);
	getsockname( fd, (struct sockaddr*)&addr, &len );

	/* Setting tls false, but setting up the context and host check because
	 * we will convert this to a TLS connection in the connection
	 * notification. */
	clientConn->initiate( &addr, false, proxyThread->clientCtx, true );

	return serverConn;
}

/* Server name available in client SSL connection. */
void ProxyThread::serverName( SelectFd *selectFd, const char *host )
{
	log_debug( DBG_PROXY, "server name: " << host );

	FdDesc *fdDesc = fdLocal( selectFd );

	/* Retreive or create the CTX. */
	SSL_CTX *newCtx = contextMap->serverCtx( this, host );

	/* Stash and carry on. */
	SSL_set_SSL_CTX( selectFd->ssl, newCtx );

	fdDesc->haveHostname = true;
	fdDesc->hostname = host;

	/* If the client connection is ready we can start tls. */
	maybeStartTlsClient( fdDesc->other );
}

/* Called by proxy threads in TLS accept. Installed into the server context in
 * the listen thread. */
int sslServerNameCallback( SSL *ssl, int *al, void * )
{
	const char *sn = SSL_get_servername( ssl, TLSEXT_NAMETYPE_host_name );

	if ( sn != 0 ) {
		ProxyThread *proxyThread = static_cast<ProxyThread*>( Thread::getThis() );
		SelectFd *selectFd = (SelectFd*)SSL_get_ex_data( ssl, 0 );
		proxyThread->serverName( selectFd, sn );
		return SSL_TLSEXT_ERR_OK;
	}

	return SSL_TLSEXT_ERR_NOACK;
}


/* Client SSL connection has succeeded. */
void ProxyConnection::failure( FailType failType )
{
	if ( failType == SslPeerFailedVerify )
		proxyThread->sslPeerFailedVerify( selectFd );
	else {
		// probably: proxyShutdown( fd );
	}
}

void ProxyConnection::notifyAccept( )
{
	selectFd->tlsWantRead = true;

	/* Immediately try to read. Is this necessary? IE will the connect
	 * leave data on the FD or buffer it in? */
	proxyThread->sslReadReady( selectFd );
}

/* Maybe initiate the TLS conenction to the server. Can happen when connected
 * and when we have a hostname from the client. */
void ProxyThread::maybeStartTlsClient( FdDesc *clientDesc )
{
	FdDesc *serverDesc = clientDesc->other;
	if ( clientDesc->connected && serverDesc->haveHostname ) {
		log_debug( DBG_PROXY, "starting TLS connection to server: " << serverDesc->hostname );

		startTlsClient( clientCtx, clientDesc->proxyConn->selectFd, serverDesc->hostname.c_str() );

		clientDesc->proxyConn->selectFd->type = SelectFd::Connection;
		clientDesc->proxyConn->selectFd->state = SelectFd::TlsConnect;
	}
}

void ProxyThread::sslPeerFailedVerify( SelectFd *fd )
{
	/* Need to propagate the verify failure to the client connection.
	 * Ideally we could push the SSL error over to the client, but not sure
	 * if one, the TLS protocol supports this, two, if supported by TLS,
	 * then does the openssl library support it.
	 *
	 * Sending the error over the HTTP protocol. This code here assumes
	 * HTTP. If we are proxying other protocols we need to send an error
	 * according to what is supported there. */
	FdDesc *fdDesc = fdLocal( fd );
	FdDesc *other = fdDesc->other;

	/* This needs work. Assuming buffer has enough space for error
	 * message. */
	other->addSpace();
	char *data = other->readTo();
	strcpy( data, verifyResponse );
	other->readIn( strlen(data) );
	proxyWrite( other );
	proxyShutdown( fd );
}

void ProxyConnection::connectComplete()
{
	if ( selectFd->state == SelectFd::Established ) {
		log_debug( DBG_PROXY, "socket connect completed" );

		proxyThread->fdLocal(selectFd)->connected = true;

		/* Hack: Go back into the connect state as we are waiting in some kind of limbo. */
		selectFd->state = SelectFd::Connect;
		selectFd->wantWrite = false;
		selectFd->wantRead = false;

		/* If we have the hostname from client we can initiate Tls with the server. */
		proxyThread->maybeStartTlsClient( proxyThread->fdLocal(selectFd) );
	}
	else if ( selectFd->state == SelectFd::TlsEstablished ) {
		proxyThread->connectComplete( selectFd );
	}
}

/* TLS connect to server has completed. */
void ProxyThread::connectComplete( SelectFd *fd )
{
	if ( fd->sslVerifyError ) {
		/* Need to propagate the verify failure to the client connection.
		 * Ideally we could push the SSL error over to the client, but not sure
		 * if one, the TLS protocol supports this, two, if supported by TLS,
		 * then does the openssl library support it.
		 *
		 * Sending the error over the HTTP protocol. This code here assumes
		 * HTTP. If we are proxying other protocols we need to send an error
		 * according to what is supported there. */
		FdDesc *fdDesc = fdLocal( fd );
		FdDesc *other = fdDesc->other;

		/* This needs work. Assuming buffer has enough space for error
		 * message. */
		other->addSpace();
		char *data = other->readTo();
		strcpy( data, verifyResponse );
		other->readIn( strlen(data) );
		proxyWrite( other );
		proxyShutdown( fd );
	}
	else {
		log_debug( DBG_PROXY, "TLS connect completed" );

		/* Go into the established state and start reading. We will buffer data
		 * until the other half enteres established as well. */
		fd->type = SelectFd::Connection;
		fd->state = SelectFd::TlsEstablished;

		fd->tlsEstablished = true;
		fd->tlsWantRead = true;

		/* Attempt an initial read. Is this necessary? IE will the connect
		 * leave data on the FD or buffer it in? */
		sslReadReady( fd );
	}
}


int ProxyThread::kdata( long id, int type, const char *remoteHost, char *data, int len )
{
	handler.decrypted( id, type, remoteHost, (unsigned char*)data, len );
	return 0;
}

int ProxyThread::proxyWrite( FdDesc *fdDesc )
{
	/* If not in the established state we cannot write anything. Just queue.
	 * Just indicate we want a write and wait the established state. This is
	 * only imporant when called from a read on the other half. */
	if ( fdDesc->proxyConn->selectFd->state != SelectFd::TlsEstablished ) {
		log_debug( DBG_PROXY, "write: socket not established, saying want write" );
		fdDesc->proxyConn->selectFd->tlsWantWrite = true;
		return 0;
	}

	while ( true ) {
		int length = fdDesc->writeAvail();
		if ( length == 0 ) {
			fdDesc->proxyConn->selectFd->tlsWantWrite = false;
			break;
		}

		char *data = fdDesc->writeFrom();

		log_debug( DBG_PROXY, fdDesc << ": writing bytes: " << length );

		int written = tlsWrite( fdDesc->proxyConn->selectFd, data, length );

		if ( written < 0 ) {
			log_debug( DBG_PROXY, "write error, closing" );

			/* Failure on write. Nothing much to do here, except shut the whole
			 * business down. */
			//int sslError = SSL_get_error( fd->ssl, result );
			proxyShutdown( fdDesc->proxyConn->selectFd );
			break;
		}
		else if ( written == 0 ) {
			/* Wrote nothing for a legit reason. The want-read and want-write
			 * flags will be set appropriately. */
			fdDesc->proxyConn->selectFd->tlsWantWrite = true;
			break;
		}
		else {
			/*
			 * Wrote something.
			 */

			int type = fdDesc->type == FdDesc::Client ? KDATA_DIR_CLIENT : KDATA_DIR_SERVER;

//			kdata_write_decrypted( &kring, fdDesc->connId, type,
//					fdDesc->proxyConn->selectFd->remoteHost, data, written );
			kdata( fdDesc->connId, type, fdDesc->proxyConn->selectFd->remoteHost, data, written );

			/* Update what we wrote and continue to try to write more. */
			fdDesc->consume( written );
		}
	}

	return -1;
}

bool ProxyThread::sslReadReady( SelectFd *fd )
{
	FdDesc *fdDesc = fdLocal( fd );

	while ( true ) {
		FdDesc *other = fdDesc->other;

		int length = other->readAvail();
		if ( length == 0 ) {
			other->addSpace();
			length = other->readAvail();
		}
		char *data = other->readTo();

		int nbytes = tlsRead( fd, data, length );

		if ( nbytes < 0 ) {
			log_debug( DBG_PROXY, fdDesc << ": read error, closing" );

			/* Fatal error. */
			proxyShutdown( fd );
			break;
		}
		else if ( nbytes > 0 ) {
			/* Successful read. Initialize the write. */
			other->readIn( nbytes );

			log_debug( DBG_PROXY, fdDesc << ": received bytes: " << nbytes << " bytes" );

			proxyWrite( other );

			/* If we are still in the established state after writing then try
			 * for another. FIXME: Probably we don't want to do this
			 * (starvation). */
			if ( !fdDesc->proxyConn->selectFd->closed )
				continue;
		}
		else {
			log_debug( DBG_PROXY, fdDesc << ": nothing to read" );
		}

		break;
	}

	return true;
}

void ProxyConnection::readReady()
{
	proxyThread->sslReadReady( selectFd );
}

void ProxyConnection::writeReady()
{
	proxyThread->proxyWrite( fdDesc );
}

void ProxyThread::writeRetry( SelectFd *fd )
{
	FdDesc *fdDesc = fdLocal( fd );
	proxyWrite( fdDesc->other );
}

/* Fatal proxy error. Shuts down both ends. */
void ProxyThread::proxyShutdown( SelectFd *fd )
{
	/* Fatal error. Shut the whole business down. Report the error and close
	 * this end. Close and/or put a stop to other end. */
	FdDesc *fdDesc = fdLocal( fd );

	log_debug( DBG_PROXY, fdDesc << ": " <<
			fdDesc->other << ": " << "proxy shutdown" );

	::close( fd->fd );

	fd->closed = true;
	fd->wantRead = false; 
	fd->wantWrite = false; 

	if ( fdDesc->other->proxyConn->selectFd->ssl != 0 )
		SSL_shutdown( fdDesc->other->proxyConn->selectFd->ssl );

	if ( fdDesc->other->proxyConn->selectFd->fd >= 0 )
		::close( fdDesc->other->proxyConn->selectFd->fd ); 

	fdDesc->other->proxyConn->selectFd->closed = true;
	fdDesc->other->proxyConn->selectFd->wantRead = false; 
	fdDesc->other->proxyConn->selectFd->wantWrite = false; 
	fdDesc->other->stop = true;
}

void ProxyThread::recvShutdown( Message::Shutdown *msg )
{
	log_debug( DBG_PROXY, "received shutdown" );

	/* Break from our loop. */
	breakLoop();
}

int ProxyThread::main()
{
	ProxyListener *listener = new ProxyListener( this );

	listener->startListenOnFd( listenFd, true, serverCtx, false );

	int res = kring_open( &kring, KRING_DATA, "r1", KRING_DECRYPTED, ringId, KRING_WRITE );
	if ( res < 0 ) {
		log_ERROR( "decrypted data kring open for write failed: " <<
				kdata_error( &kring, res ) );
		return -1;
	}

	selectLoop();

	return 0;
}
