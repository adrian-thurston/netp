#ifndef _JSON_H
#define _JSON_H

#include <aapl/vector.h>
#include <aapl/avlmap.h>
#include <aapl/astring.h>

struct Value
{
	enum Type {
		JsonObject = 1,
		JsonArray,
		JsonString,
		JsonNumber,
		JsonNull,
		JsonFalse,
		JsonTrue,
		JsonNaN,
		JsonInfinity
	};

	~Value();

	typedef AvlMap<String, Value, CmpStr> ObjectMap;
	typedef AvlMapEl<String, Value> ObjectMapEl;
	typedef Vector<Value*> ArrayVector;

	Type type;

	union {
		ArrayVector *a;
		ObjectMap   *o;
		String      *s;
	};

	static const String emptyString;
	static const String zeroString;
	static const Value null;

	const String &string() const
		{ return type == JsonString ? *s : emptyString; }
	
	const String &number() const
		{ return type == JsonNumber ? *s : zeroString; }

	const Value &obj(const char *key) const
	{
		if ( type == JsonObject ) {
			ObjectMapEl *el = o->find( String(key) );
			if ( el != 0 )
				return el->value;
		}

		/* Not an object, or not found. */
		return null;
	}

	const Value &arr(int i) const
	{
		if ( type == JsonArray ) {
			if ( i >= 0 && i < a->length() ) {
				return *a->data[i];
			}
		}

		/* Not an object, or not found. */
		return null;
	}

	int length() const
	{
		if ( type == JsonArray )
			return a->length();
		return 0;
	}
};

Value *JSON_parse( char *data, int length );
std::ostream &operator <<( std::ostream &out, const Value &v );

#endif

