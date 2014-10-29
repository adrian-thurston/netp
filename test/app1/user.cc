#include "user.h"
#include "itq_gen.h"

void UserThread::recvShutdown( Shutdown *msg )
{
	log_message( "received shutdown" );
	breakRecv = true;
}

int UserThread::main()
{
	recv();
	log_message( "exiting" );
	return 0;
}
