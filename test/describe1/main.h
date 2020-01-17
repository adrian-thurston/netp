#ifndef _MAIN_H
#define _MAIN_H

#include "genf.h"
#include "main_gen.h"

#include <aapl/avlmap.h>
#include <aapl/vector.h>
#include <aapl/astring.h>
#include <aapl/compare.h>

#include <iostream>

struct MainThread
:
	public MainGen
{
	int main();

	PacketConnection *brokerConn;

	virtual void handleTimer();
};

#endif
