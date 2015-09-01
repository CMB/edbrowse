/*********************************************************************
render.c: render the text buffer from the tree of nodes.
Also, in a separate call, spin off parallel js objects.
The js functionality may move to another file someday,
but as long as it's a separate call we're ok.
*********************************************************************/

#include "eb.h"

/* traverse the tree of nodes with a callback function */
typedef void (*nodeFunction) (struct htmlTag * node, bool opentag);
static nodeFunction traverse_callback;
/* possible callback functions */
static void renderNode(struct htmlTag *node, bool opentag);
static void jsNode(struct htmlTag *node, bool opentag);

static void traverseNode(struct htmlTag *node)
{
	struct htmlTag *child;
	(*traverse_callback) (node, true);
	for (child = node->firstchild; child; child = child->sibling)
		traverseNode(child);
	(*traverse_callback) (node, false);
}				/* traverseNode */

static void traverseAll(void)
{
	struct htmlTag *t;
	int i;

	for (i = 0; i < cw->numTags; ++i) {
		t = cw->tags[i];
		if (!t->parent)
			traverseNode(t);
	}
}				/* traverseAll */

/* the new string, the result of the render operation */
static char *ns;
int ns_l;

static void renderNode(struct htmlTag *t, bool opentag)
{
/* this is just playing around */
	switch (t->action) {
	case TAGACT_TEXT:
		if (opentag && t->textval)
			stringAndString(&ns, &ns_l, t->textval);
		break;
	}
}				/* renderNode */

char *render(void)
{
	ns = initString(&ns_l);
	traverse_callback = renderNode;
	traverseAll();
	return ns;
}				/* render */
