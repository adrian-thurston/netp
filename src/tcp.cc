#include "netp.h"
#include "fmt.h"
#include "itq_gen.h"

#include <linux/tcp.h>
#include <ctype.h>

bool Handler::srcProtected( Packet *packet )
{
	int filterRes = pcap_offline_filter( &srcProtectedBpf,
			packet->ph, packet->bytes );
	return filterRes != 0;
}

bool Handler::dstProtected( Packet *packet )
{
	int filterRes = pcap_offline_filter( &dstProtectedBpf,
			packet->ph, packet->bytes );
	return filterRes != 0;
}

void Handler::createConnection( Packet *packet, Half &key )
{
	switch ( synAckBits( packet ) ) {
		case SA_BITS_SYN: {
			/* Creating on a syn. */
			Conn *connection = new Conn( netpConfigure, key );
			connection->dir = packet->dir;

			connection->state = Conn::Syn;
			connection->h1.seq = ntohl(packet->tcp.th->seq);
			connection->h2.seq = 0;

			packet->tcp.connection = connection;
			packet->tcp.half = &connection->h1;

			log_debug( DBG_TCP, "creating to syn state" );
			break;
		}

		case SA_BITS_SYN_ACK: {
			/* Since we are initting on a reply, first swap the direction
			 * before we initialize the connection with key. */
			key.swapDir();

			Conn *connection = new Conn( netpConfigure, key );

			connection->dir =
					packet->dir == Packet::Ingress ?
					Packet::Egress :
					Packet::Ingress;

			connection->state = Conn::SynAck;

			connection->h1.seq = ntohl(packet->tcp.th->ack_seq);
			connection->h2.seq = ntohl(packet->tcp.th->seq);

			packet->tcp.connection = connection;
			packet->tcp.half = &connection->h1;

			log_debug( DBG_TCP, "new conn syn-ack" );
			break;
		}

		case SA_BITS_ACK: {
			/* Intting on just an ack. We don't know if this is the third
			 * packet in the handshake, or ifack the ack if just some random
			 * ack of data. Therefore we cannot assume the direction, so we
			 * orient the connection ends protected -> wild */
			if ( packet->dir == Packet::Ingress )
				key.swapDir();

			Conn *connection = new Conn( netpConfigure, key );

			connection->dir = packet->dir;

			connection->state = Conn::Established;
			connection->h1.seq = ntohl(packet->tcp.th->seq);
			connection->h2.seq = ntohl(packet->tcp.th->ack_seq);

			packet->tcp.connection = connection;
			packet->tcp.half = &connection->h1;

			log_debug( DBG_TCP, "creating to established on ACK, seq -> seq: " <<
					connection->h1.seq << " -> " << connection->h2.seq );

			connection->established();
			break;
		}

		/* Unknown. -- */
		case SA_BITS_NONE: {
			/* We don't know the connection direction, orient the
			 * connection ends protected -> wild. */
			if ( packet->dir == Packet::Ingress )
				key.swapDir();

			Conn *connection = new Conn( netpConfigure, key );

			connection->dir = Packet::UnknownDir;

			connection->state = Conn::Established;
			connection->h1.seq = ntohl(packet->tcp.th->ack_seq);
			connection->h2.seq = ntohl(packet->tcp.th->seq);

			log_debug( DBG_TCP, "creating to established on NO bits" );

			packet->tcp.connection = connection;
			packet->tcp.half = &connection->h1;

			connection->established();
			break;
		}
	}

	halfDict.insert( &packet->tcp.connection->h1 );
	halfDict.insert( &packet->tcp.connection->h2 );
}

/* SYN has been seen. */
void Handler::flowSynState( Packet *packet )
{
	if ( packet->dir == packet->tcp.connection->dir ) {
		/* Packet moving in the same direction as the connection. */
		switch ( synAckBits( packet ) ) {
			case SA_BITS_SYN:
				/* Dupe, ignore */
				break;
			case SA_BITS_SYN_ACK:
				/* An ack, but wrong dir, ignore. */
				break;
			case SA_BITS_ACK:
				/* Lost syn-ack, jump ahead in protocol. */
				break;
			case SA_BITS_NONE:
				/* Certainly lost packets. */
				break;
		}
	}
	else {
		switch ( synAckBits( packet ) ) {
			case SA_BITS_SYN:
				/* Got a syn back? Certainly unusual, just drop. */
				break;
			case SA_BITS_SYN_ACK:
				/* Normal TCP protocol: Syn-ack in return. */
				packet->tcp.connection->state = Conn::SynAck;
			
				/* assert( ntohl(packet->tcp.th->ack_seq) == packet->tcp.connection->h1.seq + 1 ) */
				packet->tcp.connection->h1.seq = ( packet->tcp.connection->h1.seq + 1 );
				packet->tcp.connection->h2.seq = ntohl(packet->tcp.th->seq);
				break;
			case SA_BITS_ACK:
				/* Wrong dir. */
				break;
			case SA_BITS_NONE:
				/* Lost packets, jump ahead.. */
				break;
		}
	}
}

