#ifndef __PACKET_H
#define __PACKET_H

struct PacketWriter;

namespace GenF {

	struct Packet
	{
		static void *open( PacketWriter *writer, int ID, int SZ );
		static void send( PacketWriter *writer );
	};

}

#endif
