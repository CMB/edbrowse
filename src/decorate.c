/*********************************************************************
decorate.c:
sanitize a tree of nodes produced by html,
and decorate the tree with the corresponding js objects.
A <form> tag has a corresponding Form object in the js world, etc.
This is done for the html that is on the initial web page,
and any html that is produced by javascript via
foo.innerHTML = string or document.write(string).
*********************************************************************/

#include "eb.h"

/* The current (foreground) edbrowse window and frame.
 * These are replaced with stubs when run within the javascript process. */
Window *cw;
Frame *cf;
int gfsn;

/* traverse the tree of nodes with a callback function */
nodeFunction traverse_callback;

/* possible callback functions in this file */
static void prerenderNode(Tag *node, bool opentag);
static void jsNode(Tag *node, bool opentag);
static void pushAttributes(const Tag *t);

static bool treeOverflow;

static void traverseNode(Tag *node)
{
	Tag *child;

	if (node->visited) {
		treeOverflow = true;
		debugPrint(4, "node revisit %s %d", node->info->name,
			   node->seqno);
		return;
	}
	node->visited = true;

	(*traverse_callback) (node, true);
	for (child = node->firstchild; child; child = child->sibling)
		traverseNode(child);
	(*traverse_callback) (node, false);
}

void traverseAll(int start)
{
	Tag *t;
	int i;

	treeOverflow = false;
	for (i = start; i < cw->numTags; ++i) {
		t = tagList[i];
		t->visited = false;
	}

	for (i = start; i < cw->numTags; ++i) {
		t = tagList[i];
		if (!t->parent && !t->slash && !t->dead) {
			debugPrint(6, "traverse start at %s %d", t->info->name, t->seqno);
			traverseNode(t);
		}
	}

	if (treeOverflow)
		debugPrint(3, "malformed tree!");
}

static int nopt;		/* number of options */
/* None of these tags nest, so it is reasonable to talk about
 * the current open tag. */
static Tag *currentForm, *currentSel, *currentOpt, *currentStyle;
static const char *optg; // option group
static Tag *currentTitle, *currentScript, *currentTA;
static Tag *currentA;
static char *radioCheck;
static int radio_l;

static void linkinTree(Tag *parent, Tag *child)
{
	Tag *c, *d;
	child->parent = parent;

	if (!parent->firstchild) {
		parent->firstchild = child;
		return;
	}

	for (c = parent->firstchild; c; c = c->sibling) {
		d = c;
	}
	d->sibling = child;
}

static void makeButton(void)
{
	Tag *t = newTag(cf, "input");
	t->controller = currentForm;
	t->itype = INP_SUBMIT;
	t->value = emptyString;
	t->step = 1;
	linkinTree(currentForm, t);
}

Tag *findOpenTag(Tag *t, int action)
{
	int count = 0;
	while ((t = t->parent)) {
		if (t->action == action)
			return t;
		if (++count == 10000) {	// tree shouldn't be this deep
			debugPrint(1, "infinite loop in findOpenTag()");
			break;
		}
	}
	return 0;
}

static Tag *findOpenSection(Tag *t)
{
	int count = 0;
	while ((t = t->parent)) {
		if (t->action == TAGACT_TBODY || t->action == TAGACT_THEAD ||
		    t->action == TAGACT_TFOOT)
			return t;
		if (++count == 10000) {	// tree shouldn't be this deep
			debugPrint(1, "infinite loop in findOpenTag()");
			break;
		}
	}
	return 0;
}

Tag *findOpenList(Tag *t)
{
	while ((t = t->parent))
		if (t->action == TAGACT_OL || t->action == TAGACT_UL)
			return t;
	return 0;
}

/*********************************************************************
Tables are suppose to have bodies, I guess.
So <table><tr> becomes <table><tbody><tr>
Find each table and look at its children.
Note the tags between sections, where section is tHead, tBody, or tFoot.
If that span includes <tr>, then put those tags under a new tBody.
*********************************************************************/

static void insert_tbody1(Tag *s1, Tag *s2,
			  Tag *tbl);
static bool tagBelow(Tag *t, int action);

static void insert_tbody(int start)
{
	int i, end = cw->numTags;
	Tag *tbl, *s1, *s2;

	for (i = start; i < end; ++i) {
		tbl = tagList[i];
		if (tbl->action != TAGACT_TABLE)
			continue;
		s1 = 0;
		do {
			s2 = (s1 ? s1->sibling : tbl->firstchild);
			while (s2 && s2->action != TAGACT_TBODY
			       && s2->action != TAGACT_THEAD
			       && s2->action != TAGACT_TFOOT)
				s2 = s2->sibling;
			insert_tbody1(s1, s2, tbl);
			s1 = s2;
		} while (s1);
	}
}

