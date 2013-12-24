/* jsdom.c
* Javascript support, the connection to spider monkey JS.
* Copyright (c) Karl Dahlke, 2008
* This file is part of the edbrowse project, released under GPL.
* This file contains the basics of the document object model.
 * The Spider Monkey Javascript compiler/engine is released by Mozilla,
 * under the MPL.  Install and build that package under /usr/local.
 * ftp://ftp.mozilla.org/pub/mozilla.org/js/js-1.5.tar.gz
*/

#include "eb.h"

#include "js.h"

/* jsprf.h is not publically visible on some systems,
so I can't #include it here.
Instead, I'll declare the needed prototype myself, and hope it is consistent
with whatever smjs you are using. */
extern
JS_PUBLIC_API(char *)
JS_smprintf(const char *fmt, ...);

#define PROP_FIXED (JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT)


JSContext *jcx;			/* really JSContext */
void *jwin;			/* window object, really JSObject */
void *jdoc;			/* window.document, really JSObject */
JSObject *jwloc;		/* window.location, really JSObject */
JSObject *jdloc;		/* document.location, really JSObject */
static size_t gStackChunkSize = 8192;
static FILE *gOutFile, *gErrFile;
static const char *emptyParms[] = { 0 };
static jsval emptyArgs[] = { 0 };

static void
my_ErrorReporter(JSContext * cx, const char *message, JSErrorReport * report)
{
    char *prefix, *tmp;

    if(debugLevel < 2)
	goto done;
    if(ismc)
	goto done;

    if(!report) {
	fprintf(gErrFile, "%s\n", message);
	goto done;
    }

/* Conditionally ignore reported warnings. */
    if(JSREPORT_IS_WARNING(report->flags)) {
	if(browseLocal)
	    goto done;
    }

    prefix = NULL;
    if(report->filename)
	prefix = JS_smprintf("%s:", report->filename);
    if(report->lineno) {
	tmp = prefix;
	prefix = JS_smprintf("%s%u: ", tmp ? tmp : "", report->lineno);
	if(tmp)
	    JS_free(cx, tmp);
    }
    if(JSREPORT_IS_WARNING(report->flags)) {
	tmp = prefix;
	prefix = JS_smprintf("%s%swarning: ",
	   tmp ? tmp : "", JSREPORT_IS_STRICT(report->flags) ? "strict " : "");
	if(tmp)
	    JS_free(cx, tmp);
    }

    if(prefix)
	fputs(prefix, gErrFile);
    fprintf(gErrFile, "%s\n", message);

    if(prefix)
	JS_free(cx, prefix);

  done:
    report->flags = 0;
}				/* my_ErrorReporter */

JSString *
our_JS_NewStringCopyN(JSContext * cx, const char *s, size_t n)
{
    char *converted = NULL;
    int converted_l = 0;
    const char *outbytes = s;
    int outbytes_l = n;
    JSString *forSpidermonkey = NULL;

    if(!JS_CStringsAreUTF8())
/* Fixme this is too simple.  We need to decode UTF8 to JSCHAR, for proper
 * unicode handling.  E.G., JS C strings are not UTF8, but the user has
 * a UTF8 locale. */
	return JS_NewStringCopyN(jcx, s, n);

    if(!cons_utf8) {
/* The string should not be UTF8, because of edbrowse's conversion. */
	iso2utf(s, n, &converted, &converted_l);
	outbytes = converted;
	outbytes_l = converted_l;
    }

    forSpidermonkey = JS_NewStringCopyN(jcx, outbytes, outbytes_l);
    nzFree(converted);

    return forSpidermonkey;
}				/* our_JS_NewStringCopyN */
JSString *
our_JS_NewStringCopyZ(JSContext * cx, const char *s)
{
    size_t len = strlen(s);
    return our_JS_NewStringCopyN(jcx, s, len);
}				/* our_JS_NewStringCopyZ */

char *
our_JSEncodeString(JSString * str)
{
    size_t encodedLength = JS_GetStringEncodingLength(jcx, str);
    char *buffer = allocMem(encodedLength + 1);
    buffer[encodedLength] = '\0';
    size_t result = JS_EncodeStringToBuffer(str, buffer, encodedLength);
    if(result == (size_t) - 1)
	i_printfExit(MSG_JSFailure);
    return buffer;
}				/* our_JSEncodeString */

