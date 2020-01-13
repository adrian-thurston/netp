#include "netp.h"
#include "itq_gen.h"
#include "fmt.h"

#include <aapl/vector.h>
#include <kring/kring.h>

using std::string;

void chomp( Vector<char> &buf )
{
	if ( buf.length() >= 2 && buf.data[buf.length()-2] == '\n' ) {
		buf.data[buf.length()-2] = 0;
		if ( buf.length() >= 3 && buf.data[buf.length()-3] == '\r' )
			buf.data[buf.length()-3] = 0;
	}
}


void HttpBodyParser::start( Context *ctx )
{
	identifier.start( ctx );
	parser = 0;
}

void HttpBodyParser::decidedType( Context *ctx, Packet *packet, FileIdent::FileType fileType )
{
	log_debug( DBG_HTTP, "content type:" << contentType );

	if ( fileType == FileIdent::HTML ) {
		parser = new HtmlParser();
		parser->start( ctx );
	}
	else if ( contentType == ApplicationJson ) {
		parser = new JsonParser();
		parser->start( ctx );
	}
	else {

		if ( packet != 0 && fileType == FileIdent::PE ) {
			// packet->handler->detectedPE->detectedPE( packet );
			ctx->vpt.push( Node::FileTypePe );
			ctx->vpt.pop( Node::FileTypePe );
		}
		else {
			ctx->vpt.push( Node::FileTypeUnknown );
			ctx->vpt.pop( Node::FileTypeUnknown );
		}
	}
}

int HttpBodyParser::receive( Context *ctx, Packet *packet, const wire_t *data, int dlen )
{
	log_debug( DBG_HTTP, "HTTP body: " << log_binary( data, dlen ) );

	/* First try to identify. May produce a parser. */
	if ( identifier.fileType == FileIdent::Working ) {
		// log_debug( DBG_FILE, "sending data to file identifier" );
		identifier.receive( ctx, packet, data, dlen );

		if ( identifier.fileType != FileIdent::Working ) {
			log_debug( DBG_FILE, "decided type " <<
					FileIdent::FtStr[identifier.fileType] << " " <<
					log_hex( data, dlen ) );

			decidedType( ctx, packet, identifier.fileType );
		}
	}

	/* If the half has a parser then send data to it. */
	if ( parser != 0 )
		parser->receive( ctx, packet, data, dlen );

	return 0;
}

void HttpBodyParser::finish( Context *ctx )
{
	if ( parser != 0 ) {
		parser->finish( ctx );
		delete parser;
		parser = 0;
	}
}

Consumer *HttpResponseParser::destination()
{
	Consumer *dest = &bodyParser;
	if ( source != 0 ) {
		splitter.setDest( &bodyParser, source->nextConsumer() );
		dest = &splitter;
	}
		
	if ( isGzipped ) {
		log_debug( DBG_HTTP, "http body: gzip" );
		gzip.destination( dest );
		dest = &gzip;
	}
	else if ( isBrotli ) {
		log_debug( DBG_HTTP, "http body: brotli" );
		brotli.destination( dest );
		dest = &brotli;
	}
	return dest;
}

