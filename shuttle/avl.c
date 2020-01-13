#include "avl.h"

#define Element struct avl_el
#define BASE_EL(a) a

/* Finds the first element whose grandparent is unbalanced. */
struct avl_el *find_first_unbal_gp( struct avl_tree *tree, struct avl_el *element )
{
	long lheight, rheight, balanceProp;
	struct avl_el *gp;

	if ( element == 0 || element->BASE_EL(parent) == 0 ||
			element->BASE_EL(parent)->BASE_EL(parent) == 0 )
		return 0;
	
	/* Don't do anything if we we have no grandparent. */
	gp = element->BASE_EL(parent)->BASE_EL(parent);
	while ( gp != 0 )
	{
		lheight = gp->BASE_EL(left) ? gp->BASE_EL(left)->BASE_EL(height) : 0;
		rheight = gp->BASE_EL(right) ? gp->BASE_EL(right)->BASE_EL(height) : 0;
		balanceProp = lheight - rheight;

		if ( balanceProp < -1 || balanceProp > 1 )
			return element;

		element = element->BASE_EL(parent);
		gp = gp->BASE_EL(parent);
	}
	return 0;
}

/* Recalculates the heights of all the ancestors of element. */
void recalc_heights( struct avl_tree *tree, struct avl_el *element )
{
	long lheight, rheight, new_height;
	while ( element != 0 )
	{
		lheight = element->left ? element->left->height : 0;
		rheight = element->right ? element->right->height : 0;

		new_height = (lheight > rheight ? lheight : rheight) + 1;

		/* If there is no chage in the height, then there will be no
		 * change in any of the ancestor's height. We can stop going up.
		 * If there was a change, continue upward. */
		if ( new_height == element->height )
			return;
		else
			element->height = new_height;

		element = element->parent;
	}
}

/* rebalance from a element whose gradparent is unbalanced. Only
 * call on a element that has a grandparent. */
Element *rebalance( struct avl_tree *tree, Element *n )
{
	long lheight, rheight;
	Element *a, *b, *c;
	Element *t1, *t2, *t3, *t4;

	Element *p = n->BASE_EL(parent);      /* parent (Non-NUL). L*/
	Element *gp = p->BASE_EL(parent);     /* Grand-parent (Non-NULL). */
	Element *ggp = gp->BASE_EL(parent);   /* Great grand-parent (may be NULL). */

	if (gp->BASE_EL(right) == p)
	{
		/*  gp
		 *   \
		 *    p
		 */
		if (p->BASE_EL(right) == n)
		{
			/*  gp
			 *   \
			 *    p
			 *     \
			 *      n
			 */
			a = gp;
			b = p;
			c = n;
			t1 = gp->BASE_EL(left);
			t2 = p->BASE_EL(left);
			t3 = n->BASE_EL(left);
			t4 = n->BASE_EL(right);
		}
		else
		{
			/*  gp
			 *     \
			 *       p
			 *      /
			 *     n
			 */
			a = gp;
			b = n;
			c = p;
			t1 = gp->BASE_EL(left);
			t2 = n->BASE_EL(left);
			t3 = n->BASE_EL(right);
			t4 = p->BASE_EL(right);
		}
	}
	else
	{
		/*    gp
		 *   /
		 *  p
		 */
		if (p->BASE_EL(right) == n)
		{
			/*      gp
			 *    /
			 *  p
			 *   \
			 *    n
			 */
			a = p;
			b = n;
			c = gp;
			t1 = p->BASE_EL(left);
			t2 = n->BASE_EL(left);
			t3 = n->BASE_EL(right);
			t4 = gp->BASE_EL(right);
		}
		else
		{
			/*      gp
			 *     /
			 *    p
			 *   /
			 *  n
			 */
			a = n;
			b = p;
			c = gp;
			t1 = n->BASE_EL(left);
			t2 = n->BASE_EL(right);
			t3 = p->BASE_EL(right);
			t4 = gp->BASE_EL(right);
		}
	}

	/* Perform rotation.
	 */

	/* Tie b to the great grandparent. */
	if ( ggp == 0 )
		tree->root = b;
	else if ( ggp->BASE_EL(left) == gp )
		ggp->BASE_EL(left) = b;
	else
		ggp->BASE_EL(right) = b;
	b->BASE_EL(parent) = ggp;

	/* Tie a as a leftchild of b. */
	b->BASE_EL(left) = a;
	a->BASE_EL(parent) = b;

	/* Tie c as a rightchild of b. */
	b->BASE_EL(right) = c;
	c->BASE_EL(parent) = b;

	/* Tie t1 as a leftchild of a. */
	a->BASE_EL(left) = t1;
	if ( t1 != 0 ) t1->BASE_EL(parent) = a;

	/* Tie t2 as a rightchild of a. */
	a->BASE_EL(right) = t2;
	if ( t2 != 0 ) t2->BASE_EL(parent) = a;

	/* Tie t3 as a leftchild of c. */
	c->BASE_EL(left) = t3;
	if ( t3 != 0 ) t3->BASE_EL(parent) = c;

	/* Tie t4 as a rightchild of c. */
	c->BASE_EL(right) = t4;
	if ( t4 != 0 ) t4->BASE_EL(parent) = c;

	/* The heights are all recalculated manualy and the great
	 * grand-parent is passed to recalcHeights() to ensure
	 * the heights are correct up the tree.
	 *
	 * Note that recalcHeights() cuts out when it comes across
	 * a height that hasn't changed.
	 */

	/* Fix height of a. */
	lheight = a->BASE_EL(left) ? a->BASE_EL(left)->BASE_EL(height) : 0;
	rheight = a->BASE_EL(right) ? a->BASE_EL(right)->BASE_EL(height) : 0;
	a->BASE_EL(height) = (lheight > rheight ? lheight : rheight) + 1;

	/* Fix height of c. */
	lheight = c->BASE_EL(left) ? c->BASE_EL(left)->BASE_EL(height) : 0;
	rheight = c->BASE_EL(right) ? c->BASE_EL(right)->BASE_EL(height) : 0;
	c->BASE_EL(height) = (lheight > rheight ? lheight : rheight) + 1;

	/* Fix height of b. */
	lheight = a->BASE_EL(height);
	rheight = c->BASE_EL(height);
	b->BASE_EL(height) = (lheight > rheight ? lheight : rheight) + 1;

	/* Fix height of b's parents. */
	recalc_heights( tree, ggp );
	return ggp;
}

