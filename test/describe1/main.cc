#include "main.h"
#include "packet_gen.h"

#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <fstream>
#include <sstream>

#include <sys/types.h>
#include <sys/wait.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>

#include <aapl/dlist.h>
#include <parse/module.h>

struct BrokerSendConnection
:   
	public PacketConnection
{
	BrokerSendConnection( MainThread *thread )
	:
		PacketConnection(thread),
		mainThread(thread),
		ready(false)
	{}

	MainThread *mainThread;

	virtual void connectComplete();

	void store1();
	void store2();
	bool ready;
};

void BrokerSendConnection::connectComplete()
{
	PacketConnection::notifyAccept();

	log_message( "have broker connection, sending" );

	store2();
}

void BrokerSendConnection::store1()
{
	Packer::StoreMeA::describe( this );

	Packer::StoreMeA storeMe( this );
	storeMe.set_s( "vilnius" );

	Packer::Record1 record;

	storeMe.alloc_records( record );
	record.set_t( "calgary" );
	record.set_b( true );
	record.set_i( -1 );
	record.set_ui( 2 );
	record.set_l( -3 );
	record.set_ul( 4 );
	record.set_s( "hello" );
	record.set_c( "1234567890" );

	storeMe.alloc_records( record );
	record.set_t( "vancouver" );
	record.set_b( false );
	record.set_i( -10 );
	record.set_ui( 20 );
	record.set_l( -30 );
	record.set_ul( 40 );
	record.set_s( "there" );
	record.set_c( "9876543210" );

	storeMe.send();
}

void BrokerSendConnection::store2()
{
	Packer::StoreMeB::describe( this );

	Packer::StoreMeB storeMe0( this );
	storeMe0.set_t( "0" );
	storeMe0.set_c1( "" );
	storeMe0.set_c2( "" );
	storeMe0.set_c5( "" );
	storeMe0.send();

	Packer::StoreMeB storeMe1( this );
	storeMe1.set_t( "1" );
	storeMe1.set_c1( "a" );
	storeMe1.set_c2( "a" );
	storeMe1.set_c5( "a" );
	storeMe1.send();

	Packer::StoreMeB storeMe2( this );
	storeMe2.set_t( "2" );
	storeMe2.set_c1( "aa" );
	storeMe2.set_c2( "aa" );
	storeMe2.set_c5( "aa" );
	storeMe2.send();

	Packer::StoreMeB storeMe3( this );
	storeMe3.set_t( "3" );
	storeMe3.set_c1( "aaa" );
	storeMe3.set_c2( "aaa" );
	storeMe3.set_c5( "aaa" );
	storeMe3.send();

	Packer::StoreMeB storeMe4( this );
	storeMe4.set_t( "4" );
	storeMe4.set_c1( "aaaa" );
	storeMe4.set_c2( "aaaa" );
	storeMe4.set_c5( "aaaa" );
	storeMe4.send();

	Packer::StoreMeB storeMe5( this );
	storeMe5.set_t( "5" );
	storeMe5.set_c1( "aaaaa" );
	storeMe5.set_c2( "aaaaa" );
	storeMe5.set_c5( "aaaaa" );
	storeMe5.send();

	Packer::StoreMeB storeMe6( this );
	storeMe6.set_t( "6" );
	storeMe6.set_c1( "aaaaaa" );
	storeMe6.set_c2( "aaaaaa" );
	storeMe6.set_c5( "aaaaaa" );
	storeMe6.send();
}


void MainThread::handleTimer()
{
	log_message( "timer" );
}

int MainThread::main()
{
	log_message( "starting" );

	tlsStartup();
	SSL_CTX *ctxClient = sslCtxClientInternal();

	/* Connection to broker. */
	brokerConn = new BrokerSendConnection( this );
	brokerConn->initiate( "localhost", 4830, true, ctxClient, false );

	struct timeval t;
	t.tv_sec = 10;
	t.tv_usec = 0;

	/* Main loop. */
	selectLoop( &t );

	/* done, cleanup. */
	tlsShutdown();
	log_message( "exiting" );

	return 0;
}

