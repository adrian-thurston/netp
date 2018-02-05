#!/bin/bash
#

set -x

openvpn --config server-openvpn.conf

sleep 2

iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
sysctl net.ipv4.ip_forward=1

brctl addbr inline
ip link add ep1 type veth peer name ep2
ip link add con1 type veth peer name con2
brctl addif inline ep1
brctl addif inline con1
ifconfig con1 up
ifconfig ep1 up
ifconfig inline 10.50.1.1 netmask 255.255.255.0 up
ip netns add inline
ip link set ep2 netns inline
ip link set con2 netns inline
ip link set tap0 netns inline
ip netns exec inline ifconfig lo 127.0.0.1 up
ip netns exec inline ifconfig con2 10.50.1.240 netmask 255.255.255.0 up 
ip netns exec inline ip route add default via 10.50.1.1 dev con2
