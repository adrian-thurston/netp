#ifndef _USER_H
#define _USER_H

#include "user_gen.h"

struct UserThread
	: public UserGen
{
	UserThread();

	int main();

	void *cb( int status, int timeouts, unsigned char *abuf, int alen );

	void recvShutdown( Shutdown *msg );
	void recvHello( Hello *msg );
};

#endif

