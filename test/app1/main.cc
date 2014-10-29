#include "main.h"
#include "user.h"
#include "itq_gen.h"

#include <unistd.h>

int MainThread::main()
{
	UserThread *bare = new UserThread;
	create( bare );

	SendsToUser *sendsToUser = registerSendsToUser( bare );

	mainSignal();

	Shutdown *shutdown = sendsToUser->openShutdown();
	sendsToUser->send();

	join();

	log_message( "exiting" );

	return 0;
}
