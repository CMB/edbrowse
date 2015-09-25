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
static void prerenderNode(struct htmlTag *node, bool opentag);
static void jsNode(struct htmlTag *node, bool opentag);
static void renderNode(struct htmlTag *node, bool opentag);

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
		t = tagList[i];
		if (!t->parent && !t->slash && t->step < 10)
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

const char *attribVal(const struct htmlTag *t, const char *name)
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

static bool attribPresent(const struct htmlTag *t, const char *name)
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
	t->value = emptyString;
	t->created = true;
	linkinTree(currentForm, t);
}				/* makeButton */

static struct htmlTag *findOpenTag(struct htmlTag *t, int action)
{
	while (t = t->parent)
		if (t->action == action)
			return t;
	return 0;
}				/* findOpenTag */

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
		t->rvalue = t->value = emptyString;

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
				newcontent = resolveURL(cw->hbase, copy);
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

static void prerenderNode(struct htmlTag *t, bool opentag)
{
	int itype;		/* input type */
	int j;
	int action = t->action;
	const char *a;		/* usually an attribute */

#if 0
	printf("prend %c%s\n", (opentag ? ' ' : '/'), t->info->name);
#endif

	if (t->step >= 1)
		return;
	if (!opentag)
		t->step = 1;

	switch (action) {
	case TAGACT_TEXT:
		if (!opentag || !t->textval)
			break;

		if (currentTitle) {
			if (!cw->ft) {
				cw->ft = cloneString(t->textval);
				spaceCrunch(cw->ft, true, false);
			}
			t->deleted = true;
			break;
		}

		if (currentOpt) {
			currentOpt->textval = cloneString(t->textval);
			spaceCrunch(currentOpt->textval, true, false);
			t->deleted = true;
			break;
		}

		if (currentScript) {
			currentScript->textval = cloneString(t->textval);
			t->deleted = true;
			break;
		}

		if (currentTA) {
			currentTA->value = cloneString(t->textval);
/* Sometimes tidy lops off the last newline character; it depends on
 * the tag following. And even if it didn't end in nl in the original html,
 * <textarea>foobar</textarea>, it probably should,
 * as it goes into a new buffer. */
			j = strlen(currentTA->value);
			if (j && currentTA->value[j - 1] != '\n') {
				currentTA->value =
				    reallocMem(currentTA->value, j + 2);
				currentTA->value[j] = '\n';
				currentTA->value[j + 1] = 0;
			}
			currentTA->rvalue = cloneString(currentTA->value);
			t->deleted = true;
			break;
		}

/* text is on the page */
		if (currentA) {
			char *s;
			for (s = t->textval; *s; ++s)
				if (isalnumByte(*s)) {
					currentA->textin = true;
					break;
				}
		}
		break;

	case TAGACT_TITLE:
		currentTitle = (opentag ? t : 0);
		break;

	case TAGACT_SCRIPT:
		currentScript = (opentag ? t : 0);
		break;

	case TAGACT_A:
		currentA = (opentag ? t : 0);
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
		if (!opentag && currentForm) {
			if (t->href && !t->submitted) {
				makeButton();
				t->submitted = true;
			}
			currentForm = 0;
		}
		break;

	case TAGACT_INPUT:
		if (!opentag)
			break;
		htmlInput(t);
		itype = t->itype;
		if (itype == INP_HIDDEN)
			break;
		if (currentForm) {
			++currentForm->ninp;
			if (itype == INP_SUBMIT || itype == INP_IMAGE)
				currentForm->submitted = true;
			if (itype == INP_BUTTON && t->onclick)
				currentForm->submitted = true;
			if (itype > INP_HIDDEN && itype <= INP_SELECT
			    && t->onchange)
				currentForm->submitted = true;
		}
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
		if (opentag) {
			currentSel = t;
			nopt = 0;
			t->itype = INP_SELECT;
			formControl(t, true);
		} else {
			currentSel = 0;
			t->action = TAGACT_INPUT;
			t->value = displayOptions(t);
		}
		break;

	case TAGACT_TA:
		if (opentag) {
			currentTA = t;
			t->itype = INP_TA;
			formControl(t, true);
		} else {
			t->action = TAGACT_INPUT;
			if (!t->value) {
/* This can only happen it no text inside, <textarea></textarea> */
/* like the other value fields, it can't be null */
				t->rvalue = t->value = emptyString;
			}
			j = sideBuffer(0, currentTA->value, -1, 0);
			t->lic = j;
			currentTA = 0;
		}
		break;

	case TAGACT_META:
		if (opentag)
			htmlMeta(t);
		break;

	case TAGACT_TR:
		if (opentag)
			t->controller = findOpenTag(t, TAGACT_TABLE);
		break;

	case TAGACT_TD:
		if (opentag)
			t->controller = findOpenTag(t, TAGACT_TR);
		break;

	case TAGACT_SPAN:
		if (!opentag)
			break;
		if (!(a = t->classname))
			break;
		if (stringEqualCI(a, "sup"))
			action = TAGACT_SUP;
		if (stringEqualCI(a, "sub"))
			action = TAGACT_SUB;
		if (stringEqualCI(a, "ovb"))
			action = TAGACT_OVB;
		t->action = action;
		break;

	case TAGACT_OL:
/* look for start parameter for numbered list */
		if (opentag) {
			a = attribVal(t, "start");
			if (a && (j = stringIsNum(a)) >= 0)
				t->slic = j - 1;
		}
		break;

	}			/* switch */
}				/* prerenderNode */

