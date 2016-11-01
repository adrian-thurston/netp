#ifndef _USER_H
#define _USER_H

#include "user_gen.h"

struct UserThread
	: public UserGen
{
	UserThread()
	{
		recvRequiresSignal = true;
	}

	int main();

	void recvShutdown( Shutdown *msg );
	void recvHello( Hello *msg );
};

#endif

