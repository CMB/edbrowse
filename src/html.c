/* html.c
 * Parse html tags.
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"

#define handlerPresent(obj, name) (has_property(obj, name) == EJ_PROP_FUNCTION)

bool htmlGenerated;

static const char *const handlers[] = {
	"onmousemove", "onmouseover", "onmouseout", "onmouseup", "onmousedown",
	"onclick", "ondblclick", "onblur", "onfocus",
	"onchange", "onsubmit", "onreset",
	"onload", "onunload",
	"onkeypress", "onkeyup", "onkeydown",
	0
};

static const char *const inp_types[] = {
	"reset", "button", "image", "submit",
	"hidden",
	"text", "password", "number", "file",
	"select", "textarea", "radio", "checkbox",
	0
};

static const char dfvl[] = "defaultValue";
static const char dfck[] = "defaultChecked";

static struct htmlTag *topTag;
static char *topAttrib;
static struct htmlTag *currentForm;	/* the open form */
static jsobjtype js_reset, js_submit;
uchar browseLocal;

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
/* make more room */
		a = a / 2 * 3;
		cw->tags =
		    (struct htmlTag **)reallocMem(cw->tags, a * sizeof(t));
		cw->allocTags = a;
	}
	tagList[cw->numTags++] = t;
	tagCountCheck();
}				/* pushTag */

/*********************************************************************
Comments on struct tagInfo member nest, the nestability of a tag.

nest = 0:
Like <input>, where </input> doesn't make any sense.
</input> is silently tolerated without error,
but stuff between <input> and </input> isn't really bound up
as objects under the input tag.
Some conventions require the close, like <frame></frame>,
but even here there should be nothing in between, and if there is
it is a mistake, and is not bound to the frame tag.

nest = 1:
Like <p> here is a paragraph </p>.
Semantically there should not be a paragraph inside a paragraph,
so if you run into another <p> that implicitly closes the previous <p>.
In other words <p> implies </p><p>.
Web pages usually close paragraphs properly, but not always lists.
<ol> <li> item 1 <li> item 2 <li> item 3 </ol>.

nest = 3:
Strict nesting, like <b> bold text </b>.
this should close properly, and not out of sequence like <b> <p> </b>.

Closing a tag </foo>:
If 0 then we don't care.
If 1 or 3 then back up and find the first open, unbalanced, tag of the same type.
Close the open tag and all open tags in between.

Opening a tag <foo>:
If 0 then we don't care.
If 1 or 3 then push the tag and leave it unbalanced,
but if 1, back up and find the first unbalanced tag of any type.
If it is the same type then close off that tag.
This solves the <li> item 1 <li> item 2 problem,
but not <li> stuff <p> more stuff <li>
so it's just not a perfect solution.

Parent node:
Open a tag that corresponds to an object,
and if a js object is created, back up to the first open tag
above it that corresponds to an object,
and if there is indeed a js object for that tag, create the parent link.

children:
A post scan derives children from parents.
So if I got the parent logic wrong, and I probably did,
I can fix it, and then the children will be fixed as well.
It's a normal form kind of thing.
Not yet implemented.
*********************************************************************/

static const struct tagInfo elements[] = {
	{"BASE", "base reference for relative URLs", TAGACT_BASE, 0, 13},
	{"A", "an anchor", TAGACT_A, 0, 9},
	{"INPUT", "an input item", TAGACT_INPUT, 0, 13},
	{"TITLE", "the title", TAGACT_TITLE, 0, 9},
	{"TEXTAREA", "an input text area", TAGACT_TA, 0, 9},
	{"SELECT", "an option list", TAGACT_SELECT, 0, 9},
	{"OPTION", "a select option", TAGACT_OPTION, 0, 9},
	{"SUB", "a subscript", TAGACT_SUB, 0, 0},
	{"SUP", "a superscript", TAGACT_SUP, 0, 0},
	{"OVB", "an overbar", TAGACT_OVB, 0, 0},
	{"FONT", "a font", TAGACT_NOP, 0, 0},
	{"CENTER", "centered text", TAGACT_NOP, 0, 0},
	{"CAPTION", "a caption", TAGACT_NOP, 5, 0},
	{"HEAD", "the html header information", TAGACT_HEAD, 0, 13},
	{"BODY", "the html body", TAGACT_BODY, 0, 13},
	{"TEXT", "a text section", TAGACT_TEXT, 0, 4},
	{"BGSOUND", "background music", TAGACT_MUSIC, 0, 5},
	{"AUDIO", "audio passage", TAGACT_MUSIC, 0, 5},
	{"META", "a meta tag", TAGACT_META, 0, 12},
	{"IMG", "an image", TAGACT_IMAGE, 0, 12},
	{"IMAGE", "an image", TAGACT_IMAGE, 0, 12},
	{"BR", "a line break", TAGACT_BR, 1, 4},
	{"P", "a paragraph", TAGACT_P, 2, 13},
	{"DIV", "a divided section", TAGACT_DIV, 5, 8},
	{"MAP", "a map of images", TAGACT_NOP, 5, 8},
	{"HTML", "html", TAGACT_HTML, 0, 13},
	{"BLOCKQUOTE", "a quoted paragraph", TAGACT_NOP, 10, 9},
	{"H1", "a level 1 header", TAGACT_NOP, 10, 9},
	{"H2", "a level 2 header", TAGACT_NOP, 10, 9},
	{"H3", "a level 3 header", TAGACT_NOP, 10, 9},
	{"H4", "a level 4 header", TAGACT_NOP, 10, 9},
	{"H5", "a level 5 header", TAGACT_NOP, 10, 9},
	{"H6", "a level 6 header", TAGACT_NOP, 10, 9},
	{"DT", "a term", TAGACT_DT, 2, 13},
	{"DD", "a definition", TAGACT_DD, 1, 13},
	{"LI", "a list item", TAGACT_LI, 1, 13},
	{"UL", "a bullet list", TAGACT_UL, 10, 9},
	{"DIR", "a directory list", TAGACT_NOP, 5, 9},
	{"MENU", "a menu", TAGACT_NOP, 5, 9},
	{"OL", "a numbered list", TAGACT_OL, 10, 9},
	{"DL", "a definition list", TAGACT_DL, 10, 9},
	{"HR", "a horizontal line", TAGACT_HR, 5, 5},
	{"FORM", "a form", TAGACT_FORM, 10, 9},
	{"BUTTON", "a button", TAGACT_INPUT, 0, 13},
	{"FRAME", "a frame", TAGACT_FRAME, 2, 13},
	{"IFRAME", "a frame", TAGACT_FRAME, 2, 13},
	{"MAP", "an image map", TAGACT_MAP, 2, 13},
	{"AREA", "an image map area", TAGACT_AREA, 0, 13},
	{"TABLE", "a table", TAGACT_TABLE, 10, 9},
	{"TR", "a table row", TAGACT_TR, 5, 9},
	{"TD", "a table entry", TAGACT_TD, 0, 13},
	{"TH", "a table heading", TAGACT_TD, 0, 9},
	{"PRE", "a preformatted section", TAGACT_PRE, 10, 0},
	{"LISTING", "a listing", TAGACT_PRE, 1, 0},
	{"XMP", "an example", TAGACT_PRE, 1, 0},
	{"FIXED", "a fixed presentation", TAGACT_NOP, 1, 0},
	{"CODE", "a block of code", TAGACT_NOP, 0, 0},
	{"SAMP", "a block of sample text", TAGACT_NOP, 0, 0},
	{"ADDRESS", "an address block", TAGACT_NOP, 1, 0},
	{"STYLE", "a style block", TAGACT_NOP, 0, 2},
	{"SCRIPT", "a script", TAGACT_SCRIPT, 0, 9},
	{"NOSCRIPT", "no script section", TAGACT_NOP, 0, 3},
	{"NOFRAMES", "no frames section", TAGACT_NOP, 0, 3},
	{"EMBED", "embedded html", TAGACT_MUSIC, 0, 5},
	{"NOEMBED", "no embed section", TAGACT_NOP, 0, 3},
	{"OBJECT", "an html object", TAGACT_OBJ, 0, 3},
	{"EM", "emphasized text", TAGACT_JS, 0, 0},
	{"LABEL", "a label", TAGACT_JS, 0, 0},
	{"STRIKE", "emphasized text", TAGACT_JS, 0, 0},
	{"S", "emphasized text", TAGACT_JS, 0, 0},
	{"STRONG", "emphasized text", TAGACT_JS, 0, 0},
	{"B", "bold text", TAGACT_JS, 0, 0},
	{"I", "italicized text", TAGACT_JS, 0, 0},
	{"U", "underlined text", TAGACT_JS, 0, 0},
	{"DFN", "definition text", TAGACT_JS, 0, 0},
	{"Q", "quoted text", TAGACT_JS, 0, 0},
	{"ABBR", "an abbreviation", TAGACT_JS, 0, 0},
	{"SPAN", "an html span", TAGACT_SPAN, 0, 0},
	{"FRAMESET", "a frame set", TAGACT_JS, 0, 1},
	{"", NULL, 0}
};

