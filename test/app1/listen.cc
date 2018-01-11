#include "main.h"
#include "listen.h"

#include "packet_gen.h"
#include "genf.h"

#include <errno.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/ssl.h>

#include <sys/socket.h>
#include <arpa/inet.h>

void ListenThread::recvShutdown( Shutdown *msg )
{
	log_debug( DBG_THREAD, "received shutdown" );
	breakLoop();
}


void ListenThread::handleTimer()
{
	log_message( "tick" );
}

void ListenThread::tlsAcceptResult( SelectFd *fd, int sslError )
{
	if ( sslError != SSL_ERROR_NONE ) {
		/* Fatal for the connection. Report the error and close the server end.
		 * Put a stop to the connection end. */
		// tlsError( DBG_THREAD, sslError );
		// proxyShutdown( fd );
		// tlsError( sslError );
		log_ERROR( "accept failed" );
	}
	else {
		log_message( "TLS accept completed" );

		// FdDesc *fdDesc = fdLocal( fd );

		Connection *c = static_cast<Connection*>(fd->local);
		PacketConnection *pc = dynamic_cast<PacketConnection*>(c);

		/* Go into established state and start reading. Will buffer until other
		 * half is ready. */
		fd->type = SelectFd::Connection;
		fd->state = SelectFd::TlsEstablished;

		fd->tlsEstablished = true;
		// fd->tlsWantRead = true;

		/* Immediately try to read. Is this necessary? IE will the connect
		 * leave data on the FD or buffer it in? */
		// sslReadReady( fd );
		PacketWriter writer( pc );

		for ( int i = 0; i < 100; i++ ) {
			BigPacket *msg = BigPacket::open( &writer );
			msg->set_big3( &writer, ::data3 );
			msg->set_big2( &writer, ::data2 );
			msg->set_big1( &writer, ::data1 );
			msg->l1 = ::l1;
			msg->l2 = ::l2;
			msg->l3 = ::l3;
			BigPacket::send( &writer );
		}
	}
}


void ListenThread::notifyAccept( PacketConnection *pc )
{
	log_message( "incoming connection fd: " << pc->selectFd->fd << " sending big data" );

	PacketWriter writer( pc );

	for ( int i = 0; i < 100; i++ ) {
		BigPacket *msg = BigPacket::open( &writer );
		msg->set_big3( &writer, ::data3 );
		msg->set_big2( &writer, ::data2 );
		msg->set_big1( &writer, ::data1 );
		msg->l1 = ::l1;
		msg->l2 = ::l2;
		msg->l3 = ::l3;
		BigPacket::send( &writer );
	}
}


int ListenThread::main()
{
	log_message("starting up" );

	PacketListener *listener = new PacketListener( this );

	listener->tlsAccept = true;
	listener->serverCtx = sslServerCtx(
			PKGDATADIR "/server.key",
			PKGDATADIR "/server.crt",
			PKGDATADIR "/verify.pem"
	);

	listener->startListen( 44726, true );

	struct timeval t;
	t.tv_sec = 1;
	t.tv_usec = 0;

	selectLoop( &t );

	::close( listener->selectFd->fd );

	log_message( "exiting" );

	return 0;
}
