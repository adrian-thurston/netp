#!/bin/bash
#

shuttle_up()
{
	OUTSIDE=$1
	INSIDE=$2

	undo restart_mark kernel $OUTSIDE $INSIDE

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

	godo ip netns exec inline ifconfig shuttle1 $SHUTTLE_IP netmask 255.255.255.0 up 
	undo ip netns exec inline ifconfig shuttle1 0.0.0.0 down
}

dnsmasq_up()
{
	cat <<-EOF > @pkgstatedir@/dnsmasq.conf
		pid-file=@piddir@/dnsmasq.pid
		listen-address=$NET.1
		bind-interfaces
		dhcp-range=$NET.4,$NET.136
		dhcp-leasefile=@piddir@/leases.txt
		log-facility=@logdir@/dnsmasq.log
	EOF

	# Run DHCP server.
	godo dnsmasq --conf-file=@pkgstatedir@/dnsmasq.conf

	undo start-stop-daemon --stop --pidfile @piddir@/dnsmasq.pid
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
		ifconfig-pool $NET.137 $NET.254 255.255.255.0

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

		daemon
		writepid @piddir@/openvpn.pid 
		log      @logdir@/openvpn.log
		status   @logdir@/openvpn.status 10 
	EOF

	godo openvpn --config @pkgstatedir@/openvpn.conf

	# Wait five seconds for the tap device to appear
	iters=0
	while ! ip link show dev tap0 &>/dev/null; do
		sleep 0.1
		if [ $iters == 50 ]; then
			echo $package init.d start: pidfile was not created >&2
			exit 1
		fi
		iters=$((iters + 1))
	done

	undo start-stop-daemon --stop --pidfile @piddir@/openvpn.pid

}

