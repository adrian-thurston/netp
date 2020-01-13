#ifndef _NETP_NETP_H
#define _NETP_NETP_H

#include <aapl/vector.h>
#include <aapl/avlmap.h>
#include <aapl/rope.h>

#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>

#include <pcap.h>
#include <stdint.h>
#include <aapl/avlset.h>
#include <aapl/astring.h>
#include <aapl/vector.h>
#include <aapl/avlmap.h>
#include <aapl/astring.h>
#include <aapl/dlist.h>
#include <zlib.h>
#include <map>
#include <brotli/decode.h>
#include <genf/thread.h>

#include "packet.h"
#include "json.h"

struct Conn;
struct Decrypted;
struct Half;
struct Consumer;
struct VPT;
struct Context;

typedef unsigned char wire_t;

struct PatNode;
struct Node
{
	enum Type
	{
		Root = 1,
		LangHttp,
		LangJson,
		LangHtml,

		ConnTls,
		ConnHost,

		HttpRequest,
		HttpHead,
		HttpResponse,

		/* Hostname used to establish the connection. Sent as child of HTTP
		 * request. */
		HttpConnHost,

		HttpMethod,
		HttpUri,
		HttpProtocol,
		HttpHeader,
		HttpHeaderName,
		HttpHeaderValue,

		HtmlTag,
		HtmlText,
		HtmlAttr,
		HtmlVal,

		JsonObject,
		JsonField,
		JsonArray,
		JsonString,
		JsonNumber,
		JsonNull,
		JsonFalse,
		JsonTrue,
		JsonNaN,
		JsonInfinity,

		DnsPacket,
		DnsQuestion,
		DnsAnswer,
		DnsAuthority,
		DnsAdditional,
		DnsRrA,
		DnsRrAAAA,
		DnsRrCname,

		DnsName,
		DnsAddr,

		FileTypePe,
		FileTypeUnknown
	};

	Node( Type type ) : type(type), data(0), dlen(0) {}

	Node *parent;
	Type type;
	std::string text;
	const unsigned char *data;
	int dlen;

	DList<Node> children;
	Node *prev, *next;

	/* JSON interface. */
	static const String emptyString;
	static const String zeroString;
	static const Node null;

	const Node *obj(const char *key) const;
	const Node *arr(int i) const;
	const String string() const
		{ return type == JsonString ? String(text.c_str()) : emptyString; }
	const String number() const
		{ return type == JsonNumber ? String(text.c_str()) : emptyString; }
};

const char *nodeText( Node::Type type );

struct PatState
{
	PatState( PatNode *node, PatNode *nextChild )
		: node(node), nextChild(nextChild) {}

	PatNode *node;
	PatNode *nextChild;
};

enum MatchResult
{
	MatchContinue,
	MatchFinished,
	MatchFail
};

struct OnMatch
{
	virtual MatchResult match( VPT *vpt, Node *node ) = 0;
};

struct StackNode
{
	StackNode( Node *node )
	:
		node(node),
		consumer(0)
	{}

	Node *node;
	Vector<PatState> active;
	Consumer *consumer;

	Vector<OnMatch*> onMatchList;

	StackNode *prev, *next;
};

/* The request parser maintains a list of responses that it can consume. This
 * way we can pass patterns and data to the matching response parser.  */
struct NodePair
{
	NodePair() : patTransfer(0) {}

	int id;
	PatNode *patTransfer;
	NodePair *prev, *next;
};

struct VPT
{
	VPT();

public:
	void init();

	Node *push( Node::Type type );
	bool pop( Node::Type type );
	void setText( std::string text );
	void setData( const unsigned char *data, int dlen );
	void appendText( Node::Type type, std::string text );
	void appendChar( Node::Type type, char c );
	void setConsumer( Consumer *consumer );
	Consumer *getConsumer();
	void addPat( PatNode *pat ) { patRoots.append( pat ); }
	int depth() { return vs.length(); }

	Vector<OnMatch*> getOnMatch();

	Node *up( int level );

	Vector<PatNode*> patRoots;

	/* Maintained by packet handler. */
	Packet *packet;

	/* Was the push/pop clean. Can look at this to determine if we want to dump
	 * any connection cache for debugging purposes. */
	bool vptErrorOcccurred;

	/* Stack of push counts. This is used to contain errors to differernt
	 * regions. We can stop too many pops, and clear extra pushes at each
	 * barrier. For example if there is some bad HTML it's stack can be cleaned
	 * before going back into the HTTP. */
	Vector<int> barriers;
	int topBarrier;
	void pushBarrier();
	void popBarrier();

