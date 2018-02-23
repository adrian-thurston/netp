#!/bin/bash
#

set -e

if [ '!' `whoami` = root ]; then
	echo "updown: must run as root"
	exit 1
fi

NET=10.50.10

# $NET.1 - DNS server be it on the outside or provided by the inside vpn.
# $NET.2 - inside shuttle IP address, necessary for proxy to work
# $NET.3 - interface in protected network (optional)

shuttle_up()
{
	OUTSIDE=$1
	INSIDE=$2

	godo insmod @KRING_MOD@
	undo rmmod kring

	# FIXME: remove rings
	echo r0 4 > /sys/kring/add_data
	echo r1 4 > /sys/kring/add_data

	echo c0 > /sys/kring/add_cmd

	godo insmod @SHUTTLE_MOD@
	undo rmmod shuttle

	godo ip netns exec inline bash -c "echo shuttle1 c0 r0 >/sys/shuttle/add"
	undo ip netns exec inline bash -c '"'"echo shuttle1 >/sys/shuttle/del"'"'

	godo ip netns exec inline bash -c "echo $OUTSIDE outside  >/sys/shuttle/shuttle1/port_add"
	undo ip netns exec inline bash -c '"'"echo $OUTSIDE >/sys/shuttle/shuttle1/port_del"'"'

	godo ip netns exec inline bash -c "echo $INSIDE inside  >/sys/shuttle/shuttle1/port_add"
	undo ip netns exec inline bash -c '"'"echo $INSIDE >/sys/shuttle/shuttle1/port_del"'"'

	godo ip netns exec inline ip link set $OUTSIDE up
	undo ip netns exec inline ip link set $INSIDE down

	godo ip netns exec inline ip link set $INSIDE up
	undo ip netns exec inline ip link set $OUTSIDE down

	godo ip netns exec inline iptables -t mangle -N DIVERT
	undo ip netns exec inline iptables -t mangle -X DIVERT

	godo ip netns exec inline iptables -t mangle -A PREROUTING -p tcp -m socket -j DIVERT
	undo ip netns exec inline iptables -t mangle -D PREROUTING -p tcp -m socket -j DIVERT

	godo ip netns exec inline iptables -t mangle -A DIVERT -j MARK --set-mark 101
	undo ip netns exec inline iptables -t mangle -D DIVERT -j MARK --set-mark 101

	godo ip netns exec inline iptables -t mangle -A DIVERT -j ACCEPT
	undo ip netns exec inline iptables -t mangle -D DIVERT -j ACCEPT

	godo ip netns exec inline ip rule add fwmark 101 lookup 101
	undo ip netns exec inline ip rule del fwmark 101 lookup 101

	godo ip netns exec inline ip route add local default dev lo table 101
	undo ip netns exec inline ip route del local default dev lo table 101

	godo ip netns exec inline iptables -t mangle -A PREROUTING -p tcp -m tcp \
			--dport 443 -j TPROXY --on-port 4430 --tproxy-mark 101/101
	undo ip netns exec inline iptables -t mangle -D PREROUTING -p tcp -m tcp \
			--dport 443 -j TPROXY  --on-port 4430 --tproxy-mark 101/101

	godo ip netns exec inline ifconfig shuttle1 $NET.2 netmask 255.255.255.0 up 
	undo ip netns exec inline ifconfig shuttle1 0.0.0.0 down
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
	godo dnsmasq --conf-file=@pkgstatedir@/dnsmasq.conf

	undo kill '`cat @piddir@/dnsmasq.pid`'

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

	godo openvpn --config @pkgstatedir@/openvpn.conf

	undo start-stop-daemon --stop --pidfile @piddir@/openvpn.pid

	# Move the device to the inline space.
	godo ip link set tap0 netns inline
	undo ip netns exec inline ip link set tap0 netns 1
}

