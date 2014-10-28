#include "bare.h"
#include "itq_gen.h"

void BareThread::recvHello( Hello *msg )
{
	static int i = 1;
	log_message( "recv: " << msg->s << " - " << msg->l );
	if ( i++ == 5 )
		breakRecv = true;
}

int BareThread::main()
{
	recv();
	log_message( "exiting" );
	return 0;
}
