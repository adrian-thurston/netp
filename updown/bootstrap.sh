#!/bin/bash
#

set -x 

package=@PACKAGE@
pkgdatadir=@pkgdatadir@
pkglibexecdir=@pkglibexecdir@
pkgstatedir=@pkgstatedir@

if [ '!' -f $pkgdatadir/key.pem ]; then
	CN=$package
	SUBJ="/C=CA/ST=Ontario/O=Colm Networks Inc./OU=Development/CN=$CN/emailAddress=info@colm.net/"

	umask 0077

	openssl req \
		-newkey rsa:2048 \
		-nodes -keyout $pkgdatadir/key.pem \
		-x509 -days 730 -out $pkgdatadir/cert.pem \
		-subj "$SUBJ"
	
	cp $pkgdatadir/cert.pem $pkgdatadir/verify.pem
fi

if [ '!' -f $pkgstatedir/CA/cakey.pem ]; then
	$pkglibexecdir/ca
fi

if [ '!' -f @sysconfdir@/updown.conf ]; then
	#
	# Updown config file.
	#
	FORWARD=$(route | grep ^default | awk '{ print $8; }')

	cat > @sysconfdir@/updown.conf <<-EOF
		# How to reach the outside world.
		OUTSIDE_FORWARD="$FORWARD"

		#
		# Can use any combination of protnet(s), interface(s) and vpn.
		#

		# Create a protected network namespace and analyze traffic
		# that leaves from that namespace to the outside.
		LIVE_PROTNET="prot"

		# Analyze traffic between an interface and the outside world.
		# LIVE_INTERFACES="em1"

		# Create a VPN and analyze traffic that passes through.
		# LIVE_VPN=yes
	EOF

fi

