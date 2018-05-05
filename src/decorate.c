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
struct ebWindow *cw;
struct ebFrame *cf;

/* traverse the tree of nodes with a callback function */
nodeFunction traverse_callback;

/* possible callback functions in this file */
static void prerenderNode(struct htmlTag *node, bool opentag);
static void jsNode(struct htmlTag *node, bool opentag);
static void pushAttributes(const struct htmlTag *t);

static void processStyles(jsobjtype so, const char *stylestring);

static bool treeOverflow;

static void traverseNode(struct htmlTag *node)
{
	struct htmlTag *child;

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
}				/* traverseNode */

void traverseAll(int start)
{
	struct htmlTag *t;
	int i;

	treeOverflow = false;
	for (i = start; i < cw->numTags; ++i) {
		t = tagList[i];
		t->visited = false;
	}

	for (i = start; i < cw->numTags; ++i) {
		t = tagList[i];
		if (!t->parent && !t->slash && !t->dead)
			traverseNode(t);
	}

	if (treeOverflow)
		debugPrint(3, "malformed tree!");
}				/* traverseAll */

static int nopt;		/* number of options */
/* None of these tags nest, so it is reasonable to talk about
 * the current open tag. */
static struct htmlTag *currentForm, *currentSel, *currentOpt, *currentStyle;
static struct htmlTag *currentTitle, *currentScript, *currentTA;
static struct htmlTag *currentA;
static char *radioCheck;
static int radio_l;

const char *attribVal(const struct htmlTag *t, const char *name)
{
	const char *v;
	int j;
	if (!t->attributes)
		return 0;
	j = stringInListCI(t->attributes, name);
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
	t->step = 1;
	linkinTree(currentForm, t);
}				/* makeButton */

struct htmlTag *findOpenTag(struct htmlTag *t, int action)
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
}				/* findOpenTag */

struct htmlTag *findOpenList(struct htmlTag *t)
{
	while ((t = t->parent))
		if (t->action == TAGACT_OL || t->action == TAGACT_UL)
			return t;
	return 0;
}				/* findOpenList */

/*********************************************************************
tidy workaround functions.
Consider html like this.
<body>
<A href=http://www.edbrowse.org>Link1
<A href=http://www.edbrowse.org>Link2
<A href=http://www.edbrowse.org>Link3
</body>
Each anchor should close the one before, thus rendering as
 {Link1} {Link2} {Link3}
But tidy does not do this; it allows anchors to nest, thus
 {Link1{Link2{Link3}}}
Not a serious problem really, it just looks funny.
And yes, html like this does appear in the wild.
This routine restructures the tree to move the inner anchor
back up to the same level as the outer anchor.
*********************************************************************/

static void nestedAnchors(int start)
{
	struct htmlTag *a1, *a2, *p, *c;
	int j;

	for (j = start; j < cw->numTags; ++j) {
		a2 = tagList[j];
		if (a2->action != TAGACT_A)
			continue;
		a1 = findOpenTag(a2, TAGACT_A);
		if (!a1)
			continue;

/* delete a2 from the tree */
		p = a2->parent;
		a2->parent = 0;
		if (p->firstchild == a2)
			p->firstchild = a2->sibling;
		else {
			c = p->firstchild;
			while (c->sibling) {
				if (c->sibling == a2) {
					c->sibling = a2->sibling;
					break;
				}
				c = c->sibling;
			}
		}
		a2->sibling = 0;

/* then link a2 up next to a1 */
		a2->parent = a1->parent;
		a2->sibling = a1->sibling;
		a1->sibling = a2;
	}
}				/* nestedAnchors */

/*********************************************************************
Bad html will derail tidy, so that <a><div>stuff</div></a>
will push div outside the anchor, to render as  {} stuff
m.facebook.com is loaded with them.
Here is a tiny example.

<body>
<input type=button name=whatever value=hohaa>
<a href="#bottom"><div>Cognitive business is here</div></a>
</body>

This routine puts it back.
An anchor with no children followd by div
moves div under the anchor.
For a while I had this function commented out, like it caused a problem,
but I can't see why or how, so it's back, and facebook looks better.

As an after kludge, don't move <div> under <a> if <div> has an anchor beneath it.
That could create nested anchors, which we already worked hard to get rid of.   Eeeeeeesh.

This and other tidy workaround functions are based on heuristics,
and suffer from false positives and false negatives,
the former being the more serious problem -
i.e. we rearrange the tree when we shouldn't.
Even when we do the right thing, there is another problem,
innerHTML is wrong, and doesn't match the tree of nodes
or the original source.
innerHTML comes to us from tidy, after it has fixed (sometimes broken) things.
Add <script> to the above, browse, jdb, and look at document.body.innerHTML.
It does not match the source, in fact it represent the tree *before* we fixed it.
There really isn't anything I can do about that.
In so many ways, the better approach is to fix tidy, but sometimes that is out of our hands.
*********************************************************************/

static bool tagBelow(struct htmlTag *t, int action)
{
	struct htmlTag *c;

	if (t->action == action)
		return true;
	for (c = t->firstchild; c; c = c->sibling)
		if (tagBelow(c, action))
			return true;
	return false;
}				/* tagBelow */

static void emptyAnchors(int start)
{
	int j;
	struct htmlTag *a0, *div, *up;

	for (j = start; j < cw->numTags; ++j) {
		a0 = tagList[j];
		if (a0->action != TAGACT_A || a0->firstchild)
			continue;
// anchor no children
		for (up = a0; up; up = up->parent)
			if (up->sibling)
				break;
		if (!up || !(div = up->sibling) || div->action != TAGACT_DIV)
			continue;
// div follows
/* would moving this create nested anchors? */
		if (tagBelow(div, TAGACT_A))
			continue;
/* shouldn't have inputs or forms in an anchor. */
		if (tagBelow(div, TAGACT_INPUT))
			continue;
		if (tagBelow(div, TAGACT_FORM))
			continue;
		up->sibling = div->sibling;
		a0->firstchild = div;
		div->parent = a0;
		div->sibling = 0;
	}
}				/* emptyAnchors */

/*********************************************************************
If a form is in a table, but not in tr or td, it closes immediately,
and all the following inputs are orphaned.
Check for an empty form beneath table, and move all the following siblings
down into the form.
*********************************************************************/

static void tableForm(int start)
{
	int j;
	struct htmlTag *form, *table, *t;

	for (j = start; j < cw->numTags; ++j) {
		form = tagList[j];
		if (form->action != TAGACT_FORM || form->firstchild)
			continue;
		t = form;
		for (table = form->sibling; table; table = table->sibling) {
			if (table->action == TAGACT_TABLE &&
			    tagBelow(table, TAGACT_INPUT)) {
/* table with inputs below; move it to form */
/* hope this doesn't break anything */
				table->parent = form;
				form->firstchild = table;
				t->sibling = table->sibling;
				table->sibling = 0;
				break;
			}
			t = table;
		}
	}
}				/* tableForm */

