#!/bin/bash
#

pkglibexecdir=@pkglibexecdir@
sysconfdir=@sysconfdir@

VPN="no"
LIVE="no"
[ -f $sysconfdir/interfaces.sh ] && source $sysconfdir/interfaces.sh

# Decide if we are doing a vpn-shuttle up/down or plain up/down.
root=shuttle
if [ "x$VPN" = "xyes" ]; then
	root=vpn
elif [ "x$LIVE" = "xyes" ]; then
	root=live
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
