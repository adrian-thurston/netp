
#include <valgrind/memcheck.h>
#include <thread.h>

#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include "config.h"

#define PEER_CN_NAME_LEN 256

#define CA_CERT_FILE "/etc/ssl/certs/ca-certificates.crt"

static pthread_mutex_t crypto_mutex_arr[CRYPTO_NUM_LOCKS];

static void cryptoLock(int mode, int n, const char *file, int line)
{
	if ( mode & CRYPTO_LOCK )
		pthread_mutex_lock( &crypto_mutex_arr[n] );
	else
		pthread_mutex_unlock( &crypto_mutex_arr[n] );
}

static unsigned long cryptoId(void)
{
	return ((unsigned long) pthread_self());
} 

void SelectFd::close()
{
	SSL_shutdown( ssl );
	SSL_free( ssl );
	::close( fd );
	closed = true;
}

/* Do this once at startup. */
void Thread::tlsStartup( const char *randFile )
{
	for ( int i = 0; i < CRYPTO_NUM_LOCKS; i++ )
		pthread_mutex_init( &crypto_mutex_arr[i], 0 );
	
	CRYPTO_mem_ctrl( CRYPTO_MEM_CHECK_ON );

	/* Global initialization. */
	CRYPTO_set_locking_callback( cryptoLock );
	CRYPTO_set_id_callback( cryptoId ); 

	ERR_load_crypto_strings();
	SSL_load_error_strings();
	ERR_load_BIO_strings();
	OpenSSL_add_all_algorithms();
	SSL_library_init();

	if ( randFile != 0 )
		RAND_load_file( randFile, 1024 );
}

void Thread::tlsShutdown()
{

}

SSL_CTX *Thread::sslClientCtx()
{
	return sslClientCtx( CA_CERT_FILE );
}

SSL_CTX *Thread::sslClientCtx( const char *verify, const char *key, const char *cert )
{
	/* Create the SSL_CTX. */
	SSL_CTX *ctx = SSL_CTX_new(TLSv1_client_method());
	if ( ctx == NULL )
		log_FATAL( EC_SSL_NEW_CONTEXT_FAILURE << " SSL error: new context failure" );

	/* Load the CA certificates that we will use to verify. */
	int result = SSL_CTX_load_verify_locations( ctx, verify, NULL );
	if ( !result ) {
		log_ERROR( EC_SSL_CA_CERT_LOAD_FAILURE <<
				" failed to load CA cert file " << verify );
	}

	if ( key != 0 && cert != 0 ) {
		SSL_CTX_set_verify( ctx, SSL_VERIFY_PEER, NULL );

		result = SSL_CTX_use_PrivateKey_file( ctx, key, SSL_FILETYPE_PEM );
		if ( result != 1 )
			log_FATAL( "failed to load TLS key file " << key );

		result = SSL_CTX_use_certificate_chain_file( ctx, cert );
		if ( result != 1 )
			log_FATAL( "failed to load TLS certificates file " << cert );

	}

	log_message( "client verify mode: " << SSL_CTX_get_verify_mode( ctx ) );

	return ctx;
}

SSL_CTX *Thread::sslServerCtx( const char *key, const char *cert, const char *verify )
{
	/* Create the SSL_CTX. */
	SSL_CTX *ctx = SSL_CTX_new(SSLv23_method());
	if ( ctx == NULL )
		log_FATAL( EC_SSL_NEW_CONTEXT_FAILURE << " SSL error: new context failure" );

	int result = SSL_CTX_use_PrivateKey_file( ctx, key, SSL_FILETYPE_PEM );
	if ( result != 1 )
		log_FATAL( "failed to load TLS key file " << key );

	result = SSL_CTX_use_certificate_chain_file( ctx, cert );
	if ( result != 1 )
		log_FATAL( "failed to load TLS certificates file " << cert );

	if ( verify != 0 ) {
		SSL_CTX_set_verify( ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL );

		/* Load the CA certificates that we will use to verify. */
		int result = SSL_CTX_load_verify_locations( ctx, verify, NULL );
		if ( !result ) {
			log_ERROR( EC_SSL_CA_CERT_LOAD_FAILURE <<
					" failed to load CA cert file " << verify );
		}
	}

	log_message( "server verify mode: " << SSL_CTX_get_verify_mode( ctx ) );

	return ctx;
}

