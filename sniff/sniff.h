#ifndef _SNIFF_H
#define _SNIFF_H

#include <kring/kring.h>
#include <parse/parse.h>

#include "sniff_gen.h"
#include "packet.h"

struct SniffThread
:
	public SniffGen,
	public NetpConfigure
{
	enum Type {
		Net = 1,
		Decrypted
	};

	SniffThread( const char *ring, Type type )
		: ring(ring), type(type), handler(this)
	{
		recvRequiresSignal = true;
	}

	int main();

	void recvShutdown( Message::Shutdown *msg );

	SendsPassthru *sendsPassthru;
	void matchedDns( Packet *packet, char *toa );

	virtual void configureContext( Context *ctx );

	void compileBpf();
	int sniffDecrypted();
	int sniffPcap();
	int sniffKring();
	const char *ring;
	Type type;
	Handler handler;

	struct kring_user kring;
	struct kring_user cmd;
};

#endif
