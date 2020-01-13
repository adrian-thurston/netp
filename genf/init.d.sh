#!/bin/bash
#

# This tag indicates this file comes from genf package.
# installed-by-genf

package=@PACKAGE@
binary=@bindir@/@PACKAGE@
pidfile=@piddir@/@PACKAGE@.pid
sysconfdir=@sysconfdir@

VPN="no"
OPTIONS=""
[ -f $sysconfdir/interfaces.sh ] && source $sysconfdir/interfaces.sh

# Decide if we are to execute inside a network namespace.
nsexec=""
if [ "x$VPN" = "xyes" ]; then
        nsexec="ip netns exec inline"
fi

_start()
{
	if [ -f $pidfile ]; then
		echo $package init.d start: pidfile exists already >&2
		exit 1
	fi

	# Enable core dumps up to 500 MB. One-time setup is done in csconf
	ulimit -c 512000

	# check for pidfile first
	echo starting @bindir@/@PACKAGE@
	$nsexec @bindir@/@PACKAGE@ -b $OPTIONS

	# Wait two seconds for the pidfile to appear.
	iters=0
	while [ '!' -f $pidfile ]; do
		sleep 0.1
		if [ $iters == 20 ]; then
			echo $package init.d start: pidfile was not created >&2
			exit 1
		fi
		iters=$((iters + 1))
	done
}

_stop()
{
	if [ '!' -f $pidfile ]; then
		echo $package init.d stop: pidfile does not exist >&2
		exit 1
	fi

	kill `cat $pidfile`

	# Wait two seconds for the pidfile to disappear.
	iters=0
	while [ -f $pidfile ]; do
		sleep 0.1
		if [ $iters == 20 ]; then
			echo $package init.d start: pidfile did not disappear >&2
			exit 1
		fi
		iters=$((iters + 1))
	done
}


case $1 in
	start)
		_start;
	;;
	stop)
		_stop;
	;;

esac