void freeTags(struct ebWindow *w)
{
	int i, n;
	struct htmlTag *t;
	struct htmlTag **e;
	struct ebWindow *side;

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
		if (!(side = sessionList[n].lw))
			continue;
		if (side->fileName)
			continue;
		if (side->dol)
			continue;
		if (side != sessionList[n].fw)
			continue;
/* We could have added a line, then deleted it */
		cxQuit(n, 3);
	}			/* loop over tags */

	e = w->tags;
	for (i = 0; i < w->numTags; ++i, ++e) {
		char **a;
		t = *e;
		nzFree(t->attrib);
		nzFree(t->textval);
		nzFree(t->name);
		nzFree(t->id);
		nzFree(t->value);
		cnzFree(t->rvalue);
		nzFree(t->href);
		nzFree(t->classname);
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

	free(w->tags);
	w->tags = 0;
	w->numTags = w->allocTags = 0;

/* delete any pending javascript timers */
	delTimers(w);
}				/* freeTags */

bool tagHandler(int seqno, const char *name)
{
	struct htmlTag *t = tagList[seqno];
/* check the htnl tag attributes first */
	if (t->onclick && stringEqual(name, "onclick"))
		return true;
	if (t->onsubmit && stringEqual(name, "onsubmit"))
		return true;
	if (t->onreset && stringEqual(name, "onreset"))
		return true;
	if (t->onchange && stringEqual(name, "onchange"))
		return true;

	if (!t->jv)
		return false;
	if (!isJSAlive)
		return false;
	return handlerPresent(t->jv, name);
}				/* tagHandler */

static void formReset(const struct htmlTag *form);

/*********************************************************************
This function was originally written to incorporate any strings generated by
document.write(), and it still does that,
but now it does much more.
It handles any side effects that occur from running js.
innerHTML tags generated, form input values set, timers,
form.reset(), form.submit(), document.location = url, etc.
Every js activity should start with jSyncup() and end with jSideEffects().
*********************************************************************/

void jSideEffects(void)
{
	debugPrint(4, "jSideEffects starts");
	runScriptsPending();
	debugPrint(4, "jSideEffects ends");
/* now rerender and look for differences */
	rerender(false);
}				/* jSideEffects */

/* display the checked options in an allocated string */
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

static struct htmlTag *locateOptionByName(const struct htmlTag *sel,
					  const char *name, int *pmc,
					  bool exact)
{
	struct htmlTag *t, *em = 0, *pm = 0;
	int pmcount = 0;	/* partial match count */
	const char *s;
	int i;

	for (i = 0; i < cw->numTags; ++i) {
		t = tagList[i];
		if (t->controller != sel)
			continue;
		if (!(s = t->textval))
			continue;
		if (stringEqualCI(s, name)) {
			em = t;
			continue;
		}
		if (exact)
			continue;
		if (strstrCI(s, name)) {
			pm = t;
			++pmcount;
		}
	}
	if (em)
		return em;
	if (pmcount == 1)
		return pm;
	*pmc = (pmcount > 0);
	return 0;
}				/* locateOptionByName */

static struct htmlTag *locateOptionByNum(const struct htmlTag *sel, int n)
{
	struct htmlTag *t;
	int cnt = 0;
	int i;

	for (i = 0; i < cw->numTags; ++i) {
		t = tagList[i];
		if (t->controller != sel)
			continue;
		if (!t->textval)
			continue;
		++cnt;
		if (cnt == n)
			return t;
	}
	return 0;
}				/* locateOptionByNum */

static bool
locateOptions(const struct htmlTag *sel, const char *input,
	      char **disp_p, char **val_p, bool setcheck)
{
	struct htmlTag *t;
	char *disp, *val;
	int disp_l, val_l;
	int len = strlen(input);
	int i, n, pmc, cnt;
	const char *s, *e;	/* start and end of an option */
	char *iopt;		/* individual option */

	iopt = (char *)allocMem(len + 1);
	disp = initString(&disp_l);
	val = initString(&val_l);

	if (setcheck) {
/* Uncheck all existing options, then check the ones selected. */
		if (sel->jv && isJSAlive)
			set_property_number(sel->jv, "selectedIndex", -1);
		for (i = 0; i < cw->numTags; ++i) {
			t = tagList[i];
			if (t->controller == sel && t->textval) {
				t->checked = false;
				if (t->jv && isJSAlive)
					set_property_bool(t->jv, "selected",
							  false);
			}
		}
	}

	s = input;
	while (*s) {
		e = 0;
		if (sel->multiple)
			e = strchr(s, ',');
		if (!e)
			e = s + strlen(s);
		len = e - s;
		strncpy(iopt, s, len);
		iopt[len] = 0;
		s = e;
		if (*s == ',')
			++s;

		t = locateOptionByName(sel, iopt, &pmc, true);
		if (!t) {
			n = stringIsNum(iopt);
			if (n >= 0)
				t = locateOptionByNum(sel, n);
		}
		if (!t)
			t = locateOptionByName(sel, iopt, &pmc, false);
		if (!t) {
			if (n >= 0)
				setError(MSG_XOutOfRange, n);
			else
				setError(pmc + MSG_OptMatchNone, iopt);
/* This should never happen when we're doing a set check */
			if (setcheck) {
				runningError(MSG_OptionSync, iopt);
				continue;
			}
			goto fail;
		}

		if (val_p) {
			if (*val)
				stringAndChar(&val, &val_l, '\1');
			stringAndString(&val, &val_l, t->value);
		}

		if (disp_p) {
			if (*disp)
				stringAndChar(&disp, &disp_l, ',');
			stringAndString(&disp, &disp_l, t->textval);
		}

		if (setcheck) {
			t->checked = true;
			if (t->jv && isJSAlive) {
				set_property_bool(t->jv, "selected", true);
				if (sel->jv && isJSAlive)
					set_property_number(sel->jv,
							    "selectedIndex",
							    t->lic);
			}
		}
	}			/* loop over multiple options */

	if (val_p)
		*val_p = val;
	if (disp_p)
		*disp_p = disp;
	free(iopt);
	return true;

fail:
	free(iopt);
	nzFree(val);
	nzFree(disp);
	if (val_p)
		*val_p = 0;
	if (disp_p)
		*disp_p = 0;
	return false;
}				/* locateOptions */

