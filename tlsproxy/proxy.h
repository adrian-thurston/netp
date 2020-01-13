#ifndef _PROXY_H
#define _PROXY_H

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/ssl.h>

#include "proxy_gen.h"
#include "listen_gen.h"
#include "main_gen.h"

#include <aapl/astring.h>
#include <aapl/avlmap.h>
#include <kring/kring.h>
#include <netp/netp.h>

#define EC_SOCKET_CONNECT_FAILED        104
#define EC_SSL_PEER_FAILED_VERIFY       100
#define EC_SSL_CONNECT_FAILED           105
#define EC_SSL_WRONG_HOST               106
#define EC_SSL_CA_CERT_LOAD_FAILURE     107
#define EC_SSL_NEW_CONTEXT_FAILURE      120
#define EC_SSL_CA_CERTS_NOT_SET         121
#define EC_SSL_ACCEPT_FAILED            152
#define EC_SSL_FAILED_TO_LOAD           159
#define EC_SSL_PARAM_NOT_SET            160
#define EC_SSL_CONTEXT_CREATION_FAILED  161
#define EC_NONBLOCKING_IO_NOT_AVAILABLE 162
#define EC_FCNTL_QUERY_FAILED           163
#define EC_WRITE_ERROR                  164
#define EC_DAEMON_DAEMON_TIMEOUT        165
#define EC_SOCKADDR_ERROR               166
#define EC_SOCK_NOT_LOCAL               167
#define EC_CONF_PARSE_ERROR             168
#define EC_WRITE_ON_NULL_SOCKET_BIO     169

#define PEER_CN_NAME_LEN 256

struct ContextMap;
struct ProxyThread;
struct ProxyConnection;

struct WriteBlock
{
	WriteBlock()
		: blocklen( kdata_decrypted_max_data() )
	{
		data = new char[blocklen];
		head = 0;
		tail = 0;
	}

	~WriteBlock()
	{
		memset( data, 0, blocklen );
		delete[] data;
	}

	int blocklen;
	char *data;
	int head, tail;

	WriteBlock *prev, *next;
};

typedef List<WriteBlock> WriteBlockList;

struct FdDesc
{
	enum Type { Server = 1, Client };

	FdDesc( long connId, Type type, ProxyConnection *proxyConn )
	:
		connId(connId),
		type(type),
		proxyConn(proxyConn),
		other(0),
		stop(false),
		connected(false),
		haveHostname(false)
	{
	}

	long connId;
	Type type;

	ProxyConnection *proxyConn;
	FdDesc *other;

	/* Input movement. */
	WriteBlockList wbList;

	bool stop;

	bool connected;
	bool haveHostname;
	std::string hostname;


	static void connect( FdDesc *serverDesc, FdDesc *clientDesc )
	{
		clientDesc->other = serverDesc;
		serverDesc->other = clientDesc;
	}

	int readAvail();
	int addSpace();
	char *readTo();
	void readIn( int amt );

	int writeAvail();
	char *writeFrom();
	void consume( int amt );
};

/* Logging. */
std::ostream &operator<<( std::ostream &out, FdDesc *fdDesc );

struct ProxyConnection
	: public Connection
{
	ProxyConnection( ProxyThread *proxyThread, int connId, FdDesc::Type type );

	ProxyThread *proxyThread;
	FdDesc *fdDesc;

	virtual void connectComplete();
	virtual void readReady();
	virtual void writeReady();
	virtual void failure( FailType failType );
	virtual void notifyAccept();
};

struct ProxyListener
:    
	public Listener
{   
	ProxyListener( ProxyThread *proxyThread );

	ProxyThread *proxyThread;

	virtual ProxyConnection *connectionFactory( int fd );
};


struct ProxyThread
:
	public ProxyGen,
	public NetpConfigure

{
	ProxyThread( SSL_CTX *servetCtx, SSL_CTX *clientCtx,
			ContextMap *contextMap, int listenFd, int acceptFd, int ringId )
	:
		serverCtx( servetCtx ),
		clientCtx( clientCtx ),
		contextMap( contextMap ),
		listenFd( listenFd ),
		acceptFd( acceptFd ),
		ringId( ringId ),
		receivedCtx(0),
		handler( this )
	{
		recvRequiresSignal = true;

		/* Make it possible to turn on --stash-errors */
		NetpConfigure::stashErrors = MainGen::stashErrors;
		NetpConfigure::stashAll = MainGen::stashAll;
	}

	SSL_CTX *serverCtx, *clientCtx;
	ContextMap *contextMap;

	int listenFd;
	int acceptFd;
	int ringId;

	bool capture;

	SendsToService *sendsToService;

	SSL_CTX *receivedCtx;

	/* Configuration function */	
	virtual void configureContext( Context *ctx );

	void serverName( SelectFd *selectFd, const char *host );

	void recvShutdown( Message::Shutdown *msg );

	void connectComplete( SelectFd *fd );

	int proxyWrite( FdDesc *fdDesc );
	void writeRetry( SelectFd *fd );

	bool sslReadReady( SelectFd *fd );

	void proxyShutdown( SelectFd *fd );

	static FdDesc *fdLocal( SelectFd *fd )
	{
		return static_cast<ProxyConnection*>( fd->local )->fdDesc;
	}

	int main();

	struct kring_user kring;
	void kring_write( FdDesc::Type type, const char *remoteHost, char *data, int len );

	void maybeStartTlsClient( FdDesc *serverDesc );

	void sslPeerFailedVerify( SelectFd *fd );

	Handler handler;
	int kdata( long id, int type, const char *remoteHost, char *data, int len );

	SendsPassthru *sendsPassthru;
};

#endif /* _PROXY_H */
