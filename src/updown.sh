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
}

bridge_up()
{
	# Create the bridge for forwarding to the outside world. 
	godo brctl addbr inline
	undo brctl delbr inline

	# Configure the bridge with an IP address and set up the forwarding to outside.
	godo ifconfig inline $NET.1 netmask 255.255.255.0 up
	undo ifconfig inline 0.0.0.0 down

	godo iptables -t nat -A POSTROUTING -o wlan0 -j MASQUERADE
	undo iptables -t nat -D POSTROUTING -o wlan0 -j MASQUERADE

	godo iptables -I FORWARD -i inline -o wlan0 -j ACCEPT
	undo iptables -D FORWARD -i inline -o wlan0 -j ACCEPT

	godo iptables -I FORWARD -i wlan0 -o inline -j ACCEPT
	undo iptables -D FORWARD -i wlan0 -o inline -j ACCEPT
}

comp_up()
{
	out=wlan0
	pipe=pipe0

	bridge_up
	dnsmasq_up

	# Create a veth pair for the data going out
	godo ip link add pipe0 type veth peer name pipe1
	undo ip link del pipe0 type veth peer name pipe1

	godo ip link add pipe2 type veth peer name pipe3
	undo ip link del pipe2 type veth peer name pipe3

	# Put outside end on the bridge to outside.
	godo brctl addif inline pipe0
	undo brctl delif inline pipe0

	godo ifconfig pipe0 up
	undo ifconfig pipe0 down

	# Create the namespace.
	godo ip netns add inline
	undo ip netns del inline

	# Move inside end of pairs to inline network namespace.
	godo ip link set pipe1 netns inline
	undo ip netns exec inline ip link set pipe1 netns 1

	godo ip link set pipe2 netns inline
	undo ip netns exec inline ip link set pipe2 netns 1

	# Set up network in inline namespace
	godo ip netns exec inline ifconfig lo 127.0.0.1 up
	undo ip netns exec inline ifconfig lo 0.0.0.0 down

	# Configure shuttle and kring.
	shuttle_up pipe1 pipe2

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

live_up()
{
	out=wlan0
	pipe=pipe0

	bridge_up
	dnsmasq_up

	# Create a veth pair for the data going out
	godo ip link add pipe0 type veth peer name pipe1
	undo ip link del pipe0 type veth peer name pipe1

	# Put outside end on the bridge to outside.
	godo brctl addif inline pipe0
	undo brctl delif inline pipe0

	godo ifconfig pipe0 up
	undo ifconfig pipe0 down

	# Create the namespace.
	godo ip netns add inline
	undo ip netns del inline

	# Move inside end of the pair to inline network namespace.
	godo ip link set pipe1 netns inline
	undo ip netns exec inline ip link set pipe1 netns 1

	# Move connection to internal network to network namespace.
	godo ip link set em1 netns inline
	undo ip netns exec inline ip link set em1 netns 1

	# Set up network in inline namespace
	godo ip netns exec inline ifconfig lo 127.0.0.1 up
	undo ip netns exec inline ifconfig lo 0.0.0.0 down

	# Configure shuttle and kring.
	shuttle_up pipe1 em1
}

vpn_up()
{
	# start VPN service.
	openvpn_up

	# Add the bridge for outgoing.
	godo brctl addbr inline
	undo brctl delbr inline

	# Configure it with an IP address.
	godo ifconfig inline $NET.1 netmask 255.255.255.0 up
	undo ifconfig inline 0.0.0.0 down

	# Enable forwarding out.
	godo iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
	undo iptables -t nat -D POSTROUTING -o eth0 -j MASQUERADE

	godo iptables -I FORWARD -i inline -o eth0 -j ACCEPT
	undo iptables -D FORWARD -i inline -o eth0 -j ACCEPT

	godo iptables -I FORWARD -i eth0 -o inline -j ACCEPT
	undo iptables -D FORWARD -i eth0 -o inline -j ACCEPT

	# Create the veth peer for the connection out.
	godo ip link add pipe0 type veth peer name pipe1
	undo ip link del pipe0 type veth peer name pipe1

	# Add the outside end to the output bridge.
	godo brctl addif inline pipe0
	undo brctl delif inline pipe0

	# Bring it up. 
	godo ifconfig pipe0 up
	undo ifconfig pipe0 down

	# Create the namespace
	godo ip netns add inline
	undo ip netns del inline

	# Move interior interfaces to it.
	godo ip link set pipe1 netns inline
	undo ip netns exec inline ip link set pipe1 netns 1

	godo ip link set tap0 netns inline
	undo ip netns exec inline ip link set tap0 netns 1

	# Configure interior loopback.
	godo ip netns exec inline ifconfig lo 127.0.0.1 up
	undo ip netns exec inline ifconfig lo 0.0.0.0 down

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
			echo "updown: already up, bring down first"
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
			echo "updown: already up, bring down first"
			exit 1;
		fi
		tac $UNDO > $UNDO.tac
		set -x
		source $UNDO.tac
		rm $UNDO $UNDO.tac
	;;
esac
