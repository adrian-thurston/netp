#include "user.h"
#include "itq_gen.h"

void UserThread::recvHello( Hello *msg )
{
	log_message( "received hello" );
}

void UserThread::recvShutdown( Shutdown *msg )
{
	log_message( "received shutdown" );
	breakLoop();
}

int UserThread::main()
{
	selectLoop();
	log_message( "exiting" );
	return 0;
}
