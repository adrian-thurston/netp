#include <genf/thread.h>

#include "itq_gen.h"
#include "json.h"
#include "parse.h"

const Value Value::null = { JsonNull, { 0 } };
const String Value::emptyString("");
const String Value::zeroString("0");

Value::~Value()
{
	switch ( type ) {
		case JsonObject:
			o->empty();
			delete o;
			break;
		case JsonArray:
			for ( ArrayVector::Iter i = *a; i.lte(); i++ )
				delete *i;
			a->empty();
			delete a;
			break;
		case JsonNumber:
		case JsonString:
			delete s;
			break;
		default:
			break;
	}
}

int *grow_stack( int *stack, int *psd )
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
	machine JSON_tree;

	write data;

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

	action parse_name { fhold; fcall name; }
	action parse_value { fhold; fcall value; }
	action parse_number { fhold; fcall number; }
	action parse_string { fhold; fcall string; }
	action parse_array  { fhold; fcall array; }
	action parse_object { fhold; fcall object; }
	action exit { fret; }
	action m { m = p; }
	action e { e = p; }

	number := 
		(
			'-'?
			( '0' | [1-9][0-9]* )
			( '.' @{dot=true;} [0-9]+ )? 
			( [Ee]@{E=true;} [+\-]? [0-9]+ )? 
		) >{ dot = false; E=false; } >m
		[^.Ee0-9] @{
			vt->type = Value::JsonNumber;
			char *last = p - 1;

			/* FIXME: trim for comparison to javascript only, can remove. */
			if ( dot && !E ) {
				while ( last > m && *last == '0' )
					last -= 1;
				if ( *last == '.' )
					last -= 1;
			}

			vt->s = new String( m, last - m + 1 );
			fhold; fret;
		};

	name :=
		'"' %m
		(
			^([\"\\] | 0..0x1f) |
			'\\'[\"\\/bfnrt] |
			'\\u'[0-9a-fA-F]{4} |
			'\\'^([\"\\/bfnrtu]|0..0x1f)
		)*
		'"' @e @exit;

	string :=
		'"' %m
		(
			^([\"\\] | 0..0x1f) |
			'\\'[\"\\/bfnrt] |
			'\\u'[0-9a-fA-F]{4} |
			'\\'^([\"\\/bfnrtu]|0..0x1f)
		)*
		'"' @{
			vt->type = Value::JsonString;
			vt->s = new String( m, p-m );
		} @exit;

	next_element =
		value_separator ignore* begin_value >{
			Value *n = new Value;
			vt->a->append( n );
			vs.append( vt = n );
		} >parse_value;

	array :=
		begin_array @{
			vt->type = Value::JsonArray;
			vt->a = new Vector<Value*>;
		}
		ignore*
		(
			begin_value >{
				Value *n = new Value;
				vt->a->append( n );
				vs.append( vt = n );
			} >parse_value
			ignore*
			( next_element ignore* )*
		)?
		end_array @exit;

	pair =
		ignore*
		begin_name >parse_name 
		ignore*
		name_separator >{
			Value::ObjectMapEl *el;
			vt->o->insert( String(m, e-m), &el );
			vs.append( vt = &el->value );
		}
		ignore* 
		begin_value >parse_value;

	object := 
		begin_object @{
			vt->type = Value::JsonObject;
			vt->o = new Value::ObjectMap;
		}
		(	
			pair
			(
				ignore*
				value_separator
				pair
			)*
		)?
		ignore*
		end_object @exit;

	# The vt is supplied to us by the caller (can come from different places,
	# arrays, objects, root, etc), but needs to be populated with data. When we
	# return, we pop it so the caller doesn't need to. */
	value :=
		ignore*
		(
			Vnull     @{ vt->type = Value::JsonNull; }     |
			Vfalse    @{ vt->type = Value::JsonFalse; }    |
			Vtrue     @{ vt->type = Value::JsonTrue; }     |
			VNaN      @{ vt->type = Value::JsonNaN; }      |
			VInfinity @{ vt->type = Value::JsonInfinity; } |
			begin_number >parse_number |
			begin_string >parse_string |
			begin_array  >parse_array |
			begin_object >parse_object
		)
		any @{
			vs.remove( vs.length() - 1, 1 );
			vt = vs.data[vs.length() - 1];
			fhold; fret;
		};

	main :=
		any @{
			vs.append(0); vs.append( vt = root );
			fhold; fcall value;
		} 0;

	prepush {
		if ( top == sd )
			stack = grow_stack( stack, &sd );
	}
}%%

std::ostream &operator <<( std::ostream &out, const Value &v )
{
	switch ( v.type ) {
		case Value::JsonObject:
			out << "{";
			for ( Value::ObjectMap::Iter i = *v.o; i.lte(); i++ ) {
				out << i->key << ":" << i->value;
				if ( !i.last() )
					out << ",";
			}
			out << "}";
			break;
		case Value::JsonArray:
			out << "[";
			for ( Value::ArrayVector::Iter i = *v.a; i.lte(); i++ ) {
				out << **i;
				if ( !i.last() )
					out << ",";
			}
			out << "]";
			break;
		case Value::JsonString:
			out << *v.s;
			break;
		case Value::JsonNumber:
			out << *v.s;
			break;
		case Value::JsonNull:
			out << "null";
			break;
		case Value::JsonFalse:
			out << "false";
			break;
		case Value::JsonTrue:
			out << "true";
			break;
		case Value::JsonNaN:
			out << "NaN";
			break;
		case Value::JsonInfinity:
			out << "Infinity";
			break;
	}
	return out;
}

Value *JSON_parse( char *data, int length )
{
	/* Assuming a null terminator. */
	char *p = data;
	char *pe = p + length + 1;
	char *m = 0, *e = 0;

	int cs = -1, top = 0, sd = 16;
	int *stack = new int[sd];

	bool dot, E;

	Value *root = new Value;
	Vector<Value*> vs;
	Value *vt;

	%% write init;
	%% write exec;

	delete[] stack;

	if ( cs < JSON_tree_first_final || p != pe ) {
		log_message( "JSON parse error" );
		return 0;
	}

	return root;
}
