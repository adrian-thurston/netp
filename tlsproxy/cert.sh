#!/bin/bash

set -x

CN=$1
PASS=tlsproxy

cd @pkgstatedir@ || exit 1

# Add -des3 to put a password on the key.
openssl genrsa \
	-out private/$CN.key \
	-passout pass:$PASS \
	2048

openssl req -new -key private/$CN.key -out csr/$CN.csr \
	-passin pass:$PASS \
	-subj "/C=CA/ST=Ontario/O=Colm Networks Inc./OU=Development/CN=$CN/emailAddress=info@colm.net/"

openssl ca -batch \
	-passin pass:$PASS \
	-in csr/$CN.csr \
	-out certs/$CN.pem \
	-config @pkgdatadir@/openssl.conf
