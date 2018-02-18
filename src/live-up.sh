#!/bin/bash
#


out=wlan0
pipe=pipe0

set -x

# Create the namespace.
ip netns add inline

# Create the veth pairs for connect and data.
#ip link add con0 type veth peer name con1
ip link add pipe0 type veth peer name pipe1

# Move inside end of the pairs to inline network namespace.
#ip link set con1 netns inline
ip link set pipe1 netns inline

# Put outside end of pairs on network. 
#brctl addif sn01 con0
brctl addif sn01 pipe0

#sudo ifconfig con0 up
sudo ifconfig pipe0 up

# Move connection to internal network to network namespace.
ip link set em1 netns inline

# Set up network in inline namespace
ip netns exec inline ifconfig lo 127.0.0.1 up
#ip netns exec inline ifconfig con1 10.25.51.240 netmask 255.255.255.0 up 
#ip netns exec inline ip route add default via 10.25.51.1 dev con1

iptables -I FORWARD -i $out -o $pipe -j ACCEPT
iptables -I FORWARD -i $pipe -o $out -j ACCEPT
iptables -t nat -A POSTROUTING -o $out -j MASQUERADE

ip netns exec inline bash @pkglibexecdir@/shuttle-up

ip netns exec inline ifconfig shuttle1 10.25.51.240 netmask 255.255.255.0 up 
