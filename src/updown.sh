#!/bin/bash
#

set -xe

[ `whoami` = root ] || exit

NET=10.50.10

# $NET.1 - DNS server be it on the outside or provided by the inside vpn.
# $NET.2 - inside shuttle IP address, necessary for proxy to work
# $NET.3 - interface in protected network (optional)

shuttle_up()
{
	OUTSIDE=$1
	INSIDE=$2

	echo 1 > /proc/sys/kernel/core_uses_pid
	echo "/tmp/core-%e-%s-%u-%g-%p-%t" > /proc/sys/kernel/core_pattern
	echo 2 > /proc/sys/fs/suid_dumpable

	insmod @KRING_MOD@

	echo r0 4 > /sys/kring/add_data
	echo r1 4 > /sys/kring/add_data

	echo c0 > /sys/kring/add_cmd

	insmod @SHUTTLE_MOD@

	ip netns exec inline bash -c "echo shuttle1 c0 r0 >/sys/shuttle/add"

	ip netns exec inline bash -c "echo $OUTSIDE outside  >/sys/shuttle/shuttle1/port_add"
	ip netns exec inline bash -c "echo $INSIDE inside  >/sys/shuttle/shuttle1/port_add"

	ip netns exec inline ip link set $OUTSIDE up
	ip netns exec inline ip link set $INSIDE up

	#
	# Doesn't seem to matter if we assign an IP address or bring up device, so long
	# as we don't go changing interface configuration. Altering the config seems to
	# clear the ip route rules below, which is the real reason fiddling with these
	# configs caused the system to be so fragile in the beginning.
	#
	# ifconfig shuttle1 0.0.0.0 up
	#

	for fn in /proc/sys/net/ipv4/conf/*/rp_filter; do echo 0 > $fn; done

	ip netns exec inline iptables -t mangle -N DIVERT

	ip netns exec inline iptables -t mangle -A PREROUTING -p tcp -m socket -j DIVERT
	ip netns exec inline iptables -t mangle -A DIVERT -j MARK --set-mark 101
	ip netns exec inline iptables -t mangle -A DIVERT -j ACCEPT

	ip netns exec inline ip rule add fwmark 101 lookup 101
	ip netns exec inline ip route add local default dev lo table 101

	ip netns exec inline iptables -t mangle -A PREROUTING -p tcp -m tcp \
		--dport 443 -j TPROXY --on-port 4430 --tproxy-mark 101/101
}

shuttle_down()
{
	OUTSIDE=$1
	INSIDE=$2

	ip netns exec inline iptables -t mangle -D PREROUTING -p tcp -m tcp \
		--dport 443 -j TPROXY  --on-port 4430 --tproxy-mark 101/101

	ip netns exec inline ip route del local default dev lo table 101
	ip netns exec inline ip rule del fwmark 101 lookup 101

	ip netns exec inline iptables -t mangle -D PREROUTING -p tcp -m socket -j DIVERT
	ip netns exec inline iptables -t mangle -D DIVERT -j MARK --set-mark 101
	ip netns exec inline iptables -t mangle -D DIVERT -j ACCEPT

	ip netns exec inline iptables -t mangle -X DIVERT

	for fn in /proc/sys/net/ipv4/conf/*/rp_filter; do echo 0 > $fn; done

	###############

	ip netns exec inline ip link set $OUTSIDE down
	ip netns exec inline ip link set $INSIDE down

	ip netns exec inline bash -c "echo $INSIDE >/sys/shuttle/shuttle1/port_del"
	ip netns exec inline bash -c "echo $OUTSIDE >/sys/shuttle/shuttle1/port_del"

	ip netns exec inline bash -c "echo shuttle1 >/sys/shuttle/del"

	rmmod shuttle

	# FIXME: remove rings

	rmmod kring
}

dnsmasq_up()
{
	cat <<-EOF > @pkgstatedir@/dnsmasq.conf
		pid-file=@piddir@/dnsmasq.pid
		listen-address=$NET.1
		bind-interfaces
		dhcp-range=$NET.4,$NET.254
		dhcp-leasefile=@piddir@/leases.txt
		log-facility=@logdir@/dnsmasq.log
	EOF

	# Run DHCP server.
	dnsmasq --conf-file=@pkgstatedir@/dnsmasq.conf
}

dnsmasq_down()
{
	# DHCP server down.
	sudo kill `cat @piddir@/dnsmasq.pid`
}

openvpn_up()
{
	cat <<-EOF > @pkgstatedir@/openvpn.conf
		#
		# Server Configuration
		#

		dev tap
		cipher AES-256-CBC
		keepalive 10 60

		mode server
		tls-server

		ifconfig      $NET.1 255.255.255.0
		ifconfig-pool $NET.4 $NET.254 255.255.255.0

		push "route-gateway $NET.1"
		push "redirect-gateway"
		push "dhcp-option DNS 97.107.133.4"
		push "dhcp-option DNS 207.192.69.4"
		push "dhcp-option DNS 207.192.69.5"

		keepalive 10 60

		user nobody
		group nogroup
		persist-tun
		persist-key

		ifconfig-noexec
		route-noexec

		key  @pkgdatadir@/key.pem
		cert @pkgdatadir@/cert.pem
		ca   @pkgdatadir@/verify.pem

		dh   @DH_KEYS@

		script-security 2
		up      @pkglibexecdir@/config-iface
		down    @pkglibexecdir@/config-iface

		daemon
		writepid @piddir@/openvpn.pid 
		log      @logdir@/openvpn.log
		status   @logdir@/openvpn.status 10 
	EOF

	openvpn --config @pkgstatedir@/openvpn.conf
}

