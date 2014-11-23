/* html.c
 * Parse html tags.
 * Copyright (c) Karl Dahlke, 2008
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"
#include "js.h"

static const char *const inp_types[] = {
	"reset", "button", "image", "submit",
	"hidden",
	"text", "password", "number", "file",
	"select", "textarea", "radio", "checkbox",
	0
};

static const char dfvl[] = "defaultValue";
static const char dfck[] = "defaultChecked";

struct htmlTag *topTag;
static char *topAttrib;
static char *basehref;
static struct htmlTag *currentForm;	/* the open form */
eb_bool parsePage;		/* parsing html */
int browseLine;			/* for error reporting */
static string radioChecked;
static string preamble;

/* paranoia check on the number of tags */
static void tagCountCheck(void)
{
	if (sizeof(int) == 4) {
		if (tagList.size() > MAXLINES)
			i_printfExit(MSG_LineLimit);
	}
}				/* tagCountCheck */

static eb_bool htmlAttrPresent(const char *e, const char *name)
{
	char *a;
	if (!(a = htmlAttrVal(e, name)))
		return eb_false;
	nzFree(a);
	return eb_true;
}				/* htmlAttrPresent */

static char *hrefVal(const char *e, const char *name)
{
	char *a;
	htmlAttrVal_nl = eb_true;
	a = htmlAttrVal(e, name);
	htmlAttrVal_nl = eb_false;	/* put it back */
	return a;
}				/* hrefVal */

static void toPreamble(int tagno, const char *msg, const char *j, const char *h)
{
	char buf[8];
	char fn[40], *s;

	sprintf(buf, "\r%c%d{", InternalCodeChar, tagno);
	preamble += buf;
	preamble += msg;

	if (h) {
		preamble += ": ";
		preamble += h;
	} else if (j) {
		skipWhite(&j);
		if (memEqualCI(j, "javascript:", 11))
			j += 11;
		skipWhite(&j);
		if (isalphaByte(*j) || *j == '_') {
			preamble += ": ";
			for (s = fn; isalnumByte(*j) || *j == '_'; ++j) {
				if (s < fn + sizeof(fn) - 3)
					*s++ = *j;
			}
			strcpy(s, "()");
			skipWhite(&j);
			if (*j == '(')
				preamble += fn;
		}
	}

	sprintf(buf, "%c0}\r", InternalCodeChar);
	preamble += buf;
}				/* toPreamble */

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

/* Close an open anchor when you see this tag. */
#define TAG_CLOSEA 1
/* You won't see the text between <foo> and </fooo> */
#define TAG_INVISIBLE 2
/* sometimes </foo> means nothing. */
#define TAG_NOSLASH 4
/* This tag should become a corresponding js object in the tree. */
#define TAG_JSOBJ 8

static const struct tagInfo elements[] = {
	{"BASE", "base reference for relative URLs", TAGACT_BASE, 0, 0, 13},
	{"A", "an anchor", TAGACT_A, 3, 0, 9},
	{"INPUT", "an input item", TAGACT_INPUT, 0, 0, 13},
	{"TITLE", "the title", TAGACT_TITLE, 3, 0, 9},
	{"TEXTAREA", "an input text area", TAGACT_TA, 3, 0, 9},
	{"SELECT", "an option list", TAGACT_SELECT, 3, 0, 9},
	{"OPTION", "a select option", TAGACT_OPTION, 0, 0, 13},
	{"SUB", "a subscript", TAGACT_SUB, 3, 0, 0},
	{"SUP", "a superscript", TAGACT_SUP, 3, 0, 0},
	{"OVB", "an overbar", TAGACT_OVB, 3, 0, 0},
	{"FONT", "a font", TAGACT_NOP, 3, 0, 0},
	{"CENTER", "centered text", TAGACT_NOP, 3, 0, 0},
	{"DOCWRITE", "document.write() text", TAGACT_DW, 0, 0, 0},
	{"CAPTION", "a caption", TAGACT_NOP, 3, 5, 0},
	{"HEAD", "the html header information", TAGACT_HEAD, 1, 0, 13},
	{"BODY", "the html body", TAGACT_BODY, 1, 0, 13},
	{"BGSOUND", "background music", TAGACT_MUSIC, 0, 0, 5},
	{"AUDIO", "audio passage", TAGACT_MUSIC, 0, 0, 5},
	{"META", "a meta tag", TAGACT_META, 0, 0, 12},
	{"IMG", "an image", TAGACT_IMAGE, 0, 0, 12},
	{"IMAGE", "an image", TAGACT_IMAGE, 0, 0, 12},
	{"BR", "a line break", TAGACT_BR, 0, 1, 4},
	{"P", "a paragraph", TAGACT_NOP, 1, 2, 13},
	{"DIV", "a divided section", TAGACT_DIV, 3, 5, 8},
	{"MAP", "a map of images", TAGACT_NOP, 3, 5, 8},
	{"HTML", "html", TAGACT_HTML, 1, 0, 13},
	{"BLOCKQUOTE", "a quoted paragraph", TAGACT_NOP, 1, 10, 9},
	{"H1", "a level 1 header", TAGACT_NOP, 1, 10, 9},
	{"H2", "a level 2 header", TAGACT_NOP, 1, 10, 9},
	{"H3", "a level 3 header", TAGACT_NOP, 1, 10, 9},
	{"H4", "a level 4 header", TAGACT_NOP, 1, 10, 9},
	{"H5", "a level 5 header", TAGACT_NOP, 1, 10, 9},
	{"H6", "a level 6 header", TAGACT_NOP, 1, 10, 9},
	{"DT", "a term", TAGACT_DT, 1, 2, 13},
	{"DD", "a definition", TAGACT_DT, 1, 1, 13},
	{"LI", "a list item", TAGACT_LI, 1, 1, 13},
	{"UL", "a bullet list", TAGACT_NOP, 3, 5, 9},
	{"DIR", "a directory list", TAGACT_NOP, 3, 5, 9},
	{"MENU", "a menu", TAGACT_NOP, 3, 5, 9},
	{"OL", "a numbered list", TAGACT_NOP, 3, 5, 9},
	{"DL", "a definition list", TAGACT_NOP, 3, 5, 9},
	{"HR", "a horizontal line", TAGACT_HR, 0, 5, 5},
	{"FORM", "a form", TAGACT_FORM, 1, 0, 9},
	{"BUTTON", "a button", TAGACT_INPUT, 0, 0, 13},
/* we traditionally write </frame>,
 * but it really isn't meaningful to put anything at all in between. Thus nest = 0. */
	{"FRAME", "a frame", TAGACT_FRAME, 0, 2, 13},
	{"IFRAME", "a frame", TAGACT_FRAME, 0, 2, 13},
	{"MAP", "an image map", TAGACT_MAP, 0, 2, 13},
	{"AREA", "an image map area", TAGACT_AREA, 0, 0, 9},
	{"TABLE", "a table", TAGACT_TABLE, 3, 10, 9},
	{"TR", "a table row", TAGACT_TR, 3, 5, 9},
	{"TD", "a table entry", TAGACT_TD, 3, 0, 9},
	{"TH", "a table heading", TAGACT_TD, 3, 0, 9},
	{"PRE", "a preformatted section", TAGACT_PRE, 3, 1, 0},
	{"LISTING", "a listing", TAGACT_PRE, 3, 1, 0},
	{"XMP", "an example", TAGACT_PRE, 3, 1, 0},
	{"FIXED", "a fixed presentation", TAGACT_NOP, 3, 1, 0},
	{"CODE", "a block of code", TAGACT_NOP, 3, 0, 0},
	{"SAMP", "a block of sample text", TAGACT_NOP, 3, 0, 0},
	{"ADDRESS", "an address block", TAGACT_NOP, 3, 1, 0},
	{"STYLE", "a style block", TAGACT_NOP, 0, 0, 2},
	{"SCRIPT", "a script", TAGACT_SCRIPT, 0, 0, 1},
	{"NOSCRIPT", "no script section", TAGACT_NOP, 3, 0, 3},
	{"NOFRAMES", "no frames section", TAGACT_NOP, 3, 0, 3},
	{"EMBED", "embedded html", TAGACT_MUSIC, 0, 0, 5},
	{"NOEMBED", "no embed section", TAGACT_NOP, 3, 0, 3},
	{"OBJECT", "an html object", TAGACT_OBJ, 0, 0, 3},
	{"EM", "emphasized text", TAGACT_JS, 3, 0, 0},
	{"LABEL", "a label", TAGACT_JS, 3, 0, 0},
	{"STRIKE", "emphasized text", TAGACT_JS, 3, 0, 0},
	{"S", "emphasized text", TAGACT_JS, 3, 0, 0},
	{"STRONG", "emphasized text", TAGACT_JS, 3, 0, 0},
	{"B", "bold text", TAGACT_JS, 3, 0, 0},
	{"I", "italicized text", TAGACT_JS, 3, 0, 0},
	{"U", "underlined text", TAGACT_JS, 3, 0, 0},
	{"DFN", "definition text", TAGACT_JS, 3, 0, 0},
	{"Q", "quoted text", TAGACT_JS, 3, 0, 0},
	{"ABBR", "an abbreviation", TAGACT_JS, 3, 0, 0},
	{"SPAN", "an html span", TAGACT_SPAN, 3, 0, 0},
	{"FRAMESET", "a frame set", TAGACT_JS, 3, 0, 1},
	{NULL, NULL, 0}
};

static const char *const handlers[] = {
	"onmousemove", "onmouseover", "onmouseout", "onmouseup", "onmousedown",
	"onclick", "ondblclick", "onblur", "onfocus",
	"onchange", "onsubmit", "onreset",
	"onload", "onunload",
	"onkeypress", "onkeyup", "onkeydown",
	0
};

void freeTags(struct ebWindow *w)
{
	int i1, n;
	struct htmlTag *t;
	vector < struct htmlTag *>*e;
	struct ebWindow *side;

/* if not browsing ... */
	if (!(e = (vector < struct htmlTag * >*)(w->tags)))
		return;

/* drop empty textarea buffers created by this session */
	for (i1 = 0; i1 < e->size(); ++i1) {
		t = (*e)[i1];
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
		side->changeMode = eb_false;
		cxQuit(n, 2);
	}			/* loop over tags */

	for (i1 = 0; i1 < e->size(); ++i1) {
		t = (*e)[i1];
		nzFree(t->attrib);
		nzFree(t->name);
		nzFree(t->id);
		nzFree(t->value);
		nzFree(t->href);
		nzFree(t->classname);

/* We probably don't need to do this at all,
 * since j context is soon to be destroyed. */
		if (t->jv && w->jss) {
			JSAutoRequest autoreq(w->jss->jcx);
			JSAutoCompartment ac(w->jss->jcx, w->jss->jwin);
			t->jv. ~ HeapRootedObject();
		}

		free(t);
	}

	delete e;
	w->tags = 0;
}				/* freeTags */

static void get_js_event(const char *name)
{
	char *s;
	int action = topTag->action;
	int itype = topTag->itype;

	if (!(s = htmlAttrVal(topAttrib, name)))
		return;		/* not there */

	if (topTag->jv && isJSAlive)
		handlerSet(topTag->jv, name, s);
	nzFree(s);

/* This code is only here to print warnings if js is disabled
 * or otherwise broken.  Record the fact that handlers exist,
 * outside the context of js. */
	if (stringEqual(name, "onclick")) {
		if (action == TAGACT_A || action == TAGACT_AREA
		    || action == TAGACT_FRAME || action == TAGACT_INPUT
		    && (itype >= INP_RADIO || itype <= INP_SUBMIT)
		    || action == TAGACT_OPTION) {
			topTag->onclick = eb_true;
			if (currentForm && action == TAGACT_INPUT
			    && itype == INP_BUTTON)
				currentForm->submitted = eb_true;
		}
	}
	if (stringEqual(name, "onsubmit")) {
		if (action == TAGACT_FORM)
			topTag->onsubmit = eb_true;
	}
	if (stringEqual(name, "onreset")) {
		if (action == TAGACT_FORM)
			topTag->onreset = eb_true;
	}
	if (stringEqual(name, "onchange")) {
		if (action == TAGACT_INPUT || action == TAGACT_SELECT) {
			if (itype == INP_TA)
				runningError(MSG_OnchangeText);
			else if (itype > INP_HIDDEN && itype <= INP_SELECT) {
				topTag->onchange = eb_true;
				if (currentForm)
					currentForm->submitted = eb_true;
			}
		}
	}
}				/* get_js_event */

