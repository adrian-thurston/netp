#include "parse.h"
#include "pattern.h"
#include "itq_gen.h"
#include <genf/thread.h>
#include "packet_gen.h"

VPT::VPT()
:
	vptErrorOcccurred( false ),
	other(0)
{
	init();
}

void VPT::init()
{
	vt = 0;
	vs.append( new StackNode( 0 ) );
	root = vt = new StackNode( new Node( Node::Root ) );
}
	

void VPT::generatePair()
{
	static int id = 0;

	if ( other != 0 ) {
		NodePair *pair = new NodePair;
		pair->id = id++;
		pairSend = pair;
		other->pairRecv.append( pair );

		// log_message( "generating pair " << this << " " << pair->id );
	}
}

void VPT::consumePair()
{
	NodePair *pair = 0;
	if ( pairRecv.length() > 0 ) {
		pair = pairRecv.detachFirst();
		if ( pair->patTransfer != 0 )
			addPat( pair->patTransfer );
		//log_message( "pulling pair " << this << " " << pair->id );
	}
}

void VPT::pairPattern( PatNode *pattern )
{
	if ( pairSend != 0 ) {
		pairSend->patTransfer = pattern;
		//log_message( "adding pair pattern to " << pairSend->id );
	}
}

void VPT::pushBarrier()
{
	log_debug( DBG_VPT, "VPT push barrier" );

	barriers.append( topBarrier );
	topBarrier = 0;
}

void VPT::popBarrier()
{
	if ( barriers.length() == 0 )
		return;

	if ( topBarrier > 0 ) {
		log_debug( DBG_VPT, "VPT pop barrier: need to clear " << topBarrier << " nodes" );
		while ( topBarrier > 0 ) {
			pop( vt->node->type );
		}
	}

	topBarrier = barriers[barriers.length()-1];
	barriers.remove( barriers.length() - 1, 0 );
}

Node *VPT::push( Node::Type type )
{
	log_debug( DBG_VPT, "VPT push " << nodeText( type ) );

	/* Push operation. */
	StackNode *parent = vt;
	vs.append( vt );
	vt = new StackNode( new Node( type ) );
	parent->node->children.append( vt->node );

	/* Advance the active set. */
	for ( int i = 0; i < parent->active.length(); ) {
		PatNode *nextChild = parent->active[i].nextChild;
		
		if ( nextChild == 0 ) {
			log_debug( DBG_VPT, "VPT state keeping on child <NULL>" );
			i++;
		}
		else if ( nextChild->type == type ) {
			log_debug( DBG_VPT, "VPT state keeping on child type " << nodeText(type) );
			vt->active.append( PatState( nextChild, nextChild->children.head ) );

			if ( nextChild->skip ) {
				/* Need to keep both nextChild and nextChild->next in the set. */
				//parent->active.insert( i+1, PatState( parent->active[i].node, nextChild->next ) );
				i += 1;
			}
			else {
				parent->active.remove( i, 1 );
				//i++;
			}
		}
		else if ( nextChild->skip ) {
			i++;
		}
		else {
			log_debug( DBG_VPT, "VPT state dropping on child type " << nodeText(type) );
			if ( parent->active[i].node->onFailure );
				parent->active[i].node->onFailure->match( this, vt->node );
			parent->active.remove( i, 1 );
			//i++
		}
	}
	
	/* Add root items. */
	for ( int i = 0; i < patRoots.length(); ) {
		if ( patRoots[i]->type == vt->node->type ) {
			vt->active.append( PatState( patRoots[i], patRoots[i]->children.head ) );
			log_debug( DBG_VPT, "VPT state adding " << nodeText(type) );

			if ( patRoots[i]->once )
				patRoots.remove( i, 1 );
			else
				i += 1;
		}
		else
			i += 1;
	}

	topBarrier += 1;

	return vt->node;
}

void VPT::appendText( Node::Type type, std::string text )
{
	if ( vt->node->children.length() == 0 || vt->node->children.tail->type != type ) {
		push( type );
		pop( type );
	}

	vt->node->children.tail->text += text;
}

