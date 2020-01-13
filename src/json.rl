#include <genf/thread.h>

#include "json.h"
#include "itq_gen.h"
#include "netp.h"

int *grow_stack2( int *stack, int *psd )
{
	int old = *psd;
	*psd = *psd * 2;

	log_debug( DBG_JSON, "json: growing stack from " << old << " to " << *psd );

	int *ns = new int[*psd];
	memcpy( ns, stack, old * sizeof(int) );
	delete[] stack;
	return ns;
}

%%{
	machine JSON;
	alphtype unsigned int;

	write data;

	prepush {
		if ( top == sd )
			stack = grow_stack2( stack, &sd );
	}

    cr                  = '\n';
    cr_neg              = [^\n];
    ws                  = [ \t\r\n];
    c_comment           = '/*' ( any* - (any* '*/' any* ) ) '*/';
    cpp_comment         = '//' cr_neg* cr;
    comment             = c_comment | cpp_comment;
    ignore              = ws | comment;
    name_separator      = ':';
    value_separator     = ',';
    Vnull               = 'null';
    Vfalse              = 'false';
    Vtrue               = 'true';
    VNaN                = 'NaN';
    VInfinity           = 'Infinity';
    VMinusInfinity      = '-Infinity';
    begin_value         = [nft\"\-\[\{NI] | digit;
    begin_object        = '{';
    end_object          = '}';
    begin_array         = '[';
    end_array           = ']';
    begin_string        = '"';
    begin_name          = begin_string;
    begin_number        = digit | '-';

	action buf { buf.append( *p ); }

	name := (
			'"'
			(
				^([\"\\] | 0..0x1f) |
				'\\'[\"\\/bfnrt] |
				'\\u'[0-9a-fA-F]{4} |
				'\\'^([\"\\/bfnrtu]|0..0x1f)
			)*
			'"'
		) $buf
		@{
			buf.append(0);
			ctx->vpt.setText( std::string( buf.data ) );
			buf.empty();
			fret;
		};

	number := 
		(
			'-'?
			( '0' | [1-9][0-9]* )
			( '.' @{dot=true;} [0-9]+ )? 
			( [Ee] @{ E = true; } [+\-]? [0-9]+ )? 
		) >{ dot = false; E = false; } $buf
		[^.Ee0-9]
		@{
			/* Trim trailing zeros to assist with comparison to text base. */
			if ( dot && !E ) {
				int newlen = buf.length();
				while ( newlen > 0 && buf[newlen-1] == '0' )
					newlen -= 1;
				if ( buf[newlen-1] == '.' )
					newlen -= 1;
				if ( newlen < buf.length() )
					buf.remove( newlen, buf.length() - newlen );
			}
			buf.append( 0 );

			ctx->vpt.setText( std::string( buf.data ) );
			buf.empty();
			if ( !ctx->vpt.pop( Node::JsonNumber ) ) fgoto *JSON_error;
			fhold; fret;
		};

	string :=
	(
		'"'
		(
			^( [\"\\] | 0..0x1f ) @buf |

			'\\"'  @{ buf.append( '"' ); } |
			'\\\\' @{ buf.append( '\\' ); } |
			'\\/'  @{ buf.append( '/' ); } |
			'\\b'  @{ buf.append( '\b' ); } |
			'\\f'  @{ buf.append( '\f' ); } |
			'\\n'  @{ buf.append( '\n' ); } |
			'\\r'  @{ buf.append( '\r' ); } |
			'\\t'  @{ buf.append( '\t' ); } |

			( '\\u'[0-9a-fA-F]{4} ) |
			( '\\'^([\"\\/bfnrtu]|0..0x1f) ) 
		)*
		'"'
	)
	@{
		buf.append(0);
		ctx->vpt.setText( std::string( buf.data ) );
		buf.empty();
		if ( !ctx->vpt.pop( Node::JsonString ) ) fgoto *JSON_error;
		fret;
	};

	action call_value { fhold; fcall value; }

	next_element =
		value_separator
		ignore*
		begin_value >call_value;

	array :=
		begin_array
		ignore*
		(
			begin_value >call_value
			ignore*
			( next_element ignore* )*
		)?
		end_array @{
			if ( !ctx->vpt.pop( Node::JsonArray ) ) fgoto *JSON_error;
			fret;
		};

	action call_name { ctx->vpt.push( Node::JsonField ); fhold; fcall name; }

	pair =
		ignore*
		begin_name >call_name 
		ignore*
		name_separator
		ignore* 
		begin_value >call_value;

	object :=
	# At lease one field.
	(
		begin_object 
		(	
			pair
			(
				ignore*
				value_separator @{ if ( !ctx->vpt.pop( Node::JsonField ) ) fgoto *JSON_error; }
				pair
			)*
		)
		ignore*
		end_object @{
			if ( !ctx->vpt.pop( Node::JsonField ) ) fgoto *JSON_error;
			if ( !ctx->vpt.pop( Node::JsonObject ) ) fgoto *JSON_error;
			fret;
		}
	)
	|
	# Empty object.
	(
		begin_object 
		ignore*
		end_object @{
			if ( !ctx->vpt.pop( Node::JsonObject ) ) fgoto *JSON_error;
			fret;
		}
	);

	action parse_null     { ctx->vpt.push( Node::JsonNull ); ctx->vpt.pop( Node::JsonNull ); }
	action parse_false    { ctx->vpt.push( Node::JsonFalse ); ctx->vpt.pop( Node::JsonFalse ); }
	action parse_true     { ctx->vpt.push( Node::JsonTrue ); ctx->vpt.pop( Node::JsonTrue ); }
	action parse_nan      { ctx->vpt.push( Node::JsonNaN ); ctx->vpt.pop( Node::JsonNaN ); }
	action parse_infinity { ctx->vpt.push( Node::JsonInfinity ); ctx->vpt.pop(); }
	action parse_number   { ctx->vpt.push( Node::JsonNumber ); fhold; fcall number; }
	action parse_string   { ctx->vpt.push( Node::JsonString ); fhold; fcall string; }
	action parse_array    { ctx->vpt.push( Node::JsonArray ); fhold; fcall array; }
	action parse_object   { ctx->vpt.push( Node::JsonObject ); fhold; fcall object; }

	# The vt is supplied to us by the caller (can come from different places,
	# arrays, objects, root, etc), but needs to be populated with data. When we
	# return, we pop it so the caller doesn't need to. */
	value :=
		ignore*
		(
			Vnull     @parse_null |
			Vfalse    @parse_null |
			Vtrue     @parse_null |
			VNaN      @parse_null |
			VInfinity @parse_null |
			begin_number >parse_number |
			begin_string >parse_string |
			begin_array  >parse_array |
			begin_object >parse_object
		)
		any @{
			fhold;
			fret;
		};

	main :=
		( ")]}'" 0x0a ) ?
		(
			[^)}'.\]] @{
				fhold;
				fcall value;
			}
			ignore*
		)+;
		
}%%

void JsonParser::start( Context *ctx )
{
	top = 0;
	sd = 16;
	stack = new int[sd];

	%% write init;

	localRoot = ctx->vpt.push( Node::LangJson );

	log_debug( DBG_JSON, "JSON: pat roots: " << ctx->vpt.patRoots.length() );

#if PARSE_REPORT
	if ( ctx->parseReportFailures || ctx->parseReportJson )
		parseReport.start();
#endif
	log_debug( DBG_JSON, "JSON: start" );
}

int JsonParser::receive( Context *ctx, Packet *packet, const wire_t *data, int length )
{
	if ( cs == JSON_error )
		return 0;

#if PARSE_REPORT
	if ( ctx->parseReportFailures || ctx->parseReportJson )
		parseReport.receive( packet, data, length );
#endif

	const unsigned char *p = data;
	const unsigned char *pe = p + length;
	%% write exec;

	if ( cs == JSON_error ) {
		log_message( "JSON parse error: " << ( p - data ) );
#if PARSE_REPORT
		if ( ctx->parseReportFailures || ctx->parseReportJson )
			parseReport.error( localRoot, pe - p, *p );
#endif
	}
	
	return 0;
}

void JsonParser::finish( Context *ctx )
{
	ctx->vpt.pop( Node::LangJson );

	log_debug( DBG_JSON, "JSON: finish" );
#if PARSE_REPORT
	if ( ctx->parseReportJson )
		parseReport.finish( localRoot );
#endif
}

