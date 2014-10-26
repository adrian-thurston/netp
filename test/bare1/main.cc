#include "main.h"
#include "bare.h"

#include <unistd.h>

int MainThread::main()
{
	log_message( "main" );

	BareThread *bare = new BareThread;
	create( bare );

	bare->control.registerWriter( this );

	int i = 10;
	while ( i > 0 ) {
		sleep( 1 );

		bare->control.notify();
		log_message( "main beat" );
		i -= 1;
	}

	return 0;
}
