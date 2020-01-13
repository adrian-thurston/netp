#ifndef _AVL_H
#define _AVL_H

struct avl_el 
{
	struct avl_el *left, *right, *parent;
	struct avl_el *prev, *next;
	long height;
};

struct avl_tree
{
	struct avl_el *root;
	struct avl_el *head, *tail;
	long tree_size;
};

void avl_attach_rebal( struct avl_tree *tree, struct avl_el *element,
		struct avl_el *parentEl, struct avl_el *lastLess );

#define avl_declare(name, type) \
\
int name##_compare( struct type *el1, struct type *el2 ); \
\
inline struct type *name##_find( struct avl_tree *tree, struct type *element ) \
{ \
	struct avl_el *curEl = tree->root; \
	long keyRelation; \
 \
	while ( curEl != 0 ) { \
		keyRelation = name##_compare( element, container_of(curEl, struct type, el) ); \
 \
		/* Do we go left? */ \
		if ( keyRelation < 0 ) \
			curEl = curEl->left; \
		/* Do we go right? */ \
		else if ( keyRelation > 0 ) \
			curEl = curEl->right; \
		/* We have hit the target. */ \
		else { \
			return container_of(curEl, struct type, el); \
		} \
	} \
 \
	return 0; \
} \
 \
inline struct type *name##_insert( struct avl_tree *tree, struct type *element, struct type **lastFound ) \
{ \
	long keyRelation; \
	struct avl_el *curEl = tree->root, *parentEl = 0; \
	struct avl_el *lastLess = 0; \
 \
	while ( 1 ) { \
		if ( curEl == 0 ) { \
			/* We are at an external element and did not find the key we were \
			 * looking for. Attach underneath the leaf and rebalance. */ \
			avl_attach_rebal( tree, &element->el, parentEl, lastLess ); \
 \
			if ( lastFound != 0 ) \
				*lastFound = element; \
			return element; \
		} \
 \
		keyRelation = name##_compare( element, container_of(curEl, struct type, el) ); \
 \
		/* Do we go left? */ \
		if ( keyRelation < 0 ) { \
			parentEl = lastLess = curEl; \
			curEl = curEl->left; \
		} \
		/* Do we go right? */ \
		else if ( keyRelation > 0 ) { \
			parentEl = curEl; \
			curEl = curEl->right; \
		} \
		/* We have hit the target. */ \
		else { \
			if ( lastFound != 0 ) \
				*lastFound = container_of( curEl, struct type, el ); \
			return 0; \
		} \
	} \
} \

#endif
