#ifndef __PACKET_H
#define __PACKET_H

#include <aapl/rope.h>

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

#endif

