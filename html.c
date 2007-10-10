/* html.c
 * Parse html tags.
 * Copyright (c) Karl Dahlke, 2006
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"

/* Close an open anchor when you see this tag. */
#define TAG_CLOSEA 1
/* You won't see the text between <foo> and </fooo> */
#define TAG_INVISIBLE 2
/* sometimes </foo> means nothing. */
#define TAG_NOSLASH 4

enum {
    INP_RESET, INP_BUTTON, INP_IMAGE, INP_SUBMIT,
    INP_HIDDEN,
    INP_TEXT, INP_PW, INP_NUMBER, INP_FILE,
    INP_SELECT, INP_TA, INP_RADIO, INP_CHECKBOX,
};

enum {
    TAGACT_ZERO, TAGACT_A, TAGACT_INPUT, TAGACT_TITLE, TAGACT_TA,
    TAGACT_BUTTON, TAGACT_SELECT, TAGACT_OPTION,
    TAGACT_NOP, TAGACT_JS, TAGACT_H, TAGACT_SUB, TAGACT_SUP,
    TAGACT_DW, TAGACT_BODY, TAGACT_HEAD,
    TAGACT_MUSIC, TAGACT_IMAGE, TAGACT_BR, TAGACT_IBR, TAGACT_P,
    TAGACT_BASE, TAGACT_META, TAGACT_PRE,
    TAGACT_DT, TAGACT_LI, TAGACT_HR, TAGACT_TABLE, TAGACT_TR, TAGACT_TD,
    TAGACT_DIV, TAGACT_SPAN,
    TAGACT_FORM, TAGACT_FRAME,
    TAGACT_MAP, TAGACT_AREA, TAGACT_SCRIPT, TAGACT_EMBED, TAGACT_OBJ,
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

static struct listHead htmlStack;
static struct htmlTag **tagArray, *topTag;
static int ntags;		/* number of tags in this page */
static char *topAttrib;
static char *basehref;
static struct htmlTag *currentForm;	/* the open form */
bool parsePage;			/* parsing html */
int browseLine;			/* for error reporting */
static char *radioChecked;
static int radioChecked_l;
static char *preamble;
static int preamble_l;

static void htmlName(void);

/* Switch from the linked list of tags to an array. */
static void
buildTagArray(void)
{
    int j = 0;
    struct htmlTag *t;
    if(!parsePage)
	return;
    if(tagArray)
	nzFree(tagArray);
    tagArray = allocMem(sizeof (struct htmlTag *) * ((ntags + 32) & ~31));
    cw->tags = tagArray;
    foreach(t, htmlStack)
       tagArray[j++] = t;
    tagArray[j] = 0;
}				/* buildTagArray */

static bool
htmlAttrPresent(const char *e, const char *name)
{
    char *a;
    if(!(a = htmlAttrVal(e, name)))
	return false;
    nzFree(a);
    return true;
}				/* htmlAttrPresent */

static char *
hrefVal(const char *e, const char *name)
{
    char *a;
    htmlAttrVal_nl = true;
    a = htmlAttrVal(e, name);
    htmlAttrVal_nl = false;	/* put it back */
    return a;
}				/* hrefVal */

static char *
targetVal(const char *e)
{
    return htmlAttrVal(e, "target");
}				/* targetVal */

static void
toPreamble(int tagno, const char *msg, const char *j, const char *h)
{
    char buf[120];
    sprintf(buf, "\r\200%d{%s", tagno, msg);
    if(h) {
	strcat(buf, ": ");
	strcat(buf, h);
    } else if(j) {
	char fn[40];
	char *s;
	skipWhite(&j);
	if(memEqualCI(j, "javascript:", 11))
	    j += 11;
	skipWhite(&j);
	if(isalphaByte(*j) || *j == '_') {
	    fn[0] = ':';
	    fn[1] = ' ';
	    for(s = fn + 2; isalnumByte(*j) || *j == '_'; ++j) {
		if(s < fn + sizeof (fn) - 3)
		    *s++ = *j;
	    }
	    strcpy(s, "()");
	    skipWhite(&j);
	    if(*j == '(')
		strcat(buf, fn);
	}
    }
    strcat(buf, "\2000}\r");
    if(!preamble)
	preamble = initString(&preamble_l);
    stringAndString(&preamble, &preamble_l, buf);
}				/* toPreamble */

struct tagInfo {
    const char *name;
    const char *desc;
    int action;
    uchar nest;			/* must nest, like parentheses */
    uchar para;			/* paragraph and line breaks */
    ushort bits;		/* a bunch of boolean attributes */
};

static const struct tagInfo elements[] = {
    {"BASE", "base reference for relative URLs", TAGACT_BASE, 0, 0, 5},
    {"A", "an anchor", TAGACT_A, 1, 0, 1},
    {"INPUT", "an input item", TAGACT_INPUT, 0, 0, 5},
    {"TITLE", "the title", TAGACT_TITLE, 1, 0, 1},
    {"TEXTAREA", "an input text area", TAGACT_TA, 1, 0, 1},
    {"SELECT", "an option list", TAGACT_SELECT, 1, 0, 1},
    {"OPTION", "a select option", TAGACT_OPTION, 0, 0, 5},
    {"SUB", "a subscript", TAGACT_SUB, 3, 0, 0},
    {"SUP", "a superscript", TAGACT_SUP, 3, 0, 0},
    {"FONT", "a font", TAGACT_NOP, 3, 0, 0},
    {"CENTER", "centered text", TAGACT_NOP, 3, 0, 0},
    {"DOCWRITE", "document.write() text", TAGACT_DW, 0, 0, 0},
    {"CAPTION", "a caption", TAGACT_NOP, 1, 5, 0},
    {"HEAD", "the html header information", TAGACT_HEAD, 1, 0, 5},
    {"BODY", "the html body", TAGACT_BODY, 1, 0, 5},
    {"BGSOUND", "background music", TAGACT_MUSIC, 0, 0, 5},
    {"AUDIO", "audio passage", TAGACT_MUSIC, 0, 0, 5},
    {"META", "a meta tag", TAGACT_META, 0, 0, 4},
    {"IMG", "an image", TAGACT_IMAGE, 0, 0, 4},
    {"IMAGE", "an image", TAGACT_IMAGE, 0, 0, 4},
    {"BR", "a line break", TAGACT_BR, 0, 1, 4},
    {"P", "a paragraph", TAGACT_NOP, 0, 2, 5},
    {"DIV", "a divided section", TAGACT_DIV, 1, 5, 0},
    {"HTML", "html", TAGACT_NOP, 0, 0, 0},
    {"BLOCKQUOTE", "a quoted paragraph", TAGACT_NOP, 1, 10, 1},
    {"H1", "a level 1 header", TAGACT_NOP, 1, 10, 1},
    {"H2", "a level 2 header", TAGACT_NOP, 1, 10, 1},
    {"H3", "a level 3 header", TAGACT_NOP, 1, 10, 1},
    {"H4", "a level 4 header", TAGACT_NOP, 1, 10, 1},
    {"H5", "a level 5 header", TAGACT_NOP, 1, 10, 1},
    {"H6", "a level 6 header", TAGACT_NOP, 1, 10, 1},
    {"DT", "a term", TAGACT_DT, 0, 2, 5},
    {"DD", "a definition", TAGACT_DT, 0, 1, 5},
    {"LI", "a list item", TAGACT_LI, 0, 1, 5},
    {"UL", "a bullet list", TAGACT_NOP, 3, 5, 1},
    {"DIR", "a directory list", TAGACT_NOP, 3, 5, 1},
    {"MENU", "a menu", TAGACT_NOP, 3, 5, 1},
    {"OL", "a numbered list", TAGACT_NOP, 3, 5, 1},
    {"DL", "a definition list", TAGACT_NOP, 3, 5, 1},
    {"HR", "a horizontal line", TAGACT_HR, 0, 5, 5},
    {"FORM", "a form", TAGACT_FORM, 1, 0, 1},
    {"BUTTON", "a button", TAGACT_BUTTON, 0, 0, 5},
    {"FRAME", "a frame", TAGACT_FRAME, 0, 2, 5},
    {"IFRAME", "a frame", TAGACT_FRAME, 0, 2, 5},
    {"MAP", "an image map", TAGACT_MAP, 0, 2, 5},
    {"AREA", "an image map area", TAGACT_AREA, 0, 0, 1},
    {"TABLE", "a table", TAGACT_TABLE, 3, 10, 1},
    {"TR", "a table row", TAGACT_TR, 3, 5, 1},
    {"TD", "a table entry", TAGACT_TD, 3, 0, 1},
    {"TH", "a table heading", TAGACT_TD, 1, 0, 1},
    {"PRE", "a preformatted section", TAGACT_PRE, 1, 1, 0},
    {"LISTING", "a listing", TAGACT_PRE, 1, 1, 0},
    {"XMP", "an example", TAGACT_PRE, 1, 1, 0},
    {"FIXED", "a fixed presentation", TAGACT_NOP, 1, 1, 0},
    {"CODE", "a block of code", TAGACT_NOP, 1, 0, 0},
    {"SAMP", "a block of sample text", TAGACT_NOP, 1, 0, 0},
    {"ADDRESS", "an address block", TAGACT_NOP, 1, 1, 0},
    {"STYLE", "a style block", TAGACT_NOP, 1, 0, 2},
    {"SCRIPT", "a script", TAGACT_SCRIPT, 0, 0, 1},
    {"NOSCRIPT", "no script section", TAGACT_NOP, 1, 0, 3},
    {"NOFRAMES", "no frames section", TAGACT_NOP, 1, 0, 3},
    {"EMBED", "embedded html", TAGACT_MUSIC, 1, 0, 3},
    {"NOEMBED", "no embed section", TAGACT_NOP, 1, 0, 3},
    {"OBJECT", "an html object", TAGACT_OBJ, 0, 0, 3},
    {"EM", "emphasized text", TAGACT_JS, 1, 0, 0},
    {"LABEL", "a label", TAGACT_JS, 1, 0, 0},
    {"STRIKE", "emphasized text", TAGACT_JS, 1, 0, 0},
    {"S", "emphasized text", TAGACT_JS, 1, 0, 0},
    {"STRONG", "emphasized text", TAGACT_JS, 1, 0, 0},
    {"B", "bold text", TAGACT_JS, 1, 0, 0},
    {"I", "italicized text", TAGACT_JS, 1, 0, 0},
    {"U", "underlined text", TAGACT_JS, 1, 0, 0},
    {"DFN", "definition text", TAGACT_JS, 1, 0, 0},
    {"Q", "quoted text", TAGACT_JS, 1, 0, 0},
    {"ABBR", "an abbreviation", TAGACT_JS, 1, 0, 0},
    {"SPAN", "an html span", TAGACT_SPAN, 1, 0, 0},
    {"FRAMESET", "a frame set", TAGACT_JS, 3, 0, 1},
    {NULL, NULL, 0}
};

struct htmlTag {
    struct htmlTag *next, *prev;
    void *jv;			/* corresponding java variable */
    int seqno;
    int ln;			/* line number */
    int lic;			/* list item count, highly overloaded */
    int action;
    const struct tagInfo *info;
/* the form that owns this input tag, etc */
    struct htmlTag *controller;
    bool slash:1;		/* as in </A> */
    bool balanced:1;		/* <foo> and </foo> */
    bool retain:1;
    bool multiple:1;
    bool rdonly:1;
    bool secure:1;
    bool checked:1;
    bool rchecked:1;		/* for reset */
    bool post:1;		/* post, rather than get */
    bool javapost:1;		/* post by calling javascript */
    bool mime:1;		/* encode as mime, rather than url encode */
    bool bymail:1;		/* send by mail, rather than http */
    bool submitted:1;
    bool handler:1;
    uchar itype;		/* input type = */
    short ninp;			/* number of nonhidden inputs */
    char *attrib;
    char *name, *id, *value, *href;
    const char *inner;		/* for inner html */
};

static const char *const handlers[] = {
    "onmousemove", "onmouseover", "onmouseout", "onmouseup", "onmousedown",
    "onclick", "ondblclick", "onblur", "onfocus",
    "onchange", "onsubmit", "onreset",
    "onload", "onunload",
    "onkeypress", "onkeyup", "onkeydown",
    0
};

static void
freeTag(struct htmlTag *e)
{
    nzFree(e->attrib);
    nzFree(e->name);
    nzFree(e->id);
    nzFree(e->value);
    nzFree(e->href);
    free(e);
}				/* freeTag */

void
freeTags(void *a)
{
    int n;
    struct ebWindow *w;
    struct htmlTag *t, **e;

    if(!(e = a))
	return;			/* no tags */

/* drop empty textarea buffers created by this session */
    for(e = a; t = *e; ++e) {
	if(t->action != TAGACT_INPUT)
	    continue;
	if(t->itype != INP_TA)
	    continue;
	if(!(n = t->lic))
	    continue;
	if(!(w = sessionList[n].lw))
	    continue;
	if(w->fileName)
	    continue;
	if(w->dol)
	    continue;
	if(w != sessionList[n].fw)
	    continue;
/* We could have added a line, then deleted it */
	w->changeMode = false;
	cxQuit(n, 2);
    }				/* loop over tags */

    for(e = a; t = *e; ++e)
	freeTag(t);
    free(a);
}				/* freeTags */

static void
get_js_event(const char *name)
{
    void *ev = topTag->jv;
    char *s;

    int action = topTag->action;
    int itype = topTag->itype;
    if(!(s = htmlAttrVal(topAttrib, name)))
	return;			/* not there */
    if(ev)
	handlerSet(ev, name, s);
    nzFree(s);

/* This code is only here to print warnings if js is disabled
 * or otherwise broken.  Record the fact that handlers exist,
 * outside the context of js. */
    if(stringEqual(name, "onclick")) {
	if(action == TAGACT_A || action == TAGACT_AREA || action == TAGACT_FRAME
	   || action == TAGACT_INPUT && (itype >= INP_RADIO ||
	   itype <= INP_SUBMIT) || action == TAGACT_OPTION) {
	    topTag->handler = true;
	    if(currentForm && action == TAGACT_INPUT && itype == INP_BUTTON)
		currentForm->submitted = true;
	}
    }
    if(stringEqual(name, "onsubmit") || stringEqual(name, "onreset")) {
	if(action == TAGACT_FORM)
	    topTag->handler = true;
    }
    if(stringEqual(name, "onchange")) {
	if(action == TAGACT_INPUT || action == TAGACT_SELECT) {
	    if(itype == INP_TA)
		runningError(MSG_OnchangeText);
	    else if(itype > INP_HIDDEN && itype <= INP_SELECT) {
		topTag->handler = true;
		if(currentForm)
		    currentForm->submitted = true;
	    }
	}
    }
}				/* get_js_event */

static void
get_js_events(void)
{
    void *ev = topTag->jv;
    int j;
    const char *t;
    int action = topTag->action;
    int itype = topTag->itype;

    for(j = 0; t = handlers[j]; ++j)
	get_js_event(t);

    if(!ev)
	return;

/* Some warnings about some handlers that we just don't "handle" */
    if(handlerPresent(ev, "onkeypress") ||
       handlerPresent(ev, "onkeyup") || handlerPresent(ev, "onkeydown"))
	browseError(MSG_JSKeystroke);
    if(handlerPresent(ev, "onfocus") || handlerPresent(ev, "onblur"))
	browseError(MSG_JSFocus);
    if(handlerPresent(ev, "ondblclick"))
	runningError(MSG_Doubleclick);
    if(handlerPresent(ev, "onclick")) {
	if((action == TAGACT_A || action == TAGACT_AREA || action == TAGACT_FRAME) && topTag->href || action == TAGACT_INPUT && (itype <= INP_SUBMIT || itype >= INP_RADIO)) ;	/* ok */
	else
	    browseError(MSG_StrayOnclick);
    }
    if(handlerPresent(ev, "onchange")) {
	if(action != TAGACT_INPUT && action != TAGACT_SELECT || itype == INP_TA)
	    browseError(MSG_Onchange);
    }
/* Other warnings might be appropriate, but I'm going to assume this
 * is valid javascript, and you won't put an onsubmit function on <P> etc */
}				/* get_js_events */

bool
tagHandler(int seqno, const char *name)
{
    const struct htmlTag **list = cw->tags;
    const struct htmlTag *t = list[seqno];
    return t->handler | handlerPresent(t->jv, name);
}				/* tagHandler */

static char *
getBaseHref(int n)
{
    const struct htmlTag *t, **list;
    if(parsePage)
	return basehref;
    list = cw->tags;
    if(n < 0) {
	for(n = 0; list[n]; ++n) ;
    }
    list += n;
    do
	--list;
    while((t = *list)->action != TAGACT_BASE);
    return t->href;
}				/* getBaseHref */

static void
htmlMeta(void)
{
    char *name, *content, *heq;
    char **ptr;
    void *e;

    htmlName();
    topTag->jv = e =
       domLink("Meta", topTag->name, topTag->id, 0, 0, "metas", jdoc, false);
    name = topTag->name;
    content = htmlAttrVal(topAttrib, "content");
    if(content == EMPTYSTRING)
	content = 0;
    if(e)
	establish_property_string(e, "content", content, true);
    heq = htmlAttrVal(topAttrib, "http-equiv");
    if(heq == EMPTYSTRING)
	heq = 0;

    if(heq && content) {
	bool rc;
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
	if(stringEqualCI(heq, "Set-Cookie")) {
	    rc = receiveCookie(cw->fileName, content);
	    debugPrint(3, rc ? "jar" : "rejected");
	}

	if(allowRedirection && !browseLocal && stringEqualCI(heq, "Refresh")) {
	    if(parseRefresh(content, &delay)) {
		char *newcontent;
		unpercentURL(content);
		newcontent = resolveURL(basehref, content);
		gotoLocation(newcontent, delay, true);
	    }
	}
    }

    if(name) {
	ptr = 0;
	if(stringEqualCI(name, "description"))
	    ptr = &cw->fd;
	if(stringEqualCI(name, "keywords"))
	    ptr = &cw->fk;
	if(ptr && !*ptr && content) {
	    stripWhite(content);
	    *ptr = content;
	    content = 0;
	}
    }
    /* name */
    nzFree(content);
    nzFree(heq);
}				/* htmlMeta */

static void
htmlName(void)
{
    char *name = htmlAttrVal(topAttrib, "name");
    char *id = htmlAttrVal(topAttrib, "id");
    if(name == EMPTYSTRING)
	name = 0;
    topTag->name = name;
    if(id == EMPTYSTRING)
	id = 0;
    topTag->id = id;
}				/* htmlName */

static void
htmlHref(const char *desc)
{
    char *h = hrefVal(topAttrib, desc);
    if(h == EMPTYSTRING) {
	h = 0;
	if(topTag->action == TAGACT_A)
	    h = cloneString("#");
    }
    if(h) {
	unpercentURL(h);
	topTag->href = resolveURL(basehref, h);
	free(h);
    }
}				/* htmlHref */

static void
formControl(bool namecheck)
{
    void *fo = 0;		/* form object */
    void *e;			/* the new element */
    const char *typedesc;
    int itype = topTag->itype;
    int isradio = itype == INP_RADIO;
    int isselect = (itype == INP_SELECT) * 2;

    if(currentForm) {
	topTag->controller = currentForm;
	fo = currentForm->jv;
    } else if(itype != INP_BUTTON)
	browseError(MSG_NotInForm2, topTag->info->desc);

    htmlName();
    if(namecheck && !topTag->name)
	browseError(MSG_FieldNoName, topTag->info->desc);

    if(fo) {
	topTag->jv = e =
	   domLink("Element", topTag->name, topTag->id, 0, 0,
	   "elements", fo, isradio | isselect);
    } else {
	topTag->jv = e =
	   domLink("Element", topTag->name, topTag->id, 0, 0, 0,
	   jdoc, isradio | isselect);
    }
    get_js_events();
    if(!e)
	return;

    if(itype <= INP_RADIO) {
	establish_property_string(e, "value", topTag->value, false);
	if(itype != INP_FILE) {
/* No default value on file, for security reasons */
	    establish_property_string(e, dfvl, topTag->value, true);
	}			/* not file */
    }

    if(isselect)
	typedesc = topTag->multiple ? "select-multiple" : "select-one";
    else
	typedesc = inp_types[itype];
    establish_property_string(e, "type", typedesc, true);

    if(itype >= INP_RADIO) {
	establish_property_bool(e, "checked", topTag->checked, false);
	establish_property_bool(e, dfck, topTag->checked, true);
    }
}				/* formControl */

static void
htmlImage(void)
{
    char *a;
    htmlName();
    htmlHref("src");
    topTag->jv =
       domLink("Image", topTag->name, topTag->id, "src", topTag->href,
       "images", jdoc, false);
    get_js_events();
/* don't know if javascript ever looks at alt.  Probably not. */
    if(!topTag->jv)
	return;
    a = htmlAttrVal(topAttrib, "alt");
    if(a)
	establish_property_string(topTag->jv, "alt", a, true);
}				/* htmlImage */

static void
htmlForm(void)
{
    char *a;
    void *fv;			/* form variable in javascript */

    if(topTag->slash)
	return;
    currentForm = topTag;
    htmlName();
    htmlHref("action");

    a = htmlAttrVal(topAttrib, "method");
    if(a) {
	if(stringEqualCI(a, "post"))
	    topTag->post = true;
	else if(!stringEqualCI(a, "get"))
	    browseError(MSG_GetPost);
	nzFree(a);
    }

    a = htmlAttrVal(topAttrib, "enctype");
    if(a) {
	if(stringEqualCI(a, "multipart/form-data"))
	    topTag->mime = true;
	else if(!stringEqualCI(a, "application/x-www-form-urlencoded"))
	    browseError(MSG_Enctype);
	nzFree(a);
    }

    if(a = topTag->href) {
	const char *prot = getProtURL(a);
	if(prot) {
	    if(stringEqualCI(prot, "mailto"))
		topTag->bymail = true;
	    else if(stringEqualCI(prot, "javascript"))
		topTag->javapost = true;
	    else if(stringEqualCI(prot, "https"))
		topTag->secure = true;
	    else if(!stringEqualCI(prot, "http"))
		browseError(MSG_FormProtBad, prot);
	}
    }

    nzFree(radioChecked);
    radioChecked = 0;

    topTag->jv = fv =
       domLink("Form", topTag->name, topTag->id, "action", topTag->href,
       "forms", jdoc, false);
    if(!fv)
	return;
    get_js_events();
    establish_property_array(fv, "elements");
}				/* htmlForm */

void
jsdw(void)
{
    int side;
    if(!cw->dw)
	return;
    memcpy(cw->dw + 3, "<html>\n", 7);
    side = sideBuffer(0, cw->dw + 10, -1, cw->fileName, true);
    if(side) {
	i_printf(MSG_SideBufferX, side);
    } else {
	i_puts(MSG_NoSideBuffer);
	printf("%s\n", cw->dw + 10);
    }
    nzFree(cw->dw);
    cw->dw = 0;
    cw->dw_l = 0;
}				/* jsdw */

static void
htmlInput(void)
{
    int n = INP_TEXT;
    int len;
    char *s = htmlAttrVal(topAttrib, "type");
    if(s && *s) {
	caseShift(s, 'l');
	n = stringInList(inp_types, s);
	if(n < 0) {
	    browseError(MSG_InputType, s);
	    n = INP_TEXT;
	}
    }
    topTag->itype = n;
    if(htmlAttrPresent(topAttrib, "readonly"))
	topTag->rdonly = true;
    s = htmlAttrVal(topAttrib, "maxlength");
    len = 0;
    if(s)
	len = stringIsNum(s);
    nzFree(s);
    if(len > 0)
	topTag->lic = len;
/* store the original text in value. */
/* This makes it easy to push the reset button. */
    s = htmlAttrVal(topAttrib, "value");
    if(!s)
	s = EMPTYSTRING;
    topTag->value = s;
    if(n >= INP_RADIO && htmlAttrPresent(topAttrib, "checked")) {
	char namebuf[200];
	if(n == INP_RADIO && topTag->name &&
	   strlen(topTag->name) < sizeof (namebuf) - 3) {
	    if(!radioChecked) {
		radioChecked = initString(&radioChecked_l);
		stringAndChar(&radioChecked, &radioChecked_l, '|');
	    }
	    sprintf(namebuf, "|%s|", topTag->name);
	    if(strstr(radioChecked, namebuf)) {
		browseError(MSG_RadioMany);
		return;
	    }
	    stringAndString(&radioChecked, &radioChecked_l, namebuf + 1);
	}			/* radio name */
	topTag->rchecked = true;
	topTag->checked = true;
    }

    /* Even the submit fields can have a name, but they don't have to */
    formControl(n > INP_SUBMIT);
}				/* htmlInput */

static void
makeButton(void)
{
    struct htmlTag *t = allocZeroMem(sizeof (struct htmlTag));
    addToListBack(&htmlStack, t);
    t->seqno = ntags++;
    t->info = elements + 2;
    t->action = TAGACT_INPUT;
    t->controller = currentForm;
    t->itype = INP_SUBMIT;
}				/* makeButton */

/* display the checked options */
static char *
displayOptions(const struct htmlTag *sel)
{
    const struct htmlTag *t;
    char *new;
    int l;
    new = initString(&l);
    foreach(t, htmlStack) {
	if(t->controller != sel)
	    continue;
	if(!t->checked)
	    continue;
	if(l)
	    stringAndChar(&new, &l, ',');
	stringAndString(&new, &l, t->name);
    }
    return new;
}				/* displayOptions */

static struct htmlTag *
locateOptionByName(const struct htmlTag *sel, const char *name, int *pmc,
   bool exact)
{
    struct htmlTag **list = cw->tags, *t, *em = 0, *pm = 0;
    int pmcount = 0;		/* partial match count */
    const char *s;
    while(t = *list++) {
	if(t->controller != sel)
	    continue;
	if(!(s = t->name))
	    continue;
	if(stringEqualCI(s, name)) {
	    em = t;
	    continue;
	}
	if(exact)
	    continue;
	if(strstrCI(s, name)) {
	    pm = t;
	    ++pmcount;
	}
    }
    if(em)
	return em;
    if(pmcount == 1)
	return pm;
    *pmc = pmcount;
    return 0;
}				/* locateOptionByName */

static struct htmlTag *
locateOptionByNum(const struct htmlTag *sel, int n)
{
    struct htmlTag **list = cw->tags, *t;
    int cnt = 0;
    while(t = *list++) {
	if(t->controller != sel)
	    continue;
	if(!t->name)
	    continue;
	++cnt;
	if(cnt == n)
	    return t;
    }
    return 0;
}				/* locateOptionByNum */

static bool
locateOptions(const struct htmlTag *sel, const char *input,
   char **disp_p, char **val_p, bool setcheck)
{
    struct htmlTag *t, **list;
    char *display = 0, *value = 0;
    int disp_l, val_l;
    int len = strlen(input);
    void *ev = sel->jv;
    int n, pmc, cnt;
    const char *s, *e;		/* start and end of an option */
    char *iopt;			/* individual option */

    if(disp_p)
	display = initString(&disp_l);
    if(val_p)
	value = initString(&val_l);
    iopt = allocMem(len + 1);

    if(setcheck) {
/* Uncheck all existing options, then check the ones selected. */
	if(ev)
	    set_property_number(ev, "selectedIndex", -1);
	list = cw->tags;
	while(t = *list++)
	    if(t->controller == sel && t->name) {
		t->checked = false;
		if(t->jv)
		    set_property_bool(t->jv, "selected", false);
	    }
    }

    s = input;
    while(*s) {
	e = 0;
	if(sel->multiple)
	    e = strchr(s, ',');
	if(!e)
	    e = s + strlen(s);
	len = e - s;
	strncpy(iopt, s, len);
	iopt[len] = 0;
	s = e;
	if(*s == ',')
	    ++s;

	t = locateOptionByName(sel, iopt, &pmc, true);
	if(!t) {
	    n = stringIsNum(iopt);
	    if(n >= 0)
		t = locateOptionByNum(sel, n);
	}
	if(!t)
	    t = locateOptionByName(sel, iopt, &pmc, false);
	if(!t) {
	    if(n >= 0)
		setError(MSG_XOutOfRange, n);
	    else
		setError(pmc + MSG_OptMatchNone, iopt);
/* This should never happen when we're doing a set check */
	    if(setcheck) {
		runningError(MSG_OptionSync, iopt);
		continue;
	    }
	    goto fail;
	}

	if(value) {
	    if(val_l)
		stringAndChar(&value, &val_l, '\1');
	    stringAndString(&value, &val_l, t->value);
	}
	if(display) {
	    if(disp_l)
		stringAndChar(&display, &disp_l, ',');
	    stringAndString(&display, &disp_l, t->name);
	}
	if(setcheck) {
	    t->checked = true;
	    if(t->jv) {
		set_property_bool(t->jv, "selected", true);
		if(ev)
		    set_property_number(ev, "selectedIndex", t->lic);
	    }
	}
    }				/* loop over multiple options */

    if(value)
	*val_p = value;
    if(display)
	*disp_p = display;
    free(iopt);
    return true;

  fail:
    free(iopt);
    nzFree(value);
    nzFree(display);
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

void
jSyncup(void)
{
    const struct htmlTag *t, **list;
    void *eo;			/* element object */
    int itype, j, cx;
    char *value, *cxbuf;

    if(parsePage)
	return;			/* not necessary */
    if(cw->jsdead)
	return;
    debugPrint(5, "jSyncup starts");

    jMyContext();
    buildTagArray();

    list = cw->tags;
    while(t = *list++) {
	if(t->action != TAGACT_INPUT)
	    continue;
	if(!(eo = t->jv))
	    continue;
	itype = t->itype;
	if(itype <= INP_HIDDEN)
	    continue;

	if(itype >= INP_RADIO) {
	    int checked = fieldIsChecked(t->seqno);
	    if(checked < 0)
		checked = t->rchecked;
	    set_property_bool(eo, "checked", checked);
	    continue;
	}
	/* checkable */
	value = getFieldFromBuffer(t->seqno);
/* If that line has been deleted from the user's buffer,
 * indicated by value = 0,
 * revert back to the original (reset) value. */

	if(itype == INP_SELECT) {
/* should value be replaced here, or do we know it's ok? */
	    locateOptions(t, (value ? value : t->value), 0, 0, true);
	}
	/* select */
	if(itype == INP_TA) {
	    if(!value) {
		set_property_string(eo, "value", 0);
		continue;
	    }
/* Now value is just <buffer 3>, which is meaningless. */
	    nzFree(value);
	    cx = t->lic;
	    if(!cx)
		continue;
/* The unfold command should never fail */
	    if(!unfoldBuffer(cx, false, &cxbuf, &j))
		continue;
	    set_property_string(eo, "value", cxbuf);
	    nzFree(cxbuf);
	    continue;
	}
	/* text area */
	if(value) {
	    set_property_string(eo, "value", value);
	    nzFree(value);
	} else
	    set_property_string(eo, "value", t->value);
    }				/* loop over tags */

    debugPrint(5, "jSyncup ends");
}				/* jSyncup */


/* Find the <foo> tag to match </foo> */
static struct htmlTag *
findOpenTag(const char *name)
{
    struct htmlTag *t;
    bool closing = topTag->slash;
    bool match;
    const char *desc = topTag->info->desc;
    foreachback(t, htmlStack) {
	if(t == topTag)
	    continue;		/* last one doesn't count */
	if(t->balanced)
	    continue;
	if(!t->info->nest)
	    continue;
	if(t->slash)
	    continue;		/* unbalanced slash, should never happen */
/* Now we have an unbalanced open tag */
	match = stringEqualCI(t->info->name, name);
/* I expect tags to nest perfectly, like labeled parentheses */
	if(closing) {
	    if(match)
		return t;
	    browseError(MSG_TagNest, desc, t->info->desc);
	    continue;
	}
	if(!match)
	    continue;
	if(!(t->info->nest & 2))
	    browseError(MSG_TagInTag, desc, desc);
	return t;
    }				/* loop */

    if(closing)
	browseError(MSG_TagClose, desc);
    return NULL;
}				/* findOpenTag */

static struct htmlTag *
newTag(const char *name)
{
    struct htmlTag *t;
    const struct tagInfo *ti;
    int action;
    for(ti = elements; ti->name; ++ti)
	if(stringEqualCI(ti->name, name))
	    break;
    if(!ti->name)
	return 0;
    action = ti->action;
    t = allocZeroMem(sizeof (struct htmlTag));
    t->action = action;
    t->info = ti;
    t->seqno = ntags++;
    t->balanced = true;
    addToListBack(&htmlStack, t);
    return t;
}				/* newTag */

static void
onloadGo(void *obj, const char *jsrc, const char *tagname)
{
    struct htmlTag *t;
    char buf[32];

/* The first one is easy - one line of code. */
    handlerGo(obj, "onload");

    if(!handlerPresent(obj, "onunload"))
	return;
    if(handlerPresent(obj, "onclick")) {
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

/* Always returns a string, even if errors were found.
 * Internet web pages often contain errors!
 * This routine mucks with the passed-in string, and frees it
 * when finished.  Thus the argument is not const.
 * The produced string contains only these tags.
 * Input field open and close.
 * Hyperlink open and close.
 * Internal anchor.
 * Preformat open and close.
 * Try to keep this list up to date.
 * We want a handle on what tags are hanging around during formatting. */
static char *
encodeTags(char *html)
{
    const struct tagInfo *ti;
    struct htmlTag *t, *open, *v;
    char *save_h;
    char *h = html;
    char *a;			/* for a specific attribute */
    char *new;			/* the new string */
    char *javatext;
    int js_nl;			/* number of newlines in javascript */
    const char *name, *attrib, *end;
    char tagname[12];
    const char *s;
    int j, l, namelen, lns;
    int dw_line, dw_nest = 0;
    char hnum[40];		/* hidden number */
    char c;
    bool retainTag, onload_done = false;
    bool a_text;		/* visible text within the hyperlink */
    bool slash, a_href, rc;
    bool premode = false, invisible = false;
/* Tags that cannot nest, one open at a time. */
    struct htmlTag *currentA;	/* the open anchor */
    struct htmlTag *currentSel;	/* the open select */
    struct htmlTag *currentOpt;	/* the current option */
    struct htmlTag *currentTitle;
    struct htmlTag *currentTA;	/* text area */
    int offset;			/* where the current x starts */
    int ln = 1;			/* line number */
    int action;			/* action of the tag */
    int lastact = 0;
    int nopt;			/* number of options */
    int intable = 0, inrow = 0;
    bool tdfirst;
    void *to;			/* table object */
    void *ev;			/* generic event variable */

    currentA = currentForm = currentSel = currentOpt = currentTitle =
       currentTA = 0;
    initList(&htmlStack);
    tagArray = 0;
    new = initString(&l);
    preamble = 0;
    ntags = 0;

/* first tag is a base tag, from the filename */
    t = newTag("base");
    t->href = cloneString(cw->fileName);
    basehref = t->href;

  top:
    while(c = *h) {
	if(c != '<') {
	    if(c == '\n')
		++ln;		/* keep track of line numbers */
	  putc:
	    if(!invisible) {
		if(strchr("\r\n\f", c) && !currentTA) {
		    if(!premode || c == '\r' && h[1] == '\n')
			c = ' ';
		    else if(c == '\r')
			c = '\n';
		}
		if(lastact == TAGACT_TD && c == ' ')
		    goto nextchar;
		stringAndChar(&new, &l, c);
		if(!isspaceByte(c)) {
		    a_text = true;
		    lastact = 0;
		}
	    }
	  nextchar:
	    ++h;
	    continue;
	}

	if(h[1] == '!' || h[1] == '?') {
	    h = (char *)skipHtmlComment(h, &lns);
	    ln += lns;
	    continue;
	}

	if(!parseTag(h, &name, &namelen, &attrib, &end, &lns))
	    goto putc;

/* html tag found */
	save_h = h;
	h = (char *)end;	/* skip past tag */
	if(!dw_nest)
	    browseLine = ln;
	ln += lns;
	slash = false;
	if(*name == '/')
	    slash = true, ++name, --namelen;
	if(namelen > sizeof (tagname) - 1)
	    namelen = sizeof (tagname) - 1;
	strncpy(tagname, name, namelen);
	tagname[namelen] = 0;

	for(ti = elements; ti->name; ++ti)
	    if(stringEqualCI(ti->name, tagname))
		break;
	action = ti->action;
	debugPrint(7, "tag %s %d %d %d", tagname, ntags, ln, action);

	if(currentTA) {
/* Sometimes a textarea is used to present a chunk of html code.
 * "Cut and paste this into your web page."
 * So it may contain tags.  Ignore them!
 * All except the textarea tag. */
	    if(action != TAGACT_TA) {
		ln = browseLine, h = save_h;	/* back up */
		goto putc;
	    }
/* Close it off */
	    currentTA->action = TAGACT_INPUT;
	    currentTA->itype = INP_TA;
	    currentTA->balanced = true;
	    s = currentTA->value = andTranslate(new + offset, true);
/* Text starts at the next line boundary */
	    while(*s == '\t' || *s == ' ')
		++s;
	    if(*s == '\r')
		++s;
	    if(*s == '\n')
		++s;
	    if(s > currentTA->value)
		strcpy(currentTA->value, s);
	    a = currentTA->value;
	    a += strlen(a);
	    while(a > currentTA->value && (a[-1] == ' ' || a[-1] == '\t'))
		--a;
	    *a = 0;
	    if(currentTA->jv) {
		establish_innerHTML(currentTA->jv, currentTA->inner, save_h,
		   true);
		establish_property_string(currentTA->jv, "value",
		   currentTA->value, false);
		establish_property_string(currentTA->jv, dfvl, currentTA->value,
		   true);
	    }
	    l -= strlen(new + offset);
	    new[offset] = 0;
	    j = sideBuffer(0, currentTA->value, -1, 0, false);
	    if(j) {
		currentTA->lic = j;
		sprintf(hnum, "%c%d<buffer %d%c0>",
		   InternalCodeChar, currentTA->seqno, j, InternalCodeChar);
		stringAndString(&new, &l, hnum);
	    } else
		stringAndString(&new, &l, "<buffer ?>");
	    currentTA = 0;
	    if(slash)
		continue;
	    browseError(MSG_TextareaNest);
	}

	if(!action)
	    continue;		/* tag not recognized */

	topTag = t = allocZeroMem(sizeof (struct htmlTag));
	addToListBack(&htmlStack, t);
	t->seqno = ntags;
	sprintf(hnum, "%c%d", InternalCodeChar, ntags);
	++ntags;
	t->info = ti;
	t->slash = slash;
	if(!slash)
	    t->inner = end;
	t->ln = browseLine;
	t->action = action;	/* we might change this later */
	j = end - attrib;
	topAttrib = t->attrib = j ? pullString(attrib, j) : EMPTYSTRING;

	open = 0;
	if(ti->nest) {
	    open = findOpenTag(ti->name);
	    if(slash) {
		if(!open)
		    continue;	/* unbalanced </ul> means nothing */
		open->balanced = t->balanced = true;
		if(open->jv)
		    establish_innerHTML(open->jv, open->inner, save_h, false);
	    } else
		open = 0;
	}

	if(slash && ti->bits & TAG_NOSLASH)
	    continue;		/* negated version means nothing */

/* Does this tag force the closure of an anchor? */
	if(ti->bits & TAG_CLOSEA && currentA &&
	   (action != TAGACT_A || !slash) &&
	   (a_text || action <= TAGACT_OPTION)) {
	    browseError(MSG_InAnchor, ti->desc);
	    stringAndString(&new, &l, "\2000}");
	    currentA->balanced = true;
	    currentA = 0;
/* when the </a> comes along, it will be unbalanced, and we'll ignore it. */
	}

	retainTag = true;
	if(ti->bits & TAG_INVISIBLE)
	    retainTag = false;
	if(invisible)
	    retainTag = false;
	if(ti->bits & TAG_INVISIBLE)
	    invisible = !slash;

/* Are we gathering text to build title or option? */
	if(currentTitle || currentOpt) {
	    char **ptr;
	    v = (currentTitle ? currentTitle : currentOpt);
/* Should we print an error message? */
	    if(v->action != action &&
	       !(v->action == TAGACT_OPTION && action == TAGACT_SELECT) ||
	       action != TAGACT_OPTION && !slash)
		browseError(MSG_HasTags, v->info->desc);
	    if(!(ti->bits & TAG_CLOSEA))
		continue;
/* close off the title or option */
	    v->balanced = true;
	    ptr = 0;
	    if(currentTitle && !cw->ft)
		ptr = &cw->ft;
	    if(currentOpt)
		ptr = &v->name;

	    if(ptr) {
		a = andTranslate(new + offset, true);
		stripWhite(a);
		if(currentOpt && strchr(a, ',') && currentSel->multiple) {
		    char *y;
		    for(y = a; *y; ++y)
			if(*y == ',')
			    *y = ' ';
		    browseError(MSG_OptionComma);
		}
		spaceCrunch(a, true, true);
		*ptr = a;

		if(currentTitle) {
		    if(!cw->jsdead)
			establish_property_string(jdoc, "title", a, true);
		}

		if(currentOpt) {
		    if(!*a) {
			browseError(MSG_OptionEmpty);
		    } else {
			if(!v->value)
			    v->value = cloneString(a);
		    }

		    if(ev = currentSel->jv) {	/* element variable */
			void *ov = establish_js_option(ev, v->lic);
			v->jv = ov;
			establish_property_string(ov, "text", v->name, true);
			establish_property_string(ov, "value", v->value, true);
			establish_property_bool(ov, "selected", v->checked,
			   false);
			establish_property_bool(ov, "defaultSelected",
			   v->checked, true);
		    }		/* select has corresponding java variables */
		}		/* option */
	    }
	    /* ptr is nonzero */
	    l -= strlen(new + offset);
	    new[offset] = 0;
	    currentTitle = currentOpt = 0;
	}

	switch (action) {
	case TAGACT_INPUT:
	    htmlInput();
	    if(t->itype == INP_HIDDEN)
		continue;
	    if(!retainTag)
		continue;
	    t->retain = true;
	    if(currentForm) {
		++currentForm->ninp;
		if(t->itype == INP_SUBMIT || t->itype == INP_IMAGE)
		    currentForm->submitted = true;
	    }
	    strcat(hnum, "<");
	    stringAndString(&new, &l, hnum);
	    if(t->itype < INP_RADIO) {
		if(t->value[0])
		    stringAndString(&new, &l, t->value);
		else if(t->itype == INP_SUBMIT || t->itype == INP_IMAGE)
		    stringAndString(&new, &l, "Go");
		else if(t->itype == INP_RESET)
		    stringAndString(&new, &l, "Reset");
	    } else
		stringAndChar(&new, &l, t->checked ? '+' : '-');
	    if(currentForm && (t->itype == INP_SUBMIT || t->itype == INP_IMAGE)) {
		if(currentForm->secure)
		    stringAndString(&new, &l, " secure");
		if(currentForm->bymail)
		    stringAndString(&new, &l, " bymail");
	    }
	    stringAndString(&new, &l, "\2000>");
	    goto endtag;

	case TAGACT_TITLE:
	    if(slash)
		continue;
	    if(cw->ft)
		browseError(MSG_ManyTitles);
	    offset = l;
	    currentTitle = t;
	    continue;

	case TAGACT_A:
	    a_href = false;
	    if(slash) {
		if(open->href)
		    a_href = true;
		currentA = 0;
	    } else {
		htmlName();
		htmlHref("href");
		topTag->jv =
		   domLink("Anchor", topTag->name, topTag->id, "href",
		   topTag->href, "links", jdoc, false);
		get_js_events();
		if(t->href)
		    a_href = true;
		a_text = false;
	    }
	    if(a_href) {
		if(slash) {
		    strcpy(hnum, "\2000}");
		} else {
		    strcat(hnum, "{");
		    currentA = t;
		}
	    } else {
		if(!t->name)
		    retainTag = false;	/* no need to keep this anchor */
	    }			/* href or not */
	    break;

	case TAGACT_PRE:
	    premode = !slash;
	    break;

	case TAGACT_TA:
	    currentTA = t;
	    offset = l;
	    t->itype = INP_TA;
	    formControl(true);
	    continue;

	case TAGACT_HEAD:
	    htmlName();
	    topTag->jv =
	       domLink("Head", topTag->name, topTag->id, 0, 0,
	       "heads", jdoc, false);
	    goto plainWithElements;

	case TAGACT_BODY:
	    htmlName();
	    topTag->jv =
	       domLink("Body", topTag->name, topTag->id, 0, 0,
	       "bodies", jdoc, false);
	  plainWithElements:
	    if(t->jv)
		establish_property_array(t->jv, "elements");
/* fall through */

	case TAGACT_JS:
	  plainTag:
/* check for javascript events, that's it */
	    if(!slash)
		get_js_events();
/* no need to keep these tags in the output */
	    continue;

	case TAGACT_META:
	    htmlMeta();
	    continue;

	case TAGACT_LI:
/* Look for the open UL or OL */
	    j = -1;
	    foreachback(v, htmlStack) {
		if(v->balanced || !v->info->nest)
		    continue;
		if(v->slash)
		    continue;	/* should never happen */
		s = v->info->name;
		if(stringEqual(s, "OL") ||
		   stringEqual(s, "UL") ||
		   stringEqual(s, "MENU") || stringEqual(s, "DIR")) {
		    j = 0;
		    if(*s == 'O')
			j = ++v->lic;
		}
	    }
	    if(j < 0)
		browseError(MSG_NotInList, ti->desc);
	    if(!retainTag)
		continue;
	    hnum[0] = '\r';
	    hnum[1] = 0;
	    if(j == 0)
		strcat(hnum, "* ");
	    if(j > 0)
		sprintf(hnum + 1, "%d. ", j);
	    stringAndString(&new, &l, hnum);
	    continue;

	case TAGACT_DT:
	foreachback(v, htmlStack) {
	    if(v->balanced || !v->info->nest)
		continue;
	    if(v->slash)
		continue;	/* should never happen */
	    s = v->info->name;
	    if(stringEqual(s, "DL"))
		break;
	}
	    if(v == (struct htmlTag *)&htmlStack)
		browseError(MSG_NotInList, ti->desc);
	    goto nop;

	case TAGACT_TABLE:
	    if(!slash) {
		htmlName();
		topTag->jv = to =
		   domLink("Table", topTag->name, topTag->id, 0, 0,
		   "tables", jdoc, false);
		get_js_events();
/* create the array of rows under the table */
		if(to)
		    establish_property_array(to, "rows");
	    }
	    if(slash)
		--intable;
	    else
		++intable;
	    goto nop;

	case TAGACT_TR:
	    if(!intable) {
		browseError(MSG_NotInTable, ti->desc);
		continue;
	    }
/* This really doesn't work for tables inside tables */
	    if(slash)
		--inrow;
	    else
		++inrow;
	    tdfirst = true;
	    if((!slash) && (open = findOpenTag("table")) && (to = open->jv)) {
		htmlName();
		topTag->jv =
		   domLink("Trow", topTag->name, topTag->id, 0, 0,
		   "rows", open->jv, false);
		get_js_events();
	    }
	    goto nop;

	case TAGACT_TD:
	    if(!inrow) {
		browseError(MSG_NotInRow, ti->desc);
		continue;
	    }
	    if(slash)
		continue;
	    if(tdfirst)
		tdfirst = false;
	    else if(retainTag) {
		while(l && new[l - 1] == ' ')
		    --l;
		new[l] = 0;
		stringAndChar(&new, &l, '|');
	    }
	    goto endtag;

	case TAGACT_DIV:
	    if(!slash) {
		htmlName();
		topTag->jv =
		   domLink("Div", topTag->name, topTag->id, 0, 0,
		   "divs", jdoc, false);
		get_js_events();
	    }
	    goto nop;

	case TAGACT_SPAN:
	    if(!slash) {
		htmlName();
		topTag->jv =
		   domLink("Span", topTag->name, topTag->id, 0, 0,
		   "spans", jdoc, false);
		get_js_events();
	    }
	    goto nop;

	case TAGACT_BR:
	    if(lastact == TAGACT_TD)
		continue;

	case TAGACT_NOP:
	  nop:
	    if(!retainTag)
		continue;
	    j = ti->para;
	    if(slash)
		j >>= 2;
	    else
		j &= 3;
	    if(!j)
		continue;
	    c = '\f';
	    if(j == 1) {
		c = '\r';
		if(action == TAGACT_BR)
		    c = '\n';
	    }
	    if(currentA)
		c = ' ';
	    stringAndChar(&new, &l, c);
	    continue;

	case TAGACT_FORM:
	    htmlForm();
	    if(currentSel) {
	      doneSelect:
		currentSel->action = TAGACT_INPUT;
		if(currentSel->controller)
		    ++currentSel->controller->ninp;
		currentSel->value = a = displayOptions(currentSel);
		if(retainTag) {
		    currentSel->retain = true;
/* Crank out the input tag */
		    sprintf(hnum, "%c%d<", InternalCodeChar, currentSel->seqno);
		    stringAndString(&new, &l, hnum);
		    stringAndString(&new, &l, a);
		    stringAndString(&new, &l, "\2000>");
		}
		currentSel = 0;
	    }
	    if(action == TAGACT_FORM && slash && currentForm) {
		if(retainTag && currentForm->href && !currentForm->submitted) {
		    makeButton();
		    sprintf(hnum, " %c%d<Go", InternalCodeChar, ntags - 1);
		    stringAndString(&new, &l, hnum);
		    if(currentForm->secure)
			stringAndString(&new, &l, " secure");
		    if(currentForm->bymail)
			stringAndString(&new, &l, " bymail");
		    stringAndString(&new, &l, " implicit\2000>");
		}
		currentForm = 0;
	    }
	    continue;

	case TAGACT_SELECT:
	    if(slash) {
		if(currentSel)
		    goto doneSelect;
		continue;
	    }
	    currentSel = t;
	    nopt = 0;
	    t->itype = INP_SELECT;
	    if(htmlAttrPresent(topAttrib, "readonly"))
		t->rdonly = true;
	    if(htmlAttrPresent(topAttrib, "multiple"))
		t->multiple = true;
	    formControl(true);
	    continue;

	case TAGACT_OPTION:
	    if(slash)
		continue;
	    if(!currentSel) {
		browseError(MSG_NotInSelect);
		continue;
	    }
	    currentOpt = t;
	    offset = l;
	    t->controller = currentSel;
	    t->lic = nopt++;
	    t->value = htmlAttrVal(topAttrib, "value");
	    if(htmlAttrPresent(topAttrib, "selected")) {
		if(currentSel->lic && !currentSel->multiple)
		    browseError(MSG_ManyOptSelected);
		else
		    t->checked = t->rchecked = true, ++currentSel->lic;
	    }
	    continue;

	case TAGACT_HR:
	    if(!retainTag)
		continue;
	    stringAndString(&new, &l,
	       "\r--------------------------------------------------------------------------------\r");
	    continue;

	case TAGACT_SUP:
	case TAGACT_SUB:
	    if(!retainTag)
		continue;
	    t->retain = true;
	    j = (action == TAGACT_SUP ? 2 : 1);
	    if(!slash) {
		t->lic = l;
		stringAndString(&new, &l, (j == 2 ? "^(" : "["));
		continue;
	    }
/* backup, and see if we can get rid of the parentheses or brackets */
	    a = new + open->lic + j;
	    if(j == 2 && isalphaByte(*a) && !a[1])
		goto unparen;
	    if(j == 2 && isalnumByte(a[-3]) && (stringEqual(a, "th") ||
	       stringEqual(a, "rd") ||
	       stringEqual(a, "nd") || stringEqual(a, "st"))) {
		a -= 2, l -= 2;
		strcpy(a, a + 2);
		continue;
	    }
	    while(isdigitByte(*a))
		++a;
	    if(!*a)
		goto unparen;
	    stringAndChar(&new, &l, (j == 2 ? ')' : ']'));
	    continue;
/* ok, we can trash the original ( or [ */
	  unparen:
	    a = new + open->lic + j - 1;
	    strcpy(a, a + 1);
	    --l;
	    if(j == 2)
		stringAndChar(&new, &l, ' ');
	    continue;

	case TAGACT_AREA:
	case TAGACT_FRAME:
	    htmlName();
	    if(action == TAGACT_FRAME) {
		htmlHref("src");
		topTag->jv =
		   domLink("Frame", topTag->name, 0, "src",
		   topTag->href, "frames", jwin, false);
	    } else {
		htmlHref("href");
		topTag->jv =
		   domLink("Area", topTag->name, topTag->id, "href",
		   topTag->href, "areas", jdoc, false);
	    }
	    get_js_events();
	    if(!retainTag)
		continue;
	    stringAndString(&new, &l,
	       action == TAGACT_FRAME ? "\rFrame " : "\r");
	    name = t->name;
	    if(!name)
		name = altText(t->href);
	    if(!name)
		name = (action == TAGACT_FRAME ? "???" : "area");
	    if(t->href) {
		strcat(hnum, "{");
		stringAndString(&new, &l, hnum);
		t->action = TAGACT_A;
		t->balanced = true;
	    }
	    if(t->href || action == TAGACT_FRAME)
		stringAndString(&new, &l, name);
	    if(t->href)
		stringAndString(&new, &l, "\2000}");
	    stringAndChar(&new, &l, '\r');
	    continue;

	case TAGACT_MUSIC:
	    if(!retainTag)
		continue;
	    htmlHref("src");
	    if(!t->href)
		continue;
	    toPreamble(t->seqno,
	       (ti->name[0] == 'A' ? "Audio passage" : "Background Music"),
	       0, 0);
	    t->action = TAGACT_A;
	    continue;

	case TAGACT_BASE:
	    htmlHref("href");
	    if(!t->href)
		continue;
	    basehref = t->href;
	    debugPrint(3, "base href %s", basehref);
	    continue;

	case TAGACT_IMAGE:
	    htmlImage();
	    if(!currentA) {
/* I'm going to assume that if the web designer took the time
 * to put in an alt tag, then it's worth reading.
 * You can turn this feature off, but I don't think you'd want to. */
		if(a = htmlAttrVal(topAttrib, "alt"))
		    stringAndString(&new, &l, a);
		continue;
	    }
	    if(!retainTag)
		continue;
	    if(a_text)
		continue;
	    s = 0;
	    a = htmlAttrVal(topAttrib, "alt");
	    if(a) {
		s = altText(a);
		nzFree(a);
	    }
	    if(!s)
		s = altText(t->name);
	    if(!s)
		s = altText(currentA->href);
	    if(!s)
		s = altText(t->href);
	    if(!s)
		s = "image";
	    stringAndString(&new, &l, s);
	    a_text = true;
	    continue;

	case TAGACT_SCRIPT:
	    if(slash)
		continue;
	    rc = findEndScript(h, ti->name,
	       (ti->action == TAGACT_SCRIPT), &h, &javatext, &js_nl);
	    if(*h)
		h = strchr(h, '>') + 1;
	    ln += js_nl;
/* I'm not going to process an open ended script. */
	    if(!rc) {
		nzFree(javatext);
		runningError(MSG_ScriptNotClosed);
		cw->jsdead = true;
		continue;
	    }
	    htmlHref("src");
	    a = htmlAttrVal(topAttrib, "language");
/* If no language is specified, javascript is default. */
	    if(!cw->jsdead &&
	       (!a || !*a || !intFlag &&
	       a && memEqualCI(a, "javascript", 10) && !isalphaByte(a[10]))) {
/* It's javascript, run with the source, or the inline text. */
		int js_line = browseLine;
		char *js_file = cw->fileName;
		if(t->href) {	/* fetch the javascript page */
		    nzFree(javatext);
		    javatext = 0;
		    if(javaOK(t->href)) {
			debugPrint(2, "java source %s", t->href);
			if(browseLocal && !isURL(t->href)) {
			    if(!fileIntoMemory(t->href, &serverData,
			       &serverDataLen)) {
				runningError(MSG_GetLocalJS, errorMsg);
			    } else {
				javatext = serverData;
				prepareForBrowse(javatext, serverDataLen);
			    }
			} else if(httpConnect(basehref, t->href)) {
			    if(hcode == 200) {
				javatext = serverData;
				prepareForBrowse(javatext, serverDataLen);
			    } else {
				nzFree(serverData);
				runningError(MSG_GetJS, t->href, hcode);
			    }
			} else {
			    runningError(MSG_GetJS2, errorMsg);
			}
			js_line = 1;
			js_file = t->href;
		    }
		}		/* fetch from the net */
		if(javatext) {
		    char *w = 0;
		    if(js_file)
			w = strrchr(js_file, '/');
		    if(w) {
/* Trailing slash doesn't count */
			if(w[1] == 0 && w > js_file)
			    for(--w; w >= js_file && *w != '/'; --w) ;
			js_file = w + 1;
		    }
		    debugPrint(3, "execute %s at %d", js_file, js_line);
		    javaParseExecute(jwin, javatext, js_file, js_line);
		    debugPrint(3, "execution complete");

/* See if the script has produced html, via document.write() */
		    if(cw->dw) {
			int afterlen;	/* after we fold in this string */
			char *after;
			struct htmlTag *z;
			debugPrint(3, "docwrite %d bytes", cw->dw_l);
			debugPrint(4, "<<\n%s\n>>", cw->dw + 10);
			stringAndString(&cw->dw, &cw->dw_l, "</docwrite>");
			afterlen = strlen(h) + strlen(cw->dw);
			after = allocMem(afterlen + 1);
			strcpy(after, cw->dw);
			strcat(after, h);
			nzFree(cw->dw);
			cw->dw = 0;
			cw->dw_l = 0;
			nzFree(html);
			html = h = after;
/* After the realloc, the inner pointers are no longer valid. */
			foreach(z, htmlStack) z->inner = 0;
		    }		/* document.write */
		}
	    }
	    nzFree(javatext);
	    nzFree(a);
	    continue;

	case TAGACT_OBJ:
/* no clue what to do here */
	    continue;

	case TAGACT_DW:
	    if(slash) {
		if(!--dw_nest)
		    browseLine = ln = dw_line;
	    } else {
		if(!dw_nest++)
		    dw_line = ln;
		browseLine = ln = 0;
	    }
	    continue;

	default:
	    browseError(MSG_BadTag, action);
	    continue;
	}			/* switch */

	if(!retainTag)
	    continue;
	t->retain = true;
	if(!strpbrk(hnum, "{}")) {
	    strcat(hnum, "*");
/* Leave the meaningless tags out. */
	    if(action == TAGACT_PRE || action == TAGACT_A && topTag->name) ;	/* ok */
	    else
		hnum[0] = 0;
	}
	stringAndString(&new, &l, hnum);
      endtag:
	lastact = action;
    }				/* loop over html string */

    if(currentA) {
	stringAndString(&new, &l, "\2000}");
	currentA = 0;
    }

/* Run the various onload functions */
/* Turn the onunload functions into hyperlinks */
    if(!cw->jsdead && !onload_done) {
	const struct htmlTag *lasttag;
	onloadGo(jwin, 0, "window");
	onloadGo(jdoc, 0, "document");
	lasttag = htmlStack.prev;
	foreach(t, htmlStack) {
	    char *jsrc;
	    ev = t->jv;
/* in case the option has disappeared */
	    if(t->action == TAGACT_OPTION)
		goto next_onload;
	    if(!ev)
		goto next_onload;
	    if(t->slash)
		goto next_onload;
	    jsrc = htmlAttrVal(t->attrib, "onunload");
	    onloadGo(ev, jsrc, t->info->name);

	  next_onload:
	    if(t == lasttag)
		break;
	}			/* loop over tags */
    }
    onload_done = true;

/* The onload function can, and often does, invoke document.write() */
    if(cw->dw) {
	nzFree(html);
	html = h = cw->dw;
	cw->dw = 0;
	cw->dw_l = 0;
	goto top;
    }

    if(browseLocal == 1) {	/* no errors yet */
	foreach(t, htmlStack) {
	    browseLine = t->ln;
	    if(t->info->nest && !t->slash && !t->balanced) {
		browseError(MSG_TagNotClosed, t->info->desc);
		break;
	    }

/* Make sure the internal links are defined. */
	    if(t->action != TAGACT_A)
		continue;	/* not anchor */
	    h = t->href;
	    if(!h)
		continue;
	    if(h[0] != '#')
		continue;
	    if(h[1] == 0)
		continue;
	    a = h + 1;		/* this is what we're looking for */
	    foreach(v, htmlStack) {
		if(v->action != TAGACT_A)
		    continue;	/* not achor */
		if(!v->name)
		    continue;	/* no name */
		if(stringEqual(a, v->name))
		    break;
	    }
	    if(v == (struct htmlTag *)&htmlStack) {	/* end of list */
		browseError(MSG_NoLable2, a);
		break;
	    }
	}			/* loop over all tags */
    }

    /* clean up */
    browseLine = 0;
    nzFree(html);
    nzFree(radioChecked);
    radioChecked = 0;
    basehref = 0;
    if(preamble) {
	h = allocMem(preamble_l + l + 2);
	strcpy(h, preamble);
	h[preamble_l] = '\f';
	strcpy(h + preamble_l + 1, new);
	nzFree(preamble);
	preamble = 0;
	nzFree(new);
	new = h;
    }

    return new;
}				/* encodeTags */

void
preFormatCheck(int tagno, bool * pretag, bool * slash)
{
    const struct htmlTag *t;
    if(!parsePage)
	errorPrint("@calling preFormatTag without parsePage");
    *pretag = *slash = false;
    if(tagno >= 0 && tagno < ntags) {
	t = tagArray[tagno];
	*pretag = (t->action == TAGACT_PRE);
	*slash = t->slash;
    }
}				/* preFormatCheck */

char *
htmlParse(char *buf, int remote)
{
    char *newbuf;

    if(parsePage)
	errorPrint("@htmlParse() is not reentrant.");
    parsePage = true;
    if(remote >= 0)
	browseLocal = !remote;
    buf = encodeTags(buf);
    debugPrint(7, "%s", buf);

    buildTagArray();

    newbuf = andTranslate(buf, false);
    nzFree(buf);
    buf = newbuf;
    anchorSwap(buf);
    debugPrint(7, "%s", buf);

    newbuf = htmlReformat(buf);
    nzFree(buf);
    buf = newbuf;

    parsePage = false;

/* In case one of the onload functions called document.write() */
    jsdw();

    return buf;
}				/* htmlParse */

void
findField(const char *line, int ftype, int n,
   int *total, int *realtotal, int *tagno, char **href, void **evp)
{
    const struct htmlTag *t, **list = cw->tags;
    int nt = 0;			/* number of fields total */
    int nrt = 0;		/* the real total, for input fields */
    int nm = 0;			/* number match */
    int j;
    const char *s, *ss;
    char *h, *nmh;
    char c;
    static const char urlok[] =
       "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890,./?@#%&-_+=:~";

    if(href)
	*href = 0;
    if(evp)
	*evp = 0;

    if(cw->browseMode) {
	s = line;
	while((c = *s) != '\n') {
	    ++s;
	    if(c != (char)InternalCodeChar)
		continue;
	    j = strtol(s, (char **)&s, 10);
	    if(!ftype) {
		if(*s != '{')
		    continue;
		++nt, ++nrt;
		if(n == nt)
		    nm = j;
		if(!n) {
		    if(!nm)
			nm = j;
		    else
			nm = -1;
		}
	    } else {
		if(*s != '<')
		    continue;
		if(n) {
		    ++nt, ++nrt;
		    if(n == nt)
			nm = j;
		    continue;
		}
		t = list[j];
		++nrt;
		if(ftype == 1 && t->itype <= INP_SUBMIT)
		    continue;
		if(ftype == 2 && t->itype > INP_SUBMIT)
		    continue;
		++nt;
		if(!nm)
		    nm = j;
		else
		    nm = -1;
	    }
	}			/* loop over line */
    }

    if(nm < 0)
	nm = 0;
    if(total)
	*total = nrt;
    if(realtotal)
	*realtotal = nt;
    if(tagno)
	*tagno = nm;
    if(!ftype && nm) {
	t = list[nm];
	if(href)
	    *href = cloneString(t->href);
	if(evp)
	    *evp = t->jv;
	if(href && t->jv) {
/* defer to the java variable for the reference */
	    const char *jh = get_property_url(t->jv, false);
	    if(jh) {
		if(!*href || !stringEqual(*href, jh)) {
		    nzFree(*href);
		    *href = cloneString(jh);
		}
	    }
	}
    }

    if(nt || ftype)
	return;

/* Second time through, maybe the url is in plain text. */
    nmh = 0;
    s = line;
    while(true) {
/* skip past weird characters */
	while((c = *s) != '\n') {
	    if(strchr(urlok, c))
		break;
	    ++s;
	}
	if(c == '\n')
	    break;
	ss = s;
	while(strchr(urlok, *s))
	    ++s;
	h = pullString1(ss, s);
	unpercentURL(h);
	if(!isURL(h)) {
	    free(h);
	    continue;
	}
	++nt;
	if(n == nt) {
	    nm = nt;
	    nmh = h;
	    continue;
	}
	if(n) {
	    free(h);
	    continue;
	}
	if(!nm) {
	    nm = nt;
	    nmh = h;
	    continue;
	}
	free(h);
	nm = -1;
	free(nmh);
	nmh = 0;
    }				/* loop over line */

    if(nm < 0)
	nm = 0;
    if(total)
	*total = nt;
    if(realtotal)
	*realtotal = nt;
    if(href)
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

bool
lineHasTag(const char *p, const char *s)
{
    const struct htmlTag *t, **list = cw->tags;
    char c;
    int j;
    while((c = *p++) != '\n') {
	if(c != (char)InternalCodeChar)
	    continue;
	j = strtol(p, (char **)&p, 10);
	if(*p != '*')
	    continue;
	t = list[j];
	if(t->action != TAGACT_A)
	    continue;
	if(!t->name)
	    continue;
	if(stringEqual(t->name, s))
	    return true;
    }
    return false;
}				/* lineHasTag */

/* See if there are simple tags like <p> or </font> */
bool
htmlTest(void)
{
    int j, ln;
    int cnt = 0;
    int fsize = 0;		/* file size */
    char look[12];
    bool firstline = true;

    for(ln = 1; ln <= cw->dol; ++ln) {
	char *p = (char *)fetchLine(ln, -1);
	char c;
	int state = 0;

	while(isspaceByte(*p) && *p != '\n')
	    ++p;
	if(*p == '\n')
	    continue;		/* skip blank line */
	if(firstline && *p == '<') {
/* check for <!doctype */
	    if(memEqualCI(p + 1, "!doctype", 8))
		return true;
/* If it starts with <tag, for any tag we recognize,
 * we'll call it good. */
	    for(j = 1; j < 10; ++j) {
		if(!isalnumByte(p[j]))
		    break;
		look[j - 1] = p[j];
	    }
	    look[j - 1] = 0;
	    if(j > 1 && (p[j] == '>' || isspaceByte(p[j]))) {
/* something we recognize? */
		const struct tagInfo *ti;
		for(ti = elements; ti->name; ++ti)
		    if(stringEqualCI(ti->name, look))
			return true;
	    }			/* leading tag */
	}			/* leading < */
	firstline = false;

/* count tags through the buffer */
	for(j = 0; (c = p[j]) != '\n'; ++j) {
	    if(state == 0) {
		if(c == '<')
		    state = 1;
		continue;
	    }
	    if(state == 1) {
		if(c == '/')
		    state = 2;
		else if(isalphaByte(c))
		    state = 3;
		else
		    state = 0;
		continue;
	    }
	    if(state == 2) {
		if(isalphaByte(c))
		    state = 3;
		else
		    state = 0;
		continue;
	    }
	    if(isalphaByte(c))
		continue;
	    if(c == '>')
		++cnt;
	    state = 0;
	}
	fsize += j;
    }				/* loop over lines */

/* we need at least one of these tags every 300 characters.
 * And we need at least 4 such tags.
 * Remember, you can always override by putting <html> at the top. */
    return (cnt >= 4 && cnt * 300 >= fsize);
}				/* htmlTest */

/* Show an input field */
void
infShow(int tagno, const char *search)
{
    const struct htmlTag **list = cw->tags;
    const struct htmlTag *t = list[tagno], *v;
    const char *s;
    int j, cnt;
    bool show;

    s = inp_types[t->itype];
    if(*s == ' ')
	++s;
    printf("%s", s);
    if(t->multiple)
	i_printf(MSG_Many);
    if(t->itype >= INP_TEXT && t->itype <= INP_NUMBER && t->lic)
	printf("[%d]", t->lic);
    if(t->itype == INP_TA) {
	char *rows = htmlAttrVal(t->attrib, "rows");
	char *cols = htmlAttrVal(t->attrib, "cols");
	char *wrap = htmlAttrVal(t->attrib, "wrap");
	if(rows && cols) {
	    printf("[%sx%s", rows, cols);
	    if(wrap && stringEqualCI(wrap, "virtual"))
		i_printf(MSG_Recommended);
	    i_printf(MSG_Close);
	}
	nzFree(rows);
	nzFree(cols);
	nzFree(wrap);
    }				/* text area */
    if(t->name)
	printf(" %s", t->name);
    nl();
    if(t->itype != INP_SELECT)
	return;

/* display the options in a pick list */
/* If a search string is given, display the options containing that string. */
    cnt = 0;
    show = false;
    for(j = 0; v = list[j]; ++j) {
	if(v->controller != t)
	    continue;
	if(!v->name)
	    continue;
	++cnt;
	if(*search && !strstrCI(v->name, search))
	    continue;
	show = true;
	printf("%3d %s\n", cnt, v->name);
    }
    if(!show) {
	if(!search)
	    i_puts(MSG_NoOptions);
	else
	    i_printf(MSG_NoOptionsMatch, search);
    }
}				/* infShow */

/* Update an input field. */
bool
infReplace(int tagno, const char *newtext, int notify)
{
    const struct htmlTag **list = cw->tags;
    const struct htmlTag *t = list[tagno], *v;
    const struct htmlTag *form = t->controller;
    char *display;
    int itype = t->itype;
    int newlen = strlen(newtext);

/* sanity checks on the input */
    if(itype <= INP_SUBMIT) {
	int b = MSG_IsButton;
	if(itype == INP_SUBMIT || itype == INP_IMAGE)
	    b = MSG_SubmitButton;
	if(itype == INP_RESET)
	    b = MSG_ResetButton;
	setError(b);
	return false;
    }

    if(itype == INP_TA) {
	setError(MSG_Textarea, t->lic);
	return false;
    }

    if(t->rdonly) {
	setError(MSG_Readonly);
	return false;
    }

    if(strchr(newtext, '\n')) {
	setError(MSG_InputNewline);
	return false;
    }

    if(itype >= INP_TEXT && itype <= INP_NUMBER && t->lic && newlen > t->lic) {
	setError(MSG_InputLong, t->lic);
	return false;
    }

    if(itype >= INP_RADIO) {
	if(newtext[0] != '+' && newtext[0] != '-' || newtext[1]) {
	    setError(MSG_InputRadio);
	    return false;
	}
	if(itype == INP_RADIO && newtext[0] == '-') {
	    setError(MSG_ClearRadio);
	    return false;
	}
    }

/* Two lines, clear the "other" radio button, and set this one. */
    if(!linesComing(2))
	return false;

    jMyContext();

    if(itype == INP_SELECT) {
	if(!locateOptions(t, newtext, 0, 0, false))
	    return false;
	locateOptions(t, newtext, &display, 0, false);
	updateFieldInBuffer(tagno, display, notify, true);
	nzFree(display);
    }

    if(itype == INP_FILE) {
	if(!envFile(newtext, &newtext))
	    return false;
	if(newtext[0] && access(newtext, 4)) {
	    setError(MSG_FileAccess, newtext);
	    return false;
	}
    }

    if(itype == INP_NUMBER) {
	if(*newtext && stringIsNum(newtext) < 0) {
	    setError(MSG_NumberExpected);
	    return false;
	}
    }

    if(itype == INP_RADIO && form && t->name && *newtext == '+') {
/* clear the other radio button */
	while(v = *list++) {
	    if(v->controller != form)
		continue;
	    if(v->itype != INP_RADIO)
		continue;
	    if(!v->name)
		continue;
	    if(!stringEqual(v->name, t->name))
		continue;
	    if(fieldIsChecked(v->seqno) == true)
		updateFieldInBuffer(v->seqno, "-", 0, false);
	}
    }

    if(itype != INP_SELECT) {
	updateFieldInBuffer(tagno, newtext, notify, true);
    }

    if(itype >= INP_RADIO && tagHandler(t->seqno, "onclick")) {
	if(cw->jsdead)
	    runningError(MSG_NJNoOnclick);
	else {
	    jSyncup();
	    handlerGo(t->jv, "onclick");
	    jsdw();
	    if(js_redirects)
		return true;
	}
    }

    if(itype >= INP_TEXT && itype <= INP_SELECT &&
       tagHandler(t->seqno, "onchange")) {
	if(cw->jsdead)
	    runningError(MSG_NJNoOnchange);
	else {
	    jSyncup();
	    handlerGo(t->jv, "onchange");
	    jsdw();
	    if(js_redirects)
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

static void
resetVar(struct htmlTag *t)
{
    int itype = t->itype;
    const char *w = t->value;
    bool bval;
    void *jv = t->jv;

/* This is a kludge - option looks like INP_SELECT */
    if(t->action == TAGACT_OPTION)
	itype = INP_SELECT;

    if(itype <= INP_SUBMIT)
	return;

    if(itype >= INP_SELECT) {
	bval = t->rchecked;
	t->checked = bval;
	w = bval ? "+" : "-";
    }

    if(itype == INP_TA) {
	int cx = t->lic;
	if(cx)
	    sideBuffer(cx, t->value, -1, 0, false);
    } else if(itype != INP_HIDDEN && itype != INP_SELECT)
	updateFieldInBuffer(t->seqno, w, 0, false);

    if(jv) {
	if(itype >= INP_RADIO) {
	    set_property_bool(jv, "checked", bval);
	} else if(itype == INP_SELECT) {
	    set_property_bool(jv, "selected", bval);
	    if(bval && !t->controller->multiple)
		set_property_number(t->controller->jv, "selectedIndex", t->lic);
	} else
	    set_property_string(jv, "value", w);
    }
}				/* resetVar */

static void
formReset(const struct htmlTag *form)
{
    struct htmlTag **list = cw->tags, *t, *sel = 0;
    int itype;

    while(t = *list++) {
	if(t->action == TAGACT_OPTION) {
	    if(!sel)
		continue;
	    if(t->controller != sel)
		continue;
	    resetVar(t);
	    continue;
	}
	/* option */
	if(t->action != TAGACT_INPUT)
	    continue;

	if(sel) {
	    char *display = displayOptions(sel);
	    updateFieldInBuffer(sel->seqno, display, 0, false);
	    nzFree(display);
	    sel = 0;
	}

	if(t->controller != form)
	    continue;
	itype = t->itype;
	if(itype != INP_SELECT) {
	    resetVar(t);
	    continue;
	}
	sel = t;
	if(t->jv)
	    set_property_number(t->jv, "selectedIndex", -1);
    }				/* loop over tags */

    i_puts(MSG_FormReset);
}				/* formReset */

/* Fetch a field value (from a form) to post. */
/* The result is allocated */
static char *
fetchTextVar(const struct htmlTag *t)
{
    void *jv = t->jv;
    char *v;
    if(jv) {
	return cloneString(get_property_string(jv, "value"));
    }
    if(t->itype > INP_HIDDEN) {
	v = getFieldFromBuffer(t->seqno);
	if(v)
	    return v;
    }
/* Revert to the default value */
    return cloneString(t->value);
}				/* fetchTextVar */

static bool
fetchBoolVar(const struct htmlTag *t)
{
    void *jv = t->jv;
    int checked;
    if(jv) {
	return get_property_bool(jv,
	   (t->action == TAGACT_OPTION ? "selected" : "checked"));
    }
    checked = fieldIsChecked(t->seqno);
    if(checked < 0)
	checked = t->rchecked;
    return checked;
}				/* fetchBoolVar */

/* Some information on posting forms can be found here.
 * http://www.w3.org/TR/REC-html40/interact/forms.html */

static void
postDelimiter(char fsep, const char *boundary, char **post, int *l)
{
    char *p = *post;
    char c = p[*l - 1];
    if(c == '?' || c == '\1')
	return;
    if(fsep == '-') {
	stringAndString(post, l, "--");
	stringAndString(post, l, boundary);
	stringAndChar(post, l, '\r');
	fsep = '\n';
    }
    stringAndChar(post, l, fsep);
}				/* postDelimiter */

static bool
postNameVal(const char *name, const char *val,
   char fsep, uchar isfile, const char *boundary, char **post, int *l)
{
    char *enc;
    const char *ct, *ce;	/* content type, content encoding */

    if(name && !*name)
	name = 0;
    if(val && !*val)
	val = 0;
    if(!name && !val)
	return true;

    if(name) {
	postDelimiter(fsep, boundary, post, l);
	switch (fsep) {
	case '&':
	    enc = encodePostData(name);
	    stringAndString(post, l, enc);
	    stringAndChar(post, l, '=');
	    nzFree(enc);
	    break;
	case '\n':
	    stringAndString(post, l, name);
	    stringAndString(post, l, "=\r\n");
	    break;
	case '-':
	    stringAndString(post, l, "Content-Disposition: form-data; name=\"");
	    stringAndString(post, l, name);
	    stringAndChar(post, l, '"');
/* I'm leaving nl off, in case we need ; filename */
	    break;
	}			/* switch */
    }
    /* name */
    if(!val) {
	if(fsep == '&')
	    return true;
	val = EMPTYSTRING;
    }
    /* no value */
    switch (fsep) {
    case '&':
	enc = encodePostData(val);
	stringAndString(post, l, enc);
	nzFree(enc);
	break;
    case '\n':
	stringAndString(post, l, val);
	stringAndString(post, l, eol);
	break;
    case '-':
	if(isfile) {
	    if(isfile & 2) {
		stringAndString(post, l, "; filename=\"");
		stringAndString(post, l, val);
		stringAndChar(post, l, '"');
	    }
	    if(!encodeAttachment(val, 0, &ct, &ce, &enc))
		return false;
	    val = enc;
/* remember to free val in this case */
	} else {
	    const char *s;
	    ct = "text/plain";
/* Anything nonascii makes it 8bit */
	    ce = "7bit";
	    for(s = val; *s; ++s)
		if(*s < 0) {
		    ce = "8bit";
		    break;
		}
	}
	stringAndString(post, l, "\r\nContent-Type: ");
	stringAndString(post, l, ct);
	stringAndString(post, l, "\r\nContent-Transfer-Encoding: ");
	stringAndString(post, l, ce);
	stringAndString(post, l, "\r\n\r\n");
	stringAndString(post, l, val);
	stringAndString(post, l, eol);
	if(isfile)
	    nzFree(enc);
	break;
    }				/* switch */

    return true;
}				/* postNameVal */

static bool
formSubmit(const struct htmlTag *form, const struct htmlTag *submit,
   char **post, int *l)
{
    const struct htmlTag **list = cw->tags, *t;
    int itype;
    int j;
    char *name, *value;
    const char *boundary;
    char fsep = '&';		/* field separator */
    bool noname = false, rc;
    bool bval;

    if(form->bymail)
	fsep = '\n';
    if(form->mime) {
	fsep = '-';
	boundary = makeBoundary();
	stringAndString(post, l, "`mfd~");
	stringAndString(post, l, boundary);
	stringAndString(post, l, eol);
    }

    while(t = *list++) {
	if(t->action != TAGACT_INPUT)
	    continue;
	if(t->controller != form)
	    continue;
	itype = t->itype;
	if(itype <= INP_SUBMIT && t != submit)
	    continue;
	name = t->name;

	if(t == submit) {	/* the submit button you pushed */
	    int namelen;
	    char *nx;
	    if(!name)
		continue;
	    value = t->value;
	    if(!value)
		value = "Submit";
	    if(t->itype != INP_IMAGE)
		goto success;
	    namelen = strlen(name);
	    nx = allocMem(namelen + 3);
	    strcpy(nx, name);
	    strcpy(nx + namelen, ".x");
	    postNameVal(nx, "0", fsep, false, boundary, post, l);
	    nx[namelen + 1] = 'y';
	    postNameVal(nx, "0", fsep, false, boundary, post, l);
	    nzFree(nx);
	    goto success;
	}

	if(itype >= INP_RADIO) {
	    value = t->value;
	    bval = fetchBoolVar(t);
	    if(!bval)
		continue;
	    if(!name) {
		noname = true;
		continue;
	    }
	    if(value && !*value)
		value = 0;
	    if(itype == INP_CHECKBOX && value == 0)
		value = "on";
	    goto success;
	}

	if(itype < INP_FILE) {
/* Even a hidden variable can be adjusted by js.
 * fetchTextVar allows for this possibility.
 * I didn't allow for it in the above, the value of a radio button;
 * hope that's not a problem. */
	    value = fetchTextVar(t);
	    postNameVal(name, value, fsep, false, boundary, post, l);
	    nzFree(value);
	    continue;
	}

	if(itype == INP_TA) {
	    int cx = t->lic;
	    char *cxbuf;
	    int cxlen;
	    if(!name) {
		noname = true;
		continue;
	    }
	    if(cx) {
		if(fsep == '-') {
		    char cxstring[12];
/* do this as an attachment */
		    sprintf(cxstring, "%d", cx);
		    if(!postNameVal(name, cxstring, fsep, 1, boundary, post, l))
			goto fail;
		    continue;
		}		/* attach */
		if(!unfoldBuffer(cx, textAreaDosNewlines, &cxbuf, &cxlen))
		    goto fail;
		for(j = 0; j < cxlen; ++j)
		    if(cxbuf[j] == 0) {
			setError(MSG_SessionNull, cx);
			nzFree(cxbuf);
			goto fail;
		    }
		if(j && cxbuf[j - 1] == '\n')
		    --j;
		if(j && cxbuf[j - 1] == '\r')
		    --j;
		cxbuf[j] = 0;
		rc = postNameVal(name, cxbuf, fsep, false, boundary, post, l);
		nzFree(cxbuf);
		if(rc)
		    continue;
		goto fail;
	    }
	    /* valid context */
	    /* Just an empty string */
	    postNameVal(name, 0, fsep, false, boundary, post, l);
	    continue;
	}

	if(itype == INP_SELECT) {
	    char *display = getFieldFromBuffer(t->seqno);
	    char *s, *e;
	    if(!display) {	/* off the air */
		struct htmlTag *v, **vl = cw->tags;
/* revert back to reset state */
		while(v = *vl++)
		    if(v->controller == t)
			v->checked = v->rchecked;
		display = displayOptions(t);
	    }
	    rc = locateOptions(t, display, 0, &value, false);
	    nzFree(display);
	    if(!rc)
		goto fail;	/* this should never happen */
/* option could have an empty value, usually the null choice,
 * before you have made a selection. */
	    if(!*value) {
		postNameVal(name, value, fsep, false, boundary, post, l);
		continue;
	    }
/* Step through the options */
	    for(s = value; *s; s = e) {
		char more;
		e = 0;
		if(t->multiple)
		    e = strchr(s, '\1');
		if(!e)
		    e = s + strlen(s);
		more = *e, *e = 0;
		postNameVal(name, s, fsep, false, boundary, post, l);
		if(more)
		    ++e;
	    }
	    nzFree(value);
	    continue;
	}

	if(itype == INP_FILE) {	/* the only one left */
	    value = fetchTextVar(t);
	    if(!value)
		continue;
	    if(!*value)
		continue;
	    if(!(form->post & form->mime)) {
		setError(MSG_FilePost);
		nzFree(value);
		goto fail;
	    }
	    rc = postNameVal(name, value, fsep, 3, boundary, post, l);
	    nzFree(value);
	    if(rc)
		continue;
	    goto fail;
	}
	/* file */
	errorPrint("@unexpected input type in submitForm()");

      success:
	postNameVal(name, value, fsep, false, boundary, post, l);
    }				/* loop over tags */

    if(form->mime) {		/* the last boundary */
	stringAndString(post, l, "--");
	stringAndString(post, l, boundary);
	stringAndString(post, l, "--\r\n");
    }

    if(noname)
	i_puts(MSG_UnnamedFields);
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

bool
infPush(int tagno, char **post_string)
{
    struct htmlTag **list = cw->tags;
    struct htmlTag *t = list[tagno];
    struct htmlTag *form;
    int itype;
    char *post, *section;
    int l, actlen;
    const char *action = 0;
    const char *prot;
    bool rc;

    *post_string = 0;

/* If the tag is actually a form, then infPush() was invoked
 * by form.submit().
 * Revert t back to 0, since there may be multiple submit buttons
 * on the form, and we don't know which one was pushed. */
    if(t->action == TAGACT_FORM) {
	form = t;
	t = 0;
	itype = INP_SUBMIT;
    } else {
	form = t->controller;
	itype = t->itype;
    }

    if(itype > INP_SUBMIT) {
	setError(MSG_NoButton);
	return false;
    }

    if(!form && itype != INP_BUTTON) {
	setError(MSG_NotInForm);
	return false;
    }

    if(t && tagHandler(t->seqno, "onclick")) {
	if(cw->jsdead)
	    runningError(itype ==
	       INP_BUTTON ? MSG_NJNoAction : MSG_NJNoOnclick);
	else {
	    rc = handlerGo(t->jv, "onclick");
	    jsdw();
	    if(!rc)
		return true;
	    if(js_redirects)
		return true;
	}
    }

    if(itype == INP_BUTTON) {
	if(!handlerPresent(t->jv, "onclick")) {
	    setError(MSG_ButtonNoJS);
	    return false;
	}
	return true;
    }

    if(itype == INP_RESET) {
/* Before we reset, run the onreset code */
	if(t && tagHandler(form->seqno, "onreset")) {
	    if(cw->jsdead)
		runningError(MSG_NJNoReset);
	    else {
		rc = handlerGo(form->jv, "onreset");
		jsdw();
		if(!rc)
		    return true;
		if(js_redirects)
		    return true;
	    }
	}			/* onreset */
	formReset(form);
	return true;
    }

    /* Before we submit, run the onsubmit code */
    if(t && tagHandler(form->seqno, "onsubmit")) {
	if(cw->jsdead)
	    runningError(MSG_NJNoSubmit);
	else {
	    rc = handlerGo(form->jv, "onsubmit");
	    jsdw();
	    if(!rc)
		return true;
	    if(js_redirects)
		return true;
	}
    }

    action = form->href;
/* But we defer to the java variable */
    if(form->jv) {
	const char *jh = get_property_url(form->jv, true);
	if(jh && (!action || !stringEqual(jh, action))) {
/* Tie action to the form tag, to plug a small memory leak */
	    nzFree(form->href);
	    form->href = resolveURL(getBaseHref(form->seqno), jh);
	    action = form->href;
	}
    }

/* if no action, the default is the current location */
    if(!action) {
	action = getBaseHref(form->seqno);
    }

    if(!action) {
	setError(MSG_FormNoURL);
	return false;
    }

    debugPrint(2, "* %s", action);

    prot = getProtURL(action);
    if(!prot) {
	setError(MSG_FormBadURL);
	return false;
    }

    if(stringEqualCI(prot, "javascript")) {
	if(cw->jsdead) {
	    setError(MSG_NJNoForm);
	    return false;
	}
	javaParseExecute(form->jv, action, 0, 0);
	jsdw();
	return true;
    }

    form->bymail = false;
    if(stringEqualCI(prot, "mailto")) {
	if(!validAccount(localAccount))
	    return false;
	form->bymail = true;
    } else if(stringEqualCI(prot, "http")) {
	if(form->secure) {
	    setError(MSG_BecameInsecure);
	    return false;
	}
    } else if(!stringEqualCI(prot, "https")) {
	setError(MSG_SubmitProtBad, prot);
	return false;
    }

    post = initString(&l);
    stringAndString(&post, &l, action);
    section = strchr(post, '#');
    if(section) {
	i_printf(MSG_SectionIgnored, section);
	*section = 0;
	l = strlen(post);
    }
    section = strpbrk(post, "?\1");
    if(section) {
	if(*section == '\1' || !(form->bymail | form->post)) {
	    debugPrint(3,
	       "the url already specifies some data, which will be overwritten by the data in this form");
	    *section = 0;
	    l = strlen(post);
	}
    }

    stringAndChar(&post, &l, (form->post ? '\1' : '?'));
    actlen = l;

    if(!formSubmit(form, t, &post, &l)) {
	nzFree(post);
	return false;
    }

    debugPrint(3, "%s %s", form->post ? "post" : "get", post + actlen);

/* Handle the mail method here and now. */
    if(form->bymail) {
	char *addr, *subj, *q;
	const char *tolist[2], *atlist[2];
	const char *name = form->name;
	int newlen = l - actlen;	/* the new string could be longer than post */
	decodeMailURL(action, &addr, &subj, 0);
	tolist[0] = addr;
	tolist[1] = 0;
	atlist[0] = 0;
	newlen += 9;		/* subject: \n */
	if(subj)
	    newlen += strlen(subj);
	else
	    newlen += 11 + (name ? strlen(name) : 1);
	++newlen;		/* null */
	++newlen;		/* encodeAttachment might append another nl */
	q = allocMem(newlen);
	if(subj)
	    sprintf(q, "subject:%s\n", subj);
	else
	    sprintf(q, "subject:html form(%s)\n", name ? name : "?");
	strcpy(q + strlen(q), post + actlen);
	nzFree(post);
	i_printf(MSG_MailSending, addr);
	sleep(1);
	rc = sendMail(localAccount, tolist, q, -1, atlist, 0, false);
	if(rc)
	    i_puts(MSG_MailSent);
	nzFree(addr);
	nzFree(subj);
	nzFree(q);
	*post_string = 0;
	return rc;
    }

    *post_string = post;
    return true;
}				/* infPush */

/* I don't have any reverse pointers, so I'm just going to scan the list */
static struct htmlTag *
tagFromJavaVar(void *v)
{
    struct htmlTag **list = cw->tags;
    struct htmlTag *t;
    if(!list)
	errorPrint("@list = 0 in tagFromJavaVar()");
    while(t = *list++)
	if(t->jv == v)
	    break;
    if(!t)
	runningError(MSG_LostTag);
    return t;
}				/* tagFromJavaVar */

/* Javascript has changed an input field */
void
javaSetsTagVar(void *v, const char *val)
{
    struct htmlTag *t;
    buildTagArray();
    t = tagFromJavaVar(v);
    if(!t)
	return;
/* ok, we found it */
    if(t->itype == INP_HIDDEN || t->itype == INP_RADIO) {
	return;
    }
    if(t->itype == INP_TA) {
	runningError(MSG_JSTextarea);
	return;
    }
    updateFieldInBuffer(t->seqno, val, parsePage ? 0 : 2, false);
}				/* javaSetsTagVar */

/* Return false to stop javascript, due to a url redirect */
void
javaSubmitsForm(void *v, bool reset)
{
    char *post;
    bool rc;
    struct htmlTag *t;

    buildTagArray();
    t = tagFromJavaVar(v);
    if(!t)
	return;

    if(reset) {
	formReset(t);
	return;
    }

    rc = infPush(t->seqno, &post);
    if(!rc) {
	showError();
	return;
    }
    gotoLocation(post, 0, false);
}				/* javaSubmitsForm */

void
javaOpensWindow(const char *href, const char *name)
{
    struct htmlTag *t;
    char *copy, *r;
    const char *a;

    if(!href || !*href) {
	browseError(MSG_JSBlankWindow);
	return;
    }

    copy = cloneString(href);
    unpercentURL(copy);
    r = resolveURL(getBaseHref(-1), copy);
    nzFree(copy);
    if(!parsePage) {
	gotoLocation(r, 0, false);
	return;
    }

    t = newTag("a");
    t->href = r;
    a = altText(r);
/* I'll assume this is more helpful than the name of the window */
    if(a)
	name = a;
    toPreamble(t->seqno, "Popup", 0, name);
}				/* javaOpensWindow */

void
javaSetsTimeout(int n, const char *jsrc, void *to, bool isInterval)
{
    struct htmlTag *t = newTag("A");
    char timedesc[48];
    int l;

    strcpy(timedesc, (isInterval ? "Interval" : "Timer"));
    l = strlen(timedesc);
    if(n > 1000)
	sprintf(timedesc + l, " %d", n / 1000);
    else
	sprintf(timedesc + l, " %dms", n);

    t->jv = to;
    t->href = cloneString("#");
    toPreamble(t->seqno, timedesc, jsrc, 0);
}				/* javaSetsTimeout */