%%{
	machine http_response;
	alphtype unsigned char;

	action clear1 { buf1.empty(); }
	action buf1 { buf1.append( *p ); }

	action clear2 { buf2.empty(); }
	action buf2 { buf2.append( *p ); }

	action clear { buf.empty(); }
	action buf { buf.append( *p ); }

	action uri {
		/* Deque the HttpResponse struct from the request parser. */
		ctx->vpt.consumePair();

		bodyParser.contentType = HttpBodyParser::Unspecified;
		contentLength = -1;
		isGzipped = false;
		isBrotli = false;
		isChunked = false;
		isConnectionClose = false;
		chunks = 0;
		buf.append( 0 );

		ctx->vpt.push( Node::HttpResponse  );
		ctx->vpt.push( Node::HttpHead );

		/* */
		buf.empty();
	}

	action header {
		buf1.append( 0 );
		buf2.append( 0 );
		buf.append( 0 );

		if ( buf.length() >= 15 && strncasecmp( buf.data, "Content-Length:", 15 ) == 0 )
			contentLength = atoi( buf.data + 15 );

		if ( buf.length() >= 11 && strncasecmp( buf.data, "Connection:", 11 ) == 0 ) {
			if ( strstr( buf.data, "close" ) != 0 )
				isConnectionClose = true;
		}

		if ( buf.length() >= 17 && strncasecmp( buf.data, "Content-Encoding:", 17 ) == 0 ) {
			if ( strstr( buf.data, "gzip" ) != 0 )
				isGzipped = true;
			if ( strstr( buf.data, "br" ) != 0 )
				isBrotli = true;
		}

		if ( buf.length() >= 18 && strncasecmp( buf.data, "Transfer-Encoding:", 18 ) == 0 ) {
			if ( strstr( buf.data, "chunked" ) != 0 )
				isChunked = true;
		}

		if ( wantCookies ) {
			if ( buf.length() >= 11 && strncasecmp( buf.data, "Set-Cookie:", 11 ) == 0 ) {
				char *p = strchr( buf.data, ';' );
				if ( p != 0 ) {
					String c( buf.data + 12, p - buf.data - 12 );

					char *e = strchr( c.data, '=' );
					if ( e != 0 ) {
						String k( c.data, e - c.data );
						cookies[string(k.data)] = string(c.data);
					}
				}
			}
		}

		if ( wantLocation ) {
			if ( buf.length() >= 9 && memcmp( buf.data, "Location:", 9 ) == 0 ) {
				char *p = strchr( buf.data + 10, ':' );
				if ( p ) {
					p = strchr( p + 3, '/' );
					if ( p ) {
						char *e = strchr( p, '\r' );
						location = String( p, e - p );
					}
				}
			}
		}

		if ( buf.length() >= 15 && memcmp( buf.data, "Content-Type:", 13 ) == 0 ) {
			if ( strstr( buf.data + 13, "application/json" ) != 0 )
				bodyParser.contentType = HttpBodyParser::ApplicationJson;
			else {
				bodyParser.contentType = HttpBodyParser::Other;
			}

			log_debug( DBG_HTTP, "content type: " << bodyParser.contentType );
		}

		chomp( buf );
		log_debug( DBG_HTTP, "response header: " << buf.data );
		buf.empty();

		ctx->vpt.push( Node::HttpHeader );

		ctx->vpt.push( Node::HttpHeaderName );
		ctx->vpt.setText( buf1.data );
		ctx->vpt.pop( Node::HttpHeaderName );

		ctx->vpt.push( Node::HttpHeaderValue );
		ctx->vpt.setText( buf2.data );
		ctx->vpt.pop( Node::HttpHeaderValue );

		ctx->vpt.pop( Node::HttpHeader );

	}

	action fin {
		ctx->vpt.pop( Node::HttpHead );

		if ( contentLength == 0 ) {
			isGzipped = isBrotli = false;
			bodyBlock.destination( destination() );
			bodyBlock.start( ctx, packet, p );
			bodyBlock.finish( ctx, packet, p );
		}
		else if ( contentLength > 0 ) {
			log_debug( DBG_HTTP, "http body: content length: " << contentLength );
			fgoto consume_length;
		}
		else if ( isChunked ) {
			log_debug( DBG_HTTP, "http body: chunked" );
			fgoto chunked;
		}
		else if ( isConnectionClose ) {
			fgoto consume_all;
		}

		ctx->vpt.pop( Node::HttpResponse );
	}

	action consume_start
	{
		bodyBlock.destination( destination() );
		bodyBlock.start( ctx, packet, p );
	}

	consume_length := (
		any @{
			//log_message( "consuming: " << contentLength );
			if ( --contentLength <= 0 ) {
				bodyBlockFinish( ctx, packet, p );
				ctx->vpt.pop( Node::HttpResponse );
				fgoto main;
			}
		}
	)*
	>consume_start;

	consume_all := (
		any @{
			if ( --contentLength <= 0 ) {
				bodyBlockFinish( ctx, packet, p );
				ctx->vpt.pop( Node::HttpResponse );
				fgoto main;
			}
		}
	)*
	>consume_start;

	action consume_first_chunk_start
	{
		bodyBlock.destination( destination() );
		bodyBlock.start( ctx, packet, p );
	}

	consume_first_chunk := (
		any @{
			if ( --chunkLength == 0 ) {
				bodyBlock.pause( ctx, packet, p + 1 );
				fgoto chunked;
			}
		}
	)*
	>consume_first_chunk_start;

	action consume_next_chunk_start
	{
		bodyBlock.resume( ctx, packet, p );
	}

	consume_next_chunk := (
		any @{
			if ( --chunkLength == 0 ) {
				bodyBlock.pause( ctx, packet, p + 1 );
				fgoto chunked;
			}
		}
	)*
	>consume_next_chunk_start;

	chunked := (
		( '\r'? '\n' )* [A-Fa-f0-9]+ >clear $buf (';' [^\r\n]+ )?'\r'?'\n' @{
			buf.append(0);
			chunkLength = strtol( buf.data, 0, 16 );
			log_debug( DBG_HTTP, "chunk length: " << chunkLength );
			if ( chunkLength == 0 ) {
				bodyBlockFinish( ctx, packet, p );
				ctx->vpt.pop( Node::HttpResponse );
				fgoto main;
			}
			if ( chunks++ == 0 )
				fgoto consume_first_chunk;
			else
				fgoto consume_next_chunk;
		}
	);

	main := ( 
		( [^ ]+ ' ' [^ ]+ ' ' [^\n\r]+ '\r'? '\n' ) $buf %uri
		(
			( ( [A-Za-z_0-9\-]+ ) >clear1 $buf1 ':'
				( ( [^\r\n] | '\r'? '\n' [ \t] )* '\r'? '\n' ) >clear2 $buf2 ) $buf %header
		)*
		'\r'? '\n' @fin
	)*;
}%%

