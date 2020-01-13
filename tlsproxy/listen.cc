#include "main.h"
#include "listen.h"
#include "proxy.h"
#include "service.h"

#include "packet_gen.h"
#include "genf.h"

#include <errno.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/ssl.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define _GNU_SOURCE 1
#include <sched.h>


void ListenThread::recvShutdown( Message::Shutdown *msg )
{
	log_debug( DBG_PROXY, "received shutdown" );
	breakLoop();

	for ( std::list<SendsToProxy*>::iterator s = proxySends.begin();
			s != proxySends.end(); s++ )
	{
		log_debug( DBG_PROXY, "sending shutdown to proxy thread" );
		SendsToProxy *sendsToProxy = *s;
		sendsToProxy->openShutdown();
		sendsToProxy->send();
	}
}

void ListenThread::enterInlineNamespace()
{
	if ( MainThread::netns == 0 )
		return;

	/* First retrieve an FD for returning to our original namespace. */
	origNsFd = open( "/proc/self/ns/net", O_RDONLY );

	/* Target fd using /var/run/netns facility (not sure how dist-portable this
	 * method is). */
	enterNsFd = open( "/var/run/netns/inline", O_RDONLY );

	if ( origNsFd < 0 || enterNsFd < 0 )
		log_FATAL( "namespace FD open error: " << origNsFd << " " << enterNsFd );

	int result = setns( enterNsFd, CLONE_NEWNET );
	if ( result != 0 ) {
		log_ERROR( "setns result: " << result );
	}
}

void ListenThread::leaveInlineNamespace()
{
	if ( MainThread::netns == 0 )
		return;

	int result = setns( origNsFd, CLONE_NEWNET );
	if ( result != 0 ) {
		log_ERROR( "setns result: " << result );
	}

	::close( origNsFd );
	::close( enterNsFd );
}

int ListenThread::main()
{
	serverCtx = sslCtxServer( PKGDATADIR "/self-signed.key", PKGDATADIR "/self-signed.crt" );
	clientCtx = sslCtxClientPublic();

	if ( ! SSL_CTX_set_tlsext_servername_callback( serverCtx, sslServerNameCallback )
			|| ! SSL_CTX_set_tlsext_servername_arg( serverCtx, 0 ) )
	{
		log_ERROR( "failed to set SNI callback" );
	}

	enterInlineNamespace();

	/* Initiate listen in transparent mode. */
	int listenFd = inetListen( 4430, true );
	makeNonBlocking( listenFd );

	leaveInlineNamespace();

	for ( int i = 0; i < 4; i++ ) {
		log_debug( DBG_PROXY, "starting proxy thread" );

		ProxyThread *proxy = new ProxyThread( serverCtx, clientCtx, &contextMap, listenFd, -1, i );
		SendsToProxy *sendsToProxy = registerSendsToProxy( proxy );
		proxy->sendsToService = proxy->registerSendsToService( service );

		proxy->sendsPassthru = proxy->registerSendsPassthru( service );
		proxy->passthruWriter = proxy->sendsPassthru->writer;

		create( proxy );

		proxySends.push_back( sendsToProxy );
	}

	struct timeval t;
	t.tv_sec = 1;
	t.tv_usec = 0;

	selectLoop( &t );

	::close( listenFd );
	log_debug( DBG_PROXY, "joining" );

	join();

	log_debug( DBG_PROXY, "flushing TLS state" );

	contextMap.close();

	return 0;
}
