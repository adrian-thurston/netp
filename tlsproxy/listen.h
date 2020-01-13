#ifndef _LISTEN_H
#define _LISTEN_H

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/ssl.h>

#include "listen_gen.h"
#include <aapl/dlistval.h>
#include <aapl/avlmap.h>
#include <list>

struct ServiceThread;
struct ProxyThread;

int sslServerNameCallback( SSL *ssl, int *al, void * );

struct ContextMap
{
	typedef DListVal<EVP_PKEY*> KeyList;

	ContextMap();

	void close();

	SSL_CTX *serverCtx( ProxyThread *proxyThread, std::string host );

	pthread_mutex_t mutex;

	int keysAvail();
	void addKeys( int amt );

	typedef AvlMap<std::string, SSL_CTX*> CtxMap;

	CtxMap ctxMap;
	KeyList keyList;

	EVP_PKEY *capkey;
	X509 *cacert;
	BIGNUM *serial;
	int nextKey;

	long connId();
	long nextConId;
};

struct ListenThread
	: public ListenGen
{
	ListenThread( ServiceThread *service )
	:
		service(service)
	{
		recvRequiresSignal = true;
	}

	ContextMap contextMap;

	ServiceThread *service;

	SSL_CTX *serverCtx;
	SSL_CTX *clientCtx;

	std::list<SendsToProxy*> proxySends;

	/* For network namespace enter/leave. */
	int origNsFd, enterNsFd;

	int main();
	void recvShutdown( Message::Shutdown *msg );

	void enterInlineNamespace();
	void leaveInlineNamespace();
};

#endif /* _LISTEN_H */
