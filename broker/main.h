#ifndef _MAIN_H
#define _MAIN_H

#include "genf.h"
#include "main_gen.h"

#include <aapl/dlist.h>
#include <aapl/bstset.h>
#include <aapl/astring.h>
#include <string>

struct MainThread;

typedef BstSet<long> WantIdSet;

struct ClientConnection
:
	public PacketConnection
{
	ClientConnection( MainThread *thread );

	MainThread *mainThread;

	WantIdSet wantIds;

	virtual void failure( FailType failType );
	virtual void notifyAccept();
	virtual void packetClosed();

	ClientConnection *prev, *next;

	bool brokerConnClosed;
};

typedef DList<ClientConnection> BrokerConnectionList;

struct BrokerListener
:   
	public PacketListener
{
	BrokerListener( MainThread *thread );

	MainThread *mainThread;

	virtual ClientConnection *connectionFactory( int fd );
};

typedef DList<ClientConnection> BrokerConnectionList;

struct Last
{
	Last();
	~Last();

	uint32_t msgId;
	int retain;
	Rope *msg;
	int dest;
	int have;

	Last *prev, *next;
};

typedef DList<Last> LastList;

struct Struct;
struct Field
{
	std::string name;
	int type;
	int size;
	int offset;
	Struct *listOf;

	Field *prev, *next;
};

struct Struct
{
	int ID;
	DList<Field> fieldList;

	Struct *prev, *next;
};

typedef DList<Struct> StructList;

struct MainThread
	: public MainGen
{
	void resendPacket( SelectFd *fd, Recv &recv );
	virtual void dispatchPacket( SelectFd *fd, Recv &recv );

	void runReplay();
	void handleTimer();
	int main();
	int service();

	ClientConnection *client;
	ClientConnection *attached;

	ClientConnection *attachToFile( int fd );
	void recvWantId( SelectFd *fd, Record::WantId *pkt );
	void recvPing( SelectFd *fd, Record::Ping *pkt );
	void recvSetRetain( SelectFd *fd, Record::SetRetain *pkt );
	void recvPacketType( SelectFd *fd, Record::PacketType *pkt );

	virtual void checkOptions();

	BrokerConnectionList connList;
	LastList lastList;
	StructList structList;
};

#endif