void formControl(struct htmlTag *t, bool namecheck)
{
	int itype = t->itype;
	char *myname = (t->name ? t->name : t->id);
	struct htmlTag *cform = currentForm;
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
}				/* formControl */

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
void htmlInputHelper(struct htmlTag *t)
{
	int n = INP_TEXT;
	int len;
	char *myname = (t->name ? t->name : t->id);
	const char *s = attribVal(t, "type");
	if (stringEqual(t->info->name, "button")) {
		n = INP_BUTTON;
	}
	if (s) {
		n = stringInListCI(inp_types, s);
		if (n < 0) {
			n = stringInListCI(inp_others, s);
			if (n < 0)
				debugPrint(3, "unrecognized input type %s", s);
			else
				t->itype_minor = n;
			if (n == INP_PW)
				t->masked = true;
			n = INP_TEXT;
		}
	}
	t->itype = n;

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
}				/* htmlInputHelper */

/* return an allocated string containing the text entries for the checked options */
char *displayOptions(const struct htmlTag *sel)
{
	const struct htmlTag *t;
	char *opt;
	int opt_l;
	int i;

	opt = initString(&opt_l);
	for (i = 0; i < cw->numTags; ++i) {
		t = tagList[i];
		if (t->controller != sel)
			continue;
		if (!t->checked)
			continue;
		if (*opt)
			stringAndChar(&opt, &opt_l, ',');
		stringAndString(&opt, &opt_l, t->textval);
	}

	return opt;
}				/* displayOptions */

static void prerenderNode(struct htmlTag *t, bool opentag)
{
	int itype;		/* input type */
	int j;
	int action = t->action;
	const char *a;		/* usually an attribute */
	struct htmlTag *cdt;

	debugPrint(6, "prend %c%s %d%s",
		   (opentag ? ' ' : '/'), t->info->name,
		   t->seqno, (t->step >= 1 ? "-" : ""));

	if (t->step >= 1)
		return;
	if (!opentag)
		t->step = 1;

	switch (action) {
	case TAGACT_NOSCRIPT:
// If javascript is enabled kill everything under noscript
		if (allowJS && !opentag)
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
			currentStyle->textval = cloneString(t->textval);
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
// Don't need leading whitespace.
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
		if (!t->value)
			t->value = emptyString;
		t->textval = emptyString;
		break;

	case TAGACT_STYLE:
		if (!opentag) {
			currentStyle = 0;
			break;
		}
		currentStyle = t;
		t->textval = emptyString;
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
			if (whichproc == 'e') {
				j = sideBuffer(0, t->value, -1, 0);
				t->lic = j;
			}
			currentTA = 0;
		}
		break;

	case TAGACT_META:
		if (opentag) {
/* This function doesn't do anything inside the js process.
 * It only works when scanning the original web page.
 * Thus I assume meta tags that set cookies, or keywords, or description,
 * or a refresh directive, are there from the get-go.
 * If js was going to generate a cookie it would just set document.cookie,
 * it wouldn't build a meta tag to set the cookie and then
 * appendChild it to head, right? */
			htmlMetaHelper(t);
		}
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
		if (!(a = t->class))
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
// If somebody wrote <frame><p><a></frame>, those tags should be excised,
		underKill(t);
		cdt = newTag("document");
		t->firstchild = cdt;
		cdt->parent = t;
		break;

	}			/* switch */
}				/* prerenderNode */

void prerender(int start)
{
/* some cleanup routines to rearrange the tree, working around some tidy5 bugs. */
	nestedAnchors(start);
	emptyAnchors(start);
	tableForm(start);

	currentForm = currentSel = currentOpt = NULL;
	currentTitle = currentScript = currentTA = NULL;
	currentStyle = NULL;
	nzFree(radioCheck);
	radioCheck = 0;
	traverse_callback = prerenderNode;
	traverseAll(start);
	currentForm = NULL;
	nzFree(radioCheck);
	radioCheck = 0;
}				/* prerender */

/* create a new url with constructor */
jsobjtype instantiate_url(jsobjtype parent, const char *name, const char *url)
{
	jsobjtype uo;		/* url object */
	uo = instantiate(parent, name, "URL");
	if (uo)
		set_property_string(uo, "href", url);
	return uo;
}				/* instantiate_url */

static void handlerSet(jsobjtype ev, const char *name, const char *code)
{
	char *newcode = allocMem(strlen(code) + 60);
	strcpy(newcode, "(function(){");

/*********************************************************************
I read somewhere, long ago, that handlers are suppose to run in the context of
the document object and sometimes the form that houses them.
So I should put with(document) and sometimes with(form) around the code.
For onclick, the code just runs, but for onsubmit the code is suppose to
return true or false.
Mozilla had no trouble compiling and running  return true;  at top level.
Duktape won't do that. Return has to be in a function.
I even tried DUK_COMPILE_FUNCTION, no dice.
So I tried wrapping the code in
(function() { code })();
Then it doesn't matter if the code is just expression, or return expression.
But should the with clauses be inside or outside the function?
Also, the this binding was lost,
which is absolutely necessary.
Sami taught me how to get the binding back, via the .bind method,
which is pretty dog gone cool.
But what about the with clauses?
Well so far I haven't found an example that needs with(document) around it,
so I'm not going to worry about that right now.

	strcpy(newcode, "with(document) { ");
	enum ej_proptype hasform = typeof_property(ev, "form");
	if (hasform)
		strcat(newcode, "with(this.form) { ");
*********************************************************************/

	strcat(newcode, code);

/*********************************************************************
	if (hasform)
		strcat(newcode, " }");
	strcat(newcode, " }");
*********************************************************************/

	strcat(newcode, " }.bind(this))()");
	set_property_function(ev, name, newcode);
	nzFree(newcode);
}				/* handlerSet */

