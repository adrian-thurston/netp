#include "main.h"
#include "itq_gen.h"
#include "packet_gen.h"
#include "parse.h"
#include "pattern.h"

#include <unistd.h>
#include <signal.h>
#include <fstream>

struct NetpTesting
:
	public NetpConfigure
{
	void configureContext( Context *ctx )
	{
		// ctx->addPat( PatternTdWebBrokerHeader::cons( this ) );
	}
};

int MainThread::main()
{
	log_message( "starting" );

	NetpTesting netpTesting;

	if ( MainThread::parseHttpRequest != 0 ) {
		Context ctx( &netpTesting );
		HttpRequestParser httpRequestParser( &ctx );

		std::ifstream f( MainThread::parseHttpRequest );

		std::string host;
		char c;

		f >> host;
		f >> c;

		ctx.vpt.push( Node::ConnTls );
		ctx.vpt.push( Node::ConnHost );
		if ( host.size() > 0 )
			ctx.vpt.setText( host );
		ctx.vpt.pop( Node::ConnHost );

		httpRequestParser.start( &ctx );

		Packet _packet;
		memset( &_packet, 0, sizeof(_packet) );
		Packet *packet = &_packet;
		packet->handler = 0;
		packet->dir = Packet::UnknownDir;
		
		if ( f.is_open() ) {
			while ( !f.eof() ) {
				char buf[BUFSIZ];
				f.read( buf, sizeof(buf ) );
				size_t count = f.gcount();
				if ( count > 0 ) {
					log_message( "read " << count << " bytes" );

					httpRequestParser.receive( &ctx, packet, (const wire_t*)buf, count );
				}
			}
		}
	}

	if ( MainThread::parseHttpResponse != 0 ) {
		Context ctx( &netpTesting );
		HttpResponseParser httpResponseParser( &ctx, 0, false, false );

		std::ifstream f( MainThread::parseHttpResponse );

		std::string host;
		char c;

		f >> host;
		f >> c;

		ctx.vpt.push( Node::ConnTls );
		ctx.vpt.push( Node::ConnHost );
		if ( host.size() > 0 )
			ctx.vpt.setText( host );
		ctx.vpt.pop( Node::ConnHost );

		httpResponseParser.start( &ctx );

		Packet _packet;
		memset( &_packet, 0, sizeof(_packet) );
		Packet *packet = &_packet;
		packet->handler = 0;
		packet->dir = Packet::UnknownDir;
		
		if ( f.is_open() ) {
			while ( !f.eof() ) {
				char buf[BUFSIZ];
				f.read( buf, sizeof(buf ) );
				size_t count = f.gcount();
				if ( count > 0 ) {
					log_message( "read " << count << " bytes, sending to response parser" );

					httpResponseParser.receive( &ctx, packet, (const wire_t*)buf, count );
				}
			}
		}
	}

	log_message( "exiting" );
	return 0;
}