SSL_CTX *Thread::sslServerCtx( EVP_PKEY *pkey, X509 *x509 )
{
	/* Create the SSL_CTX. */
	SSL_CTX *ctx = SSL_CTX_new(SSLv23_method());
	if ( ctx == NULL )
		log_FATAL( EC_SSL_NEW_CONTEXT_FAILURE << " SSL error: new context failure" );

	int result = SSL_CTX_use_PrivateKey( ctx, pkey );
	if ( result != 1 )
		log_FATAL( "failed to load TLS key" );

	result = SSL_CTX_use_certificate( ctx, x509 );
	if ( result != 1 )
		log_FATAL( "failed to load TLS certificate" );

	return ctx;
}

void Thread::startTlsClient( SSL_CTX *clientCtx, SelectFd *selectFd, const char *remoteHost )
{
	bool nb = makeNonBlocking( selectFd->fd );
	if ( !nb )
		log_ERROR( "TLS start client: non-blocking IO not available" );

	BIO *bio = BIO_new_fd( selectFd->fd, BIO_NOCLOSE );

	/* Create the SSL object and set it in the secure BIO. */
	SSL *ssl = SSL_new( clientCtx );
	SSL_set_mode( ssl, SSL_MODE_AUTO_RETRY );
	SSL_set_bio( ssl, bio, bio );
	SSL_set_tlsext_host_name( ssl, remoteHost );

	selectFd->ssl = ssl;
	selectFd->bio = bio;

	if ( selectFd->remoteHost == 0 )
		selectFd->remoteHost = strdup(remoteHost);

	selectFd->wantRead = false;
	selectFd->wantWrite = true;

	SSL_set_ex_data( ssl, 0, selectFd );
}

void Thread::startTlsServer( SSL_CTX *defaultCtx, SelectFd *selectFd )
{
	BIO *bio = BIO_new_fd( selectFd->fd, BIO_NOCLOSE );

	/* Create the SSL object an set it in the secure BIO. */
	SSL *ssl = SSL_new( defaultCtx );
	SSL_set_mode( ssl, SSL_MODE_AUTO_RETRY );
	SSL_set_bio( ssl, bio, bio );

	selectFd->ssl = ssl;
	selectFd->bio = bio;
	SSL_set_ex_data( ssl, 0, selectFd );

	selectFd->wantRead = true;
	selectFd->wantWrite = false;

	selectFdList.append( selectFd );
}

void Thread::_tlsConnectResult( SelectFd *fd, int sslError )
{
	switch ( fd->type ) {
		case SelectFd::User:
			tlsConnectResult( fd, sslError );
			break;
		case SelectFd::Connection:
			if ( sslError == SSL_ERROR_NONE ) {
				Connection *c = static_cast<Connection*>(fd->local);
				c->connectComplete();

				fd->state = SelectFd::TlsEstablished;
				fd->tlsEstablished = true;
				fd->tlsWantRead = true;
				fd->wantWrite = false;
			}
			break;
		case SelectFd::Listen:
		case SelectFd::ConnListen:
			break;
	}
}

void Thread::clientConnect( SelectFd *fd )
{
	int result = SSL_connect( fd->ssl );
	if ( result <= 0 ) {
		/* No connect yet. May need more data. */
		result = SSL_get_error( fd->ssl, result );

		bool retry = prepNextRound( fd, result );
		if ( !retry ) {
			/* Not a retry failure. */
			_tlsConnectResult( fd, result );
		}
	}
	else {
		/* Check the verification result. */
		long verifyResult = SSL_get_verify_result( fd->ssl );
		if ( verifyResult != X509_V_OK ) {
			fd->sslVerifyError = true;

			log_ERROR( "ssl peer failed verify: " << fd->remoteHost );
			Connection *c = static_cast<Connection*>(fd->local);
			c->failure( Connection::SslPeerFailedVerify );
			c->close();
		}
		else {
			/* Check the cert chain. The chain length is automatically checked by
			 * OpenSSL when we set the verify depth in the CTX */

			/* Check the common name. */
			X509 *peer = SSL_get_peer_certificate( fd->ssl );
			char peer_CN[PEER_CN_NAME_LEN];
			X509_NAME_get_text_by_NID( X509_get_subject_name(peer),
					NID_commonName, peer_CN, PEER_CN_NAME_LEN );

#ifdef HAVE_X509_CHECK_HOST
			int cr = X509_check_host( peer, fd->remoteHost, strlen(fd->remoteHost), 0, 0 );
			if ( cr != 1 ) {
				fd->sslVerifyError = true;

				log_ERROR( "ssl peer cn host mismatch: requested " <<
						fd->remoteHost << " but cert is for " << peer_CN );

				Connection *c = static_cast<Connection*>(fd->local);
				c->failure( Connection::SslPeerCnHostMismatch );
				c->close();
			}
			else
#endif
			{
				/* Would like to require or implement this. Not available on 14.04. */
				/* #error no X509_check_host */

				/* Create a BIO for the ssl wrapper. */
				BIO *bio = BIO_new( BIO_f_ssl() );
				BIO_set_ssl( bio, fd->ssl, BIO_NOCLOSE );

				/* Just wrapped the bio, update in select fd. */
				fd->bio = bio;

				_tlsConnectResult( fd, SSL_ERROR_NONE );
			}
		}
	}
}

