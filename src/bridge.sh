#!/bin/bash
#

set -x

brctl addbr inline1
brctl addif inline1 eth1
brctl addif inline1 eth2

ip link set inline1 up
ip link set eth1 up
ip link set eth2 up

for fn in /proc/sys/net/ipv4/conf/*/rp_filter; do echo 0 > $fn; done
for fn in /proc/sys/net/bridge/*; do echo 0 > $fn; done

ebtables -t broute -A BROUTING -p IPv4 -i eth1 --ip-proto tcp --ip-dport 443 -j redirect --redirect-target DROP
ebtables -t broute -A BROUTING -p IPv4 -i eth2 --ip-proto tcp --ip-sport 443 -j redirect --redirect-target DROP

iptables -t mangle -N DIVERT

iptables -t mangle -A PREROUTING -p tcp -m socket -j DIVERT
iptables -t mangle -A DIVERT -j MARK --set-mark 101
iptables -t mangle -A DIVERT -j ACCEPT

ip rule add fwmark 101 lookup 101
ip route add local default dev inline1 table 101

iptables -t mangle -A PREROUTING -p tcp -m tcp --dport 443 -j TPROXY  --on-port 4430 --tproxy-mark 101/101

ifconfig inline1 192.168.0.185 netmask 255.255.255.0


