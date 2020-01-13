#ifndef _PACKET_H
#define _PACKET_H

#include <pcap.h>

struct Half;
struct Conn;
struct Handler;

struct PackTcp
{
	struct tcphdr *th;
	Conn *connection;
	Half *half;
};

struct Packet
{
	Handler *handler;

	const struct pcap_pkthdr *ph;
	const u_char *bytes;

	struct ethhdr *eh;
	struct iphdr *ih;

	/* Packet is either ingress or egress, all other packets are dropped.
	 * Therefore Unknown is not used for the packet. Connections, however, can
	 * be ingress, egress or unknown. */
	enum Dir { Ingress = 1, Egress, UnknownDir };
	Dir dir;

	union {
		PackTcp tcp;
		struct udphdr *uh;
	};

	u_char *data;
	int dlen;
	int caplen;
};

inline Packet::Dir reverse( Packet::Dir dir )
{
	return dir == Packet::Ingress ? 
			Packet::Egress : 
			( dir == Packet::Egress ?
				Packet::Ingress : Packet::UnknownDir );
}

#endif
