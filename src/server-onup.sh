#!/bin/bash
#

dev=$1
tap_mtu=$2

set -x

ip link set dev "$1" mtu "$tap_mtu"

