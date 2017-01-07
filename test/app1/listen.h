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

void notifAccept( SelectFd *selectFd );
void writeReady( SelectFd *fd );

	virtual void accept( int fd );
	virtual void selectFdReady( SelectFd *fd, uint8_t readyMask );
};

#endif /* _LISTEN_H */
