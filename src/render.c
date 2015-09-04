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
static bool invisible, tdfirst;
static int nopt;		/* number of options */
static int listnest;		/* count nested lists */
/* None of these tags nest, so it is reasonable to talk about
 * the current open tag. */
static struct htmlTag *currentForm, *currentSel, *currentOpt;
static struct htmlTag *currentTitle, *currentScript, *currentTA;
static struct htmlTag *currentA;
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

static bool attribPresent(struct htmlTag *t, const char *name)
{
	int j = stringInListCI(t->attributes, name);
	return (j >= 0);
}				/* attribPresent */

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

static struct htmlTag *findList(struct htmlTag *t)
{
	while (t = t->parent)
		if (t->action == TAGACT_OL || t->action == TAGACT_UL)
			return t;
	return 0;
}				/* findList */

static void formControl(struct htmlTag *t, bool namecheck)
{
	int itype = t->itype;
	char *myname = (t->name ? t->name : t->id);
	if (currentForm) {
		t->controller = currentForm;
	} else if (itype != INP_BUTTON)
		debugPrint(3, "%s is not part of a fill-out form",
			   t->info->desc);
	if (namecheck && !myname)
		debugPrint(3, "%s does not have a name", t->info->desc);
}				/* formControl */

static const char *const inp_types[] = {
	"reset", "button", "image", "submit",
	"hidden",
	"text", "password", "number", "file",
	"select", "textarea", "radio", "checkbox",
	0
};

/* helper function for input tag */
static void htmlInput(struct htmlTag *t)
{
	int n = INP_TEXT;
	int len;
	char *myname = (t->name ? t->name : t->id);
	const char *s = attribVal(t, "type");
	if (s) {
		n = stringInListCI(inp_types, s);
		if (n < 0) {
			debugPrint(3, "unrecognized input type %s", s);
			n = INP_TEXT;
		}
	} else if (stringEqual(t->info->name, "BUTTON")) {
		n = INP_BUTTON;
	}
	t->itype = n;

	s = attribVal(t, "maxlength");
	len = 0;
	if (s)
		len = stringIsNum(s);
	if (len > 0)
		t->lic = len;

/* In this case an empty value should be "", not null */
	if (t->value == 0)
		t->value = emptyString;

	if (n >= INP_RADIO && t->checked) {
		char namebuf[200];
		if (n == INP_RADIO && myname &&
		    radioCheck && strlen(myname) < sizeof(namebuf) - 3) {
			if (!*radioCheck)
				stringAndChar(&radioCheck, &radio_l, '|');
			sprintf(namebuf, "|%s|", t->name);
			if (strstr(radioCheck, namebuf)) {
				debugPrint(3,
					   "multiple radio buttons have been selected");
				return;
			}
			stringAndString(&radioCheck, &radio_l, namebuf + 1);
		}		/* radio name */
	}

	/* Even the submit fields can have a name, but they don't have to */
	formControl(t, (n > INP_SUBMIT));
}				/* htmlInput */

/* helper function for meta tag */
static void htmlMeta(struct htmlTag *t)
{
	char *name;
	const char *content, *heq;
	char **ptr;
	char *copy = 0;

	name = t->name;
	content = attribVal(t, "content");
	copy = cloneString(content);
	heq = attribVal(t, "http-equiv");

	if (heq && content) {
		bool rc;
		int delay;

/* It's not clear if we should process the http refresh command
 * immediately, the moment we spot it, or if we finish parsing
 * all the html first.
 * Does it matter?  It might.
 * A subsequent meta tag could use http-equiv to set a cooky,
 * and we won't see that cooky if we jump to the new page right now.
 * And there's no telling what subsequent javascript might do.
 * So I'm going to postpone the refresh until everything is parsed.
 * Bear in mind, we really don't want to refresh if we're working
 * on a local file. */

		if (stringEqualCI(heq, "Set-Cookie")) {
			rc = receiveCookie(cw->fileName, content);
			debugPrint(3, rc ? "jar" : "rejected");
		}

		if (allowRedirection && !browseLocal
		    && stringEqualCI(heq, "Refresh")) {
			if (parseRefresh(copy, &delay)) {
				char *newcontent;
				unpercentURL(copy);
				newcontent = resolveURL(cw->fileName, content);
				gotoLocation(newcontent, delay, true);
			}
		}
	}

	if (name) {
		ptr = 0;
		if (stringEqualCI(name, "description"))
			ptr = &cw->fd;
		if (stringEqualCI(name, "keywords"))
			ptr = &cw->fk;
		if (ptr && !*ptr && content) {
			stripWhite(copy);
			*ptr = copy;
			copy = 0;
		}
	}

	nzFree(copy);
}				/* htmlMeta */

