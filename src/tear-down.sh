#!/bin/bash
#

set -x

[ `whoami` == root ] || exit

/home/thurston/pkgs/fetch/libexec/fetch/init.d stop
/home/thurston/pkgs/netp/libexec/netp/init.d stop
/home/thurston/pkgs/tlsproxy/libexec/tlsproxy/init.d stop
/home/thurston/pkgs/broker/libexec/broker/init.d stop

sleep 1

set -e

source ./interfaces.sh

iptables -t mangle -D PREROUTING -p tcp -m tcp --dport 443 -j TPROXY  --on-port 4430 --tproxy-mark 101/101

ip route del local default dev lo table 101
ip rule del fwmark 101 lookup 101

iptables -t mangle -D PREROUTING -p tcp -m socket -j DIVERT
iptables -t mangle -D DIVERT -j MARK --set-mark 101
iptables -t mangle -D DIVERT -j ACCEPT

iptables -t mangle -X DIVERT

for fn in /proc/sys/net/ipv4/conf/*/rp_filter; do echo 0 > $fn; done

###############

ip link set $OUTSIDE down
ip link set $INSIDE down

echo eth1 >/sys/shuttle/shuttle1/port_del
echo eth2 >/sys/shuttle/shuttle1/port_del

echo shuttle1 >/sys/shuttle/del

rmmod shuttle

# FIXME: remove rings

rmmod kring

