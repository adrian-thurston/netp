#include "main.h"
#include "bare.h"

int MainThread::main()
{
	log_message( "main" );
	BareThread *bare = new BareThread;
	create( bare );

	while ( true ) {}
	return 0;
}
