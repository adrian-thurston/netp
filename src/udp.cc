#include "netp.h"
#include "itq_gen.h"
#include "fmt.h"

#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <arpa/inet.h>
#include <pcap.h>

void Handler::RR_A( const char *data, int length )
{
	String s( data, length );
	nameCollect.A.append( s );
}

void Handler::QUESTION_NAME( const char *data, int length )
{
	nameCollect.question.setAs( data, length );
}

void Handler::reportName()
{
	log_debug( DBG_DNS, "QUESTION_NAME: " << nameCollect.question );
	for ( Vector<String>::Iter i = nameCollect.A; i.lte(); i++ ) {
		log_debug( DBG_DNS, "  RR_A: " << *i );
	}
}

void Handler::udp( Packet *packet )
{
	log_debug( DBG_UDP, "UDP dlen: " << packet->dlen << " source port: " <<
			FmtIpPortNet(packet->uh->source) << " dest port: " << FmtIpPortNet(packet->uh->dest) );
	
	if ( ntohs(packet->uh->source) == 53 || ntohs(packet->uh->dest) == 53 ) {
		DnsParser dnsParser;
		dnsParser.data( &udpCtx, packet, packet->data, packet->dlen );
	}
}
