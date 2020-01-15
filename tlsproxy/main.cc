#include "main.h"
#include "listen.h"
#include "service.h"
#include "genf.h"
#include <netp/netp.h>

#include <unistd.h>
#include <signal.h>
#include <fstream>

#include <sys/stat.h>
#include <sys/types.h>
#include <netp/module.h>

using std::endl;
using std::ofstream;

void makeCert( EVP_PKEY *capkey, X509 *cacert, BIGNUM *serial,
		EVP_PKEY **ppkey, X509 **px509, const String &cert )
{
	chdir( PKGSTATEDIR );

	const char *nameTypes[] = {
		"C",  "ST", "O",
		"OU", "CN", "emailAddress", 0
	};

	const char *nameValues[] = {
		"CA", "Ontario", "Colm Networks Inc.",
		"Development", cert, "info@colm.net", 0
	};

	/* If no new key was supplied, create it. */
	if ( *ppkey == 0 ) {
		log_ERROR( "need to make genrsa in make-cert" );
		*ppkey = genrsa( 2048 );
	}

	/* Sign. */
	*px509 = sign_cert( capkey, cacert, serial, *ppkey, nameTypes, nameValues, 365 );
}

void makeCa()
{
	log_message( "making CA" );

	String keyFile = "CA/cakey.pem";
	String outFile = "CA/cacert.pem";

	const char *nameTypes[] = {
		"C",  "ST", "O",
		"OU", "CN", "emailAddress", 0
	};

	const char *nameValues[] = {
		"CA", "Ontario", "Colm Networks Inc.",
		"Development", "ca.colm.net", "info@colm.net", 0
	};

	chdir( PKGSTATEDIR );

	/* Private key. */
	genrsa_file( keyFile, 2048 );

	/* Self-signed root cert. */
	make_ca_cert( keyFile, outFile, nameTypes, nameValues, 365 );
}

void initTls()
{
	chdir( PKGSTATEDIR );

	mkdir( "CA",      0777 );
	mkdir( "certs",   0777 );
	mkdir( "private", 0777 );

	ofstream serial( "CA/serial" );
	serial << "01" << endl;
	serial.close();
}

int MainThread::funcMakeCa()
{
	tlsStartup();
	::makeCa();
	tlsShutdown();
	return 0;
}

int MainThread::funcCert()
{
	tlsStartup();
	// ::makeCert( cert.c_str() );
	tlsShutdown();
	return 0;
}

int MainThread::funcInitTls()
{
	tlsStartup();
	::initTls();
	tlsShutdown();
	return 0;
}

void MainThread::handleTimer()
{
	const int minPoolSize = 15;

	/* Keep the key pool well-stocked. */
	int avail = listen->contextMap.keysAvail();
	if ( avail < minPoolSize ) {
		int add = minPoolSize - avail;
		log_debug( DBG_PROXY, "adding " << add << " keys to the pool of available keys" );
		listen->contextMap.addKeys( add );
	}
}

int MainThread::main()
{
	if ( makeCa )
		return funcMakeCa();
	else if ( cert != 0 )
		return funcCert();
	else if ( initTls )
		return funcInitTls();

	log_message( "starting" );

	for ( OptStringEl *opt = moduleArgs.head; opt != 0; opt = opt->next )
		moduleList.loadModule( opt->data );

	moduleList.initModules();

	tlsStartup( PKGSTATEDIR "/rand" );

	service = new ServiceThread;
	listen = new ListenThread( service );

	SendsToListen *sendsToListen = registerSendsToListen( listen );
	SendsToService *sendsToService = registerSendsToService( service );

	create( listen );
	create( service );

	handleTimer();

	struct timeval t;
	t.tv_sec = 1;
	t.tv_usec = 0;

	signalLoop( &t );

	log_message( "sending shutdown messages" );

	sendsToListen->openShutdown();
	sendsToListen->send();

	sendsToService->openShutdown();
	sendsToService->send( true );

	join();

	tlsShutdown();

	log_message( "exiting" );

	return 0;
}