static void set_onhandler(const struct htmlTag *t, const char *name)
{
	const char *s;
	if (t->jv) {
		s = attribVal(t, name);
		if (s)
			handlerSet(t->jv, name, s);
	}
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

static char fakePropLast[24];
static const char *fakePropName(void)
{
	static int idx = 0;
	++idx;
	sprintf(fakePropLast, "gc$%c%d", whichproc, idx);
	return fakePropLast;
}				/*fakePropName */

static jsobjtype establish_js_option(jsobjtype obj, int idx)
{
	jsobjtype oa;		/* option array */
	jsobjtype oo;		/* option object */
	jsobjtype so;		// style object
	jsobjtype ato;		// attributes object
	jsobjtype fo;		/* form object */

	if ((oa = get_property_object(obj, "options")) == NULL)
		return NULL;
	if ((oo = instantiate_array_element(oa, idx, "Option")) == NULL)
		return NULL;

	set_property_object(oo, "parentNode", oa);

/* option.form = select.form */
	fo = get_property_object(obj, "form");
	if (fo)
		set_property_object(oo, "form", fo);
	instantiate_array(oo, "childNodes");
	ato = instantiate(oo, "attributes", "NamedNodeMap");
	set_property_object(ato, "owner", oo);
	so = instantiate(oo, "style", "CSSStyleDeclaration");
	set_property_object(so, "element", oo);

	return oo;
}				/* establish_js_option */

static void establish_inner(jsobjtype obj, const char *start, const char *end,
			    bool isText)
{
	const char *s = emptyString;
	const char *name = (isText ? "innerText" : "innerHTML");
	if (start) {
		s = start;
		if (end)
			s = pullString(start, end - start);
	}
	set_property_string(obj, name, s);
	if (start && end)
		nzFree((char *)s);
}				/* establish_inner */

static void domLink(struct htmlTag *t, const char *classname,	/* instantiate this class */
		    const char *href, const char *list,	/* next member of this array */
		    jsobjtype owner, int radiosel)
{
	jsobjtype master;
	jsobjtype alist = 0;
	jsobjtype io = 0;	/* input object */
	int length;
	bool dupname = false;
/* some strings from the html tag */
	const char *symname = t->name;
	const char *idname = t->id;
	const char *membername = 0;	/* usually symname */
	const char *href_url = t->href;
	const char *stylestring = attribVal(t, "style");
	jsobjtype so = 0;	/* obj.style */
	jsobjtype ato = 0;	/* obj.attributes */

	debugPrint(5, "domLink %s.%d name %s",
		   classname, radiosel, (symname ? symname : emptyString));

	if (symname && typeof_property(owner, symname)) {
/*********************************************************************
This could be a duplicate name.
Yes, that really happens.
Link to the first tag having this name,
and link the second tag under a fake name so gc won't throw it away.
Or - it could be a duplicate name because multiple radio buttons
all share the same name.
The first time we create the array,
and thereafter we just link under that array.
Or - and this really does happen -
an input tag could have the name action, colliding with form.action.
I have no idea what to do here.
radiosel is 1 for radio buttons and 2 for select.
*********************************************************************/

		if (radiosel == 1) {
/* name present and radio buttons, name should be the array of buttons */
			io = get_property_object(owner, symname);
			if (io == NULL)
				return;
		} else {
/* don't know why the duplicate name */
			dupname = true;
		}
	}

/* The input object is nonzero if&only if the input is a radio button,
 * and not the first button in the set, thus it isce the array containing
 * these buttons. */
	if (io == NULL) {
/*********************************************************************
Ok, the above condition does not hold.
We'll be creating a new object under owner, but through what name?
The name= tag, unless it's a duplicate,
or id= if there is no name=, or a fake name just to protect it from gc.
*********************************************************************/

		if (!symname && idname) {
			membername = idname;
		} else if (symname && !dupname) {
			membername = symname;
		}
/* id= or name= must not displace submit, reset, or action in form.
 * Example www.startpage.com, where id=submit.
 * nor should it collide with another attribute, such as document.cookie and
 * <div ID=cookie> in www.orange.com.
 * This call checks for the name in the object and its prototype. */
		if (has_property(owner, membername)) {
			debugPrint(3, "membername overload %s.%s", classname,
				   membername);
			membername = NULL;
		}
		if (!membername)
			membername = fakePropName();

		if (radiosel) {
/* The first radio button, or input type=select */
/* Either way the form element is suppose to be an array. */
			io = instantiate_array(owner, membername);
			if (io == NULL)
				return;
			if (radiosel == 1) {
				set_property_string(io, "type", "radio");
				set_property_string(io, "nodeName", "radio");
			} else {
/* I've read some docs that say select is itself an array,
 * and then references itself as an array of options.
 * Self referencing? Really? Well it seems to work. */
				set_property_object(io, "options", io);
				set_property_object(io, "childNodes", io);
				set_property_number(io, "selectedIndex", -1);
			}
		} else {
/* A standard input element, just create it. */
			io = instantiate(owner, membername, classname);
			if (io == NULL)
				return;

/* not an array; needs the childNodes array beneath it for the children */
			instantiate_array(io, "childNodes");

/* in the special case of form, also need an array of elements */
			if (t->action == TAGACT_FORM)
				instantiate_array(io, "elements");
		}

/* deal with the 'styles' here.
object will get 'style' regardless of whether there is
anything to put under it, just like it gets childNodes whether
or not there are any.  After that, there is a conditional step.
If this node contains style='' of one or more name-value pairs,
call out to process those and add them to the object.
Don't do any of this if the tag is itself <style>. */
		if (t->action != TAGACT_STYLE) {
			so = instantiate(io, "style", "CSSStyleDeclaration");
			set_property_object(so, "element", io);
/* now if there are any style pairs to unpack,
 processStyles can rely on obj.style existing */
			if (stylestring)
				processStyles(so, stylestring);
		}

/* Other attributes that are expected by pages, even if they
 * aren't populated at domLink-time */
		set_property_string(io, "class", "");
		set_property_string(io, "nodeValue", "");
		ato = instantiate(io, "attributes", "NamedNodeMap");
		set_property_object(ato, "owner", io);
		set_property_object(io, "ownerDocument", cf->docobj);

		if (membername == symname) {
/* link to document.all */
			master = get_property_object(cf->docobj, "all");
			if (master == NULL)
				return;
			set_property_object(master, symname, io);

			if (stringEqual(symname, "action"))
				set_property_bool(io, "actioncrash", true);
		}

		if (list)
			alist = get_property_object(owner, list);
		if (alist) {
			length = get_arraylength(alist);
			if (length < 0)
				return;
			set_array_element_object(alist, length, io);
			if (symname && !dupname
			    && !has_property(alist, symname))
				set_property_object(alist, symname, io);
			if (idname && symname != idname
			    && !has_property(alist, idname))
				set_property_object(alist, idname, io);
		}		/* list indicated */
	}

	if (radiosel == 1) {
/* drop down to the element within the radio array, and return that element */
/* w becomes the object associated with this radio button */
/* io is, by assumption, an array */
		jsobjtype w;
		length = get_arraylength(io);
		if (length < 0)
			return;
		w = instantiate_array_element(io, length, "Element");
		if (w == NULL)
			return;
		io = w;
	}

	if (symname)
		set_property_string(io, "name", symname);

	if (idname) {
/* io.id becomes idname, and idMaster.idname becomes io */
		set_property_string(io, "id", idname);
		master = get_property_object(cf->docobj, "idMaster");
		set_property_object(master, idname, io);
	}

	if (href && href_url)
// This use to be instantiate_url, but with the new side effects
// on Anchor, Image, etc, we can just set the string.
		set_property_string(io, href, href_url);

	if (t->action == TAGACT_INPUT) {
/* link back to the form that owns the element */
		set_property_object(io, "form", owner);
	}

	connectTagObject(t, io);

	set_property_string(io, "nodeName", t->info->name);
/* documentElement is now set in the "Body" case because the 
"Html" does not appear ever to be encountered */

	if (t->action == TAGACT_BODY) {
		set_property_object(cf->docobj, "body", io);
		set_property_object(cf->docobj, "documentElement", io);
	}

	if (t->action == TAGACT_HEAD) {
		set_property_object(cf->docobj, "head", io);
	}

}				/* domLink */

static const char defvl[] = "defaultValue";
static const char defck[] = "defaultChecked";
static const char defsel[] = "defaultSelected";

static void formControlJS(struct htmlTag *t)
{
	const char *typedesc;
	int itype = t->itype;
	int isradio = itype == INP_RADIO;
	int isselect = (itype == INP_SELECT) * 2;
	const struct htmlTag *form = t->controller;

	if (form && form->jv)
		domLink(t, "Element", 0, "elements", form->jv,
			isradio | isselect);
	else
		domLink(t, "Element", 0, 0, cf->docobj, isradio | isselect);
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

	connectTagObject(t, establish_js_option(sel->jv, t->lic));
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

static void link_css(struct htmlTag *t)
{
	struct i_get g;
	char *b;
	int blen;
	const char *a;
	const char *a1 = attribVal(t, "type");
	const char *a2 = attribVal(t, "rel");
	if (a1)
		set_property_string(t->jv, "type", a1);
	if (a2)
		set_property_string(t->jv, "rel", a2);
	if (!t->href)
		return;
	if ((!a1 || !stringEqualCI(a1, "text/css")) &&
	    (!a2 || !stringEqualCI(a2, "stylesheet")))
		return;
// Fetch the css file so we can apply its attributes.
	a = NULL;
	if (browseLocal && !isURL(t->href)) {
		debugPrint(3, "css source %s", t->href);
		if (!fileIntoMemory(t->href, &b, &blen)) {
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
			if (g.code == 200) {
				a = force_utf8(g.buffer, g.length);
				if (!a)
					a = g.buffer;
				else
					nzFree(g.buffer);
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
	if (a)
		set_property_string(t->jv, "data", a);
	cnzFree(a);
}				/* link_css */

static jsobjtype innerParent;

static void jsNode(struct htmlTag *t, bool opentag)
{
	const struct tagInfo *ti = t->info;
	int action = t->action;
	const struct htmlTag *above;
	const char *a;
	bool linked_in;

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
		connectTagObject(t,
				 instantiate(cf->docobj, fakePropName(),
					     "TextNode"));
		if (t->jv) {
			const char *w = t->textval;
			if (!w)
				w = emptyString;
			set_property_string(t->jv, "data", w);
		}
		break;

	case TAGACT_META:
		domLink(t, "Meta", 0, "metas", cf->docobj, 0);
		a = attribVal(t, "content");
		set_property_string(t->jv, "content", a);
		set_property_number(t->jv, "nodeType", 1);
		break;

	case TAGACT_STYLE:
		domLink(t, "CSSStyleDeclaration", 0, "styles", cf->docobj, 0);
		a = attribVal(t, "type");
		if (!a)
			a = emptyString;
		set_property_string(t->jv, "type", a);
		set_property_number(t->jv, "nodeType", 1);
		if (t->textval && t->textval[0])
			set_property_string(t->jv, "data", t->textval);
		break;

	case TAGACT_SCRIPT:
		domLink(t, "Script", "src", "scripts", cf->docobj, 0);
		a = attribVal(t, "type");
		if (a)
			set_property_string(t->jv, "type", a);
		a = attribVal(t, "language");
		if (a)
			set_property_string(t->jv, "language", a);
		a = attribVal(t, "src");
		if (a) {
			set_property_string(t->jv, "src", a);
		} else {
			set_property_string(t->jv, "src", "");
		}
		a = attribVal(t, "data");
		if (a) {
			set_property_string(t->jv, "data", a);
		} else {
			set_property_string(t->jv, "data", "");
		}
		set_property_number(t->jv, "nodeType", 1);
		break;

	case TAGACT_FORM:
		domLink(t, "Form", "action", "forms", cf->docobj, 0);
		set_onhandlers(t);
		set_property_number(t->jv, "nodeType", 1);
		break;

	case TAGACT_INPUT:
		formControlJS(t);
		if (t->itype == INP_TA)
			establish_inner(t->jv, t->value, 0, true);
		set_property_number(t->jv, "nodeType", 1);
		break;

	case TAGACT_OPTION:
		optionJS(t);
// The parent child relationship has already been established,
// don't break, just return;
		set_property_number(t->jv, "nodeType", 1);
		return;

	case TAGACT_A:
		domLink(t, "Anchor", "href", "anchors", cf->docobj, 0);
		set_onhandlers(t);
		set_property_number(t->jv, "nodeType", 1);
		break;

	case TAGACT_HEAD:
		domLink(t, "Head", 0, "heads", cf->docobj, 0);
		set_property_number(t->jv, "nodeType", 1);
		cf->headtag = t;
		break;

	case TAGACT_BODY:
		domLink(t, "Body", 0, "bodies", cf->docobj, 0);
		set_onhandlers(t);
		set_property_number(t->jv, "nodeType", 1);
		cf->bodytag = t;
		break;

	case TAGACT_OL:
	case TAGACT_UL:
	case TAGACT_DL:
		domLink(t, "Lister", 0, 0, cf->docobj, 0);
		set_property_number(t->jv, "nodeType", 1);
		break;

	case TAGACT_LI:
		domLink(t, "Listitem", 0, 0, cf->docobj, 0);
		set_property_number(t->jv, "nodeType", 1);
		break;

	case TAGACT_TABLE:
		domLink(t, "Table", 0, "tables", cf->docobj, 0);
/* create the array of rows under the table */
		instantiate_array(t->jv, "rows");
		set_property_number(t->jv, "nodeType", 1);
		break;

	case TAGACT_TR:
		if ((above = t->controller) && above->jv) {
			domLink(t, "Trow", 0, "rows", above->jv, 0);
			instantiate_array(t->jv, "cells");
		}
		set_property_number(t->jv, "nodeType", 1);
		break;

	case TAGACT_TD:
		if ((above = t->controller) && above->jv) {
			domLink(t, "Cell", 0, "cells", above->jv, 0);
		}
		set_property_number(t->jv, "nodeType", 1);
		break;

	case TAGACT_DIV:
		domLink(t, "Div", 0, "divs", cf->docobj, 0);
		set_property_number(t->jv, "nodeType", 1);
		break;

	case TAGACT_OBJECT:
		domLink(t, "HtmlObj", 0, "htmlobjs", cf->docobj, 0);
		set_property_number(t->jv, "nodeType", 1);
		break;

	case TAGACT_SPAN:
	case TAGACT_SUB:
	case TAGACT_SUP:
	case TAGACT_OVB:
		domLink(t, "Span", 0, "spans", cf->docobj, 0);
		set_property_number(t->jv, "nodeType", 1);
		set_onhandlers(t);
		break;

	case TAGACT_AREA:
		domLink(t, "Area", "href", "areas", cf->docobj, 0);
		set_property_number(t->jv, "nodeType", 1);
		break;

	case TAGACT_FRAME:
// about:blank means a blank frame with no sourcefile.
		if (stringEqual(t->href, "about:blank")) {
			nzFree(t->href);
			t->href = 0;
		}
		domLink(t, "Frame", "src", "frames", cf->winobj, 0);
		set_onhandlers(t);
		set_property_number(t->jv, "nodeType", 1);
		break;

	case TAGACT_IMAGE:
		domLink(t, "Image", "src", "images", cf->docobj, 0);
		set_property_number(t->jv, "nodeType", 1);
		break;

	case TAGACT_P:
		domLink(t, "P", 0, "paragraphs", cf->docobj, 0);
		set_property_number(t->jv, "nodeType", 1);
		break;

	case TAGACT_HEADER:
		domLink(t, "Header", 0, "headers", cf->docobj, 0);
		set_property_number(t->jv, "nodeType", 1);
		break;

	case TAGACT_FOOTER:
		domLink(t, "Footer", 0, "footers", cf->docobj, 0);
		set_property_number(t->jv, "nodeType", 1);
		break;

	case TAGACT_TITLE:
		if (cw->htmltitle)
			set_property_string(cf->docobj, "title", cw->htmltitle);
// Title is not a node, more like an attribute of the page.
		break;

	case TAGACT_LINK:
		domLink(t, "Link", "href", "links", cf->docobj, 0);
		set_property_number(t->jv, "nodeType", 1);
		link_css(t);
		break;

	default:
// Don't know what this tag is, or it's not semantically important,
// so just call it an html element.
		domLink(t, "Element", 0, 0, cf->docobj, 0);
		if (t->action == TAGACT_BASE && t->href)
			instantiate_url(t->jv, "href", t->href);
		break;
	}			/* switch */

	if (!t->jv)
		return;		/* nothing else to do */

/* js tree mirrors the dom tree. */
/* but head and body link to document */
	linked_in = false;
	if (action == TAGACT_HEAD || action == TAGACT_BODY) {
		run_function_onearg(cf->docobj, "eb$apch1", t->jv);
		linked_in = true;
	} else {
		if (t->parent && t->parent->jv) {
			run_function_onearg(t->parent->jv, "eb$apch1", t->jv);
			linked_in = true;
// special code for frame.contentDocument.
			if (t->parent->action == TAGACT_FRAME) {
				set_property_object(t->parent->jv,
						    "contentDocument", t->jv);
				set_property_object(t->parent->jv,
						    "contentWindow", t->jv);
			}
		}

		if (!t->parent && innerParent) {
			run_function_onearg(innerParent, "eb$apch1", t->jv);
			linked_in = true;
		}
	}

	if (linked_in && fakePropLast[0]) {
// Node linked to document/gc to protect if from garbage collection,
// but now it is linked to its parent.
		delete_property(cf->docobj, fakePropLast);
	}

/* set innerHTML from the source html, if this tag supports it */
	if (ti->bits & TAG_INNERHTML)
		establish_inner(t->jv, t->innerHTML, 0, false);

// If the tag has foo=bar as an attribute, pass this forward to javascript.
	pushAttributes(t);
}				/* jsNode */

static void pushAttributes(const struct htmlTag *t)
{
	int i;
	const char **a = t->attributes;
	const char **v = t->atvals;
	if (!a)
		return;
	for (i = 0; a[i]; ++i) {
// There are some exceptions, some attributes that we handle individually.
		const char *const exclist[] = {
			"onclick", "onchange", "onsubmit", "onreset", "onload",
			"onunload",
			"name", "id",
			"checked", "value", "type", "style",
			"href", "src", "action",
			0
		};
		const char *const dotrue[] = {
			"multiple", "readonly", "disabled", 0
		};
		const char *u;
		if (stringInListCI(exclist, a[i]) >= 0)
			continue;
// I surely haven't thought of everything, so check generally.
// Maybe they wrote <a firstChild=foo>
// See if the name is specifically in the prototype.
		if (has_property(t->jv, a[i]) && !typeof_property(t->jv, a[i])) {
			debugPrint(3, "html attribute overload %s.%s",
				   t->info->name, a[i]);
			continue;
		}
// Should we drop attribute name to lower case? I don't, for now.
		u = v[i];
		if (!u)
			u = emptyString;
// There are some, like multiple or readonly, that should be set to true,
// not the empty string.
		if (!*u && stringInList(dotrue, a[i]) >= 0) {
			set_property_bool(t->jv, a[i], true);
			continue;
		}
		set_property_string(t->jv, a[i], u);
	}
}				/* pushAttributes */

/* decorate the tree of nodes with js objects */
void decorate(int start)
{
	traverse_callback = jsNode;
	traverseAll(start);
}				/* decorate */

/* paranoia check on the number of tags */
static void tagCountCheck(void)
{
	if (sizeof(int) == 4) {
		if (cw->numTags > MAXLINES)
			i_printfExit(MSG_LineLimit);
	}
}				/* tagCountCheck */

static void pushTag(struct htmlTag *t)
{
	int a = cw->allocTags;
	if (cw->numTags == a) {
		debugPrint(4, "%d tags, %d dead", a, cw->deadTags);
/* make more room */
		a = a / 2 * 3;
		cw->tags =
		    (struct htmlTag **)reallocMem(cw->tags, a * sizeof(t));
		cw->allocTags = a;
	}
	tagList[cw->numTags++] = t;
	tagCountCheck();
}				/* pushTag */

static void freeTag(struct htmlTag *t);

// garbage collect the dead tags.
// You must rerender after this runs, so that the buffer has no dead tags,
// and the remaining tags have their new numbers embedded in the buffer.
void tag_gc(void)
{
	int cx;			/* edbrowse context */
	struct ebWindow *w, *save_cw;
	struct htmlTag *t;
	int i, j;

	for (cx = 1; cx <= maxSession; ++cx) {
		for (w = sessionList[cx].lw; w; w = w->prev) {
			if (!w->tags)
				continue;
// Don't bother unless a third of the tags are dead.
			if (w->deadTags * 3 < w->numTags)
				continue;

// sync any changed fields before we muck with the tags.
			save_cw = cw;
			cw = w;
			cf = &(cw->f0);
			jSyncup(true);
			cw = save_cw;
			cf = &(cw->f0);

// ok let's crunch.
			for (i = j = 0; i < w->numTags; ++i) {
				t = w->tags[i];
				if (t->dead) {
					freeTag(t);
				} else {
					t->seqno = j;
					w->tags[j++] = t;
				}
			}
			debugPrint(4, "tag_gc from %d to %d", w->numTags, j);
			w->numTags = j;
			w->deadTags = 0;

// We must rerender when we return to this window,
// or at the input loop if this is the current window.
// Tags have been renumbered, need to rebuild the text buffer accordingly.
			w->mustrender = true;
			if (w != cw)
				w->nextrender = 0;
		}
	}
}

/* first three have to be in this order */
const struct tagInfo availableTags[] = {
	{"html", "html", TAGACT_ZERO},
	{"base", "base reference for relative URLs", TAGACT_BASE, 0, 4},
	{"object", "an html object", TAGACT_OBJECT, 5, 1},
	{"a", "an anchor", TAGACT_A, 0, 1},
	{"input", "an input item", TAGACT_INPUT, 0, 4},
	{"element", "an input element", TAGACT_INPUT, 0, 4},
	{"title", "the title", TAGACT_TITLE, 0, 0},
	{"textarea", "an input text area", TAGACT_TA, 0, 0},
	{"select", "an option list", TAGACT_SELECT, 0, 0},
	{"option", "a select option", TAGACT_OPTION, 0, 0},
	{"sub", "a subscript", TAGACT_SUB, 0, 0},
	{"sup", "a superscript", TAGACT_SUP, 0, 0},
	{"ovb", "an overbar", TAGACT_OVB, 0, 0},
	{"font", "a font", TAGACT_NOP, 0, 0},
	{"cite", "a citation", TAGACT_NOP, 0, 0},
	{"tt", "teletype", TAGACT_NOP, 0, 0},
	{"center", "centered text", TAGACT_P, 2, 5},
	{"caption", "a caption", TAGACT_NOP, 5, 0},
	{"head", "the html header information", TAGACT_HEAD, 0, 5},
	{"body", "the html body", TAGACT_BODY, 0, 5},
	{"text", "a text section", TAGACT_TEXT, 0, 4},
	{"bgsound", "background music", TAGACT_MUSIC, 0, 4},
	{"audio", "audio passage", TAGACT_MUSIC, 0, 4},
	{"meta", "a meta tag", TAGACT_META, 0, 4},
	{"style", "a style tag", TAGACT_STYLE, 0, 2},
	{"link", "a link tag", TAGACT_LINK, 0, 4},
	{"img", "an image", TAGACT_IMAGE, 0, 4},
	{"image", "an image", TAGACT_IMAGE, 0, 4},
	{"br", "a line break", TAGACT_BR, 1, 4},
	{"p", "a paragraph", TAGACT_P, 2, 5},
	{"header", "a header", TAGACT_HEADER, 2, 5},
	{"footer", "a footer", TAGACT_FOOTER, 2, 5},
	{"div", "a divided section", TAGACT_DIV, 5, 1},
	{"map", "a map of images", TAGACT_NOP, 5, 0},
	{"blockquote", "a quoted paragraph", TAGACT_NOP, 10, 1},
	{"document", "a document", TAGACT_NOP, 5, 1},
	{"h1", "a level 1 header", TAGACT_NOP, 10, 1},
	{"h2", "a level 2 header", TAGACT_NOP, 10, 1},
	{"h3", "a level 3 header", TAGACT_NOP, 10, 1},
	{"h4", "a level 4 header", TAGACT_NOP, 10, 1},
	{"h5", "a level 5 header", TAGACT_NOP, 10, 1},
	{"h6", "a level 6 header", TAGACT_NOP, 10, 1},
	{"dt", "a term", TAGACT_DT, 2, 4},
	{"dd", "a definition", TAGACT_DD, 1, 4},
	{"li", "a list item", TAGACT_LI, 1, 5},
	{"ul", "a bullet list", TAGACT_UL, 10, 1},
	{"dir", "a directory list", TAGACT_NOP, 5, 0},
	{"menu", "a menu", TAGACT_NOP, 5, 0},
	{"ol", "a numbered list", TAGACT_OL, 10, 1},
	{"dl", "a definition list", TAGACT_DL, 10, 1},
	{"hr", "a horizontal line", TAGACT_HR, 5, 4},
	{"form", "a form", TAGACT_FORM, 10, 1},
	{"button", "a button", TAGACT_INPUT, 0, 0},
	{"frame", "a frame", TAGACT_FRAME, 2, 0},
	{"iframe", "a frame", TAGACT_FRAME, 2, 1},
	{"map", "an image map", TAGACT_MAP, 2, 4},
	{"area", "an image map area", TAGACT_AREA, 0, 4},
	{"table", "a table", TAGACT_TABLE, 10, 1},
	{"tbody", "a table body", TAGACT_TBODY, 0, 1},
	{"tr", "a table row", TAGACT_TR, 5, 1},
	{"td", "a table entry", TAGACT_TD, 0, 5},
	{"th", "a table heading", TAGACT_TD, 0, 5},
	{"pre", "a preformatted section", TAGACT_PRE, 10, 0},
	{"listing", "a listing", TAGACT_PRE, 1, 0},
	{"xmp", "an example", TAGACT_PRE, 1, 0},
	{"fixed", "a fixed presentation", TAGACT_NOP, 1, 0},
	{"code", "a block of code", TAGACT_NOP, 0, 0},
	{"samp", "a block of sample text", TAGACT_NOP, 0, 0},
	{"address", "an address block", TAGACT_NOP, 1, 0},
	{"style", "a style block", TAGACT_NOP, 0, 2},
	{"script", "a script", TAGACT_SCRIPT, 0, 0},
	{"noscript", "no script section", TAGACT_NOSCRIPT, 0, 2},
	{"noframes", "no frames section", TAGACT_NOP, 0, 2},
	{"embed", "embedded html", TAGACT_MUSIC, 0, 4},
	{"noembed", "no embed section", TAGACT_NOP, 0, 2},
	{"em", "emphasized text", TAGACT_JS, 0, 0},
	{"label", "a label", TAGACT_JS, 0, 0},
	{"strike", "emphasized text", TAGACT_JS, 0, 0},
	{"s", "emphasized text", TAGACT_JS, 0, 0},
	{"strong", "emphasized text", TAGACT_JS, 0, 0},
	{"b", "bold text", TAGACT_JS, 0, 0},
	{"i", "italicized text", TAGACT_JS, 0, 0},
	{"u", "underlined text", TAGACT_JS, 0, 0},
	{"var", "variable text", TAGACT_JS, 0, 0},
	{"kbd", "keyboard text", TAGACT_JS, 0, 0},
	{"dfn", "definition text", TAGACT_JS, 0, 0},
	{"q", "quoted text", TAGACT_JS, 0, 0},
	{"abbr", "an abbreviation", TAGACT_JS, 0, 0},
	{"span", "an html span", TAGACT_SPAN, 0, 1},
	{"frameset", "a frame set", TAGACT_JS, 0, 0},
	{"", NULL, 0}
};

static void freeTag(struct htmlTag *t)
{
	char **a;
	nzFree(t->textval);
	nzFree(t->name);
	nzFree(t->id);
	nzFree(t->class);
	nzFree(t->nodeName);
	nzFree(t->value);
	cnzFree(t->rvalue);
	nzFree(t->href);
	nzFree(t->js_file);
	nzFree(t->innerHTML);

	a = (char **)t->attributes;
	if (a) {
		while (*a) {
			nzFree(*a);
			++a;
		}
		free(t->attributes);
	}

	a = (char **)t->atvals;
	if (a) {
		while (*a) {
			nzFree(*a);
			++a;
		}
		free(t->atvals);
	}

	free(t);
}

void freeTags(struct ebWindow *w)
{
	int i, n;
	struct htmlTag *t;
	struct htmlTag **e;

/* if not browsing ... */
	if (!(e = w->tags))
		return;

/* drop empty textarea buffers created by this session */
	for (i = 0; i < w->numTags; ++i, ++e) {
		t = *e;
		if (t->action != TAGACT_INPUT)
			continue;
		if (t->itype != INP_TA)
			continue;
		if (!(n = t->lic))
			continue;
		freeEmptySideBuffer(n);
	}			/* loop over tags */

	e = w->tags;
	for (i = 0; i < w->numTags; ++i, ++e) {
		t = *e;
		disconnectTagObject(t);
		freeTag(t);
	}

	free(w->tags);
	w->tags = 0;
	w->numTags = w->allocTags = w->deadTags = 0;
}				/* freeTags */

struct htmlTag *newTag(const char *name)
{
	struct htmlTag *t;
	const struct tagInfo *ti;
	int action;

	for (ti = availableTags; ti->name[0]; ++ti)
		if (stringEqualCI(ti->name, name))
			break;

	if (!ti->name[0]) {
		debugPrint(4, "warning, created node %s reverts to generic",
			   name);
		ti = availableTags + 2;
	}

	if ((action = ti->action) == TAGACT_ZERO)
		return 0;

	t = (struct htmlTag *)allocZeroMem(sizeof(struct htmlTag));
	t->f0 = cf;		/* set current frame */
	t->action = action;
	t->info = ti;
	t->seqno = cw->numTags;
	t->nodeName = cloneString(name);
	pushTag(t);
	return t;
}				/* newTag */

/* reserve space for 512 tags */
void initTagArray(void)
{
	cw->numTags = 0;
	cw->allocTags = 512;
	cw->deadTags = 0;
	cw->tags =
	    (struct htmlTag **)allocMem(cw->allocTags *
					sizeof(struct htmlTag *));
}				/* initTagArray */

bool htmlGenerated;
static struct htmlTag *treeAttach;
static int tree_pos;
static bool treeDisable;
static void intoTree(struct htmlTag *parent);

void htmlNodesIntoTree(int start, struct htmlTag *attach)
{
	treeAttach = attach;
	tree_pos = start;
	treeDisable = false;
	debugPrint(4, "@@tree of nodes");
	intoTree(0);
	debugPrint(4, "}\n@@end tree");
}				/* htmlNodesIntoTree */

/* Convert a list of html nodes, properly nested open close, into a tree.
 * Attach the tree to an existing tree here, for document.write etc,
 * or just build the tree if attach is null. */
static void intoTree(struct htmlTag *parent)
{
	struct htmlTag *t, *prev = 0;
	int j;
	const char *v;
	int action;

	if (!parent)
		debugPrint(4, "root {");
	else
		debugPrint(4, "%s %d {", parent->info->name, parent->seqno);

	while (tree_pos < cw->numTags) {
		t = tagList[tree_pos++];
		if (t->slash) {
			if (parent) {
				parent->balance = t, t->balance = parent;
				t->dead = parent->dead;
				if (t->dead)
					++cw->deadTags;
			}
			debugPrint(4, "}");
			return;
		}

		if (treeDisable) {
			debugPrint(4, "node skip %s", t->info->name);
			t->dead = true;
			++cw->deadTags;
			intoTree(t);
			continue;
		}

		if (htmlGenerated) {
/*Some things are different if the html is generated, not part of the original web page.
 * You can skip past <head> altogether, including its
 * tidy generated descendants, and you want to pass through <body>
 * to the children below. */
			action = t->action;
			if (action == TAGACT_HEAD) {
				debugPrint(4, "node skip %s", t->info->name);
				t->dead = true;
				++cw->deadTags;
				treeDisable = true;
				intoTree(t);
				treeDisable = false;
				continue;
			}
			if (action == TAGACT_BODY) {
				debugPrint(4, "node pass %s", t->info->name);
				t->dead = true;
				++cw->deadTags;
				intoTree(t);
				continue;
			}

/* this node is ok, but if parent is a pass through node... */
			if (parent == 0 ||	/* this shouldn't happen */
			    parent->action == TAGACT_BODY) {
/* link up to treeAttach */
				const char *w = "root";
				if (treeAttach)
					w = treeAttach->info->name;
				debugPrint(4, "node up %s to %s", t->info->name,
					   w);
				t->parent = treeAttach;
				if (treeAttach) {
					struct htmlTag *c =
					    treeAttach->firstchild;
					if (!c)
						treeAttach->firstchild = t;
					else {
						while (c->sibling)
							c = c->sibling;
						c->sibling = t;
					}
				}
				goto checkattributes;
			}
		}

/* regular linking through the parent node */
/* Could be treeAttach if this is a frame inside a window */
		t->parent = (parent ? parent : treeAttach);
		if (prev) {
			prev->sibling = t;
		} else if (parent) {
			parent->firstchild = t;
		} else if (treeAttach) {
			treeAttach->firstchild = t;
		}
		prev = t;

checkattributes:
/* check for some common attributes here */
		action = t->action;
		if (stringInListCI(t->attributes, "onclick") >= 0)
			t->onclick = t->doorway = true;
		if (stringInListCI(t->attributes, "onchange") >= 0)
			t->onchange = t->doorway = true;
		if (stringInListCI(t->attributes, "onsubmit") >= 0)
			t->onsubmit = t->doorway = true;
		if (stringInListCI(t->attributes, "onreset") >= 0)
			t->onreset = t->doorway = true;
		if (stringInListCI(t->attributes, "onload") >= 0)
			t->onload = t->doorway = true;
		if (stringInListCI(t->attributes, "onunload") >= 0)
			t->onunload = t->doorway = true;
		if (stringInListCI(t->attributes, "checked") >= 0)
			t->checked = t->rchecked = true;
		if (stringInListCI(t->attributes, "readonly") >= 0)
			t->rdonly = true;
		if (stringInListCI(t->attributes, "disabled") >= 0)
			t->disabled = true;
		if (stringInListCI(t->attributes, "multiple") >= 0)
			t->multiple = true;
		if ((j = stringInListCI(t->attributes, "name")) >= 0) {
/* temporarily, make another copy; some day we'll just point to the value */
			v = t->atvals[j];
			if (v && !*v)
				v = 0;
			t->name = cloneString(v);
		}
		if ((j = stringInListCI(t->attributes, "id")) >= 0) {
			v = t->atvals[j];
			if (v && !*v)
				v = 0;
			t->id = cloneString(v);
		}
		if ((j = stringInListCI(t->attributes, "class")) >= 0) {
			v = t->atvals[j];
			if (v && !*v)
				v = 0;
			t->class = cloneString(v);
		}
		if ((j = stringInListCI(t->attributes, "value")) >= 0) {
			v = t->atvals[j];
			if (v && !*v)
				v = 0;
			t->value = cloneString(v);
			t->rvalue = cloneString(v);
		}
// Resolve href against the base, but wait a minute, what if it's <p href=blah>
// and we're not suppose to resolve it? I don't ask about the parent node.
// Well, in general, I don't carry the href attribute into the js node.
// I only do it when it is relevant, such as <a> or <area>.
// See the exceptions in pushAttributes() in this file.
// I know, it's confusing.
		if ((j = stringInListCI(t->attributes, "href")) >= 0) {
			v = t->atvals[j];
			if (v && !*v)
				v = 0;
			if (v) {
				v = resolveURL(cf->hbase, v);
				cnzFree(t->atvals[j]);
				t->atvals[j] = v;
				if (action == TAGACT_BASE && !cf->baseset) {
					nzFree(cf->hbase);
					cf->hbase = cloneString(v);
					cf->baseset = true;
				}
				t->href = cloneString(v);
			}
		}
		if ((j = stringInListCI(t->attributes, "src")) >= 0) {
			v = t->atvals[j];
			if (v && !*v)
				v = 0;
			if (v) {
				v = resolveURL(cf->hbase, v);
				cnzFree(t->atvals[j]);
				t->atvals[j] = v;
				if (!t->href)
					t->href = cloneString(v);
			}
		}
		if ((j = stringInListCI(t->attributes, "action")) >= 0) {
			v = t->atvals[j];
			if (v && !*v)
				v = 0;
			if (v) {
				v = resolveURL(cf->hbase, v);
				cnzFree(t->atvals[j]);
				t->atvals[j] = v;
				if (!t->href)
					t->href = cloneString(v);
			}
		}

/* href=javascript:foo() is another doorway into js */
		if (t->href && memEqualCI(t->href, "javascript:", 11))
			t->doorway = true;
/* And of course the primary doorway */
		if (action == TAGACT_SCRIPT) {
			t->doorway = true;
			t->scriptgen = htmlGenerated;
		}

		intoTree(t);
	}
}				/* intoTree */

void underKill(struct htmlTag *t)
{
	struct htmlTag *u, *v;
	for (u = t->firstchild; u; u = v) {
		v = u->sibling;
		u->sibling = u->parent = 0;
		u->deleted = true;
		if (!u->jv)
			killTag(u);
	}
	t->firstchild = NULL;
}

void killTag(struct htmlTag *t)
{
	struct htmlTag *c, *parent;
	debugPrint(4, "kill tag %s %d", t->info->name, t->seqno);
	t->dead = true;
	++cw->deadTags;
	if (t->balance) {
		t->balance->dead = true;
		++cw->deadTags;
	}
	t->deleted = true;
	t->jv = NULL;

// unlink it from the tree above.
	parent = t->parent;
	if (parent) {
		t->parent = NULL;
		if (parent->firstchild == t)
			parent->firstchild = t->sibling;
		else {
			c = parent->firstchild;
			if (c) {
				for (; c->sibling; c = c->sibling) {
					if (c->sibling != t)
						continue;
					c->sibling = t->sibling;
					break;
				}
			}
		}
	}

	underKill(t);
}

/* Parse some html, as generated by innerHTML or document.write. */
void html_from_setter(jsobjtype inner, const char *h)
{
	struct htmlTag *t = NULL;
	int l = 0;
	debugPrint(4, "Generated {%s}", h);
	t = tagFromJavaVar(inner);
	if (!t) {
		debugPrint(1,
			   "innerHTML finds no tag for %p, cannot parse",
			   inner);
		return;
	}
	debugPrint(4, "parse under %s %d", t->info->name, t->seqno);
	l = cw->numTags;

/* Cut all the children away from t */
	underKill(t);

	html2nodes(h, false);
	htmlGenerated = true;
	htmlNodesIntoTree(l, t);
	prerender(0);
	innerParent = inner;
	decorate(0);
	innerParent = 0;
}				/* html_from_setter */

static void processStyles(jsobjtype so, const char *stylestring)
{
	char *workstring = cloneString(stylestring);
	char *s;		// gets truncated to the style name
	char *sv;
	char *next;
	for (s = workstring; *s; s = next) {
		next = strchr(s, ';');
		if (!next) {
			next = s + strlen(s);
		} else {
			*next++ = 0;
			skipWhite2(&next);
		}
		sv = strchr(s, ':');
		// if there was something there, but it didn't
		// adhere to the expected syntax, skip this pair
		if (sv) {
			*sv++ = '\0';
			skipWhite2(&sv);
			trimWhite(s);
			trimWhite(sv);
// the property name has to be nonempty
			if (*s) {
				cssAttributeCrunch(s);
				set_property_string(so, s, sv);
			}
		}
	}
	nzFree(workstring);
}				/* processStyles */