static void insert_tbody1(Tag *s1, Tag *s2,
			  Tag *tbl)
{
	Tag *s1a = (s1 ? s1->sibling : tbl->firstchild);
	Tag *u, *uprev, *ns;	// new section

	if (s1a == s2)		// nothing between
		return;

// Look for the direct html <table><tr><th>.
// If th is anywhere else down the path, we won't find it.
	if (!s1 && s1a->action == TAGACT_TR &&
	    (u = s1a->firstchild) && stringEqual(u->info->name, "th")) {
		ns = newTag(cf, "thead");
		tbl->firstchild = ns;
		ns->parent = tbl;
		ns->firstchild = s1a;
		s1a->parent = ns;
		ns->sibling = s1a->sibling;
		s1a->sibling = 0;
		s1 = ns;
		s1a = s1->sibling;
	}

	for (u = s1a; u != s2; u = u->sibling)
		if (tagBelow(u, TAGACT_TR))
			break;
	if (u == s2)		// no rows below
		return;

	ns = newTag(cf, "tbody");
	for (u = s1a; u != s2; u = u->sibling)
		uprev = u, u->parent = ns;
	if (s1)
		s1->sibling = ns;
	else
		tbl->firstchild = ns;
	if (s2)
		uprev->sibling = 0, ns->sibling = s2;
	ns->firstchild = s1a;
	ns->parent = tbl;
}

static bool tagBelow(Tag *t, int action)
{
	Tag *c;

	if (t->action == action)
		return true;
	for (c = t->firstchild; c; c = c->sibling)
		if (tagBelow(c, action))
			return true;
	return false;
}

void formControl(Tag *t, bool namecheck)
{
	int itype = t->itype;
	char *myname = (t->name ? t->name : t->id);
	Tag *cform = currentForm;
	if (!cform) {
/* nodes could be created dynamically, not through html */
		cform = findOpenTag(t, TAGACT_FORM);
	}
	if (cform)
		t->controller = cform;
	else if (itype != INP_BUTTON && itype != INP_SUBMIT && !htmlGenerated)
		debugPrint(3, "%s is not part of a fill-out form",
			   t->info->desc);
	if (namecheck && !myname && !htmlGenerated)
		debugPrint(3, "%s does not have a name", t->info->desc);
}

const char *const inp_types[] = {
	"reset", "button", "image", "submit",
	"hidden", "text", "file",
	"select", "textarea", "radio", "checkbox",
	0
};

/*********************************************************************
Here are some other input types that should have additional syntax checks
performed on them, but as far as this version of edbrowse is concerned,
they are equivalent to text. Just here to suppress warnings.
List taken from https://www.tutorialspoint.com/html/html_input_tag.htm
*********************************************************************/

const char *const inp_others[] = {
	"no_minor", "date", "datetime", "datetime-local",
	"month", "week", "time", "email", "range",
	"search", "tel", "url", "number", "password",
	0
};

/* helper function for input tag */
void htmlInputHelper(Tag *t)
{
	int n = INP_TEXT;
	int len;
	char *myname = (t->name ? t->name : t->id);
	const char *s = attribVal(t, "type");
	bool isbutton = stringEqual(t->info->name, "button");

	t->itype = (isbutton ? INP_BUTTON : INP_TEXT);
	if (s && *s) {
		n = stringInListCI(inp_types, s);
		if (n < 0) {
			n = stringInListCI(inp_others, s);
			if (n < 0)
				debugPrint(3, "unrecognized input type %s", s);
			else
				t->itype = INP_TEXT, t->itype_minor = n;
			if (n == INP_PW)
				t->masked = true;
		} else
			t->itype = n;
	}
// button no type means submit
	if (!s && isbutton)
		t->itype = INP_SUBMIT;

	s = attribVal(t, "maxlength");
	len = 0;
	if (s)
		len = stringIsNum(s);
	if (len > 0)
		t->lic = len;

// No preset value on file, for security reasons.
// <input type=file value=/etc/passwd> then submit via onload().
	if (n == INP_FILE) {
		nzFree(t->value);
		t->value = 0;
		cnzFree(t->rvalue);
		t->rvalue = 0;
	}

/* In this case an empty value should be "", not null */
	if (t->value == 0)
		t->value = emptyString;
	if (t->rvalue == 0)
		t->rvalue = cloneString(t->value);

	if (n == INP_RADIO && t->checked && radioCheck && myname) {
		char namebuf[200];
		if (strlen(myname) < sizeof(namebuf) - 3) {
			if (!*radioCheck)
				stringAndChar(&radioCheck, &radio_l, '|');
			sprintf(namebuf, "|%s|", t->name);
			if (strstr(radioCheck, namebuf)) {
				debugPrint(3,
					   "multiple radio buttons have been selected");
				return;
			}
			stringAndString(&radioCheck, &radio_l, namebuf + 1);
		}
	}

	/* Even the submit fields can have a name, but they don't have to */
	formControl(t, (n > INP_SUBMIT));
}

