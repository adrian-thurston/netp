#include "main.h"
#include "bare.h"

#include <unistd.h>

int MainThread::main()
{
	log_message( "main" );

	BareThread *bare = new BareThread;
	create( bare );

	ItWriter *writer = bare->control.registerWriter( this );

	int i = 10;
	while ( i > 0 ) {
		sleep( 1 );

		ItHeader *header = bare->control.startMessage( writer );
		bare->control.send( header );

		i -= 1;
	}

	return 0;
}