void avl_attach_rebal( struct avl_tree *tree, struct avl_el *element,
		struct avl_el *parentEl, struct avl_el *lastLess )
{
	struct avl_el *ub;

	/* Increment the number of element in the tree. */
	tree->tree_size += 1;

	/* Set element's parent. */
	element->parent = parentEl;

	/* New element always starts as a leaf with height 1. */
	element->left = 0;
	element->right = 0;
	element->height = 1;

	/* Are we inserting in the tree somewhere? */
	if ( parentEl != 0 ) {
		/* We have a parent so we are somewhere in the tree. If the parent
		 * equals lastLess, then the last traversal in the insertion went
		 * left, otherwise it went right. */
		if ( lastLess == parentEl ) {
			parentEl->BASE_EL(left) = element;
#ifdef WALKABLE
			BASELIST::addBefore( parentEl, element );
#endif
		}
		else {
			parentEl->BASE_EL(right) = element;
#ifdef WALKABLE
			BASELIST::addAfter( parentEl, element );
#endif
		}

#ifndef WALKABLE
		/* Maintain the first and last pointers. */
		if ( tree->head->BASE_EL(left) == element )
			tree->head = element;

		/* Maintain the first and last pointers. */
		if ( tree->tail->BASE_EL(right) == element )
			tree->tail = element;
#endif
	}
	else {
		/* No parent element so we are inserting the root. */
		tree->root = element;
#ifdef WALKABLE
		BASELIST::addAfter( BASELIST::tail, element );
#else
		tree->head = tree->tail = element;
#endif

	}

	/* Recalculate the heights. */
	recalc_heights( tree, parentEl );

	/* Find the first unbalance. */
	ub = find_first_unbal_gp( tree, element );

	/* rebalance. */
	if ( ub != 0 )
	{
		/* We assert that after this single rotation the 
		 * tree is now properly balanced. */
		rebalance( tree, ub );
	}
}

