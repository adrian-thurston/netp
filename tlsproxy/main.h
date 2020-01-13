#ifndef _MAIN_H
#define _MAIN_H

#include "genf.h"
#include "main_gen.h"

#include <aapl/astring.h>

struct MainThread
	: public MainGen
{
	void handleTimer();

	ListenThread *listen;
	ServiceThread *service;

	int funcCert();
	int funcMakeCa();
	int funcInitTls();

	int main();
};

EVP_PKEY *genrsa( int num );
int genrsa_file( char *outfile, int num );
int make_ca_cert( char *keyfile, char *outfile, const char **ne_types,
		const char **ne_values, int days );

int cert_req( char *keyfile, char *outfile, char *subj );
X509 *sign_cert( EVP_PKEY *cakey, X509 *cacert, BIGNUM *serial, EVP_PKEY *ppkey,
		const char **ne_types, const char **ne_values, long days );

void makeCert( EVP_PKEY *capkey, X509 *cacert, BIGNUM *serial, EVP_PKEY **ppkey, X509 **px509, const String &cert );

X509 *loadCert( const char *file );
EVP_PKEY *loadKey( const char *file );
BIGNUM *loadSerial( const char *serialfile );
int saveSerial( char *serialfile, BIGNUM *serial );

#endif
