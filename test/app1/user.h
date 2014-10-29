#ifndef _USER_H
#define _USER_H

#include "user_gen.h"

struct UserThread
	: public UserGen
{
	int main();

	void recvShutdown( Shutdown *msg );
};

#endif

