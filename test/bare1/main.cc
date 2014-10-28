#include "main.h"
#include "bare.h"
#include "itq_gen.h"

#include <unistd.h>

int MainThread::main()
{
	BareThread *bare = new BareThread;
	create( bare );

	SendsToBare *sendsToBare = registerSendsToBare( bare );

	mainSignal();

	Shutdown *shutdown = sendsToBare->openShutdown();
	sendsToBare->send();

	join();


	log_message( "exiting" );

	return 0;
}
