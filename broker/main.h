#ifndef _MAIN_H
#define _MAIN_H

#include "genf.h"
#include "main_gen.h"

#include <aapl/dlist.h>
#include <aapl/bstset.h>
#include <aapl/astring.h>
#include <aapl/avlmap.h>
#include <string>

struct Struct;
struct MainThread;

/* The set of messages a client wants to receive. */
typedef BstSet<long> WantIdSet;

/*
 * Tracking most recently received messages so we can retransmit when a new
 * client connects.
 */

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

/*
 * Recording the structure of messages for writing to databases.
 */

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
	std::string name;
	int ID;
	DList<Field> fieldList;

	Struct *prev, *next;
};

typedef DList<Struct> StructList;
typedef AvlMap<int, Struct*> StructMap;
typedef AvlMapEl<int, Struct*> StructMapEl;

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

	void stashBool( std::ostream &post, char &sep, Field *f, Recv &recv );
	void stashInt( std::ostream &post, char &sep, Field *f, Recv &recv );
	void stashUnsignedInt( std::ostream &post, char &sep, Field *f, Recv &recv );
	void stashLong( std::ostream &post, char &sep, Field *f, Recv &recv );
	void stashUnsignedLong( std::ostream &post, char &sep, Field *f, Recv &recv );
	void stashString( std::ostream &post, char &sep, Field *f, Recv &recv );
	void stashChar( std::ostream &post, char &sep, Field *f, Recv &recv );
	void stashInflux( Struct *_struct, Recv &recv );

	virtual void checkOptions();

	BrokerConnectionList connList;
	LastList lastList;
	StructMap structMap;
};

#endif
