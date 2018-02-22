#!/bin/bash
#

set -x

# start VPN service.
openvpn --config @pkgdatadir@/openvpn.conf

sleep 1

# Expected system configuration. Not torn down the -down script.
sysctl net.ipv4.ip_forward=1
iptables -P FORWARD DROP

# Add the bridge for outgoing.
brctl addbr inline

# Configure it with an IP address.
ifconfig inline 10.50.1.1 netmask 255.255.255.0 up

# Enable forwarding out.
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
iptables -I FORWARD -i inline -o eth0 -j ACCEPT
iptables -I FORWARD -i eth0 -o inline -j ACCEPT

# Create the veth peer for the connection out.
ip link add pipe0 type veth peer name pipe1

# Add the outside end to the output bridge.
brctl addif inline pipe0

# Bring it up. 
ifconfig pipe0 up

# Create the namespace
ip netns add inline

# Move interior interfaces to it.
ip link set pipe1 netns inline
ip link set tap0 netns inline

# Configure interior loopback.
ip netns exec inline ifconfig lo 127.0.0.1 up

# Setup the shuttle inside the namespace.
ip netns exec inline bash @pkglibexecdir@/shuttle-up
ip netns exec inline ifconfig shuttle1 10.50.1.240 netmask 255.255.255.0 up 
