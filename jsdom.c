/* jsdom.c
* Javascript support, the connection to spider monkey JS.
* Copyright (c) Karl Dahlke, 2006
* This file is part of the edbrowse project, released under GPL.
* This file contains the basics of the document object model.
 * The Spider Monkey Javascript compiler/engine is released by Mozilla,
 * under the MPL.  Install and build that package under /usr/local.
 * ftp://ftp.mozilla.org/pub/mozilla.org/js/js-1.5.tar.gz
*/

#include "eb.h"

#include "jsapi.h"
#include "jsprf.h"

#define PROP_FIXED (JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT)


static JSRuntime *jrt;
void *jcx;			/* really JSContext */
void *jwin;			/* window object, really JSObject */
void *jdoc;			/* window.document, really JSObject */
void *jwloc;			/* window.location, really JSObject */
static size_t gStackChunkSize = 8192;
static FILE *gOutFile, *gErrFile;

static void
my_ErrorReporter(JSContext * cx, const char *message, JSErrorReport * report)
{
    char *prefix, *tmp;

    if(!report) {
	fprintf(gErrFile, "%s\n", message);
	return;
    }

/* Conditionally ignore reported warnings. */
    if(JSREPORT_IS_WARNING(report->flags)) {
	if(ismc | browseLocal)
	    return;
    }

    prefix = NULL;
    if(report->filename)
	prefix = JS_smprintf("%s:", report->filename);
    if(report->lineno) {
	tmp = prefix;
	prefix = JS_smprintf("%s%u: ", tmp ? tmp : "", report->lineno);
	JS_free(cx, tmp);
    }
    if(JSREPORT_IS_WARNING(report->flags)) {
	tmp = prefix;
	prefix = JS_smprintf("%s%swarning: ",
	   tmp ? tmp : "", JSREPORT_IS_STRICT(report->flags) ? "strict " : "");
	JS_free(cx, tmp);
    }

    if(prefix)
	fputs(prefix, gErrFile);
    fprintf(gErrFile, "%s\n", message);

    JS_free(cx, prefix);
}				/* my_ErrorReporter */


/*********************************************************************
Set up the javascript runtime world, as used by edbrowse.
*********************************************************************/

void
js_eb_setup(void)
{
/* 10 meg js space - should this be configurable? */
    jrt = JS_NewRuntime(4L * 1024L * 1024L);
    if(!jrt)
	errorPrint("2could not allocate memory for javascript operations.");
    gOutFile = stdout;
    gErrFile = stderr;
}				/* js_eb_setup */


/*********************************************************************
When an element is created without a name, it is not linked to its
owner (via that name), and can be cleared via garbage collection.
This is a disaster!
Create a fake name, so we can attach the element.
*********************************************************************/

static const char *
fakePropName(void)
{
    static char fakebuf[20];
    static int idx = 0;
    ++idx;
    sprintf(fakebuf, "gc$away%d", idx);
    return fakebuf;
}				/*fakePropName */


/*********************************************************************
Here come the classes, and their inbuilt methods, for the document object model.
Start with window and document.
*********************************************************************/

