
#include <thread.h>

#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>


#define PEER_CN_NAME_LEN 256

#define CA_CERT_FILE "/etc/ssl/certs/ca-certificates.crt"
#define CERT_FILE "/etc/ssl/certs/ssl-cert-snakeoil.pem"
#define KEY_FILE "/etc/ssl/private/ssl-cert-snakeoil.key"

/* Do this once at startup. */
void Thread::sslInit()
{
	/* Global initialization. */
	SSL_load_error_strings();
	ERR_load_BIO_strings();
	OpenSSL_add_all_algorithms();
	SSL_library_init();
}
    
SSL_CTX *Thread::sslClientCtx()
{
	/* Create the SSL_CTX. */
	SSL_CTX *ctx = SSL_CTX_new(TLSv1_client_method());
	if ( ctx == NULL )
		log_FATAL( EC_SSL_NEW_CONTEXT_FAILURE << " SSL error: new context failure" );

	/* Load the CA certificates that we will use to verify. */
	int result = SSL_CTX_load_verify_locations( ctx, CA_CERT_FILE, NULL );
	if ( !result ) {
		log_ERROR( EC_SSL_CA_CERT_LOAD_FAILURE << " failed to load CA cert file " << CA_CERT_FILE );
	}

	return ctx;
}

SelectFd *Thread::startSslClient( SSL_CTX *clientCtx, const char *remoteHost, int connFd )
{
	makeNonBlocking( connFd );

	BIO *bio = BIO_new_fd( connFd, BIO_NOCLOSE );

	/* Create the SSL object and set it in the secure BIO. */
	SSL *ssl = SSL_new( clientCtx );
	SSL_set_mode( ssl, SSL_MODE_AUTO_RETRY );
	SSL_set_bio( ssl, bio, bio );
	SSL_set_tlsext_host_name( ssl, remoteHost );

	SelectFd *selectFd = new SelectFd( connFd, 0, SelectFd::TlsConnect, ssl, bio, strdup(remoteHost) );

	selectFd->wantRead = false;
	selectFd->wantWrite = true;
	selectFdList.append( selectFd );

	return selectFd;
}

SelectFd *Thread::startSslServer( SSL_CTX *defaultCtx, int fd )
{
	BIO *bio = BIO_new_fd( fd, BIO_NOCLOSE );

	/* Create the SSL object an set it in the secure BIO. */
	SSL *ssl = SSL_new( defaultCtx );
	SSL_set_mode( ssl, SSL_MODE_AUTO_RETRY );
	SSL_set_bio( ssl, bio, bio );

	SelectFd *selectFd = new SelectFd( fd, 0, SelectFd::TlsAccept, ssl, bio, 0 );

	selectFd->wantRead = true;
	selectFd->wantWrite = false;

	selectFdList.append( selectFd );

	return selectFd;
}

void Thread::clientConnect( SelectFd *fd )
{
	int result = SSL_connect( fd->ssl );
	if ( result <= 0 ) {
		/* No connect yet. May need more data. */
		result = SSL_get_error( fd->ssl, result );

		fd->wantRead = false;
		fd->wantWrite = false;
		if ( result == SSL_ERROR_WANT_READ )
			fd->wantRead = true;
		else if ( result == SSL_ERROR_WANT_WRITE )
			fd->wantWrite = true;
		else {
			sslError( result );
		}
	}
	else {
		/* Check the verification result. */
		long verifyResult = SSL_get_verify_result( fd->ssl );
		if ( verifyResult != X509_V_OK ) {
			log_ERROR( "ssl peer failed verify: " << fd->remoteHost );
		}

		/* Check the cert chain. The chain length is automatically checked by
		 * OpenSSL when we set the verify depth in the CTX */

		/* Check the common name. */
		X509 *peer = SSL_get_peer_certificate( fd->ssl );
		char peer_CN[PEER_CN_NAME_LEN];
		X509_NAME_get_text_by_NID( X509_get_subject_name(peer),
				NID_commonName, peer_CN, PEER_CN_NAME_LEN );

		int cr = X509_check_host( peer, fd->remoteHost, strlen(fd->remoteHost), 0, 0 );
		if ( cr != 1 ) {
			log_ERROR( "ssl peer cn host mismatch: requested " <<
					fd->remoteHost << " but cert is for " << peer_CN );
		}

		/* Create a BIO for the ssl wrapper. */
		BIO *bio = BIO_new( BIO_f_ssl() );
		BIO_set_ssl( bio, fd->ssl, BIO_NOCLOSE );

		/* Just wrapped the bio, update in select fd. */
		fd->bio = bio;

		sslConnectSuccess( fd, fd->ssl, bio );
	}
}

int Thread::read( SelectFd *fd, void *buf, int len )
{
	int nbytes = BIO_read( fd->bio, buf, len );

	if ( nbytes <= 0 && BIO_should_retry( fd->bio ) ) {
		// log_debug( DBG_THREAD, "bio should retry" );
		fd->wantRead = true; // BIO_should_read(fd->bio);
		fd->wantWrite = BIO_should_write(fd->bio);

		return 0;
	}
	else if ( nbytes <= 0 ) {
		return -1;
	}

	return nbytes;
}

