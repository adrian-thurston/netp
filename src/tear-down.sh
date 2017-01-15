#!/bin/bash
#

set -x 

iptables -t mangle -D PREROUTING -p tcp -m tcp --dport 443 -j TPROXY  --on-port 4430 --tproxy-mark 101/101

ip route del local default dev filter1 table 101
ip rule del fwmark 101 lookup 101

iptables -t mangle -D PREROUTING -p tcp -m socket -j DIVERT
iptables -t mangle -D DIVERT -j MARK --set-mark 101
iptables -t mangle -D DIVERT -j ACCEPT

iptables -t mangle -X DIVERT

for fn in /proc/sys/net/ipv4/conf/*/rp_filter; do echo 0 > $fn; done

###############

ip link set eth2 down
ip link set eth1 down

echo eth1 >/sys/filter/filter1/port_del
echo eth2 >/sys/filter/filter1/port_del

echo filter1 >/sys/filter/del

rmmod filter