void prerender(int start)
{
	currentForm = currentSel = currentOpt = NULL;
	currentTitle = currentScript = currentTA = NULL;
	traverse_callback = prerenderNode;
	traverseAll(start);
}				/* prerender */

static struct htmlTag *findList(struct htmlTag *t)
{
	while (t = t->parent)
		if (t->action == TAGACT_OL || t->action == TAGACT_UL)
			return t;
	return 0;
}				/* findList */

static void tagInStream(int tagno)
{
	char buf[8];
	sprintf(buf, "%c%d*", InternalCodeChar, tagno);
	stringAndString(&ns, &ns_l, buf);
}				/* tagInStream */

/* see if a number or star is pending, waiting to be printed */
static void liCheck(struct htmlTag *t)
{
	struct htmlTag *ltag;	/* the list tag */
	if (listnest && (ltag = findList(t)) && ltag->post) {
		char olbuf[32];
		if (ltag->ninp)
			tagInStream(ltag->ninp);
		if (ltag->action == TAGACT_OL) {
			int j = ++ltag->lic;
			sprintf(olbuf, "%d. ", j);
		} else {
			strcpy(olbuf, "* ");
		}
		if (!invisible)
			stringAndString(&ns, &ns_l, olbuf);
		ltag->post = false;
	}
}				/* liCheck */

static struct htmlTag *deltag;

