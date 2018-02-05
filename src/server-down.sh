#!/bin/bash
#

set -x

ip netns exec inline ip route del default via 10.50.1.1 dev con2
ip netns exec inline ifconfig con2 0.0.0.0 down
ip netns exec inline ifconfig lo 0.0.0.0 down
ip netns exec inline ip link set tap0 netns 1
ip netns exec inline ip link set con2 netns 1
ip netns exec inline ip link set ep2 netns 1
ip netns del inline
ifconfig inline 0.0.0.0 down
ifconfig ep1 down
ifconfig con1 down
brctl delif inline ep1
brctl delif inline con1
ip link del ep1 type veth peer name ep1
ip link del con1 type veth peer name con2
brctl delbr inline
sysctl net.ipv4.ip_forward=0
iptables -t nat -D POSTROUTING -o eth0 -j MASQUERADE

pkill openvpn