void VPT::appendChar( Node::Type type, char c )
{
	if ( vt->node->children.length() == 0 || vt->node->children.tail->type != type ) {
		push( type );
		pop( type );
	}

	vt->node->children.tail->text += c;
}

void VPT::setData( const unsigned char *data, int dlen )
{
	log_debug( DBG_VPT, "VPT set-data " );

	unsigned char *dest = (unsigned char*)malloc( dlen );
	memcpy( dest, data, dlen );

	vt->node->data = dest;
	vt->node->dlen = dlen;
}

void VPT::setText( std::string text )
{
	log_debug( DBG_VPT, "VPT set-text " << text );

	vt->node->text = text;

	for ( int i = 0; i < vt->active.length(); ) {
		if ( vt->active[i].node->text.size() > 0 && vt->active[i].node->text != text ) {
			if ( vt->active[i].node->onFailure != 0 )
				vt->active[i].node->onFailure->match( this, vt->node );
			vt->active.remove( i, 1 );
			log_debug( DBG_VPT, "VPT state dropping on text " <<
					nodeText( vt->active[i].node->type ) << " " << 
					vt->active[i].node->text << " " << text );
		}
		else {
			log_debug( DBG_VPT, "VPT state keeping on text " <<
					nodeText(vt->active[i].node->type ) << " " << text );
			i += 1;
		}
	}
}

void VPT::setConsumer( Consumer *consumer )
{
	vt->consumer = consumer;
}

Consumer *VPT::getConsumer()
{
	if ( vt->consumer != 0 )
		return vt->consumer;

	StackNode *sn = vs.tail;
	while ( sn != 0 ) {
		if ( sn->consumer != 0 )
			return sn->consumer;
		sn = sn->prev;
	}
	return 0;
}

Node *VPT::up( int n )
{
	if ( n < 0 )
		return 0;
	else if ( n == 0 )
		return vt->node;

	StackNode *sn = vs.tail;
	n -= 1;

	while ( n > 0 ) {
		sn = sn->prev;
		n -= 1;
	}
	return sn->node;
}

bool VPT::pop( Node::Type type )
{
	log_debug( DBG_VPT, "VPT pop  " << nodeText( type ) );

	if ( vt->node->type != type ) {
		log_ERROR( "VPT pop mismatch, wire popped: " <<
				nodeText(type) << " but tree has " << nodeText (vt->node->type)
				<< " " << ( vt->node->text.length() > 0 ? vt->node->text.c_str() : "" ) );

		vptErrorOcccurred = true;
		return false;
	}

	if ( topBarrier == 0 ) {
		log_ERROR( "VPT pop barrier: attempt to cross barrier" );
		vptErrorOcccurred = true;
		return false;
	}

	/* Collect matches to the appropriate match list. */
	for ( int i = 0; i < vt->active.length(); i++ ) {
		if ( vt->active[i].nextChild == 0 ) {
			if ( vt->active[i].node->onMatch != 0 ) {
				StackNode *where = vt;
				int up = vt->active[i].node->up;
				if ( up > 0 ) {
					up -= 1;
					where = vs.tail;
					while ( up > 0 && where != 0 ) {
						where = where->prev;
						up -= 1;
					}
				}
				if ( where != 0 )
					where->onMatchList.append( vt->active[i].node->onMatch );
			}
		}
	}

	/* Execute match functions. Take note of the return value and if it comes
	 * back false then we cannot keep the match going. */
	for ( int i = 0; i < vt->onMatchList.length(); ) {
		MatchResult result = vt->onMatchList[i]->match( this, vt->node );
		if ( result != MatchContinue ) {
			for ( int j = 0; j < vt->active.length(); ) {
				if ( vt->active[j].node->onMatch == vt->onMatchList[i] ) {
					if ( result == MatchFail && vt->active[j].node->onFailure != 0 )
						vt->active[j].node->onFailure->match( this, vt->node );
					vt->active.remove( j, 1 );
				}
				else
					j += 1;
				
			}
		}

		i += 1;
	}

	/* Move matches up to the parent. */
	for ( int i = 0; i < vt->active.length(); i++ ) {
		if ( vs.tail != 0 && vt->active[i].node->parent != 0 )
			vs.tail->active.append( PatState( vt->active[i].node->parent, vt->active[i].node->next ) );
	}
	
	vt = vs.tail; //data[vs.length() - 1].node;
	vs.detach( vs.tail );
	topBarrier -= 1;

	return true;
}