	void generatePair();
	void consumePair();
	void pairPattern( PatNode *pattern );

	VPT *other;
	NodePair *pairSend;
	DList<NodePair> pairRecv;

private:
	StackNode *root;
	DList<StackNode> vs;
	StackNode *vt;

};

extern void configureContext( Context *ctx );

struct NetpConfigure
{
	NetpConfigure()
	:
		passthruWriter(0),
		stashErrors(false),
		stashAll(false)
	{}

	virtual void configureContext( Context *ctx ) = 0;

	ItWriter *passthruWriter;

	/* Debugging only: stash connection data and if there is an error in
	 * parsing, dump to a file. Do not turn this on in a production
	 * environment. */
	bool stashErrors;
	bool stashAll;
};

/* Parse Context. */
struct Context
{
	Context( NetpConfigure *netpConfigure )
	:
		parseReportFailures( false ),
		parseReportJson( false ),
		parseReportHtml( false ),
		parseReportHttp( false )
	{
		netpConfigure->configureContext( this );
	}

	VPT vpt;

	void addPat( PatNode *pat )
	{
		vpt.addPat( pat );
	}

	bool parseReportFailures;
	bool parseReportJson;
	bool parseReportHtml;
	bool parseReportHttp;
};

struct Consumer
{
	virtual ~Consumer() {}
	virtual void start( Context *ctx ) = 0;
	virtual int receive( Context *ctx, Packet *packet, const wire_t *data, int length ) = 0;
	virtual void finish( Context *ctx ) = 0;
};

struct Splitter
:
	public Consumer
{
	Splitter() : d1(0), d2(0) {}

	void setDest( Consumer *d1, Consumer *d2 )
	{
		this->d1 = d1;
		this->d2 = d2;
	}

	virtual ~Splitter() {}

	virtual void start( Context *ctx )
	{
		d1->start( ctx );
		d2->start( ctx );
	}

	virtual int receive( Context *ctx, Packet *packet, const wire_t *data, int length )
	{
		d1->receive( ctx, packet, data, length );
		return d2->receive( ctx, packet, data, length );
	}

	void finish( Context *ctx )
	{
		d1->finish(ctx);
		d2->finish(ctx);
	}

	Consumer *d1, *d2;
};

struct Identifier
:
	public Consumer
{
	~Identifier() {}

	enum Proto
	{
		Working,
		HTTP_REQ,
		HTTP_RSP,
		Unknown,
	};

	int cs;
	Proto proto;

	void start( Context *ctx );
	int receive( Context *ctx, Packet *packet, const wire_t *data, int length );
	void finish( Context *ctx );
};

struct FileIdent
:
	public Consumer
{
	~FileIdent() {}

	enum FileType
	{
		Working = 1,
		HTML,
		PNG, JPG, GIF,
		BZ, Z, GZ, ZIP,
		PE,

		Unknown,
	};

	static const char *FtStr[];

	int cs;
	FileType fileType;

	int c, nbytes;

	void start( Context *ctx );
	int receive( Context *ctx, Packet *packet, const wire_t *data, int length );
	void finish( Context *ctx );
};

/*
 * Connection Half, represents state for addr1 -> addr2 half of connection.
 */

struct Half
{
	enum State {
		New,
		Established,
	};

	Half( NetpConfigure *netpConfigure, Conn *connection,
			uint32_t addr1, uint32_t addr2, uint16_t port1, uint16_t port2 )
	:
		ctx( netpConfigure ),
		connection(connection),
		addr1(addr1), addr2(addr2),
		port1(port1), port2(port2),
		state(New),
		seq(0),
		parser(0)
	{}

	void swapDir()
	{
		uint32_t tAddr = addr1;
		uint16_t tPort = port1;
		
		addr1 = addr2;
		port1 = port2;

		addr2 = tAddr;
		port2 = tPort;
	}

	Context ctx;

	Conn *connection;

	uint32_t addr1, addr2;
	uint16_t port1, port2;

	State state;

	/* Sent sequence number. This is the number initially chosen and sent by
	 * addr1, then acked by addr2. */
	uint32_t seq;

	Identifier identifier;
	Consumer *parser;
};

struct Conn 
{
	Conn( NetpConfigure *netpConfigure,
			uint32_t addr1, uint32_t addr2, uint16_t port1, uint16_t port2 )
	:
		h1( netpConfigure, this, addr1, addr2, port1, port2 ),
		h2( netpConfigure, this, addr2, addr1, port2, port1 ),
		proto( Working ),
		stashErrors( netpConfigure->stashErrors ),
		stashAll( netpConfigure->stashAll )
	{}

