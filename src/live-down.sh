#!/bin/bash
#

out=wlan0
pipe=pipe0

set -x

# Tear down the shuttle and kring
ip netns exec inline ifconfig shuttle1 0.0.0.0 down
ip netns exec inline bash @pkglibexecdir@/shuttle-down

# Deconfigure loopback.
ip netns exec inline ifconfig lo 0.0.0.0 down

# Move internal net connection out of inline namespace.
ip netns exec inline ip link set em1 netns 1

# Move inside end of outside pair out of inline namespace.
ip netns exec inline ip link set pipe1 netns 1

# Remove the inline network namespace.
ip netns del inline

# Take down the outside end of the outside pair and take it off the bridge.
ifconfig pipe0 down
brctl delif inline pipe0

# Delete the outside pair.
ip link del pipe0 type veth peer name pipe1

# DHCP server down.
sudo kill `cat @piddir@/dnsmasq.pid`

# Deconfigure the outside bridge.
iptables -D FORWARD -i inline -o wlan0 -j ACCEPT
iptables -D FORWARD -i wlan0 -o inline -j ACCEPT
iptables -t nat -D POSTROUTING -o wlan0 -j MASQUERADE
ifconfig inline 0.0.0.0 down

# Remove the bridge.
brctl delbr inline

