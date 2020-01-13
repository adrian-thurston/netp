#!/bin/bash

set -x

CN=$1
PASS=tlsproxy

cd @pkgstatedir@ || exit 1

mkdir -p CA
mkdir -p certs
mkdir -p csr
mkdir -p private

echo 01 > CA/serial

echo -n > CA/index.txt
echo -n > CA/index.txt.attr

C="C=CA"
ST="ST=Ontario"
O="O=Colm Networks Inc."
OU="OU=Development"
email="emailAddress=info@colm.net"
SUBJ="/$C/$ST/$O/$OU/CN=$CN/$email/"

echo $SUBJ

# Making the CA key and cert. The -nodes option specifies no des encryption
# (passphrase). To specify a password we can use -passout pass:$PASS
openssl req -new -x509 -extensions v3_ca \
	-keyout CA/cakey.pem -out CA/cacert.pem \
	-days 3650 -nodes -subj "$SUBJ"
