#ifndef _MAIN_H
#define _MAIN_H

#include "main_gen.h"

struct MainThread
	: public MainGen
{
	void recvBigPacket( SelectFd *fd, BigPacket *pkt );
	int main();
};

#endif
