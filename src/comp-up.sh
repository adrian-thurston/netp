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
ip link add pipe2 type veth peer name pipe3

# Put outside end on the bridge to outside.
brctl addif inline pipe0
ifconfig pipe0 up

# Create the namespace.
ip netns add inline

# Move inside end of pairs to inline network namespace.
ip link set pipe1 netns inline
ip link set pipe2 netns inline

# Set up network in inline namespace
ip netns exec inline ifconfig lo 127.0.0.1 up

# Configure shuttle and kring.
ip netns exec inline bash @pkglibexecdir@/shuttle-up
ip netns exec inline ifconfig shuttle1 10.50.2.2 netmask 255.255.255.0 up 

ip netns add prot
ip link set pipe3 netns prot
ip netns exec prot ifconfig lo 127.0.0.1 up
ip netns exec prot ifconfig pipe3 10.50.2.3 \
	broadcast 10.50.2.255 netmask 255.255.255.0 up
ip netns exec prot route add default gw 10.50.2.1 pipe3

mkdir -p /etc/netns/prot
echo "nameserver 10.50.2.1" > /etc/netns/prot/resolv.conf