static void renderNode(struct htmlTag *t, bool opentag)
{
	int tagno = t->seqno;
	char hnum[40];		/* hidden number */
#define ns_hnum() stringAndString(&ns, &ns_l, hnum)
#define ns_ic() stringAndChar(&ns, &ns_l, InternalCodeChar)
	int j, l;
	int itype;		/* input type */
	const struct tagInfo *ti = t->info;
	int action = t->action;
	char c;
	bool retainTag;
	const char *a;		/* usually an attribute */
	char *u;
	struct htmlTag *ltag;	/* list tag */

#if 0
	printf("rend %c%s\n", (opentag ? ' ' : '/'), t->info->name);
#endif

	if (deltag) {
		if (t == deltag && !opentag)
			deltag = 0;
li_hide:
/* we can skate past the li tag, but still need to increment the count */
		if (action == TAGACT_LI && opentag &&
		    (ltag = findList(t)) && ltag->action == TAGACT_OL)
			++ltag->lic;
		return;
	}
	if (t->deleted) {
		deltag = t;
		goto li_hide;
	}

	if (!opentag && ti->bits & TAG_NOSLASH)
		return;

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
		if (!t->textval && t->jv) {
/* A text node from html should always contain a string. But if this node
 * is created by document.createTextNode(), the string is
 * down in the member "data". */
			t->textval = get_property_string(t->jv, "data");
/* Unfortunately this does not reflect subsequent changes to TextNode.data.
 * either we query js every time, on every piece of text,
 * or we include a setter so that TextNode.data assignment has a side effect. */
		}
		if (!t->textval)
			break;
		liCheck(t);
		if (!invisible) {
			tagInStream(tagno);
			stringAndString(&ns, &ns_l, t->textval);
		}
		break;

	case TAGACT_A:
		liCheck(t);
		currentA = (opentag ? t : 0);
		if (!retainTag)
			break;
		if (t->href) {
			if (opentag)
				sprintf(hnum, "%c%d{", InternalCodeChar, tagno);
			else
				sprintf(hnum, "%c0}", InternalCodeChar);
		} else {
			if (opentag)
				sprintf(hnum, "%c%d*", InternalCodeChar, tagno);
			else
				hnum[0] = 0;
		}
		ns_hnum();
		break;

	case TAGACT_OL:
	case TAGACT_UL:
		t->lic = t->slic;
		t->post = false;
		if (opentag)
			++listnest;
		else
			--listnest;
	case TAGACT_DL:
	case TAGACT_DT:
	case TAGACT_DD:
	case TAGACT_DIV:
	case TAGACT_BR:
	case TAGACT_P:
	case TAGACT_SPAN:
	case TAGACT_NOP:
nop:
		if (invisible)
			break;
		j = ti->para;
		if (opentag)
			j &= 3;
		else
			j >>= 2;
		if (j) {
			c = '\f';
			if (j == 1) {
				c = '\r';
				if (action == TAGACT_BR)
					c = '\n';
			}
			stringAndChar(&ns, &ns_l, c);
		}
/* tags with id= have to be part of the screen, so you can jump to them */
		if (t->id && opentag && action != TAGACT_LI)
			tagInStream(tagno);
		break;

	case TAGACT_PRE:
		if (!retainTag)
			break;
/* one of those rare moments when I really need </tag> in the text stream */
		j = (opentag ? tagno : t->balance->seqno);
/* I need to manage the paragraph breaks here, rather than t->info->para,
 * which would rule if I simply redirected to nop.
 * But the order is wrong if I do that. */
		if (opentag)
			stringAndChar(&ns, &ns_l, '\f');
		sprintf(hnum, "%c%d*", InternalCodeChar, j);
		ns_hnum();
		if (!opentag)
			stringAndChar(&ns, &ns_l, '\f');
		break;

	case TAGACT_FORM:
		currentForm = (opentag ? t : 0);
		goto nop;

	case TAGACT_INPUT:
		if (!opentag)
			break;
		itype = t->itype;
		if (itype == INP_HIDDEN)
			break;
		if (!retainTag)
			break;
		liCheck(t);
		if (itype == INP_TA) {
			j = t->lic;
			if (j)
				sprintf(hnum, "%c%d<buffer %d%c0>",
					InternalCodeChar, t->seqno, j,
					InternalCodeChar);
			else
				strcpy(hnum, "<buffer ?>");
			ns_hnum();
			break;
		}
		sprintf(hnum, "%c%d<", InternalCodeChar, tagno);
		ns_hnum();
		if (itype < INP_RADIO) {
			if (t->value[0])
				stringAndString(&ns, &ns_l, t->value);
			else if (itype == INP_SUBMIT || itype == INP_IMAGE)
				stringAndString(&ns, &ns_l, "Go");
			else if (itype == INP_RESET)
				stringAndString(&ns, &ns_l, "Reset");
		} else
			stringAndChar(&ns, &ns_l, (t->checked ? '+' : '-'));
		if (currentForm && (itype == INP_SUBMIT || itype == INP_IMAGE)) {
			if (t->created)
				stringAndString(&ns, &ns_l, " implicit");
			if (currentForm->secure)
				stringAndString(&ns, &ns_l, " secure");
			if (currentForm->bymail)
				stringAndString(&ns, &ns_l, " bymail");
		}
		ns_ic();
		stringAndString(&ns, &ns_l, "0>");
		break;

	case TAGACT_LI:
		if ((ltag = findList(t))) {
			ltag->post = true;
/* borrow ninp to store the tag number of <li> */
			ltag->ninp = t->seqno;
		}
		goto nop;

	case TAGACT_HR:
		liCheck(t);
		if (retainTag) {
			tagInStream(tagno);
			stringAndString(&ns, &ns_l, "\r----------\r");
		}
		break;

	case TAGACT_TR:
		if (opentag)
			tdfirst = true;
	case TAGACT_TABLE:
		goto nop;

	case TAGACT_TD:
		if (!retainTag)
			break;
		if (tdfirst)
			tdfirst = false;
		else {
			liCheck(t);
			j = ns_l;
			while (j && ns[j - 1] == ' ')
				--j;
			ns[j] = 0;
			ns_l = j;
			stringAndChar(&ns, &ns_l, TableCellChar);
		}
		tagInStream(tagno);
		break;

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
			liCheck(t);
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
/* skip past <span> tag indicator */
		if (*u == InternalCodeChar) {
			++u;
			while (isdigit(*u))
				++u;
			++u;
		}
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
unparen:
/* ok, we can trash the original ( or [ */
		l = t->lic + j;
		strmove(ns + l - 1, ns + l);
		--ns_l;
		if (j == 2)
			stringAndChar(&ns, &ns_l, ' ');
		break;

	case TAGACT_AREA:
	case TAGACT_FRAME:
		liCheck(t);
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
		liCheck(t);
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
		break;

	case TAGACT_IMAGE:
		liCheck(t);
		tagInStream(tagno);
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
/* image is part of a hyperlink */
		if (!retainTag || !currentA->href || currentA->textin)
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
	currentForm = currentSel = currentOpt = NULL;
	currentTA = NULL;
	currentA = NULL;
	traverse_callback = renderNode;
	traverseAll(start);
	return ns;
}				/* render */

/*********************************************************************
The following code creates parallel js objects for the nodes in our html tree.
This follows the template of the above: traverse the tree,
callback() on each node, switch on the node type,
and create the corresponding js object or objects.
It's here because the code is similar to the above,
but it is semantically quite different.
It doesn't even have to run if js is off.
And it only runs at the start of browse, whereas render can be called
again and again as the running js makes changes to the tree.
We may want to move this js code to another file some day,
but for now it's here.
*********************************************************************/

static void set_onhandler(const struct htmlTag *t, const char *name)
{
	const char *s;
	if ((s = attribVal(t, name)) && t->jv && isJSAlive)
		handlerSet(t->jv, name, s);
}				/* set_onhandler */

