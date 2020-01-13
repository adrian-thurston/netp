#ifndef _SERVICE_H
#define _SERVICE_H

#include "service_gen.h"
#include <aapl/astring.h>
#include <aapl/avlset.h>

struct ServiceThread
	: public ServiceGen
{
	ServiceThread()
	{
		// recvRequiresSignal = true;
	}

	virtual void recvPassthru( Message::PacketPassthru *msg );
	virtual void recvCertGenerated( Message::CertGenerated *msg );
	virtual void recvShutdown( Message::Shutdown *msg );
	virtual void handleTimer();

	String request;

	typedef AvlSet<uint32_t> IpSet;

	IpSet ipsAdded;
	PacketConnection *brokerConn;

	int main();
	void accept( int fd );
};

#endif
