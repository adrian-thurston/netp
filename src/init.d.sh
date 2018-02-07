#!/bin/bash
#

pkglibexecdir=@pkglibexecdir@

_start()
{
	$pkglibexecdir/vpn-up
}

_stop()
{
	$pkglibexecdir/vpn-down
}

case $1 in
	start)
		_start;
	;;
	stop)
		_stop;
	;;

esac
