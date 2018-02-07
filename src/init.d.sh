#!/bin/bash
#

pkglibexecdir=@pkglibexecdir@
sysconfdir=@sysconfdir@

VPN="no"
[ -f $sysconfdir/interfaces.sh ] && source $sysconfdir/interfaces.sh

# Decide if we are doing a vpn-shuttle up/down or plain up/down.
root=shuttle
if [ "x$VPN" = "xyes" ]; then
	root=vpn
fi

_start()
{
	$pkglibexecdir/$root-up
}

_stop()
{
	$pkglibexecdir/$root-down
}

case $1 in
	start)
		_start;
	;;
	stop)
		_stop;
	;;

esac