struct Tender
{
	Tender( std::string code, std::string title,
		std::string proponent, std::string type,
		std::string number, std::string announced,
		std::string deadline )
	: 
		code(code),
		title(title),
		proponent(proponent),
		type(type),
		number(number),
		announced(announced),
		deadline(deadline)
	{}

	std::string code;
	std::string title;
	std::string proponent;
	std::string type;
	std::string number;
	std::string announced;
	std::string deadline;

	Tender *prev, *next;
};

struct TenderSearch
{
	TenderSearch( Node *root, DList<Tender> &tenderList )
		: tenderList(tenderList) {}
	
	Node *root;
	DList<Tender> &tenderList;

	bool title_a( Node *html );
	void title( Node *html );
	void noticeSearchItem( Node *html );
	void noticeSearchResult( Node *html );

	void start();

	bool tag( Node *html, const char *is )
	{
		return html->type == Node::HtmlTag && html->text == is;
	}

	bool attr( Node *html, const char *contains )
	{
		for ( Node *child = html->children.head; child != 0; child = child->next ) {
			if ( child->type == Node::HtmlAttr ) {
				Node *value = child->children.head;
				if ( strstr( value->text.c_str(), contains ) != 0 )
					return true;
			}
		}
		return false;
	}

	int len( Node *html )
	{
		int count = 0;
		for ( Node *child = html->children.head; child != 0; child = child->next ) {
			if ( child->type == Node::HtmlTag )
				count += 1;
		}
		return count;
	}

	Node *child( Node *html, int i )
	{
		int count = 0;
		for ( Node *child = html->children.head; child != 0; child = child->next ) {
			if ( child->type == Node::HtmlTag ) {
				if ( count == i )
					return child;
				count += 1;
			}
		}
		return 0;
	}

	void text_r( std::string &ret, Node *html );
	std::string text( Node *html );
};

void TenderSearch::text_r( std::string &ret, Node *html )
{
	if ( html->type == Node::HtmlText ) {
		ret += html->text;
	}
	else {
		for ( Node *c = html->children.head; c != 0; c = c->next )
			text_r( ret, c );
	}
}

std::string TenderSearch::text( Node *html )
{
	std::string ret;
	text_r( ret, html );
	return ret;
}

bool TenderSearch::title_a( Node *html )
{
	if ( tag( html, "a" ) && attr( html, "/Notice/Details/" ) ) {
		return true;
	}
	return false;
}


