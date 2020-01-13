#!/bin/bash
#

sudo iptables -t nat -A PREROUTING -s 192.168.0.0/24 -d $1 -p tcp --dport 443 -j DNAT --to-destination 192.168.0.238
