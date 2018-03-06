#ifndef __PACKET_H
#define __PACKET_H

#include <aapl/rope.h>
#include "thread.h"

struct PacketHeader
{
	uint32_t firstLen;
	uint32_t totalLen;
	uint32_t msgId;
};

typedef uint32_t PacketBlockHeader;

struct Recv
{
	Recv()
	:
		state(WantHead),
		have(0)
	{}

	enum State
	{
		WantHead = 1,
		WantBlock
	};

	State state;
	PacketHeader *head;
	Rope buf;
	char *data;

	char headBuf[sizeof(PacketBlockHeader)+sizeof(PacketHeader)];

	/* If read fails in the middle of something we need, amount we have is
	 * recorded here. */
	int have;
	int need;
};


struct PacketWriter;
struct ItQueue;

struct PacketBase
{
	static void *open( PacketWriter *writer, int ID, int SZ );

	/* Send a received passthrough out on a connection. */
	static void send( PacketWriter *writer, ItQueue *queue );

	static void send( PacketWriter *writer );
	static void send( PacketWriter *writer, Rope &blocks, bool canConsume );
};

struct PacketConnection
	: public Connection
{
	PacketConnection( Thread *thread )
		: Connection( thread ) { tlsConnect = false; }

	virtual void failure( FailType failType ) {}
	virtual void connectComplete() {}
	virtual void notifyAccept()
		{ selectFd->wantReadSet( true ); }
	virtual void readReady();
	virtual void writeReady();

	virtual void packetClosed() {}

	Recv recv;
	Rope queue;

	void parsePacket( SelectFd *fd );
};

struct PacketListener
:
	public Listener
{
	PacketListener( Thread *thread )
		: Listener( thread ) {}

	virtual Connection *connectionFactory( int fd )
		{ return new PacketConnection( thread ); }
};

struct PacketWriter
{
	PacketWriter( PacketConnection *pc )
	:
		pc(pc),
		itw(0)
	{}

	PacketWriter( ItWriter *itWriter )
	:
		pc(0),
		itw(itWriter)
	{}

	/* One of two places we can send packets. */
	PacketConnection *pc;
	ItWriter *itw;

	PacketHeader *toSend;
	void *content;
	Rope buf;
	Message::PacketPassthru *pp;

	bool usingItWriter() { return itw != 0; }

	char *allocBytes( int nb, uint32_t &offset );

	int length()
		{ return buf.length(); }

	void reset()
		{ buf.empty(); pp = 0;}
};


#endif

