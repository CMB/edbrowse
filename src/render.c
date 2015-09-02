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
static int ns_l;
static bool invisible;
static struct htmlTag *currentForm, *currentSelect;
static struct htmlTag *currentTitle, *currentScript;

static void renderNode(struct htmlTag *t, bool opentag)
{
	int tagno = t->seqno;
	char hnum[40];		/* hidden number */
#define ns_hnum() stringAndString(&ns, &ns_l, hnum)
#define ns_ic() stringAndChar(&ns, &ns_l, InternalCodeChar)

	hnum[0] = 0;

	switch (t->action) {
	case TAGACT_TEXT:
		if (!opentag)
			break;
		if (!t->textval)
			break;
		if (currentTitle) {
			if (!cw->ft)
				cw->ft = cloneString(t->textval);
			spaceCrunch(cw->ft, true, false);
			break;
		}
		if (currentScript) {
			currentScript->textval = cloneString(t->textval);
			break;
		}
		if (!invisible && t->textval)
			stringAndString(&ns, &ns_l, t->textval);
		break;

	case TAGACT_TITLE:
		if (opentag)
			currentTitle = t;
		else
			currentTitle = 0;
		break;

	case TAGACT_SCRIPT:
		if (opentag)
			currentScript = t;
		else
			currentScript = 0;
		break;

	case TAGACT_A:
		if (t->href) {
			if (opentag) {
				sprintf(hnum, "%c%d{", InternalCodeChar, tagno);
			} else {
				sprintf(hnum, "%c0}", InternalCodeChar);
			}
			ns_hnum();
		}
		break;

	case TAGACT_BASE:
		if (!opentag)
			break;
		if (!t->href)
			break;
		nzFree(cw->hbase);
		cw->hbase = cloneString(t->href);
		break;
	}			/* switch */
}				/* renderNode */

/* returns an allocated string */
char *render(void)
{
	ns = initString(&ns_l);
	invisible = false;
	traverse_callback = renderNode;
	traverseAll();
	return ns;
}				/* render */

static void jsNode(struct htmlTag *t, bool opentag)
{
/* if js is not active then we should even be here, but just in case ... */
	if (!isJSAlive)
		return;

/* all the js variables are on the open tag */
	if (!opentag)
		return;

}				/* jsNode */
