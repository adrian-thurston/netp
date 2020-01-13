#ifndef _MAIN_H
#define _MAIN_H

#include "genf.h"
#include "main_gen.h"

#include <aapl/avlmap.h>
#include <aapl/vector.h>
#include <aapl/astring.h>
#include <aapl/compare.h>

#include <netp/fetch.h>

#include <iostream>

struct MainThread
:
	public MainGen,
	public NetpConfigure
{
	int main();

	FetchList fetchList;

	PacketConnection *brokerConn;
	PacketListener *commandListener;

	virtual void handleTimer();

	virtual void dispatchPacket( SelectFd *fd, Recv &recv );
	virtual void configureContext( Context *ctx );
};

#endif