int Thread::write( SelectFd *fd, char *data, int length )
{
	int written = BIO_write( fd->bio, data, length );

	if ( written <= 0 ) {
		/* If the BIO is saying it we should retry later, go back into select.
		 * */
		if ( BIO_should_retry( fd->bio ) ) {
			/* Write failure is retry-related. */
			fd->wantRead = BIO_should_read(fd->bio);
			fd->wantWrite = BIO_should_write(fd->bio);
			fd->abortRound = true;
			fd->state = SelectFd::TlsWriteRetry;
		}
		else {
			/* Write failed for some non-retry reason. */
			log_ERROR( "SSL write failed for non-retry reason" );
		}
	}
	else {
		/* Wrote something. Write it all? */
		if ( written < length ) {
			fd->wantRead = false;
			fd->wantWrite = true;
			fd->abortRound = true;
			fd->state = SelectFd::TlsWriteRetry;
		}
		else {
			/* Wrote it all. Ensure other half is back in the established state
			 * from write retry state (maybe never left -- depending on where
			 * we were called from). */
			fd->state = SelectFd::TlsEstablished;
			fd->wantRead = true;
			fd->wantWrite = false;
		}
	}

	return written;
}

void Thread::sslError( int e )
{
	switch ( e ) {
		case SSL_ERROR_NONE:
			log_ERROR("ssl error: SSL_ERROR_NONE\n");
			break;
		case SSL_ERROR_ZERO_RETURN:
			log_ERROR("ssl error: SSL_ERROR_ZERO_RETURN\n");
			break;
		case SSL_ERROR_WANT_READ:
			log_ERROR("ssl error: SSL_ERROR_WANT_READ\n");
			break;
		case SSL_ERROR_WANT_WRITE:
			log_ERROR("ssl error: SSL_ERROR_WANT_WRITE\n");
			break;
		case SSL_ERROR_WANT_CONNECT:
			log_ERROR("ssl error: SSL_ERROR_WANT_CONNECT\n");
			break;
		case SSL_ERROR_WANT_ACCEPT:
			log_ERROR("ssl error: SSL_ERROR_WANT_ACCEPT\n");
			break;
		case SSL_ERROR_WANT_X509_LOOKUP:
			log_ERROR("ssl error: SSL_ERROR_WANT_X509_LOOKUP\n");
			break;
		case SSL_ERROR_SYSCALL:
			log_ERROR("ssl error: SSL_ERROR_SYSCALL\n");
			break;
		case SSL_ERROR_SSL:
			log_ERROR("ssl error: SSL_ERROR_SSL\n");
			break;
	}
}

void Thread::prepNextRound( SelectFd *fd, int result )
{
	fd->wantRead = false;
	fd->wantWrite = false;
	if ( result == SSL_ERROR_WANT_READ )
		fd->wantRead = true;
	else if ( result == SSL_ERROR_WANT_WRITE )
		fd->wantWrite = true;
	else
		sslError( result );
}

void Thread::serverAccept( SelectFd *fd )
{
	int result = SSL_accept( fd->ssl );
	if ( result <= 0 ) {
		/* No accept yet, may need more data. */
		result = SSL_get_error( fd->ssl, result );

		prepNextRound( fd, result );
	}
	else {
		// log_debug( DBG_THREAD, "accept succeeded" );

		/* Success. Stop the select loop. Create a BIO for the ssl wrapper. */
		BIO *bio = BIO_new(BIO_f_ssl());
		BIO_set_ssl( bio, fd->ssl, BIO_NOCLOSE );
		fd->bio = bio;

		bool nb = makeNonBlocking( fd->fd );
		if ( !nb )
			log_ERROR( "non-blocking IO not available" );

		fd->wantRead = fd->wantWrite = false;

		notifServerAccept( fd );
	}
}

void Thread::_selectFdReady( SelectFd *fd, uint8_t readyMask )
{
	switch ( fd->state ) {
		case SelectFd::User:
			selectFdReady( fd, readyMask );
			break;

		case SelectFd::PktListen: {
			sockaddr_in peer;
			socklen_t len = sizeof(sockaddr_in);

			int result = ::accept( fd->fd, (sockaddr*)&peer, &len );
			if ( result >= 0 ) {
				SelectFd *selectFd = new SelectFd( result, 0 );
				selectFd->state = SelectFd::PktData;
				selectFd->wantRead = true;
				selectFdList.append( selectFd );
			}
			else {
				log_ERROR( "failed to accept connection: " << strerror(errno) );
			}
			break;
		}
		case SelectFd::PktData: {
			data( fd );
			break;
		}

		case SelectFd::TlsConnect:
			clientConnect( fd );
			break;

		case SelectFd::TlsAccept:
			serverAccept( fd );
			break;

		case SelectFd::TlsEstablished:
			sslReadReady( fd );
			break;

		case SelectFd::TlsWriteRetry:
			writeRetry( fd );
			break;

		case SelectFd::TlsPaused:
		case SelectFd::Closed:
			/* This shouldn't come in. We need to disable the flags. */
			fd->wantRead = false;
			fd->wantWrite = false;
			break;
	}
}