static void renderNode(struct htmlTag *t, bool opentag)
{
	int tagno = t->seqno;
	char hnum[40];		/* hidden number */
#define ns_hnum() stringAndString(&ns, &ns_l, hnum)
#define ns_ic() stringAndChar(&ns, &ns_l, InternalCodeChar)
	int j, l;
	const struct tagInfo *ti = t->info;
	int action = t->action;
	char c;
	bool retainTag;
	const char *a;		/* usually an attribute */
	char *u;
	struct htmlTag *ltag;	/* list tag */

#if 0
	printf("rend %c%s\n", (opentag ? ' ' : '/'), ti->name);
#endif

	if (!opentag && ti->bits & TAG_NOSLASH)
		return;

	hnum[0] = 0;
	retainTag = true;
	if (invisible)
		retainTag = false;
	if (ti->bits & TAG_INVISIBLE) {
		retainTag = false;
		invisible = opentag;
/* special case for noscript with no js */
		if (stringEqual(ti->name, "NOSCRIPT") && !cw->jcx)
			invisible = false;
	}

	switch (action) {
	case TAGACT_TEXT:
		if (!t->textval)
			break;

		if (currentTitle) {
			if (!cw->ft)
				cw->ft = cloneString(t->textval);
			spaceCrunch(cw->ft, true, false);
			break;
		}

		if (currentOpt) {
			currentOpt->textval = cloneString(t->textval);
			spaceCrunch(currentOpt->textval, true, false);
			break;
		}

		if (currentScript) {
			currentScript->textval = cloneString(t->textval);
			break;
		}

		if (currentTA) {
			currentTA->value = cloneString(t->textval);
			break;
		}

		if (listnest && (ltag = findList(t)) && ltag->post) {
			char olbuf[20];
			if (ltag->action == TAGACT_OL) {
				j = ++ltag->lic;
				sprintf(olbuf, "%d. ", j);
			} else {
				strcpy(olbuf, "* ");
			}
			if (!invisible)
				stringAndString(&ns, &ns_l, olbuf);
			ltag->post = false;
		}

		if (!invisible)
			stringAndString(&ns, &ns_l, t->textval);
		break;

	case TAGACT_TITLE:
		currentTitle = (opentag ? t : 0);
		break;

	case TAGACT_SCRIPT:
		currentScript = (opentag ? t : 0);
		break;

	case TAGACT_A:
		currentA = (opentag ? t : 0);
		if (t->href && retainTag) {
			if (opentag) {
				sprintf(hnum, "%c%d{", InternalCodeChar, tagno);
			} else {
				sprintf(hnum, "%c0}", InternalCodeChar);
			}
			ns_hnum();
		}
		break;

	case TAGACT_OL:
/* look for start parameter for numbered list */
		if (opentag) {
			a = attribVal(t, "start");
			if (a && (j = stringIsNum(a)) >= 0)
				t->lic = j - 1;
		} else {
			t->lic = 0;
		}
	case TAGACT_UL:
		t->post = false;
		if (opentag)
			++listnest;
		else
			--listnest;
	case TAGACT_DL:
	case TAGACT_DT:
	case TAGACT_DD:
	case TAGACT_PRE:
	case TAGACT_DIV:
	case TAGACT_BR:
	case TAGACT_P:
	case TAGACT_BASE:
	case TAGACT_NOP:
nop:
		if (invisible)
			break;
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

	case TAGACT_INPUT:
		htmlInput(t);
		if (t->itype == INP_HIDDEN)
			break;
		if (!retainTag)
			break;
		t->retain = true;
		if (currentForm) {
			++currentForm->ninp;
			if (t->itype == INP_SUBMIT || t->itype == INP_IMAGE)
				currentForm->submitted = true;
		}
		sprintf(hnum, "%c%d<", InternalCodeChar, tagno);
		ns_hnum();
		if (t->itype < INP_RADIO) {
			if (t->value[0])
				stringAndString(&ns, &ns_l, t->value);
			else if (t->itype == INP_SUBMIT
				 || t->itype == INP_IMAGE)
				stringAndString(&ns, &ns_l, "Go");
			else if (t->itype == INP_RESET)
				stringAndString(&ns, &ns_l, "Reset");
		} else
			stringAndChar(&ns, &ns_l, (t->checked ? '+' : '-'));
		if (currentForm
		    && (t->itype == INP_SUBMIT || t->itype == INP_IMAGE)) {
			if (currentForm->secure)
				stringAndString(&ns, &ns_l, " secure");
			if (currentForm->bymail)
				stringAndString(&ns, &ns_l, " bymail");
		}
		ns_ic();
		stringAndString(&ns, &ns_l, "0>");
		break;

	case TAGACT_OPTION:
		if (!opentag) {
			currentOpt = 0;
			break;
		}

		if (!currentSel) {
			debugPrint(3,
				   "option appears outside a select statement");
			break;
		}

		currentOpt = t;
		t->controller = currentSel;
		t->lic = nopt++;
		if (attribPresent(t, "selected")) {
			if (currentSel->lic && !currentSel->multiple)
				debugPrint(3, "multiple options are selected");
			else {
				t->checked = t->rchecked = true;
				++currentSel->lic;
			}
		}
		break;

	case TAGACT_SELECT:
		if (!opentag) {
			if (currentSel)
				goto doneSelect;
			break;
		}
		currentSel = t;
		nopt = 0;
		t->itype = INP_SELECT;
		formControl(t, true);
		break;

	case TAGACT_LI:
		if ((ltag = findList(t)))
			ltag->post = true;
		goto nop;

	case TAGACT_HR:
		if (retainTag)
			stringAndString(&ns, &ns_l, "\r----------\r");
		break;

	case TAGACT_TA:
		if (opentag) {
			currentTA = t;
			t->itype = INP_TA;
			formControl(t, true);
		} else {
			currentTA->action = TAGACT_INPUT;
			if (retainTag) {
#if 0
				j = sideBuffer(0, currentTA->value, -1, 0);
#else
				j = 0;
#endif
				if (j) {
					currentTA->lic = j;
					sprintf(hnum, "%c%d<buffer %d%c0>",
						InternalCodeChar,
						currentTA->seqno, j,
						InternalCodeChar);
				} else
					strcpy(hnum, "<buffer ?>");
				ns_hnum();
			}
			currentTA = 0;
		}
		break;

	case TAGACT_META:
		htmlMeta(t);
		break;

	case TAGACT_TR:
		if (opentag)
			tdfirst = true;
	case TAGACT_TABLE:
		goto nop;

	case TAGACT_TD:
		if (tdfirst)
			tdfirst = false;
		else if (retainTag) {
			j = ns_l;
			while (j && ns[j - 1] == ' ')
				--j;
			ns[j] = 0;
			ns_l = j;
			stringAndChar(&ns, &ns_l, '|');
		}
		break;

	case TAGACT_SPAN:
		if (opentag) {
			if (!(a = t->classname))
				goto nop;
			if (stringEqualCI(a, "sup"))
				action = TAGACT_SUP;
			if (stringEqualCI(a, "sub"))
				action = TAGACT_SUB;
			if (stringEqualCI(a, "ovb"))
				action = TAGACT_OVB;
		} else if (t->subsup)
			action = t->subsup;
		if (action == TAGACT_SPAN)
			goto nop;
		t->subsup = action;
/* fall through */

/* This is strictly for rendering math pages written with my particular css.
* <span class=sup> becomes TAGACT_SUP, which means superscript.
* sub is subscript and ovb is overbar.
* Sorry to put my little quirks into this program, but hey,
* it's my program. */

	case TAGACT_SUP:
	case TAGACT_SUB:
	case TAGACT_OVB:
		if (!retainTag)
			break;
		t->retain = true;
		if (action == TAGACT_SUB)
			j = 1;
		if (action == TAGACT_SUP)
			j = 2;
		if (action == TAGACT_OVB)
			j = 3;

		if (opentag) {
			static const char *openstring[] = { 0,
				"[", "^(", "`"
			};
			t->lic = ns_l;
			stringAndString(&ns, &ns_l, openstring[j]);
			break;
		}

		if (j == 3) {
			stringAndChar(&ns, &ns_l, '\'');
			break;
		}

/* backup, and see if we can get rid of the parentheses or brackets */
		l = t->lic + j;
		u = ns + l;
		if (j == 2 && isalphaByte(u[0]) && !u[1])
			goto unparen;
		if (j == 2 && (stringEqual(u, "th") || stringEqual(u, "rd")
			       || stringEqual(u, "nd") || stringEqual(u, "st"))) {
			strmove(ns + l - 2, ns + l);
			ns_l -= 2;
			break;
		}
		while (isdigitByte(*u))
			++u;
		if (!*u)
			goto unparen;
		stringAndChar(&ns, &ns_l, (j == 2 ? ')' : ']'));
		break;

/* ok, we can trash the original ( or [ */
unparen:
		l = t->lic + j;
		strmove(ns + l - 1, ns + l);
		--ns_l;
		if (j == 2)
			stringAndChar(&ns, &ns_l, ' ');
		break;

	case TAGACT_AREA:
	case TAGACT_FRAME:
		if (!retainTag)
			break;
		stringAndString(&ns, &ns_l,
				(action == TAGACT_FRAME ? "\rFrame " : "\r"));
		a = 0;
		if (action == TAGACT_AREA)
			a = attribVal(t, "alt");
		u = (char *)a;
		if (!u) {
			u = t->name;
			if (!u)
				u = altText(t->href);
		}
		if (!u)
			u = (action == TAGACT_FRAME ? "???" : "area");
		if (t->href) {
			sprintf(hnum, "%c%d{", InternalCodeChar, tagno);
			ns_hnum();
			t->action = TAGACT_A;
		}
		if (t->href || action == TAGACT_FRAME)
			stringAndString(&ns, &ns_l, u);
		if (t->href) {
			ns_ic();
			stringAndString(&ns, &ns_l, "0}");
		}
		stringAndChar(&ns, &ns_l, '\r');
		break;

	case TAGACT_MUSIC:
		if (!retainTag)
			break;
		if (!t->href)
			break;
		sprintf(hnum, "\r%c%d{", InternalCodeChar, tagno);
		ns_hnum();
		stringAndString(&ns, &ns_l,
				(ti->name[0] ==
				 'B' ? "Background Music" : "Audio passage"));
		sprintf(hnum, "%c0}\r", InternalCodeChar);
		ns_hnum();
		t->action = TAGACT_A;
		break;

	case TAGACT_IMAGE:
		if (!currentA) {
			if (a = attribVal(t, "alt")) {
				u = altText(a);
				a = NULL;
				if (u && !invisible) {
					stringAndChar(&ns, &ns_l, '[');
					stringAndString(&ns, &ns_l, u);
					stringAndChar(&ns, &ns_l, ']');
				}
			}
			break;
		}

/* image part of a hyperlink */
		if (!retainTag || !currentA->href)
			break;
		u = 0;
		a = attribVal(t, "alt");
		if (a)
			u = altText(a);
		if (!u)
			u = altText(t->name);
		if (!u)
			u = altText(currentA->href);
		if (!u)
			u = altText(t->href);
		if (!u)
			u = "image";
		stringAndString(&ns, &ns_l, u);
		break;

	}			/* switch */
}				/* renderNode */

/* returns an allocated string */
char *render(int start)
{
	ns = initString(&ns_l);
	invisible = false;
	listnest = 0;
	currentForm = currentSel = currentOpt = 0;
	currentTitle = currentScript = currentTA = 0;
	currentA = 0;
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