openvpn_down()
{
	# Stop the VPN.
	start-stop-daemon --stop --pidfile @piddir@/openvpn.pid
}

comp_up()
{
	out=wlan0
	pipe=pipe0

	# Create the bridge for forwarding to the outside world. 
	brctl addbr inline

	# Configure the bridge with an IP address and set up the forwarding to outside.
	ifconfig inline $NET.1 netmask 255.255.255.0 up
	iptables -t nat -A POSTROUTING -o wlan0 -j MASQUERADE
	iptables -I FORWARD -i inline -o wlan0 -j ACCEPT
	iptables -I FORWARD -i wlan0 -o inline -j ACCEPT

	dnsmasq_up

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
	shuttle_up pipe1 pipe2
	ip netns exec inline ifconfig shuttle1 $NET.2 netmask 255.255.255.0 up 

	ip netns add prot
	ip link set pipe3 netns prot
	ip netns exec prot ifconfig lo 127.0.0.1 up
	ip netns exec prot ifconfig pipe3 $NET.3 \
		broadcast $NET.255 netmask 255.255.255.0 up
	ip netns exec prot route add default gw $NET.1 pipe3

	mkdir -p /etc/netns/prot
	echo "nameserver $NET.1" > /etc/netns/prot/resolv.conf
}

comp_down()
{
	out=wlan0
	pipe=pipe0

	ip netns exec prot ifconfig pipe3 0.0.0.0 down
	ip netns exec prot ifconfig lo 0.0.0.0 down
	ip netns exec prot ip link set pipe3 netns 1
	ip netns del prot

	# Tear down the shuttle and kring
	ip netns exec inline ifconfig shuttle1 0.0.0.0 down
	shuttle_down pipe1 pipe2

	# Deconfigure loopback.
	ip netns exec inline ifconfig lo 0.0.0.0 down

	# Move inside end of pairs out of inline namespace.
	ip netns exec inline ip link set pipe1 netns 1
	ip netns exec inline ip link set pipe2 netns 1

	# Remove the inline network namespace.
	ip netns del inline

	# Take down the outside end of the outside pair and take it off the bridge.
	ifconfig pipe0 down
	brctl delif inline pipe0

	# Delete the outside pair.
	ip link del pipe2 type veth peer name pipe3
	ip link del pipe0 type veth peer name pipe1

	# DHCP server down.
	dnsmasq_down

	# Deconfigure the outside bridge.
	iptables -D FORWARD -i inline -o wlan0 -j ACCEPT
	iptables -D FORWARD -i wlan0 -o inline -j ACCEPT
	iptables -t nat -D POSTROUTING -o wlan0 -j MASQUERADE
	ifconfig inline 0.0.0.0 down

	# Remove the bridge.
	brctl delbr inline
}

live_up()
{
	out=wlan0
	pipe=pipe0

	# Create the bridge for forwarding to the outside world. 
	brctl addbr inline

	# Configure the bridge with an IP address and set up the forwarding to outside.
	ifconfig inline $NET.1 netmask 255.255.255.0 up
	iptables -t nat -A POSTROUTING -o wlan0 -j MASQUERADE
	iptables -I FORWARD -i inline -o wlan0 -j ACCEPT
	iptables -I FORWARD -i wlan0 -o inline -j ACCEPT

	dnsmasq_up

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
	shuttle_up pipe1 em1
	ip netns exec inline ifconfig shuttle1 $NET.2 netmask 255.255.255.0 up 
}

live_down()
{
	out=wlan0
	pipe=pipe0

	# Tear down the shuttle and kring
	ip netns exec inline ifconfig shuttle1 0.0.0.0 down
	shuttle_down pipe1 em1

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
	dnsmasq_down

	# Deconfigure the outside bridge.
	iptables -D FORWARD -i inline -o wlan0 -j ACCEPT
	iptables -D FORWARD -i wlan0 -o inline -j ACCEPT
	iptables -t nat -D POSTROUTING -o wlan0 -j MASQUERADE
	ifconfig inline 0.0.0.0 down

	# Remove the bridge.
	brctl delbr inline
}

vpn_up()
{
	# start VPN service.
	openvpn_up

	sleep 1

	# Expected system configuration. Not torn down the -down script.
	sysctl net.ipv4.ip_forward=1
	iptables -P FORWARD DROP

	# Add the bridge for outgoing.
	brctl addbr inline

	# Configure it with an IP address.
	ifconfig inline $NET.1 netmask 255.255.255.0 up

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
	shuttle_up pipe1 tap0
	ip netns exec inline ifconfig shuttle1 $NET.240 netmask 255.255.255.0 up 
}

vpn_down()
{
	# Take down the shuttle inside the namespace.
	ip netns exec inline ifconfig shuttle1 0.0.0.0 down
	shuttle_down pipe1 tap0

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
	openvpn_down
}

# Modes
#  bare    - both ends pure wire
#            needs IP address on network, interface names
#  vpn     - outside bridge, inside vpn
#            needs forward interfaces
#  live    - outside bridge, inside netns and/or bare
#            needs forward interfaces, inside config

$1 $2 $3
