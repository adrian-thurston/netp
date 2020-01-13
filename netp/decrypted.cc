#include "packet.h"
#include "netp.h"
#include "itq_gen.h"

#include <sstream>
#include <fstream>

void Handler::decrypted( long id, int type, const char *host, wire_t *bytes, int len )
{
	DecrDictEl *decrEl = decrDict.find( id );
	Packet _packet;
	memset( &_packet, 0, sizeof(_packet) );
	Packet *packet = &_packet;
	packet->handler = this;
	packet->dir = Packet::UnknownDir;

	if ( decrEl == 0 ) {
		log_debug( DBG_DECR, "creating decrypted connection: " << id );

		Decrypted *decrypted = new Decrypted( netpConfigure, id );
		decrDict.insert( id, decrypted, &decrEl );
	}

	Decrypted *decrypted = decrEl->value;
	DecrHalf *half = type == 1 ? &decrypted->h1 : &decrypted->h2;
	DecrHalf *other = type == 1 ? &decrypted->h2 : &decrypted->h1;

	if ( decrypted->proto == Conn::Working ) {

		log_debug( DBG_DECR, "sending data to identifier" );
		half->identifier.receive( &half->ctx, packet, bytes, len );

		if ( half->identifier.proto == Identifier::HTTP_REQ ||
			half->identifier.proto == Identifier::HTTP_RSP )
		{
			log_debug( DBG_DECR, "determined type to be HTTP" );

			decrypted->proto = Conn::HTTP;

			HttpRequestParser *requestParser =
					new HttpRequestParser( &decrypted->h1.ctx );
			HttpResponseParser *responseParser =
					new HttpResponseParser( &decrypted->h2.ctx, 0, false, false );

			requestParser->responseParser = responseParser;
			responseParser->requestParser = requestParser;

			decrypted->h1.parser = requestParser;
			decrypted->h2.parser = responseParser;

			decrypted->h1.ctx.vpt.other = &decrypted->h2.ctx.vpt;
			decrypted->h2.ctx.vpt.other = &decrypted->h1.ctx.vpt;
			
			decrypted->h1.ctx.vpt.push( Node::ConnTls );
			decrypted->h1.ctx.vpt.push( Node::ConnHost );
			if ( host != 0 )
				decrypted->h1.ctx.vpt.setText( host );
			decrypted->h1.ctx.vpt.pop( Node::ConnHost );

			decrypted->h2.ctx.vpt.push( Node::ConnTls );
			decrypted->h2.ctx.vpt.push( Node::ConnHost );
			if ( host != 0 )
				decrypted->h2.ctx.vpt.setText( host );
			decrypted->h2.ctx.vpt.pop( Node::ConnHost );

			requestParser->start( &decrypted->h1.ctx );
			responseParser->start( &decrypted->h2.ctx );
		}
	}

	bool existingError = half->ctx.vpt.vptErrorOcccurred || other->ctx.vpt.vptErrorOcccurred;

	/* If the half has a parser then send data to it. */
	if ( half->parser != 0 ) {
		if ( decrypted->stashErrors || decrypted->stashAll )
			half->cache.append( (char*)bytes, len );

		half->parser->receive( &half->ctx, packet, bytes, len );
	}

	/* Was no error. Error now means to trigger dump. */
	if ( !existingError && half->ctx.vpt.vptErrorOcccurred ) {
		
		std::stringstream fn1;
		fn1 << "/tmp/dump-h1-" << id;
		std::ofstream f1( fn1.str().c_str() );

		for ( RopeBlock *rb = decrypted->h1.cache.hblk; rb != 0; rb = rb->next )
			f1.write( decrypted->h1.cache.data( rb ), decrypted->h1.cache.length( rb ) );

		std::stringstream fn2;
		fn2 << "/tmp/dump-h2-" << id;
		std::ofstream f2( fn2.str().c_str() );
		
		for ( RopeBlock *rb = decrypted->h2.cache.hblk; rb != 0; rb = rb->next )
			f2.write( decrypted->h2.cache.data( rb ), decrypted->h2.cache.length( rb ) );
	}

	if ( decrypted->stashAll && half->ctx.vpt.depth() <= 2 ) {
		std::stringstream fn1;
		fn1 << "/tmp/dump-h1-" << id;
		std::ofstream f1( fn1.str().c_str() );

		f1 << host << std::endl;

		for ( RopeBlock *rb = half->cache.hblk; rb != 0; rb = rb->next )
			f1.write( half->cache.data( rb ), half->cache.length( rb ) );
	}

	if ( decrypted->stashAll && other->ctx.vpt.depth() <= 2 ) {
		std::stringstream fn2;
		fn2 << "/tmp/dump-h2-" << id;
		std::ofstream f2( fn2.str().c_str() );
		
		f2 << host << std::endl;

		for ( RopeBlock *rb = other->cache.hblk; rb != 0; rb = rb->next )
			f2.write( other->cache.data( rb ), other->cache.length( rb ) );
	}
}
