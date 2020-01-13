#include <genf/thread.h>
#include <sstream>
#include <fstream>
#include <iomanip>

#include "netp.h"
#include "itq_gen.h"

void ParseReport::dumpRoot( std::ostream &out, Node *node )
{
	for ( DList<Node>::Iter i = node->children; i.lte(); i++ ) {
		dumpTree( out, i );
		if ( !i.last() )
			out << ",";
	}
}

const char *nodeText( Node::Type type )
{
	switch ( type ) {
		case Node::Root: return "root";
		case Node::LangHttp: return "lang-http";
		case Node::LangJson: return "lang-json";
		case Node::LangHtml: return "lang-html";

		case Node::ConnTls: return "conn-tls";
		case Node::ConnHost: return "conn-host";

		case Node::HttpRequest: return "http-request";
		case Node::HttpHead: return "http-head";
		case Node::HttpResponse: return "http-response";
		case Node::HttpConnHost: return "http-conn-host";
		case Node::HttpMethod: return "http-method";
		case Node::HttpUri: return "http-uri";
		case Node::HttpProtocol: return "http-protocol";
		case Node::HttpHeader: return "http-header";
		case Node::HttpHeaderName: return "http-header-name";
		case Node::HttpHeaderValue: return "http-header-value";

		case Node::JsonObject: return "json-object";
		case Node::JsonArray: return "json-array";
		case Node::JsonField: return "json-field";
		case Node::JsonString: return "json-string";
		case Node::JsonNumber: return "json-number";
		case Node::JsonNull: return "json-null";
		case Node::JsonFalse: return "json-false";
		case Node::JsonTrue: return "json-true";
		case Node::JsonNaN: return "json-nan";
		case Node::JsonInfinity: return "json-infinity";

		case Node::HtmlTag: return "html-tag";
		case Node::HtmlText: return "html-text";
		case Node::HtmlAttr: return "html-attr";
		case Node::HtmlVal: return "html-val";

		case Node::DnsPacket:     return "dns-packet";
		case Node::DnsQuestion:   return "dns-question";
		case Node::DnsAnswer:     return "dns-answer";
		case Node::DnsAuthority:  return "dns-authority";
		case Node::DnsAdditional: return "dns-additional";
		case Node::DnsRrA:        return "dns-rr-a";
		case Node::DnsRrAAAA:     return "dns-rr-aaaa";
		case Node::DnsRrCname:    return "dns-rr-cname";

		case Node::DnsName:         return "dns-name";
		case Node::DnsAddr:         return "dns-addr";
		case Node::FileTypePe:      return "file-type-pe";
		case Node::FileTypeUnknown: return "file-type-unknown";
	}
	return "<null>";
}

void ParseReport::dumpTree( std::ostream &out, Node *node )
{
	out << nodeText( node->type );

	if ( node->text.length() > 0 )
		out << ':' << node->text;

	if ( node->children.length() > 0 )
	{
		out << "{";
		for ( DList<Node>::Iter i = node->children; i.lte(); i++ ) {
			dumpTree( out, i );
			if ( !i.last() )
				out << ",";
		}
		out << "}";
	}
}

int ParseReport::id = 0;

std::ofstream *ParseReport::open()
{
	std::stringstream ss;
	ss << "report-" << std::setfill('0') << std::setw(4) << id++; 
	return new std::ofstream( ss.str().c_str() );
}

void ParseReport::start()
{
	size = 0;
	errorReported = false;
}

void ParseReport::receive( Packet *packet, const wire_t *data, int len )
{
	if ( packet != 0 && conn.length() == 0 ) {
		std::stringstream ss;
		ss << packet->tcp.connection;
		conn = ss.str();
	}
	rope.append( (const char*)data, len );
	size += len;
}

void ParseReport::successReport( std::ostream &out, Node *root )
{
	out << "connection: " << conn << std::endl;
	out << "tree: ";
	if ( root != 0 )
		dumpRoot( out, root );
	out << std::endl;

	out << "input:" << std::endl;
	for ( RopeBlock *rb = rope.hblk; rb != 0; rb = rb->next )
		out.write( rope.data(rb), rope.length(rb) );
}

void ParseReport::finish( Node *root )
{
	if ( errorReported )
		return;

	std::ofstream *pout = open();
	successReport( *pout, root );
	pout->close();
	delete pout;
}

void ParseReport::errorReport( std::ostream &out, Node *root, int offset, int pc )
{
	long errat = size - offset;

	out << "connection: " << conn << std::endl;
	out << "offset: " << errat << std::endl;

	out << "tree: ";
	if ( root != 0 )
		dumpRoot( out, root );
	out << std::endl;

	out << "error on char: " << (int)pc << std::endl;
	out << "input:" << std::endl;

	long sofar = 0;
	RopeBlock *rb = rope.hblk;
	for ( ; rb != 0; rb = rb->next ) {
		long lower = sofar;
		long upper = sofar + rope.length(rb);
		if ( lower < errat && errat <= upper ) {
			long local = errat - sofar;
			out.write( rope.data(rb), local );
			out << std::endl << "failure at:" << std::endl;
			out.write( rope.data(rb) + local, rope.length(rb) - local );
		}
		else {
			out.write( rope.data(rb), rope.length(rb) );
		}
		sofar += rope.length(rb);
	}

	out << std::endl;
	errorReported = true;
}

void ParseReport::error( Node *root, int offset, int pc )
{
	std::ofstream *pout = open();
	errorReport( *pout, root, offset, pc );
	pout->close();
	delete pout;
}
