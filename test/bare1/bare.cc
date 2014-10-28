#include "bare.h"
#include "itq_gen.h"

void BareThread::recvShutdown( Shutdown *msg )
{
	log_message( "received shutdown" );
	breakRecv = true;
}

int BareThread::main()
{
	recv();
	log_message( "exiting" );
	return 0;
}
