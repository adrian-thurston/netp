#ifndef _MAIN_H
#define _MAIN_H

#include "main_gen.h"

extern const char data1[];
extern const char data2[];
extern const char data3[];

extern const long l1;
extern const long l2;
extern const long l3;

struct MainThread
	: public MainGen
{
	void handleTimer();
	void recvBigPacket( SelectFd *fd, BigPacket *pkt );
	int main();
};

#endif
