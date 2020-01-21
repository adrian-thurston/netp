#include <parse/fmt.h>
#include <parse/module.h>
#include "sniff.h"
#include "main.h"
#include "itq_gen.h"

#include <signal.h>
#include <sys/mman.h>
#include <kring/kring.h>
#include <aapl/vector.h>
#include <sstream>
#include <parse/pattern.h>

struct MatchDnsAnswer
{
	MatchDnsAnswer( SniffThread *sniffThread )
	:
		sniffThread( sniffThread )
	{
		moduleList.loadProxyHostNames( &lookupSet );
	}

	SniffThread *sniffThread;
	LookupSet lookupSet;
	IpSet ipsAdded;

	int answerCname( const char *name, const char *cname )
	{
		if ( lookupSet.find( name ) ) {
			if ( !lookupSet.find( cname ) ) {
				char *dup = strdup( cname );
				log_debug( DBG_PAT_DNS, "CNAME lookup set add: " << cname );
				lookupSet.insert( dup );
			}
		}

		return 0;
	}

	int answerA( const char *name, const unsigned char *addr )
	{
		if ( lookupSet.find( name ) ) {
			char toa[16];
			sprintf( toa, "%u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3] );

			log_debug( DBG_PAT_DNS, "TARGET: " << name << " -> " << toa );
		
			uint32_t ip = 
					(uint32_t)addr[0] <<  0 |
					(uint32_t)addr[1] <<  8 |
					(uint32_t)addr[2] << 16 |
					(uint32_t)addr[3] << 24;

			if ( ! ipsAdded.find( ip ) ) {
				ipsAdded.insert( ip );

				log_debug( DBG_PAT_DNS, "TARGET: " << name << " -> " << toa );
				sniffThread->matchedDns( 0, toa );
			}
		}

		return 0;
	}
};

struct MatchDnsAnswer1
:
	public OnMatch
{
	MatchDnsAnswer1( MatchDnsAnswer *mda )
	:
		mda(mda)
	{}

	MatchDnsAnswer *mda;

	virtual MatchResult match( VPT *vpt, Node *node )
	{
		Node *rra = node; //findChild( node, Node::DnsRrA );
		if ( rra != 0 ) {
			Node *name = findChild( rra, Node::DnsName );
			Node *value = findChild( rra, Node::DnsAddr );

			if ( name != 0 && value != 0 && value->data != 0 ) {
				char toa[16] = "";
				sprintf( toa, "%u.%u.%u.%u",
						value->data[0], value->data[1], value->data[2], value->data[3] );
		
				uint32_t ip = 
						(uint32_t)value->data[0] <<  0 |
						(uint32_t)value->data[1] <<  8 |
						(uint32_t)value->data[2] << 16 |
						(uint32_t)value->data[3] << 24;

				log_debug( DBG_PAT_DNS, "A target check: " << name->text <<
						" -> " << toa << " (" << ip << ")" );

				mda->answerA( name->text.c_str(), value->data );
			}
		}
		return MatchFinished;
	}
};

struct MatchDnsAnswer2
:
	public OnMatch
{
	MatchDnsAnswer2( MatchDnsAnswer *mda )
	:
		mda(mda)
	{}

	MatchDnsAnswer *mda;

	virtual MatchResult match( VPT *vpt, Node *node )
	{
		Node *rra = node; //findChild( node, Node::DnsRrCname );
		if ( rra != 0 ) {
			Node *name = findChild( rra, Node::DnsName );
			Node *value = name->next;

			if ( name != 0 && value != 0 ) {
				log_debug( DBG_PAT_DNS, "CNAME target check: " << name->text << " -> " << value->text );

				mda->answerCname( name->text.c_str(), value->text.c_str() );
			}
		}
		return MatchFinished;
	}

	virtual void capture( VPT *vpt, Node *node, Vector<char> *text ) {}
};

void SniffThread::matchedDns( Packet *packet, char *toa )
{
	SniffThread *sniffThread = this;

	std::stringstream ss;
	ss << "p " << toa;

	struct kring_user *kring = &sniffThread->cmd;
	kctrl_write_plain( kring, (char*)ss.str().c_str(), ss.str().size() + 1 );

	/* Send out notification of the redirect. */
	Packer::KringRedirect bkr( sniffThread->sendsPassthru->writer );
	bkr.set_ip( toa );
	bkr.send();

	log_debug( DBG_PAT_DNS, "sent kring cmd: " << ss.str() );
}


PatNode *consPatDnsAnswer1( MatchDnsAnswer1 *a1 )
{
	PatNode *rra;
	PatNode *dns = new PatNode( 0, Node::DnsAnswer );
	dns->children.append( rra = new PatNode( dns, Node::DnsRrA ) );

	rra->onMatch = a1;
	rra->skip = true;
	return dns;
}


PatNode *consPatDnsAnswer2( MatchDnsAnswer2 *a2 )
{
	PatNode *rrcname;
	PatNode *dns = new PatNode( 0, Node::DnsAnswer );
	dns->children.append( rrcname = new PatNode( dns, Node::DnsRrCname ) );

	rrcname->onMatch = a2;
	rrcname->skip = true;
	return dns;
}

void SniffThread::configureContext( Context *ctx )
{
	MatchDnsAnswer *mda = new MatchDnsAnswer( this );
	MatchDnsAnswer1 *a1 = new MatchDnsAnswer1( mda );
	MatchDnsAnswer2 *a2 = new MatchDnsAnswer2( mda );

	ctx->addPat( consPatDnsAnswer1( a1 ) );
	ctx->addPat( consPatDnsAnswer2( a2 ) );

	moduleList.sniffConfigureContext( this, ctx );
}

void handler( u_char *user, const struct pcap_pkthdr *h, const u_char *bytes )
{
	SniffThread *ut = (SniffThread*) user;
	ut->handler.handler( Packet::UnknownDir, h, bytes );
}

static pcap_t *signalPcapAccess = 0;

static void sigusrHandler( int s )
{
	if ( signalPcapAccess != 0 ) 
		pcap_breakloop( signalPcapAccess );
}

void SniffThread::recvShutdown( Message::Shutdown *msg )
{
	log_message( "received shutdown" );
	breakLoop();
}


/* PCAP compile is not thread safe. */
void SniffThread::compileBpf()
{
	pcap_t *pcap = pcap_open_dead( DLT_EN10MB, 1024 );

	int cr1 = pcap_compile( pcap, &handler.srcProtectedBpf,
		"src net 10.0.0.0/8 or 172.16.0.0/12 or 192.168.0.0/16", 0,
		PCAP_NETMASK_UNKNOWN );

	if ( cr1 != 0 )
		log_ERROR( "failed to compile protected pcap expression: " << pcap_geterr( handler.pcap ) );

	int cr2 = pcap_compile( pcap, &handler.dstProtectedBpf,
		"dst net 10.0.0.0/8 or 172.16.0.0/12 or 192.168.0.0/16", 0,
		PCAP_NETMASK_UNKNOWN );

	if ( cr2 != 0 )
		log_ERROR( "failed to compile protected pcap expressoin: " << pcap_geterr( handler.pcap ) );

	pcap_close( pcap );
}

int SniffThread::sniffDecrypted()
{
	handler.pcap = pcap_open_dead( DLT_EN10MB, 1024 );

	compileBpf();

	int r = kring_open( &kring, KRING_DATA, ring, KRING_DECRYPTED, KDATA_RING_ID_ALL, KRING_READ );
	if ( r < 0 )
		log_FATAL( "decrypted data kring open failed: " << kdata_error( &kring, r ) );

	loopBegin();

	while ( true ) {
		poll();

		if ( !loopContinue() )
			break;

		/* Spin. */
		if ( kdata_avail( &kring ) ) {
			/* Load. */
			struct kdata_decrypted dcy;
			kdata_next_decrypted( &kring, &dcy );
			
			log_debug( DBG_DECR, "decrypted packet: conn: " <<
					dcy.id << " type: " <<
					( dcy.type == 1 ? "INSIDE" : "OUTSIDE" ) <<
					" host: " << dcy.host );
			log_debug( DBG_DECR, log_binary(dcy.bytes, dcy.len) );

			handler.decrypted( dcy.id, dcy.type, dcy.host, dcy.bytes, dcy.len );
		}

		int r = kdata_read_wait( &kring );
		if ( r < 0 )
			log_ERROR( "kring recv failed: " << strerror( errno ) );
	}

	return 0;
}


int SniffThread::sniffKring()
{
	struct kring_user kring;

	handler.pcap = pcap_open_dead( DLT_EN10MB, 1024 );

	int r = kring_open( &kring, KRING_DATA, ring, KRING_PACKETS, 0, KRING_READ );
	if ( r < 0 )
		log_FATAL( "packet kring open failed: " << kdata_error( &kring, r ) );

	r = kring_open( &cmd, KRING_CTRL, "c0", KRING_PLAIN, 0, KRING_WRITE );
	if ( r < 0 )
		log_FATAL( "command kring open failed: " << kctrl_error( &cmd, r ) );

	loopBegin();

	while ( true ) {
		poll();

		if ( !loopContinue() )
			break;

		/* Spin. */
		if ( kdata_avail( &kring ) ) {
			/* Load the packet data. */
			struct kdata_packet pkt;
			kdata_next_packet( &kring, &pkt );
			
			pcap_pkthdr hdr;
			hdr.len = pkt.len;
			hdr.caplen = pkt.caplen;

			Packet::Dir dir = pkt.dir == KDATA_DIR_INSIDE ? Packet::Egress : Packet::Ingress;

			/* Process. */
			handler.handler( dir, &hdr, pkt.bytes );

			if ( kdata_skips( &kring ) != 0 ) {
				log_debug( DBG_KRING, "skips: " << kdata_skips( &kring ) );
			}
		}

		int r = kdata_read_wait( &kring );
		if ( r < 0 )
			log_ERROR( "kring recv failed: " << strerror( errno ) );
	}

	return 0;
}

int SniffThread::sniffPcap()
{
	const char *dev = "eth1";
	char errbuf[PCAP_ERRBUF_SIZE];
	#define BUFSIZE 16384

	handler.pcap = pcap_open_live( dev, BUFSIZE, 1, 100, errbuf );
	if ( handler.pcap == NULL ) {
		log_ERROR( "couldn't open device " << dev << ": " << errbuf );
		return 0;
	}

	signalPcapAccess = handler.pcap;
	signal( SIGUSR1, sigusrHandler );

	pcap_set_timeout( handler.pcap, 100 );

	handler.datalink = pcap_datalink( handler.pcap );

	log_debug( DBG_PCAP, "entering packet loop, datalink: " << handler.datalink );

	loopBegin();

	if ( handler.datalink == DLT_EN10MB ) {
		while ( true ) {
			poll();

			if ( !loopContinue() )
				break;

			int rt = pcap_dispatch( handler.pcap, -1, &::handler, (u_char*)this );
			if ( rt < 0 )
				break;
		}
	}

	signalPcapAccess = 0;

	pcap_close( handler.pcap );
	log_message( "exiting" );

	return 0;
}

int SniffThread::main()
{
	int ret;
	switch ( type ) {
		case Net:
			ret = sniffKring();
			break;
		case Decrypted:
			ret = sniffDecrypted();
			break;
	}
	return ret;
}
