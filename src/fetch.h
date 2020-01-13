#ifndef NETP_FETCH_H
#define NETP_FETCH_H

#include "netp.h"

struct PacketConnection;

struct Request
{
	Request( int id, Consumer *consumer, std::string content )
		:
			consumer(consumer),
			content(content)
	{}

	Consumer *consumer;
	std::string content;
	Request *prev, *next;

	Vector<char> rqbf;
};

typedef DList<Request> RequestList;

struct Failure
	: public OnMatch
{
	virtual MatchResult match( VPT *vpt, Node *node )
	{
		log_message( "failure" );
		return MatchFail;
	}
};

struct FetchConnection
:
	public Connection,
	public Source
{
    FetchConnection( NetpConfigure *netpConfigure, Thread *thread, SSL_CTX *sslCtx, int throttleDelay )
	:
		Connection( thread ),
		state( Disconnected ),
		requestParser( &requestContext ),
		responseParser( &responseContext, this, true, true ),
		sslCtx( sslCtx ),
		throttleDelay( throttleDelay ),
		requestContext( netpConfigure ),
		responseContext( netpConfigure )
	{
	}

	enum State { Disconnected = 1, Connecting, Sent, Idle };

	State state;
	time_t lastSend;
	HttpRequestParser requestParser;
	HttpResponseParser responseParser;
	SSL_CTX *sslCtx;
	int throttleDelay;
	Context requestContext;
	Context responseContext;
	std::string fetchHost;

	void disconnect();

	void getGoing( const char *host );

	virtual void connectComplete();
	virtual void notifyAccept() {}
	virtual void failure( FailType failType );
	virtual void readReady();
	virtual void writeReady() {}

	void queueRequest( Request *request );

	virtual void dataAvail( char *bytes, int nbytes );

	RequestList requestList;

	virtual Consumer *nextConsumer();
	virtual void preFinish();
	virtual void responseComplete();

	void maybeSend();
	void timer();
};

struct Fetch
{
	Fetch( String host )
	:
		host(host),
		closed( false )
	{
	}

	virtual void timer() = 0;
	virtual void dispatchPacket( SelectFd *fd, Recv &recv ) = 0;
	virtual void wantIds( PacketConnection *pc ) {}

	String host;
	bool closed;

	MainThread *main;

	Fetch *prev, *next;
};

struct FetchList
:
	public DList<Fetch>
{

};


#endif
