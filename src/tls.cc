
#include <thread.h>

#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <sys/socket.h>

#define PEER_CN_NAME_LEN 256

FdDesc *Thread::prepSslClient( const char *remoteHost, int connFd )
{
	SelectFd *selectFd = new SelectFd( connFd, 0, SelectFd::Connect, 0, 0, remoteHost );

	FdDesc *fdDesc = new FdDesc( FdDesc::Client, selectFd );
	selectFd->local = fdDesc;

	return fdDesc;
}

SSL *Thread::startSslClient( SSL_CTX *clientCtx, const char *remoteHost, int connFd )
{
	makeNonBlocking( connFd );

	BIO *bio = BIO_new_fd( connFd, BIO_NOCLOSE );

	/* Create the SSL object and set it in the secure BIO. */
	SSL *ssl = SSL_new( clientCtx );
	SSL_set_mode( ssl, SSL_MODE_AUTO_RETRY );
	SSL_set_bio( ssl, bio, bio );
	SSL_set_tlsext_host_name( ssl, remoteHost );

	SelectFd *selectFd = new SelectFd( connFd, 0, SelectFd::Connect, ssl, bio, strdup(remoteHost) );

	FdDesc *fdDesc = new FdDesc( FdDesc::Client, selectFd );
	selectFd->local = fdDesc;

	// selectFd->wantRead = true;
	selectFd->wantWrite = true;
	selectFdList.append( selectFd );

	return ssl;
}

void Thread::clientConnect( SelectFd *fd, uint8_t readyMask )
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

FdDesc *Thread::startSslServer( SSL_CTX *defaultCtx, int fd )
{
	BIO *bio = BIO_new_fd( fd, BIO_NOCLOSE );

	/* Create the SSL object an set it in the secure BIO. */
	SSL *ssl = SSL_new( defaultCtx );
	SSL_set_mode( ssl, SSL_MODE_AUTO_RETRY );
	SSL_set_bio( ssl, bio, bio );

	SelectFd *selectFd = new SelectFd( fd, 0, SelectFd::Accept, ssl, bio, 0 );

	FdDesc *fdDesc = new FdDesc( FdDesc::Server, selectFd );
	selectFd->local = fdDesc;

	selectFd->wantRead = true;
	// selectFd->wantWrite = true;

	selectFdList.append( selectFd );

	return fdDesc;
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

void Thread::dataRecv( SelectFd *fd, uint8_t readyMask )
{
	sslReadReady( fd, readyMask );
}

int Thread::write( SelectFd *fd, uint8_t readyMask, char *data, int length )
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
			fd->state = SelectFd::WriteRetry;
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
			fd->state = SelectFd::WriteRetry;
		}
		else {
			/* Wrote it all. Ensure other half is back in the established state
			 * from write retry state (maybe never left -- depending on where
			 * we were called from). */
			fd->state = SelectFd::Established;
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

void Thread::serverAccept( SelectFd *fd, uint8_t readyMask )
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

		notifServerAccept( fd, readyMask );
	}
}

void Thread::_selectFdReady( SelectFd *fd, uint8_t readyMask )
{
//	log_debug( DBG_THREAD, "select fd ready" );

	switch ( fd->state ) {
		case SelectFd::NonSsl:
			selectFdReady( fd, readyMask );
			break;

		case SelectFd::Connect:
			clientConnect( fd, readyMask );
			break;

		case SelectFd::Accept:
			serverAccept( fd, readyMask );
			break;

		case SelectFd::Established:
			dataRecv( fd, readyMask );
			break;

		case SelectFd::WriteRetry:
			writeRetry( fd, readyMask );
			break;

		case SelectFd::Paused:
		case SelectFd::Closed:
			/* This shouldn't come in. We need to disable the flags. */
			fd->wantRead = false;
			fd->wantWrite = false;
			break;
	}
}