	Conn( NetpConfigure *netpConfigure, Half &key )
	:
		h1( netpConfigure, this, key.addr1, key.addr2, key.port1, key.port2 ),
		h2( netpConfigure, this, key.addr2, key.addr1, key.port2, key.port1 ),
		proto( Working ),
		stashErrors( netpConfigure->stashErrors ),
		stashAll( netpConfigure->stashAll )
	{}

	/* Connections are normally oriented from client (h1) -> server (h2). In
	 * cases where we started monitoring before the connection was established,
	 * we may not be able to figure out which direction initiated the
	 * connection and this relationship may not hold. In that case the dir
	 * field is set to unknown and we assume the connection was oriented
	 * protected -> wild. */
	Half h1, h2;

	Packet::Dir dir;

	enum State
	{
		Syn,
		SynAck,
		Established,
	};

	State state;

	enum Proto
	{
		Working,
		HTTP,
		Unknown,
	};

	Proto proto;

	bool stashErrors;
	bool stashAll;

	void established();
};

struct CmpHalf
{
	static int compare( const Half *h1, const Half *h2 )
	{
		if ( h1->addr1 < h2->addr1 )
			return -1;
		else if ( h1->addr1 > h2->addr1 )
			return 1;
		else if ( h1->addr2 < h2->addr2 )
			return -1;
		else if ( h1->addr2 > h2->addr2 )
			return 1;
		else if ( h1->port1 < h2->port1 )
			return -1;
		else if ( h1->port1 > h2->port1 )
			return 1;
		else if ( h1->port2 < h2->port2 )
			return -1;
		else if ( h1->port2 > h2->port2 )
			return 1;
		return 0;
	}
};

struct DecrHalf
{
	DecrHalf( NetpConfigure *netpConfigure )
	:
		ctx( netpConfigure ),
		parser(0)
	{
		identifier.start( &ctx );
	}

	Context ctx;
	Identifier identifier;
	Consumer *parser;

	Rope cache;
};

struct Decrypted 
{
	Decrypted( NetpConfigure *netpConfigure, long id )
	:
		id( id ),
		h1( netpConfigure ),
		h2( netpConfigure ),
		proto( Conn::Working ),
		stashErrors( netpConfigure->stashErrors ),
		stashAll( netpConfigure->stashAll )
	{}

	Decrypted( NetpConfigure *netpConfigure )
	:
		h1( netpConfigure ),
		h2( netpConfigure ),
		proto( Conn::Working ),
		stashErrors( netpConfigure->stashErrors ),
		stashAll( netpConfigure->stashAll )
	{}

	long id;

	DecrHalf h1;
	DecrHalf h2;

	Conn::Proto proto;

	bool stashErrors;
	bool stashAll;
};

struct CmpDecr
{
	static int compare( const Decrypted *h1, const Decrypted *h2 )
	{
		if ( h1->id < h2->id )
			return -1;
		else if ( h1->id > h2->id )
			return 1;
		return 0;
	}
};

inline uint32_t syn( Packet *packet )
{
	return (uint32_t) packet->tcp.th->syn;
}

inline uint32_t ack( Packet *packet )
{
	return (uint32_t) packet->tcp.th->ack;
}

inline uint32_t synAckBits( Packet *packet )
{
	return ( (uint32_t)packet->tcp.th->syn << 1 ) | (uint32_t)packet->tcp.th->ack;
}

#define SA_BITS_SYN      0x2
#define SA_BITS_SYN_ACK  0x3
#define SA_BITS_ACK      0x1
#define SA_BITS_NONE     0x0

typedef AvlSet< Half*, CmpHalf > HalfDict;
typedef AvlSetEl< Half* > HalfDictEl;

typedef AvlMap< long, Decrypted* > DecrDict;
typedef AvlMapEl< long, Decrypted* > DecrDictEl;

struct LookupSet
:
	public AvlSet<const char *, CmpStr>
{
};

struct IpSet
:
	public AvlSet<uint32_t>
{
};

struct NameCollect
{
	String question;
	Vector<String> A;

	void reset()
	{
		question.empty();
		A.empty();
	}
};