static void set_onhandlers(const struct htmlTag *t)
{
/* I don't do anything with onkeypress, onfocus, etc,
 * these are just the most common handlers */
	if (t->onclick)
		set_onhandler(t, "onclick");
	if (t->onchange)
		set_onhandler(t, "onchange");
	if (t->onsubmit)
		set_onhandler(t, "onsubmit");
	if (t->onreset)
		set_onhandler(t, "onreset");
	if (t->onload)
		set_onhandler(t, "onload");
	if (t->onunload)
		set_onhandler(t, "onunload");
}				/* set_onhandlers */

static const char defvl[] = "defaultValue";
static const char defck[] = "defaultChecked";
static const char defsel[] = "defaultSelected";

static void formControlJS(struct htmlTag *t)
{
	const char *typedesc;
	int itype = t->itype;
	int isradio = itype == INP_RADIO;
	int isselect = (itype == INP_SELECT) * 2;
	char *myname = (t->name ? t->name : t->id);
	const struct htmlTag *form = t->controller;

	if (!isJSAlive)
		return;

	if (form && form->jv)
		domLink(t, "Element", 0, "elements", form->jv,
			isradio | isselect);
	else
		domLink(t, "Element", 0, 0, cw->docobj, isradio | isselect);
	if (!t->jv)
		return;

	set_onhandlers(t);

	if (itype <= INP_RADIO) {
		set_property_string(t->jv, "value", t->value);
		if (itype != INP_FILE) {
/* No default value on file, for security reasons */
			set_property_string(t->jv, defvl, t->value);
		}		/* not file */
	}

	if (isselect)
		typedesc = t->multiple ? "select-multiple" : "select-one";
	else
		typedesc = inp_types[itype];
	set_property_string(t->jv, "type", typedesc);

	if (itype >= INP_RADIO) {
		set_property_bool(t->jv, "checked", t->checked);
		set_property_bool(t->jv, defck, t->checked);
	}
}				/* formControlJS */

static void optionJS(struct htmlTag *t)
{
	struct htmlTag *sel = t->controller;
	const char *tx = t->textval;

	if (!sel)
		return;

	if (!tx) {
		debugPrint(3, "empty option");
	} else {
		if (!t->value)
			t->value = cloneString(tx);
	}

/* no point if the controlling select doesn't have a js object */
	if (!sel->jv)
		return;

	t->jv = establish_js_option(sel->jv, t->lic);
	set_property_string(t->jv, "text", t->textval);
	set_property_string(t->jv, "value", t->value);
	set_property_string(t->jv, "nodeName", "option");
	set_property_bool(t->jv, "selected", t->checked);
	set_property_bool(t->jv, defsel, t->checked);

	if (t->checked && !sel->multiple) {
		set_property_number(sel->jv, "selectedIndex", t->lic);
		set_property_string(sel->jv, "value", t->value);
	}
}				/* optionJS */

/* helper function to prepare an html script.
 * Fetch from the internetif src=url.
 * Some day we'll do these fetches in parallel in the background. */
static void prepareScript(struct htmlTag *t)
{
	const char *js_file = "generated";
	char *js_text = 0;
	const char *a;
	const char *filepart;

/* no need to fetch if no js */
	if (!isJSAlive)
		return;

/* Create the script object. */
	domLink(t, "Script", "src", "scripts", cw->docobj, 0);

	a = attribVal(t, "type");
	if (a)
		set_property_string(t->jv, "type", a);
	a = attribVal(t, "language");
	if (a)
		set_property_string(t->jv, "language", a);
/* if the above calls failed */
	if (!isJSAlive)
		return;

/* If no language is specified, javascript is default. */
	if (a && (!memEqualCI(a, "javascript", 10) || isalphaByte(a[10])))
		return;

/* It's javascript, run with the source or the inline text.
 * As per the starting line number, we cant distinguish between
 * <script> foo </script>  and
 * <script>
 * foo
 * </script>
 * so make a guess towards the first form, knowing we could be off by 1.
 * Just leave it at t->js_ln */
	if (cw->fileName && !htmlGenerated)
		js_file = cw->fileName;

	if (t->href) {		/* fetch the javascript page */
		if (javaOK(t->href)) {
			bool from_data = isDataURI(t->href);
			debugPrint(3, "java source %s",
				   !from_data ? t->href : "data URI");
			if (from_data) {
				char *mediatype;
				int data_l = 0;
				if (parseDataURI(t->href, &mediatype,
						 &js_text, &data_l)) {
					prepareForBrowse(js_text, data_l);
					nzFree(mediatype);
				} else {
					debugPrint(3,
						   "Unable to parse data URI containing JavaScript");
				}
			} else if (browseLocal && !isURL(t->href)) {
				if (!fileIntoMemory
				    (t->href, &serverData, &serverDataLen)) {
					if (debugLevel >= 1)
						i_printf(MSG_GetLocalJS,
							 errorMsg);
				} else {
					js_text = serverData;
					prepareForBrowse(js_text,
							 serverDataLen);
				}
			} else if (httpConnect(t->href, false, false)) {
				if (hcode == 200) {
					js_text = serverData;
					prepareForBrowse(js_text,
							 serverDataLen);
				} else {
					nzFree(serverData);
					if (debugLevel >= 3)
						i_printf(MSG_GetJS, t->href,
							 hcode);
				}
			} else {
				if (debugLevel >= 3)
					i_printf(MSG_GetJS2, errorMsg);
			}
			t->js_ln = 1;
			js_file = (!from_data ? t->href : "data_URI");
			nzFree(changeFileName);
			changeFileName = NULL;
		}
	} else {
		js_text = t->textval;
		t->textval = 0;
	}

	if (!js_text)
		return;
	set_property_string(t->jv, "data", js_text);
	nzFree(js_text);
	filepart = getFileURL(js_file, true);
	t->js_file = cloneString(filepart);
}				/* prepareScript */

