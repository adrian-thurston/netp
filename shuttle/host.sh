#!/bin/bash
#

set -x

# Create the VE namespace
ip netns add ve0

# Need a correct resolv.conf for the host network.
# echo "nameserver 192.168.1.1" >/etc/netns/ve0/resolv.conf

# Move all the physical devices we will use to the namespace.
ip link set enp0s25 netns ve0
ip link set enx94103eb85cb7 netns ve0
ip link set enx94103eb85caa netns ve0

# The rest must happen inside the namespace.
ip netns exec ve0 bash -c "

set -x

ifconfig lo 127.0.0.1 up

# For VE to work right.
iptables -P FORWARD DROP

#
# Gray, external, to repeater
#
brctl addbr ext1
brctl addif ext1 enx94103eb85cb7

#
# Black, internal, to colm networks.
#
brctl addbr int1
brctl addif int1 enx94103eb85caa

#
# Bring up the inline interfaces.
#

ip link set int1 up
ip link set enx94103eb85caa up

ip link set ext1 up
ip link set enx94103eb85cb7 up

# Bring up the connect interface.
ifup enp0s25

"