struct Gzip
:
	public Consumer
{
	Gzip();

	void destination( Consumer *parser )
		{ this->output = parser; }

	void logResult( int r );

	void decompressBuf( Context *ctx, Packet *packet, char *buf, int len );

	virtual void start( Context *ctx );
	virtual int receive( Context *ctx, Packet *packet, const wire_t *data, int length )
		{ decompressBuf( ctx, packet, (char *)data, length ); return 0; }
	virtual void finish( Context *ctx );

	Consumer *output;
	uLongf destLen;
	Bytef *dest;

	int uncompress2( Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen );

    z_stream stream;
};

struct Brotli
:
	public Consumer
{
	Brotli();

	void destination( Consumer *parser )
		{ this->output = parser; }
//
//	void logResult( int r );
//
//	void decompressBuf( Packet *packet, char *buf, int len );
//
	virtual void start( Context *ctx );
	virtual int receive( Context *ctx, Packet *packet, const wire_t *data, int length );
	virtual void finish( Context *ctx );

	size_t destLen;
	uint8_t *dest;

//	int uncompress2( Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen );
//
//	z_stream stream;

	Consumer *output;
	BrotliDecoderState *stream;
};


/* When parse a region and only need to pass it through unchanged, but don't
 * want to execute char by-char. Allows us to setup a destination, set
 * start/end markers, and execute on a block we are partially through at the
 * level of the driver of whatever we are parsing (IE the end of the exec
 * function). */
struct BlockExec
{
	BlockExec()
	:
		open(false),
		output(0)
	{}

	bool open;
	Consumer *output;

	const wire_t *from;

	void destination( Consumer *parser )
		{ this->output = parser; }

	void finish( Context *ctx );

	void start( Context *ctx, Packet *packet, const wire_t *data );
	void finish( Context *ctx, Packet *packet, const wire_t *data );
	void pause( Context *ctx, Packet *packet, const wire_t *data );
	void resume( Context *ctx, Packet *packet, const wire_t *data );

	void preExec( Context *ctx, Packet *packet, const wire_t *data );
	void postExec( Context *ctx, Packet *packet, const wire_t *data );
};

struct MainThread;
struct SelectFd;

struct Source
{
	virtual Consumer *nextConsumer() = 0;
	virtual void responseComplete() = 0;
	virtual void preFinish() = 0;
};

struct ParseReport
{
	ParseReport() : size(0), errorReported(false) {}

	Rope rope;
	std::string conn;
	long size;
	static int id;
	bool errorReported;

	void addText( const wire_t *data, int len );
	void dumpText();
	void dumpRoot( std::ostream &out, Node *node );
	void dumpTree( std::ostream &out, Node *root );

	void start();
	void receive( Packet *packet, const wire_t *data, int len );
	void finish( Node *root );
	void error( Node *root, int offset, int pc );
	void errorReport( std::ostream &out, Node *root, int offset, int pc );
	void successReport( std::ostream &out, Node *root );

	std::ofstream *open();
};

struct JsonParser
:
	public Consumer
{
	int cs, top, sd;
	int *stack;
	bool dot, E;
	Vector<char> buf;

	virtual void start( Context *ctx );
	virtual int receive( Context *ctx, Packet *packet, const wire_t *data, int len );
	virtual void finish( Context *ctx );

	ParseReport parseReport;
	Node *localRoot;
};

struct HtmlParser
:
	public Consumer
{
	Vector<char> tag;
	Vector<char> key;
	Vector<char> val;
	Vector<char> content;
	Vector<char> attr_buf;
	bool noEndTag;
	bool closeAttr;

	int cs;

	virtual void start( Context *ctx );
	virtual int receive( Context *ctx, Packet *packet, const wire_t *data, int len );
	virtual void finish( Context *ctx );

	ParseReport parseReport;
	Node *localRoot;
};

struct Decompress
:
	public Consumer
{
	Decompress( Consumer *consumer )
	:
		consumer(consumer)
	{}

	int init();

	Vector<char> buf;

	virtual void decompress( Packet *packet, const wire_t *data, int len, bool finish );

	virtual void start();
	virtual int receive( Packet *packet, const wire_t *data, int len );
	virtual void finish();

	Consumer *consumer;
	z_stream stream;
};

struct HttpBodyParser
:
	public Consumer
{
	int cs;

	void start( Context *ctx );
	int receive( Context *ctx, Packet *packet, const wire_t *data, int length );
	void finish( Context *ctx );

	void decidedType( Context *ctx, Packet *packet, FileIdent::FileType type );

	enum ContentType { Unspecified = 1, Other, ApplicationJson };

	FileIdent identifier;
	Consumer *parser;
	ContentType contentType;
};

struct HttpRequestParser;
struct HttpResponseParser;