/*********************************************************************
Sync up the javascript variables with the input fields.
This is required before running any javascript, e.g. an onclick function.
After all, the input fields may have changed.
You may have changed the last name from Flintstone to Rubble.
This has to propagate down to the javascript strings in the DOM.
This is quick and stupid; I just update everything.
Most of the time I'm setting the strings to what they were before;
that's the way it goes.
*********************************************************************/

void jSyncup(void)
{
	struct htmlTag *t;
	int itype, i, j, cx;
	char *value, *cxbuf;

	if (!cw->browseMode)
		return;		/* not necessary */
	if (!isJSAlive)
		return;
	debugPrint(4, "jSyncup starts");

	for (i = 0; i < cw->numTags; ++i) {
		t = tagList[i];
		if (t->action != TAGACT_INPUT)
			continue;
		itype = t->itype;
		if (itype <= INP_HIDDEN)
			continue;

		if (itype >= INP_RADIO) {
			int checked = fieldIsChecked(t->seqno);
			if (checked < 0)
				checked = t->rchecked;
			t->checked = checked;
			set_property_bool(t->jv, "checked", checked);
			continue;
		}

		value = getFieldFromBuffer(t->seqno);
/* If that line has been deleted from the user's buffer,
 * indicated by value = 0,
 * revert back to the original (reset) value. */
		if (!value)
			value = cloneString(t->rvalue);

		if (itype == INP_SELECT) {
/* set option.selected in js based on the option(s) in value */
			locateOptions(t, (value ? value : t->value), 0, 0,
				      true);
/* This is totally confusing. In the case of select,
 * t->value is the value displayed on the screen,
 * but within js, select.value is the value of the option selected,
 * assuming multiple options are not allowed. */
			if (value) {
				nzFree(t->value);
				t->value = value;
			}
			if (!t->multiple) {
				value = get_property_option(t->jv);
				set_property_string(t->jv, "value", value);
				nzFree(value);
			}
			continue;
		}

		if (itype == INP_TA) {
			if (!value) {
				set_property_string(t->jv, "value", 0);
				continue;
			}
/* Now value is just <buffer 3>, which is meaningless. */
			nzFree(value);
			cx = t->lic;
			if (!cx)
				continue;
/* The unfold command should never fail */
			if (!unfoldBuffer(cx, false, &cxbuf, &j))
				continue;
			set_property_string(t->jv, "value", cxbuf);
			nzFree(cxbuf);
			continue;
		}

		if (value) {
			set_property_string(t->jv, "value", value);
			nzFree(t->value);
			t->value = value;
		}
	}			/* loop over tags */

/* screen snap, to compare with the new screen after js runs */
	nzFree(cw->lastrender);
	cw->lastrender = 0;
	if (unfoldBuffer(context, false, &cxbuf, &j))
		cw->lastrender = cxbuf;

	debugPrint(4, "jSyncup ends");
}				/* jSyncup */

struct htmlTag *newTag(const char *name)
{
	struct htmlTag *t;
	const struct tagInfo *ti;
	int action;

	for (ti = elements; ti->name[0]; ++ti)
		if (stringEqualCI(ti->name, name))
			break;
	if (!ti->name[0])
		return 0;

	action = ti->action;
	t = (struct htmlTag *)allocZeroMem(sizeof(struct htmlTag));
	t->action = action;
	t->info = ti;
	t->seqno = cw->numTags;
	pushTag(t);
	return t;
}				/* newTag */

static struct htmlTag *treeAttach;
static int tree_pos;
static bool treeDisable;
static void intoTree(struct htmlTag *parent);

static void runGeneratedHtml(struct htmlTag *t, const char *h)
{
	int l = cw->numTags;
	debugPrint(4, "generated {%s}\n", h);
	htmlGenerated = true;
	html2nodes(h);
	treeAttach = t;
	tree_pos = l;
	intoTree(0);
	treeAttach = NULL;
	prerender(0);
	decorate(0);
	htmlGenerated = false;
}				/* runGeneratedHtml */

void runScriptsPending(void)
{
	struct htmlTag *t;
	struct inputChange *ic;
	int j, l;
	char *jtxt;
	const char *js_file;
	int ln;
	bool change;
	jsobjtype v;

/* if onclick code or some such does document write, where does that belong?
 * I don't know, I'll just put it at the end.
 * As you see below, document.write that comes from a specific javascript
 * appears inline where the script is. */
	if (cw->dw) {
		stringAndString(&cw->dw, &cw->dw_l, "</body>\n");
		runGeneratedHtml(NULL, cw->dw);
		nzFree(cw->dw);
		cw->dw = 0;
		cw->dw_l = 0;
	}

top:
	change = false;

	for (j = 0; j < cw->numTags; ++j) {
		t = tagList[j];
		if (t->action != TAGACT_SCRIPT)
			continue;
		if (!t->jv)
			continue;
		if (t->step >= 3)
			continue;
/* now running the script */
		t->step = 3;
		change = true;
		jtxt = get_property_string(t->jv, "data");
		if (!jtxt)
			continue;	/* nothing there */
		js_file = t->js_file;
		if (!js_file)
			js_file = "generated";
		ln = t->js_ln;
		if (!ln)
			ln = 1;
		debugPrint(3, "execute %s at %d", js_file, ln);
/* if script is in the html it usually begins on the next line, so increment,
 * and hope the error messages line up. */
		if (ln > 1)
			++ln;
		javaParseExecute(cw->winobj, jtxt, js_file, ln);
		debugPrint(3, "execution complete");
		nzFree(jtxt);

/* look for document.write from this script */
		if (cw->dw) {
			stringAndString(&cw->dw, &cw->dw_l, "</body>\n");
			runGeneratedHtml(t, cw->dw);
			nzFree(cw->dw);
			cw->dw = 0;
			cw->dw_l = 0;
		}
	}

	if (change)
		goto top;

/* look for an run innerHTML */
	foreach(ic, inputChangesPending) {
		char *h;
		if (ic->major != 'i' || ic->minor != 'h')
			continue;
		ic->major = 'x';
/* one line will cut all the children away from t,
 * though it's not clear how to do the same in the javascript world. */
		ic->t->firstchild = NULL;
		runGeneratedHtml(ic->t, ic->value);
		change = true;
	}

	if (change)
		goto top;

	foreach(ic, inputChangesPending) {
		char *v;
		int side;
		if (ic->major != 'i' || ic->minor != 't')
			continue;
		ic->major = 'x';
		t = ic->t;
/* the tag should always be a textarea tag. */
/* Not sure what to do if it's not. */
		if (t->action != TAGACT_INPUT || t->itype != INP_TA) {
			debugPrint(3,
				   "innerText is applied to tag %d that is not a textarea.",
				   t->seqno);
			continue;
		}
/* 2 parts: innerText copies over to textarea->value
 * if js has not already done so,
 * and the text replaces what was in the side buffer. */
		v = ic->value;
		set_property_string(t->jv, "valueue", v);
		side = t->lic;
		if (side <= 0 || side >= MAXSESSION || side == context)
			continue;
		if (sessionList[side].lw == NULL)
			continue;
		if (cw->browseMode)
			i_printf(MSG_BufferUpdated, side);
		sideBuffer(side, v, -1, 0);
	}
	freeList(&inputChangesPending);

	rebuildSelectors();

	if (v = js_reset) {
		js_reset = 0;
		if (t = tagFromJavaVar(v))
			formReset(t);
	}

	if (v = js_submit) {
		js_submit = 0;
		if (t = tagFromJavaVar(v)) {
			char *post;
			bool rc = infPush(t->seqno, &post);
			if (rc)
				gotoLocation(post, 0, false);
			else
				showError();
		}
	}
}				/* runScriptsPending */

