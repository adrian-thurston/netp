#!/bin/bash
#

set -x 

if [ '!' -f @sysconfdir@/updown.conf ]; then
	# Bootstrap the database.
	@PIPELINE_PREFIX@/libexec/pipeline/bootstrap

	#
	# Create default databse.
	#
	@PIPELINE_PREFIX@/libexec/pipeline/init.d start
	@PIPELINE_PREFIX@/bin/createdb thurston start
	@PIPELINE_PREFIX@/libexec/pipeline/init.d stop

	#
	#  Bootstrap services.
	#
	@BROKER_PREFIX@/libexec/broker/bootstrap
	@NETP_PREFIX@/libexec/netp/bootstrap
	@TLSPROXY_PREFIX@/libexec/tlsproxy/bootstrap
	@FETCH_PREFIX@/libexec/fetch/bootstrap

	#
	# Fetch share data.
	#

	TMPDIR=$(mktemp -d /tmp/bootstrap.XXXXXX)
	cd $TMPDIR
	git clone $git/data.git $TMPDIR/data
	mkdir @FETCH_PREFIX@/share/fetch/dialer
	cp -a $TMPDIR/data/dialer/tessdata @FETCH_PREFIX@/share/fetch/dialer
	rm -Rf $TMPDIR

	mkdir @FETCH_PREFIX@/var/fetch/cvpp
	sqlite3 @FETCH_PREFIX@/var/fetch/cvpp/cvpp.db < @FETCH_PREFIX@/share/fetch/cvpp.sql

	#
	# Updown config file.
	#
	FORWARD=$(route | grep ^default | awk '{ print $8; }')

	cat > @sysconfdir@/updown.conf <<-EOF
		OUTSIDE_FORWARD="$FORWARD"
		LIVE_PROTNET="prot"
		#LIVE_INTERFACES="em1"
		#LIVE_VPN=yes
	EOF

	echo $FORWARD
fi