static void jsNode(struct htmlTag *t, bool opentag)
{
	int itype;		/* input type */
	const struct tagInfo *ti = t->info;
	int action = t->action;
	const struct htmlTag *above;

/* if js is not active then we shouldn't even be here, but just in case ... */
	if (!isJSAlive)
		return;

/* all the js variables are on the open tag */
	if (!opentag)
		return;
	if (t->step >= 2)
		return;
	t->step = 2;

#if 0
	printf("decorate %s\n", t->info->name);
#endif

	switch (action) {
	case TAGACT_TEXT:
		t->jv = instantiate(cw->docobj, fakePropName(), "TextNode");
		if (t->jv) {
			const char *w = t->textval;
			if (!w)
				w = emptyString;
			set_property_string(t->jv, "data", w);
			set_property_string(t->jv, "nodeName", "text");
		}
		break;

	case TAGACT_SCRIPT:
		prepareScript(t);
		break;

	case TAGACT_FORM:
		domLink(t, "Form", "action", "forms", cw->docobj, 0);
		set_onhandlers(t);
		break;

	case TAGACT_INPUT:
		formControlJS(t);
		if (t->itype == INP_TA)
			establish_inner(t->jv, t->value, 0, true);
		break;

	case TAGACT_OPTION:
		optionJS(t);
		break;

	case TAGACT_A:
		domLink(t, "Anchor", "href", "anchors", cw->docobj, 0);
		set_onhandlers(t);
		break;

	case TAGACT_HEAD:
		domLink(t, "Head", 0, "heads", cw->docobj, 0);
		break;

	case TAGACT_BODY:
		domLink(t, "Body", 0, "bodies", cw->docobj, 0);
		set_onhandlers(t);
		break;

	case TAGACT_TABLE:
		domLink(t, "Table", 0, "tables", cw->docobj, 0);
/* create the array of rows under the table */
		instantiate_array(t->jv, "rows");
		break;

	case TAGACT_TR:
		if ((above = t->controller) && above->jv) {
			domLink(t, "Trow", 0, "rows", above->jv, 0);
			instantiate_array(t->jv, "cells");
		}
		break;

	case TAGACT_TD:
		if ((above = t->controller) && above->jv) {
			domLink(t, "Cell", 0, "cells", above->jv, 0);
			establish_inner(t->jv, t->innerHTML, 0, false);
		}
		break;

	case TAGACT_DIV:
		domLink(t, "Div", 0, "divs", cw->docobj, 0);
		establish_inner(t->jv, t->innerHTML, 0, false);
		break;

	case TAGACT_SPAN:
	case TAGACT_SUB:
	case TAGACT_SUP:
	case TAGACT_OVB:
		domLink(t, "Span", 0, "spans", cw->docobj, 0);
		establish_inner(t->jv, t->innerHTML, 0, false);
		break;

	case TAGACT_AREA:
		domLink(t, "Area", "href", "areas", cw->docobj, 0);
		break;

	case TAGACT_FRAME:
		domLink(t, "Frame", "src", "frames", cw->winobj, 0);
		break;

	case TAGACT_IMAGE:
		domLink(t, "Image", "src", "images", cw->docobj, 0);
		break;

	case TAGACT_P:
		domLink(t, "P", 0, "paragraphs", cw->docobj, 0);
		establish_inner(t->jv, t->innerHTML, 0, false);
		break;

	}			/* switch */

/* js tree mirrors the dom tree. */
	if (t->jv && t->parent && t->parent->jv)
		run_function_objargs(t->parent->jv, "apch$", 1, t->jv);

/* head and body link to document */
	if (t->jv && !t->parent &&
	    (action == TAGACT_HEAD || action == TAGACT_BODY))
		run_function_objargs(cw->docobj, "apch$", 1, t->jv);

}				/* jsNode */

/* decorate the tree of nodes with js objects */
void decorate(int start)
{
	if (!isJSAlive)
		return;

/* title special case */
	set_property_string(cw->docobj, "title", cw->ft);

	traverse_callback = jsNode;
	traverseAll(start);
}				/* decorate */