int Thread::tlsRead( SelectFd *fd, void *buf, int len )
{
	fd->tlsReadWantsWrite = false;

	int nbytes = BIO_read( fd->bio, buf, len );

	if ( nbytes > 0 ) {
		/* FIXME: should this be controlled by a debug build option */
		VALGRIND_MAKE_MEM_DEFINED( buf, nbytes );
	}
	else {
		if ( BIO_should_retry( fd->bio ) ) {
			/* Read failure is retry-related. */
			fd->tlsReadWantsWrite = BIO_should_write(fd->bio);

			/* Indicate wrote nothing for legit reason. */
			nbytes = 0;
		}
		else {
			/* Indicate error. */
			nbytes = -1;
		}
	}

	return nbytes;
}

int Thread::tlsWrite( SelectFd *fd, char *data, int length )
{
	fd->tlsWriteWantsRead = false;

	int written = BIO_write( fd->bio, data, length );

	if ( written <= 0 ) {
		/* If the BIO is saying it we should retry later, go back into select.
		 * */
		if ( BIO_should_retry( fd->bio ) ) {
			/* Write failure is retry-related. */
			fd->tlsWriteWantsRead = BIO_should_read(fd->bio);

			/* Indicate we wrote nothing for a legit reason. */
			written = 0;
		}
		else {
			/* Indicate error. */
			written = -1;
		}
	}

	return written;
}

void Thread::tlsError( RealmSet realm, int e )
{
	switch ( e ) {
		case SSL_ERROR_NONE:
			log_debug( realm, "ssl error: SSL_ERROR_NONE" );
			break;
		case SSL_ERROR_ZERO_RETURN:
			log_debug( realm, "ssl error: SSL_ERROR_ZERO_RETURN" );
			break;
		case SSL_ERROR_WANT_READ:
			log_debug( realm, "ssl error: SSL_ERROR_WANT_READ" );
			break;
		case SSL_ERROR_WANT_WRITE:
			log_debug( realm, "ssl error: SSL_ERROR_WANT_WRITE" );
			break;
		case SSL_ERROR_WANT_CONNECT:
			log_debug( realm, "ssl error: SSL_ERROR_WANT_CONNECT" );
			break;
		case SSL_ERROR_WANT_ACCEPT:
			log_debug( realm, "ssl error: SSL_ERROR_WANT_ACCEPT" );
			break;
		case SSL_ERROR_WANT_X509_LOOKUP:
			log_debug( realm, "ssl error: SSL_ERROR_WANT_X509_LOOKUP" );
			break;
		case SSL_ERROR_SYSCALL: {
			while ( true ) {
				int eq = ERR_get_error(); 
				if ( eq == 0 )
					break;

				log_debug(  realm, "SSL_ERROR_SYSCALL: : " << ERR_error_string( eq, NULL ) );
			}
			break;
		}
		case SSL_ERROR_SSL: {
			while ( true ) {
				int eq = ERR_get_error(); 
				if ( eq == 0 )
					break;

				log_debug(  realm, "SSL_ERROR_SSL: " << ERR_error_string( eq, NULL ) );
			}
			break;
		}
		default: {
			log_debug( realm, "SSL_ERROR_ERROR: not handled");
			break;
		}
	}
}

/* Returns true if failue was due to a retry request. */
bool Thread::prepNextRound( SelectFd *fd, int result )
{
	fd->wantRead = false;
	fd->wantWrite = false;
	if ( result == SSL_ERROR_WANT_READ )
		fd->wantRead = true;
	else if ( result == SSL_ERROR_WANT_WRITE )
		fd->wantWrite = true;

	return fd->wantRead || fd->wantWrite;
}