bridge_up()
{
	BRIDGE=$1; shift;

	# Create the bridge for forwarding to the outside world. 
	godo brctl addbr $BRIDGE
	undo brctl delbr $BRIDGE

	# Configure the bridge with an IP address and set up the forwarding to outside.
	godo ifconfig $BRIDGE $NET.1 netmask 255.255.255.0 up
	undo ifconfig $BRIDGE 0.0.0.0 down

	for iface in $OUTSIDE_FORWARD; do 
		godo iptables -t nat -A POSTROUTING -o $iface -j MASQUERADE
		undo iptables -t nat -D POSTROUTING -o $iface -j MASQUERADE

		godo iptables -I FORWARD -i $BRIDGE -o $iface -j ACCEPT
		undo iptables -D FORWARD -i $BRIDGE -o $iface -j ACCEPT

		godo iptables -I FORWARD -i $iface -o $BRIDGE -j ACCEPT
		undo iptables -D FORWARD -i $iface -o $BRIDGE -j ACCEPT
	done

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

config_protnet()
{
	PIFACE=$1
	godo ip netns exec prot ifconfig lo 127.0.0.1 up
	undo ip netns exec prot ifconfig lo 0.0.0.0 down

	godo ip netns exec prot ifconfig $PIFACE $NET.3 \
			broadcast $NET.255 netmask 255.255.255.0 up
	undo ip netns exec prot ifconfig lo 0.0.0.0 down

	godo ip netns exec prot route add default gw $NET.1 $PIFACE
	undo ip netns exec prot ifconfig $PIFACE 0.0.0.0 down

	mkdir -p /etc/netns/prot
	echo "nameserver $NET.1" > /etc/netns/prot/resolv.conf
}

protnet_pipe()
{
	godo ip netns add prot
	undo ip netns del prot

	godo ip link add pipe2 type veth peer name pipe3
	undo ip link del pipe2 type veth peer name pipe3

	godo ip link set pipe2 netns inline
	undo ip netns exec inline ip link set pipe2 netns 1

	godo ip link set pipe3 netns prot
	undo ip netns exec prot ip link set pipe3 netns 1
}


live_up()
{
	NUM_LIVE=$(echo $LIVE_INTERFACES $LIVE_PROTNET $LIVE_VPN | wc -w)
	if [ $NUM_LIVE = 0 ]; then
		#
		# no interfaces, protnet, or vpn
		#
		echo "updown live: no interfaces, protnet or vpn configured"
		exit 1;
	fi

	# Create central namespace
	create_inline

	if [ $NUM_LIVE = 1 ]; then 
		#
		# Just one thing.
		#
		if [ -n "$LIVE_VPN" ]; then
			SHUTTLE_INSIDE=tap0

			# Set up inside.
			openvpn_up

			# Move the device to the inline space.
			godo ip link set tap0 netns inline
			undo ip netns exec inline ip link set tap0 netns 1

		elif [ -n "$LIVE_INTERFACES" ]; then
			SHUTTLE_INSIDE=$LIVE_INTERFACES

			# Move single interface to inside network namespace.
			godo ip link set $SHUTTLE_INSIDE netns inline
			undo ip netns exec inline ip link set $SHUTTLE_INSIDE netns 1

		elif [ -n "$LIVE_PROTNET" ]; then
			#
			# protnet and no interface: pipe
			#
			SHUTTLE_INSIDE=pipe2

			protnet_pipe

			config_protnet pipe3
		fi
	else
		# More than one thing. need a bridge.

		SHUTTLE_INSIDE=pipe2

		protnet_pipe

		# Make the bridge.
		godo ip netns exec prot brctl addbr pbr
		undo ip netns exec prot brctl delbr pbr

		if [ -n "$LIVE_VPN" ]; then
			# Start vpn and move it to the protnet.
			openvpn_up

			godo ip link set tap0 netns prot
			undo ip netns exec prot ip link set tap0 netns 1

			LIVE_VPN="tap0"
		fi

		# Move live interfaces to prot namespace
		for iface in $LIVE_INTERFACES; do
			godo ip link set $iface netns prot
			undo ip netns exec prot ip link set $iface netns 1
		done

		# Put everything that needs to go on the bridge.
		for iface in pipe3 $LIVE_VPN $LIVE_INTERFACES; do
			godo ip netns exec prot brctl addif pbr $iface
			undo ip netns exec prot brctl delif pbr $iface

			godo ip netns exec prot ifconfig $iface up
			undo ip netns exec prot ifconfig $iface down
		done

		# If we want a protnet, configure the bridge, otherwise it gets left
		# unconfigured in the protnet.. 
		if [ -n "$LIVE_PROTNET" ]; then
			config_protnet pbr
		else
			godo ip netns exec prot ifconfig pbr up
			undo ip netns exec prot ifconfig pbr down
		fi
	fi

	# Set up outside.
	bridge_up inline
	dnsmasq_up

	# Configure shuttle and kring.
	shuttle_up pipe1 $SHUTTLE_INSIDE

	# postgres_up
	# services_up
}

bring_up()
{
	if [ -n "$DIRECT_INTERFACES" ]; then
		# Direct mode uses two existing interfaces for both ends.
		create_inline

		for iface in $DIRECT_INTERFACES; do
			godo ip link set $iface netns inline
			undo ip netns exec inline ip link set $iface netns 1
		done

		shuttle_up $DIRECT_INTERFACES

		# postgres_up
		# services_up
	else
		live_up
	fi
}

postgres_up()
{
	godo sudo -u thurston @POSTGRES_PREFIX@/libexec/postgres/init.d start
	undo sudo -u thurston @POSTGRES_PREFIX@/libexec/postgres/init.d stop
}

bg_up()
{
	package=$1; prefix=$2; shift 2; options=$@
	bindir=$prefix/bin

	pidfile=$prefix/var/run/$package.pid

	(
		# Enable core dumps up to 500 MB. Other setup below.
		ulimit -c 512000

		godo $bindir/$package -b $options
	)

	# Wait two seconds for the pidfile to appear.
	iters=0
	while [ '!' -f $pidfile ]; do
		sleep 0.1
		if [ $iters == 20 ]; then
			echo $package init.d start: pidfile was not created >&2
			exit 1
		fi
		iters=$((iters + 1))
	done

	undo start-stop-daemon --stop --pidfile $pidfile --retry 5 '||' rm -f $pidfile
}

services_up()
{
	undo restart_mark services
	bg_up broker @BROKER_PREFIX@ $BROKER_OPTIONS
	bg_up sniff @SNIFF_PREFIX@ $NETP_OPTIONS
	bg_up tlsproxy @TLSPROXY_PREFIX@ --netns inline $TLSPROXY_OPTIONS
	bg_up fetch @FETCH_PREFIX@ $FETCH_OPTIONS
}

RESTART_BOUNCE=

restart_mark()
{
	bounce_target=$1
	if [ -n "$RESTART_BOUNCE" ] && [ "$RESTART_BOUNCE" = "$bounce_target" ]; then
		set +x

		set -- $(grep "^restart_mark $RESTART_BOUNCE" $UNDO)
		sed -i "/^restart_mark $RESTART_BOUNCE/,\${d}" $UNDO

		echo $@

		if [ "$2" = kernel ]; then
			shuttle_up $3 $4
			# postgres_up
		fi

		# services_up

		exit 0		
	fi
}

godo()
{
	( set -x; "$@" )
}

undo()
{
	echo "$@" >> $UNDO
}

set -e

if [ '!' `whoami` = root ]; then
	echo "updown: must run as root"
	exit 1
fi

SHUTTLE_NET=10.50.10
SHUTTLE_IP="$SHUTTLE_NET.2"
OUTSIDE_FORWARD=""
LIVE_INTERFACES=""
LIVE_PROTNET=""
LIVE_VPN=""
DIRECT_INTERFACES=""
BROKER_OPTIONS=""
NETP_OPTIONS=""
TLSPROXY_OPTIONS=""
FETCH_OPTIONS=""

if [ -f @sysconfdir@/updown.conf ]; then
	source @sysconfdir@/updown.conf;
fi

NET="$SHUTTLE_NET"

# $NET.1 - DNS server be it on the outside or provided by the inside vpn.
# $NET.2 - inside shuttle IP address, necessary for proxy to work
# $NET.3 - interface in protected network (optional)

UNDO=@pkgstatedir@/undo


# Expected system configuration. Not torn down the -down script.
for fn in /proc/sys/net/ipv4/conf/*/rp_filter; do echo 0 > $fn; done
echo 1 > /proc/sys/kernel/core_uses_pid
echo "/tmp/core-%e-%s-%u-%g-%p-%t" > /proc/sys/kernel/core_pattern
echo 2 > /proc/sys/fs/suid_dumpable
sysctl -q net.ipv4.ip_forward=1
iptables -P FORWARD DROP

case $1 in
	stop|restart)
		if [ '!' -f $UNDO ]; then
			echo "updown: nothing to bring down"
			exit 1;
		fi
		tac $UNDO > $UNDO.tac
		set -x

		if [ $1 = restart ]; then
			case $2 in
				services|kernel) RESTART_BOUNCE=$2;;
				*)
					echo "updown: invalid restart: $2"
					exit 1
				;;
			esac
		fi

		source $UNDO.tac

		rm $UNDO $UNDO.tac
	;;

	# Modes
	#  bare    - both ends pure wire
	#            needs IP address on network, interface names
	#  vpn     - outside bridge, inside vpn
	#            needs forward interfaces
	#  live    - outside bridge, inside netns and/or bare
	#            needs forward interfaces, inside config
	start)
		if [ -f $UNDO ]; then
			echo "updown: some config is up already, bring down first"
			exit 1;
		fi

		bring_up
	;;
	status|st)
		if [ -f $UNDO ]; then
			echo "updown: some config is up"
		else
			echo "updown: fully down"
		fi
	;;
	clear)
		rm -f $UNDO $UNDO.tac
	;;
esac
