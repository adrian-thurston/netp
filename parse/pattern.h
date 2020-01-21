#ifndef _NETP_PATTERN_H
#define _NETP_PATTERN_H

#include "parse.h"
#include <aapl/dlist.h>

struct PatNode
{
	PatNode( PatNode *parent, Node::Type type )
	:
		parent(parent),
		type(type),
		onMatch(0),
		onFailure(0),
		up(0),
		skip(false),
		once(false)
	{}

	PatNode *parent;
	Node::Type type;
	std::string text;

	DList<PatNode> children;
	PatNode *prev, *next;
	OnMatch *onMatch;
	OnMatch *onFailure;
	int up;
	bool skip;
	bool once;
};

PatNode *consPat1();

#endif /* _NETP_PATTERN_H */
