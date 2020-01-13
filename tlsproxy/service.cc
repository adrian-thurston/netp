#include "service.h"
#include "main.h" 
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fstream>

void ServiceThread::recvShutdown( Message::Shutdown *msg )
{
	log_message( "received shutdown" );
	breakLoop();
}

void ServiceThread::recvCertGenerated( Message::CertGenerated *msg )
{
	Packer::CertGenerated pkt( brokerConn );
	pkt.set_host( msg->host );
	pkt.send();
}

void ServiceThread::recvPassthru( Message::PacketPassthru *msg )
{
	if ( brokerConn->selectFd != 0 ) {
		PacketWriter writer( brokerConn );
		Rope *rope = (Rope*) msg->rope;
		PacketBase::send( &writer, *rope, true );
	}
}

void ServiceThread::handleTimer()
{
}

int ServiceThread::main()
{
	/* Connection to broker. */
	const char *bh = MainThread::broker != 0 ? MainThread::broker : "localhost";

	brokerConn = new PacketConnection( this );

	SSL_CTX *sslCtx = sslCtxClientInternal();
	brokerConn->initiate( bh, 4830, true, sslCtx, false );

	struct timeval t;
	t.tv_sec = 1;
	t.tv_usec = 0;

	handleTimer();

	selectLoop( &t );

	return 0;
}
