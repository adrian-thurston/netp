#!/bin/bash
#

set -xe

insmod $HOME/devel/kring/src/kring.ko

echo r0 4 4 > /sys/kring/add
echo r1 4 4 > /sys/kring/add

insmod ./shuttle.ko

echo shuttle1 r0 >/sys/shuttle/add

echo eth1 outside  >/sys/shuttle/shuttle1/port_add
echo eth2 inside  >/sys/shuttle/shuttle1/port_add

ip link set eth1 up
ip link set eth2 up

#
# Doesn't seem to matter if we assign an IP address or bring up device, so long
# as we don't go changing interface configuration. Altering the config seems to
# clear the ip route rules below, which is the real reason fiddling with these
# configs caused the system to be so fragile in the beginning.
#
# ifconfig shuttle1 0.0.0.0 up
#

for fn in /proc/sys/net/ipv4/conf/*/rp_filter; do echo 0 > $fn; done

iptables -t mangle -N DIVERT

iptables -t mangle -A PREROUTING -p tcp -m socket -j DIVERT
iptables -t mangle -A DIVERT -j MARK --set-mark 101
iptables -t mangle -A DIVERT -j ACCEPT

ip rule add fwmark 101 lookup 101
ip route add local default dev lo table 101

iptables -t mangle -A PREROUTING -p tcp -m tcp --dport 443 -j TPROXY  --on-port 4430 --tproxy-mark 101/101
