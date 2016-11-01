#include "main.h"
#include "user.h"
#include "itq_gen.h"

#include <unistd.h>

int MainThread::main()
{
	UserThread *bare = new UserThread;

	SendsToUser *sendsToUser = registerSendsToUser( bare );

	sendsToUser->openHello();
	sendsToUser->send();

	create( bare );

	signalLoop();

	sendsToUser->openShutdown();
	sendsToUser->send();

	join();

	log_message( "exiting" );

	return 0;
}
