#include "parse.h"
#include "fmt.h"
#include "packet.h"
#include "itq_gen.h"

#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <arpa/inet.h>

Handler::Handler( NetpConfigure *netpConfigure )
:
	netpConfigure(netpConfigure),
	pcap(0),
	datalink(0),
	continuation(false),
	udpCtx( netpConfigure )
{
}

void Handler::handler( Packet::Dir dir, const struct pcap_pkthdr *h, const u_char *bytes )
{
	if ( continuation ) {
		log_debug( DBG_TCP, "have continuation data" );

		Packet *packet = &contPacket;

		packet->bytes = bytes;
		packet->data = (u_char*)bytes;
		packet->dlen = h->len;
		packet->caplen = h->caplen;

		flowEstabState( packet );
	}
	else {
		Packet _packet;
		Packet *packet = &_packet;

		packet->handler = this;
		packet->ph = h;
		packet->bytes = bytes;
		packet->eh = (struct ethhdr*)(bytes);

		log_debug( DBG_ETH, "eh proto: " << FmtEthProto( packet->eh->h_proto ) );

		if ( packet->eh->h_proto == htons( ETH_P_IP ) ) {

			if ( dir != Packet::UnknownDir )
				packet->dir = dir;
			else {
				bool sprot = srcProtected( packet );
				bool dprot = dstProtected( packet );

				if ( sprot && !dprot )
					packet->dir = Packet::Egress;
				else if ( !sprot && dprot )
					packet->dir = Packet::Ingress;
				else {
					/* Drop anything not strictly egress or ingress. */
					return;
				}
			}

			/* IP header and header length. */
			packet->ih = (struct iphdr*)(bytes + sizeof(struct ethhdr));
			const int ihlen = packet->ih->ihl * 4;

			log_debug( DBG_IP, "ip protocol: " << FmtIpProtocol( packet->ih->protocol ) << " " <<
					FmtIpAddrNet(packet->ih->saddr) << " -> " << FmtIpAddrNet(packet->ih->daddr) );

			if ( packet->ih->protocol == IPPROTO_TCP ) {
				/* TCP header and header length. */
				packet->tcp.th = (struct tcphdr*)( (u_char*)packet->ih + ihlen );
				const int thlen = packet->tcp.th->doff * 4;

				/* TCP data and data length. */
				packet->data = (u_char*)packet->tcp.th + thlen;
				packet->dlen = ntohs(packet->ih->tot_len) - thlen - ihlen;
				packet->caplen = h->caplen - thlen - ihlen - sizeof(struct ethhdr);

				tcp( packet );
			}
			else if ( packet->ih->protocol == IPPROTO_UDP ) {
				/* TCP header and header length. */
				packet->uh = (struct udphdr*)( (u_char*)packet->ih + ihlen );
				const int uhlen = sizeof( struct udphdr );

				/* TCP data and data length. */
				packet->data = (u_char*)packet->uh + uhlen;
				packet->dlen = ntohs(packet->ih->tot_len) - uhlen - ihlen;
				packet->caplen = h->caplen - uhlen - ihlen - sizeof(struct ethhdr);

				udpCtx.vpt.packet = packet;
				udp( packet );
				udpCtx.vpt.packet = 0;;
			}
		}
	}
}