static char *
transcode_get_js_bytes(JSString * s)
{
    char *converted = NULL;
    int converted_l = 0;
    char *origbytes = our_JSEncodeString(s);

    if(!JS_CStringsAreUTF8())
	return origbytes;

    if(cons_utf8)
	return origbytes;

    utf2iso(origbytes, strlen(origbytes), &converted, &converted_l);
    nzFree(origbytes);
    return converted;
}				/* our_JS_GetTranscodedBytes */

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
    JSCLASS_HAS_PRIVATE | JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSBool
window_ctor(JSContext * cx, uintN argc, jsval * vp)
{
    char *newloc = 0;
    const char *winname = 0;
    JSString *str;
    jsval *argv = JS_ARGV(cx, vp);
    JSObject *newwin = JS_NewObjectForConstructor(cx, vp);
    if(argc > 0 && (str = JS_ValueToString(jcx, argv[0]))) {
	newloc = our_JSEncodeString(str);
    }
    if(argc > 1 && (str = JS_ValueToString(jcx, argv[1]))) {
	winname = transcode_get_js_bytes(str);
    }
/* third argument is attributes, like window size and location, that we don't care about. */
    javaOpensWindow(newloc, winname);
    if(newloc)
	nzFree(newloc);

    if(!parsePage)
	return JS_FALSE;
    establish_property_object(newwin, "opener", jwin);
    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(newwin));
    return JS_TRUE;
}				/* window_ctor */

/* window.open() instantiates a new window object */
static JSBool
win_open(JSContext * cx, uintN argc, jsval * vp)
{
    jsval *argv = JS_ARGV(cx, vp);
    JSObject *newwin = JS_ConstructObjectWithArguments(jcx,
       &window_class, 0, jwin, argc, argv);
    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(newwin));
    return JS_TRUE;
}				/* win_open */

/* for window.focus etc */
static JSBool
nullFunction(JSContext * cx, uintN argc, jsval * vp)
{
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}				/* nullFunction */

static JSBool
falseFunction(JSContext * cx, uintN argc, jsval * vp)
{
    JS_SET_RVAL(cx, vp, JSVAL_FALSE);
    return JS_TRUE;
}				/* falseFunction */

static JSBool
trueFunction(JSContext * cx, uintN argc, jsval * vp)
{
    JS_SET_RVAL(cx, vp, JSVAL_TRUE);
    return JS_TRUE;
}				/* trueFunction */

static JSBool
setAttribute(JSContext * cx, uintN argc, jsval * vp)
{
    JSObject *this = JS_THIS_OBJECT(cx, vp);
    jsval *argv = JS_ARGV(cx, vp);
    if(argc != 2 || !JSVAL_IS_STRING(argv[0])) {
	JS_ReportError(jcx, "unexpected arguments to setAttribute()");
    } else {
	const char *prop = stringize(argv[0]);
	JS_DefineProperty(jcx, this, prop, argv[1], NULL, NULL, PROP_FIXED);
    }
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}				/* setAttribute */

static JSBool
appendChild(JSContext * cx, uintN argc, jsval * vp)
{
    JSObject *elar;		/* elements array */
    jsuint length;
    jsval v;
    jsval *argv = JS_ARGV(cx, vp);
    JSObject *this = JS_THIS_OBJECT(cx, vp);
    JS_GetProperty(jcx, this, "elements", &v);
    elar = JSVAL_TO_OBJECT(v);
    JS_GetArrayLength(jcx, elar, &length);
    JS_DefineElement(jcx, elar, length,
       (argc > 0 ? argv[0] : JSVAL_NULL), NULL, NULL, JSPROP_ENUMERATE);
    return JS_TRUE;
}				/* appendChild */

static JSBool
win_close(JSContext * cx, uintN argc, jsval * vp)
{
/* It's too confusing to just close the window */
    i_puts(MSG_PageDone);
    cw->jsdead = eb_true;
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}				/* win_close */

static JSBool
win_alert(JSContext * cx, uintN argc, jsval * vp)
{
    jsval *argv = JS_ARGV(cx, vp);
    char *msg = NULL;
    JSString *str;
    if(argc > 0 && (str = JS_ValueToString(jcx, argv[0]))) {
	msg = transcode_get_js_bytes(str);
    }
    if(msg) {
	puts(msg);
	nzFree(msg);
    }
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}				/* win_alert */