void Thread::tlsAccept( SelectFd *fd )
{
	bool nb = makeNonBlocking( fd->fd );
	if ( !nb )
		log_ERROR( "TLS accept: non-blocking IO not available" );

	int result = SSL_accept( fd->ssl );
	if ( result <= 0 ) {
		/* No accept yet, may need more data. */
		result = SSL_get_error( fd->ssl, result );

		bool retry = prepNextRound( fd, result );
		if ( !retry ) {
			/* Notify of connection error. */
			tlsAcceptResult( fd, result );
		}
	}
	else {
		long verifyResult = SSL_get_verify_result( fd->ssl );
		log_message( "SSL_accept: verify result: " <<
				( verifyResult == X509_V_OK ? "OK" : "FAILED" ) );

		/* Success. Stop the select loop. Create a BIO for the ssl wrapper. */
		BIO *bio = BIO_new(BIO_f_ssl());
		BIO_set_ssl( bio, fd->ssl, BIO_NOCLOSE );
		fd->bio = bio;

		//fd->wantRead = fd->wantWrite = false;

		tlsAcceptResult( fd, SSL_ERROR_NONE );
	}
}

void Thread::asyncConnect( SelectFd *fd, Connection *conn )
{
	if ( conn->tlsConnect ) {
		startTlsClient( threadClientCtx, fd, fd->remoteHost );
		// selectFdList.append( fd );
		fd->type = SelectFd::Connection;
		fd->state = SelectFd::TlsConnect;
	}
	else {
		/* FIXME: some type of notification here? */
		fd->type = SelectFd::Connection;
		fd->state = SelectFd::Established;
		fd->wantRead = true;
		conn->connectComplete();
	}
}

void Thread::_selectFdReady( SelectFd *fd, uint8_t readyMask )
{
	switch ( fd->type ) {
		case SelectFd::User: {
			selectFdReady( fd, readyMask );
			break;
		}

		case SelectFd::Listen: {
			sockaddr_in peer;
			socklen_t len = sizeof(sockaddr_in);

			int result = ::accept( fd->fd, (sockaddr*)&peer, &len );
			if ( result >= 0 ) {
				notifyAccept( result );
			}
			else {
				if ( errno != EAGAIN && errno != EWOULDBLOCK )
					log_ERROR( "failed to accept connection: " << strerror(errno) );
			}
			break;
		}

		case SelectFd::ConnListen: {
			sockaddr_in peer;
			socklen_t len = sizeof(sockaddr_in);

			int result = ::accept( fd->fd, (sockaddr*)&peer, &len );
			if ( result >= 0 ) {
				Listener *l = static_cast<Listener*>(fd->local);
				l->accept( result );
			}
			else {
				if ( errno != EAGAIN && errno != EWOULDBLOCK )
					log_ERROR( "failed to accept connection: " << strerror(errno) );
			}
			break;
		}
		case SelectFd::Connection: {
			switch ( fd->state ) {
				case SelectFd::Lookup:
					/* Shouldn't happen. When in lookup state, events happen on the resolver. */
					break;
				case SelectFd::Connect: {
					Connection *c = static_cast<Connection*>(fd->local);

					if ( readyMask & WRITE_READY ) {
						/* Turn off want write. We must do this before any notification
						 * below, which may want to turn it on. */
						fd->wantWrite = false;

						int option;
						socklen_t optlen = sizeof(int);
						getsockopt( fd->fd, SOL_SOCKET, SO_ERROR, &option, &optlen );
						if ( option == 0 ) {
							asyncConnect( fd, c );
						}
						else {
							log_ERROR( "failed async connect: " << strerror(option) );
							c->failure( Connection::AsyncConnectFailed );
							c->close();
						}
					}

					break;
				}

				case SelectFd::TlsAccept:
					tlsAccept( fd );
					break;

				case SelectFd::TlsConnect:
					clientConnect( fd );
					break;

				case SelectFd::TlsEstablished: {
					Connection *c = static_cast<Connection*>(fd->local);
					if ( fd->tlsWantRead )
						c->readReady();

					if ( fd->tlsWantWrite )
						c->writeReady();
					break;
				}
				case SelectFd::Established: {
					Connection *c = static_cast<Connection*>(fd->local);
					if ( readyMask & READ_READY ) {
						c->readReady();
					}

					if ( readyMask & WRITE_READY && fd->wantWrite )
						c->writeReady();
					break;
				}
			}
		}
	}
}
