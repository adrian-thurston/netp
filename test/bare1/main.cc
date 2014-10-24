#include "main.h"
#include "bare.h"

int MainThread::main()
{
	log_message( "main" );
	BareThread *bare = new BareThread;
	bare->create();
	while ( true ) {}
}
