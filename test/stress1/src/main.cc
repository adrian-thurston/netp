#include "main.h"
#include "reader.h"
#include "writer.h"

#define READERS 1

MainThread::MainThread()
:
	entered(0)
{

}

void MainThread::recvEntered( Entered * )
{
	entered += 1;
}

int MainThread::main()
{
	ReaderThread *readers[READERS];
	WriterThread *writers[WRITERS];
	// SendsToReader *sendsTos[READERS];

	system( "rmmod kring" );
	system( "insmod $HOME/devel/kring/src/kring.ko" );
	system( "echo stress1 4 4 > /sys/kring/add" );

	for ( int i = 0; i < READERS; i++ )
		readers[i] = new ReaderThread();

	for ( int i = 0; i < READERS; i++ ) {
		readers[i]->sendsToMain = readers[i]->registerSendsToMain( this );
		/* sendsTos[i] = */ registerSendsToReader( readers[i] );
	}

	for ( int i = 0; i < READERS; i++ )
		create( readers[i] );

	/* Wait for all the reader threads to indicate they have entered. */
	while ( entered < READERS )
		recvSingle();

	for ( int i = 0; i < WRITERS; i++ )
		writers[i] = new WriterThread( i );

	for ( int i = 0; i < WRITERS; i++ )
		create( writers[i] );

	join();

	log_message( "main exiting" );
	return 0;
}
