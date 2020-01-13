#include "service.h"
#include "main.h"
#include "packet_gen.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sstream>

void ServiceThread::recvShutdown( Message::Shutdown *msg )
{
	breakLoop();
}

void ServiceThread::recvPassthru( Message::PacketPassthru *msg )
{
	if ( brokerConn->selectFd != 0 ) {
		PacketWriter writer( brokerConn );
		Rope *rope = (Rope*) msg->rope;
		PacketBase::send( &writer, *rope, true );
	}
}

int ServiceThread::main()
{
	SSL_CTX *sslCtx = sslCtxClientInternal();

	/* Connection to broker. */
	const char *bh = MainThread::broker != 0 ? MainThread::broker : "localhost";
	brokerConn = new PacketConnection( this );
	brokerConn->initiate( bh , 4830, true, sslCtx, false );

	selectLoop();

	return 0;
}
