#include "netp.h"
#include "itq_gen.h"
#include "fmt.h"

#include <aapl/vector.h>
#include <kring/kring.h>

using std::string;

%%{
	machine http_request;
	alphtype unsigned char;

	action clear1 { buf1.empty(); }
	action buf1 { buf1.append( *p ); }

	action clear2 { buf2.empty(); }
	action buf2 { buf2.append( *p ); }

	action request_line {
		bodyParser.contentType = HttpBodyParser::Unspecified;
		contentLength = -1;
	}

	action header {
		buf1.append( 0 );
		buf2.append( 0 );

		/* Chomping. */
		char *hd = buf2.data;
		int len = buf2.length();
		if ( len >= 2 && hd[len - 2] == '\n' ) {
			hd[len - 2] = 0;
			if ( len >= 3 && hd[len - 3 ] == '\r' )
				hd[len - 3] = 0;
		}
		while ( *hd == '\t' || *hd == ' ' )
			hd += 1;

		if ( strcasecmp( buf1.data, "Content-Length" ) == 0 ) {
			contentLength = atoi( hd );
		}

		ctx->vpt.push( Node::HttpHeader );

		ctx->vpt.push( Node::HttpHeaderName );
		ctx->vpt.setText( buf1.data );
		ctx->vpt.pop( Node::HttpHeaderName );

		ctx->vpt.push( Node::HttpHeaderValue );
		ctx->vpt.setText( buf2.data );
		ctx->vpt.pop( Node::HttpHeaderValue );

		ctx->vpt.pop( Node::HttpHeader );

		buf1.empty();
		buf2.empty();
	}

	action fin {
		ctx->vpt.pop( Node::HttpHead );

		if ( contentLength > 0 ) {
			log_debug( DBG_HTTP, "content length: " << contentLength );
			fgoto consume;
		}

		ctx->vpt.pop( Node::HttpRequest );
	}

	consume := (
		any @{
			if ( --contentLength <= 0 ) {
				/* We execute this ON the last char, not next. Since we are
				 * using p .. pe semantics we pass p + 1. */
				bodyBlock.finish( ctx, packet, p + 1 );
				ctx->vpt.pop( Node::HttpRequest );
				fgoto main;
			}
		}
	)* >{
		bodyBlock.destination( &bodyParser );
		bodyBlock.start( ctx, packet, p );
	};

	action method
	{
		ctx->vpt.generatePair();

		ctx->vpt.push( Node::HttpRequest  );
		ctx->vpt.setConsumer( this );

		ctx->vpt.push( Node::HttpHead );

		buf1.append(0);
		ctx->vpt.push( Node::HttpMethod );
		ctx->vpt.setText( buf1.data );
		ctx->vpt.pop( Node::HttpMethod );
	}

	action uri
	{
		buf1.append(0);
		ctx->vpt.push( Node::HttpUri );
		ctx->vpt.setText( buf1.data );
		ctx->vpt.pop( Node::HttpUri );
	}

	action protocol
	{
		buf1.append(0);
		ctx->vpt.push( Node::HttpProtocol );
		ctx->vpt.setText( buf1.data );
		ctx->vpt.pop( Node::HttpProtocol );
	}

	main := ( 
		( 
			[A-Z]+ >clear1 $buf1 %method ' '
			( [^ ]+ ) >clear1 $buf1 %uri ' '
			[^\n\r]+ >clear1 $buf1 %protocol '\r'? '\n'
		) %request_line
		(
			( ( [A-Za-z_0-9\-]+ ) >clear1 $buf1 ':'
				( ( [^\r\n] | '\r'? '\n' [ \t] )* '\r'? '\n' ) >clear2 $buf2 ) %header 
		)*
		'\r'? '\n' @fin
	)*;
}%%

%% write data;

void HttpRequestParser::start( Context *ctx )
{
	%% write init;
}

int HttpRequestParser::receive( Context *ctx, Packet *packet, const wire_t *data, int length )
{
	if ( cs == http_request_error )
		return 0;

	const wire_t *p = data;
	const wire_t *pe = data + length;
	const wire_t *eof = 0;

	bodyBlock.preExec( ctx, packet, data );

	%% write exec;

	if ( cs == http_request_error )
		log_debug( DBG_HTTP, "REQUEST PARSE FAILED" );
	else {
		bodyBlock.postExec( ctx, packet, p );
	}

	return 0;
}