static JSClass window_class = {
    "Window",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSBool
window_ctor(JSContext * cx, JSObject * obj, uintN argc, jsval * argv,
   jsval * rval)
{
    const char *newloc = 0;
    const char *winname = 0;
    JSString *str;
    if(argc > 0 && (str = JS_ValueToString(jcx, argv[0]))) {
	newloc = JS_GetStringBytes(str);
    }
    if(argc > 1 && (str = JS_ValueToString(jcx, argv[1]))) {
	winname = JS_GetStringBytes(str);
    }
/* third argument is attributes, like window size and location, that we don't care about. */
    javaOpensWindow(newloc, winname);
    if(!parsePage)
	return JS_FALSE;
    JS_DefineProperty(jcx, obj, "opener",
       OBJECT_TO_JSVAL(jwin), NULL, NULL, PROP_FIXED);
    return JS_TRUE;
}				/* window_ctor */

/* for window.focus etc */
static JSBool
nullFunction(JSContext * cx, JSObject * obj, uintN argc, jsval * argv,
   jsval * rval)
{
    return JS_TRUE;
}				/* nullFunction */

static JSBool
falseFunction(JSContext * cx, JSObject * obj, uintN argc, jsval * argv,
   jsval * rval)
{
    *rval = JSVAL_FALSE;
    return JS_TRUE;
}				/* falseFunction */

static JSBool
trueFunction(JSContext * cx, JSObject * obj, uintN argc, jsval * argv,
   jsval * rval)
{
    *rval = JSVAL_TRUE;
    return JS_TRUE;
}				/* trueFunction */

static JSBool
setAttribute(JSContext * cx, JSObject * obj, uintN argc, jsval * argv,
   jsval * rval)
{
    if(argc != 2 || !JSVAL_IS_STRING(argv[0])) {
	JS_ReportError(jcx, "unexpected arguments to setAttribute()");
    } else {
	const char *prop = stringize(argv[0]);
	JS_DefineProperty(jcx, obj, prop, argv[1], NULL, NULL, PROP_FIXED);
    }
    return JS_TRUE;
}				/* setAttribute */

static JSBool
appendChild(JSContext * cx, JSObject * obj, uintN argc, jsval * argv,
   jsval * rval)
{
    JSObject *elar;		/* elements array */
    jsuint length;
    jsval v;
    JS_GetProperty(jcx, obj, "elements", &v);
    elar = JSVAL_TO_OBJECT(v);
    JS_GetArrayLength(jcx, elar, &length);
    JS_DefineElement(jcx, elar, length,
       (argc > 0 ? argv[0] : JSVAL_NULL), NULL, NULL, JSPROP_ENUMERATE);
    return JS_TRUE;
}				/* appendChild */

static JSBool
win_close(JSContext * cx, JSObject * obj, uintN argc, jsval * argv,
   jsval * rval)
{
/* It's too confusing to just close the window */
    puts("this page is finished, please use your back key or quit");
    cw->jsdead = true;
    return JS_TRUE;
}				/* win_close */

static JSBool
win_alert(JSContext * cx, JSObject * obj, uintN argc, jsval * argv,
   jsval * rval)
{
    const char *msg;
    JSString *str;
    if(argc > 0 && (str = JS_ValueToString(jcx, argv[0]))) {
	msg = JS_GetStringBytes(str);
    }
    if(msg)
	puts(msg);
    return JS_TRUE;
}				/* win_alert */

static JSBool
win_prompt(JSContext * cx, JSObject * obj, uintN argc, jsval * argv,
   jsval * rval)
{
    const char *msg = 0;
    const char *answer = 0;
    JSString *str;
    char inbuf[80];
    char *s;
    char c;

    if(argc > 0 && (str = JS_ValueToString(jcx, argv[0]))) {
	msg = JS_GetStringBytes(str);
    }
    if(argc > 1 && (str = JS_ValueToString(jcx, argv[1]))) {
	answer = JS_GetStringBytes(str);
    }

    if(!msg)
	msg = "";
    printf("%s", msg);
/* If it doesn't end in space or question mark, print a colon */
    c = 'x';
    if(*msg)
	c = msg[strlen(msg) - 1];
    if(!isspace(c)) {
	if(!ispunct(c))
	    printf(":");
	printf(" ");
    }
    if(answer)
	printf("[%s] ", answer);
    fflush(stdout);
    if(!fgets(inbuf, sizeof (inbuf), stdin))
	exit(1);
    s = inbuf + strlen(inbuf);
    if(s > inbuf && s[-1] == '\n')
	*--s = 0;
    if(inbuf[0])
	answer = inbuf;
    if(!answer)
	answer = "";
    *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(jcx, answer));
    return JS_TRUE;
}				/* win_prompt */

static JSBool
win_confirm(JSContext * cx, JSObject * obj, uintN argc, jsval * argv,
   jsval * rval)
{
    const char *msg = 0;
    JSString *str;
    char inbuf[80];
    char *s;
    char c;
    bool first = true;

    if(argc > 0 && (str = JS_ValueToString(jcx, argv[0]))) {
	msg = JS_GetStringBytes(str);
    }

    if(!msg)
	msg = "";
    while(true) {
	printf("%s", msg);
	c = 'x';
	if(*msg)
	    c = msg[strlen(msg) - 1];
	if(!isspace(c)) {
	    if(!ispunct(c))
		printf(":");
	    printf(" ");
	}
	if(!first)
	    printf("[y|n] ");
	first = false;
	fflush(stdout);
	if(!fgets(inbuf, sizeof (inbuf), stdin))
	    exit(1);
	c = *inbuf;
	if(c && strchr("nNyY", c))
	    break;
    }

    c = tolower(c);
    if(c == 'y')
	*rval = JSVAL_TRUE;
    else
	*rval = JSVAL_FALSE;
    return JS_TRUE;
}				/* win_confirm */

