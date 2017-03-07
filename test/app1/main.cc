#include "main.h"
#include "user.h"
#include "listen.h"
#include "itq_gen.h"

#include <unistd.h>

void MainThread::recvBigPacket( SelectFd *fd, BigPacket *pkt )
{
	log_message( "received BigPacket" );

	std::cout << pkt->big1 << std::endl;
	std::cout << pkt->big2 << std::endl;
	std::cout << pkt->big3 << std::endl;

	std::cout.flush();

	breakLoop();
}

int MainThread::main()
{
	if ( fetch ) {
		int fd = inetConnect( "127.0.0.1", 44726 );

		SelectFd *selectFd = new SelectFd( this, fd, 0 );
		selectFd->state = SelectFd::PktData;
		selectFd->wantRead = true;

		selectFdList.append( selectFd );
		selectLoop();
		::close( fd );

		return 0;
	}

	log_message( "starting up" );

	UserThread *bare = new UserThread;
	ListenThread *listen = new ListenThread;

	SendsToUser *sendsToUser = registerSendsToUser( bare );
	SendsToListen *sendsToListen = registerSendsToListen( listen );

	sendsToUser->openHello();
	sendsToUser->send();

	create( bare );
	create( listen );

	signalLoop();

	sendsToUser->openShutdown();
	sendsToUser->send();

	sendsToListen->openShutdown();
	sendsToListen->send();

	join();

	log_message( "exiting" );

	return 0;
}
