#include "main.h"
#include "listen.h"
#include "service.h"
#include "genf.h"

#include <unistd.h>
#include <signal.h>
#include <fstream>

#include <openssl/opensslconf.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/conf.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/bn.h>
#include <openssl/txt_db.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/objects.h>
#include <openssl/ocsp.h>
#include <openssl/pem.h>
#include <sys/file.h>

#include <sys/stat.h>
#include <sys/types.h>

#define SERIAL_RAND_BITS 64

using std::ofstream;
using std::endl;

String randfile = PKGSTATEDIR "/rand";
String serialfile = PKGSTATEDIR "/CA/serial";

static int genrsaCallback( int p, int n, BN_GENCB *gencb )
{
	return 1;
}

static int passwordCallback( char *buf, int bufsiz, int verify, void * )
{
	return 0;
}

X509 *loadCert( const char *file )
{
	BIO *cert = BIO_new( BIO_s_file() );
	BIO_read_filename( cert, file );
	X509 *x509 = PEM_read_bio_X509_AUX( cert, 0, (pem_password_cb *)passwordCallback, 0 );
	BIO_free( cert );
	return x509;
}

EVP_PKEY *loadKey( const char *file )
{
	BIO *key = BIO_new( BIO_s_file() );
	BIO_read_filename( key, file );
	EVP_PKEY *pkey = PEM_read_bio_PrivateKey( key, 0, (pem_password_cb *)passwordCallback, 0 );
	BIO_free( key );
	return pkey;
}

static X509_NAME *consName( const char **nameTypes, const char **nameValues )
{
	int i, nid;

	X509_NAME *name = X509_NAME_new();
	for (i = 0; nameTypes[i] != 0; i++) {
		nid = OBJ_txt2nid( nameTypes[i] );
		X509_NAME_add_entry_by_NID( name, nid, MBSTRING_ASC, (unsigned char *)nameValues[i], -1, -1, 0 );
	}

	return name;
}

static int sign( X509 *x509, EVP_PKEY *pkey, const EVP_MD *md )
{
	EVP_MD_CTX mctx;
	EVP_PKEY_CTX *pkctx = 0;

	EVP_MD_CTX_init( &mctx );
	EVP_DigestSignInit( &mctx, &pkctx, md, 0, pkey );
	X509_sign_ctx( x509, &mctx );
	EVP_MD_CTX_cleanup( &mctx );

	return 1;
}

static int addExtension( X509 *cert, int nid, char *value )
{
	X509V3_CTX ctx;

	X509V3_set_ctx_nodb( &ctx );
	X509V3_set_ctx( &ctx, cert, cert, 0, 0, 0 );
	X509_EXTENSION *ex = X509V3_EXT_conf_nid( 0, &ctx, nid, value );
	int result = X509_add_ext( cert, ex, -1 );
	X509_EXTENSION_free( ex );

	return result;
}

static int randSerial( ASN1_INTEGER *ai )
{
	BIGNUM *btmp = BN_new();
	BN_pseudo_rand( btmp, SERIAL_RAND_BITS, 0, 0 );
	BN_to_ASN1_INTEGER( btmp, ai );
	BN_free(btmp);
	return 0;
}

BIGNUM *loadSerial( const char *serialfile )
{
	char buf[1024];

	ASN1_INTEGER *ai = ASN1_INTEGER_new();
	BIO *in = BIO_new( BIO_s_file() );

	int res = BIO_read_filename( in, serialfile );
	if ( ! res ) {
		log_ERROR( "load-serial: failed to set filename to " << serialfile );
		BIO_free_all( in );
		return 0;
	}

	a2i_ASN1_INTEGER( in, ai, buf, 1024 );
	BIGNUM *ret = ASN1_INTEGER_to_BN( ai, 0 );
	BIO_free( in );
	ASN1_INTEGER_free( ai );

	return ret;
}