struct HttpRequestParser
:
	public Consumer
{
	HttpRequestParser( Context *ctx )
	:
		responseParser(0),
		_ctx(ctx)
	{}

	int cs;

	virtual void start( Context *ctx );
	virtual int receive( Context *ctx, Packet *packet, const wire_t *data, int length );
	virtual void finish( Context *ctx ) {}

	Vector<char> buf1, buf2;
	int contentLength;

	BlockExec bodyBlock;
	HttpBodyParser bodyParser;

	HttpResponseParser *responseParser;
	Context *_ctx;
};

struct HttpResponseParser
:
	public Consumer
{

	HttpResponseParser( Context *ctx, Source *source, bool wantCookies, bool wantLocation )
	:
		source(source),
		consumer(0),
		wantCookies(wantCookies),
		wantLocation(wantLocation),
		requestParser(0),
		_ctx(ctx)
	{}

	virtual void start( Context *ctx );
	virtual int receive( Context *ctx, Packet *packet, const wire_t *data, int length );
	virtual void finish( Context *ctx );

	void bodyBlockFinish( Context *ctx, Packet *packet, const wire_t *p );

	Consumer *destination();

	int cs;
	Vector<char> buf;
	Vector<char> buf1, buf2;
	std::string uri;
	String last;
	int contentLength;
	int chunkLength;
	bool isGzipped;
	bool isBrotli;
	bool isChunked;
	bool isConnectionClose;
	int chunks;

	Gzip gzip;
	Brotli brotli;
	BlockExec bodyBlock;
	HttpBodyParser bodyParser;
	Splitter splitter;

	Source *source;
	Consumer *consumer;

	bool wantCookies;
	std::map<std::string, std::string> cookies;

	bool wantLocation;
	String location;

	ParseReport parseReport;
	Node *localRoot;
	HttpRequestParser *requestParser;
	Context *_ctx;
};


struct DnsSection
{
	unsigned long count;
	Node::Type tree;
};

struct DnsParser
{
	DnsParser() {}

	int cs;

	unsigned int names[32];
	int n;
	const unsigned char *packet_start;

	unsigned long id;
	unsigned long options;

	/* Current section type. indexes into sect and rr arrays. */
	int s;

	static const int nsect = 4;
	static const int qd = 0, an = 1;
	static const int ns = 2, ar = 3;

	DnsSection sect[nsect];
	static const char *rr[nsect];

	int copyName( char *nb, int max, const unsigned char *ptr );

	const unsigned char *header( Context *ctx, Packet *packet,
			const unsigned char *p, const unsigned char *pe );
	const unsigned char *question( Context *ctx, Packet *packet,
			const unsigned char *p, const unsigned char *pe );
	const unsigned char *resource_record( Context *ctx, Packet *packet,
			const unsigned char *p, const unsigned char *pe );

	int data( Context *ctx, Packet *packet, const wire_t *data, int length );
};

struct Handler
{
	Handler( NetpConfigure *netpConfigure );

	NetpConfigure *netpConfigure;

	pcap_t *pcap;
	int datalink;

	bpf_program srcProtectedBpf;
	bpf_program dstProtectedBpf;

	HalfDict halfDict;
	DecrDict decrDict;

	bool continuation;
	Packet contPacket;
	Packet decrPacket;

	Context udpCtx;

	bool srcProtected( Packet *packet );
	bool dstProtected( Packet *packet );

	void payload( Packet *packet );
	void flow( Packet *packet );

	void flowSynState( Packet *packet );
	void flowSynAckState( Packet *packet );
	void flowEstabState( Packet *packet );
	void flowData( Packet *packet );

	void createConnection( Packet *packet, Half &key );

	void tcp( Packet *packet );
	void udp( Packet *packet );

	void dns_colm( Packet *packet );

	void handler( Packet::Dir dir, const struct pcap_pkthdr *h, const u_char *bytes );

	void decrypted( long id, int type, const char *host, unsigned char *bytes, int len );

	void RR_A( const char *data, int length );
	void QUESTION_NAME( const char *data, int length );
	void reportName();
	NameCollect nameCollect;

	int main();
};

PatNode *consPatTdAcctSelect();
PatNode *consPatCvpp();
PatNode *consPatHttp();
PatNode *consPatJsonQuotes();
PatNode *consPatJsonFxRate();
PatNode *consPatJsonHoldings();
PatNode *consPatHttpUri1( NetpConfigure *netpConfigure );
PatNode *consPatHttpUri2( NetpConfigure *netpConfigure );

Node *findChild( Node *node, Node::Type type );

#endif
