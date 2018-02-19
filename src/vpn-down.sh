#!/bin/bash
#

set -x

# Take down the shuttle inside the namespace.
ip netns exec inline ifconfig shuttle1 0.0.0.0 down
ip netns exec inline bash @pkglibexecdir@/shuttle-down

# Deconfigure interior loopback.
ip netns exec inline ifconfig lo 0.0.0.0 down

# Return interior interfaces to root namespace.
ip netns exec inline ip link set tap0 netns 1
ip netns exec inline ip link set pipe1 netns 1

# Destroy namespace.
ip netns del inline

# Take down outside connection.
ifconfig pipe0 down

# Remove from bridge.
brctl delif inline pipe0

# Remove outside veth pair.
ip link del pipe0 type veth peer name pipe1

# Deconfigure the forwarding.
iptables -t nat -D POSTROUTING -o eth0 -j MASQUERADE
iptables -D FORWARD -i inline -o eth0 -j ACCEPT
iptables -D FORWARD -i eth0 -o inline -j ACCEPT

# Take down the forward interface.
ifconfig inline 0.0.0.0 down

# Delete the bridge.
brctl delbr inline

# Stop the VPN.
start-stop-daemon --stop --pidfile @piddir@/openvpn.pid
