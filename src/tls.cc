
#include <valgrind/memcheck.h>
#include <thread.h>
#include <strings.h>

#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <aapl/astring.h>
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

SSL_CTX *Thread::sslCtxClientPublic()
{
	return sslCtxClient( CA_CERT_FILE );
}

SSL_CTX *Thread::sslCtxClientInternal()
{
	String verify = String(pkgDataDir()) + "/verify.pem";
	String key    = String(pkgDataDir()) + "/key.pem";
	String cert   = String(pkgDataDir()) + "/cert.pem";
	return sslCtxClient( verify, key, cert );
}

SSL_CTX *Thread::sslCtxClient( const char *verify, const char *key, const char *cert )
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

	log_debug( DBG_TLS, "client verify mode: " << SSL_CTX_get_verify_mode( ctx ) );

	SSL_CTX_set_mode( ctx, SSL_MODE_ENABLE_PARTIAL_WRITE );

	return ctx;
}

SSL_CTX *Thread::sslCtxServerInternal()
{
	String key    = String(pkgDataDir()) + "/key.pem";
	String cert   = String(pkgDataDir()) + "/cert.pem";
	String verify = String(pkgDataDir()) + "/verify.pem";

	return sslCtxServer( key, cert, verify );
}

SSL_CTX *Thread::sslCtxServer( const char *key, const char *cert, const char *verify )
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

	log_debug( DBG_TLS, "server verify mode: " << SSL_CTX_get_verify_mode( ctx ) );

	SSL_CTX_set_mode( ctx, SSL_MODE_ENABLE_PARTIAL_WRITE );

	return ctx;
}

SSL_CTX *Thread::sslCtxServer( EVP_PKEY *pkey, X509 *x509 )
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

	SSL_CTX_set_mode( ctx, SSL_MODE_ENABLE_PARTIAL_WRITE );

	return ctx;
}

