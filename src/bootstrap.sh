#!/bin/bash
#

# This tag indicates this file comes from genf package.
# installed-by-genf

package=@PACKAGE@
pkgdatadir=@pkgdatadir@

CN=$package
SUBJ="/C=CA/ST=Ontario/O=Colm Networks Inc./OU=Development/CN=$CN/emailAddress=info@colm.net/"

if [ '!' -f $pkgdatadir/key.pem ]; then
	openssl req \
		-newkey rsa:2048 \
		-nodes -keyout $pkgdatadir/key.pem \
		-x509 -days 730 -out $pkgdatadir/cert.pem \
		-subj "$SUBJ"
	
	cp $pkgdatadir/cert.pem $pkgdatadir/verify.pem
fi