/*********************************************************************
Diff the old screen with the new rendered screen.
This is a simple front back diff algorithm.
Compare the two strings from the start, how many lines are the same.
Compare the two strings from the back, how many lines are the same.
That zeros in on the line that has changed.
Most of the time one line has changed,
or a couple of adjacent lines, or a couple of nearby lines.
So this should do it.
sameFront counts the lines from the top that are the same.
We're here because the buffers are different, so sameFront will not equal $.
Lines past sameBack1 and same back2 are the same to the bottom in the two buffers.
*********************************************************************/

static int sameFront, sameBack1, sameBack2;
static const char *newChunkStart, *newChunkEnd;

static void frontBackDiff(const char *b1, const char *b2)
{
	const char *f1, *f2, *s1, *s2, *e1, *e2;

	sameFront = 0;
	s1 = b1, s2 = b2;
	f1 = b1, f2 = b2;
	while (*s1 == *s2 && *s1) {
		if (*s1 == '\n') {
			f1 = s1 + 1, f2 = s2 + 1;
			++sameFront;
		}
		++s1, ++s2;
	}

	s1 = b1 + strlen(b1);
	s2 = b2 + strlen(b2);
	while (s1 > f1 && s2 > f2 && s1[-1] == s2[-1])
		--s1, --s2;

	if (s1 == f1 && s2[-1] == '\n')
		goto mark_e;
	if (s2 == f2 && s1[-1] == '\n')
		goto mark_e;
/* advance both pointers to newline or null */
	while (*s1 && *s1 != '\n')
		++s1, ++s2;
/* these buffers should always end in nl, so the next if should always be true */
	if (*s1 == '\n')
		++s1, ++s2;

mark_e:
	e1 = s1, e2 = s2;

	sameBack1 = sameFront;
	for (s1 = f1; s1 < e1; ++s1)
		if (*s1 == '\n')
			++sameBack1;
	if (s1 > f1 && s1[-1] != '\n')	// should never happen
		++sameBack1;

	sameBack2 = sameFront;
	for (s2 = f2; s2 < e2; ++s2)
		if (*s2 == '\n')
			++sameBack2;
	if (s2 > f2 && s2[-1] != '\n')	// should never happen
		++sameBack2;

	newChunkStart = f2;
	newChunkEnd = e2;
}				/* frontBackDiff */

static bool backgroundJS;

/* Rerender the buffer and notify of any lines that have changed */
void rerender(bool rr_command)
{
	char *a, *newbuf;

	if (rr_command) {
/* take the screen snap */
		jSyncup();
	}

	if (!cw->lastrender) {
		puts("lastrender = NULL");
		return;
	}

/* and the new screen */
	a = render(0);
	newbuf = htmlReformat(a);
	nzFree(a);

/* the high runner case, most of the time nothing changes,
 * and we can check that efficiently with strcmp */
	if (stringEqual(newbuf, cw->lastrender)) {
		if (rr_command)
			i_puts(MSG_NoChange);
		nzFree(newbuf);
		return;
	}

	frontBackDiff(cw->lastrender, newbuf);
	if (sameBack1 > sameFront)
		delText(sameFront + 1, sameBack1);
	if (sameBack2 > sameFront)
		addTextToBuffer((pst) newChunkStart,
				newChunkEnd - newChunkStart, sameFront, false);
	cw->undoable = false;

	if (!backgroundJS) {
/* It's almost easier to do it than to report it. */
		if (sameBack2 == sameFront) {	/* delete */
			if (sameBack1 == sameFront + 1)
				i_printf(MSG_LineDelete1, sameFront);
			else
				i_printf(MSG_LineDelete2, sameBack1 - sameFront,
					 sameFront);
		} else if (sameBack1 == sameFront) {
			if (sameBack2 == sameFront + 1)
				i_printf(MSG_LineAdd1, sameFront + 1);
			else
				i_printf(MSG_LineAdd2, sameFront + 1,
					 sameBack2);
		} else {
			if (sameBack1 == sameFront + 1
			    && sameBack2 == sameFront + 1)
				i_printf(MSG_LineUpdate1, sameFront + 1);
			else if (sameBack2 == sameFront + 1)
				i_printf(MSG_LineUpdate2, sameBack1 - sameFront,
					 sameFront + 1);
			else
				i_printf(MSG_LineUpdate3, sameFront + 1,
					 sameBack2);
		}
	}

	nzFree(newbuf);
	nzFree(cw->lastrender);
	cw->lastrender = 0;
}				/* rerender */

/* mark the tags on the deleted lines as deleted */
void delTags(int startRange, int endRange)
{
	pst p;
	int j, tagno, action;
	struct htmlTag *t, *last_td;

	for (j = startRange; j <= endRange; ++j) {
		p = fetchLine(j, -1);
		last_td = 0;
		for (; *p != '\n'; ++p) {
			if (*p != InternalCodeChar)
				continue;
			tagno = strtol(p + 1, (char **)&p, 10);
			t = tagList[tagno];
/* Only mark certain tags as deleted.
 * If you mark <div> deleted, it could wipe out half the page. */
			action = t->action;
			if (action == TAGACT_TEXT ||
			    action == TAGACT_HR ||
			    action == TAGACT_LI || action == TAGACT_IMAGE)
				t->deleted = true;
#if 0
/* this seems to cause more trouble than it's worth */
			if (action == TAGACT_TD) {
				printf("td%d\n", tagno);
				if (last_td)
					last_td->deleted = true;
				last_td = t;
			}
#endif
		}
	}
}				/* delTags */

