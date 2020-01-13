#ifndef _SERVICE_H
#define _SERVICE_H

#include "service_gen.h"

#include <aapl/astring.h>

struct ServiceThread
	: public ServiceGen
{
	ServiceThread()
	{
		/* Waiting on packets from the OS. Need to be interrupted. */
		recvRequiresSignal = true;
	}

	virtual void recvShutdown( Message::Shutdown *msg );
	virtual void recvPassthru( Message::PacketPassthru *msg );

	PacketConnection *brokerConn;

	int main();
};

#endif /* _SERVICE_H */