/* Convert a list of nodes, properly nested open close, into a tree.
 * Attach the tree to an existing tree here, for document.write etc,
 * or just build the tree if this is null. */
static void intoTree(struct htmlTag *parent)
{
	struct htmlTag *t, *prev = 0;
	int j;
	const char *v;

	while (tree_pos < cw->numTags) {
		t = tagList[tree_pos++];
		if (t->slash) {
			if (parent)
				parent->balance = t, t->balance = parent;
			return;
		}

		if (treeDisable) {
			debugPrint(5, "node skip %s", t->info->name);
			t->step = 100;
			intoTree(t);
			continue;
		}

		if (htmlGenerated) {
/*Some things are different if the html is generated, not part of the original web page.
 * You can skip past <head> altogether, including its
 * tidy generated descendants, and you want to pass through <body>
 * to the children below. */
			int action = t->action;
			if (action == TAGACT_HEAD) {
				debugPrint(5, "node skip %s", t->info->name);
				t->step = 100;
				treeDisable = true;
				intoTree(t);
				treeDisable = false;
				continue;
			}
			if (action == TAGACT_HTML || action == TAGACT_BODY) {
				debugPrint(5, "node pass %s", t->info->name);
				t->step = 100;
				intoTree(t);
				continue;
			}

/* this node is ok, but if parent is a pass through node... */
			if (parent == 0 ||	/* this shouldn't happen */
			    parent->action == TAGACT_HTML
			    || parent->action == TAGACT_BODY) {
/* link up to treeAttach */
				const char *w = "root";
				if (treeAttach)
					w = treeAttach->info->name;
				debugPrint(5, "node up %s to %s", t->info->name,
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
		t->parent = parent;
		if (prev) {
			prev->sibling = t;
		} else if (parent) {
			parent->firstchild = t;
		}
		prev = t;

checkattributes:
/* check for some common attributes here */
		if (stringInListCI(t->attributes, "onclick") >= 0)
			t->onclick = true;
		if (stringInListCI(t->attributes, "onchange") >= 0)
			t->onchange = true;
		if (stringInListCI(t->attributes, "onsubmit") >= 0)
			t->onsubmit = true;
		if (stringInListCI(t->attributes, "onreset") >= 0)
			t->onreset = true;
		if (stringInListCI(t->attributes, "onload") >= 0)
			t->onload = true;
		if (stringInListCI(t->attributes, "onunload") >= 0)
			t->onunload = true;
		if (stringInListCI(t->attributes, "checked") >= 0)
			t->checked = t->rchecked = true;
		if (stringInListCI(t->attributes, "readonly") >= 0)
			t->rdonly = true;
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
			t->classname = cloneString(v);
		}
		if ((j = stringInListCI(t->attributes, "value")) >= 0) {
			v = t->atvals[j];
			if (v && !*v)
				v = 0;
			t->value = cloneString(v);
			t->rvalue = cloneString(v);
		}
		if ((j = stringInListCI(t->attributes, "href")) >= 0) {
			v = t->atvals[j];
			if (v && !*v)
				v = 0;
			if (v) {
/* <base> sets the base URL, and should not be resolved */
				if (t->action != TAGACT_BASE) {
					v = resolveURL(cw->hbase, v);
					cnzFree(t->atvals[j]);
					t->atvals[j] = v;
				} else if (!cw->baseset) {
					nzFree(cw->hbase);
					cw->hbase = cloneString(v);
					cw->baseset = true;
				}
				t->href = cloneString(v);
			}
		}
		if ((j = stringInListCI(t->attributes, "src")) >= 0) {
			v = t->atvals[j];
			if (v && !*v)
				v = 0;
			if (v) {
				v = resolveURL(cw->hbase, v);
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
				v = resolveURL(cw->hbase, v);
				cnzFree(t->atvals[j]);
				t->atvals[j] = v;
				if (!t->href)
					t->href = cloneString(v);
			}
		}

		intoTree(t);
	}
}				/* intoTree */

void preFormatCheck(int tagno, bool * pretag, bool * slash)
{
	const struct htmlTag *t;
	*pretag = *slash = false;
	if (tagno >= 0 && tagno < cw->numTags) {
		t = tagList[tagno];
		*pretag = (t->action == TAGACT_PRE);
		*slash = t->slash;
	}
}				/* preFormatCheck */

char *htmlParse(char *buf, int remote)
{
	char *a, *newbuf;
	struct htmlTag *t;

	if (tagList)
		i_printfExit(MSG_HtmlNotreentrant);
	if (remote >= 0)
		browseLocal = !remote;

/* reserve space for 512 tags */
	cw->numTags = 0;
	cw->allocTags = 512;
	cw->tags =
	    (struct htmlTag **)allocMem(cw->allocTags *
					sizeof(struct htmlTag *));
	cw->baseset = false;
	cw->hbase = cloneString(cw->fileName);

/* call the tidy parser to build the html nodes */
	html2nodes(buf);
	nzFree(buf);

/* convert the list of nodes, with open close,
 * like properly nested parentheses, into a tree. */
	treeAttach = NULL;
	tree_pos = 0;
	intoTree(0);

	prerender(0);
	if (isJSAlive) {
		decorate(0);
		runScriptsPending();
		runOnload();
		runScriptsPending();
	}

	a = render(0);
	debugPrint(6, "|%s|\n", a);
	newbuf = htmlReformat(a);
	nzFree(a);

	set_property_string(cw->docobj, "readyState", "complete");
	return newbuf;
}				/* htmlParse */

/* See if there are simple tags like <p> or </font> */
bool htmlTest(void)
{
	int j, ln;
	int cnt = 0;
	int fsize = 0;		/* file size */
	char look[12];
	bool firstline = true;

	for (ln = 1; ln <= cw->dol; ++ln) {
		char *p = (char *)fetchLine(ln, -1);
		char c;
		int state = 0;

		while (isspaceByte(*p) && *p != '\n')
			++p;
		if (*p == '\n')
			continue;	/* skip blank line */
		if (firstline && *p == '<') {
/* check for <!doctype */
			if (memEqualCI(p + 1, "!doctype", 8))
				return true;
			if (memEqualCI(p + 1, "?xml", 4))
				return true;
/* If it starts with <tag, for any tag we recognize,
 * we'll call it good. */
			for (j = 1; j < 10; ++j) {
				if (!isalnumByte(p[j]))
					break;
				look[j - 1] = p[j];
			}
			look[j - 1] = 0;
			if (j > 1 && (p[j] == '>' || isspaceByte(p[j]))) {
/* something we recognize? */
				const struct tagInfo *ti;
				for (ti = elements; ti->name; ++ti)
					if (stringEqualCI(ti->name, look))
						return true;
			}	/* leading tag */
		}		/* leading < */
		firstline = false;

/* count tags through the buffer */
		for (j = 0; (c = p[j]) != '\n'; ++j) {
			if (state == 0) {
				if (c == '<')
					state = 1;
				continue;
			}
			if (state == 1) {
				if (c == '/')
					state = 2;
				else if (isalphaByte(c))
					state = 3;
				else
					state = 0;
				continue;
			}
			if (state == 2) {
				if (isalphaByte(c))
					state = 3;
				else
					state = 0;
				continue;
			}
			if (isalphaByte(c))
				continue;
			if (c == '>')
				++cnt;
			state = 0;
		}
		fsize += j;
	}			/* loop over lines */

/* we need at least one of these tags every 300 characters.
 * And we need at least 4 such tags.
 * Remember, you can always override by putting <html> at the top. */
	return (cnt >= 4 && cnt * 300 >= fsize);
}				/* htmlTest */

/* Show an input field */
void infShow(int tagno, const char *search)
{
	const struct htmlTag *t = tagList[tagno], *v;
	const char *s;
	int i, cnt;
	bool show;

	s = inp_types[t->itype];
	if (*s == ' ')
		++s;
	printf("%s", s);
	if (t->multiple)
		i_printf(MSG_Many);
	if (t->itype >= INP_TEXT && t->itype <= INP_NUMBER && t->lic)
		printf("[%d]", t->lic);
	if (t->itype == INP_TA) {
		const char *rows = attribVal(t, "rows");
		const char *cols = attribVal(t, "cols");
		const char *wrap = attribVal(t, "wrap");
		if (rows && cols) {
			printf("[%sx%s", rows, cols);
			if (wrap && stringEqualCI(wrap, "virtual"))
				i_printf(MSG_Recommended);
			i_printf(MSG_Close);
		}
		cnzFree(rows);
		cnzFree(cols);
		cnzFree(wrap);
	}			/* text area */
	if (t->name)
		printf(" %s", t->name);
	nl();
	if (t->itype != INP_SELECT)
		return;

/* display the options in a pick list */
/* If a search string is given, display the options containing that string. */
	cnt = 0;
	show = false;
	for (i = 0; i < cw->numTags; ++i) {
		v = tagList[i];
		if (v->controller != t)
			continue;
		if (!v->textval)
			continue;
		++cnt;
		if (*search && !strstrCI(v->textval, search))
			continue;
		show = true;
		printf("%3d %s\n", cnt, v->textval);
	}
	if (!show) {
		if (!search)
			i_puts(MSG_NoOptions);
		else
			i_printf(MSG_NoOptionsMatch, search);
	}
}				/* infShow */

/*********************************************************************
Update an input field in the current edbrowse buffer.
This can be done for one of two reasons.
First, the user has interactively entered a value in the form, such as
	i=foobar
In this case fromForm will be set to true.
I need to find the tag in the current buffer.
He just modified it, so it ought to be there.
If it isn't there, print an error and do nothing.
The second case: the value has been changed by form reset,
either the user has pushed the reset button or javascript has called form.reset.
Here fromForm is false.
I'm not sure why js would reset a form before the page was even rendered;
that's the only way the line should not be found,
or perhaps if that section of the web page was deleted.
notify = true causes the line to be printed after the change is made.
Notify true and fromForm false is impossible.
You don't need to be notified as each variable is changed during a reset.
The new line replaces the old, and the old is freed.
This works because undo is disabled in browse mode.
*********************************************************************/

static void
updateFieldInBuffer(int tagno, const char *newtext, bool notify, bool fromForm)
{
	int ln, idx, n, plen;
	char *p, *s, *t, *new;

	if (locateTagInBuffer(tagno, &ln, &p, &s, &t)) {
		n = (plen = pstLength((pst) p)) + strlen(newtext) - (t - s);
		new = allocMem(n);
		memcpy(new, p, s - p);
		strcpy(new + (s - p), newtext);
		memcpy(new + strlen(new), t, plen - (t - p));
		free(cw->map[ln].text);
		cw->map[ln].text = new;
		if (notify)
			displayLine(ln);
		return;
	}

	if (fromForm)
		i_printf(MSG_NoTagFound, tagno, newtext);
}				/* updateFieldInBuffer */

/* Update an input field. */
bool infReplace(int tagno, const char *newtext, bool notify)
{
	const struct htmlTag *t = tagList[tagno], *v;
	const struct htmlTag *form = t->controller;
	char *display;
	int itype = t->itype;
	int newlen = strlen(newtext);
	int i;

/* sanity checks on the input */
	if (itype <= INP_SUBMIT) {
		int b = MSG_IsButton;
		if (itype == INP_SUBMIT || itype == INP_IMAGE)
			b = MSG_SubmitButton;
		if (itype == INP_RESET)
			b = MSG_ResetButton;
		setError(b);
		return false;
	}

	if (itype == INP_TA) {
		setError(MSG_Textarea, t->lic);
		return false;
	}

	if (t->rdonly) {
		setError(MSG_Readonly);
		return false;
	}

	if (strchr(newtext, '\n')) {
		setError(MSG_InputNewline);
		return false;
	}

	if (itype >= INP_TEXT && itype <= INP_NUMBER && t->lic
	    && newlen > t->lic) {
		setError(MSG_InputLong, t->lic);
		return false;
	}

	if (itype >= INP_RADIO) {
		if (newtext[0] != '+' && newtext[0] != '-' || newtext[1]) {
			setError(MSG_InputRadio);
			return false;
		}
		if (itype == INP_RADIO && newtext[0] == '-') {
			setError(MSG_ClearRadio);
			return false;
		}
	}

/* Two lines, clear the "other" radio button, and set this one. */

	if (itype == INP_SELECT) {
		if (!locateOptions(t, newtext, 0, 0, false))
			return false;
		locateOptions(t, newtext, &display, 0, false);
		updateFieldInBuffer(tagno, display, notify, true);
		nzFree(display);
	}

	if (itype == INP_FILE) {
		if (!envFile(newtext, &newtext))
			return false;
		if (newtext[0] && access(newtext, 4)) {
			setError(MSG_FileAccess, newtext);
			return false;
		}
	}

	if (itype == INP_NUMBER) {
		if (*newtext && stringIsNum(newtext) < 0) {
			setError(MSG_NumberExpected);
			return false;
		}
	}

	if (itype == INP_RADIO && form && t->name && *newtext == '+') {
/* clear the other radio button */
		for (i = 0; i < cw->numTags; ++i) {
			v = tagList[i];
			if (v->controller != form)
				continue;
			if (v->itype != INP_RADIO)
				continue;
			if (!v->name)
				continue;
			if (!stringEqual(v->name, t->name))
				continue;
			if (fieldIsChecked(v->seqno) == true)
				updateFieldInBuffer(v->seqno, "-", false, true);
		}
	}

	if (itype != INP_SELECT) {
		updateFieldInBuffer(tagno, newtext, notify, true);
	}

	if (itype >= INP_RADIO && tagHandler(t->seqno, "onclick")) {
		if (!isJSAlive)
			runningError(MSG_NJNoOnclick);
		else {
			jSyncup();
			run_function_bool(t->jv, "onclick");
			jSideEffects();
			if (js_redirects)
				return true;
		}
	}

	if (itype >= INP_TEXT && itype <= INP_SELECT &&
	    tagHandler(t->seqno, "onchange")) {
		if (!isJSAlive)
			runningError(MSG_NJNoOnchange);
		else {
			jSyncup();
			run_function_bool(t->jv, "onchange");
			jSideEffects();
			if (js_redirects)
				return true;
		}
	}

	return true;
}				/* infReplace */

/*********************************************************************
Reset or submit a form.
This function could be called by javascript, as well as a human.
It must therefore update the js variables and the text simultaneously.
Most of this work is done by resetVar().
To reset a variable, copy its original value, in the html tag,
back to the text buffer, and over to javascript.
*********************************************************************/

static void resetVar(struct htmlTag *t)
{
	int itype = t->itype;
	const char *w = t->rvalue;
	bool bval;

/* This is a kludge - option looks like INP_SELECT */
	if (t->action == TAGACT_OPTION)
		itype = INP_SELECT;

	if (itype <= INP_SUBMIT)
		return;

	if (itype >= INP_SELECT && itype != INP_TA) {
		bval = t->rchecked;
		t->checked = bval;
		w = bval ? "+" : "-";
	}

	if (itype == INP_TA) {
		int cx = t->lic;
		if (cx)
			sideBuffer(cx, w, -1, 0);
	} else if (itype != INP_HIDDEN && itype != INP_SELECT)
		updateFieldInBuffer(t->seqno, w, false, false);

	if (itype >= INP_TEXT && itype <= INP_FILE || itype == INP_TA) {
		nzFree(t->value);
		t->value = cloneString(t->rvalue);
	}

	if (!t->jv)
		return;
	if (!isJSAlive)
		return;

	if (itype >= INP_RADIO) {
		set_property_bool(t->jv, "checked", bval);
	} else if (itype == INP_SELECT) {
/* remember this means option */
		set_property_bool(t->jv, "selected", bval);
		if (bval && !t->controller->multiple && t->controller->jv)
			set_property_number(t->controller->jv,
					    "selectedIndex", t->lic);
	} else
		set_property_string(t->jv, "value", w);
}				/* resetVar */

static void formReset(const struct htmlTag *form)
{
	struct htmlTag *t;
	int i, itype;
	char *display;

	for (i = 0; i < cw->numTags; ++i) {
		t = tagList[i];
		if (t->action == TAGACT_OPTION) {
			resetVar(t);
			continue;
		}

		if (t->action != TAGACT_INPUT)
			continue;
		if (t->controller != form)
			continue;
		itype = t->itype;
		if (itype != INP_SELECT) {
			resetVar(t);
			continue;
		}
		if (t->jv && isJSAlive)
			set_property_number(t->jv, "selectedIndex", -1);
	}			/* loop over tags */

/* loop again to look for select, now that options are set */
	for (i = 0; i < cw->numTags; ++i) {
		t = tagList[i];
		if (t->action != TAGACT_INPUT)
			continue;
		if (t->controller != form)
			continue;
		itype = t->itype;
		if (itype != INP_SELECT)
			continue;
		display = displayOptions(t);
		updateFieldInBuffer(t->seqno, display, false, false);
		nzFree(t->value);
		t->value = display;
/* this should now be the same as t->rvalue, but I guess I'm
 * not going to check for that, or take advantage of it. */
	}			/* loop over tags */

	i_puts(MSG_FormReset);
}				/* formReset */

/* Fetch a field value (from a form) to post. */
/* The result is allocated */
static char *fetchTextVar(const struct htmlTag *t)
{
	char *v;

	if (t->jv && isJSAlive)
		return get_property_string(t->jv, "value");

	if (t->itype > INP_HIDDEN) {
		v = getFieldFromBuffer(t->seqno);
		if (v)
			return v;
	}

/* Revert to the default value */
	return cloneString(t->value);
}				/* fetchTextVar */

static bool fetchBoolVar(const struct htmlTag *t)
{
	int checked;

	if (t->jv && isJSAlive)
		return get_property_bool(t->jv,
					 (t->action ==
					  TAGACT_OPTION ? "selected" :
					  "checked"));

	checked = fieldIsChecked(t->seqno);
	if (checked < 0)
		checked = t->rchecked;
	return checked;
}				/* fetchBoolVar */

/* Some information on posting forms can be found here.
 * http://www.w3.org/TR/REC-html40/interact/forms.html */

static char *pfs;		/* post form string */
static int pfs_l;
static const char *boundary;

static void postDelimiter(char fsep)
{
	char c = pfs[strlen(pfs) - 1];
	if (c == '?' || c == '\1')
		return;
	if (fsep == '-') {
		stringAndString(&pfs, &pfs_l, "--");
		stringAndString(&pfs, &pfs_l, boundary);
		stringAndChar(&pfs, &pfs_l, '\r');
		fsep = '\n';
	}
	stringAndChar(&pfs, &pfs_l, fsep);
}				/* postDelimiter */

static bool
postNameVal(const char *name, const char *val, char fsep, uchar isfile)
{
	char *enc;
	const char *ct, *ce;	/* content type, content encoding */

	if (!name)
		name = emptyString;
	if (!val)
		val = emptyString;
	if (!*name && !*val)
		return true;

	postDelimiter(fsep);
	switch (fsep) {
	case '&':
		enc = encodePostData(name);
		stringAndString(&pfs, &pfs_l, enc);
		stringAndChar(&pfs, &pfs_l, '=');
		nzFree(enc);
		break;

	case '\n':
		stringAndString(&pfs, &pfs_l, name);
		stringAndString(&pfs, &pfs_l, "=\r\n");
		break;

	case '-':
		stringAndString(&pfs, &pfs_l,
				"Content-Disposition: form-data; name=\"");
		stringAndString(&pfs, &pfs_l, name);
		stringAndChar(&pfs, &pfs_l, '"');
/* I'm leaving nl off, in case we need ; filename */
		break;
	}			/* switch */

	if (!*val && fsep == '&')
		return true;

	switch (fsep) {
	case '&':
		enc = encodePostData(val);
		stringAndString(&pfs, &pfs_l, enc);
		nzFree(enc);
		break;

	case '\n':
		stringAndString(&pfs, &pfs_l, val);
		stringAndString(&pfs, &pfs_l, eol);
		break;

	case '-':
		if (isfile) {
			if (isfile & 2) {
				stringAndString(&pfs, &pfs_l, "; filename=\"");
				stringAndString(&pfs, &pfs_l, val);
				stringAndChar(&pfs, &pfs_l, '"');
			}
			if (!encodeAttachment(val, 0, true, &ct, &ce, &enc))
				return false;
			val = enc;
/* remember to free val in this case */
		} else {
			const char *s;
			ct = "text/plain";
/* Anything nonascii makes it 8bit */
			ce = "7bit";
			for (s = val; *s; ++s)
				if (*s < 0) {
					ce = "8bit";
					break;
				}
		}
		stringAndString(&pfs, &pfs_l, "\r\nContent-Type: ");
		stringAndString(&pfs, &pfs_l, ct);
		stringAndString(&pfs, &pfs_l,
				"\r\nContent-Transfer-Encoding: ");
		stringAndString(&pfs, &pfs_l, ce);
		stringAndString(&pfs, &pfs_l, "\r\n\r\n");
		stringAndString(&pfs, &pfs_l, val);
		stringAndString(&pfs, &pfs_l, eol);
		if (isfile)
			nzFree(enc);
		break;
	}			/* switch */

	return true;
}				/* postNameVal */

static bool formSubmit(const struct htmlTag *form, const struct htmlTag *submit)
{
	const struct htmlTag *t;
	int i, j, itype;
	char *name, *dynamicvalue = NULL;
/* dynamicvalue needs to be freed with nzFree. */
	const char *value;
	char fsep = '&';	/* field separator */
	bool noname = false, rc;
	bool bval;

	if (form->bymail)
		fsep = '\n';
	if (form->mime) {
		fsep = '-';
		boundary = makeBoundary();
		stringAndString(&pfs, &pfs_l, "`mfd~");
		stringAndString(&pfs, &pfs_l, boundary);
		stringAndString(&pfs, &pfs_l, eol);
	}

	for (i = 0; i < cw->numTags; ++i) {
		t = tagList[i];
		if (t->action != TAGACT_INPUT)
			continue;
		if (t->controller != form)
			continue;
		itype = t->itype;
		if (itype <= INP_SUBMIT && t != submit)
			continue;
		name = t->name;

		if (t == submit) {	/* the submit button you pushed */
			int namelen;
			char *nx;
			if (!name)
				continue;
			value = t->value;
			if (!value || !*value)
				value = "Submit";
			if (t->itype != INP_IMAGE)
				goto success;
			namelen = strlen(name);
			nx = (char *)allocMem(namelen + 3);
			strcpy(nx, name);
			strcpy(nx + namelen, ".x");
			postNameVal(nx, "0", fsep, false);
			nx[namelen + 1] = 'y';
			postNameVal(nx, "0", fsep, false);
			nzFree(nx);
			goto success;
		}

		if (itype >= INP_RADIO) {
			value = t->value;
			bval = fetchBoolVar(t);
			if (!bval)
				continue;
			if (!name)
				noname = true;
			if (value && !*value)
				value = 0;
			if (itype == INP_CHECKBOX && value == 0)
				value = "on";
			goto success;
		}

		if (itype < INP_FILE) {
/* Even a hidden variable can be adjusted by js.
 * fetchTextVar allows for this possibility.
 * I didn't allow for it in the above, the value of a radio button;
 * hope that's not a problem. */
			dynamicvalue = fetchTextVar(t);
			postNameVal(name, dynamicvalue, fsep, false);
			nzFree(dynamicvalue);
			dynamicvalue = NULL;
			continue;
		}

		if (itype == INP_TA) {
			int cx = t->lic;
			char *cxbuf;
			int cxlen;
			if (!name)
				noname = true;
			if (cx) {
				if (fsep == '-') {
					char cxstring[12];
/* do this as an attachment */
					sprintf(cxstring, "%d", cx);
					if (!postNameVal
					    (name, cxstring, fsep, 1))
						goto fail;
					continue;
				}	/* attach */
				if (!unfoldBuffer(cx, true, &cxbuf, &cxlen))
					goto fail;
				for (j = 0; j < cxlen; ++j)
					if (cxbuf[j] == 0) {
						setError(MSG_SessionNull, cx);
						nzFree(cxbuf);
						goto fail;
					}
				if (j && cxbuf[j - 1] == '\n')
					--j;
				if (j && cxbuf[j - 1] == '\r')
					--j;
				cxbuf[j] = 0;
				rc = postNameVal(name, cxbuf, fsep, false);
				nzFree(cxbuf);
				if (rc)
					continue;
				goto fail;
			}

			postNameVal(name, 0, fsep, false);
			continue;
		}

		if (itype == INP_SELECT) {
			char *display = getFieldFromBuffer(t->seqno);
			char *s, *e;
			if (!display) {	/* off the air */
				struct htmlTag *v;
				int i2;
/* revert back to reset state */
				for (i2 = 0; i2 < cw->numTags; ++i2) {
					v = tagList[i2];
					if (v->controller == t)
						v->checked = v->rchecked;
				}
				display = displayOptions(t);
			}
			rc = locateOptions(t, display, 0, &dynamicvalue, false);
			nzFree(display);
			if (!rc)
				goto fail;	/* this should never happen */
/* option could have an empty value, usually the null choice,
 * before you have made a selection. */
			if (!*dynamicvalue) {
				if (!t->multiple)
					postNameVal(name, dynamicvalue, fsep,
						    false);
				continue;
			}
/* Step through the options */
			for (s = dynamicvalue; *s; s = e) {
				char more;
				e = 0;
				if (t->multiple)
					e = strchr(s, '\1');
				if (!e)
					e = s + strlen(s);
				more = *e, *e = 0;
				postNameVal(name, s, fsep, false);
				if (more)
					++e;
			}
			nzFree(dynamicvalue);
			dynamicvalue = NULL;
			continue;
		}

		if (itype == INP_FILE) {	/* the only one left */
			dynamicvalue = fetchTextVar(t);
			if (!dynamicvalue)
				continue;
			if (!*dynamicvalue)
				continue;
			if (!(form->post & form->mime)) {
				setError(MSG_FilePost);
				nzFree(dynamicvalue);
				goto fail;
			}
			rc = postNameVal(name, dynamicvalue, fsep, 3);
			nzFree(dynamicvalue);
			dynamicvalue = NULL;
			if (rc)
				continue;
			goto fail;
		}

		i_printfExit(MSG_UnexSubmitForm);

success:
		postNameVal(name, value, fsep, false);
	}			/* loop over tags */

	if (form->mime) {	/* the last boundary */
		stringAndString(&pfs, &pfs_l, "--");
		stringAndString(&pfs, &pfs_l, boundary);
		stringAndString(&pfs, &pfs_l, "--\r\n");
	}

	i_puts(MSG_FormSubmit);
	return true;

fail:
	return false;
}				/* formSubmit */

/*********************************************************************
Push the reset or submit button.
This routine must be reentrant.
You push submit, which calls this routine, which runs the onsubmit code,
which checks the fields and calls form.submit(),
which calls this routine.  Happens all the time.
*********************************************************************/

bool infPush(int tagno, char **post_string)
{
	struct htmlTag *t = tagList[tagno];
	struct htmlTag *form;
	int itype;
	int actlen;
	const char *action = 0;
	char *section;
	const char *prot;
	bool rc;

	*post_string = 0;

/* If the tag is actually a form, then infPush() was invoked
 * by form.submit().
 * Revert t back to 0, since there may be multiple submit buttons
 * on the form, and we don't know which one was pushed. */
	if (t->action == TAGACT_FORM) {
		form = t;
		t = 0;
		itype = INP_SUBMIT;
	} else {
		form = t->controller;
		itype = t->itype;
	}

	if (itype > INP_SUBMIT) {
		setError(MSG_NoButton);
		return false;
	}

	if (!form && itype != INP_BUTTON) {
		setError(MSG_NotInForm);
		return false;
	}

	if (t && tagHandler(t->seqno, "onclick")) {
		if (!isJSAlive)
			runningError(itype ==
				     INP_BUTTON ? MSG_NJNoAction :
				     MSG_NJNoOnclick);
		else {
			if (t->jv)
				run_function_bool(t->jv, "onclick");
			jSideEffects();
			if (js_redirects)
				return true;
		}
	}

	if (itype == INP_BUTTON) {
		if (isJSAlive && t->jv && !handlerPresent(t->jv, "onclick")) {
			setError(MSG_ButtonNoJS);
			return false;
		}
		return true;
	}

	if (itype == INP_RESET) {
/* Before we reset, run the onreset code */
		if (t && tagHandler(form->seqno, "onreset")) {
			if (!isJSAlive)
				runningError(MSG_NJNoReset);
			else {
				rc = true;
				if (form->jv)
					rc = run_function_bool(form->jv,
							       "onreset");
				jSideEffects();
				if (!rc)
					return true;
				if (js_redirects)
					return true;
			}
		}		/* onreset */
		formReset(form);
		return true;
	}

	/* Before we submit, run the onsubmit code */
	if (t && tagHandler(form->seqno, "onsubmit")) {
		if (!isJSAlive)
			runningError(MSG_NJNoSubmit);
		else {
			rc = true;
			if (form->jv)
				rc = run_function_bool(form->jv, "onsubmit");
			jSideEffects();
			if (!rc)
				return true;
			if (js_redirects)
				return true;
		}
	}

	action = form->href;
/* But we defer to the java variable */
	if (form->jv && isJSAlive) {
		char *jh = get_property_url(form->jv, true);
		if (jh && (!action || !stringEqual(jh, action))) {
/* Tie action to the form tag, to plug a small memory leak */
			nzFree(form->href);
			form->href = resolveURL(cw->hbase, jh);
			action = form->href;
		}
		nzFree(jh);
	}

/* if no action, or action is "#", the default is the current location */
	if (!action || stringEqual(action, "#")) {
		action = cw->hbase;
	}

	if (!action) {
		setError(MSG_FormNoURL);
		return false;
	}

	debugPrint(2, "* %s", action);

	prot = getProtURL(action);
	if (!prot) {
		setError(MSG_FormBadURL);
		return false;
	}

	if (stringEqualCI(prot, "javascript")) {
		if (!isJSAlive) {
			setError(MSG_NJNoForm);
			return false;
		}
		javaParseExecute(form->jv, action, 0, 0);
		jSideEffects();
		return true;
	}

	form->bymail = false;
	if (stringEqualCI(prot, "mailto")) {
		if (!validAccount(localAccount))
			return false;
		form->bymail = true;
	} else if (stringEqualCI(prot, "http")) {
		if (form->secure) {
			setError(MSG_BecameInsecure);
			return false;
		}
	} else if (!stringEqualCI(prot, "https")) {
		setError(MSG_SubmitProtBad, prot);
		return false;
	}

	pfs = initString(&pfs_l);
	stringAndString(&pfs, &pfs_l, action);
	section = findHash(pfs);
	if (section) {
		i_printf(MSG_SectionIgnored, section);
		*section = 0;
		pfs_l = section - pfs;
	}
	section = strpbrk(pfs, "?\1");
	if (section && (*section == '\1' || !(form->bymail | form->post))) {
		debugPrint(3,
			   "the url already specifies some data, which will be overwritten by the data in this form");
		*section = 0;
		pfs_l = section - pfs;
	}

	stringAndChar(&pfs, &pfs_l, (form->post ? '\1' : '?'));
	actlen = strlen(pfs);

	if (!formSubmit(form, t)) {
		nzFree(pfs);
		return false;
	}

	debugPrint(3, "%s %s", form->post ? "post" : "get", pfs + actlen);

/* Handle the mail method here and now. */
	if (form->bymail) {
		char *addr, *subj, *q;
		const char *tolist[2], *atlist[2];
		const char *name = form->name;
		int newlen = strlen(pfs) - actlen;	/* the new string could be longer than post */
		decodeMailURL(action, &addr, &subj, 0);
		tolist[0] = addr;
		tolist[1] = 0;
		atlist[0] = 0;
		newlen += 9;	/* subject: \n */
		if (subj)
			newlen += strlen(subj);
		else
			newlen += 11 + (name ? strlen(name) : 1);
		++newlen;	/* null */
		++newlen;	/* encodeAttachment might append another nl */
		q = (char *)allocMem(newlen);
		if (subj)
			sprintf(q, "subject:%s\n", subj);
		else
			sprintf(q, "subject:html form(%s)\n",
				name ? name : "?");
		strcpy(q + strlen(q), pfs + actlen);
		nzFree(pfs);
		i_printf(MSG_MailSending, addr);
		sleep(1);
		rc = sendMail(localAccount, tolist, q, -1, atlist, 0, 0, false);
		if (rc)
			i_puts(MSG_MailSent);
		nzFree(addr);
		nzFree(subj);
		nzFree(q);
		*post_string = 0;
		return rc;
	}

	*post_string = pfs;
	return true;
}				/* infPush */

/* I don't have any reverse pointers, so I'm just going to scan the list */
/* This doesn't come up all that often. */
struct htmlTag *tagFromJavaVar(jsobjtype v)
{
	struct htmlTag *t = 0;
	int i;

	if (!tagList)
		i_printfExit(MSG_NullListInform);

	for (i = 0; i < cw->numTags; ++i) {
		t = tagList[i];
		if (t->jv == v)
			return t;
	}
	return 0;
}				/* tagFromJavaVar */

/* Like the above but create it if you can't find it. */
struct htmlTag *tagFromJavaVar2(jsobjtype v, const char *tagname)
{
	struct htmlTag *t = tagFromJavaVar(v);
	if (t)
		return t;
	if (!tagname)
		return 0;
	t = newTag(tagname);
	if (!t)
		return 0;
	t->jv = v;
/* this node now has a js object, don't decorate it again. */
	t->step = 2;
	return t;
}				/* tagFromJavaVar2 */

/* Return false to stop javascript, due to a url redirect */
void javaSubmitsForm(jsobjtype v, bool reset)
{
	if (reset)
		js_reset = v;
	else
		js_submit = v;
}				/* javaSubmitsForm */

bool handlerGoBrowse(const struct htmlTag *t, const char *name)
{
	if (!isJSAlive)
		return true;
	if (!t->jv)
		return true;
	return run_function_bool(t->jv, name);
}				/* handlerGoBrowse */

/* Javascript errors, we need to see these no matter what. */
void runningError(int msg, ...)
{
	va_list p;
	if (ismc)
		return;
	if (debugLevel <= 2)
		return;
	va_start(p, msg);
	vprintf(i_getString(msg), p);
	va_end(p);
	nl();
}				/* runningError */
