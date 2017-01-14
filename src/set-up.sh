#!/bin/bash
#

set -x 

insmod ./filter.ko

echo filter1 >/sys/filter/add

echo eth1 inside  >/sys/filter/filter1/port_add
echo eth2 outside  >/sys/filter/filter1/port_add

ip link set eth1 up
ip link set eth2 up

ifconfig filter1 192.168.1.195 netmask 255.255.255.0