void TenderSearch::title( Node *html )
{
	/* Title is second or third item in header. First item is img, always skip.
	 * Start at second skip until we find an <a> where href matches our
	 * pattern. */
	if ( len( html ) > 0 ) {
		Node *header = child( html, 0 );
		Node *title = 0;
		if ( len( header ) > 1 && title_a( child( header, 1 ) ) )
			title = child( header, 1 );
		else if ( len( header ) > 2 && title_a( child( header, 2 ) ) )
			title = child( header, 2 );
		if ( title != 0 ) {
			std::string title_t = text( title );

			Node *left_col = child( html, 1 );

			std::string proponent = "";
			if ( len(child(left_col,0)) > 0 )
				proponent = text(child( child( left_col, 0 ), 0 ) );

			std::string type = text(child(child(left_col,1),0));
			std::string number = text(child(child(left_col,2),0));

			/* Right col */
			Node *right_col = child(html,2);

			std::string code = "";
			std::string deadline = "";
			std::string announced = "";

			for ( Node *c = right_col->children.head; c != 0; c = c->next ) {
				if ( c->type == Node::HtmlTag ) {
					std::string it = text( c );
					if ( strstr( it.c_str(),  "kodas:" ) != 0 ) {
						const char *s = strchr( it.c_str(), ':' );
						if ( s != 0 && s[1] != 0 && s[2] != 0 )
							code = s + 2;
						else
							code = it;
					}
					else if ( strstr( it.c_str(), "terminas:" ) != 0 )
						deadline = text(child(c,0));
					else if ( strstr( it.c_str(), "Paskelbimo data:") != 0 ) {
						const char *s = strchr( it.c_str(), ':' );
						if ( s != 0 && s[1] != 0 && s[2] != 0 )
							announced = s + 2;
						else
							announced = it;
					}
				}
			}

			Tender *tender = new Tender( code, title_t, proponent, type, number, announced, deadline );
			tenderList.append( tender );
		}
	}
}

struct MatchCvpp
:
	public OnMatch
{
	virtual MatchResult match( VPT *vpt, Node *node )
	{
		log_message( "VPT MATCH: CVPP" );

		Node *result = node->children.head;
		DList<Tender> tenders;
		while ( result != 0 ) {
			if ( result->type == Node::HtmlTag && result->text == "div" ) {
				std::cerr << "  item: ";
				ParseReport report;
				report.dumpTree( std::cerr, result );
				std::cerr << std::endl;
				TenderSearch tenderSearch( result, tenders );
				tenderSearch.title( result );
			}

			result = result->next;
		}

		for ( Tender *tender = tenders.head; tender != 0; tender = tender->next ) {
			log_message( "located tender: " << std::endl <<
				"    " << tender->code << std::endl <<
				"    " << tender->title << std::endl <<
				"    " << tender->proponent << std::endl <<
				"    " << tender->type << std::endl <<
				"    " << tender->number << std::endl <<
				"    " << tender->announced << std::endl <<
				"    " << tender->deadline );
		}

		return MatchFinished;
	}
};

const Node Node::null( Node::JsonNull );
const String Node::emptyString("");
const String Node::zeroString("0");

const Node *Node::obj( const char *key ) const
{
	if ( type == JsonObject ) {
		for ( Node *child = children.head; child != 0; child = child->next ) {
			if ( child->type == Node::JsonField && child->text == std::string(key) )
				return child->children.head;
		}
	}

	/* Not an object, or not found. */
	return &null;
}

const Node *Node::arr(int i) const
{
	if ( type == JsonArray ) {
		int cn = 0;
		for ( Node *child = children.head; child != 0; child = child->next ) {
			if ( cn == i )
				return child;
			cn += 1;
		}
	}
	return &null;
}

PatNode *consPatCvpp()
{
	//if ( tag( html, "div" ) && attr( html, "class=\"notice-search-result\"" ) ) {
	//	log_message( "notice search result" );

	PatNode *attr, *value;

	PatNode *div = new PatNode( 0, Node::HtmlTag );
	div->text = ( "div" );

	{
		div->children.append( attr = new PatNode( div, Node::HtmlAttr ) );
		attr->text = ( "class" );
		{
			attr->children.append( value = new PatNode( attr, Node::HtmlVal ) );
			value->text = ( "\"notice-search-result\"" );
			value->onMatch = new MatchCvpp;
			value->up = 2;
		}
	}


	return div;
}

/* For collecting quotes, holdings, movers, etc. */
#if 0
PatNode *consPatHttp()
{
	PatNode *response = new PatNode( 0, Node::HttpRequest );
	response->onMatch = new MatchHttpRequest;
	return response;
}
#endif

Node *findChild( Node *node, Node::Type type )
{
	Node *child = node->children.head;
	while ( child != 0 ) {
		if ( child->type == type )
			return child;

		child = child->next;
	}

	return 0;
}