/* SYN-ACK has been seen. */
void Handler::flowSynAckState( Packet *packet )
{
	if ( packet->dir == packet->tcp.connection->dir ) {
		switch ( synAckBits( packet ) ) {
			case SA_BITS_SYN:
				/* Dupe of syn, ignore. */
				break;
			case SA_BITS_SYN_ACK:
				/* wrong direction, ignore. */
				break;
			case SA_BITS_ACK:
				/* Normal TCP protocol, ack in return of syn-ack. */
				packet->tcp.connection->state = Conn::Established;

				/* assert( ntohl(packet->tcp.th->ack_seq) == packet->tcp.connection->h2.seq ) + 1 */
				packet->tcp.connection->h2.seq = ( packet->tcp.connection->h2.seq + 1 );

				packet->tcp.connection->established();

				break;
			case SA_BITS_NONE:
				/* Jump ahead over the syn-ack. */
				break;
		}
	}
	else {
		switch ( synAckBits( packet ) ) {
			case SA_BITS_SYN:
				/* Wrong dir, in the past, ignore. */
				break;
			case SA_BITS_SYN_ACK:
				/* Dupe */
				break;
			case SA_BITS_ACK:
				/* Wrong dir, ignore. */
				break;
			case SA_BITS_NONE:
				/* Jump ahead over the syn-ack. */
				break;

		}
	}
}

void Handler::flowData( Packet *packet )
{
	/* First try to identify. May produce a parser. */
	if ( packet->tcp.connection->proto == Conn::Working ) {

		log_debug( DBG_IDENT, "sending data to identifier" );
		packet->tcp.half->identifier.receive( &packet->tcp.half->ctx, packet, packet->data, packet->caplen );

		if ( packet->tcp.half->identifier.proto == Identifier::HTTP_REQ ||
			packet->tcp.half->identifier.proto == Identifier::HTTP_RSP )
		{
			Conn *connection = packet->tcp.connection;

			connection->proto = Conn::HTTP;

			HttpRequestParser *requestParser = new HttpRequestParser( &connection->h1.ctx );
			HttpResponseParser *responseParser = new HttpResponseParser( &connection->h2.ctx, 0, false, false );

			requestParser->responseParser = responseParser;
			responseParser->requestParser = requestParser;

			connection->h1.parser = requestParser;
			connection->h2.parser = responseParser;

			requestParser->start( &connection->h1.ctx );
			responseParser->start( &connection->h2.ctx );
		}
	}

	/* If the half has a parser then send data to it. */
	if ( packet->tcp.half->parser != 0 ) {

		packet->tcp.half->ctx.vpt.packet = packet;

		packet->tcp.half->parser->receive( &packet->tcp.half->ctx, packet, packet->data, packet->caplen );

		packet->tcp.half->ctx.vpt.packet = 0;
	}

	log_debug( DBG_TCP, "tcp payload: " << log_binary( packet->data, packet->caplen ) );
}

void Handler::flowEstabState( Packet *packet )
{
	uint32_t pktseq = ntohl(packet->tcp.th->seq);

	log_debug( DBG_TCP, "expecting seq: " << packet->tcp.half->seq << " packet seq: " << pktseq );

	flowData( packet );

	if ( packet->caplen < packet->dlen ) {
		continuation = true;
		contPacket = *packet;

		/* Expecting a continuation. */
		log_debug( DBG_TCP, "expecting a continuation of " << ( packet->dlen - packet->caplen ) << " bytes" );

	}
	else {
		continuation = false;

		packet->tcp.half->seq = pktseq + packet->dlen;

		if ( packet->tcp.th->fin ) {
			log_debug( DBG_TCP, FmtConnection(packet->tcp.connection) <<
					": fin, connection closed" );
		}
	}

}

void Handler::flow( Packet *packet )
{
	switch ( packet->tcp.connection->state ) {
		case Conn::Syn:
			flowSynState( packet );
			break;

		case Conn::SynAck:
			flowSynAckState( packet );
			break;

		case Conn::Established:
			flowEstabState( packet );
			break;
	}
}

void Conn::established()
{
	h1.identifier.start( &h1.ctx );
	h2.identifier.start( &h2.ctx );
}

void Handler::tcp( Packet *packet )
{
	log_debug( DBG_TCP, "TCP: source port: " <<
		FmtIpPortNet(packet->tcp.th->source) << " dest port: " << FmtIpPortNet(packet->tcp.th->dest) );

	Half key( netpConfigure, 0,
			packet->ih->saddr, packet->ih->daddr,
			packet->tcp.th->source, packet->tcp.th->dest );
	
	HalfDictEl *halfEl = halfDict.find( &key );

	if ( halfEl != 0 ) {
		log_debug( DBG_TCP, "existing connection" );

		packet->tcp.half = halfEl->key;
		packet->tcp.connection = packet->tcp.half->connection;

		flow( packet );
	}
	else {
		log_debug( DBG_TCP, "new connection: " <<
				FmtIpAddrNet(packet->ih->saddr) << ':' << FmtIpPortNet(packet->tcp.th->source) << " -> " <<
				FmtIpAddrNet(packet->ih->daddr) << ':' << FmtIpPortNet(packet->tcp.th->dest) );

		createConnection( packet, key );
	}
}
