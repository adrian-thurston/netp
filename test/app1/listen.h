#ifndef _LISTEN_H
#define _LISTEN_H

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/ssl.h>

#include "listen_gen.h"
#include <aapl/dlistval.h>
#include <aapl/avlmap.h>
#include <list>

struct ListenThread
	: public ListenGen
{
	ListenThread()
	{
		recvRequiresSignal = true;
	}

	int main();

	void handleTimer();
	void recvShutdown( Shutdown *msg );

	void notifyAccept( PacketConnection *pc );
};

#endif /* _LISTEN_H */
