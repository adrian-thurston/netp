#!/bin/bash
#

out=wlan0
pipe=pipe0

set -x

# Create the bridge for forwarding to the outside world. 
brctl addbr inline

# Configure the bridge with an IP address and set up the forwarding to outside.
ifconfig inline 10.50.2.1 netmask 255.255.255.0 up
iptables -t nat -A POSTROUTING -o wlan0 -j MASQUERADE
iptables -I FORWARD -i inline -o wlan0 -j ACCEPT
iptables -I FORWARD -i wlan0 -o inline -j ACCEPT

# Run DHCP server.
dnsmasq --conf-file=@pkgdatadir@/dnsmasq.conf

# Create a veth pair for the data going out
ip link add pipe0 type veth peer name pipe1

# Put outside end on the bridge to outside.
brctl addif inline pipe0
ifconfig pipe0 up

# Create the namespace.
ip netns add inline

# Move inside end of the pair to inline network namespace.
ip link set pipe1 netns inline

# Move connection to internal network to network namespace.
ip link set em1 netns inline

# Set up network in inline namespace
ip netns exec inline ifconfig lo 127.0.0.1 up

# Configure shuttle and kring.
ip netns exec inline bash @pkglibexecdir@/shuttle-up
ip netns exec inline ifconfig shuttle1 10.50.2.2 netmask 255.255.255.0 up 