/* return an allocated string containing the text entries for the checked options */
char *displayOptions(const Tag *sel)
{
	const Tag *t;
	char *opt;
	int opt_l;

	opt = initString(&opt_l);
	for (t = cw->optlist; t; t = t->same) {
		if (t->controller != sel)
			continue;
		if (!t->checked)
			continue;
		if (*opt)
			stringAndChar(&opt, &opt_l, selsep);
		stringAndString(&opt, &opt_l, t->textval);
	}

	return opt;
}

static void prerenderNode(Tag *t, bool opentag)
{
	int itype;		/* input type */
	int j;
	int action = t->action;
	const char *a;		/* usually an attribute */

	if (t->step >= 1)
		return;
	debugPrint(6, "prend %c%s %d",
		   (opentag ? ' ' : '/'), t->info->name,
		   t->seqno);
	if (!opentag)
		t->step = 1;

	switch (action) {
	case TAGACT_NOSCRIPT:
// If javascript is enabled kill everything under noscript
		if (isJSAlive && !opentag)
			underKill(t);
		break;

	case TAGACT_TEXT:
		if (!opentag || !t->textval)
			break;

		if (currentTitle) {
			if (!cw->htmltitle) {
				cw->htmltitle = cloneString(t->textval);
				spaceCrunch(cw->htmltitle, true, false);
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

		if (currentStyle) {
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
			leftClipString(currentTA->value);
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
		if((t->controller = findOpenTag(t, TAGACT_SVG))) {
// this is title for svg, not for the document.
// Just turn things into span.
			t->action = t->controller->action = TAGACT_SPAN;
			break;
		}
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
				else if (stringEqualCI(a, "text/plain"))
					t->plain = true;
				else if (!stringEqualCI(a, 
					  "application/x-www-form-urlencoded"))
					debugPrint(3,
						   "unrecognized enctype, plese use multipart/form-data or application/x-www-form-urlencoded or text/plain");
			}
			if ((a = t->href)) {
				const char *prot = getProtURL(a);
				if (prot) {
					if (stringEqualCI(prot, "mailto"))
						t->bymail = true;
					else if (stringEqualCI
						 (prot, "javascript"))
						t->javapost = true;
					else if (stringEqualCI(prot, "https"))
						t->secure = true;
					else if (!stringEqualCI(prot, "http") &&
						 !stringEqualCI(prot, "gopher"))
						debugPrint(3,
							   "form cannot submit using protocol %s",
							   prot);
				}
			}

			nzFree(radioCheck);
			radioCheck = initString(&radio_l);
		}
		if (!opentag && currentForm) {
			if (t->ninp && !t->submitted) {
				makeButton();
				t->submitted = true;
			}
			currentForm = 0;
		}
		break;

	case TAGACT_INPUT:
		if (!opentag)
			break;
		htmlInputHelper(t);
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

	case TAGACT_OPTG:
		if(opentag)
			optg = attribVal(t, "label");
		else
			optg = 0;
		break;

	case TAGACT_OPTION:
		if (!opentag) {
			currentOpt = 0;
			optg = 0;
// in datalist, the value becomes the text
			if(currentSel && currentSel->action == TAGACT_DATAL) {
				nzFree(t->textval);
				t->textval = cloneString(t->value);
			}
			a = attribVal(t, "label");
			if(a && *a) {
				nzFree(t->textval);
				t->textval = cloneString(a);
			}
			break;
		}
		if (!currentSel) {
			debugPrint(3,
				   "option appears outside a select statement");
			optg = 0;
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
		if (!t->value)
			t->value = emptyString;
		t->textval = emptyString;
		if(optg && *optg) {
// borrow custom_h, opt group is like a custom header
			t->custom_h = cloneString(optg);
			optg = 0;
		}
		break;

	case TAGACT_STYLE:
		if (!opentag) {
			currentStyle = 0;
			break;
		}
		currentStyle = t;
		break;

	case TAGACT_SELECT:
	case TAGACT_DATAL:
		optg = 0;
		if (opentag) {
			currentSel = t;
			nopt = 0;
			t->itype = INP_SELECT;
			if(action == TAGACT_SELECT)
				formControl(t, true);
		} else {
			currentSel = 0;
			if(action == TAGACT_SELECT) {
				t->action = TAGACT_INPUT;
				t->value = displayOptions(t);
			}
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
			currentTA = 0;
		}
		break;

	case TAGACT_META:
		if (opentag) {
/*********************************************************************
htmlMetaHelper is not called for document.createElement("meta")
I don't know if that matters.
Another problem which is extant in the result of a google search:
there is a meta tag with a refresh inside a noscript block.
The refresh shouldn't run at all, but it does,
and the next page has the same tags, and so on forever.
So don't do this if inside <noscript>, and js is active.
Are there other situations where we need to supress meta processing?
*********************************************************************/
			if(!(findOpenTag(t, TAGACT_NOSCRIPT) && isJSAlive))
				htmlMetaHelper(t);
		}
		break;

	case TAGACT_TBODY:
	case TAGACT_THEAD:
	case TAGACT_TFOOT:
		if (opentag)
			t->controller = findOpenTag(t, TAGACT_TABLE);
		break;

	case TAGACT_TR:
		if (opentag) {
			t->controller = findOpenSection(t);
			if (!t->controller)
				t->controller = findOpenTag(t, TAGACT_TABLE);
		}
		break;

	case TAGACT_TD:
		if (opentag)
			t->controller = findOpenTag(t, TAGACT_TR);
		break;

	case TAGACT_SPAN:
		if (!opentag)
			break;
		if (!(a = t->jclass))
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

	case TAGACT_FRAME:
		if (opentag)
			break;
// If somebody wrote <frame><p>foo</frame>, those tags should be excised.
		underKill(t);
		break;

	case TAGACT_MUSIC:
		if (opentag)
			break;
// If somebody wrote <audio><p>foo</audio>, those tags should be excised.
// However <source> tags should be kept and/or expanded. Not yet implemented.
		underKill(t);
		break;

	}			/* switch */
}

void prerender(int start)
{
/* some cleanup routines to rearrange the tree */
	insert_tbody(start);

	currentForm = currentSel = currentOpt = NULL;
	currentTitle = currentScript = currentTA = NULL;
	currentStyle = NULL;
	optg = NULL;
	nzFree(radioCheck);
	radioCheck = 0;
	traverse_callback = prerenderNode;
	traverseAll(start);
	currentForm = NULL;
	nzFree(radioCheck);
	radioCheck = 0;
}

static char fakePropLast[24];
const char *fakePropName(void)
{
	static int idx = 0;
	sprintf(fakePropLast, "gc$%d", ++idx);
	return fakePropLast;
}

static void establish_inner(Tag *t, const char *start, const char *end,
			    bool isText)
{
	const char *s = emptyString;
	const char *name = (isText ? "value" : "innerHTML");
	if (start) {
		s = start;
		if (end)
			s = pullString(start, end - start);
	}
	set_property_string_t(t, name, s);
	if (start && end)
		nzFree((char *)s);
// If this is a textarea, we haven't yet set up the innerHTML
// getter and seter
	if (isText)
		set_property_string_t(t, "innerHTML", emptyString);
}

static const char defvl[] = "defaultValue";
static const char defck[] = "defaultChecked";
static const char defsel[] = "defaultSelected";

static void formControlJS(Tag *t)
{
	const char *typedesc;
	int itype = t->itype;
	int isradio = (itype == INP_RADIO);
	bool isselect = (itype == INP_SELECT);
	bool ista = (itype == INP_TA);
	const char *whichclass = (isselect ? "HTMLSelectElement" : (ista ? "HTMLTextAreaElement" : "HTMLInputElement"));
	const Tag *form = t->controller;

	if (form && form->jslink)
		domLink(t, whichclass, 0, "elements", form, isradio);
	else
		domLink(t, whichclass, 0, 0, 0, (4|isradio));
	if (!t->jslink)
		return;

	if (itype <= INP_RADIO && !isselect) {
		set_property_string_t(t, "value", t->value);
		if (itype != INP_FILE) {
/* No default value on file, for security reasons */
			set_property_string_t(t, defvl, t->value);
		}		/* not file */
	}

	if (isselect)
		typedesc = t->multiple ? "select-multiple" : "select-one";
	else
		typedesc = inp_types[itype];
	set_property_string_t(t, "type", typedesc);

	if (itype >= INP_RADIO) {
		set_property_bool_t(t, "checked", t->checked);
		set_property_bool_t(t, defck, t->checked);
	}
}

static void optionJS(Tag *t)
{
	Tag *sel = t->controller;
	const char *tx = t->textval;
	const char *cl = t->jclass;

	if (!sel)
		return;

	if (!tx) {
		debugPrint(3, "empty option");
	} else {
		if (!t->value)
			t->value = cloneString(tx);
	}
/* no point if the controlling select doesn't have a js object */
	if (!sel->jslink)
		return;
establish_js_option(t, sel);
// nodeName and nodeType set in constructor
	set_property_string_t(t, "text", t->textval);
	set_property_string_t(t, "value", t->value);
	set_property_bool_t(t, "selected", t->checked);
	set_property_bool_t(t, defsel, t->checked);
	if (!cl)
		cl = emptyString;
	set_property_string_t(t, "class", cl);
	set_property_string_t(t, "last$class", cl);

	if (t->checked && !sel->multiple)
		set_property_number_t(sel, "selectedIndex", t->lic);
}

static void link_css(Tag *t)
{
	struct i_get g;
	char *b;
	int blen;
	const char *a;
	const char *a1 = attribVal(t, "type");
	const char *a2 = attribVal(t, "rel");
	const char *altsource, *realsource;

	if (a1)
		set_property_string_t(t, "type", a1);
	if (a2)
		set_property_string_t(t, "rel", a2);
	if (!t->href)
		return;
	if (!stringEqualCI(a1, "text/css") &&
	    !stringEqualCI(a2, "stylesheet"))
		return;

// Fetch the css file so we can apply its attributes.
	a = NULL;
	altsource = fetchReplace(t->href);
	realsource = (altsource ? altsource : t->href);
	if ((browseLocal || altsource) && !isURL(realsource)) {
		debugPrint(3, "css source %s", realsource);
		if (!fileIntoMemory(realsource, &b, &blen)) {
			if (debugLevel >= 1)
				i_printf(MSG_GetLocalCSS);
		} else {
			a = force_utf8(b, blen);
			if (!a)
				a = b;
			else
				nzFree(b);
		}
	} else {
		debugPrint(3, "css source %s", t->href);
		memset(&g, 0, sizeof(g));
		g.thisfile = cf->fileName;
		g.uriEncoded = true;
		g.url = t->href;
		if (httpConnect(&g)) {
			nzFree(g.referrer);
			nzFree(g.cfn);
			if (g.code == 200) {
				a = force_utf8(g.buffer, g.length);
				if (!a)
					a = g.buffer;
				else
					nzFree(g.buffer);
// acid3 test[0] says we don't process this file if it's content type is
// text/html. Should I test for anything outside of text/css?
// For now I insist it be missing or text/css or text/plain.
// A similar test is performed in css.c after httpConnect.
				if (g.content[0]
				    && !stringEqual(g.content, "text/css")
				    && !stringEqual(g.content, "text/plain")) {
					debugPrint(3,
						   "css suppressed because content type is %s",
						   g.content);
					cnzFree(a);
					a = NULL;
				}
			} else {
				nzFree(g.buffer);
				if (debugLevel >= 3)
					i_printf(MSG_GetCSS, g.url, g.code);
			}
		} else {
			if (debugLevel >= 3)
				i_printf(MSG_GetCSS2);
		}
	}
	if (a) {
		set_property_string_t(t, "css$data", a);
// indicate we can run the onload function, if there is one
		t->lic = 1;
	}
	cnzFree(a);
}

static Tag *innerParent;

static void jsNode(Tag *t, bool opentag)
{
	const struct tagInfo *ti = t->info;
	int action = t->action;
	const Tag *above;
	const char *a;
	bool linked_in;

// run reindex at table close
	if (action == TAGACT_TABLE && !opentag && t->jslink)
		run_function_onearg_win(cf, "rowReindex", t);

/* all the js variables are on the open tag */
	if (!opentag)
		return;
	if (t->step >= 2)
		return;
	t->step = 2;

/*********************************************************************
If js is off, and you don't decorate this tree,
then js is turned on later, and you parse and decorate a frame,
it might also decorate this tree in the wrong context.
Needless to say that's not good!
*********************************************************************/
	if (t->f0 != cf)
		return;

	debugPrint(6, "decorate %s %d", t->info->name, t->seqno);
	fakePropLast[0] = 0;

	switch (action) {

	case TAGACT_TEXT:
		debugPrint(5, "domText");
		establish_js_textnode(t, fakePropName());
// nodeName and nodeType set in constructor
		if (t->jslink) {
			const char *w = t->textval;
			if (!w)
				w = emptyString;
			set_property_string_t(t, "data", w);
			w = (t->jclass ? t->jclass : emptyString);
			set_property_string_t(t, "class", w);
			set_property_string_t(t, "last$class", w);
		}
		break;

	case TAGACT_DOCTYPE:
		domLink(t, "DocType", 0, 0, 0, 4);
		t->deleted = true;
		break;

	case TAGACT_HTML:
		domLink(t, "HTML", 0, 0, 0, 4);
		cf->htmltag = t;
		break;

	case TAGACT_META:
		domLink(t, "HTMLMetaElement", 0, "metas", 0, 4);
		break;

	case TAGACT_STYLE:
		domLink(t, "HTMLStyleElement", 0, "styles", 0, 4);
		a = attribVal(t, "type");
		if (!a)
			a = emptyString;
		set_property_string_t(t, "type", a);
		break;

	case TAGACT_SCRIPT:
		domLink(t, "HTMLScriptElement", "src", "scripts", 0, 4);
		a = attribVal(t, "type");
		if (a)
			set_property_string_t(t, "type", a);
		a = attribVal(t, "text");
		if (a) {
			set_property_string_t(t, "text", a);
		} else {
			set_property_string_t(t, "text", "");
		}
		a = attribVal(t, "src");
		if (a) {
			set_property_string_t(t, "src", a);
			if (down_jsbg && a[0])	// from another source, let's get it started
				prepareScript(t);
		} else {
			set_property_string_t(t, "src", "");
		}
		break;

	case TAGACT_FORM:
		domLink(t, "HTMLFormElement", "action", "forms", 0, 4);
		break;

	case TAGACT_INPUT:
		formControlJS(t);
		if (t->itype == INP_TA)
			establish_inner(t, t->value, 0, true);
		break;

	case TAGACT_OPTION:
		optionJS(t);
// The parent child relationship has already been established,
// don't break, just return;
		return;

	case TAGACT_DATAL:
		domLink(t, "Datalist", 0, 0, 0, 4);
		break;

	case TAGACT_A:
		domLink(t, "HTMLAnchorElement", "href", "links", 0, 4);
		break;

	case TAGACT_HEAD:
		domLink(t, "HTMLHeadElement", 0, "heads", 0, 4);
		cf->headtag = t;
		break;

	case TAGACT_BODY:
		domLink(t, "HTMLBodyElement", 0, "bodies", 0, 4);
		cf->bodytag = t;
		break;

	case TAGACT_OL:
		domLink(t, "HTMLOListElement", 0, 0, 0, 4);
		break;
	case TAGACT_UL:
		domLink(t, "HTMLUListElement", 0, 0, 0, 4);
		break;
	case TAGACT_DL:
		domLink(t, "HTMLDListElement", 0, 0, 0, 4);
		break;

	case TAGACT_LI:
		domLink(t, "HTMLLIElement", 0, 0, 0, 4);
		break;

	case TAGACT_CANVAS:
		domLink(t, "HTMLCanvasElement", 0, 0, 0, 4);
		break;

	case TAGACT_TABLE:
		domLink(t, "HTMLTableElement", 0, "tables", 0, 4);
		break;

	case TAGACT_TBODY:
		if ((above = t->controller) && above->jslink)
			domLink(t, "tBody", 0, "tBodies", above, 0);
		break;

	case TAGACT_THEAD:
		if ((above = t->controller) && above->jslink) {
			domLink(t, "tHead", 0, 0, above, 0);
			set_property_object_t(above, "tHead", t);
		}
		break;

	case TAGACT_TFOOT:
		if ((above = t->controller) && above->jslink) {
			domLink(t, "tFoot", 0, 0, above, 0);
			set_property_object_t(above, "tFoot", t);
		}
		break;

	case TAGACT_TR:
		if ((above = t->controller) && above->jslink)
			domLink(t, "HTMLTableRowElement", 0, "rows", above, 0);
		break;

	case TAGACT_TD:
		if ((above = t->controller) && above->jslink)
			domLink(t, "HTMLTableCellElement", 0, "cells", above, 0);
		break;

	case TAGACT_DIV:
		domLink(t, "HTMLDivElement", 0, "divs", 0, 4);
		break;

	case TAGACT_LABEL:
		domLink(t, "HTMLLabelElement", 0, "labels", 0, 4);
		break;

	case TAGACT_OBJECT:
		domLink(t, "HtmlObj", 0, "htmlobjs", 0, 4);
		break;

	case TAGACT_UNKNOWN:
		domLink(t, "HTMLElement", 0, 0, 0, 4);
		break;

	case TAGACT_SPAN:
	case TAGACT_SUB:
	case TAGACT_SUP:
	case TAGACT_OVB:
		domLink(t, "HTMLSpanElement", 0, "spans", 0, 4);
		break;

	case TAGACT_AREA:
		domLink(t, "HTMLAreaElement", "href", "links", 0, 4);
		break;

	case TAGACT_FRAME:
// about:blank means a blank frame with no sourcefile.
		if (stringEqual(t->href, "about:blank")) {
			nzFree(t->href);
			t->href = 0;
		}
		domLink(t, "HTMLFrameElement", "src", 0, 0, 2);
		break;

	case TAGACT_IMAGE:
		domLink(t, "HTMLImageElement", "src", "images", 0, 4);
		break;

	case TAGACT_P:
		domLink(t, "HTMLParagraphElement", 0, "paragraphs", 0, 4);
		break;

	case TAGACT_H:
		domLink(t, "HTMLHeadingElement", 0, 0, 0, 4);
		break;

	case TAGACT_HEADER:
		domLink(t, "Header", 0, "headers", 0, 4);
		break;

	case TAGACT_FOOTER:
		domLink(t, "Footer", 0, "footers", 0, 4);
		break;

	case TAGACT_TITLE:
		if (cw->htmltitle)
			set_property_string_doc(cf, "title", cw->htmltitle);
		domLink(t, "Title", 0, 0, 0, 4);
		break;

	case TAGACT_LINK:
		domLink(t, "HTMLLinkElement", "href", 0, 0, 4);
		link_css(t);
		break;

	case TAGACT_MUSIC:
		domLink(t, "HTMLAudioElement", "src", 0, 0, 4);
		break;

	case TAGACT_BASE:
		domLink(t, "HTMLBaseElement", "href", 0, 0, 4);
		break;

	default:
// Don't know what this tag is, or it's not semantically important,
// so just call it an html element.
		domLink(t, "HTMLElement", 0, 0, 0, 4);
		break;
	}			/* switch */

	if (!t->jslink)
		return;		/* nothing else to do */

/* js tree mirrors the dom tree. */
	linked_in = false;

	if (t->parent && t->parent->jslink) {
		run_function_onearg_t(t->parent, "eb$apch1", t);
		linked_in = true;
	}

	if (action == TAGACT_HTML || action == TAGACT_DOCTYPE) {
		run_function_onearg_doc(cf, "eb$apch1", t);
		linked_in = true;
	}

	if (!t->parent && innerParent) {
// this is the top of innerHTML or some such.
// It is never html head or body, as those are skipped.
		run_function_onearg_t(innerParent, "eb$apch1", t);
		linked_in = true;
	}

	if (linked_in && fakePropLast[0]) {
// Node linked to document/gc to protect if from garbage collection,
// but now it is linked to its parent.
		delete_property_win(cf, fakePropLast);
	}

	if (!linked_in) {
		debugPrint(3, "tag %s not linked in", ti->name);
		if (action == TAGACT_TEXT)
			debugPrint(1, "text %s\n", t->textval);
	}

/* set innerHTML from the source html, if this tag supports it */
	if (ti->bits & TAG_INNERHTML)
		establish_inner(t, t->innerHTML, 0, false);

// If the tag has foo=bar as an attribute, pass this forward to javascript.
	pushAttributes(t);
}

static void pushAttributes(const Tag *t)
{
	int i;
	const char **a = t->attributes;
	const char **v = t->atvals;
	const char *u;
	char *x;
	if (!a)
		return;

	for (i = 0; a[i]; ++i) {
// There are some exceptions, some attributes that we handle individually.
// these are handled in domLink()
		static const char *const excdom[] = {
			"name", "id", "class", "classname",
			"checked", "value", "type",
			"href", "src", "action",
			0
		};
// These are on node.prototype, and you don't want to displace them.
// Sadly, you have to keep this list in sync with startwindow.js.
// These must be lower case, something about the attribute processing.
		static const char *const excprot[] = {
			"getelementsbytagname", "getelementsbyname",
			"getelementsbyclassname", "contains",
			"queryselectorall", "queryselector", "matches",
			"haschildnodes", "appendchild", "prependchild",
			"insertbefore", "replacechild",
			"eb$apch1", "eb$apch2", "eb$insbf", "removechild",
			"firstchild", "firstelementchild",
			"lastchild", "lastelementchild",
			"nextsibling", "nextelementsibling",
			"previoussibling", "previouselementsibling",
			"children",
			"hasattribute", "hasattributens", "markattribute",
			"getattribute", "getattributens",
			"setattribute", "setattributens",
			"removeattribute", "removeattributens",
			"parentelement",
			"getattributenode", "getclientrects",
			"clonenode", "importnode",
			"comparedocumentposition", "getboundingclientrect",
			"focus", "blur",
			"eb$listen", "eb$unlisten",
			"addeventlistener", "removeeventlistener",
			"attachevent", "detachevent", "dispatchevent",
			"insertadjacenthtml", "outerhtml",
			"injectsetup", "document_fragment_node",
			"classlist", "textcontent", "contenttext", "nodevalue",
// handlers are reserved yes, but they have important setters,
// so we do want to allow <A onclick=blah>
			0
		};
		static const char *const dotrue[] = {
			"required", "hidden", "aria-hidden",
			"multiple", "readonly", "disabled", "async", 0
		};

// html tags and attributes are case insensitive.
// That's why the entire getAttribute system drops names to lower case.
		u = v[i];
		if (!u)
			u = emptyString;
		x = cloneString(a[i]);
		caseShift(x, 'l');

// attributes on HTML tags that begin with "data-" should be available under a
// "dataset" object in JS
		if (strncmp(x, "data-", 5) == 0) {
// must convert to camelCase
			char *a2 = cloneString(x + 5);
			camelCase(a2);
			set_dataset_string_t(t, a2, u);
			nzFree(a2);
			run_function_onestring_t(t, "markAttribute",        x);
			nzFree(x);
			continue;
		}

		if (stringEqual(x, "style")) {	// no clue
			nzFree(x);
			continue;
		}

// check for exceptions.
// Maybe they wrote <a firstChild=foo>
		if( stringInList(excprot, a[i]) >= 0) {
			debugPrint(3, "html attribute overload %s.%s",
				   t->info->name, x);
			nzFree(x);
			continue;
		}

// There are some, like multiple or readonly, that should be set to true,
// not the empty string.
		if (stringInList(dotrue, a[i]) >= 0) {
			set_property_bool_t(t, x, !stringEqual(u, "false"));
		} else {
// standard attribute here
			                        if (stringInListCI(excdom, x) < 0)
				set_property_string_t(t, x, u);
		}
// special case, classname sets the class.
// Are there others like this?
		if(stringEqual(x, "classname"))
			run_function_onestring_t(t, "markAttribute", "class");
		else
			run_function_onestring_t(t, "markAttribute", x);
		nzFree(x);
	}
}

/* decorate the tree of nodes with js objects */
void decorate(int start)
{
	traverse_callback = jsNode;
	traverseAll(start);
}

/* Parse some html, as generated by innerHTML or document.write. */
void html_from_setter(Tag *t, const char *h)
{
	int l = cw->numTags;

	debugPrint(3, "parse html from innerHTML");
	debugPrint(4, "parse under tag %s %d", t->info->name, t->seqno);
	debugGenerated(h);

// Cut all the children away from t
	underKill(t);

	htmlGenerated = true;
	htmlScanner(h, t);
	prerender(0);
	innerParent = t;
	decorate(0);
	innerParent = 0;
	debugPrint(3, "end parse html from innerHTML");
}

void debugGenerated(const char *h)
{
	const char *h1, *h2;
	h1 = strstr(h, "<body>"); // it should be there
	h1 = h1 ? h1 + 6 : h;
// and </body> should be at the end
	h2 = h + strlen(h) - 7;
// Yeah this is one of those times I override const, but I put it back,
// so it's like a const string.
	*(char*)h2 = 0;
	if(debugLevel == 3) {
		if(strlen(h1) >= 200)
			debugPrint(3, "Generated ↑long↑");
		else
			debugPrint(3, "Generated ↑%s↑", h1);
	} else
		debugPrint(4, "Generated ↑%s↑", h1);
	*(char*)h2 = '<';
}

// Find window and frame based on the js context. Set cw and cf accordingly.
// This is inefficient, but is not called very often.
bool frameFromContext(jsobjtype cx)
{
	int i;
	Window *w;
	Frame *f;
	for (i = 0; i < MAXSESSION; ++i) {
		for (w = sessionList[i].lw; w; w = w->prev) {
			for (f = &(w->f0); f; f = f->next) {
				if(f->cx == cx) {
					cf = f, cw = w;
					return true;
				}
			}
		}
	}
	debugPrint(3, "frameFromContext cannot find the frame, job is not executed");
	return false;
}