/* turn an onunload function into a clickable hyperlink */
static void unloadHyperlink(const char *js_function, const char *where)
{
	dwStart();
	stringAndString(&cw->dw, &cw->dw_l, "<P>Onclose <A href='javascript:");
	stringAndString(&cw->dw, &cw->dw_l, js_function);
	stringAndString(&cw->dw, &cw->dw_l, "()'>");
	stringAndString(&cw->dw, &cw->dw_l, where);
	stringAndString(&cw->dw, &cw->dw_l, "</A><br>");
}				/* unloadHyperlink */

/* Run the various onload functions */
/* Turn the onunload functions into hyperlinks */
/* This runs after the page is parsed and before the various javascripts run, is that right? */
void runOnload(void)
{
	int i, action;
	int fn;			/* form number */
	struct htmlTag *t;

	if (!isJSAlive)
		return;

/* window and document onload */
	run_function_bool(cw->winobj, "onload");
	run_function_bool(cw->docobj, "onload");

	fn = -1;
	for (i = 0; i < cw->numTags; ++i) {
		t = tagList[i];
		if (t->slash)
			continue;
		action = t->action;
		if (action == TAGACT_FORM)
			++fn;
		if (!t->jv)
			continue;
		if (action == TAGACT_BODY && t->onload)
			run_function_bool(t->jv, "onload");
		if (action == TAGACT_BODY && t->onunload)
			unloadHyperlink("document.body.onunload", "Body");
		if (action == TAGACT_FORM && t->onload)
			run_function_bool(t->jv, "onload");
/* tidy5 says there is no form.onunload */
		if (action == TAGACT_FORM && t->onunload) {
			char formfunction[20];
			sprintf(formfunction, "document.forms[%d].onunload",
				fn);
			unloadHyperlink(formfunction, "Form");
		}
	}
}				/* runOnload */

/*********************************************************************
Manage js timers here.
It's a simple list of timers, assuming there aren't too many.
Store the seconds and milliseconds when the timer should fire,
the code to execute, and the timer object, which becomes "this".
*********************************************************************/

struct jsTimer {
	struct jsTimer *next, *prev;
	struct ebWindow *w;	/* edbrowse window holding this timer */
	time_t sec;
	int ms;
	bool isInterval;
	int jump_sec;		/* for interval */
	int jump_ms;
	const char *jsrc;
	jsobjtype timerObject;
};

/* list of pending timers */
struct listHead timerList = {
	&timerList, &timerList
};

static time_t now_sec;
static int now_ms;
static void currentTime(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	now_sec = tv.tv_sec;
	now_ms = tv.tv_usec / 1000;
}				/* currentTime */

void javaSetsTimeout(int n, const char *jsrc, jsobjtype to, bool isInterval)
{
	struct jsTimer *jt;

	if (jsrc[0] == 0)
		return;		/* nothing to run */

	jt = allocMem(sizeof(struct jsTimer));
	jt->jsrc = cloneString(jsrc);
	jt->sec = n / 1000;
	jt->ms = n % 1000;
	jt->isInterval = isInterval;
	if (isInterval)
		jt->jump_sec = n / 1000, jt->jump_ms = n % 1000;
	currentTime();
	jt->sec += now_sec;
	jt->ms += now_ms;
	if (jt->ms >= 1000)
		jt->ms -= 1000, ++jt->sec;
	jt->timerObject = to;
	jt->w = cw;
	addToListBack(&timerList, jt);
	debugPrint(4, "timer %d %s\n", n, jsrc);
}				/* javaSetsTimeout */

static struct jsTimer *soonest(void)
{
	struct jsTimer *t, *best_t = 0;
	if (listIsEmpty(&timerList))
		return 0;
	foreach(t, timerList) {
		if (!best_t || t->sec < best_t->sec ||
		    t->sec == best_t->sec && t->ms < best_t->ms)
			best_t = t;
	}
	return best_t;
}				/* soonest */

bool timerWait(int *delay_sec, int *delay_ms)
{
	struct jsTimer *jt = soonest();
	if (!jt)
		return false;
	currentTime();
	if (now_sec > jt->sec || now_sec == jt->sec && now_ms >= jt->ms)
		*delay_sec = *delay_ms = 0;
	else {
		*delay_sec = jt->sec - now_sec;
		*delay_ms = (jt->ms - now_ms);
		if (*delay_ms < 0)
			*delay_ms += 1000, --*delay_sec;
	}
	return true;
}				/* timerWait */

void delTimers(struct ebWindow *w)
{
	int delcount = 0;
	struct jsTimer *jt, *jnext;
	for (jt = timerList.next; jt != (void *)&timerList; jt = jnext) {
		jnext = jt->next;
		if (jt->w == w) {
			++delcount;
			delFromList(jt);
			cnzFree(jt->jsrc);
			nzFree(jt);
		}
	}
	debugPrint(4, "%d timers deleted", delcount);
}				/* delTimers */