static JSBool
win_prompt(JSContext * cx, uintN argc, jsval * vp)
{
    jsval *argv = JS_ARGV(cx, vp);
    char *msg = EMPTYSTRING;
    char *answer = EMPTYSTRING;
    JSString *str;
    char inbuf[80];
    char *s;
    char c;

    if(argc > 0 && (str = JS_ValueToString(jcx, argv[0]))) {
	msg = transcode_get_js_bytes(str);
    }
    if(argc > 1 && (str = JS_ValueToString(jcx, argv[1]))) {
	answer = transcode_get_js_bytes(str);
    }

    printf("%s", msg);
/* If it doesn't end in space or question mark, print a colon */
    c = 'x';
    if(*msg)
	c = msg[strlen(msg) - 1];
    if(!isspaceByte(c)) {
	if(!ispunctByte(c))
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
    if(inbuf[0]) {
	nzFree(answer);		/* Don't need the default answer anymore. */
	answer = inbuf;
    }
    JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(our_JS_NewStringCopyZ(jcx, answer)));
    return JS_TRUE;
}				/* win_prompt */

static JSBool
win_confirm(JSContext * cx, uintN argc, jsval * vp)
{
    jsval *argv = JS_ARGV(cx, vp);
    char *msg = EMPTYSTRING;
    JSString *str;
    char inbuf[80];
    char c;
    eb_bool first = eb_true;

    if(argc > 0 && (str = JS_ValueToString(jcx, argv[0]))) {
	msg = transcode_get_js_bytes(str);
    }

    while(eb_true) {
	printf("%s", msg);
	c = 'x';
	if(*msg)
	    c = msg[strlen(msg) - 1];
	if(!isspaceByte(c)) {
	    if(!ispunctByte(c))
		printf(":");
	    printf(" ");
	}
	if(!first)
	    printf("[y|n] ");
	first = eb_false;
	fflush(stdout);
	if(!fgets(inbuf, sizeof (inbuf), stdin))
	    exit(1);
	c = *inbuf;
	if(c && strchr("nNyY", c))
	    break;
    }

    c = tolower(c);
    if(c == 'y')
	JS_SET_RVAL(cx, vp, JSVAL_TRUE);
    else
	JS_SET_RVAL(cx, vp, JSVAL_FALSE);
    nzFree(msg);
    return JS_TRUE;
}				/* win_confirm */

static JSClass timer_class = {
    "Timer",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

/* Set a timer or an interval */
static JSObject *
setTimeout(uintN argc, jsval * argv, eb_bool isInterval)
{
    jsval v0, v1;
    JSObject *fo = 0;		/* function object */
    JSObject *to;		/* tag object */
    int n;			/* number of milliseconds */
    char fname[48];		/* function name */
    const char *fstr;		/* function string */
    const char *methname = (isInterval ? "setInterval" : "setTimeout");
    char *allocatedName = NULL;
    char *s = NULL;

    if(!parsePage) {
	JS_ReportError(jcx,
	   "cannot use %s() to delay the execution of a function", methname);
	return NULL;
    }

    if(argc != 2 || !JSVAL_IS_INT(argv[1]))
	goto badarg;

    v0 = argv[0];
    v1 = argv[1];
    n = JSVAL_TO_INT(v1);

    if(JSVAL_IS_STRING(v0) ||
       JSVAL_IS_OBJECT(v0) &&
       JS_ValueToObject(jcx, v0, &fo) && JS_ObjectIsFunction(jcx, fo)) {

/* build the tag object and link it to window */
	to = JS_NewObject(jcx, &timer_class, NULL, jwin);
	v1 = OBJECT_TO_JSVAL(to);
	JS_DefineProperty(jcx, jwin, fakePropName(), v1,
	   NULL, NULL, JSPROP_READONLY | JSPROP_PERMANENT);

	if(fo) {
/* Extract the function name, which requires several steps */
	    JSFunction *f = JS_ValueToFunction(jcx, OBJECT_TO_JSVAL(fo));
	    JSString *jss = JS_GetFunctionId(f);
	    if(jss)
		allocatedName = our_JSEncodeString(jss);
	    s = allocatedName;
/* Remember that unnamed functions are named anonymous. */
	    if(!s || !*s || stringEqual(s, "anonymous"))
		s = "javascript";
	    int len = strlen(s);
	    if(len > sizeof (fname) - 4)
		len = sizeof (fname) - 4;
	    strncpy(fname, s, len);
	    nzFree(allocatedName);
	    fname[len] = 0;
	    strcat(fname, "()");
	    fstr = fname;
	    establish_property_object(to, "onclick", fo);
	} else {
/* compile the function from the string */
	    fstr = stringize(v0);
	    JS_CompileFunction(jcx, to, "onclick", 0, emptyParms,	/* no named parameters */
	       fstr, strlen(fstr), "onclick", 1);
	}

	javaSetsTimeout(n, fstr, to, isInterval);
	return to;
    }

  badarg:
    JS_ReportError(jcx, "invalid arguments to %s()", methname);
    return NULL;
}				/* setTimeout */

static JSBool
win_sto(JSContext * cx, uintN argc, jsval * vp)
{
    jsval *argv = JS_ARGV(cx, vp);
    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(setTimeout(argc, argv, eb_false)));
    return JS_TRUE;
}				/* win_sto */

