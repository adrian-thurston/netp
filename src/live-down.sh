#!/bin/bash
#

out=wlan0
pipe=pipe0

set -x

ip netns exec inline bash @pkglibexecdir@/shuttle-down

iptables -t nat -D POSTROUTING -o $out -j MASQUERADE
iptables -D FORWARD -i $out -o $pipe -j ACCEPT
iptables -D FORWARD -i $pipe -o $out -j ACCEPT

ip netns exec inline ip route del default via 10.25.51.1 dev con1
ip netns exec inline ifconfig con1 0.0.0.0 down
ip netns exec inline ifconfig lo 0.0.0.0 down

ip netns exec inline ip link set em1 netns 1

sudo ifconfig con0 down
sudo ifconfig pipe0 down

brctl delif sn01 con0
brctl delif sn01 pipe0

ip netns exec inline ip link set con1 netns 1
ip netns exec inline ip link set pipe1 netns 1

ip link del pipe0 type veth peer name pipe1
ip link del con0 type veth peer name con1

ip netns del inline

