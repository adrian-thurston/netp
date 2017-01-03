#!/bin/bash
#

set -x

for fn in /proc/sys/net/ipv4/conf/*/rp_filter; do echo 0 > $fn; done

iptables -t mangle -N DIVERT

iptables -t mangle -A PREROUTING -p tcp -m socket -j DIVERT
iptables -t mangle -A DIVERT -j MARK --set-mark 101
iptables -t mangle -A DIVERT -j ACCEPT

ip rule add fwmark 101 lookup 101
ip route add local default dev filter1 table 101

iptables -t mangle -A PREROUTING -p tcp -m tcp --dport 443 -j TPROXY  --on-port 4430 --tproxy-mark 101/101
