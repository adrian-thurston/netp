#include "main.h"
#include "bare.h"

int MainThread::main()
{
	log_message( "main" );
	BareThread *bare = new BareThread;
	registerChild( bare );
	bare->create();
	while ( true ) {}
	return 0;
}