static JSBool
win_sto(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
    if(!parsePage) {
	JS_ReportError(jcx,
	   "cannot use setTimeout() to delay the execution of a function");
    } else if(argc == 2 && JSVAL_IS_STRING(argv[0]) && JSVAL_IS_INT(argv[1])) {
	const char *s = stringize(argv[0]);
	int n = JSVAL_TO_INT(argv[1]);
	javaSetsTimeout(s, n);
    } else {
	JS_ReportError(jcx, "invalid arguments to setTimeout()");
    }
    return JS_TRUE;
}				/* win_sto */

static JSFunctionSpec window_methods[] = {
    {"alert", win_alert, 1, 0, 0},
    {"prompt", win_prompt, 2, 0, 0},
    {"confirm", win_confirm, 1, 0, 0},
    {"setTimeout", win_sto, 2, 0, 0},
    {"open", window_ctor, 3, 0, 0},
    {"close", win_close, 0, 0, 0},
    {"focus", nullFunction, 0, 0, 0},
    {"blur", nullFunction, 0, 0, 0},
    {"scroll", nullFunction, 0, 0, 0},
    {0}
};

static JSClass doc_class = {
    "Document",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static void
dwrite2(const char *s)
{
    if(!cw->dw) {
	cw->dw = initString(&cw->dw_l);
	stringAndString(&cw->dw, &cw->dw_l, "<docwrite>");
    }
    stringAndString(&cw->dw, &cw->dw_l, s);
}				/* dwrite2 */

static void
dwrite1(uintN argc, jsval * argv, bool newline)
{
    int i;
    const char *msg;
    JSString *str;
    for(i = 0; i < argc; ++i) {
	if((str = JS_ValueToString(jcx, argv[i])) &&
	   (msg = JS_GetStringBytes(str)))
	    dwrite2(msg);
/* I assume I don't have to free msg?? */
    }
    if(newline)
	dwrite2("\n");
}				/* dwrite1 */

static JSBool
doc_write(JSContext * cx, JSObject * obj, uintN argc, jsval * argv,
   jsval * rval)
{
    dwrite1(argc, argv, false);
    return JS_TRUE;
}				/* doc_write */

static JSBool
setter_innerHTML(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
    const char *s = stringize(*vp);
    if(s && strlen(s)) {
	dwrite2(parsePage ? "<hr>\n" : "<html>\n");
	dwrite2(s);
	if(s[strlen(s) - 1] != '\n')
	    dwrite2("\n");
    }
/* The string has already been updated in the object. */
    return JS_TRUE;
}				/* setter_innerHTML */

static JSBool
setter_innerText(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
    jsval v = *vp;
    const char *s;
    if(!JSVAL_IS_STRING(v))
	return JS_FALSE;
    s = JS_GetStringBytes(JSVAL_TO_STRING(v));
    puts("Sorry, innerText update not yet implemented.");
/* The string has already been updated in the object. */
    return JS_TRUE;
}				/* setter_innerText */

static JSBool
doc_writeln(JSContext * cx, JSObject * obj, uintN argc, jsval * argv,
   jsval * rval)
{
    dwrite1(argc, argv, true);
    return JS_TRUE;
}				/* doc_writeln */

static JSFunctionSpec doc_methods[] = {
    {"focus", nullFunction, 0, 0, 0},
    {"blur", nullFunction, 0, 0, 0},
    {"open", nullFunction, 0, 0, 0},
    {"close", nullFunction, 0, 0, 0},
    {"write", doc_write, 0, 0, 0},
    {"writeln", doc_writeln, 0, 0, 0},
    {0}
};

static JSClass element_class = {
    "Element",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSFunctionSpec element_methods[] = {
    {"focus", nullFunction, 0, 0, 0},
    {"blur", nullFunction, 0, 0, 0},
    {0}
};

static JSClass form_class = {
    "Form",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSBool
form_submit(JSContext * cx, JSObject * obj, uintN argc, jsval * argv,
   jsval * rval)
{
    javaSubmitsForm(obj, false);
    return JS_TRUE;
}				/* form_submit */

static JSBool
form_reset(JSContext * cx, JSObject * obj, uintN argc, jsval * argv,
   jsval * rval)
{
    javaSubmitsForm(obj, true);
    return JS_TRUE;
}				/* form_reset */

static JSFunctionSpec form_methods[] = {
    {"submit", form_submit, 0, 0, 0},
    {"reset", form_reset, 0, 0, 0},
    {0}
};

static JSClass body_class = {
    "Body",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSFunctionSpec body_methods[] = {
    {"setAttribute", setAttribute, 2, 0, 0},
    {"appendChild", appendChild, 1, 0, 0},
    {0}
};

static JSClass head_class = {
    "Head",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSFunctionSpec head_methods[] = {
    {"setAttribute", setAttribute, 2, 0, 0},
    {"appendChild", appendChild, 1, 0, 0},
    {0}
};

/* Don't be confused; this is for <link>, not <a> */
static JSClass link_class = {
    "Link",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSFunctionSpec link_methods[] = {
    {"setAttribute", setAttribute, 2, 0, 0},
    {0}
};

static JSClass image_class = {
    "Image",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSClass frame_class = {
    "Frame",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSClass anchor_class = {
    "Anchor",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSClass table_class = {
    "Table",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSClass trow_class = {
    "Trow",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSClass div_class = {
    "Div",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSClass span_class = {
    "Span",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSClass area_class = {
    "Area",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSClass option_class = {
    "Option",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

struct DOMCLASS {
    JSClass *class;
    JSFunctionSpec *methods;
      JSBool(*constructor) (JSContext *, JSObject *, uintN, jsval *, jsval *);
    int nargs;
};

static struct DOMCLASS domClasses[] = {
    {&element_class, element_methods, nullFunction, 0},
    {&form_class, form_methods},
    {&body_class, body_methods},
    {&head_class, head_methods},
    {&link_class, link_methods, nullFunction, 0},
    {&image_class, 0, nullFunction, 1},
    {&frame_class},
    {&anchor_class, 0, nullFunction, 1},
    {&table_class},
    {&div_class},
    {&area_class},
    {&span_class},
    {&trow_class},
    {&option_class},
    {0}
};

static const char *docarrays[] = {
    "frames", "heads", "bodies", "links", "tables", "divs", "spans",
    "forms", "images", "areas", 0
};


/* Some things are just easier in javascript */
static const char initScript[] = "document.all.tags = function(s) { \n\
switch(s.toLowerCase()) { \n\
case 'form': return document.forms; \n\
case 'table': return document.tables; \n\
case 'div': return document.divs; \n\
case 'a': return document.anchors; \n\
case 'img': case 'image': return document.images; \n\
case 'span': return document.spans; \n\
case 'head': return document.heads; \n\
case 'body': return document.bodies; \n\
default: alert('all.tags default'); return new Array(); }} \n\
\n\
document.getElementById = function(s) { \n\
return document.idMaster[s]; } \n\
\n\
document.getElementsByTagName = function(s) { \n\
return document.all.tags(s); }\n\
\n\
document.createElement = function(s) { \n\
switch(s.toLowerCase()) { \n\
case 'link': return new Link();\n\
case 'image': case 'img': return new Image();\n\
default: alert('createElement default'); return new Object(); }} \n\
\n\
URL.prototype.indexOf = function(s) { \n\
return this.toString().indexOf(s); }\n\
\n\
URL.prototype.lastIndexOf = function(s) { \n\
return this.toString().lastIndexOf(s); }\n\
\n\
URL.prototype.substring = function(from, to) { \n\
return this.toString().substring(from, to); }\n\
\n\
URL.prototype.toLowerCase = function() { \n\
return this.toString().toLowerCase(); }\n\
\n\
URL.prototype.toUpperCase = function() { \n\
return this.toString().toUpperCase(); }\n\
\n\
history.toString = function() { \n\
return 'Sorry, edbrowse does not maintain a browsing history.'; } \
";

void *
createJavaContext(void)
{
    JSObject *o, *nav, *screen, *hist;
    const char *itemname;
    int i;
    char verx11[20];
    jsval rval;

    jcx = JS_NewContext(jrt, gStackChunkSize);
    if(!jcx)
	errorPrint("2unable to create javascript context");
    JS_SetErrorReporter(jcx, my_ErrorReporter);
    JS_SetOptions(jcx, JSOPTION_VAROBJFIX);

/* Create the Window object, which is the global object in DOM. */
    jwin = JS_NewObject(jcx, &window_class, NULL, NULL);
    if(!jwin)
	errorPrint("2unable to create window object for javascript");
    JS_InitClass(jcx, jwin, 0, &window_class, window_ctor, 3,
       NULL, window_methods, NULL, NULL);
/* Ok, but the global object was created before the class,
 * so it doesn't have its methods yet. */
    JS_DefineFunctions(jcx, jwin, window_methods);

/* Math, Date, Number, String, etc */
    if(!JS_InitStandardClasses(jcx, jwin))
	errorPrint("2unable to create standard classes for javascript");

    JS_DefineProperty(jcx, jwin, "window",
       OBJECT_TO_JSVAL(jwin), NULL, NULL, PROP_FIXED);
    JS_DefineProperty(jcx, jwin, "self",
       OBJECT_TO_JSVAL(jwin), NULL, NULL, PROP_FIXED);
    JS_DefineProperty(jcx, jwin, "parent",
       OBJECT_TO_JSVAL(jwin), NULL, NULL, PROP_FIXED);
    JS_DefineProperty(jcx, jwin, "top",
       OBJECT_TO_JSVAL(jwin), NULL, NULL, PROP_FIXED);

/* Some visual attributes of the window.
 * These are just guesses.
 * Better to have something, than to leave them undefined. */
    establish_property_number(jwin, "height", 768, true);
    establish_property_number(jwin, "width", 1024, true);
    establish_property_string(jwin, "status", 0, false);
    establish_property_string(jwin, "defaultStatus", 0, false);
    establish_property_bool(jwin, "returnValue", true, false);
    establish_property_bool(jwin, "menubar", true, false);
    establish_property_bool(jwin, "scrollbars", true, false);
    establish_property_bool(jwin, "toolbar", true, false);
    establish_property_bool(jwin, "resizable", true, false);
    establish_property_bool(jwin, "directories", false, false);

/* Other classes that we'll need. */
    for(i = 0; domClasses[i].class; ++i) {
	JS_InitClass(jcx, jwin, 0, domClasses[i].class,
	   domClasses[i].constructor, domClasses[i].nargs,
	   NULL, domClasses[i].methods, NULL, NULL);
    }

    initLocationClass();

/* document under window */
    JS_InitClass(jcx, jwin, 0, &doc_class, NULL, 0,
       NULL, doc_methods, NULL, NULL);
    jdoc = JS_NewObject(jcx, &doc_class, NULL, jwin);
    if(!jdoc)
	errorPrint("2unable to create document object for javascript");
    JS_DefineProperty(jcx, jwin, "document",
       OBJECT_TO_JSVAL(jdoc), NULL, NULL, PROP_FIXED);

    establish_property_string(jdoc, "bgcolor", "white", false);
    establish_property_string(jdoc, "cookie", 0, false);
    establish_property_string(jdoc, "referrer", cw->referrer, true);
    establish_property_url(jdoc, "URL", cw->fileName, true);
    establish_property_url(jdoc, "location", cw->fileName, true);
    establish_property_url(jwin, "location", firstURL(), false);
    establish_property_string(jdoc, "domain", getHostURL(cw->fileName), false);

/* create arrays under document */
    for(i = 0; itemname = docarrays[i]; ++i)
	establish_property_array(jdoc, itemname);

    o = JS_NewObject(jcx, 0, 0, jdoc);
    JS_DefineProperty(jcx, jdoc, "idMaster",
       OBJECT_TO_JSVAL(o), NULL, NULL, PROP_FIXED);
    o = JS_NewObject(jcx, 0, 0, jdoc);
    JS_DefineProperty(jcx, jdoc, "all",
       OBJECT_TO_JSVAL(o), NULL, NULL, PROP_FIXED);

    nav = JS_NewObject(jcx, 0, 0, jwin);
    JS_DefineProperty(jcx, jwin, "navigator",
       OBJECT_TO_JSVAL(nav), NULL, NULL, PROP_FIXED);

/* attributes of the navigator */
    establish_property_string(nav, "appName", "edbrowse", true);
    establish_property_string(nav, "appCode Name", "edbrowse C/SMJS", true);
/* Use X11 to indicate unix/linux.  Sort of a standard */
    sprintf(verx11, "%s%s", version, "-X11");
    establish_property_string(nav, "appVersion", version, true);
    establish_property_string(nav, "userAgent", currentAgent, true);
    establish_property_string(nav, "oscpu", currentOS(), true);
    establish_property_string(nav, "platform", currentMachine(), true);
    establish_property_string(nav, "product", "smjs", true);
    establish_property_string(nav, "productSub", "1.5", true);
    establish_property_string(nav, "vendor", "eklhad", true);
    establish_property_string(nav, "vendorSub", version, true);
/* We need to locale-ize the next one */
    establish_property_string(nav, "userLanguage", "english", true);
    establish_property_string(nav, "language", "english", true);
    establish_property_array(nav, "plugins");
    establish_property_array(nav, "mimeTypes");
/* And we need to fill in the mime types that edbrowse understands, I guess */
    JS_DefineFunction(jcx, nav, "javaEnabled", falseFunction, 0, PROP_FIXED);
    JS_DefineFunction(jcx, nav, "taintEnabled", falseFunction, 0, PROP_FIXED);
    establish_property_bool(nav, "cookieEnabled", true, true);
    establish_property_bool(nav, "onLine", true, true);

    screen = JS_NewObject(jcx, 0, 0, jwin);
    JS_DefineProperty(jcx, jwin, "screen",
       OBJECT_TO_JSVAL(screen), NULL, NULL, PROP_FIXED);

/* attributes of the screen */
    establish_property_number(screen, "height", 768, true);
    establish_property_number(screen, "width", 1024, true);
    establish_property_number(screen, "availHeight", 768, true);
    establish_property_number(screen, "availWidth", 1024, true);
    establish_property_number(screen, "availTop", 0, true);
    establish_property_number(screen, "availLeft", 0, true);

    hist = JS_NewObject(jcx, 0, 0, jwin);
    JS_DefineProperty(jcx, jwin, "history",
       OBJECT_TO_JSVAL(hist), NULL, NULL, PROP_FIXED);

/* attributes of history */
    establish_property_string(hist, "current", cw->fileName, true);
/* There's no history in edbrowse. */
/* Only the current file is known, hence length is 1. */
    establish_property_number(hist, "length", 1, true);
    establish_property_string(hist, "next", 0, true);
    establish_property_string(hist, "previous", 0, true);
    JS_DefineFunction(jcx, hist, "back", nullFunction, 0, PROP_FIXED);
    JS_DefineFunction(jcx, hist, "forward", nullFunction, 0, PROP_FIXED);
    JS_DefineFunction(jcx, hist, "go", nullFunction, 0, PROP_FIXED);

/* Set up some things in javascript */
    JS_EvaluateScript(jcx, jwin, initScript, strlen(initScript),
       "initScript", 1, &rval);

    return jcx;
}				/* createJavaContext */

void
freeJavaContext(void *jsc)
{
    if(jsc)
	JS_DestroyContext((JSContext *) jsc);
}				/* freeJavaContext */

void
establish_innerHTML(void *jv, const char *start, const char *end, bool is_ta)
{
    JSObject *obj = jv;
    if(!obj)
	return;
/* null start means the pointer has been corrupted by a document.write() call */
    if(!start)
	start = end = EMPTYSTRING;
    JS_DefineProperty(jcx, obj, "innerHTML",
       STRING_TO_JSVAL(JS_NewStringCopyN(jcx, start, end - start)),
       NULL, (is_ta ? setter_innerText : setter_innerHTML),
       JSPROP_ENUMERATE | JSPROP_PERMANENT);
    if(is_ta) {
	JS_DefineProperty(jcx, obj, "innerText",
	   STRING_TO_JSVAL(JS_NewStringCopyN(jcx, start, end - start)),
	   NULL, setter_innerText, JSPROP_ENUMERATE | JSPROP_PERMANENT);
    }
}				/* establish_innerHTML */

void
jMyContext(void)
{
    jsval oval;
    jcx = cw->jsc;
    if(jcx) {
	jwin = JS_GetGlobalObject(jcx);
	JS_GetProperty(jcx, jwin, "document", &oval);
	jdoc = JSVAL_TO_OBJECT(oval);
	JS_GetProperty(jcx, jwin, "location", &oval);
	jwloc = JSVAL_TO_OBJECT(oval);
    } else
	jwin = jdoc = jwloc = 0;
}				/* jMyContext */

bool
javaParseExecute(void *this, const char *str, const char *filename, int lineno)
{
    JSBool ok;
    bool rc;
    jsval rval;

    debugPrint(6, "javascript:\n%s", str);
    ok = JS_EvaluateScript(jcx, this, str, strlen(str),
       filename, lineno, &rval);
    rc = true;
    if(JSVAL_IS_BOOLEAN(rval))
	rc = JSVAL_TO_BOOLEAN(rval);
    JS_GC(jcx);
    return rc;
}				/* javaParseExecute */

/* link a frame, span, anchor, etc to the document object model */
void *
domLink(const char *classname,	/* instantiate this class */
   const char *symname, const char *idname, const char *href, const char *href_url, const char *list,	/* next member of this array */
   void *owner, bool isradio)
{
    JSObject *v = 0, *w, *alist = 0;
    jsval vv, listv;
    jsuint length, attr = PROP_FIXED;
    JSClass *cp;
    int i;

    if(cw->jsdead)
	return 0;

/* find the class */
    for(i = 0; cp = domClasses[i].class; ++i)
	if(stringEqual(cp->name, classname))
	    break;

    if(isradio) {
	JSBool found;
	JS_HasProperty(jcx, owner, symname, &found);
	if(found) {
	    JS_GetProperty(jcx, owner, symname, &vv);
	    v = JSVAL_TO_OBJECT(vv);
	}
    }

    if(!v) {
	if(isradio) {
	    v = JS_NewArrayObject(jcx, 0, NULL);
	    establish_property_string(v, "type", "radio", true);
	} else
	    v = JS_NewObject(jcx, cp, NULL, owner);
	vv = OBJECT_TO_JSVAL(v);

	if(symname) {
	    JS_DefineProperty(jcx, owner, symname, vv, NULL, NULL, attr);
	} else {
	    JS_DefineProperty(jcx, owner, fakePropName(), vv,
	       NULL, NULL, JSPROP_READONLY | JSPROP_PERMANENT);
	}

	if(list) {
	    JS_GetProperty(jcx, owner, list, &listv);
	    alist = JSVAL_TO_OBJECT(listv);
	}
	if(alist) {
	    JS_GetArrayLength(jcx, alist, &length);
	    JS_DefineElement(jcx, alist, length, vv, NULL, NULL, attr);
	}			/* list indicated */
    }

    if(isradio) {
/* drop down to the element within the radio array, and return that element */
/* w becomes the object associated with this radio button */
/* v is, by assumption, an array */
	JS_GetArrayLength(jcx, v, &length);
	w = JS_NewObject(jcx, &element_class, NULL, owner);
	vv = OBJECT_TO_JSVAL(w);
	JS_DefineElement(jcx, v, length, vv, NULL, NULL, attr);
	v = w;
    }

    if(symname)
	establish_property_string(v, "name", symname, true);
    if(idname) {
/* v.id becomes idname, and idMaster.idname becomes v */
	establish_property_string(v, "id", idname, true);
	JS_GetProperty(jcx, jdoc, "idMaster", &listv);
	JSObject *master = JSVAL_TO_OBJECT(listv);
	JS_DefineProperty(jcx, master, idname, vv, NULL, NULL, attr);
    }

    if(href && href_url) {
	establish_property_url(v, href, href_url, false);
    }

    if(cp == &element_class) {
/* link back to the form that owns the element */
	JS_DefineProperty(jcx, v, "form",
	   OBJECT_TO_JSVAL(owner), NULL, NULL, attr);
    }

    return v;
}				/* domLink */
