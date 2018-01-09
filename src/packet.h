#ifndef __PACKET_H
#define __PACKET_H

#include <aapl/rope.h>

struct PacketWriter;

namespace GenF {

	struct Packet
	{
		static void *open( PacketWriter *writer, int ID, int SZ );
		static void send( PacketWriter *writer );
		static void send( PacketWriter *writer, Rope &blocks, bool canConsume );
	};
}

#endif

