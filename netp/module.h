#ifndef NETP_MODULE_H
#define NETP_MODULE_H

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <aapl/dlist.h>

struct NetpConfigure;
struct Context;
struct LookupSet;
struct SelectFd;
struct Recv;
struct FetchList;
struct Thread;
struct PacketConnection;

typedef void (*ModuleLoadProxyHostNames)( LookupSet *lookupSet );
typedef void (*ModuleSniffConfigureContext)( NetpConfigure *npc, Context *ctx );
typedef void (*ModuleProxyConfigureContext)( NetpConfigure *npc, Context *ctx );
typedef void (*ModuleFetchConfigureContext)( NetpConfigure *npc, Context *ctx );

typedef void (*ModuleFetchAllocFetchObjs)( FetchList *fetchList,
		NetpConfigure *npc, Thread *thread, SSL_CTX *sslCtx, PacketConnection *brokerConn );
typedef void (*ModuleBrokerDispatchPacket)( SelectFd *fd, Recv &recv );

struct Module
{
	ModuleLoadProxyHostNames loadProxyHostNames;
	ModuleSniffConfigureContext sniffConfigureContext;
	ModuleProxyConfigureContext proxyConfigureContext;
	ModuleFetchConfigureContext fetchConfigureContext;
	ModuleFetchAllocFetchObjs fetchAllocFetchObjs;
	ModuleBrokerDispatchPacket brokerDispatchPacket;
	
	Module *prev, *next;
};

struct ModuleList
{
	void loadModule( const char *fn );

	void loadProxyHostNames( LookupSet *lookupSet );

	void brokerDispatchPacket( SelectFd *fd, Recv &recv );

	void sniffConfigureContext( NetpConfigure *npc, Context *ctx );
	void proxyConfigureContext( NetpConfigure *npc, Context *ctx );

	void fetchConfigureContext( NetpConfigure *npc, Context *ctx );
	void fetchAllocFetchObjs( FetchList *fetchList, NetpConfigure *npc,
			Thread *thread, SSL_CTX *sslCtx, PacketConnection *brokerConn );

	DList<Module> moduleList;
};

extern "C" {

	void loadProxyHostNames( LookupSet *lookupSet );

	void brokerDispatchPacket( SelectFd *fd, Recv &recv );

	void sniffConfigureContext( NetpConfigure *npc, Context *ctx );
	void proxyConfigureContext( NetpConfigure *npc, Context *ctx );

	void fetchConfigureContext( NetpConfigure *npc, Context *ctx );
	void fetchAllocFetchObjs( FetchList *fetchList, NetpConfigure *npc,
			Thread *thread, SSL_CTX *sslCtx, PacketConnection *brokerConn );
}

extern ModuleList moduleList;

#endif
