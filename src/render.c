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

static void traverseAll(int start)
{
	struct htmlTag *t;
	int i;

	for (i = start; i < cw->numTags; ++i) {
		t = cw->tags[i];
		if (!t->parent && !t->slash)
			traverseNode(t);
	}
}				/* traverseAll */

/* the new string, the result of the render operation */
static char *ns;
static int ns_l;
static bool invisible;
static struct htmlTag *currentForm, *currentSel;
static struct htmlTag *currentTitle, *currentScript;
static char *radioCheck;
static int radio_l;

static const char *attribVal(struct htmlTag *t, const char *name)
{
	const char *v;
	int j = stringInListCI(t->attributes, name);
	if (j < 0)
		return 0;
	v = t->atvals[j];
	if (!v || !*v)
		return 0;
	return v;
}				/* attribVal */

static void linkinTree(struct htmlTag *parent, struct htmlTag *child)
{
	struct htmlTag *c, *d;
	child->parent = parent;

	if (!parent->firstchild) {
		parent->firstchild = child;
		return;
	}

	for (c = parent->firstchild; c; c = c->sibling) {
		d = c;
	}
	d->sibling = child;
}				/* linkinTree */

static void makeButton(void)
{
	struct htmlTag *t = newTag("input");
	t->controller = currentForm;
	t->itype = INP_SUBMIT;
	t->created = true;
	linkinTree(currentForm, t);
}				/* makeButton */

static void renderNode(struct htmlTag *t, bool opentag)
{
	int tagno = t->seqno;
	char hnum[40];		/* hidden number */
#define ns_hnum() stringAndString(&ns, &ns_l, hnum)
#define ns_ic() stringAndChar(&ns, &ns_l, InternalCodeChar)
	int j;
	const struct tagInfo *ti = t->info;
	int action = t->action;
	char c;
	bool retainTag;
	const char *a;		/* usually an attribute */
	char *u;

	hnum[0] = 0;
	retainTag = true;
	if (ti->bits & TAG_INVISIBLE)
		retainTag = false;
	if (invisible)
		retainTag = false;
	if (ti->bits & TAG_INVISIBLE) {
		invisible = opentag;
/* special case for noscript with no js */
		if (stringEqual(ti->name, "NOSCRIPT") && !cw->jcx)
			invisible = false;
	}

	switch (action) {
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

	case TAGACT_BR:
	case TAGACT_P:
	case TAGACT_NOP:
nop:
		j = ti->para;
		if (opentag)
			j &= 3;
		else
			j >>= 2;
		if (!j)
			break;
		c = '\f';
		if (j == 1) {
			c = '\r';
			if (action == TAGACT_BR)
				c = '\n';
		}
		stringAndChar(&ns, &ns_l, c);
		break;

	case TAGACT_FORM:
		if (opentag) {
			currentForm = t;

			a = attribVal(t, "method");
			if (a) {
				if (stringEqualCI(a, "post"))
					t->post = true;
				else if (!stringEqualCI(a, "get"))
					debugPrint(3,
						   "form method should be get or post");
			}
			a = attribVal(t, "enctype");
			if (a) {
				if (stringEqualCI(a, "multipart/form-data"))
					t->mime = true;
				else if (!stringEqualCI
					 (a,
					  "application/x-www-form-urlencoded"))
					debugPrint(3,
						   "unrecognized enctype, plese use multipart/form-data or application/x-www-form-urlencoded");
			}
			if (a = t->href) {
				const char *prot = getProtURL(a);
				if (prot) {
					if (stringEqualCI(prot, "mailto"))
						t->bymail = true;
					else if (stringEqualCI
						 (prot, "javascript"))
						t->javapost = true;
					else if (stringEqualCI(prot, "https"))
						t->secure = true;
					else if (!stringEqualCI(prot, "http"))
						debugPrint(3,
							   "form cannot submit using protocol %s",
							   prot);
				}
			}

			nzFree(radioCheck);
			radioCheck = initString(&radio_l);
		}

		if (currentSel) {
doneSelect:
			currentSel->action = TAGACT_INPUT;
			if (currentSel->controller)
				++currentSel->controller->ninp;
			currentSel->value = u = displayOptions(currentSel);
			if (retainTag) {
				currentSel->retain = true;
/* Crank out the input tag */
				sprintf(hnum, "%c%d<", InternalCodeChar,
					currentSel->seqno);
				ns_hnum();
				stringAndString(&ns, &ns_l, u);
				ns_ic();
				stringAndString(&ns, &ns_l, "0>");
			}
			currentSel = 0;
		}

		if (action == TAGACT_FORM && !opentag && currentForm) {
			if (retainTag && currentForm->href
			    && !currentForm->submitted) {
				makeButton();
				sprintf(hnum, " %c%d<Go",
					InternalCodeChar, cw->numTags - 1);
				ns_hnum();
				if (currentForm->secure)
					stringAndString(&ns, &ns_l, " secure");
				if (currentForm->bymail)
					stringAndString(&ns, &ns_l, " bymail");
				stringAndString(&ns, &ns_l, " implicit");
				ns_ic();
				stringAndString(&ns, &ns_l, "0>");
			}
			currentForm = 0;
		}
		goto nop;

	}			/* switch */
}				/* renderNode */

/* returns an allocated string */
char *render(int start)
{
	ns = initString(&ns_l);
	invisible = false;
	currentForm = currentSel = 0;
	currentTitle = currentScript = 0;
	traverse_callback = renderNode;
	traverseAll(start);
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