void runTimers(void)
{
	struct jsTimer *jt;
	struct ebWindow *save_cw = cw;
	char *screen;
	int screenlen;

	currentTime();

	while (jt = soonest()) {
		if (jt->sec > now_sec || jt->sec == now_sec && jt->ms > now_ms)
			break;

		cw = jt->w;
		backgroundJS = true;
		javaParseExecute(jt->timerObject, jt->jsrc, "timer", 1);
		runScriptsPending();

		if (cw != save_cw) {
/* background window, go ahead and rerender, silently. */
/* Screen snap, because we didn't run jSyncup */
			nzFree(cw->lastrender);
			cw->lastrender = 0;
			if (unfoldBufferW(cw, false, &screen, &screenlen))
				cw->lastrender = screen;
			rerender(false);
		}
		backgroundJS = false;

		if (jt->isInterval) {
			jt->sec = now_sec + jt->jump_sec;
			jt->ms = now_ms + jt->jump_ms;
			if (jt->ms >= 1000)
				jt->ms -= 1000, ++jt->sec;
		} else {
			delFromList(jt);
			cnzFree(jt->jsrc);
			nzFree(jt);
		}
	}

	cw = save_cw;
}				/* runTimers */

void javaOpensWindow(const char *href, const char *name)
{
	struct htmlTag *t;
	char *copy, *r;
	const char *a;

	if (!href || !*href) {
		debugPrint(3, "javascript is opening a blank window");
		return;
	}

	copy = cloneString(href);
	unpercentURL(copy);
	r = resolveURL(cw->hbase, copy);
	nzFree(copy);
	if (cw->browseMode && !backgroundJS) {
		gotoLocation(r, 0, false);
		return;
	}

/* Turn the new window into a hyperlink. */
/* just shovel this onto dw, as though it came from document.write() */
	dwStart();
	stringAndString(&cw->dw, &cw->dw_l, "<P>");
	stringAndString(&cw->dw, &cw->dw_l, i_getString(MSG_Redirect));
	stringAndString(&cw->dw, &cw->dw_l, ": <A href=");
	stringAndString(&cw->dw, &cw->dw_l, r);
	stringAndChar(&cw->dw, &cw->dw_l, '>');
	a = altText(r);
	nzFree(r);
/* I'll assume this is more helpful than the name of the window */
	if (a)
		name = a;
	r = htmlEscape(name);
	stringAndString(&cw->dw, &cw->dw_l, r);
	nzFree(r);
	stringAndString(&cw->dw, &cw->dw_l, "</A><br>\n");
}				/* javaOpensWindow */

void javaSetsLinkage(char type, jsobjtype p_j, const char *rest)
{
	struct htmlTag *parent, *add, *before, *c, *t;
	jsobjtype *a_j, *b_j;
	char p_name[MAXTAGNAME], a_name[MAXTAGNAME], b_name[MAXTAGNAME];
	int action;

	sscanf(rest, "%s %p,%s %p,%s ", p_name, &a_j, a_name, &b_j, b_name);
	parent = tagFromJavaVar2(p_j, p_name);
	if (type == 'c')	/* create */
		return;

	add = tagFromJavaVar2(a_j, a_name);
	if (!parent || !add)
		return;

	if (type == 'r') {
/* add is a misnomer here, it's being removed */
		add->deleted = true;
		add->parent = NULL;
		if (parent->firstchild == add)
			parent->firstchild = add->sibling;
		else {
			for (c = parent->firstchild; c->sibling; c = c->sibling) {
				if (c->sibling != add)
					continue;
				c->sibling = add->sibling;
				break;
			}
		}
		return;
	}

	if (type == 'b') {	/* insertBefore */
		before = tagFromJavaVar2(b_j, b_name);
		if (!before)
			return;
		c = parent->firstchild;
		if (!c)
			return;
		if (c == before) {
			parent->firstchild = add;
			add->sibling = before;
			add->parent = parent;
			add->deleted = false;
			return;
		}
		while (c->sibling && c->sibling != before)
			c = c->sibling;
		if (!c->sibling)
			return;
		c->sibling = add;
		add->sibling = before;
	} else {
/* type = a, appendchild */
		if (!parent->firstchild)
			parent->firstchild = add;
		else {
			c = parent->firstchild;
			while (c->sibling)
				c = c->sibling;
			c->sibling = add;
		}
	}
	add->parent = parent;
	add->deleted = false;

/* Bad news, we have to replicate some of the prerender logic here. */
/* This node is attached to the tree, just like an html tag would be. */
	t = add;
	action = t->action;
	switch (action) {
	case TAGACT_INPUT:
		if (!t->value)
			t->value = emptyString;
		t->rvalue = cloneString(t->value);
		t->controller = findOpenTag(t, TAGACT_FORM);
		break;
	}			/* switch */
}				/* javaSetsLinkage */