void Thread::startTlsClient( SSL_CTX *clientCtx, SelectFd *selectFd, const char *remoteHost )
{
	log_debug( DBG_CONNECTION, "starting TLS client" );

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

	if ( selectFd->remoteHost == 0 && remoteHost != 0 )
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

/* Test name against pattern, allowing for a *. wildcard at the head of the
 * pattern, matching any subdomain. */
int Thread::checkName( const char *name, ASN1_STRING *pattern )
{
	const char *subj = name;
	size_t slen = strlen( name );

	const char *pat = (const char*)ASN1_STRING_data( pattern );
	size_t plen = ASN1_STRING_length( pattern );

	/* If pattern starts with *. then take off the most specific component. */
	if ( plen > 2 && pat[0] == '*' && pat[1] == '.' ) {
		plen -= 1;
		pat += 1;

		/* Take one level off the name. */
		subj = strchr( name, '.');
		if ( subj == NULL )
			return false;
		slen -= ( subj - name );
	}

	if ( slen == plen && strncasecmp( subj, pat, slen ) == 0 )
		return true;

	return false;
}

/* RFC6125, RFC2818 */
bool Thread::hostMatch( X509 *cert, const char *name )
{
	/* Check subjectAltName extension first. */
	STACK_OF(GENERAL_NAME) *altnames = (STACK_OF(GENERAL_NAME)*) X509_get_ext_d2i( 
			cert, NID_subject_alt_name, NULL, NULL );

	if ( altnames ) {
		int n = sk_GENERAL_NAME_num( altnames );

		for ( int i = 0; i < n; i++ ) {
			GENERAL_NAME *altname = sk_GENERAL_NAME_value( altnames, i );
			if ( altname->type != GEN_DNS )
				continue;

			ASN1_STRING *pat = altname->d.dNSName;

			if ( checkName( name, pat ) ) {
				GENERAL_NAMES_free( altnames );
				return true;
			}
		}

		GENERAL_NAMES_free(altnames);
		return false;
	}
 
	/* Check common name from subject. */
	X509_NAME *sname = X509_get_subject_name( cert );
	if ( sname == NULL )
		return false; 
 
	int i = -1;
	while ( true ) {
		i = X509_NAME_get_index_by_NID( sname, NID_commonName, i );
		if ( i < 0 )
			break;

		X509_NAME_ENTRY *entry = X509_NAME_get_entry( sname, i );
		ASN1_STRING *pat = X509_NAME_ENTRY_get_data( entry );
		if ( checkName( name, pat ) )
			return true;
	}
 
	return false;
}
 
bool Thread::hostMatch( SelectFd *selectFd, const char *name )
{
	/* Check the common name. */
	X509 *peer = SSL_get_peer_certificate( selectFd->ssl );
	if ( peer == 0 )
		return false;

#ifdef HAVE_X509_CHECK_HOST
	bool result =  X509_check_host( peer, name, strlen( name ), 0, 0 ) == 1;
#else
	bool result = hostMatch( peer, name );
#endif

	X509_free( peer );
	return result;
}

int Thread::tlsRead( SelectFd *fd, void *buf, int len )
{
	fd->tlsReadWantsWrite = false;

	int nbytes = SSL_read( fd->ssl, buf, len );

	if ( nbytes > 0 ) {
		/* FIXME: should this be controlled by a debug build option */
		VALGRIND_MAKE_MEM_DEFINED( buf, nbytes );
	}
	else {
		int err = SSL_get_error( fd->ssl, nbytes );
		if ( err == SSL_ERROR_WANT_WRITE ) {
			fd->tlsReadWantsWrite = true;
			nbytes = 0;
		}
		else if ( err == SSL_ERROR_WANT_READ )
			nbytes = 0;
		else {
			/* Indicate error. */
			nbytes = -1;
		}
	}

	return nbytes;
}

int Thread::tlsWrite( SelectFd *fd, char *data, int length )
{
	if ( length <= 0 )
		return 0;

	fd->tlsWriteWantsRead = false;

	int nbytes = SSL_write( fd->ssl, data, length );

	if ( nbytes <= 0 ) {
		/* If the BIO is saying it we should retry later, go back into select.
		 * */
		int err = SSL_get_error( fd->ssl, nbytes );
		if ( err == SSL_ERROR_WANT_WRITE )
			nbytes = 0;
		else if ( err == SSL_ERROR_WANT_READ ) {
			fd->tlsWriteWantsRead = true;
			nbytes = 0;
		}
		else {
			/* Indicate error. */
			nbytes = -1;
		}
	}

	return nbytes;
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

void Thread::connTlsConnectReady( SelectFd *fd )
{
	int result = SSL_connect( fd->ssl );
	if ( result <= 0 ) {
		/* No connect yet. May need more data. */
		result = SSL_get_error( fd->ssl, result );

		bool retry = prepNextRound( fd, result );
		if ( !retry ) {
			/* Not a retry failure. */
			log_ERROR( "SSL_connect failure" );
		}
	}
	else {
		Connection *c = static_cast<Connection*>(fd->local);
		log_debug( DBG_CONNECTION, "successful SSL_connect" );

		/* Check the verification result. */
		long verifyResult = SSL_get_verify_result( fd->ssl );
		if ( verifyResult != X509_V_OK ) {
			fd->sslVerifyError = true;

			log_ERROR( "ssl peer failed verify: " << fd );
			c->failure( Connection::SslPeerFailedVerify );
			c->close();
		}
		else {
			if ( c->checkHost && !hostMatch( fd, fd->remoteHost ) ) {
				fd->sslVerifyError = true;

				log_ERROR( "unable to match peer host to: " << fd->remoteHost );

				c->failure( Connection::SslPeerCnHostMismatch );
				c->close();
			}
			else {
				/* Would like to require or implement this. Not available on 14.04. */
				/* #error no X509_check_host */

				fd->state = SelectFd::TlsEstablished;
				fd->tlsEstablished = true;
				fd->tlsWantRead = true;

				Connection *c = static_cast<Connection*>(fd->local);
				c->connectComplete();
			}
		}
	}
}

void Thread::connTlsAcceptReady( SelectFd *fd )
{
	log_debug( DBG_CONNECTION, "tls-accept socket is ready" );

	Connection *c = static_cast<Connection*>(fd->local);

	bool nb = makeNonBlocking( fd->fd );
	if ( !nb )
		log_ERROR( "TLS accept: non-blocking IO not available" );

	int result = SSL_accept( fd->ssl );
	if ( result <= 0 ) {
		/* No accept yet, may need more data. */
		result = SSL_get_error( fd->ssl, result );

		bool retry = prepNextRound( fd, result );

		log_debug( DBG_CONNECTION, "tls-accept did not succeed, retry: " <<
				retry << " error: " << result );
		tlsError( DBG_CONNECTION, result );

		if ( !retry ) {
			/* Notify of connection error. */
			c->failure( Connection::SslAcceptError );
		}
	}
	else {
		log_debug( DBG_CONNECTION, "tls-accept successful" );

		long verifyResult = SSL_get_verify_result( fd->ssl );

		log_debug( DBG_TLS, "SSL_accept: verify result: " <<
				( verifyResult == X509_V_OK ? "OK" : "FAILED" ) );

		//fd->wantRead = fd->wantWrite = false;

		/* Go into established state and start reading. Will buffer until other
		 * half is ready. */
		fd->state = SelectFd::TlsEstablished;
		fd->tlsEstablished = true;

		c->notifyAccept();
	}
}


