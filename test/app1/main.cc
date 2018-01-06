#include "main.h"
#include "user.h"
#include "listen.h"
#include "itq_gen.h"

#include <unistd.h>

extern const char data1[];
extern const char data2[];

void MainThread::recvBigPacket( SelectFd *fd, BigPacket *pkt )
{
	log_message( "received BigPacket" );

	log_message( "big1 check ... " << ( strcmp( pkt->big1, ::data1 ) == 0 ? "OK" : "FAILED" ) );
	log_message( "big2 check ... " << ( strcmp( pkt->big2, ::data2 ) == 0 ? "OK" : "FAILED" ) );
	log_message( "big3 check ... " << ( strcmp( pkt->big3, ::data3 ) == 0 ? "OK" : "FAILED" ) );

	log_message( "l1 check   ... " << ( ( pkt->l1 != ::l1 ) == 0 ? "OK" : "FAILED" ) );
	log_message( "l2 check   ... " << ( ( pkt->l2 != ::l2 ) == 0 ? "OK" : "FAILED" ) );
	log_message( "l3 check   ... " << ( ( pkt->l3 != ::l3 ) == 0 ? "OK" : "FAILED" ) );

}

void MainThread::handleTimer()
{
	static bool done = false;

	if ( !done ) {
		done = true;

		/* Connection to broker. */
		PacketConnection *pc = new PacketConnection( this, 0 );
		pc->initiatePkt( "localhost", 44726 );
	}
}

int MainThread::main()
{
	log_message( "starting up" );

	UserThread *bare = new UserThread;
	ListenThread *listen = new ListenThread;

	SendsToUser *sendsToUser = registerSendsToUser( bare );
	SendsToListen *sendsToListen = registerSendsToListen( listen );

	sendsToUser->openHello();
	sendsToUser->send();

	create( bare );
	create( listen );

	struct timeval t;
	t.tv_sec = 1;
	t.tv_usec = 0;

	selectLoop( &t );

	sendsToUser->openShutdown();
	sendsToUser->send();

	sendsToListen->openShutdown();
	sendsToListen->send();

	join();

	log_message( "exiting" );

	return 0;
}
