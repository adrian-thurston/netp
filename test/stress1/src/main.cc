#include "main.h"
#include "reader.h"
#include "writer.h"

#define READERS 1
#define WRITERS 1

int MainThread::main()
{
	ReaderThread *readers[READERS];
	WriterThread *writers[WRITERS];
	SendsToReader *sendsTos[READERS];

	system( "rmmod kring" );
	system( "insmod $HOME/devel/kring/src/kring.ko" );
	system( "echo stress1 4 4 > /sys/kring/add" );

	for ( int i = 0; i < READERS; i++ )
		readers[i] = new ReaderThread();

	for ( int i = 0; i < WRITERS; i++ )
		writers[i] = new WriterThread();

	for ( int i = 0; i < READERS; i++ )
		sendsTos[i] = registerSendsToReader( readers[i] );

	for ( int i = 0; i < READERS; i++ )
		create( readers[i] );

	sleep( 1 );

	for ( int i = 0; i < WRITERS; i++ )
		create( writers[i] );

	sleep( 1 );

	for ( int i = 0; i < READERS; i++ ) {
		sendsTos[i]->openShutdown();
		sendsTos[i]->send( true );
	}

	join();

	log_message( "main exiting" );
	return 0;
}