static eb_bool strayClick;

static void get_js_events(void)
{
	int j;
	const char *t;
	int action = topTag->action;
	int itype = topTag->itype;

	for (j = 0; t = handlers[j]; ++j)
		get_js_event(t);

	if (!topTag->jv)
		return;
	if (!isJSAlive)
		return;

/* Some warnings about some handlers that we just don't "handle" */
	if (handlerPresent(topTag->jv, "onkeypress") ||
	    handlerPresent(topTag->jv, "onkeyup")
	    || handlerPresent(topTag->jv, "onkeydown"))
		browseError(MSG_JSKeystroke);
	if (handlerPresent(topTag->jv, "onfocus")
	    || handlerPresent(topTag->jv, "onblur"))
		browseError(MSG_JSFocus);
	if (handlerPresent(topTag->jv, "ondblclick"))
		runningError(MSG_Doubleclick);
	if (handlerPresent(topTag->jv, "onclick")) {
		if ((action == TAGACT_A || action == TAGACT_AREA || action == TAGACT_FRAME) && topTag->href || action == TAGACT_INPUT && (itype <= INP_SUBMIT || itype >= INP_RADIO)) ;	/* ok */
		else
			browseError(MSG_StrayOnclick);
	}
	if (handlerPresent(topTag->jv, "onchange")) {
		if (action != TAGACT_INPUT && action != TAGACT_SELECT
		    || itype == INP_TA)
			browseError(MSG_StrayOnchange);
	}
/* Other warnings might be appropriate, but I'm going to assume this
 * is valid javascript, and you won't put an onsubmit function on <P> etc */
}				/* get_js_events */

eb_bool tagHandler(int seqno, const char *name)
{
	struct htmlTag *t = tagList[seqno];
/* check the htnl tag attributes first */
	if (t->onclick && stringEqual(name, "onclick"))
		return eb_true;
	if (t->onsubmit && stringEqual(name, "onsubmit"))
		return eb_true;
	if (t->onreset && stringEqual(name, "onreset"))
		return eb_true;
	if (t->onchange && stringEqual(name, "onchange"))
		return eb_true;

	if (!t->jv)
		return eb_false;
	if (!isJSAlive)
		return eb_false;
	return handlerPresent(t->jv, name);
}				/* tagHandler */

static char *getBaseHref(int n)
{
	const struct htmlTag *t;
	if (parsePage)
		return basehref;
	if (n < 0)
		n = tagList.size();
	do
		--n;
	while ((t = tagList[n])->action != TAGACT_BASE);
	return t->href;
}				/* getBaseHref */

