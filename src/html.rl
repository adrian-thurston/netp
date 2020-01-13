#include "netp.h"

%%{
	machine HTML;
	alphtype unsigned char;

	ws = [ \t\n\r];

	action tag_buf {
		tag.append(*p);
	}
	action tag_fin {
		tag.append(0);
		if ( strcasecmp( tag.data, "base" ) == 0 ) {
			noEndTag = true;
		}
		else if ( strcasecmp( tag.data, "link" ) == 0 ) {
			noEndTag = true;
		}
		else if ( strcasecmp( tag.data, "meta" ) == 0 ) {
			noEndTag = true;
		}
		else if ( strcasecmp( tag.data, "img" ) == 0 ) {
			noEndTag = true;
		}
		else if ( strcasecmp( tag.data, "hr" ) == 0 ) {
			noEndTag = true;
		}
		else if ( strcasecmp( tag.data, "input" ) == 0 ) {
			noEndTag = true;
		}
		else if ( strcasecmp( tag.data, "br" ) == 0 ) {
			noEndTag = true;
		}
		else {
			noEndTag = false;
		}
	}

	action key_buf {
		key.append(*p);
	}

	action reset_close_attr {
		closeAttr = false;
	}
	action maybe_close_attr {
		if ( closeAttr ) {
			if ( !ctx->vpt.pop( Node::HtmlAttr ) ) fgoto *HTML_error;
		}
	}

	action key_fin {
		key.append(0);
		ctx->vpt.push( Node::HtmlAttr );
		ctx->vpt.setText( key.data );
		key.empty();
		closeAttr = true;
	}

	action val_buf {
		val.append(*p);
	}

	action val_fin {
		val.append(0);
		ctx->vpt.push( Node::HtmlVal );
		ctx->vpt.setText( val.data );
		val.empty();
		ctx->vpt.pop( Node::HtmlVal );
	}

	tag_name_base =
		( ^( '/' | '>' | '!' | ws ) )+;

	tag_name_prohib = 'script';

	tag_name =
		( tag_name_base - tag_name_prohib ) $tag_buf %tag_fin;

	attributes = ( 
		ws+ 
		(
			( ^( '>' | '/' | '=' | ws ) )+ >maybe_close_attr $key_buf %key_fin
			(
				ws* '=' ws*
				(
					( '"' [^"]* '"' ) |
					^('>'|'"'|ws) ( ^ ('>'|ws) )*
				) $val_buf %val_fin 
			)?
			ws*
		)**
	);

	action tag_open { ctx->vpt.push( Node::HtmlTag ); ctx->vpt.setText( tag.data ); tag.empty(); }
	action tag_close { if ( ! ctx->vpt.pop( Node::HtmlTag ) ) fgoto *HTML_error; tag.empty(); }
	action tag_singular { if ( !noEndTag) { if ( ! ctx->vpt.pop( Node::HtmlTag ) ) fgoto *HTML_error; } }
	action tag_no_end { if ( noEndTag ) { if ( ! ctx->vpt.pop( Node::HtmlTag ) ) fgoto *HTML_error; } }

	tag_open = (
		'<' @reset_close_attr
			tag_name %tag_open
			attributes?
		( ( '/' @tag_singular )? '>' ) >maybe_close_attr @tag_no_end
	);

	tag_close = (
		'</'
		tag_name %tag_close
		'>'
	);

	action script_tag {
		ctx->vpt.push( Node::HtmlTag );
		ctx->vpt.setText( "script" );
		tag.empty();
		if ( ! ctx->vpt.pop( Node::HtmlTag ) ) fgoto *HTML_error;
	}

	script =
		'<' ws* 'script' %script_tag ( ws [^>]* )? '>' any* :>> ( '</' ws* 'script' ws* '>' );

	action content_buf { ctx->vpt.appendChar( Node::HtmlText, *p ); }
	action ws_buf      { ctx->vpt.appendChar( Node::HtmlText, ' ' ); }

	word = ( ^('<' | ws ) @content_buf )+;

	main :=
	(
		(
			tag_open |
			tag_close |
			script |
			( '<!--' any* :>> '-->' ) |
			( '<!' [^\-] any* :>> '>' ) |
			( '<!>' ) |
			( ws+ | ws* word ( ws+ %ws_buf word )* ws* )
		)**
	);
}%%

%% write data;

void HtmlParser::start( Context *ctx )
{
	%% write init;

	localRoot = ctx->vpt.push( Node::LangHtml );

	ctx->vpt.pushBarrier();

#if PARSE_REPORT
	if ( ctx->parseReportFailures || ctx->parseReportHtml )
		parseReport.start();
#endif
}

int HtmlParser::receive( Context *ctx, Packet *packet, const wire_t *data, int length )
{
	if ( cs == HTML_error )
		return 0;

#if PARSE_REPORT
	if ( ctx->parseReportFailures || ctx->parseReportHtml )
		parseReport.receive( packet, data, length );
#endif

	const wire_t *p = data;
	const wire_t *pe = p + length;

	%% write exec;

	if ( cs == HTML_error ) {
		log_message( "HTML parse error: " << ( p - data ) );
#if PARSE_REPORT
		if ( ctx->parseReportFailures || ctx->parseReportHtml )
			parseReport.error( localRoot, pe - p, *p );
#endif
	}

	return 0;
}

void HtmlParser::finish( Context *ctx )
{
	ctx->vpt.popBarrier();

	ctx->vpt.pop( Node::LangHtml );

#if PARSE_REPORT
	if ( ctx->parseReportHtml )
		parseReport.finish( localRoot );
#endif
}