int saveSerial( char *serialfile, BIGNUM *serial )
{
	BIO *out = BIO_new( BIO_s_file() );

	int res = BIO_write_filename( out, serialfile );
	if ( ! res ) {
		log_ERROR( "save-serial: failed to set filename to " << serialfile );
		BIO_free_all( out );
		return -1;
	}

	ASN1_INTEGER *ai = BN_to_ASN1_INTEGER( serial, 0 );
	i2a_ASN1_INTEGER( out, ai );
	BIO_puts( out, "\n" );
	BIO_free_all( out );
	ASN1_INTEGER_free( ai );

	return 0;
}

static void writeCert( BIO *bp, X509 *x509 )
{
	X509_print( bp, x509 );
	PEM_write_bio_X509( bp, x509 );
}

RSA *genrsa_rsa( int num )
{
	BN_GENCB gencb;
	BIGNUM *bn = BN_new();

	BN_GENCB_set( &gencb, genrsaCallback, 0 );

	RSA *rsa = RSA_new();
	BN_set_word( bn, RSA_F4 );
	RSA_generate_key_ex( rsa, num, bn, &gencb );

	BN_free( bn );

	return rsa;
}

EVP_PKEY *genrsa( int num )
{
	RSA *rsa = genrsa_rsa( num );
	EVP_PKEY *pkey = EVP_PKEY_new();
	EVP_PKEY_set1_RSA( pkey, rsa );
	return pkey;
}

int genrsa_file( char *outfile, int num )
{
	RSA *rsa = genrsa_rsa( num );

	BIO *out = BIO_new( BIO_s_file() );
	BIO_write_filename( out, outfile );
	PEM_write_bio_RSAPrivateKey( out, rsa, 0, 0, 0, (pem_password_cb *)passwordCallback, 0 );

	RSA_free( rsa );
	BIO_free_all( out );

	return 0;
}

int make_ca_cert( char *keyfile, char *outfile, const char **nameTypes, const char **nameValues, int days )
{
	int bcNid = OBJ_sn2nid( "basicConstraints" );
	String bc = "CA:TRUE";
	String mask = "utf8only";

	ASN1_STRING_set_default_mask_asc( mask );

	BIO *in = BIO_new( BIO_s_file() );

	EVP_PKEY *pkey = loadKey( keyfile );

	X509_NAME *name = consName( nameTypes, nameValues );

	X509 *x509 = X509_new();

	X509_set_version( x509, 2 );

	randSerial( X509_get_serialNumber( x509 ) );

	X509_gmtime_adj( X509_get_notBefore( x509 ), 0 );
	X509_time_adj_ex( X509_get_notAfter( x509 ), days, 0, 0 );

	X509_set_issuer_name( x509, name );
	X509_set_subject_name( x509, name );

	X509_set_pubkey( x509, pkey );

	addExtension( x509, bcNid, bc );

	sign( x509, pkey, 0 );

	BIO *out = BIO_new( BIO_s_file() );
	BIO_write_filename( out, outfile );
	writeCert( out, x509 );

	BIO_free( in );
	BIO_free_all( out );
	EVP_PKEY_free( pkey );
	X509_NAME_free( name );
	X509_free( x509 );

	return 0;
}

X509 *sign_cert( EVP_PKEY *capkey, X509 *cacert, BIGNUM *serial, EVP_PKEY *pkey,
		const char **nameTypes, const char **nameValues, long days )
{
	int def_nid;
	EVP_PKEY_get_default_digest_nid( capkey, &def_nid );
	char *md = (char *)OBJ_nid2sn( def_nid );

	const EVP_MD *dgst = EVP_get_digestbyname( md );

	X509_NAME *subject = consName( nameTypes, nameValues );

	X509 *x509 = X509_new();

	BN_to_ASN1_INTEGER( serial, x509->cert_info->serialNumber );

	X509_time_adj_ex( X509_get_notBefore( x509 ), -days, 0, 0 );
	X509_time_adj_ex( X509_get_notAfter( x509 ), days, 0, 0 );
	X509_set_issuer_name( x509, X509_get_subject_name( cacert ) );
	X509_set_subject_name( x509, subject );
	X509_set_pubkey( x509, pkey );

	sign( x509, capkey, dgst );

	return x509;
}