%% write data;

void HttpResponseParser::bodyBlockFinish( Context *ctx, Packet *packet, const wire_t *p )
{
	if ( source != 0 )
		source->preFinish();

	/* We execute this ON the last char, not next. Since we are
	 * using p .. pe semantics we pass p + 1. */
	bodyBlock.finish( ctx, packet, p + 1 );

	if ( source != 0 ) 
		source->responseComplete();
}

void HttpResponseParser::start( Context *ctx )
{
	%% write init;

	localRoot = ctx->vpt.push( Node::LangHttp );

#if PARSE_REPORT
	if ( ctx->parseReportFailures || ctx->parseReportHttp )
		parseReport.start();
#endif
}

int HttpResponseParser::receive( Context *ctx, Packet *packet, const wire_t *data, int length )
{
	if ( cs == http_response_error )
		return 0;

#if PARSE_REPORT
	if ( ctx->parseReportFailures || ctx->parseReportHttp )
		parseReport.receive( packet, data, length );
#endif

	const wire_t *p = data;
	const wire_t *pe = data + length;
	const wire_t *eof = 0;

	bodyBlock.preExec( ctx, packet, data );

	%% write exec;

	if ( cs == http_response_error ) {
		log_message( "HTTP parse error" );
#if PARSE_REPORT
		if ( ctx->parseReportFailures || ctx->parseReportHttp )
			parseReport.error( localRoot, pe - p, *p );
#endif
	}
	else {
		bodyBlock.postExec( ctx, packet, p );
	}

	return 0;
}

void HttpResponseParser::finish( Context *ctx )
{
	log_message( "response parser finish" );
	if ( isConnectionClose )
		bodyBlock.finish( ctx );

	ctx->vpt.pop( Node::LangHttp );

#if PARSE_REPORT
	if ( ctx->parseReportHttp )
		parseReport.finish( localRoot );
#endif
}