static JSBool
win_intv(JSContext * cx, uintN argc, jsval * vp)
{
    jsval *argv = JS_ARGV(cx, vp);
    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(setTimeout(argc, argv, eb_true)));
    return JS_TRUE;
}				/* win_intv */

static JSFunctionSpec window_methods[] = {
    {"alert", win_alert, 1, 0},
    {"prompt", win_prompt, 2, 0},
    {"confirm", win_confirm, 1, 0},
    {"setTimeout", win_sto, 2, 0},
    {"setInterval", win_intv, 2, 0},
    {"open", win_open, 3, 0},
    {"close", win_close, 0, 0},
    {"focus", nullFunction, 0, 0},
    {"blur", nullFunction, 0, 0},
    {"scroll", nullFunction, 0, 0},
    {0}
};

static JSClass doc_class = {
    "Document",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
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
dwrite1(uintN argc, jsval * argv, eb_bool newline)
{
    int i;
    char *msg;
    JSString *str;
    for(i = 0; i < argc; ++i) {
	if((str = JS_ValueToString(jcx, argv[i])) &&
	   (msg = transcode_get_js_bytes(str))) {
	    dwrite2(msg);
	    nzFree(msg);
	}
    }
    if(newline)
	dwrite2("\n");
}				/* dwrite1 */

static JSBool
doc_write(JSContext * cx, uintN argc, jsval * vp)
{
    jsval *argv = JS_ARGV(cx, vp);
    dwrite1(argc, argv, eb_false);
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}				/* doc_write */

static JSBool
setter_innerHTML(JSContext * cx, JSObject * obj, jsid id, JSBool strict,
   jsval * vp)
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
setter_innerText(JSContext * cx, JSObject * obj, jsid id, JSBool strict,
   jsval * vp)
{
    jsval v = *vp;
    char *s;
    if(!JSVAL_IS_STRING(v))
	return JS_FALSE;
    s = our_JSEncodeString(JSVAL_TO_STRING(v));
    nzFree(s);
    i_puts(MSG_InnerText);
/* The string has already been updated in the object. */
    return JS_TRUE;
}				/* setter_innerText */

static JSBool
doc_writeln(JSContext * cx, uintN argc, jsval * vp)
{
    jsval *argv = JS_ARGV(cx, vp);
    dwrite1(argc, argv, eb_true);
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}				/* doc_writeln */

static JSFunctionSpec doc_methods[] = {
    {"focus", nullFunction, 0, 0},
    {"blur", nullFunction, 0, 0},
    {"open", nullFunction, 0, 0},
    {"close", nullFunction, 0, 0},
    {"write", doc_write, 0, 0},
    {"writeln", doc_writeln, 0, 0},
    {0}
};

static JSClass element_class = {
    "Element",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSFunctionSpec element_methods[] = {
    {"focus", nullFunction, 0, 0},
    {"blur", nullFunction, 0, 0},
    {0}
};

static JSClass form_class = {
    "Form",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSBool
form_submit(JSContext * cx, uintN argc, jsval * vp)
{
    JSObject *this = JS_THIS_OBJECT(cx, vp);
    javaSubmitsForm(this, eb_false);
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}				/* form_submit */

static JSBool
form_reset(JSContext * cx, uintN argc, jsval * vp)
{
    JSObject *this = JS_THIS_OBJECT(cx, vp);
    javaSubmitsForm(this, eb_true);
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}				/* form_reset */

static JSFunctionSpec form_methods[] = {
    {"submit", form_submit, 0, 0},
    {"reset", form_reset, 0, 0},
    {0}
};

static JSClass body_class = {
    "Body",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSFunctionSpec body_methods[] = {
    {"setAttribute", setAttribute, 2, 0},
    {"appendChild", appendChild, 1, 0},
    {0}
};

static JSClass head_class = {
    "Head",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSFunctionSpec head_methods[] = {
    {"setAttribute", setAttribute, 2, 0},
    {"appendChild", appendChild, 1, 0},
    {0}
};

static JSClass meta_class = {
    "Meta",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

/* Don't be confused; this is for <link>, not <a> */
static JSClass link_class = {
    "Link",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSFunctionSpec link_methods[] = {
    {"setAttribute", setAttribute, 2, 0},
    {0}
};

static JSClass image_class = {
    "Image",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSClass frame_class = {
    "Frame",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSClass anchor_class = {
    "Anchor",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSClass table_class = {
    "Table",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSClass trow_class = {
    "Trow",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSClass cell_class = {
    "Cell",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSClass div_class = {
    "Div",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSClass span_class = {
    "Span",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSClass area_class = {
    "Area",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSClass option_class = {
    "Option",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

struct DOMCLASS {
    JSClass *class;
    JSFunctionSpec *methods;
    JSNative constructor;
    int nargs;
};

static struct DOMCLASS domClasses[] = {
    {&element_class, element_methods, nullFunction, 0},
    {&form_class, form_methods},
    {&body_class, body_methods},
    {&head_class, head_methods},
    {&meta_class},
    {&link_class, link_methods, nullFunction, 0},
    {&image_class, 0, nullFunction, 1},
    {&frame_class},
    {&anchor_class, 0, nullFunction, 1},
    {&table_class},
    {&div_class},
    {&area_class},
    {&span_class},
    {&trow_class},
    {&cell_class},
    {&option_class},
    {0}
};

static const char *docarrays[] = {
    "heads", "bodies", "links", "tables", "divs", "spans",
    "forms", "images", "areas", "metas", 0
};


/* Some things are just easier in javascript */
static const char initScript[] = "document.all.tags = function(s) { \n\
switch(s.toLowerCase()) { \n\
case 'form': return document.forms; \n\
case 'table': return document.tables; \n\
case 'div': return document.divs; \n\
case 'a': return document.links; \n\
case 'img': case 'image': return document.images; \n\
case 'span': return document.spans; \n\
case 'head': return document.heads; \n\
case 'meta': return document.metas; \n\
case 'body': return document.bodies; \n\
default: /* alert('all.tags default ' + s); */ return new Array(); }} \n\
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
default: /* alert('createElement default ' + s); */ return new Object(); }} \n\
\n\
URL.prototype.indexOf = function(s) { \n\
return this.toString().indexOf(s); }\n\
URL.prototype.lastIndexOf = function(s) { \n\
return this.toString().lastIndexOf(s); }\n\
URL.prototype.substring = function(from, to) { \n\
return this.toString().substring(from, to); }\n\
URL.prototype.toLowerCase = function() { \n\
return this.toString().toLowerCase(); }\n\
URL.prototype.toUpperCase = function() { \n\
return this.toString().toUpperCase(); }\n\
URL.prototype.match = function(s) { \n\
return this.toString().match(s); }\n\
\n\
history.toString = function() { \n\
return 'Sorry, edbrowse does not maintain a browsing history.'; } \
";

void *
createJavaContext(void)
{
    static JSRuntime *jrt;
    JSObject *o, *nav, *screen, *hist, *del;
/* navigator mime types and plugins */
    JSObject *navmt, *navpi;
    const char *itemname;
    int i;
    char verx11[20];
    jsval rval;
    struct MIMETYPE *mt;

    if(!jrt) {
/* 4 meg js space - should this be configurable? */
	jrt = JS_NewRuntime(4L * 1024L * 1024L);
	if(!jrt)
	    i_printfExit(MSG_JavaMemError);
	gOutFile = stdout;
	gErrFile = stderr;
    }

    jcx = JS_NewContext(jrt, gStackChunkSize);
    if(!jcx)
	i_printfExit(MSG_JavaContextError);
    JS_SetErrorReporter(jcx, my_ErrorReporter);
    JS_SetOptions(jcx, JSOPTION_VAROBJFIX);

/* Create the Window object, which is the global object in DOM. */
//    jwin = JS_NewObject(jcx, &window_class, NULL, NULL);
    jwin = JS_NewCompartmentAndGlobalObject(jcx, &window_class, NULL);
    if(!jwin)
	i_printfExit(MSG_JavaWindowError);
    JS_InitClass(jcx, jwin, 0, &window_class, window_ctor, 3,
       NULL, window_methods, NULL, NULL);

/* Ok, but the global object was created before the class,
 * so it doesn't have its methods yet. */
    JS_DefineFunctions(jcx, jwin, window_methods);

/* Math, Date, Number, String, etc */
    if(!JS_InitStandardClasses(jcx, jwin))
	i_printfExit(MSG_JavaClassError);

    establish_property_object(jwin, "window", jwin);
    establish_property_object(jwin, "self", jwin);
    establish_property_object(jwin, "parent", jwin);
    establish_property_object(jwin, "top", jwin);

/* Some visual attributes of the window.
 * These are just guesses.
 * Better to have something, than to leave them undefined. */
    establish_property_number(jwin, "height", 768, eb_true);
    establish_property_number(jwin, "width", 1024, eb_true);
    establish_property_string(jwin, "status", 0, eb_false);
    establish_property_string(jwin, "defaultStatus", 0, eb_false);
    establish_property_bool(jwin, "returnValue", eb_true, eb_false);
    establish_property_bool(jwin, "menubar", eb_true, eb_false);
    establish_property_bool(jwin, "scrollbars", eb_true, eb_false);
    establish_property_bool(jwin, "toolbar", eb_true, eb_false);
    establish_property_bool(jwin, "resizable", eb_true, eb_false);
    establish_property_bool(jwin, "directories", eb_false, eb_false);
    establish_property_string(jwin, "name", "unspecifiedFrame", eb_false);

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
	i_printfExit(MSG_JavaObjError);
    establish_property_object(jwin, "document", jdoc);

    establish_property_string(jdoc, "bgcolor", "white", eb_false);
    establish_property_string(jdoc, "cookie", 0, eb_false);
    establish_property_string(jdoc, "referrer", cw->referrer, eb_true);
    establish_property_url(jdoc, "URL", cw->fileName, eb_true);
    establish_property_url(jdoc, "location", cw->fileName, eb_false);
    establish_property_url(jwin, "location", cw->firstURL, eb_false);
    establish_property_string(jdoc, "domain", getHostURL(cw->fileName), eb_false);

/* create arrays under document */
    for(i = 0; itemname = docarrays[i]; ++i)
	establish_property_array(jdoc, itemname);

/* Some arrays are under window */
    establish_property_array(jwin, "frames");

    o = JS_NewObject(jcx, 0, 0, jdoc);
    establish_property_object(jdoc, "idMaster", o);
    o = JS_NewObject(jcx, 0, 0, jdoc);
    establish_property_object(jdoc, "all", o);

    nav = JS_NewObject(jcx, 0, 0, jwin);
    establish_property_object(jwin, "navigator", nav);

/* attributes of the navigator */
    establish_property_string(nav, "appName", "edbrowse", eb_true);
    establish_property_string(nav, "appCode Name", "edbrowse C/SMJS", eb_true);
/* Use X11 to indicate unix/linux.  Sort of a standard */
    sprintf(verx11, "%s%s", version, "-X11");
    establish_property_string(nav, "appVersion", version, eb_true);
    establish_property_string(nav, "userAgent", currentAgent, eb_true);
    establish_property_string(nav, "oscpu", currentOS(), eb_true);
    establish_property_string(nav, "platform", currentMachine(), eb_true);
    establish_property_string(nav, "product", "smjs", eb_true);
    establish_property_string(nav, "productSub", "1.5", eb_true);
    establish_property_string(nav, "vendor", "eklhad", eb_true);
    establish_property_string(nav, "vendorSub", version, eb_true);
/* We need to locale-ize the next one */
    establish_property_string(nav, "userLanguage", "english", eb_true);
    establish_property_string(nav, "language", "english", eb_true);
    JS_DefineFunction(jcx, nav, "javaEnabled", falseFunction, 0, PROP_FIXED);
    JS_DefineFunction(jcx, nav, "taintEnabled", falseFunction, 0, PROP_FIXED);
    establish_property_bool(nav, "cookieEnabled", eb_true, eb_true);
    establish_property_bool(nav, "onLine", eb_true, eb_true);

/* Build the array of mime types and plugins,
 * according to the entries in the config file. */
    navpi = establish_property_array(nav, "plugins");
    navmt = establish_property_array(nav, "mimeTypes");
    mt = mimetypes;
    for(i = 0; i < maxMime; ++i, ++mt) {
/* po is the plugin object and mo is the mime object */
	JSObject *mo, *po;
	jsval mov, pov;
	int len;

	po = JS_NewObject(jcx, 0, 0, nav);
	pov = OBJECT_TO_JSVAL(po);
	JS_DefineElement(jcx, navpi, i, pov, NULL, NULL, PROP_FIXED);
	mo = JS_NewObject(jcx, 0, 0, nav);
	mov = OBJECT_TO_JSVAL(mo);
	JS_DefineElement(jcx, navmt, i, mov, NULL, NULL, PROP_FIXED);
	establish_property_object(mo, "enabledPlugin", po);
	establish_property_string(mo, "type", mt->type, eb_true);
	establish_property_object(navmt, mt->type, mo);
	establish_property_string(mo, "description", mt->desc, eb_true);
	establish_property_string(mo, "suffixes", mt->suffix, eb_true);
/* I don't really have enough information, from the config file, to fill
 * in the attributes of the plugin object.
 * I'm just going to fake it.
 * Description will be the same as that of the mime type,
 * and the filename will be the program to run.
 * No idea if this is right or not. */
	establish_property_string(po, "description", mt->desc, eb_true);
	establish_property_string(po, "filename", mt->program, eb_true);
/* For the name, how about the program without its options? */
	len = strcspn(mt->program, " \t");
	JS_DefineProperty(jcx, po, "name",
	   STRING_TO_JSVAL(our_JS_NewStringCopyN(jcx, mt->program, len)),
	   0, 0, PROP_FIXED);
    }

    screen = JS_NewObject(jcx, 0, 0, jwin);
    establish_property_object(jwin, "screen", screen);
    establish_property_number(screen, "height", 768, eb_true);
    establish_property_number(screen, "width", 1024, eb_true);
    establish_property_number(screen, "availHeight", 768, eb_true);
    establish_property_number(screen, "availWidth", 1024, eb_true);
    establish_property_number(screen, "availTop", 0, eb_true);
    establish_property_number(screen, "availLeft", 0, eb_true);

    del = JS_NewObject(jcx, 0, 0, jdoc);
    establish_property_object(jdoc, "body", del);
    establish_property_object(jdoc, "documentElement", del);
    establish_property_number(del, "clientHeight", 768, eb_true);
    establish_property_number(del, "clientWidth", 1024, eb_true);
    establish_property_number(del, "offsetHeight", 768, eb_true);
    establish_property_number(del, "offsetWidth", 1024, eb_true);
    establish_property_number(del, "scrollHeight", 768, eb_true);
    establish_property_number(del, "scrollWidth", 1024, eb_true);
    establish_property_number(del, "scrollTop", 0, eb_true);
    establish_property_number(del, "scrollLeft", 0, eb_true);

    hist = JS_NewObject(jcx, 0, 0, jwin);
    establish_property_object(jwin, "history", hist);

/* attributes of history */
    establish_property_string(hist, "current", cw->fileName, eb_true);
/* There's no history in edbrowse. */
/* Only the current file is known, hence length is 1. */
    establish_property_number(hist, "length", 1, eb_true);
    establish_property_string(hist, "next", 0, eb_true);
    establish_property_string(hist, "previous", 0, eb_true);
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
establish_innerHTML(void *jv, const char *start, const char *end, eb_bool is_ta)
{
    JSObject *obj = jv, *o;
    jsval v;

    if(!obj)
	return;

/* null start means the pointer has been corrupted by a document.write() call */
    if(!start)
	start = end = EMPTYSTRING;
    JS_DefineProperty(jcx, obj, "innerHTML",
       STRING_TO_JSVAL(our_JS_NewStringCopyN(jcx, start, end - start)),
       NULL, (is_ta ? setter_innerText : setter_innerHTML),
       JSPROP_ENUMERATE | JSPROP_PERMANENT);
    if(is_ta) {
	JS_DefineProperty(jcx, obj, "innerText",
	   STRING_TO_JSVAL(our_JS_NewStringCopyN(jcx, start, end - start)),
	   NULL, setter_innerText, JSPROP_ENUMERATE | JSPROP_PERMANENT);
    }

/* Anything with an innerHTML might also have a style. */
    o = JS_NewObject(jcx, 0, 0, obj);
    v = OBJECT_TO_JSVAL(o);
    JS_DefineProperty(jcx, obj, "style", v, NULL, NULL, JSPROP_ENUMERATE);
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
	JS_GetProperty(jcx, jdoc, "location", &oval);
	jdloc = JSVAL_TO_OBJECT(oval);
    } else
	jwin = jdoc = jwloc = jdloc = 0;
}				/* jMyContext */

eb_bool
javaParseExecute(void *this, const char *str, const char *filename, int lineno)
{
    JSBool ok;
    eb_bool rc;
    jsval rval;

/* Sometimes Mac puts these three chars at the start of a text file. */
    if(!strncmp(str, "\xef\xbb\xbf", 3))
	str += 3;

    debugPrint(6, "javascript:\n%s", str);
    ok = JS_EvaluateScript(jcx, this, str, strlen(str),
       filename, lineno, &rval);
    rc = eb_true;
    if(JSVAL_IS_BOOLEAN(rval))
	rc = JSVAL_TO_BOOLEAN(rval);
    JS_GC(jcx);
    return rc;
}				/* javaParseExecute */

/* link a frame, span, anchor, etc to the document object model */
void *
domLink(const char *classname,	/* instantiate this class */
   const char *symname, const char *idname, const char *href, const char *href_url, const char *list,	/* next member of this array */
   void *owner, int radiosel)
{
    JSObject *v = 0, *w, *alist = 0, *master;
    jsval vv, listv;
    jsuint length, attr = PROP_FIXED;
    JSClass *cp;
    eb_bool dupname = eb_false;
    int i;

    if(cw->jsdead)
	return 0;

/* find the class */
    for(i = 0; cp = domClasses[i].class; ++i)
	if(stringEqual(cp->name, classname))
	    break;

    if(symname) {
	JSBool found;
	JS_HasProperty(jcx, owner, symname, &found);
	if(found) {

/*********************************************************************
This could be a duplicate name.
Yes, that really happens.
Link to the first tag having this name,
and link the second tag under a fake name, so gc won't throw it away.
Or - it could be a duplicate name because multiple radio buttons
all share the same name.
The first time we create the array, and thereafter we just link
under that array.
Or - and this really does happen -
an input tag could have the name action, colliding with form.action.
I have no idea what to do here.
I will assume the tag displaces the action.
That means javascript cannot change the action of the form,
which it rarely does anyways.
When it refers to form.action, that will be the input tag.
I'll check for that one first.
Ok???
Yeah, it makes my head spin too.
*********************************************************************/

	    if(stringEqual(symname, "action")) {
		JSObject *ao;	/* action object */
		JS_GetProperty(jcx, owner, symname, &vv);
		ao = JSVAL_TO_OBJECT(vv);
/* actioncrash tells me if we've already had this collision */
		JS_HasProperty(jcx, ao, "actioncrash", &found);
		if(!found) {
		    JS_DeleteProperty(jcx, owner, symname);
/* gc will clean this up later */
/* advance, as though this were not found */
		    goto afterfound;
		}
	    }

	    if(radiosel == 1) {
		JS_GetProperty(jcx, owner, symname, &vv);
		v = JSVAL_TO_OBJECT(vv);
	    } else {
		dupname = eb_true;
	    }
	}
    }

  afterfound:
    if(!v) {
	if(radiosel) {
	    v = JS_NewArrayObject(jcx, 0, NULL);
	    if(radiosel == 1) {
		establish_property_string(v, "type", "radio", eb_true);
	    } else {
/* self-referencing - hope this is ok */
		establish_property_object(v, "options", v);
		establish_property_number(v, "selectedIndex", -1, eb_false);
// not the normal pathway; we have to create our own element methods here.
		JS_DefineFunction(jcx, v, "focus", nullFunction, 0, PROP_FIXED);
		JS_DefineFunction(jcx, v, "blur", nullFunction, 0, PROP_FIXED);
	    }
	} else {
	    v = JS_NewObject(jcx, cp, NULL, owner);
	}
	vv = OBJECT_TO_JSVAL(v);

/* if no name, then use id as name */
	if(!symname && idname) {
	    JS_DefineProperty(jcx, owner, idname, vv, NULL, NULL, attr);
	} else if(symname && !dupname) {
	    JS_DefineProperty(jcx, owner, symname, vv, NULL, NULL, attr);
	    if(stringEqual(symname, "action"))
		establish_property_bool(v, "actioncrash", eb_true, eb_true);

/* link to document.all */
	    JS_GetProperty(jcx, jdoc, "all", &listv);
	    master = JSVAL_TO_OBJECT(listv);
	    establish_property_object(master, symname, v);
	} else {
/* tie this to something, to protect it from gc */
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
	    if(symname && !dupname)
		establish_property_object(alist, symname, v);
	    if(idname && (!symname || !stringEqual(symname, idname)))
		establish_property_object(alist, idname, v);
	}			/* list indicated */
    }

    if(radiosel == 1) {
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
	establish_property_string(v, "name", symname, eb_true);
    if(idname) {
/* v.id becomes idname, and idMaster.idname becomes v
 * In case of forms, v.id should remain undefined.  So we can have
 * a form field named "id". */
	if(strcmp(classname, "Form") != 0)
	    establish_property_string(v, "id", idname, eb_true);
	JS_GetProperty(jcx, jdoc, "idMaster", &listv);
	master = JSVAL_TO_OBJECT(listv);
	establish_property_object(master, idname, v);
    } else {
	if(strcmp(classname, "Form") != 0)
	    establish_property_string(v, "id", EMPTYSTRING, eb_true);
    }

    if(href && href_url) {
	establish_property_url(v, href, href_url, eb_false);
    }

    if(cp == &element_class) {
/* link back to the form that owns the element */
	establish_property_object(v, "form", owner);
    }

    return v;
}				/* domLink */
