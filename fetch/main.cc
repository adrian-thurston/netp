#include "main.h"

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
		: PacketConnection(thread), mainThread(thread) {}

	MainThread *mainThread;

	virtual void connectComplete();
};

void BrokerSendConnection::connectComplete()
{
	PacketConnection::notifyAccept();

	// log_message( "broker connect complete" );

	//log_message( "fetch dispatch packet" );
	for ( Fetch *fetch = mainThread->fetchList.head; fetch != 0; fetch = fetch->next )
		fetch->wantIds( this );
}

void MainThread::configureContext( Context *ctx )
{
	moduleList.fetchConfigureContext( this, ctx );
}

void MainThread::handleTimer()
{
	for ( Fetch *fetch = fetchList.head; fetch != 0; fetch = fetch->next )
		fetch->timer();
}

void MainThread::dispatchPacket( SelectFd *fd, Recv &recv )
{
	//log_message( "fetch dispatch packet" );
	for ( Fetch *fetch = fetchList.head; fetch != 0; fetch = fetch->next )
		fetch->dispatchPacket( fd, recv );
}

int MainThread::main()
{
	log_message( "starting" );

	for ( OptStringEl *opt = moduleArgs.head; opt != 0; opt = opt->next )
		moduleList.loadModule( opt->data );
	
	tlsStartup();
	SSL_CTX *ctxPublic = sslCtxClientPublic();
	SSL_CTX *ctxClient = sslCtxClientInternal();
	SSL_CTX *ctxServer = sslCtxServerInternal();

	/* Connection to broker. */
	brokerConn = new BrokerSendConnection( this );
	brokerConn->initiate( "localhost", 4830, true, ctxClient, false );

	/* Listener for incoming commands. */
	commandListener = new PacketListener( this );
	commandListener->startListen( 4831, true, ctxServer, false );

	moduleList.fetchAllocFetchObjs( &fetchList, this, this, ctxPublic, this->brokerConn );

	struct timeval t;
	t.tv_sec = 1;
	t.tv_usec = 0;

	handleTimer();

	/* Main loop. */
	selectLoop( &t );

	/* done, cleanup. */
	tlsShutdown();
	log_message( "exiting" );

	return 0;
}

