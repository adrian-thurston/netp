#include "main.h"
#include "bare.h"
#include "itq_gen.h"

#include <unistd.h>

int MainThread::main()
{
	BareThread *bare = new BareThread;
	create( bare );

	SendsToBare *sendsToBare = registerSendsToBare( bare );

	int i = 5;
	while ( i > 0 ) {
		sleep( 1 );

		static long l = 1;

		Hello *hello = sendsToBare->openHello();
		hello->b = true;
		hello->l = l++;
		hello->s = "hello";
		bare->control.send( sendsToBare->writer );

		i -= 1;
	}

	join();

	mainSignal();

	log_message( "exiting" );

	return 0;
}
