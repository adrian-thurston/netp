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
#include <netp/module.h>

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

	void store();
	bool ready;
};

void BrokerSendConnection::connectComplete()
{
	PacketConnection::notifyAccept();

	log_message( "have broker connection, sending" );

	Packer::PacketType pt( this );
	Packer::StoreMe::describe( pt );
	pt.send();

	ready = true;

	store();
}

void BrokerSendConnection::store()
{
	if ( !ready )
		return;

	Packer::StoreMe storeMe( this );
	storeMe.set_s( "vilnius" );

	Packer::Record record;

	storeMe.alloc_records( record );
	record.set_b( true );
	record.set_i( -1 );
	record.set_ui( 2 );
	record.set_l( -3 );
	record.set_ul( 4 );
	record.set_s( "hello" );
	record.set_c( "1234567890" );

	storeMe.alloc_records( record );
	record.set_b( false );
	record.set_i( -10 );
	record.set_ui( 20 );
	record.set_l( -30 );
	record.set_ul( 40 );
	record.set_s( "there" );
	record.set_c( "9876543210" );

	storeMe.send();
}

void MainThread::handleTimer()
{
	log_message( "timer" );
	brokerConn->store();
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