static void htmlMeta(void)
{
	char *name, *content, *heq;
	char **ptr;

	name = topTag->name;
	content = htmlAttrVal(topAttrib, "content");
	if (content == EMPTYSTRING)
		content = 0;

	domLink("Meta", 0, "metas", cw->jss->jdoc, eb_false);
	if (topTag->jv)
		establish_property_string(topTag->jv, "content",
					  content, eb_true);

	heq = htmlAttrVal(topAttrib, "http-equiv");
	if (heq == EMPTYSTRING)
		heq = 0;

	if (heq && content) {
		eb_bool rc;
		int delay;
/* It's not clear if we should process the http refresh command
 * immediately, the moment we spot it, or if we finish parsing
 * all the html first.
 * Does it matter?  It might.
 * A subsequent meta tag could use http-equiv to set a cooky,
 * and we won't see that cooky if we jump to the new page right now.
 * And there's no telling what subsequent javascript might do.
 * So - I'm going to postpone the refresh, until everything is parsed.
 * Bear in mind, we really don't want to refresh if we're working
 * on a local file. */
		if (stringEqualCI(heq, "Set-Cookie")) {
			rc = receiveCookie(cw->fileName, content);
			debugPrint(3, rc ? "jar" : "rejected");
		}

		if (allowRedirection && !browseLocal
		    && stringEqualCI(heq, "Refresh")) {
			if (parseRefresh(content, &delay)) {
				char *newcontent;
				unpercentURL(content);
				newcontent = resolveURL(basehref, content);
				gotoLocation(newcontent, delay, eb_true);
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
			stripWhite(content);
			*ptr = content;
			content = 0;
		}
	}

	nzFree(content);
	nzFree(heq);
}				/* htmlMeta */

static void htmlName(void)
{
	char *name = htmlAttrVal(topAttrib, "name");
	char *id = htmlAttrVal(topAttrib, "id");
	char *classname = htmlAttrVal(topAttrib, "class");
	if (name == EMPTYSTRING)
		name = 0;
	topTag->name = name;
	if (id == EMPTYSTRING)
		id = 0;
	topTag->id = id;
	if (classname == EMPTYSTRING)
		classname = 0;
	topTag->classname = classname;
}				/* htmlName */

static void htmlHref(const char *desc)
{
	char *h = hrefVal(topAttrib, desc);
	if (h == EMPTYSTRING) {
		h = 0;
		if (topTag->action == TAGACT_A)
			h = cloneString("#");
	}
	if (h) {
		unpercentURL(h);
		topTag->href = resolveURL(basehref, h);
		free(h);
	}
}				/* htmlHref */

static void formControl(eb_bool namecheck)
{
	const char *typedesc;
	int itype = topTag->itype;
	int isradio = itype == INP_RADIO;
	int isselect = (itype == INP_SELECT) * 2;
	char *myname = (topTag->name ? topTag->name : topTag->id);

	if (currentForm) {
		topTag->controller = currentForm;
	} else if (itype != INP_BUTTON)
		browseError(MSG_NotInForm2, topTag->info->desc);

	if (namecheck && !myname)
		browseError(MSG_FieldNoName, topTag->info->desc);

	if (isJSAlive) {
		if (currentForm && currentForm->jv) {
			domLink("Element", 0, "elements", currentForm->jv,
				isradio | isselect);
		} else {
			domLink("Element", 0, 0, cw->jss->jdoc,
				isradio | isselect);
		}
	}

	get_js_events();

	if (!topTag->jv)
		return;
	if (!isJSAlive)
		return;

	if (itype <= INP_RADIO) {
		establish_property_string(topTag->jv, "value", topTag->value,
					  eb_false);
		if (itype != INP_FILE) {
/* No default value on file, for security reasons */
			establish_property_string(topTag->jv, dfvl,
						  topTag->value, eb_true);
		}		/* not file */
	}

	if (isselect)
		typedesc = topTag->multiple ? "select-multiple" : "select-one";
	else
		typedesc = inp_types[itype];
	establish_property_string(topTag->jv, "type", typedesc, eb_true);

	if (itype >= INP_RADIO) {
		establish_property_bool(topTag->jv, "checked", topTag->checked,
					eb_false);
		establish_property_bool(topTag->jv, dfck, topTag->checked,
					eb_true);
	}
}				/* formControl */

static void htmlImage(void)
{
	char *a;

	htmlHref("src");

	if (!isJSAlive)
		return;

	domLink("Image", "src", "images", cw->jss->jdoc, eb_false);

	get_js_events();

/* don't know if javascript ever looks at alt.  Probably not. */
	if (!topTag->jv)
		return;
	a = htmlAttrVal(topAttrib, "alt");
	if (a)
		establish_property_string(topTag->jv, "alt", a, eb_true);
	nzFree(a);
}				/* htmlImage */

static void htmlForm(void)
{
	char *a;

	if (topTag->slash)
		return;
	currentForm = topTag;
	htmlHref("action");

	a = htmlAttrVal(topAttrib, "method");
	if (a) {
		if (stringEqualCI(a, "post"))
			topTag->post = eb_true;
		else if (!stringEqualCI(a, "get"))
			browseError(MSG_GetPost);
		nzFree(a);
	}

	a = htmlAttrVal(topAttrib, "enctype");
	if (a) {
		if (stringEqualCI(a, "multipart/form-data"))
			topTag->mime = eb_true;
		else if (!stringEqualCI(a, "application/x-www-form-urlencoded"))
			browseError(MSG_Enctype);
		nzFree(a);
	}

	if (a = topTag->href) {
		const char *prot = getProtURL(a);
		if (prot) {
			if (stringEqualCI(prot, "mailto"))
				topTag->bymail = eb_true;
			else if (stringEqualCI(prot, "javascript"))
				topTag->javapost = eb_true;
			else if (stringEqualCI(prot, "https"))
				topTag->secure = eb_true;
			else if (!stringEqualCI(prot, "http"))
				browseError(MSG_FormProtBad, prot);
		}
	}

	radioChecked.clear();

	if (!isJSAlive)
		return;

	domLink("Form", "action", "forms", cw->jss->jdoc, eb_false);
	if (!topTag->jv)
		return;

	get_js_events();

	establish_property_array(topTag->jv, "elements");
}				/* htmlForm */

/*********************************************************************
This function was originally written to print any strings generated by
document.write(), and it still does that,
but it also does any cleanup or post-scans to see if js
has changed anything.
Every js activity should start with jSyncup() and end with jsdw().
*********************************************************************/

static void scriptsPending(void);
void jsdw(void)
{
	int side;

/* Are there other scripts waiting to run? */
	scriptsPending();

	if (cw->dw) {
/* replace the <docwrite> tag with <html> */
		memcpy(cw->dw + 3, "<html>\n", 7);
		side = sideBuffer(0, cw->dw + 3, -1, cw->fileName, eb_true);
		if (side) {
			i_printf(MSG_SideBufferX, side);
		} else {
			i_puts(MSG_NoSideBuffer);
			printf("%s\n", cw->dw + 10);
		}
		nzFree(cw->dw);
		cw->dw = 0;
		cw->dw_l = 0;
	}

	rebuildSelectors();
}				/* jsdw */

static void htmlInput(void)
{
	int n = INP_TEXT;
	int len;
	char *myname = (topTag->name ? topTag->name : topTag->id);
	char *s = htmlAttrVal(topAttrib, "type");
	if (s && *s) {
		caseShift(s, 'l');
		n = stringInList(inp_types, s);
		if (n < 0) {
			browseError(MSG_InputType, s);
			n = INP_TEXT;
		}
	} else if (stringEqual(topTag->info->name, "BUTTON")) {
		n = INP_BUTTON;
	}

	nzFree(s);
	topTag->itype = n;

	if (htmlAttrPresent(topAttrib, "readonly"))
		topTag->rdonly = eb_true;
	s = htmlAttrVal(topAttrib, "maxlength");
	len = 0;
	if (s)
		len = stringIsNum(s);
	nzFree(s);
	if (len > 0)
		topTag->lic = len;
/* store the original text in value. */
/* This makes it easy to push the reset button. */
	s = htmlAttrVal(topAttrib, "value");
	if (!s)
		s = EMPTYSTRING;
	topTag->value = s;
	if (n >= INP_RADIO && htmlAttrPresent(topAttrib, "checked")) {
		char namebuf[200];
		if (n == INP_RADIO && myname
		    && strlen(myname) < sizeof(namebuf) - 3) {
			if (!radioChecked.length())
				radioChecked = "|";
			sprintf(namebuf, "|%s|", topTag->name);
			if (radioChecked.find(namebuf) != string::npos) {
				browseError(MSG_RadioMany);
				return;
			}
			radioChecked += namebuf + 1;
		}		/* radio name */
		topTag->rchecked = eb_true;
		topTag->checked = eb_true;
	}

	/* Even the submit fields can have a name, but they don't have to */
	formControl(n > INP_SUBMIT);
}				/* htmlInput */

static void makeButton(void)
{
	struct htmlTag *t =
	    (struct htmlTag *)allocZeroMem(sizeof(struct htmlTag));
	t->seqno = tagList.size();
	tagList.push_back(t);
	tagCountCheck();
	t->info = elements + 2;
	t->action = TAGACT_INPUT;
	t->controller = currentForm;
	t->itype = INP_SUBMIT;
}				/* makeButton */

/* display the checked options */
char *displayOptions(const struct htmlTag *sel)
{
	const struct htmlTag *t;
	string options;

	for (int i1 = 0; i1 < tagList.size(); ++i1) {
		t = tagList[i1];
		if (t->controller != sel)
			continue;
		if (!t->checked)
			continue;
		if (options.length())
			options += ',';
		options += t->name;
	}

	return cloneString(options.c_str());
}				/* displayOptions */

static struct htmlTag *locateOptionByName(const struct htmlTag *sel,
					  const char *name, int *pmc,
					  eb_bool exact)
{
	struct htmlTag *t, *em = 0, *pm = 0;
	int pmcount = 0;	/* partial match count */
	const char *s;
	for (int i1 = 0; i1 < tagList.size(); ++i1) {
		t = tagList[i1];
		if (t->controller != sel)
			continue;
		if (!(s = t->name))
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
	for (int i1 = 0; i1 < tagList.size(); ++i1) {
		t = tagList[i1];
		if (t->controller != sel)
			continue;
		if (!t->name)
			continue;
		++cnt;
		if (cnt == n)
			return t;
	}
	return 0;
}				/* locateOptionByNum */

static eb_bool
locateOptions(const struct htmlTag *sel, const char *input,
	      char **disp_p, char **val_p, eb_bool setcheck)
{
	struct htmlTag *t;
	string display, value;
	int len = strlen(input);
	int n, pmc, cnt;
	const char *s, *e;	/* start and end of an option */
	char *iopt;		/* individual option */

	iopt = (char *)allocMem(len + 1);

	if (setcheck) {
/* Uncheck all existing options, then check the ones selected. */
		if (sel->jv && isJSAlive)
			set_property_number(sel->jv, "selectedIndex", -1);
		for (int i1 = 0; i1 < tagList.size(); ++i1) {
			t = tagList[i1];
			if (t->controller == sel && t->name) {
				t->checked = eb_false;
				if (t->jv && isJSAlive)
					set_property_bool(t->jv, "selected",
							  eb_false);
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

		t = locateOptionByName(sel, iopt, &pmc, eb_true);
		if (!t) {
			n = stringIsNum(iopt);
			if (n >= 0)
				t = locateOptionByNum(sel, n);
		}
		if (!t)
			t = locateOptionByName(sel, iopt, &pmc, eb_false);
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
			if (value.length())
				value += '\1';
			value += t->value;
		}

		if (disp_p) {
			if (display.length())
				display += ',';
			display += t->name;
		}

		if (setcheck) {
			t->checked = eb_true;
			if (t->jv && isJSAlive) {
				set_property_bool(t->jv, "selected", eb_true);
				if (sel->jv && isJSAlive)
					set_property_number(sel->jv,
							    "selectedIndex",
							    t->lic);
			}
		}
	}			/* loop over multiple options */

	if (val_p)
		*val_p = cloneString(value.c_str());
	if (disp_p)
		*disp_p = cloneString(display.c_str());
	free(iopt);
	return eb_true;

fail:
	free(iopt);
	if (val_p)
		*val_p = 0;
	if (disp_p)
		*disp_p = 0;
	return eb_false;
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
	const struct htmlTag *t;
	int itype, j, cx;
	char *value, *cxbuf;

	if (parsePage)
		return;		/* not necessary */
	if (!isJSAlive)
		return;
	debugPrint(5, "jSyncup starts");

	for (int i1 = 0; i1 < tagList.size(); ++i1) {
		t = tagList[i1];
		if (t->action != TAGACT_INPUT)
			continue;
		if (!t->jv)
			continue;
		itype = t->itype;
		if (itype <= INP_HIDDEN)
			continue;

		if (itype >= INP_RADIO) {
			int checked = fieldIsChecked(t->seqno);
			if (checked < 0)
				checked = t->rchecked;
			set_property_bool(t->jv, "checked", checked);
			continue;
		}

		value = getFieldFromBuffer(t->seqno);
/* If that line has been deleted from the user's buffer,
 * indicated by value = 0,
 * revert back to the original (reset) value. */

		if (itype == INP_SELECT) {
			locateOptions(t, (value ? value : t->value), 0, 0,
				      eb_true);
			if (!t->multiple)
				value = get_property_option(t->jv);
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
			if (!unfoldBuffer(cx, eb_false, &cxbuf, &j))
				continue;
			set_property_string(t->jv, "value", cxbuf);
			nzFree(cxbuf);
			continue;
		}

		if (value) {
			set_property_string(t->jv, "value", value);
			nzFree(value);
		} else
			set_property_string(t->jv, "value", t->value);
	}			/* loop over tags */

	debugPrint(5, "jSyncup ends");
}				/* jSyncup */

/* Find the <foo> tag to match </foo> */
/* If name is null then find the most recent open tag. */
static struct htmlTag *findOpenTag(const char *name)
{
	struct htmlTag *t;
	eb_bool closing = topTag->slash;
	eb_bool match;
	const char *desc = topTag->info->desc;

	for (int i1 = tagList.size() - 2; i1 >= 0; --i1) {
		t = tagList[i1];
		if (t->balanced)
			continue;
		if (!t->info->nest)
			continue;
		if (t->slash)
			continue;	/* unbalanced slash, should never happen */
/* Now we have an unbalanced open tag */
		if (!name)
			return t;
		match = stringEqualCI(t->info->name, name);
/* I expect tags to nest perfectly, like labeled parentheses */
		if (closing) {
			if (match)
				return t;
			if (t->info->nest & 2)
				browseError(MSG_TagNest, desc, t->info->desc);
			continue;
		}
		if (!match)
			continue;
		if (!(t->info->nest & 2))
			browseError(MSG_TagInTag, desc, desc);
		return t;
	}			/* loop */

	if (closing)
		browseError(MSG_TagClose, desc);
	return NULL;
}				/* findOpenTag */

void makeParentNode(const struct htmlTag *t)
{
	const struct htmlTag *v;
	int i;

/* there should always be an object on t */
	if (!t->jv)
		return;

/* this test should also pass */
	if (!(t->info->bits & TAG_JSOBJ))
		return;

	for (i = t->seqno - 1; i >= 0; --i) {
		v = tagList[i];
		if ((v->info->bits & TAG_JSOBJ) &&
		    v->info->nest && !v->balanced)
			break;
	}

	if (i < 0) {
/* nothing open, link to document */
		debugPrint(5, "parent %s > document", t->info->name);
		establish_property_object(t->jv, "parentNode", cw->jss->jdoc);
		return;
	}

/* parent tag should also have a js object */
	if (!v->jv)
		return;

	debugPrint(5, "parent %s > %s", t->info->name, v->info->name);
	establish_property_object(t->jv, "parentNode", v->jv);
}				/* makeParentNode */

struct htmlTag *newTag(const char *name)
{
	struct htmlTag *t;
	const struct tagInfo *ti;
	int action;

	for (ti = elements; ti->name; ++ti)
		if (stringEqualCI(ti->name, name))
			break;
	if (!ti->name)
		return 0;

	action = ti->action;
	t = (struct htmlTag *)allocZeroMem(sizeof(struct htmlTag));
	t->action = action;
	t->info = ti;
	t->seqno = tagList.size();
	t->balanced = eb_true;
	if (stringEqual(name, "a"))
		t->clickable = eb_true;
	tagList.push_back(t);
	tagCountCheck();
	return t;
}				/* newTag */

/* This is only called if js is alive */
static void
onloadGo(JS::HandleObject obj, const char *jsrc, const char *tagname)
{
	struct htmlTag *t;
	char buf[32];

/* The first one is easy - one line of code. */
	handlerGo(obj, "onload");

	if (!handlerPresent(obj, "onunload"))
		return;
	if (handlerPresent(obj, "onclick")) {
		runningError(MSG_UnloadClick);
		return;
	}

	t = newTag("a");
	t->jv = obj;
	t->href = cloneString("#");
	link_onunload_onclick(obj);
	sprintf(buf, "on close %s", tagname);
	caseShift(buf, 'm');
	toPreamble(t->seqno, buf, jsrc, 0);
}				/* onloadGo */

/* Given a tag with an id attribute whose value is foo, generate a label
 * <a name="foo">.  If the tag has no id attribute, this function is a no-op.
*/

static void htmlLabelID(string & newstr)
{
	char buf[16];
	char *id = htmlAttrVal(topAttrib, "id");
	if (id) {
		struct htmlTag *t = newTag("a");
		int tagnum = t->seqno;
		t->name = id;
		sprintf(buf, "%c%d*", InternalCodeChar, tagnum);
		newstr += buf;
	}
}				/* htmlLabelID */

static void htmlOption(struct htmlTag *sel, struct htmlTag *v, const char *a)
{
	if (!*a) {
		browseError(MSG_OptionEmpty);
	} else {
		if (!v->value)
			v->value = cloneString(a);
	}

	if (!sel->jv)
		return;
	if (!isJSAlive)
		return;

	v->jv = establish_js_option(sel->jv, v->lic);
	establish_property_string(v->jv, "text", v->name, eb_true);
	establish_property_string(v->jv, "value", v->value, eb_true);
	establish_property_string(v->jv, "nodeName", "OPTION", eb_true);
	establish_property_bool(v->jv, "selected", v->checked, eb_false);
	establish_property_bool(v->jv, "defaultSelected", v->checked, eb_true);
	debugPrint(5, "parent OPTION > SELECT");
	establish_property_object(v->jv, "parentNode", sel->jv);

	if (v->checked && !sel->multiple) {
		set_property_number(sel->jv, "selectedIndex", v->lic);
		set_property_string(sel->jv, "value", v->value);
	}
}				/* htmlOption */

static char *javatext;
static void htmlScript(char *&html, char *&h)
{
	char *a = 0, *w = 0;
	struct htmlTag *t = topTag;	// shorthand
	int js_line;
	char *js_file;

	if (!isJSAlive)
		goto done;
	if (intFlag)
		goto done;

/* Create the script object. */
	htmlHref("src");
	domLink("Script", "src", "scripts", cw->jss->jdoc, eb_false);

	a = htmlAttrVal(topAttrib, "type");
	if (a)
		establish_property_string(t->jv, "type", a, eb_true);

	a = htmlAttrVal(topAttrib, "language");
	if (a)
		establish_property_string(t->jv, "language", a, eb_true);

	if (!isJSAlive)
		goto done;
/* If no language is specified, javascript is default. */
	if (a && (!memEqualCI(a, "javascript", 10) || isalphaByte(a[10])))
		goto done;

/* It's javascript, run with the source, or the inline text. */
	js_line = browseLine;
	js_file = cw->fileName;
	if (t->href) {		/* fetch the javascript page */
		nzFree(javatext);
		javatext = 0;
		if (javaOK(t->href)) {
			debugPrint(2, "java source %s", t->href);
			if (browseLocal && !isURL(t->href)) {
				if (!fileIntoMemory
				    (t->href, &serverData, &serverDataLen)) {
					runningError(MSG_GetLocalJS, errorMsg);
				} else {
					javatext = serverData;
					prepareForBrowse(javatext,
							 serverDataLen);
				}
			} else if (httpConnect(basehref, t->href)) {
				if (hcode == 200) {
					javatext = serverData;
					prepareForBrowse(javatext,
							 serverDataLen);
				} else {
					nzFree(serverData);
					runningError(MSG_GetJS, t->href, hcode);
				}
			} else {
				runningError(MSG_GetJS2, errorMsg);
			}
			js_line = 1;
			js_file = t->href;
			nzFree(changeFileName);
			changeFileName = NULL;
		}
	}

	if (!javatext)
		goto done;

	if (js_file)
		w = strrchr(js_file, '/');
	if (w) {
/* Trailing slash doesn't count */
		if (w[1] == 0 && w > js_file)
			for (--w; w >= js_file && *w != '/'; --w) ;
		js_file = w + 1;
	}
	debugPrint(3, "execute %s at %d", js_file, js_line);
/* mark this script as having been executed */
	establish_property_bool(t->jv, "exec$$ed", eb_true, eb_true);
	establish_property_string(t->jv, "data", javatext, eb_true);
/* now run the script */
	javaParseExecute(cw->jss->jwin, javatext, js_file, js_line);
	debugPrint(3, "execution complete");

/* See if the script has produced html via document.write() */
	if (cw->dw) {
		int afterlen;	/* after we fold in this string */
		char *after;
		debugPrint(3, "docwrite %d bytes", cw->dw_l);
		debugPrint(4, "<<\n%s\n>>", cw->dw + 10);
		stringAndString(&cw->dw, &cw->dw_l, "</docwrite>");
		afterlen = strlen(h) + strlen(cw->dw);
		after = (char *)allocMem(afterlen + 1);
		strcpy(after, cw->dw);
		strcat(after, h);
		nzFree(cw->dw);
		cw->dw = 0;
		cw->dw_l = 0;
		nzFree(html);
		html = h = after;

/* After the realloc, the inner pointers are no longer valid. */
		for (int i1 = 0; i1 < tagList.size(); ++i1) {
			t = tagList[i1];
			t->inner = 0;
		}
	}

done:
	nzFree(javatext);
	javatext = 0;
	nzFree(a);
}				/* htmlScript */

static void objectScript(JS::Handle < JSObject * >obj)
{
	char *lang = 0;
	const char *w = 0;
	char *jsrc = 0, *jtext = 0;

	if (!isJSAlive)
		return;
	if (intFlag)
		return;

/* can't be some other language */
	lang = get_property_string(obj, "language");
	if (lang
	    && (!memEqualCI(lang, "javascript", 10) || isalphaByte(lang[10])))
		goto done;

	jsrc = get_property_string(obj, "src");
	if (jsrc) {
		char *h;
		unpercentURL(jsrc);
		h = resolveURL(getBaseHref(-1), jsrc);
		if (h) {
			nzFree(jsrc);
			jsrc = h;
		}

		if (!javaOK(jsrc))
			goto done;
		debugPrint(2, "java source %s", jsrc);

		if (browseLocal && !isURL(jsrc)) {
			if (!fileIntoMemory(jsrc, &serverData, &serverDataLen)) {
				runningError(MSG_GetLocalJS, errorMsg);
				goto done;
			}
			jtext = serverData;
			prepareForBrowse(jtext, serverDataLen);
			goto execute;
		}

		if (!httpConnect(getBaseHref(-1), jsrc)) {
			runningError(MSG_GetJS2, errorMsg);
			goto done;
		}

		if (hcode != 200) {
			nzFree(serverData);
			runningError(MSG_GetJS, jsrc, hcode);
			goto done;
		}

		jtext = serverData;
		prepareForBrowse(jtext, serverDataLen);
		goto execute;
	}

	jtext = get_property_string(obj, "data");
	if (!jtext)
		goto done;
	debugPrint(2, "java source dynamic");
	prepareForBrowse(jtext, strlen(jtext));

execute:
	w = "script";
	if (jsrc) {
		establish_property_string(obj, "data", jtext, eb_true);
		if (w = strrchr(jsrc, '/')) {
/* Trailing slash doesn't count */
			if (w[1] == 0 && w > jsrc)
				for (--w; w >= jsrc && *w != '/'; --w) ;
			++w;
		} else
			w = jsrc;
	}

	debugPrint(3, "execute %s", w);
	javaParseExecute(cw->jss->jwin, jtext, w, 0);
	debugPrint(3, "execution complete");

done:
	nzFree(jtext);
	nzFree(jsrc);
	nzFree(lang);
	nzFree(changeFileName);
	changeFileName = NULL;
/* mark this script as having been executed, even if it didn't run properly */
	establish_property_bool(obj, "exec$$ed", eb_true, eb_true);
}				/* objectScript */

/* runs scripts that have ben dynamically created */
static void scriptsPending(void)
{
	SWITCH_COMPARTMENT();
	JS::RootedObject obj(cw->jss->jcx);
	while (obj = run_function_object(cw->jss->jdoc, "script$$pending"))
		objectScript(obj);
}				/* scriptsPending */

/*********************************************************************
Encode the html tags - parse the web page.
Always returns a string, even if errors were found.
Internet web pages often contain errors!
This routine mucks with the passed-in string, and frees it
when finished.  Thus the argument is not const.
*********************************************************************/

static char *encodeTags(char *html)
{
	const struct tagInfo *ti;
	struct htmlTag *t, *open, *v;
	char *save_h;
	char *h = html;
	char *a;		/* for a specific attribute */
	string newstr;		/* the new string */
	int js_nl;		/* number of newlines in javascript */
	int i1, i2;		/* iterators */
	const char *name, *attrib, *end;
	char tagname[12];
	int tagno;		// number of current tag
	const char *s;
	int j, l, namelen, lns;
	int dw_line, dw_nest = 0;
	char hnum[40];		/* hidden number */
	char c;
	eb_bool retainTag, onload_done = eb_false;
	eb_bool a_text;		/* visible text within the hyperlink */
	eb_bool slash, a_href, rc;
	eb_bool premode = eb_false, invisible = eb_false;
/* Tags that cannot nest, one open at a time. */
	struct htmlTag *currentA;	/* the open anchor */
	struct htmlTag *currentSel;	/* the open select */
	struct htmlTag *currentOpt;	/* the current option */
	struct htmlTag *currentTitle;
	struct htmlTag *currentTA;	/* text area */
	int offset;		/* where the current x starts */
	int ln = 1;		/* line number */
	int action;		/* action of the tag */
	int lastact = 0;
	int nopt;		/* number of options */
	int intable = 0, inrow = 0;
	eb_bool tdfirst;

	currentA = currentForm = currentSel = currentOpt = currentTitle =
	    currentTA = 0;
	cw->tags = new vector < struct htmlTag *>;
	preamble.clear();

/* first tag is a base tag, from the filename */
	t = newTag("base");
	t->href = cloneString(cw->fileName);
	basehref = t->href;

top:
	while (c = *h) {
		if (c != '<') {
			if (c == '\n')
				++ln;	/* keep track of line numbers */
putc:
			if (!invisible) {
				if (strchr("\r\n\f", c) && !currentTA) {
					if (!premode || c == '\r'
					    && h[1] == '\n')
						c = ' ';
					else if (c == '\r')
						c = '\n';
				}
				if (lastact == TAGACT_TD && c == ' ')
					goto nextchar;
				newstr += c;
				if (!isspaceByte(c)) {
					a_text = eb_true;
					lastact = 0;
				}
			}
nextchar:
			++h;
			continue;
		}

		if (h[1] == '!' || h[1] == '?') {
			h = (char *)skipHtmlComment(h, &lns);
			ln += lns;
			continue;
		}

		if (!parseTag(h, &name, &namelen, &attrib, &end, &lns))
			goto putc;

/* html tag found */
		save_h = h;
		h = (char *)end;	/* skip past tag */
		if (!dw_nest)
			browseLine = ln;
		ln += lns;
		slash = eb_false;
		if (*name == '/')
			slash = eb_true, ++name, --namelen;
		if (namelen > sizeof(tagname) - 1)
			namelen = sizeof(tagname) - 1;
		strncpy(tagname, name, namelen);
		tagname[namelen] = 0;

		for (ti = elements; ti->name; ++ti)
			if (stringEqualCI(ti->name, tagname))
				break;
		action = ti->action;
		tagno = tagList.size();
		debugPrint(7, "tag %s %d %d %d", tagname, tagno, ln, action);

		if (currentTA) {
/* Sometimes a textarea is used to present a chunk of html code.
 * "Cut and paste this into your web page."
 * So it may contain tags.  Ignore them!
 * All except the textarea tag. */
			if (action != TAGACT_TA) {
				ln = browseLine, h = save_h;	/* back up */
				goto putc;
			}
/* Close it off */
			currentTA->action = TAGACT_INPUT;
			currentTA->itype = INP_TA;
			currentTA->balanced = eb_true;
			s = currentTA->value =
			    andTranslate(newstr.substr(offset).c_str(),
					 eb_true);
/* Text starts at the next line boundary */
			while (*s == '\t' || *s == ' ')
				++s;
			if (*s == '\r')
				++s;
			if (*s == '\n')
				++s;
			if (s > currentTA->value)
				strmove(currentTA->value, s);
			a = currentTA->value;
			a += strlen(a);
			while (a > currentTA->value
			       && (a[-1] == ' ' || a[-1] == '\t'))
				--a;
			*a = 0;
			if (currentTA->jv && isJSAlive) {
				establish_innerHTML(currentTA->jv,
						    currentTA->inner, save_h,
						    eb_true);
				establish_property_string(currentTA->jv,
							  "value",
							  currentTA->value,
							  eb_false);
				establish_property_string(currentTA->jv, dfvl,
							  currentTA->value,
							  eb_true);
			}
			newstr.resize(offset);
			j = sideBuffer(0, currentTA->value, -1, 0, eb_false);
			if (j) {
				currentTA->lic = j;
				sprintf(hnum, "%c%d<buffer %d%c0>",
					InternalCodeChar, currentTA->seqno, j,
					InternalCodeChar);
				newstr += hnum;
			} else
				newstr += "<buffer ?>";
			currentTA = 0;
			if (slash)
				continue;
			browseError(MSG_TextareaNest);
		}

		if (!action)
			continue;	/* tag not recognized */

		topTag = t =
		    (struct htmlTag *)allocZeroMem(sizeof(struct htmlTag));
		tagList.push_back(t);
		tagCountCheck();
		t->seqno = tagno;
		sprintf(hnum, "%c%d", InternalCodeChar, tagno);
		t->info = ti;
		t->slash = slash;
		if (!slash)
			t->inner = end;
		t->ln = browseLine;
		t->action = action;	/* we might change this later */
		j = end - attrib;
		topAttrib = t->attrib = j ? pullString(attrib, j) : EMPTYSTRING;

		open = 0;
		if (ti->nest && slash) {
			t->balanced = eb_true;
			open = findOpenTag(ti->name);
			if (!open)
				continue;	/* unbalanced </ul> means nothing */
			open->balanced = eb_true;
			if (open->jv && isJSAlive)
				establish_innerHTML(open->jv, open->inner,
						    save_h, eb_false);
/* and mark everything in between */
			for (i2 = open->seqno; i2 < tagno; ++i2) {
				v = tagList[i2];
				if (v->info->nest)
					v->balanced = eb_true;
			}
		}

		if (ti->nest == 1 && !slash) {
			open = findOpenTag(0);
			if (open && open->info == ti)
				open->balanced = eb_true;
		}

		if (slash && ti->bits & TAG_NOSLASH)
			continue;	/* negated version means nothing */

/* just about any tag can have a name or id */
		if (!slash)
			htmlName();

/* Does this tag force the closure of an anchor? */
		if (currentA && (action != TAGACT_A || !slash)) {
			if (open && open->clickable)
				goto forceCloseAnchor;
			rc = htmlAttrPresent(topAttrib, "onclick");
			if (rc ||
			    ti->bits & TAG_CLOSEA && (a_text
						      || action <=
						      TAGACT_OPTION)) {
				browseError(MSG_InAnchor, ti->desc);
forceCloseAnchor:
				newstr += InternalCodeChar;
				newstr += "0}";
				currentA->balanced = eb_true;
				currentA = 0;
/* if/when the </a> comes along, it will be unbalanced, and we'll ignore it. */
			}
		}

		retainTag = eb_true;
		if (ti->bits & TAG_INVISIBLE)
			retainTag = eb_false;
		if (invisible)
			retainTag = eb_false;
		if (ti->bits & TAG_INVISIBLE) {
			invisible = !slash;
/* special case for noscript with no js */
			if (stringEqual(ti->name, "NOSCRIPT") && !cw->jss)
				invisible = eb_false;
		}

		strayClick = eb_false;

/* Are we gathering text to build title or option? */
		if (currentTitle || currentOpt) {
			char **ptr;
			v = (currentTitle ? currentTitle : currentOpt);
/* Should we print an error message? */
			if (v->action != action &&
			    !(v->action == TAGACT_OPTION
			      && action == TAGACT_SELECT)
			    || action != TAGACT_OPTION && !slash)
				browseError(MSG_HasTags, v->info->desc);
			if (!(ti->bits & TAG_CLOSEA))
				continue;
/* close off the title or option */
			v->balanced = eb_true;
			ptr = 0;
			if (currentTitle && !cw->ft)
				ptr = &cw->ft;
			if (currentOpt)
				ptr = &v->name;

			if (ptr) {
				string piece = newstr.substr(offset);
				char *piece1 = cloneString(piece.c_str());
				a = andTranslate(piece1, eb_true);
				stripWhite(a);
				if (currentOpt && strchr(a, ',')
				    && currentSel->multiple) {
					char *y;
					for (y = a; *y; ++y)
						if (*y == ',')
							*y = ' ';
					browseError(MSG_OptionComma);
				}
				spaceCrunch(a, eb_true, eb_false);
				*ptr = a;

				if (ptr == &cw->ft) {
					spaceCrunch(piece1, eb_true, eb_true);
					cw->fto = piece1;
				} else
					nzFree(piece1);

				if (currentTitle && isJSAlive)
					establish_property_string(cw->jss->jdoc,
								  "title", a,
								  eb_true);

				if (currentOpt)
					htmlOption(currentSel, currentOpt, a);
			}

			newstr.resize(offset);
			currentTitle = currentOpt = 0;
		}

/* Generate an html label for this tag if necessary. */
		htmlLabelID(newstr);

		switch (action) {
		case TAGACT_INPUT:
			htmlInput();
			if (t->itype == INP_HIDDEN)
				continue;
			if (!retainTag)
				continue;
			t->retain = eb_true;
			if (currentForm) {
				++currentForm->ninp;
				if (t->itype == INP_SUBMIT
				    || t->itype == INP_IMAGE)
					currentForm->submitted = eb_true;
			}
			strcat(hnum, "<");
			newstr += hnum;
			if (t->itype < INP_RADIO) {
				if (t->value[0])
					newstr += t->value;
				else if (t->itype == INP_SUBMIT
					 || t->itype == INP_IMAGE)
					newstr += "Go";
				else if (t->itype == INP_RESET)
					newstr += "Reset";
			} else
				newstr += (t->checked ? '+' : '-');
			if (currentForm
			    && (t->itype == INP_SUBMIT
				|| t->itype == INP_IMAGE)) {
				if (currentForm->secure)
					newstr += " secure";
				if (currentForm->bymail)
					newstr += " bymail";
			}
			newstr += InternalCodeChar;
			newstr += "0>";
			goto endtag;

		case TAGACT_TITLE:
			if (slash)
				continue;
			if (cw->ft)
				browseError(MSG_ManyTitles);
			offset = newstr.length();
			currentTitle = t;
			continue;

		case TAGACT_A:
			a_href = eb_false;
			if (slash) {
				if (open->href)
					a_href = eb_true;
				currentA = 0;
			} else {
				htmlHref("href");
				domLink("Anchor", "href", "anchors",
					cw->jss->jdoc, eb_false);
				get_js_events();
				if (t->href) {
					a_href = eb_true;
					topTag->clickable = eb_true;
				}
				a_text = eb_false;
			}
			if (a_href) {
				if (slash) {
					sprintf(hnum, "%c0}", InternalCodeChar);
				} else {
					strcat(hnum, "{");
					currentA = t;
				}
			} else {
				if (!t->name)
					retainTag = eb_false;	/* no need to keep this anchor */
			}	/* href or not */
			break;

		case TAGACT_PRE:
			premode = !slash;
			break;

		case TAGACT_TA:
			currentTA = t;
			offset = newstr.length();
			t->itype = INP_TA;
			formControl(eb_true);
			continue;

		case TAGACT_HTML:
			domLink("Html", 0, "htmls", cw->jss->jdoc, eb_false);
			goto endtag;

		case TAGACT_HEAD:
			domLink("Head", 0, "heads", cw->jss->jdoc, eb_false);
			goto plainWithElements;

		case TAGACT_BODY:
			domLink("Body", 0, "bodies", cw->jss->jdoc, eb_false);
plainWithElements:
			if (t->jv && !t->slash)
				establish_property_array(t->jv, "elements");
/* fall through */

		case TAGACT_JS:
plainTag:
/* check for javascript events, that's it */
			if (!slash)
				get_js_events();
/* no need to keep these tags in the output */
			continue;

		case TAGACT_META:
			htmlMeta();
			continue;

		case TAGACT_LI:
/* Look for the open UL or OL */
			j = -1;
			for (i1 = tagList.size() - 1; i1 >= 0; --i1) {
				v = tagList[i1];
				if (v->balanced || !v->info->nest)
					continue;
				if (v->slash)
					continue;	/* should never happen */
				s = v->info->name;
				if (stringEqual(s, "OL") ||
				    stringEqual(s, "UL") ||
				    stringEqual(s, "MENU")
				    || stringEqual(s, "DIR")) {
					j = 0;
					if (*s == 'O')
						j = ++v->lic;
				}
			}
			if (j < 0)
				browseError(MSG_NotInList, ti->desc);
			if (!retainTag)
				continue;
			hnum[0] = '\r';
			hnum[1] = 0;
			if (j == 0)
				strcat(hnum, "* ");
			if (j > 0)
				sprintf(hnum + 1, "%d. ", j);
			newstr += hnum;
			continue;

		case TAGACT_DT:
			for (i1 = tagList.size() - 1; i1 >= 0; --i1) {
				v = tagList[i1];
				if (v->balanced || !v->info->nest)
					continue;
				if (v->slash)
					continue;	/* should never happen */
				s = v->info->name;
				if (stringEqual(s, "DL"))
					break;
			}
			if (i1 < 0)
				browseError(MSG_NotInList, ti->desc);
			goto nop;

		case TAGACT_TABLE:
			if (!slash && isJSAlive) {
				domLink("Table", 0, "tables", cw->jss->jdoc,
					eb_false);
				get_js_events();
/* create the array of rows under the table */
				if (topTag->jv)
					establish_property_array(topTag->jv,
								 "rows");
			}
			if (slash)
				--intable;
			else
				++intable;
			goto nop;

		case TAGACT_TR:
			if (!intable) {
				browseError(MSG_NotInTable, ti->desc);
				continue;
			}
			if (slash)
				--inrow;
			else
				++inrow;
			tdfirst = eb_true;
			if ((!slash) && isJSAlive
			    && (open = findOpenTag("table")) && open->jv) {
				domLink("Trow", 0, "rows", open->jv, eb_false);
				get_js_events();
				if (topTag->jv)
					establish_property_array(topTag->jv,
								 "cells");
			}
			goto nop;

		case TAGACT_TD:
			if (!inrow) {
				browseError(MSG_NotInRow, ti->desc);
				continue;
			}
			if (slash)
				continue;
			if (tdfirst)
				tdfirst = eb_false;
			else if (retainTag) {
				l = newstr.length();
				while (l && newstr[l - 1] == ' ')
					--l;
				newstr.resize(l);
				newstr += '|';
			}
			if (isJSAlive && (open = findOpenTag("tr")) && open->jv) {
				domLink("Cell", 0, "cells", open->jv, eb_false);
				get_js_events();
			}
			goto endtag;

		case TAGACT_DIV:
			if (!slash && isJSAlive) {
				domLink("Div", 0, "divs", cw->jss->jdoc,
					eb_false);
				get_js_events();
			}
			goto nop;

		case TAGACT_SPAN:
			if (!slash) {
				domLink("Span", 0, "spans",
					cw->jss->jdoc, eb_false);
				get_js_events();
				a = htmlAttrVal(topAttrib, "class");
				if (!a)
					goto nop;
				caseShift(a, 'l');
				if (stringEqual(a, "sup"))
					action = TAGACT_SUP;
				if (stringEqual(a, "sub"))
					action = TAGACT_SUB;
				if (stringEqual(a, "ovb"))
					action = TAGACT_OVB;
				nzFree(a);
			} else if (open && open->subsup)
				action = open->subsup;
			if (action == TAGACT_SPAN)
				goto nop;
			t->subsup = action;
			goto subsup;

		case TAGACT_BR:
			if (lastact == TAGACT_TD)
				continue;

		case TAGACT_NOP:
nop:
			if (!retainTag)
				continue;
			j = ti->para;
			if (slash)
				j >>= 2;
			else
				j &= 3;
			if (!j)
				goto endtag;
			c = '\f';
			if (j == 1) {
				c = '\r';
				if (action == TAGACT_BR)
					c = '\n';
			}
			if (currentA)
				c = ' ';
			newstr += c;
			goto endtag;

		case TAGACT_FORM:
			htmlForm();
			if (currentSel) {
doneSelect:
				currentSel->action = TAGACT_INPUT;
				if (currentSel->controller)
					++currentSel->controller->ninp;
				currentSel->value = a =
				    displayOptions(currentSel);
				if (retainTag) {
					currentSel->retain = eb_true;
/* Crank out the input tag */
					sprintf(hnum, "%c%d<", InternalCodeChar,
						currentSel->seqno);
					newstr += hnum;
					newstr += a;
					newstr += InternalCodeChar;
					newstr += "0>";
				}
				currentSel = 0;
			}
			if (action == TAGACT_FORM && slash && currentForm) {
				if (retainTag && currentForm->href
				    && !currentForm->submitted) {
					makeButton();
					sprintf(hnum, " %c%d<Go",
						InternalCodeChar,
						tagList.size() - 1);
					newstr += hnum;
					if (currentForm->secure)
						newstr += " secure";
					if (currentForm->bymail)
						newstr += " bymail";
					newstr += " implicit";
					newstr += InternalCodeChar;
					newstr += "0>";
				}
				currentForm = 0;
			}
			continue;

		case TAGACT_SELECT:
			if (slash) {
				if (currentSel)
					goto doneSelect;
				continue;
			}
			currentSel = t;
			nopt = 0;
			t->itype = INP_SELECT;
			if (htmlAttrPresent(topAttrib, "readonly"))
				t->rdonly = eb_true;
			if (htmlAttrPresent(topAttrib, "multiple"))
				t->multiple = eb_true;
			formControl(eb_true);
			continue;

		case TAGACT_OPTION:
			if (slash)
				continue;
			if (!currentSel) {
				browseError(MSG_NotInSelect);
				continue;
			}
			currentOpt = t;
			offset = newstr.length();
			t->controller = currentSel;
			t->lic = nopt++;
			t->value = htmlAttrVal(topAttrib, "value");
			if (htmlAttrPresent(topAttrib, "selected")) {
				if (currentSel->lic && !currentSel->multiple)
					browseError(MSG_ManyOptSelected);
				else
					t->checked = t->rchecked =
					    eb_true, ++currentSel->lic;
			}
			continue;

		case TAGACT_HR:
			if (!retainTag)
				continue;
			newstr +=
			    "\r----------------------------------------\r";
			continue;

/* This is strictly for rendering math pages written with my particular css.
 * <span class=sup> becomes TAGACT_SUP, which means superscript.
* sub is subscript and ovb is overbar.
 * Sorry to put my little quirks into this program, but hey,
 * it's my program. */
		case TAGACT_SUP:
		case TAGACT_SUB:
		case TAGACT_OVB:
subsup:
			if (!retainTag)
				continue;
			t->retain = eb_true;
			if (action == TAGACT_SUB)
				j = 1;
			if (action == TAGACT_SUP)
				j = 2;
			if (action == TAGACT_OVB)
				j = 3;

			if (!slash) {	// open
				static const char *openstring[] = { 0,
					"[", "^(", "`"
				};
				t->lic = newstr.length();
				newstr += openstring[j];
				continue;
			}

			if (j == 3) {
				newstr += "'";
				continue;
			}

/* backup, and see if we can get rid of the parentheses or brackets */
			l = open->lic + j;
			s = newstr.substr(l).c_str();
			if (j == 2 && isalphaByte(s[0]) && !s[1])
				goto unparen;
			if (j == 2 && isalnumByte(newstr[l - 3])
			    && (stringEqual(s, "th") || stringEqual(s, "rd")
				|| stringEqual(s, "nd")
				|| stringEqual(s, "st"))) {
				newstr.erase(l - 2, 2);
				continue;
			}
			while (isdigitByte(*s))
				++s;
			if (!*s)
				goto unparen;
			newstr += (j == 2 ? ')' : ']');
			continue;

/* ok, we can trash the original ( or [ */
unparen:
			l = open->lic + j;
			newstr.erase(l - 1, 1);
			if (j == 2)
				newstr += ' ';
			continue;

		case TAGACT_AREA:
		case TAGACT_FRAME:
			if (action == TAGACT_FRAME) {
				htmlHref("src");
				domLink("Frame", "src", "frames",
					cw->jss->jwin, eb_false);
			} else {
				htmlHref("href");
				domLink("Area", "href", "areas",
					cw->jss->jdoc, eb_false);
			}
			topTag->clickable = eb_true;
			get_js_events();
			if (!retainTag)
				continue;
			newstr += (action == TAGACT_FRAME ? "\rFrame " : "\r");
			a = 0;
			if (action == TAGACT_AREA)
				a = htmlAttrVal(topAttrib, "alt");
			s = a;
			if (!s) {
				s = t->name;
				if (!s)
					s = altText(t->href);
			}
			if (!s)
				s = (action == TAGACT_FRAME ? "???" : "area");
			if (t->href) {
				strcat(hnum, "{");
				newstr += hnum;
				t->action = TAGACT_A;
				t->balanced = eb_true;
			}
			if (t->href || action == TAGACT_FRAME)
				newstr += s;
			nzFree(a);
			if (t->href) {
				newstr += InternalCodeChar;
				newstr += "0}";
			}
			newstr += '\r';
			continue;

		case TAGACT_MUSIC:
			if (!retainTag)
				continue;
			htmlHref("src");
			if (!t->href)
				continue;
			toPreamble(t->seqno,
				   (ti->name[0] !=
				    'B' ? "Audio passage" : "Background Music"),
				   0, 0);
			t->action = TAGACT_A;
			continue;

		case TAGACT_BASE:
			htmlHref("href");
			if (t->href) {
				basehref = t->href;
				debugPrint(3, "base href %s", basehref);
			}
			domLink("Base", "href", "bases", cw->jss->jdoc,
				eb_false);
			continue;

		case TAGACT_IMAGE:
			htmlImage();
			if (!currentA) {
/* I'm going to assume that if the web designer took the time
 * to put in an alt tag, then it's worth reading.
 * You can turn this feature off, but I don't think you'd want to. */
				if (a = htmlAttrVal(topAttrib, "alt")) {
					s = altText(a);
					nzFree(a);
					a = NULL;
					if (s) {
						newstr += '[';
						newstr += s;
						newstr += ']';
					}
				}
				continue;
			}
			if (!retainTag)
				continue;
			if (a_text)
				continue;
			s = 0;
			a = htmlAttrVal(topAttrib, "alt");
			if (a) {
				s = altText(a);
				nzFree(a);
			}
			if (!s)
				s = altText(t->name);
			if (!s)
				s = altText(currentA->href);
			if (!s)
				s = altText(t->href);
			if (!s)
				s = "image";
			newstr += s;
			a_text = eb_true;
			continue;

		case TAGACT_SCRIPT:
			if (slash)
				continue;
			rc = findEndScript(h, ti->name,
					   (ti->action == TAGACT_SCRIPT), &h,
					   &javatext, &js_nl);
			if (*h)
				h = strchr(h, '>') + 1;
			ln += js_nl;
/* I'm not going to process an open ended script. */
			if (!rc) {
				nzFree(javatext);
				runningError(MSG_ScriptNotClosed);
				continue;
			}

			htmlScript(html, h);
			scriptsPending();
			continue;

		case TAGACT_OBJ:
/* no clue what to do here */
			continue;

		case TAGACT_DW:
			if (slash) {
				if (!--dw_nest)
					browseLine = ln = dw_line;
			} else {
				if (!dw_nest++)
					dw_line = ln;
				browseLine = ln = 0;
			}
			continue;

		default:
			browseError(MSG_BadTag, action);
			continue;
		}		/* switch */

		if (!retainTag)
			continue;
		t->retain = eb_true;
		if (!strpbrk(hnum, "{}")) {
			strcat(hnum, "*");
/* Leave the meaningless tags out. */
			if (action == TAGACT_PRE || action == TAGACT_A && topTag->name) ;	/* ok */
			else
				hnum[0] = 0;
		}
		newstr += hnum;
endtag:
		lastact = action;
#if 0
		if (strayClick) {
			topTag->clickable = eb_true;
			a_text = eb_false;
			topTag->href = cloneString("#");
			currentA = topTag;
			sprintf(hnum, "%c%d{", InternalCodeChar, topTag->seqno);
			newstr += hnum;
		}
#endif
	}			/* loop over html string */

	if (currentA) {
		newstr += InternalCodeChar;
		newstr += "0}";
		currentA = 0;
	}

/* Run the various onload functions */
/* Turn the onunload functions into hyperlinks */
	if (isJSAlive && !onload_done) {
		int stoptag = tagList.size() - 1;
		onloadGo(cw->jss->jwin, 0, "window");
		onloadGo(cw->jss->jdoc, 0, "document");

		for (i1 = 0; i1 < stoptag; ++i1) {
			char *jsrc;
			t = tagList[i1];
			if (t->action == TAGACT_OPTION)
				continue;
			if (!t->jv)
				continue;
			if (t->slash)
				continue;
			jsrc = htmlAttrVal(t->attrib, "onunload");
			onloadGo(t->jv, jsrc, t->info->name);
			nzFree(jsrc);
		}		/* loop over tags */
	}
	onload_done = eb_true;

/* The onload function can, and often does, invoke document.write() */
	if (cw->dw) {
		nzFree(html);
		html = h = cw->dw;
		cw->dw = 0;
		cw->dw_l = 0;
		goto top;
	}

	if (browseLocal == 1) {	/* no errors yet */
		for (i1 = 0; i1 < tagList.size(); ++i1) {
			t = tagList[i1];
			browseLine = t->ln;
			if (t->info->nest && !t->slash && !t->balanced) {
				browseError(MSG_TagNotClosed, t->info->desc);
				break;
			}

/* Make sure the internal links are defined. */
			if (t->action != TAGACT_A)
				continue;	/* not anchor */
			h = t->href;
			if (!h)
				continue;
			if (h[0] != '#')
				continue;
			if (h[1] == 0)
				continue;
			a = h + 1;	/* this is what we're looking for */
			for (i2 = 0; i2 < tagList.size(); ++i2) {
				v = tagList[i2];
				if (v->action != TAGACT_A)
					continue;	/* not achor */
				if (!v->name)
					continue;	/* no name */
				if (stringEqual(a, v->name))
					break;
			}
			if (i2 == tagList.size()) {
				browseError(MSG_NoLable2, a);
				break;
			}
		}		/* loop over all tags */
	}

	/* clean up */
	browseLine = 0;
	nzFree(html);
	radioChecked.clear();
	basehref = 0;

	if (preamble.length()) {
		preamble += '\f';
		newstr.insert(0, preamble);
		preamble.clear();
	}

	return cloneString(newstr.c_str());
}				/* encodeTags */

void preFormatCheck(int tagno, eb_bool * pretag, eb_bool * slash)
{
	const struct htmlTag *t;
	if (!parsePage)
		i_printfExit(MSG_ErrCallPreFormat);
	*pretag = *slash = eb_false;
/* Don't think we really need the bounds check here. */
	if (tagno >= 0 && tagno < tagList.size()) {
		t = tagList[tagno];
		*pretag = (t->action == TAGACT_PRE);
		*slash = t->slash;
	}
}				/* preFormatCheck */

char *htmlParse(char *buf, int remote)
{
	char *newbuf;

	if (parsePage)
		i_printfExit(MSG_HtmlNotreentrant);
	parsePage = eb_true;
	if (remote >= 0)
		browseLocal = !remote;
	buf = encodeTags(buf);
	debugPrint(7, "%s", buf);

	newbuf = andTranslate(buf, eb_false);
	nzFree(buf);
	buf = newbuf;
	anchorSwap(buf);
	debugPrint(7, "%s", buf);

	newbuf = htmlReformat(buf);
	nzFree(buf);
	buf = newbuf;

	parsePage = eb_false;

/* In case one of the onload functions called document.write() */
	jsdw();

	set_property_string(cw->jss->jdoc, "readyState", "complete");

	return buf;
}				/* htmlParse */

void
findField(const char *line, int ftype, int n,
	  int *total, int *realtotal, int *tagno, char **href,
	  const struct htmlTag **tagp)
{
	const struct htmlTag *t;
	int nt = 0;		/* number of fields total */
	int nrt = 0;		/* the real total, for input fields */
	int nm = 0;		/* number match */
	int j;
	const char *s, *ss;
	char *h, *nmh;
	char c;
	static const char urlok[] =
	    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890,./?@#%&-_+=:~";

	if (href)
		*href = 0;
	if (tagp)
		*tagp = 0;

	if (cw->browseMode) {

		s = line;
		while ((c = *s) != '\n') {
			++s;
			if (c != InternalCodeChar)
				continue;
			j = strtol(s, (char **)&s, 10);
			if (!ftype) {
				if (*s != '{')
					continue;
				++nt, ++nrt;
				if (n == nt)
					nm = j;
				if (!n) {
					if (!nm)
						nm = j;
					else
						nm = -1;
				}
			} else {
				if (*s != '<')
					continue;
				if (n) {
					++nt, ++nrt;
					if (n == nt)
						nm = j;
					continue;
				}
				t = tagList[j];
				++nrt;
				if (ftype == 1 && t->itype <= INP_SUBMIT)
					continue;
				if (ftype == 2 && t->itype > INP_SUBMIT)
					continue;
				++nt;
				if (!nm)
					nm = j;
				else
					nm = -1;
			}
		}		/* loop over line */
	}

	if (nm < 0)
		nm = 0;
	if (total)
		*total = nrt;
	if (realtotal)
		*realtotal = nt;
	if (tagno)
		*tagno = nm;
	if (!ftype && nm) {
		t = tagList[nm];
		if (href)
			*href = cloneString(t->href);
		if (tagp)
			*tagp = t;
		if (href && isJSAlive && t->jv) {
/* defer to the java variable for the reference */
			char *jh = get_property_url(t->jv, eb_false);
			if (jh) {
				if (!*href || !stringEqual(*href, jh)) {
					nzFree(*href);
					*href = jh;
				}
			}
		}
	}

	if (nt || ftype)
		return;

/* Second time through, maybe the url is in plain text. */
	nmh = 0;
	s = line;
	while (eb_true) {
/* skip past weird characters */
		while ((c = *s) != '\n') {
			if (strchr(urlok, c))
				break;
			++s;
		}
		if (c == '\n')
			break;
		ss = s;
		while (strchr(urlok, *s))
			++s;
		h = pullString1(ss, s);
		unpercentURL(h);
		if (!isURL(h)) {
			free(h);
			continue;
		}
		++nt;
		if (n == nt) {
			nm = nt;
			nmh = h;
			continue;
		}
		if (n) {
			free(h);
			continue;
		}
		if (!nm) {
			nm = nt;
			nmh = h;
			continue;
		}
		free(h);
		nm = -1;
		free(nmh);
		nmh = 0;
	}			/* loop over line */

	if (nm < 0)
		nm = 0;
	if (total)
		*total = nt;
	if (realtotal)
		*realtotal = nt;
	if (href)
		*href = nmh;
	else
		nzFree(nmh);
}				/* findField */

void
findInputField(const char *line, int ftype, int n, int *total, int *realtotal,
	       int *tagno)
{
	findField(line, ftype, n, total, realtotal, tagno, 0, 0);
}				/* findInputField */

eb_bool lineHasTag(const char *p, const char *s)
{
	const struct htmlTag *t;
	char c;
	int j;
	while ((c = *p++) != '\n') {
		if (c != InternalCodeChar)
			continue;
		j = strtol(p, (char **)&p, 10);
		t = tagList[j];
		if (t->action != TAGACT_A)
			continue;
		if (!t->name)
			continue;
		if (stringEqual(t->name, s))
			return eb_true;
	}
	return eb_false;
}				/* lineHasTag */

/* See if there are simple tags like <p> or </font> */
eb_bool htmlTest(void)
{
	int j, ln;
	int cnt = 0;
	int fsize = 0;		/* file size */
	char look[12];
	eb_bool firstline = eb_true;

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
				return eb_true;
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
						return eb_true;
			}	/* leading tag */
		}		/* leading < */
		firstline = eb_false;

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
	int cnt;
	eb_bool show;

	s = inp_types[t->itype];
	if (*s == ' ')
		++s;
	printf("%s", s);
	if (t->multiple)
		i_printf(MSG_Many);
	if (t->itype >= INP_TEXT && t->itype <= INP_NUMBER && t->lic)
		printf("[%d]", t->lic);
	if (t->itype == INP_TA) {
		char *rows = htmlAttrVal(t->attrib, "rows");
		char *cols = htmlAttrVal(t->attrib, "cols");
		char *wrap = htmlAttrVal(t->attrib, "wrap");
		if (rows && cols) {
			printf("[%sx%s", rows, cols);
			if (wrap && stringEqualCI(wrap, "virtual"))
				i_printf(MSG_Recommended);
			i_printf(MSG_Close);
		}
		nzFree(rows);
		nzFree(cols);
		nzFree(wrap);
	}			/* text area */
	if (t->name)
		printf(" %s", t->name);
	nl();
	if (t->itype != INP_SELECT)
		return;

/* display the options in a pick list */
/* If a search string is given, display the options containing that string. */
	cnt = 0;
	show = eb_false;
	for (int i1 = 0; i1 < tagList.size(); ++i1) {
		v = tagList[i1];
		if (v->controller != t)
			continue;
		if (!v->name)
			continue;
		++cnt;
		if (*search && !strstrCI(v->name, search))
			continue;
		show = eb_true;
		printf("%3d %s\n", cnt, v->name);
	}
	if (!show) {
		if (!search)
			i_puts(MSG_NoOptions);
		else
			i_printf(MSG_NoOptionsMatch, search);
	}
}				/* infShow */

/* Update an input field. */
eb_bool infReplace(int tagno, const char *newtext, int notify)
{
	const struct htmlTag *t = tagList[tagno], *v;
	const struct htmlTag *form = t->controller;
	char *display;
	int itype = t->itype;
	int newlen = strlen(newtext);

/* sanity checks on the input */
	if (itype <= INP_SUBMIT) {
		int b = MSG_IsButton;
		if (itype == INP_SUBMIT || itype == INP_IMAGE)
			b = MSG_SubmitButton;
		if (itype == INP_RESET)
			b = MSG_ResetButton;
		setError(b);
		return eb_false;
	}

	if (itype == INP_TA) {
		setError(MSG_Textarea, t->lic);
		return eb_false;
	}

	if (t->rdonly) {
		setError(MSG_Readonly);
		return eb_false;
	}

	if (strchr(newtext, '\n')) {
		setError(MSG_InputNewline);
		return eb_false;
	}

	if (itype >= INP_TEXT && itype <= INP_NUMBER && t->lic
	    && newlen > t->lic) {
		setError(MSG_InputLong, t->lic);
		return eb_false;
	}

	if (itype >= INP_RADIO) {
		if (newtext[0] != '+' && newtext[0] != '-' || newtext[1]) {
			setError(MSG_InputRadio);
			return eb_false;
		}
		if (itype == INP_RADIO && newtext[0] == '-') {
			setError(MSG_ClearRadio);
			return eb_false;
		}
	}

/* Two lines, clear the "other" radio button, and set this one. */

	if (itype == INP_SELECT) {
		if (!locateOptions(t, newtext, 0, 0, eb_false))
			return eb_false;
		locateOptions(t, newtext, &display, 0, eb_false);
		updateFieldInBuffer(tagno, display, notify, eb_true);
		nzFree(display);
	}

	if (itype == INP_FILE) {
		if (!envFile(newtext, &newtext))
			return eb_false;
		if (newtext[0] && access(newtext, 4)) {
			setError(MSG_FileAccess, newtext);
			return eb_false;
		}
	}

	if (itype == INP_NUMBER) {
		if (*newtext && stringIsNum(newtext) < 0) {
			setError(MSG_NumberExpected);
			return eb_false;
		}
	}

	if (itype == INP_RADIO && form && t->name && *newtext == '+') {
/* clear the other radio button */
		for (int i1 = 0; i1 < tagList.size(); ++i1) {
			v = tagList[i1];
			if (v->controller != form)
				continue;
			if (v->itype != INP_RADIO)
				continue;
			if (!v->name)
				continue;
			if (!stringEqual(v->name, t->name))
				continue;
			if (fieldIsChecked(v->seqno) == eb_true)
				updateFieldInBuffer(v->seqno, "-", 0, eb_false);
		}
	}

	if (itype != INP_SELECT) {
		updateFieldInBuffer(tagno, newtext, notify, eb_true);
	}

	if (itype >= INP_RADIO && tagHandler(t->seqno, "onclick")) {
		if (!isJSAlive)
			runningError(MSG_NJNoOnclick);
		else {
			jSyncup();
			handlerGo(t->jv, "onclick");
			jsdw();
			if (js_redirects)
				return eb_true;
		}
	}

	if (itype >= INP_TEXT && itype <= INP_SELECT &&
	    tagHandler(t->seqno, "onchange")) {
		if (!isJSAlive)
			runningError(MSG_NJNoOnchange);
		else {
			jSyncup();
			handlerGo(t->jv, "onchange");
			jsdw();
			if (js_redirects)
				return eb_true;
		}
	}

	return eb_true;
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
	const char *w = t->value;
	eb_bool bval;

/* This is a kludge - option looks like INP_SELECT */
	if (t->action == TAGACT_OPTION)
		itype = INP_SELECT;

	if (itype <= INP_SUBMIT)
		return;

	if (itype >= INP_SELECT) {
		bval = t->rchecked;
		t->checked = bval;
		w = bval ? "+" : "-";
	}

	if (itype == INP_TA) {
		int cx = t->lic;
		if (cx)
			sideBuffer(cx, t->value, -1, 0, eb_false);
	} else if (itype != INP_HIDDEN && itype != INP_SELECT)
		updateFieldInBuffer(t->seqno, w, 0, eb_false);

	if (!t->jv)
		return;
	if (!isJSAlive)
		return;

	if (itype >= INP_RADIO) {
		set_property_bool(t->jv, "checked", bval);
	} else if (itype == INP_SELECT) {
		set_property_bool(t->jv, "selected", bval);
		if (bval && !t->controller->multiple && t->controller->jv)
			set_property_number(t->controller->jv,
					    "selectedIndex", t->lic);
	} else
		set_property_string(t->jv, "value", w);
}				/* resetVar */

static void formReset(const struct htmlTag *form)
{
	struct htmlTag *t, *sel = 0;
	int itype;

	for (int i1 = 0; i1 < tagList.size(); ++i1) {
		t = tagList[i1];
		if (t->action == TAGACT_OPTION) {
			if (!sel)
				continue;
			if (t->controller != sel)
				continue;
			resetVar(t);
			continue;
		}

		if (t->action != TAGACT_INPUT)
			continue;

		if (sel) {
			char *display = displayOptions(sel);
			updateFieldInBuffer(sel->seqno, display, 0, eb_false);
			nzFree(display);
			sel = 0;
		}

		if (t->controller != form)
			continue;
		itype = t->itype;
		if (itype != INP_SELECT) {
			resetVar(t);
			continue;
		}
		sel = t;
		if (t->jv && isJSAlive)
			set_property_number(t->jv, "selectedIndex", -1);
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

static eb_bool fetchBoolVar(const struct htmlTag *t)
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

static string post;
static const char *boundary;

static void postDelimiter(char fsep)
{
	char c = post[post.length() - 1];
	if (c == '?' || c == '\1')
		return;
	if (fsep == '-') {
		post += "--";
		post += boundary;
		post += '\r';
		fsep = '\n';
	}
	post += fsep;
}				/* postDelimiter */

static eb_bool
postNameVal(const char *name, const char *val, char fsep, uchar isfile)
{
	char *enc;
	const char *ct, *ce;	/* content type, content encoding */

	if (!name)
		name = EMPTYSTRING;
	if (!val)
		val = EMPTYSTRING;
	if (!*name && !*val)
		return eb_true;

	postDelimiter(fsep);
	switch (fsep) {
	case '&':
		enc = encodePostData(name);
		post += enc;
		post += '=';
		nzFree(enc);
		break;
	case '\n':
		post += name;
		post += "=\r\n";
		break;
	case '-':
		post += "Content-Disposition: form-data; name=\"";
		post += name;
		post += '"';
/* I'm leaving nl off, in case we need ; filename */
		break;
	}			/* switch */

	if (!*val && fsep == '&')
		return eb_true;

	switch (fsep) {
	case '&':
		enc = encodePostData(val);
		post += enc;
		nzFree(enc);
		break;
	case '\n':
		post += val;
		post += eol;
		break;
	case '-':
		if (isfile) {
			if (isfile & 2) {
				post += "; filename=\"";
				post += val;
				post += '"';
			}
			if (!encodeAttachment(val, 0, eb_true, &ct, &ce, &enc))
				return eb_false;
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
		post += "\r\nContent-Type: ";
		post += ct;
		post += "\r\nContent-Transfer-Encoding: ";
		post += ce;
		post += "\r\n\r\n";
		post += val;
		post += eol;
		if (isfile)
			nzFree(enc);
		break;
	}			/* switch */

	return eb_true;
}				/* postNameVal */

static eb_bool
formSubmit(const struct htmlTag *form, const struct htmlTag *submit)
{
	const struct htmlTag *t;
	int itype;
	int j;
	char *name, *dynamicvalue = NULL;
/* dynamicvalue needs to be freed with nzFree. */
	const char *value;
	char fsep = '&';	/* field separator */
	eb_bool noname = eb_false, rc;
	eb_bool bval;

	if (form->bymail)
		fsep = '\n';
	if (form->mime) {
		fsep = '-';
		boundary = makeBoundary();
		post += "`mfd~";
		post += boundary;
		post += eol;
	}

	for (int i1 = 0; i1 < tagList.size(); ++i1) {
		t = tagList[i1];
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
			postNameVal(nx, "0", fsep, eb_false);
			nx[namelen + 1] = 'y';
			postNameVal(nx, "0", fsep, eb_false);
			nzFree(nx);
			goto success;
		}

		if (itype >= INP_RADIO) {
			value = t->value;
			bval = fetchBoolVar(t);
			if (!bval)
				continue;
			if (!name)
				noname = eb_true;
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
			postNameVal(name, dynamicvalue, fsep, eb_false);
			nzFree(dynamicvalue);
			dynamicvalue = NULL;
			continue;
		}

		if (itype == INP_TA) {
			int cx = t->lic;
			char *cxbuf;
			int cxlen;
			if (!name)
				noname = eb_true;
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
				if (!unfoldBuffer(cx, eb_true, &cxbuf, &cxlen))
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
				rc = postNameVal(name, cxbuf, fsep, eb_false);
				nzFree(cxbuf);
				if (rc)
					continue;
				goto fail;
			}

			postNameVal(name, 0, fsep, eb_false);
			continue;
		}

		if (itype == INP_SELECT) {
			char *display = getFieldFromBuffer(t->seqno);
			char *s, *e;
			if (!display) {	/* off the air */
				struct htmlTag *v;
/* revert back to reset state */
				for (int i2 = 0; i2 < tagList.size(); ++i2) {
					v = tagList[i2];
					if (v->controller == t)
						v->checked = v->rchecked;
				}
				display = displayOptions(t);
			}
			rc = locateOptions(t, display, 0, &dynamicvalue,
					   eb_false);
			nzFree(display);
			if (!rc)
				goto fail;	/* this should never happen */
/* option could have an empty value, usually the null choice,
 * before you have made a selection. */
			if (!*dynamicvalue) {
				postNameVal(name, dynamicvalue, fsep, eb_false);
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
				postNameVal(name, s, fsep, eb_false);
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
		postNameVal(name, value, fsep, eb_false);
	}			/* loop over tags */

	if (form->mime) {	/* the last boundary */
		post += "--";
		post += boundary;
		post += "--\r\n";
	}

	i_puts(MSG_FormSubmit);
	return eb_true;

fail:
	return eb_false;
}				/* formSubmit */

/*********************************************************************
Push the reset or submit button.
This routine must be reentrant.
You push submit, which calls this routine, which runs the onsubmit code,
which checks the fields and calls form.submit(),
which calls this routine.  Happens all the time.
*********************************************************************/

eb_bool infPush(int tagno, char **post_string)
{
	struct htmlTag *t = tagList[tagno];
	struct htmlTag *form;
	int itype;
	int actlen;
	const char *action = 0;
	const char *sec1;
	const char *sec2;
	const char *post_c;
	const char *prot;
	eb_bool rc;

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
		return eb_false;
	}

	if (!form && itype != INP_BUTTON) {
		setError(MSG_NotInForm);
		return eb_false;
	}

	if (t && tagHandler(t->seqno, "onclick")) {
		if (!isJSAlive)
			runningError(itype ==
				     INP_BUTTON ? MSG_NJNoAction :
				     MSG_NJNoOnclick);
		else {
			rc = eb_true;
			if (t->jv)
				rc = handlerGo(t->jv, "onclick");
			jsdw();
			if (!rc)
				return eb_true;
			if (js_redirects)
				return eb_true;
		}
	}

	if (itype == INP_BUTTON) {
		if (isJSAlive && t->jv && !handlerPresent(t->jv, "onclick")) {
			setError(MSG_ButtonNoJS);
			return eb_false;
		}
		return eb_true;
	}

	if (itype == INP_RESET) {
/* Before we reset, run the onreset code */
		if (t && tagHandler(form->seqno, "onreset")) {
			if (!isJSAlive)
				runningError(MSG_NJNoReset);
			else {
				rc = eb_true;
				if (form->jv)
					rc = handlerGo(form->jv, "onreset");
				jsdw();
				if (!rc)
					return eb_true;
				if (js_redirects)
					return eb_true;
			}
		}		/* onreset */
		formReset(form);
		return eb_true;
	}

	/* Before we submit, run the onsubmit code */
	if (t && tagHandler(form->seqno, "onsubmit")) {
		if (!isJSAlive)
			runningError(MSG_NJNoSubmit);
		else {
			rc = eb_true;
			if (form->jv)
				rc = handlerGo(form->jv, "onsubmit");
			jsdw();
			if (!rc)
				return eb_true;
			if (js_redirects)
				return eb_true;
		}
	}

	action = form->href;
/* But we defer to the java variable */
	if (form->jv && isJSAlive) {
		char *jh = get_property_url(form->jv, eb_true);
		if (jh && (!action || !stringEqual(jh, action))) {
/* Tie action to the form tag, to plug a small memory leak */
			nzFree(form->href);
			form->href = resolveURL(getBaseHref(form->seqno), jh);
			action = form->href;
		}
		nzFree(jh);
	}

/* if no action, or action is "#", the default is the current location */
	if (!action || stringEqual(action, "#")) {
		action = getBaseHref(form->seqno);
	}

	if (!action) {
		setError(MSG_FormNoURL);
		return eb_false;
	}

	debugPrint(2, "* %s", action);

	prot = getProtURL(action);
	if (!prot) {
		setError(MSG_FormBadURL);
		return eb_false;
	}

	if (stringEqualCI(prot, "javascript")) {
		if (!isJSAlive) {
			setError(MSG_NJNoForm);
			return eb_false;
		}
		javaParseExecute(form->jv, action, 0, 0);
		jsdw();
		return eb_true;
	}

	form->bymail = eb_false;
	if (stringEqualCI(prot, "mailto")) {
		if (!validAccount(localAccount))
			return eb_false;
		form->bymail = eb_true;
	} else if (stringEqualCI(prot, "http")) {
		if (form->secure) {
			setError(MSG_BecameInsecure);
			return eb_false;
		}
	} else if (!stringEqualCI(prot, "https")) {
		setError(MSG_SubmitProtBad, prot);
		return eb_false;
	}

	post = action;
	sec1 = strchr(action, '#');
	if (sec1) {
		i_printf(MSG_SectionIgnored, sec1);
		post.resize(sec1 - action);
	}
	sec2 = strpbrk(action, "?\1");
	if (sec2 > sec1)
		sec2 = 0;
	if (sec2 && (*sec2 == '\1' || !(form->bymail | form->post))) {
		debugPrint(3,
			   "the url already specifies some data, which will be overwritten by the data in this form");
		post.resize(sec2 - action);
	}

	post += (form->post ? '\1' : '?');
	actlen = post.length();

	if (!formSubmit(form, t)) {
		post.clear();
		return eb_false;
	}

	post_c = post.c_str();
	debugPrint(3, "%s %s", form->post ? "post" : "get", post_c + actlen);

/* Handle the mail method here and now. */
	if (form->bymail) {
		char *addr, *subj, *q;
		const char *tolist[2], *atlist[2];
		const char *name = form->name;
		int newlen = post.length() - actlen;	/* the new string could be longer than post */
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
		strcpy(q + strlen(q), post_c + actlen);
		post.clear();
		i_printf(MSG_MailSending, addr);
		sleep(1);
		rc = sendMail(localAccount, tolist, q, -1, atlist, 0, 0,
			      eb_false);
		if (rc)
			i_puts(MSG_MailSent);
		nzFree(addr);
		nzFree(subj);
		nzFree(q);
		*post_string = 0;
		return rc;
	}

	*post_string = cloneString(post_c);
	post.clear();
	return eb_true;
}				/* infPush */

/* I don't have any reverse pointers, so I'm just going to scan the list */
static struct htmlTag *tagFromJavaVar(JS::HandleObject v)
{
	struct htmlTag *t = 0;
	if (!cw->tags)
		i_printfExit(MSG_NullListInform);
	for (int i1 = 0; i1 < tagList.size(); ++i1) {
		t = tagList[i1];
		if (t->jv == v)
			break;
	}
	if (!t)
		runningError(MSG_LostTag);
	return t;
}				/* tagFromJavaVar */

/* Javascript has changed an input field */
void javaSetsTagVar(JS::HandleObject v, const char *val)
{
	struct htmlTag *t = tagFromJavaVar(v);
	if (!t)
		return;
/* ok, we found it */
	if (t->itype == INP_HIDDEN || t->itype == INP_RADIO)
		return;
	if (t->itype == INP_TA) {
		runningError(MSG_JSTextarea);
		return;
	}
	updateFieldInBuffer(t->seqno, val, parsePage ? 0 : 2, eb_false);
}				/* javaSetsTagVar */

/* Return false to stop javascript, due to a url redirect */
void javaSubmitsForm(JS::HandleObject v, eb_bool reset)
{
	char *post;
	eb_bool rc;
	struct htmlTag *t = tagFromJavaVar(v);
	if (!t)
		return;

	if (reset) {
		formReset(t);
		return;
	}

	rc = infPush(t->seqno, &post);
	if (!rc) {
		showError();
		return;
	}
	gotoLocation(post, 0, eb_false);
}				/* javaSubmitsForm */

void javaOpensWindow(const char *href, const char *name)
{
	struct htmlTag *t;
	char *copy, *r;
	const char *a;

	if (!href || !*href) {
		browseError(MSG_JSBlankWindow);
		return;
	}

	copy = cloneString(href);
	unpercentURL(copy);
	r = resolveURL(getBaseHref(-1), copy);
	nzFree(copy);
	if (!parsePage) {
		gotoLocation(r, 0, eb_false);
		return;
	}

	t = newTag("a");
	t->href = r;
	a = altText(r);
/* I'll assume this is more helpful than the name of the window */
	if (a)
		name = a;
	toPreamble(t->seqno, "Popup", 0, name);
}				/* javaOpensWindow */

void
javaSetsTimeout(int n, const char *jsrc, JS::HandleObject to,
		eb_bool isInterval)
{
	struct htmlTag *t = newTag("a");
	char timedesc[48];
	int l;

	strcpy(timedesc, (isInterval ? "Interval" : "Timer"));
	l = strlen(timedesc);
	if (n > 1000)
		sprintf(timedesc + l, " %d", n / 1000);
	else
		sprintf(timedesc + l, " %dms", n);

	t->jv = to;
	t->href = cloneString("#");
	toPreamble(t->seqno, timedesc, jsrc, 0);
}				/* javaSetsTimeout */

eb_bool handlerGoBrowse(const struct htmlTag *t, const char *name)
{
	if (!isJSAlive)
		return eb_true;
	if (!t->jv)
		return eb_true;
	return handlerGo(t->jv, name);
}				/* handlerGoBrowse */