bridge_up()
{
	bridge=$1
	out=$2

	# Create the bridge for forwarding to the outside world. 
	godo brctl addbr $bridge
	undo brctl delbr $bridge

	# Configure the bridge with an IP address and set up the forwarding to outside.
	godo ifconfig $bridge $NET.1 netmask 255.255.255.0 up
	undo ifconfig $bridge 0.0.0.0 down

	godo iptables -t nat -A POSTROUTING -o $out -j MASQUERADE
	undo iptables -t nat -D POSTROUTING -o $out -j MASQUERADE

	godo iptables -I FORWARD -i $bridge -o $out -j ACCEPT
	undo iptables -D FORWARD -i $bridge -o $out -j ACCEPT

	godo iptables -I FORWARD -i $out -o $bridge -j ACCEPT
	undo iptables -D FORWARD -i $out -o $bridge -j ACCEPT

	# Create a veth pair for the data going out
	godo ip link add pipe0 type veth peer name pipe1
	undo ip link del pipe0 type veth peer name pipe1

	# Put outside end on the bridge to outside.
	godo brctl addif inline pipe0
	undo brctl delif inline pipe0

	# Bring up the outside.
	godo ifconfig pipe0 up
	undo ifconfig pipe0 down

	# Move inside end of pairs to inline network namespace.
	godo ip link set pipe1 netns inline
	undo ip netns exec inline ip link set pipe1 netns 1
}

create_inline()
{
	# Create the namespace.
	godo ip netns add inline
	undo ip netns del inline

	# Set up network in inline namespace
	godo ip netns exec inline ifconfig lo 127.0.0.1 up
	undo ip netns exec inline ifconfig lo 0.0.0.0 down
}

inside_bridge()
{
	godo ip link add pipe2 type veth peer name pipe3
	undo ip link del pipe2 type veth peer name pipe3

	godo ip link set pipe2 netns inline
	undo ip netns exec inline ip link set pipe2 netns 1

	godo ip netns add prot
	undo ip netns del prot

	godo ip link set pipe3 netns prot
	undo ip netns exec prot ip link set pipe3 netns 1

	godo ip netns exec prot ifconfig lo 127.0.0.1 up
	undo ip netns exec prot ifconfig lo 0.0.0.0 down

	godo ip netns exec prot ifconfig pipe3 $NET.3 \
		broadcast $NET.255 netmask 255.255.255.0 up
	undo ip netns exec prot ifconfig lo 0.0.0.0 down

	godo ip netns exec prot route add default gw $NET.1 pipe3
	undo ip netns exec prot ifconfig pipe3 0.0.0.0 down

	mkdir -p /etc/netns/prot
	echo "nameserver $NET.1" > /etc/netns/prot/resolv.conf
}

comp_up()
{
	create_inline

	inside_bridge

	bridge_up inline wlan0

	dnsmasq_up

	# Configure shuttle and kring.
	shuttle_up pipe1 pipe2
}

live_up()
{
	# Create central namespace
	create_inline

	# Move connection to internal network to network namespace.
	godo ip link set em1 netns inline
	undo ip netns exec inline ip link set em1 netns 1

	# Set up outside.
	bridge_up inline wlan0
	dnsmasq_up

	# Configure shuttle and kring.
	shuttle_up pipe1 em1
}

vpn_up()
{
	# Create central namespace.
	create_inline

	# Set up inside.
	openvpn_up

	# Set up outside
	bridge_up inline eth0

	# Setup the shuttle inside the namespace.
	shuttle_up pipe1 tap0
}

UNDO=@pkgstatedir@/undo

godo()
{
	( set -x; "$@" )
}

undo()
{
	echo "$@" >> $UNDO
}

# Modes
#  bare    - both ends pure wire
#            needs IP address on network, interface names
#  vpn     - outside bridge, inside vpn
#            needs forward interfaces
#  live    - outside bridge, inside netns and/or bare
#            needs forward interfaces, inside config

# Expected system configuration. Not torn down the -down script.
for fn in /proc/sys/net/ipv4/conf/*/rp_filter; do echo 0 > $fn; done
echo 1 > /proc/sys/kernel/core_uses_pid
echo "/tmp/core-%e-%s-%u-%g-%p-%t" > /proc/sys/kernel/core_pattern
echo 2 > /proc/sys/fs/suid_dumpable
sysctl -q net.ipv4.ip_forward=1
iptables -P FORWARD DROP

case $1 in
	up)
		if [ -f $UNDO ]; then
			echo "updown: some config is up already, bring down first"
			exit 1;
		fi
		case $2 in
			vpn)
				vpn_up
			;;
			live)
				live_up
			;;
			comp)
				comp_up
			;;
		esac
	;;
	down)
		if [ '!' -f $UNDO ]; then
			echo "updown: nothing to bring down"
			exit 1;
		fi
		tac $UNDO > $UNDO.tac
		set -x
		source $UNDO.tac
		rm $UNDO $UNDO.tac
	;;
esac
