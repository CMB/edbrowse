/*********************************************************************
This is the back-end process for javascript.
This is the server, and edbrowse is the client.
We receive interprocess messages from edbrowse,
getting and setting properties for various DOM objects.

This is the mozilla version.
If you package this with the mozilla js libraries,
you will need to include the MPL, mozilla public license,
along with the GPL, general public license.

The interface between this process and edbrowse is defined in ebjs.h.
There should be no other local header files common to both.

Exit codes are as follows:
0 terminate normally, as directed by edbrowse
1. bad arguments
2 cannot read or write to edbrowse
3 messages are out of sync
4 cannot create javascript runtime environmet
5 cannot read from stdin or write to stdout
6 unexpected message command from edbrowse
7 unexpected property type from edbrowse
8 unexpected class name from edbrowse
9 only arrays of objects are supported at this time
90 this program was never executed
99 memory allocation error or heap corruption
*********************************************************************/

#include "eb.h"

#include <limits>
#include <iostream>
#include <string>

/* work around a bug where the standard UINT32_MAX isn't defined, I really hope this is correct */
#ifndef UINT32_MAX
#define UINT32_MAX std::numeric_limits<uint32_t>::max()
#endif

/* now we can include our jsapi */
#include <jsapi.h>
#include <jsfriendapi.h>

using namespace std;

/* This function closes edbrowse down, e.g. after malloc failure,
 * it is just a stub for exit here. */
void ebClose(int n)
{
	exit(n);
}				/* ebclose */

/* stub, this function called by freeTags(), but never called in this context */
void freeEmptySideBuffer(int n)
{
}

/* meta tags don't have side effects from within the js process. */
void htmlMetaHelper(struct htmlTag *t)
{
}

/* textarea does not generate a side buffer here */
int sideBuffer(int cx, const char *text, int textlen, const char *bufname)
{
	return 0;
}

/* ebrc strings don't mean anything here */
const char *ebrc_en = emptyString;
const char *ebrc_fr = emptyString;
const char *ebrc_pt_br = emptyString;
const char *ebrc_de = emptyString;

static void usage(void)
{
	fprintf(stderr, "Usage:  edbrowse-js pipe_in pipe_out jsHeapSize\n");
	exit(1);
}				/* usage */

/* arguments, as indicated by the above */
static int pipe_in, pipe_out, enginePool;

static void js_start(void);
static void readMessage(void);
static void processMessage(void);
static void createContext(void);
static void writeHeader(void);

static JSContext *jcx;
static JSObject *winobj;	/* window object */
static JSObject *docobj;	/* document object */

static void cwSetup(void)
{
	in_js_cw.winobj = winobj;
	in_js_cw.docobj = docobj;
	in_js_cw.hbase = get_property_string(docobj, "base$href");
	in_js_cw.baseset = true;
}				/* cwSetup */

static void cwBringdown(void)
{
	freeTags(cw);
	nzFree(cw->ft);		/* title could have been set by prerender */
	cw->ft = 0;
	nzFree(cw->hbase);
	cw->hbase = 0;
}				/* cwBringdown */

static struct EJ_MSG head;
static char *errorMessage;
static char *effects;
static int eff_l;
#define effectString(s) stringAndString(&effects, &eff_l, (s))
#define effectChar(s) stringAndChar(&effects, &eff_l, (s))
#define endeffect() effectString("`~@}\n");

/* pack the decoration of a tree into the effects string */
static void packDecoration(void)
{
	struct htmlTag *t;
	int j;
	if (!cw->tags)		/* should never happen */
		return;
	for (j = 0; j < cw->numTags; ++j) {
		char line[60];
		t = tagList[j];
		if (!t->jv)
			continue;
		sprintf(line, ",%d=%p", j, t->jv);
		effectString(line);
	}
}				/* packDecoration */

static char *membername;
static char *propval;
static enum ej_proptype proptype;
static char *runscript;

int main(int argc, char **argv)
{
/* do this first, in case usage some day is tailored to language */
	selectLanguage();

	if (argc != 4)
		usage();

	pipe_in = stringIsNum(argv[1]);
	pipe_out = stringIsNum(argv[2]);
	enginePool = stringIsNum(argv[3]);
	if (pipe_in < 0 || pipe_out < 0 || enginePool < 0)
		usage();

	if (enginePool < 2)
		enginePool = 2;

	js_start();

/* edbrowse catches interrupt, this process ignores it. */
/* Use quit to terminate, or kill from another console. */
	signal(SIGINT, SIG_IGN);

	effects = initString(&eff_l);

	while (true) {
		readMessage();
		head.highstat = EJ_HIGH_OK;
		head.lowstat = EJ_LOW_OK;
		head.side = head.msglen = 0;

		if (head.cmd == EJ_CMD_EXIT)
			exit(0);

		if (head.cmd == EJ_CMD_CREATE) {
/* this one is special */
			createContext();
			if (!head.highstat) {
				head.jcx = jcx;
				head.winobj = winobj;
				head.docobj = docobj;
			}
			head.n = head.proplength = 0;
			writeHeader();
			continue;
		}

		jcx = (JSContext *) head.jcx;
		winobj = (JSObject *) head.winobj;
		docobj = (JSObject *) head.docobj;

		if (head.cmd == EJ_CMD_DESTROY) {
/* don't enter the compartment of a context you want to destroy */
			JS_DestroyContext(jcx);
			head.n = head.proplength = 0;
			writeHeader();
			continue;
		}

/* this function will enter the compartment */
		processMessage();
	}
}				/* main */

/* read from and write to edbrowse */
static void readFromEb(void *data_p, int n)
{
	int rc;
	if (n == 0)
		return;
	rc = read(pipe_in, data_p, n);
	if (rc == n)
		return;
/* Oops - can't read from the process any more */
	exit(2);
}				/* readFromEb */

static void writeToEb(const void *data_p, int n)
{
	int rc;
	if (n == 0)
		return;
	rc = write(pipe_out, data_p, n);
	if (rc == n)
		return;
/* Oops - can't write to the process any more */
	fprintf(stderr, "js cannot communicate with edbrowse\n");
	exit(2);
}				/* writeToEb */

static void writeHeader(void)
{
	head.magic = EJ_MAGIC;
	head.side = eff_l;
	head.msglen = 0;
	if (errorMessage)
		head.msglen = strlen(errorMessage);

	writeToEb(&head, sizeof(head));

/* send out the error message and side effects, if present. */
/* Edbrowse will expect these before any returned values. */
	if (head.side) {
		writeToEb(effects, head.side);
		nzFree(effects);
		effects = initString(&eff_l);
	}

	if (head.msglen) {
		writeToEb(errorMessage, head.msglen);
		nzFree(errorMessage);
		errorMessage = 0;
	}

/* That's the header, you may still need to send a returned value */
}				/* writeHeader */

static char *readString(int n)
{
	char *s;
	if (!n)
		return 0;
	s = allocString(n + 1);
	readFromEb(s, n);
	s[n] = 0;
	return s;
}				/* readString */

/* Read the entire message, so we can process it and move on,
 * without any sync errors. This means we must read the property or run script
 * or anything else passed along. */
static void readMessage(void)
{
	enum ej_cmd cmd;
	enum ej_proptype pt;

	readFromEb(&head, sizeof(head));

	if (head.magic != EJ_MAGIC) {
		fprintf(stderr,
			"Messages between js and edbrowse are out of sync\n");
		exit(3);
	}

	cmd = head.cmd;
	pt = head.proptype;

	if (cmd == EJ_CMD_SCRIPT) {
		if (head.proplength)
			runscript = readString(head.proplength);
	}

	if (cmd == EJ_CMD_HASPROP ||
	    cmd == EJ_CMD_GETPROP ||
	    cmd == EJ_CMD_CALL ||
	    cmd == EJ_CMD_SETPROP || cmd == EJ_CMD_DELPROP) {
		if (head.n)
			membername = readString(head.n);
	}

/* property in function call is | separated list of object args */
	if (cmd == EJ_CMD_SETPROP || cmd == EJ_CMD_SETAREL ||
	    cmd == EJ_CMD_CALL) {
		proptype = head.proptype;
		if (head.proplength)
			propval = readString(head.proplength);
	}

/* and that's the whole message */
}				/* readMessage */

static JSRuntime *jrt;		/* our js runtime environment */
static const size_t gStackChunkSize = 8192;

static void js_start(void)
{
	jrt = JS_NewRuntime(enginePool * 1024L * 1024L, JS_NO_HELPER_THREADS);
	if (jrt)
		return;		/* ok */

	fprintf(stderr, "Cannot create javascript runtime environment\n");
/* send a message to edbrowse, so it can disable javascript,
 * so we don't get this same error on every browse. */
	head.highstat = EJ_HIGH_PROC_FAIL;
	head.lowstat = EJ_LOW_RUNTIME;
	writeHeader();
	exit(4);
}				/* js_start */

static void misconfigure(void)
{
/* there may already be a larger error */
	if (head.highstat > EJ_HIGH_CX_FAIL)
		return;
	head.highstat = EJ_HIGH_CX_FAIL;
	head.lowstat = EJ_LOW_VARS;
}				/* misconfigure */

static void
my_ErrorReporter(JSContext * cx, const char *message, JSErrorReport * report)
{
	if (report && report->errorNumber == JSMSG_OUT_OF_MEMORY ||
	    message && strstr(message, "out of memory")) {
		head.highstat = EJ_HIGH_HEAP_FAIL;
		head.lowstat = EJ_LOW_MEMORY;
	} else if (errorMessage == 0 && head.highstat == EJ_HIGH_OK &&
		   message && *message) {
		if (report)
			head.lineno = report->lineno;
		errorMessage = cloneString(message);
		head.highstat = EJ_HIGH_STMT_FAIL;
		head.lowstat = EJ_LOW_SYNTAX;
	}

	if (report)
		report->flags = 0;
}				/* my_ErrorReporter */

/* see if a rooted js object is window or document.
 * Note what we have to do to get back to the actual object pointer. */
static bool iswindoc(JS::HandleObject obj)
{
	JSObject *p = *obj.address();
	return (p == winobj || p == docobj);
}				/* iswindoc */

/* is a rooted object window.location or document.location? */
static bool iswindocloc(JS::HandleObject obj)
{
	JS::RootedObject w(jcx), p(jcx);
	js::RootedValue v(jcx);
/* try window.location first */
	w = winobj;
	if (JS_GetProperty(jcx, w, "location", v.address()) == JS_TRUE &&
	    v.isObject()) {
		p = JSVAL_TO_OBJECT(v);
		if (p == obj)
			return true;
	}
	w = docobj;
	if (JS_GetProperty(jcx, w, "location", v.address()) == JS_TRUE &&
	    v.isObject()) {
		p = JSVAL_TO_OBJECT(v);
		if (p == obj)
			return true;
	}
	return false;
}				/* iswindocloc */

/*********************************************************************
Convert a JS string to a C string.
This function is named JS_c_str to remind you of the C++ equivalent.
However, this function allocates the result; you must free it.
The converse is handled by JS_NewStringcopyZ, as provided by the library.
*********************************************************************/

static char *JS_c_str(js::HandleString str)
{
	size_t encodedLength = JS_GetStringEncodingLength(jcx, str);
	char *buffer = allocString(encodedLength + 1);
	buffer[encodedLength] = '\0';
	size_t result =
	    JS_EncodeStringToBuffer(jcx, str, buffer, encodedLength);
	if (result == (size_t) - 1) {
		misconfigure();
		buffer[0] = 0;
	}
	return buffer;
}				/* JS_c_str */

/* represent an object pointer in ascii */
static const char *pointer2string(const JSObject * obj)
{
	static char pbuf[32];
	sprintf(pbuf, "%p", obj);
	return pbuf;
}				/* pointer2string */

static JSObject *string2pointer(const char *s)
{
	JSObject *p;
	sscanf(s, "%p", &p);
	return p;
}				/* string2pointer */

/* like the function in ebjs.c, but a different name */
static const char *fakePropName(void)
{
	static char fakebuf[24];
	static int idx = 0;
	++idx;
	sprintf(fakebuf, "cg$$%d", idx);
	return fakebuf;
}				/*fakePropName */

/*********************************************************************
This returns the string equivalent of the js value, but use with care.
It's only good til the next call to stringize, then it will be trashed.
If you want the result longer than that, you better copy it.
*********************************************************************/

static const char *stringize(js::HandleValue v)
{
	static char buf[32];
	static const char *dynamic;
	int n;
	double d;
	if (JSVAL_IS_STRING(v)) {
		if (dynamic)
			cnzFree(dynamic);
		js::RootedString str(jcx, JSVAL_TO_STRING(v));
		dynamic = JS_c_str(str);
		return dynamic;
	}

	if (JSVAL_IS_INT(v)) {
		n = JSVAL_TO_INT(v);
		sprintf(buf, "%d", n);
		return buf;
	}

	if (JSVAL_IS_BOOLEAN(v)) {
		n = JSVAL_TO_BOOLEAN(v);
		sprintf(buf, "%d", n);
		return buf;
	}

	if (JSVAL_IS_DOUBLE(v)) {
		d = JSVAL_TO_DOUBLE(v);
		n = d;
		if (n == d)
			sprintf(buf, "%d", n);
		else
			sprintf(buf, "%lf", d);
		return buf;
	}

/* don't know what it is */
	return 0;
}				/* stringize */

#define PROP_STD (JSPROP_ENUMERATE)
#define PROP_READONLY (JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY)

/* The generic class and constructor */

#define generic_class(c, name) \
static JSClass c##_class = { \
	#name, JSCLASS_HAS_PRIVATE, \
	JS_PropertyStub, JS_DeletePropertyStub, \
	JS_PropertyStub, JS_StrictPropertyStub, \
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, \
	NULL, JSCLASS_NO_OPTIONAL_MEMBERS \
};

#define generic_ctor(c) \
static JSBool c##_ctor(JSContext * cx, unsigned int argc, jsval * vp) \
{ \
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp); \
	JSObject & callee = args.callee(); \
	jsval callee_val = JS::ObjectValue(callee); \
	JS::RootedObject newobj(cx, \
		JS_NewObjectForConstructor(cx, &c##_class, &callee_val)); \
	if (newobj == NULL) { \
		misconfigure(); \
		return JS_FALSE; \
	} \
	args.rval().set(OBJECT_TO_JSVAL(newobj)); \
	return JS_TRUE; \
}

#define generic_class_ctor(c, name) \
generic_class(c, name) \
generic_ctor(c)

/* window class is diffferent, because the flags must be global */
static JSClass window_class = {
	"Window",
	JSCLASS_HAS_PRIVATE | JSCLASS_GLOBAL_FLAGS,
	JS_PropertyStub, JS_DeletePropertyStub,
	JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
	NULL, JSCLASS_NO_OPTIONAL_MEMBERS
};

/* the window constructor can open a new window, a new edbrowse session. */
/* This is done by setting n{url} in the effects string */
static JSBool window_ctor(JSContext * cx, unsigned int argc, jsval * vp)
{
	const char *newloc = 0;
	const char *winname = 0;
	JS::RootedString str(cx);
	js::RootedValue v(cx);
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JSObject & callee = args.callee();
	jsval callee_val = JS::ObjectValue(callee);
	JS::RootedObject newwin(cx,
				JS_NewObjectForConstructor(cx, &window_class,
							   &callee_val));
	if (newwin == NULL) {
		misconfigure();
		return JS_FALSE;
	}
	if (args.length() > 0 && (str = JS_ValueToString(cx, args[0])))
		newloc = JS_c_str(str);
	if (args.length() > 1 && (str = JS_ValueToString(cx, args[1])))
		winname = JS_c_str(str);
/* third argument is attributes, like window size and location,
 * that we don't care about.
 * I only do something if opening a new web page.
 * If it's just a blank window, I don't know what to do with that. */
	if (newloc && *newloc) {
		effectString("n{p");	// }
		effectString(newloc);
		effectChar('\n');
		if (winname)
			effectString(winname);
		endeffect();
	}
	cnzFree(newloc);
	cnzFree(winname);
	v = OBJECT_TO_JSVAL(winobj);
	JS_DefineProperty(cx, newwin, "opener", v, NULL, NULL, PROP_READONLY);
	args.rval().set(OBJECT_TO_JSVAL(newwin));
	return JS_TRUE;
}				/* window_ctor */

/* All the other dom classes and constructors.
 * If a constructor is not in this list, it is coming later,
 * because it does something special. */
generic_class_ctor(document, Document)
    generic_class_ctor(head, Head)
    generic_class_ctor(meta, Meta)
    generic_class_ctor(link, Link)
    generic_class_ctor(body, Body)
    generic_class_ctor(base, Base)
    generic_class_ctor(form, Form)
    generic_class_ctor(element, Element)
    generic_class_ctor(image, Image)
    generic_class_ctor(frame, Frame)
    generic_class_ctor(anchor, Anchor)
    generic_class_ctor(lister, Lister)
    generic_class_ctor(listitem, Listitem)
    generic_class_ctor(tbody, Tbody)
    generic_class_ctor(table, Table)
    generic_class_ctor(div, Div)
    generic_class_ctor(area, Area)
    generic_class_ctor(span, Span)
    generic_class_ctor(trow, Trow)
    generic_class_ctor(cell, Cell)
    generic_class_ctor(para, P)
    generic_class(option, Option)
/* constructor below */
    generic_class_ctor(script, Script)
    generic_class(url, URL)
/* constructor below */
    generic_class(timer, Timer)
/* instantiated through window.setTimout() */
    generic_class(textnode, TextNode)
/* constructor below */
/* Here are a couple nonstandard constructors. */
/* text and value can be passed as args to the option constructor */
static JSBool option_ctor(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::RootedString str(cx);
	js::RootedValue v(cx);
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JSObject & callee = args.callee();
	jsval callee_val = JS::ObjectValue(callee);
	JS::RootedObject newopt(cx,
				JS_NewObjectForConstructor(cx, &option_class,
							   &callee_val));
	if (newopt == NULL) {
		misconfigure();
		return JS_FALSE;
	}
	if (args.length() > 0 && (str = JS_ValueToString(cx, args[0]))) {
		v = STRING_TO_JSVAL(str);
		JS_DefineProperty(cx, newopt, "text", v, NULL, NULL, PROP_STD);
	}
	if (args.length() > 1 && (str = JS_ValueToString(cx, args[1]))) {
		v = STRING_TO_JSVAL(str);
		JS_DefineProperty(cx, newopt, "value", v, NULL, NULL, PROP_STD);
	}
	str = JS_NewStringCopyZ(cx, "option");
	v = STRING_TO_JSVAL(str);
	JS_DefineProperty(cx, newopt, "nodeName", v, NULL, NULL, PROP_STD);
	v = JSVAL_FALSE;
	JS_DefineProperty(cx, newopt, "selected", v, NULL, NULL, PROP_STD);
	JS_DefineProperty(cx, newopt, "defaultSelected", v, NULL, NULL,
			  PROP_STD);
	args.rval().set(OBJECT_TO_JSVAL(newopt));
	return JS_TRUE;
}				/* option_ctor */

/* text can be passed to the constructor */
static JSBool textnode_ctor(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::RootedString str(cx);
	js::RootedValue v(cx);
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JSObject & callee = args.callee();
	jsval callee_val = JS::ObjectValue(callee);
	JS::RootedObject newtext(cx,
				 JS_NewObjectForConstructor(cx, &textnode_class,
							    &callee_val));
	if (newtext == NULL) {
		misconfigure();
		return JS_FALSE;
	}
	if (args.length() > 0 && (str = JS_ValueToString(cx, args[0])))
		v = STRING_TO_JSVAL(str);
	else
		v = JS_GetEmptyStringValue(jcx);
	JS_DefineProperty(cx, newtext, "data", v, NULL, NULL, PROP_STD);
	args.rval().set(OBJECT_TO_JSVAL(newtext));
	return JS_TRUE;
}				/* textnode_ctor */

static JSBool setter_loc_hrefval(JSContext * cx, JS::HandleObject uo,
				 JS::Handle < jsid > id, JSBool strict,
				 JS::MutableHandle < JS::Value > vp);

static JSBool url_ctor(JSContext * cx, unsigned int argc, jsval * vp)
{
	const char *url = emptyString;
	const char *s;
	js::RootedValue v(cx);

	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JSObject & callee = args.callee();
	jsval callee_val = JS::ObjectValue(callee);
	JS::RootedObject uo(cx,
			    JS_NewObjectForConstructor(cx, &url_class,
						       &callee_val));
	if (uo == NULL) {
abort:
		misconfigure();
		return JS_FALSE;
	}

/* href$val has a setter, so define it here */
	v = JS_GetEmptyStringValue(cx);
	if (JS_DefineProperty
	    (cx, uo, "href$val", v, NULL, setter_loc_hrefval,
	     PROP_STD) == JS_FALSE)
		goto abort;

	if (args.length() > 0 && JSVAL_IS_STRING(args[0])) {
		v = args[0];
		s = stringize(v);
		if (s[0])
			url = s;
	}
	/* string argument */
	v = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, url));
/* This should invoke the js setter URL.href, I hope */
	if (JS_SetProperty(cx, uo, "href", v.address()) == JS_FALSE)
		goto abort;

	args.rval().set(OBJECT_TO_JSVAL(uo));
	return JS_TRUE;
}				/* url_ctor */

/*********************************************************************
Setters perform special actions when values are assigned to js properties
or even objects. These must be implemented as native C functions.
Setters are effectively suspended by setting the following variable to true.
This is usually done when the dom manipulates the javascript objects directly.
When website javascript is running however,
the setters should run as well.
Realize that the javascript level setters in startwindow.js run all the time.
There's no way to turn those off, so make sure you don't want to.
*********************************************************************/

static bool setter_suspend;

/* This setter is only for window.location or document.location.
 * It returns false to stop javascript;
 * the browser is redirected elsewhere and the current page replaced. */
static JSBool
setter_loc(JSContext * cx, JS::HandleObject uo, JS::Handle < jsid > id,
	   JSBool strict, JS::MutableHandle < JS::Value > vp)
{
	const char *s = stringize(vp);
	if (!s) {
		JS_ReportError(jcx,
			       "window.location is assigned something that I don't understand");
	} else {
		effectString("n{r");	// }
		effectString(s);
		effectChar('\n');
		endeffect();
	}
	return JS_FALSE;
}				/* setter_loc */

/* this setter can open a new window, if the parent object
 * is window.location or document.location.
 * Otherwise it does nothing. */
static JSBool
setter_loc_hrefval(JSContext * cx, JS::HandleObject uo,
		   JS::Handle < jsid > id, JSBool strict,
		   JS::MutableHandle < JS::Value > vp)
{
	const char *url = 0;
	if (setter_suspend)
		return JS_TRUE;
	url = stringize(vp);
	if (!url)
		return JS_TRUE;
	if (iswindocloc(uo)) {
		effectString("n{r");	// }
		effectString(url);
		effectChar('\n');
		endeffect();
		return JS_FALSE;
	}
	return JS_TRUE;
}				/* setter_loc_hrefval */

static const char *emptyParms[] = { 0 };
static jsval emptyArgs[] = { jsval() };

/* determine js type from js value */
static ej_proptype val_proptype(JS::HandleValue v)
{
	JS::RootedObject child(jcx);

	if (v.isNullOrUndefined())
		return EJ_PROP_NONE;
	if (JSVAL_IS_STRING(v))
		return EJ_PROP_STRING;
	if (JSVAL_IS_INT(v))
		return EJ_PROP_INT;
	if (JSVAL_IS_DOUBLE(v))
		return EJ_PROP_FLOAT;
	if (JSVAL_IS_BOOLEAN(v))
		return EJ_PROP_BOOL;

	if (v.isObject()) {
		child = JSVAL_TO_OBJECT(v);
		if (JS_ObjectIsFunction(jcx, child))
			return EJ_PROP_FUNCTION;
		if (JS_IsArrayObject(jcx, child))
			return EJ_PROP_ARRAY;
		return EJ_PROP_OBJECT;
	}

	return EJ_PROP_NONE;	/* don't know */
}				/* val_proptype */

static ej_proptype find_proptype(JS::HandleObject parent, const char *name)
{
	js::RootedValue v(jcx);
	if (JS_GetProperty(jcx, parent, name, v.address()) == JS_FALSE)
		return EJ_PROP_NONE;
	return val_proptype(v);
}				/* find_proptype */

enum ej_proptype has_property(jsobjtype parent, const char *name)
{
	JS::RootedObject p(jcx, (JSObject *) parent);
	return find_proptype(p, name);
}				/* has_property */

static void delete_property1(JS::HandleObject parent, const char *name)
{
	JS_DeleteProperty(jcx, parent, name);
}				/* delete_property1 */

void delete_property(jsobjtype parent, const char *name)
{
	JS::RootedObject p(jcx, (JSObject *) parent);
	delete_property1(p, name);
}				/* delete_property */

static int get_arraylength1(JS::HandleObject a)
{
	unsigned int length;
	if (JS_GetArrayLength(jcx, a, &length) == JS_FALSE)
		return -1;
	return length;
}				/* get_arraylength1 */

int get_arraylength(jsobjtype a)
{
	JS::RootedObject p(jcx, (JSObject *) a);
	return get_arraylength1(p);
}				/* get_arraylength */

/* Use stringize() to return a property as a string, if it is
 * string compatible. The string is allocated, free it when done. */
static char *get_property_string1(JS::HandleObject parent, const char *name)
{
	js::RootedValue v(jcx);
	const char *s;
	proptype = EJ_PROP_NONE;
	if (JS_GetProperty(jcx, parent, name, v.address()) == JS_FALSE)
		return NULL;

	proptype = val_proptype(v);
	if (proptype == EJ_PROP_NONE)
		return NULL;

	if (v.isObject()) {
/* special code here to return the object pointer */
/* That's what edbrowse is going to want. */
		s = pointer2string(JSVAL_TO_OBJECT(v));
	} else
		s = stringize(v);
	return cloneString(s);
}				/* get_property_string1 */

char *get_property_string(jsobjtype parent, const char *name)
{
	JS::RootedObject p(jcx, (JSObject *) parent);
	return get_property_string1(p, name);
}				/* get_property_string */

static JSObject *get_property_object1(JS::HandleObject parent, const char *name)
{
	js::RootedValue v(jcx);
	JS::RootedObject child(jcx);
	if (JS_GetProperty(jcx, parent, name, v.address()) == JS_FALSE)
		return 0;
	if (!v.isObject())
		return 0;
	child = JSVAL_TO_OBJECT(v);
	return child;
}				/* get_property_object1 */

jsobjtype get_property_object(jsobjtype parent, const char *name)
{
	JS::RootedObject p(jcx, (JSObject *) parent);
	return get_property_object1(p, name);
}				/* get_property_object */

/* if js changes the value of an input field, this must be reflected
 * in the <foobar> text in edbrowse. */
static JSBool
setter_value(JSContext * cx, JS::HandleObject obj,
	     JS::Handle < jsid > id, JSBool strict,
	     JS::MutableHandle < JS::Value > vp)
{
	const char *val;
	if (setter_suspend)
		return JS_TRUE;
	val = stringize(vp);
	if (!val) {
		JS_ReportError(jcx,
			       "input.value is assigned something other than a string; this can cause problems when you submit the form.");
	} else {
		effectString("v{");	// }
		effectString(pointer2string(*obj.address()));
		effectChar('=');
		effectString(val);
		endeffect();
	}
	return JS_TRUE;
}				/* setter_value */

static JSBool
setter_innerHTML(JSContext * cx, JS::HandleObject obj,
		 JS::Handle < jsid > id, JSBool strict,
		 JS::MutableHandle < jsval > vp)
{
	const char *s = stringize(vp);
	if (!s)
		return JS_TRUE;

/* lop off the preexisting children */
	JS::RootedObject children(jcx);
	children = get_property_object1(obj, "childNodes");
	if (children)
		JS_SetArrayLength(jcx, children, 0);

	int begin;
	effectString("i{h");	// }
	effectString(pointer2string(obj));
	begin = eff_l + 1;
	effectString("|<!DOCTYPE public><body>\n");
	effectString(s);
	if (*s && s[strlen(s) - 1] != '\n')
		effectChar('\n');
	effectString("</body>");
	cwSetup();
	jsobjtype innerParent = obj;
	html_from_setter(innerParent, effects + begin);
	effectChar('@');
	packDecoration();
	cwBringdown();
	endeffect();
	return JS_TRUE;
}				/* setter_innerHTML */

static JSBool
setter_innerText(JSContext * cx, JS::HandleObject obj,
		 JS::Handle < jsid > id, JSBool strict,
		 JS::MutableHandle < jsval > vp)
{
	const char *s = stringize(vp);
	if (!s)
		s = emptyString;
	effectString("i{t");	// }
	effectString(pointer2string(obj));
	effectChar('|');
	effectString(s);
	endeffect();
	return JS_TRUE;
}				/* setter_innerText */

/*********************************************************************
Maintain a copy of the cookie string that is relevant for this web page.
Include a leading semicolon, thus the form
; foo=73838; bar=j_k_qqr; bas=21998999
This is the same as the string in document.cookie.
But the setter folds a new cookie into this string,
and also passes the cookie back to edbrowse to put in the cookie jar.
*********************************************************************/

static char *cookieCopy;
static int cook_l;

static bool foldinCookie(const char *newcook)
{
	char *nc, *loc, *loc2;
	int j;
	char *s;
	char save;

/* make a copy with ; in front */
	j = strlen(newcook);
	nc = allocString(j + 3);
	strcpy(nc, "; ");
	strcpy(nc + 2, newcook);

/* cut off the extra attributes */
	s = strpbrk(nc + 2, " \t;");
	if (s)
		*s = 0;

/* cookie has to look like keyword=value */
	s = strchr(nc + 2, '=');
	if (!s || s == nc + 2)
		return false;

/* pass back to edbrowse */
	effectString("c{");	// }
	effectString(newcook);
	endeffect();

	++s;
	save = *s;
	*s = 0;			/* I'll put it back later */
	loc = strstr(cookieCopy, nc);
	*s = save;
	if (!loc)
		goto add;

/* find next piece */
	loc2 = strchr(loc + 2, ';');
	if (!loc2)
		loc2 = loc + strlen(loc);

/* excise the oold, put in the new */
	j = loc2 - loc;
	strmove(loc, loc2);
	cook_l -= j;

add:
	stringAndString(&cookieCopy, &cook_l, nc);
	return true;
}				/* foldinCookie */

static JSBool
setter_cookie(JSContext * cx, JS::HandleObject obj,
	      JS::Handle < jsid > id, JSBool strict,
	      JS::MutableHandle < JS::Value > vp)
{
	const char *newcook;
	char *original;

	if (setter_suspend)
		return JS_TRUE;

/* grab the existing document.cookie string, is this reentrant, is this ok? */
	original = get_property_string1(obj, "cookie");
	if (!original)		/* should never happen */
		original = emptyString;
	cookieCopy = initString(&cook_l);
	if (original[0]) {
		stringAndString(&cookieCopy, &cook_l, "; ");
		stringAndString(&cookieCopy, &cook_l, original);
	}
	nzFree(original);

	newcook = stringize(vp);
	if (newcook)
		foldinCookie(newcook);

	if (cookieCopy[0]) {
		js::RootedValue v(cx);
		v = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, cookieCopy + 2));
		vp.set(v);
	}
	nzFree(cookieCopy);
	return JS_TRUE;
}				/* setter_cookie */

/* this is a placeholder */
static JSBool
setter_domain(JSContext * cx, JS::HandleObject obj,
	      JS::Handle < jsid > id, JSBool strict,
	      JS::MutableHandle < JS::Value > vp)
{
	const char *val;
	if (setter_suspend)
		return JS_TRUE;
	val = stringize(vp);
	return JS_TRUE;
}				/* setter_domain */

static void
set_property_string1(js::HandleObject parent, const char *name,
		     const char *value)
{
	js::RootedValue v(jcx);
	JSPropertyOp my_getter = NULL;
	JSStrictPropertyOp my_setter = NULL;
	JSBool found;

	if (value && *value)
		v = STRING_TO_JSVAL(JS_NewStringCopyZ(jcx, value));
	else
		v = JS_GetEmptyStringValue(jcx);

	JS_HasProperty(jcx, parent, name, &found);
	if (found) {
		if (JS_SetProperty(jcx, parent, name, v.address()) == JS_FALSE)
			misconfigure();
		return;
	}

	if (stringEqual(name, "value"))
		my_setter = setter_value;
	if (stringEqual(name, "innerHTML"))
		my_setter = setter_innerHTML;
	if (stringEqual(name, "innerText"))
		my_setter = setter_innerText;
	if (stringEqual(name, "domain") && *parent.address() == docobj)
		my_setter = setter_domain;
	if (stringEqual(name, "cookie") && *parent.address() == docobj)
		my_setter = setter_cookie;

	if (JS_DefineProperty
	    (jcx, parent, name, v, my_getter, my_setter, PROP_STD) == JS_FALSE)
		misconfigure();
}				/* set_property_string1 */

int set_property_string(jsobjtype parent, const char *name, const char *value)
{
	JS::RootedObject p(jcx, (JSObject *) parent);
	set_property_string1(p, name, value);
	return 0;
}				/* set_property_string */

static void set_property_bool1(js::HandleObject parent, const char *name,
			       bool n)
{
	js::RootedValue v(jcx);
	JSBool found;

	v = (n ? JSVAL_TRUE : JSVAL_FALSE);

	JS_HasProperty(jcx, parent, name, &found);
	if (found) {
		if (JS_SetProperty(jcx, parent, name, v.address()) == JS_FALSE)
			misconfigure();
		return;
	}

	if (JS_DefineProperty(jcx, parent, name, v, NULL, NULL, PROP_STD) ==
	    JS_FALSE)
		misconfigure();
}				/* set_property_bool1 */

int set_property_bool(jsobjtype parent, const char *name, bool n)
{
	JS::RootedObject p(jcx, (JSObject *) parent);
	set_property_bool1(p, name, n);
	return 0;
}				/* set_property_bool */

static void set_property_number1(js::HandleObject parent, const char *name,
				 int n)
{
	js::RootedValue v(jcx);
	JSBool found;

	v = INT_TO_JSVAL(n);

	JS_HasProperty(jcx, parent, name, &found);
	if (found) {
		if (JS_SetProperty(jcx, parent, name, v.address()) == JS_FALSE)
			misconfigure();
		return;
	}

	if (JS_DefineProperty(jcx, parent, name, v, NULL, NULL, PROP_STD) ==
	    JS_FALSE)
		misconfigure();
}				/* set_property_number1 */

int set_property_number(jsobjtype parent, const char *name, int n)
{
	JS::RootedObject p(jcx, (JSObject *) parent);
	set_property_number1(p, name, n);
	return 0;
}				/* set_property_number */

static void set_property_float1(js::HandleObject parent, const char *name,
				double n)
{
	js::RootedValue v(jcx);
	JSBool found;

	v = DOUBLE_TO_JSVAL(n);

	JS_HasProperty(jcx, parent, name, &found);
	if (found) {
		if (JS_SetProperty(jcx, parent, name, v.address()) == JS_FALSE)
			misconfigure();
		return;
	}

	if (JS_DefineProperty(jcx, parent, name, v, NULL, NULL, PROP_STD) ==
	    JS_FALSE)
		misconfigure();
}				/* set_property_float1 */

int set_property_float(jsobjtype parent, const char *name, double n)
{
	JS::RootedObject p(jcx, (JSObject *) parent);
	set_property_float1(p, name, n);
	return 0;
}				/* set_property_float */

static void
set_property_object1(js::HandleObject parent, const char *name,
		     JS::HandleObject child)
{
	js::RootedValue v(jcx);
	JSBool found;
	JSStrictPropertyOp my_setter = NULL;

	v = OBJECT_TO_JSVAL(child);

	JS_HasProperty(jcx, parent, name, &found);
	if (found) {
		if (JS_SetProperty(jcx, parent, name, v.address()) == JS_FALSE)
			misconfigure();
		return;
	}

	if (stringEqual(name, "location") && iswindoc(parent))
		my_setter = setter_loc;

	if (JS_DefineProperty(jcx, parent, name, v, NULL, my_setter, PROP_STD)
	    == JS_FALSE)
		misconfigure();
}				/* set_property_object1 */

int set_property_object(jsobjtype parent, const char *name, jsobjtype child)
{
	JS::RootedObject p(jcx, (JSObject *) parent);
	JS::RootedObject c(jcx, (JSObject *) child);
	set_property_object1(p, name, c);
	return 0;
}				/* set_property_object */

/* for window.focus etc */
static JSBool nullFunction(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* nullFunction */

static JSBool falseFunction(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	args.rval().set(JSVAL_FALSE);
	return JS_TRUE;
}				/* falseFunction */

static JSBool trueFunction(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	args.rval().set(JSVAL_TRUE);
	return JS_TRUE;
}				/* trueFunction */

static void set_property_function1(js::HandleObject parent,
				   const char *name, const char *body)
{
	if (!body || !*body) {
/* null or empty function, link to native null function */
		JS_NewFunction(jcx, nullFunction, 0, 0, parent, name);
	} else {
		JS_CompileFunction(jcx, parent, name, 0, emptyParms,
				   body, strlen(body), name, 1);
	}
}				/* set_property_function1 */

int set_property_function(jsobjtype parent, const char *name, const char *body)
{
	JS::RootedObject p(jcx, (JSObject *) parent);
	set_property_function1(p, name, body);
	return 0;
}				/* set_property_function */

static void
run_function_onearg1(js::HandleObject parent, const char *name,
		     JS::HandleObject child)
{
	js::RootedValue v(jcx);
	jsval argv[2];
	argv[0] = OBJECT_TO_JSVAL(child);
	argv[1] = jsval();
	JS_CallFunctionName(jcx, parent, name, 1, argv, v.address());
}				/* run_function_onearg1 */

void run_function_onearg(jsobjtype parent, const char *name, jsobjtype child)
{
	JS::RootedObject p(jcx, (JSObject *) parent);
	JS::RootedObject c(jcx, (JSObject *) child);
	run_function_onearg1(p, name, c);
}				/* run_function_onearg */

#if 0
/* Not clear that setAttribute needs any side effects, or needs to be native. */
static JSBool setAttribute(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	js::RootedValue v1(cx), v2(cx);
	if (args.length() != 2 || !JSVAL_IS_STRING(args[0])) {
		JS_ReportError(cx, "unexpected arguments to setAttribute()");
	} else {
		v1 = args[0];
		v2 = args[1];
		const char *prop = stringize(v1);
		JS_DefineProperty(cx, thisobj, prop, v2, NULL, NULL, PROP_STD);
	}
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* setAttribute */
#endif

static void embedNodeName(JS::HandleObject obj)
{
	const char *nodeName;
	js::RootedValue v(jcx);
	int length;
	if (JS_GetProperty(jcx, obj, "nodeName", v.address()) == JS_TRUE) {
		nodeName = stringize(v);
		if (nodeName) {
			length = strlen(nodeName);
			if (length >= MAXTAGNAME)
				length = MAXTAGNAME - 1;
			stringAndBytes(&effects, &eff_l, nodeName, length);
		}
	}
}				/* embedNodeName */

static JSBool appendChild0(bool side, JSContext * cx, unsigned int argc,
			   jsval * vp)
{
	unsigned i, length;
	js::RootedValue v(cx);
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	args.rval().set(JSVAL_VOID);
/* we need an argument that is an object */
	if (args.length() == 0 || !args[0].isObject())
		return JS_TRUE;
	JS::RootedObject child(cx, JSVAL_TO_OBJECT(args[0]));

	JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
	if (JS_GetProperty(cx, thisobj, "childNodes", v.address()) == JS_FALSE)
		return JS_TRUE;	/* no such array */
	JS::RootedObject elar(cx, JSVAL_TO_OBJECT(v));
	if (elar == NULL) {
		misconfigure();
		return JS_FALSE;
	}
	if (JS_GetArrayLength(cx, elar, &length) == JS_FALSE) {
		misconfigure();
		return JS_FALSE;
	}
// see if it's already there.
	for (i = 0; i < length; ++i) {
		if (JS_GetElement(cx, elar, i, v.address()) == JS_FALSE) {
			misconfigure();
			return JS_FALSE;
		}
		if (!v.isObject())
			continue;
		if (JSVAL_TO_OBJECT(v) == child) {
// child was already there, what should we do?
			args.rval().set(args[0]);
			return JS_TRUE;
		}
	}

// add child to the end
	if (JS_DefineElement(cx, elar, length,
			     args[0], NULL, NULL, PROP_STD) == JS_FALSE) {
		misconfigure();
		return JS_FALSE;
	}
	JS_DefineProperty(cx, child, "parentNode", OBJECT_TO_JSVAL(thisobj),
			  NULL, NULL, PROP_STD);

	args.rval().set(args[0]);
	if (!side)
		return JS_TRUE;

/* pass this linkage information back to edbrowse, to update its dom tree */
	char e[40];
	sprintf(e, "l{a|%s,", pointer2string(thisobj));
	effectString(e);
	embedNodeName(thisobj);
	effectChar(' ');
	effectString(pointer2string(child));
	effectChar(',');
	embedNodeName(child);
	effectString(" 0x0, ");
	endeffect();
	return JS_TRUE;
}				/* appendChild0 */

static JSBool appendChild(JSContext * cx, unsigned int argc, jsval * vp)
{
	return appendChild0(true, cx, argc, vp);
}				/* appendChild */

static JSBool apch(JSContext * cx, unsigned int argc, jsval * vp)
{
	return appendChild0(false, cx, argc, vp);
}				/* apch */

static JSBool insertBefore(JSContext * cx, unsigned int argc, jsval * vp)
{
	unsigned i, mark, length;
	js::RootedValue v(cx);
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	args.rval().set(JSVAL_VOID);
/* we need two objects */
	if (args.length() < 2 || !args[0].isObject() || !args[1].isObject())
		return JS_TRUE;

	JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
	JS::RootedObject child(cx, JSVAL_TO_OBJECT(args[0]));
	JS::RootedObject item(cx, JSVAL_TO_OBJECT(args[1]));
	if (JS_GetProperty(cx, thisobj, "childNodes", v.address()) == JS_FALSE)
		return JS_TRUE;	/* no such array */
	JS::RootedObject elar(cx, JSVAL_TO_OBJECT(v));
	if (elar == NULL) {
		misconfigure();
		return JS_FALSE;
	}
	if (JS_GetArrayLength(cx, elar, &length) == JS_FALSE) {
		misconfigure();
		return JS_FALSE;
	}

/* item better be somewhere in the array, and child should not be */
	mark = -1;
	for (i = 0; i < length; ++i) {
		if (JS_GetElement(cx, elar, i, v.address()) == JS_FALSE) {
			misconfigure();
			return JS_FALSE;
		}
		if (!v.isObject())
			continue;
		if (JSVAL_TO_OBJECT(v) == item)
			mark = i;
		if (JSVAL_TO_OBJECT(v) == child) {
// child was already there, what should we do?
			args.rval().set(args[0]);
			return JS_TRUE;
		}
	}

	if (mark < 0)
		return JS_TRUE;

/* since the item to insert before was found, the call is going to */
/* succeed, so put the return value here */
	args.rval().set(args[0]);

/* push the other elements down */
	for (i = length; i > mark; --i) {
		JS_GetElement(cx, elar, i - 1, v.address());
		if (i == length)
			JS_DefineElement(cx, elar, i, v, NULL, NULL, PROP_STD);
		else
			JS_SetElement(cx, elar, i, v.address());
	}
/* and place the child */
	v = args[0];
	JS_SetElement(cx, elar, mark, v.address());
	JS_DefineProperty(cx, child, "parentNode", OBJECT_TO_JSVAL(thisobj),
			  NULL, NULL, PROP_STD);

/* pass this linkage information back to edbrowse, to update its dom tree */
	char e[40];
	sprintf(e, "l{b|%s,", pointer2string(thisobj));
	effectString(e);
	embedNodeName(thisobj);
	effectChar(' ');
	effectString(pointer2string(child));
	effectChar(',');
	embedNodeName(child);
	effectChar(' ');
	effectString(pointer2string(item));
	effectChar(',');
	embedNodeName(item);
	effectChar(' ');
	endeffect();
	return JS_TRUE;
}				/* insertBefore */

static JSBool removeChild(JSContext * cx, unsigned int argc, jsval * vp)
{
	unsigned i, mark, length;
	js::RootedValue v(cx);
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	args.rval().set(JSVAL_VOID);
/* we need an object */
	if (args.length() < 1 || !args[0].isObject())
		return JS_TRUE;

	JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
	JS::RootedObject child(cx, JSVAL_TO_OBJECT(args[0]));
	if (JS_GetProperty(cx, thisobj, "childNodes", v.address()) == JS_FALSE)
		return JS_TRUE;	/* no such array */
	JS::RootedObject elar(cx, JSVAL_TO_OBJECT(v));
	if (elar == NULL) {
		misconfigure();
		return JS_FALSE;
	}
	if (JS_GetArrayLength(cx, elar, &length) == JS_FALSE) {
		misconfigure();
		return JS_FALSE;
	}

/* child better be somewhere in the array */
	for (mark = 0; mark < length; ++mark) {
		if (JS_GetElement(cx, elar, mark, v.address()) == JS_FALSE) {
			misconfigure();
			return JS_FALSE;
		}
		if (!v.isObject())
			continue;
		if (JSVAL_TO_OBJECT(v) == child)
			goto found;
	}
	return JS_TRUE;

found:
/* we are now situated properly so set the return value, the node to remove*/
	args.rval().set(args[0]);

/* pull the others back */
	for (i = mark; i < length - 1; ++i) {
		JS_GetElement(cx, elar, i + 1, v.address());
		JS_SetElement(cx, elar, i, v.address());
	}
	JS_SetArrayLength(cx, elar, length - 1);
	JS_DeleteProperty(cx, child, "parentNode");

/* pass this linkage information back to edbrowse, to update its dom tree */
	char e[40];
	sprintf(e, "l{r|%s,", pointer2string(thisobj));
	effectString(e);
	embedNodeName(thisobj);
	effectChar(' ');
	effectString(pointer2string(child));
	effectChar(',');
	embedNodeName(child);
	effectString(" 0x0, ");
	endeffect();
	return JS_TRUE;
}				/* removeChild */

static void dwrite1(unsigned int argc, jsval * argv, bool newline)
{
	int i, begin;
	const char *msg;
	JS::RootedString str(jcx);
	effectString("w{");	// }
	begin = eff_l;
	for (i = 0; i < (signed)argc; ++i) {
		if ((str = JS_ValueToString(jcx, argv[i])) &&
		    (msg = JS_c_str(str))) {
			effectString(msg);
			cnzFree(msg);
		}
	}
	if (newline)
		effectChar('\n');
	endeffect();
}				/* dwrite1 */

static JSBool doc_write(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	dwrite1(args.length(), args.array(), false);
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* doc_write */

static JSBool doc_writeln(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	dwrite1(args.length(), args.array(), true);
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* doc_writeln */

/* this has a native wrapper so we can set innerHTML with a setter */
static JSBool doc_createElement(JSContext * cx, unsigned int argc, jsval * vp)
{
	char run[60];
	const char *tagname = NULL, *s;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedString str(cx);
	if (args.length() > 0 && (str = JS_ValueToString(cx, args[0])))
		tagname = JS_c_str(str);
	if (!tagname || strlen(tagname) >= MAXTAGNAME) {
fail:
		cnzFree(tagname);
		args.rval().set(JSVAL_NULL);
		return JS_TRUE;
	}
	for (s = tagname; *s; ++s)
		if (!isalnumByte(*s))
			goto fail;
/* let js do most of the work */
	sprintf(run, "document.crel$$('%s')", tagname);
	JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
	js::RootedValue v(cx);
	if (JS_EvaluateScript(cx, thisobj, run, strlen(run),
			      "create", 1, v.address()) == JS_FALSE)
		goto fail;
	if (!v.isObject())
		goto fail;
	JS::RootedObject child(cx, JSVAL_TO_OBJECT(v));
/* and now the reason for the wrapper */
	JS_DefineProperty(cx, child, "innerHTML",
			  JS_GetEmptyStringValue(cx),
			  NULL, setter_innerHTML, PROP_STD);
/* But we can't set innerHTML unless the object exists in edbrowse */
	sprintf(run, "l{c|%s,%s 0x0, 0x0, ", pointer2string(child), tagname);
	effectString(run);
	endeffect();

/* and return the created object */
	args.rval().set(v);
	return JS_TRUE;
}				/* doc_createElement */

static JSFunctionSpec document_methods[] = {
	JS_FS("focus", nullFunction, 0, 0),
	JS_FS("blur", nullFunction, 0, 0),
	JS_FS("open", nullFunction, 0, 0),
	JS_FS("close", nullFunction, 0, 0),
	JS_FS("write", doc_write, 0, 0),
	JS_FS("writeln", doc_writeln, 0, 0),
	JS_FS("createElement", doc_createElement, 0, 0),
	JS_FS("appendChild", appendChild, 1, 0),
	JS_FS("apch$", apch, 1, 0),
	JS_FS("insertBefore", insertBefore, 2, 0),
	JS_FS("removeChild", removeChild, 1, 0),
	JS_FS_END
};

static JSBool form_submit(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
	effectString("f{s");	// }
	effectString(pointer2string(obj));
	endeffect();
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* form_submit */

static JSBool form_reset(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
	effectString("f{r");	// }
	effectString(pointer2string(obj));
	endeffect();
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* form_reset */

static JSFunctionSpec form_methods[] = {
	JS_FS("submit", form_submit, 0, 0),
	JS_FS("reset", form_reset, 0, 0),
	JS_FS_END
};

static JSBool win_close(JSContext * cx, unsigned int argc, jsval * vp)
{
	if (head.highstat <= EJ_HIGH_CX_FAIL) {
		head.highstat = EJ_HIGH_CX_FAIL;
		head.lowstat = EJ_LOW_CLOSE;
	}
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* win_close */

static JSBool win_alert(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	const char *msg = NULL;
	JS::RootedString str(cx);
	if (args.length() > 0 && (str = JS_ValueToString(cx, args[0])))
		msg = JS_c_str(str);
	if (msg && *msg)
		puts(msg);
	cnzFree(msg);
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* win_alert */

static JSBool win_prompt(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	const char *msg = emptyString;
	const char *answer = emptyString;
	JS::RootedString str(cx);
	char inbuf[80];
	char *s;
	char c;

	if (args.length() > 0 && (str = JS_ValueToString(cx, args[0])))
		msg = JS_c_str(str);
	if (!msg)
		return JS_TRUE;
	if (!*msg) {
		cnzFree(msg);
		return JS_TRUE;
	}
	if (args.length() > 1 && (str = JS_ValueToString(cx, args[1])))
		answer = JS_c_str(str);

	printf("%s", msg);
/* If it doesn't end in space or question mark, print a colon */
	c = msg[strlen(msg) - 1];
	if (!isspace(c)) {
		if (!ispunct(c))
			printf(":");
		printf(" ");
	}
	if (answer)
		printf("[%s] ", answer);
	fflush(stdout);
	if (!fgets(inbuf, sizeof(inbuf), stdin))
		exit(5);
	s = inbuf + strlen(inbuf);
	if (s > inbuf && s[-1] == '\n')
		*--s = 0;
	if (inbuf[0]) {
		cnzFree(answer);	/* Don't need the default answer anymore. */
		answer = inbuf;
	}
	args.rval().set(STRING_TO_JSVAL(JS_NewStringCopyZ(cx, answer)));
	if (answer != inbuf)
		cnzFree(answer);
	return JS_TRUE;
}				/* win_prompt */

static JSBool win_confirm(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	const char *msg = emptyString;
	JS::RootedString str(cx);
	char inbuf[80];
	char c;
	bool first = true;

	if (args.length() > 0 && (str = JS_ValueToString(cx, args[0])))
		msg = JS_c_str(str);
	if (!msg)
		return JS_TRUE;
	if (!*msg) {
		cnzFree(msg);
		return JS_TRUE;
	}

	while (true) {
		printf("%s", msg);
		c = msg[strlen(msg) - 1];
		if (!isspace(c)) {
			if (!ispunct(c))
				printf(":");
			printf(" ");
		}
		if (!first)
			printf("[y|n] ");
		first = false;
		fflush(stdout);
		if (!fgets(inbuf, sizeof(inbuf), stdin))
			exit(5);
		c = *inbuf;
		if (c && strchr("nNyY", c))
			break;
	}

	c = tolower(c);
	if (c == 'y')
		args.rval().set(JSVAL_TRUE);
	else
		args.rval().set(JSVAL_FALSE);
	cnzFree(msg);
	return JS_TRUE;
}				/* win_confirm */

/* Set a timer or an interval */
static JSObject *setTimeout(unsigned int argc, jsval * argv, bool isInterval)
{
	JS::RootedValue v0(jcx), v1(jcx);	// values of the 2 args
// the function object, to execute, and the timer object.
	JS::RootedObject fo(jcx, 0), to(jcx);
	int n;			/* number of milliseconds */
	char nstring[20];
	char fname[48];		/* function name */
	const char *fstr;	/* function string */
	const char *methname = (isInterval ? "setInterval" : "setTimeout");
	const char *allocatedName = NULL;
	const char *s = NULL;

	if (argc != 2 || !JSVAL_IS_INT(argv[1]))
		goto badarg;

	v0 = argv[0];
	v1 = argv[1];
	n = JSVAL_TO_INT(v1);
	if (JSVAL_IS_STRING(v0) ||
	    v0.isObject() &&
	    JS_ValueToObject(jcx, v0, fo.address()) &&
	    JS_ObjectIsFunction(jcx, fo)) {

/* build the tag object and link it to window */
		to = JS_NewObject(jcx, &timer_class, NULL, winobj);
		if (to == NULL) {
abort:
			misconfigure();
			return NULL;
		}
// protect this timer from the garbage collector
		v1 = OBJECT_TO_JSVAL(to);
		if (JS_DefineProperty
		    (jcx, winobj, fakePropName(), v1, NULL, NULL,
		     PROP_STD) == JS_FALSE)
			goto abort;

		if (fo) {
/* Extract the function name, which requires several steps */
			js::RootedFunction f(jcx,
					     JS_ValueToFunction(jcx,
								OBJECT_TO_JSVAL
								(fo)));
			JS::RootedString jss(jcx, JS_GetFunctionId(f));
			if (jss)
				allocatedName = JS_c_str(jss);
			s = allocatedName;
/* Remember that unnamed functions are named anonymous. */
			if (!s || !*s || stringEqual(s, "anonymous"))
				s = "javascript";
			int len = strlen(s);
			if (len > (signed)sizeof(fname) - 4)
				len = sizeof(fname) - 4;
			strncpy(fname, s, len);
			cnzFree(allocatedName);
			fname[len] = 0;
			strcat(fname, "()");
			fstr = fname;
			set_property_object1(to, "onclick", fo);

		} else {

/* compile the function from the string */
			fstr = stringize(v0);
			if (JS_CompileFunction
			    (jcx, to, "onclick", 0, emptyParms, fstr,
			     strlen(fstr), "onclick", 1) == NULL) {
				JS_ReportError(jcx,
					       "error compiling function in %s()",
					       methname);
				return NULL;
			}
			strcpy(fname, "?");
			s = fstr;
			skipWhite(&s);
			if (memEqualCI(s, "javascript:", 11))
				s += 11;
			skipWhite(&s);
			if (isalpha(*s) || *s == '_') {
				char *j;
				for (j = fname; isalnum(*s) || *s == '_'; ++s) {
					if (j < fname + sizeof(fname) - 3)
						*j++ = *s;
				}
				strcpy(j, "()");
				skipWhite(&s);
				if (*s != '(')
					strcpy(fname, "?");
			}
			fstr = fname;
		}

		sprintf(nstring, "t{%d|", n);	// }
		effectString(nstring);
		effectString(fstr);
		effectChar('|');
		effectString(pointer2string(to));
		effectChar('|');
		effectChar((isInterval ? '1' : '0'));
		endeffect();

		return to;
	}

badarg:
	JS_ReportError(jcx, "invalid arguments to %s()", methname);
	return NULL;
}				/* setTimeout */

/* set timer and set interval */
static JSBool win_sto(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject obj(jcx,
			     setTimeout(args.length(), args.array(), false));
	if (obj == NULL)
		return JS_FALSE;
	args.rval().set(OBJECT_TO_JSVAL(obj));
	return JS_TRUE;
}				/* win_sto */

static JSBool win_intv(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject obj(jcx,
			     setTimeout(args.length(), args.array(), true));
	if (obj == NULL)
		return JS_FALSE;
	args.rval().set(OBJECT_TO_JSVAL(obj));
	return JS_TRUE;
}				/* win_intv */

/* Clear a timer or an interval */
static JSBool clearTimeout(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	args.rval().set(JSVAL_VOID);
	if (argc == 0 || !args[0].isObject())
		return JS_TRUE;
	JS::RootedObject obj(jcx, JSVAL_TO_OBJECT(args[0]));
	char nstring[80];
	sprintf(nstring, "t{0|-|%s|0", pointer2string(obj));	// }
	effectString(nstring);
	endeffect();
	return JS_TRUE;
}				/* clearTimeout */

static JSFunctionSpec window_methods[] = {
	JS_FS("alert", win_alert, 1, 0),
	JS_FS("prompt", win_prompt, 2, 0),
	JS_FS("confirm", win_confirm, 1, 0),
	JS_FS("setTimeout", win_sto, 2, 0),
	JS_FS("setInterval", win_intv, 2, 0),
	JS_FS("clearTimeout", clearTimeout, 1, 0),
	JS_FS("clearInterval", clearTimeout, 1, 0),
	JS_FS("close", win_close, 0, 0),
	JS_FS("focus", nullFunction, 0, 0),
	JS_FS("blur", nullFunction, 0, 0),
	JS_FS("scroll", nullFunction, 0, 0),
	JS_FS_END
};

static struct {
	JSClass *obj_class;
	JSNative constructor;
	JSFunctionSpec *methods;
	int nargs;
} const domClasses[] = {
	{&document_class, document_ctor, document_methods},
	{&head_class, head_ctor},
	{&meta_class, meta_ctor},
	{&link_class, link_ctor},
	{&body_class, body_ctor},
	{&base_class, base_ctor},
	{&form_class, form_ctor, form_methods},
	{&element_class, element_ctor},
	{&image_class, image_ctor, NULL, 1},
	{&frame_class, frame_ctor},
	{&anchor_class, anchor_ctor, NULL, 1},
	{&lister_class, lister_ctor},
	{&listitem_class, listitem_ctor},
	{&table_class, table_ctor},
	{&tbody_class, tbody_ctor},
	{&trow_class, trow_ctor},
	{&cell_class, cell_ctor},
	{&div_class, div_ctor},
	{&area_class, area_ctor},
	{&span_class, span_ctor},
	{&option_class, option_ctor, NULL, 2},
	{&script_class, script_ctor},
	{&para_class, para_ctor},
	{&url_class, url_ctor},
	{&textnode_class, textnode_ctor},
	{0}
};

static void createContext(void)
{
	int i;

	jcx = JS_NewContext(jrt, gStackChunkSize);
	if (!jcx) {
		head.highstat = EJ_HIGH_HEAP_FAIL;
		head.lowstat = EJ_LOW_CX;
		return;
	}

	JSAutoRequest autoreq(jcx);
	JS_SetErrorReporter(jcx, my_ErrorReporter);
	JS_SetOptions(jcx, JSOPTION_VAROBJFIX);

/* Create the Window object, which is the global object in DOM. */
	winobj = JS_NewGlobalObject(jcx, &window_class, NULL);
	if (!winobj) {
no_win:
		head.highstat = EJ_HIGH_HEAP_FAIL;
		head.lowstat = EJ_LOW_WIN;
		JS_DestroyContext(jcx);
		return;
	}

/* enter the compartment for this object for the duration of this function */
	JSAutoCompartment ac(jcx, winobj);

/* now set the window object as global */
	JS_SetGlobalObject(jcx, winobj);
/* Math, Date, Number, String, etc */
	if (!JS_InitStandardClasses(jcx, winobj)) {
no_std:
		head.highstat = EJ_HIGH_HEAP_FAIL;
		head.lowstat = EJ_LOW_STD;
		JS_DestroyContext(jcx);
		return;
	}

/* initialise the window class */
	if (JS_InitClass
	    (jcx, winobj, NULL, &window_class, window_ctor, 3, NULL,
	     window_methods, NULL, NULL) == NULL)
		goto no_win;
/* Ok, but the global object was created before the class,
* so it doesn't have its methods yet. */
	if (JS_DefineFunctions(jcx, winobj, window_methods) == JS_FALSE)
		goto no_win;

/* Other classes that we'll need. */
	for (i = 0; domClasses[i].obj_class; ++i) {
		if (JS_InitClass
		    (jcx, winobj, 0, domClasses[i].obj_class,
		     domClasses[i].constructor, domClasses[i].nargs, NULL,
		     domClasses[i].methods, NULL, NULL) == NULL)
			goto no_std;
	}

/* document under window */
	JS::RootedObject d(jcx,
			   JS_NewObject(jcx, &document_class, NULL, winobj));
	if (!d) {
no_doc:
		head.highstat = EJ_HIGH_HEAP_FAIL;
		head.lowstat = EJ_LOW_DOC;
		JS_DestroyContext(jcx);
		return;
	}
	docobj = *d.address();
	js::RootedValue v(jcx, OBJECT_TO_JSVAL(d));
	if (JS_DefineProperty
	    (jcx, winobj, "document", v, NULL, NULL, PROP_READONLY) == JS_FALSE)
		goto no_doc;

}				/* createContext */

static JSClass *classByName(const char *classname)
{
	JSClass *cp = 0;
	int i;

	if (!classname)
		return cp;	/* generic object */

	for (i = 0; cp = domClasses[i].obj_class; ++i)
		if (stringEqual(cp->name, classname))
			break;
	if (!cp) {
		fprintf(stderr, "Unexpected class name %s from edbrowse\n",
			classname);
		exit(8);
	}
	return cp;
}				/* classByName */

/* based on propval and proptype */
static void set_property_generic(js::HandleObject parent, const char *name)
{
	int n;
	double d;
	JSObject *child;
	JS::RootedObject childroot(jcx);
	JSClass *cp;

	switch (proptype) {
	case EJ_PROP_STRING:
		set_property_string1(parent, name, propval);
		break;

	case EJ_PROP_INT:
		n = atoi(propval);
		set_property_number1(parent, name, n);
		break;

	case EJ_PROP_BOOL:
		n = atoi(propval);
		set_property_bool1(parent, name, n);
		break;

	case EJ_PROP_FLOAT:
		d = atof(propval);
		set_property_float1(parent, name, d);
		break;

	case EJ_PROP_OBJECT:
		child = string2pointer(propval);
		childroot = child;
		set_property_object1(parent, name, childroot);
		break;

	case EJ_PROP_INSTANCE:
		cp = classByName(propval);
		nzFree(propval);
		propval = 0;
		childroot = JS_NewObject(jcx, cp, NULL, parent);
		if (!childroot) {
			misconfigure();
			break;
		}
		if (cp == &url_class) {
// the constructor didn't run, so create href$val with its native setter here
			js::RootedValue v(jcx);
			v = JS_GetEmptyStringValue(jcx);
			if (JS_DefineProperty
			    (jcx, childroot, "href$val", v, NULL,
			     setter_loc_hrefval, PROP_STD) == JS_FALSE) {
				misconfigure();
				break;
			}
		}

childreturn:
		set_property_object1(parent, name, childroot);
		propval = cloneString(pointer2string(*childroot.address()));
		break;

	case EJ_PROP_ARRAY:
		childroot = JS_NewArrayObject(jcx, 0, NULL);
		goto childreturn;

	case EJ_PROP_FUNCTION:
		set_property_function1(parent, name, propval);
		break;

	default:
		fprintf(stderr, "Unexpected property type %d from edbrowse\n",
			proptype);
		exit(7);
	}

}				/* set_property_generic */

static JSObject *instantiate_array1(js::HandleObject parent, const char *name)
{
	js::RootedValue v(jcx);
	js::RootedObject a(jcx);
	JSBool found;
	JS_HasProperty(jcx, parent, name, &found);
	if (found) {
		if (v.isObject()) {
			a = JSVAL_TO_OBJECT(v);
			if (JS_IsArrayObject(jcx, a))
				return a;
		}
		JS_DeleteProperty(jcx, parent, name);
	}
	a = JS_NewArrayObject(jcx, 0, NULL);
	v = OBJECT_TO_JSVAL(a);
	if (JS_DefineProperty(jcx, parent, name, v, NULL, NULL, PROP_STD) ==
	    JS_FALSE) {
		misconfigure();
		return 0;
	}
	return a;
}				/* instantiate_array1 */

jsobjtype instantiate_array(jsobjtype parent, const char *name)
{
	JS::RootedObject p(jcx, (JSObject *) parent);
	return instantiate_array1(p, name);
}				/* instantiate_array */

static JSObject *instantiate1(js::HandleObject parent, const char *name,
			      const char *classname)
{
	js::RootedValue v(jcx);
	js::RootedObject a(jcx);
	JSBool found;
	JS_HasProperty(jcx, parent, name, &found);
	if (found) {
		if (v.isObject()) {
			a = JSVAL_TO_OBJECT(v);
/* I'm going to assume it is of the proper class */
			return a;
		}
		JS_DeleteProperty(jcx, parent, name);
	}
	JSClass *cp = classByName(classname);
	a = JS_NewObject(jcx, cp, NULL, parent);
	v = OBJECT_TO_JSVAL(a);
	if (JS_DefineProperty(jcx, parent, name, v, NULL, NULL, PROP_STD) ==
	    JS_FALSE) {
		misconfigure();
		return 0;
	}
	return a;
}				/* instantiate1 */

jsobjtype instantiate(jsobjtype parent, const char *name, const char *classname)
{
	JS::RootedObject p(jcx, (JSObject *) parent);
	return instantiate1(p, name, classname);
}				/* instantiate */

static JSObject *instantiate_array_element1(js::HandleObject parent, int idx,
					    const char *classname)
{
	js::RootedObject a(jcx);
	JSClass *cp = classByName(classname);
	a = JS_NewObject(jcx, cp, NULL, parent);
	set_array_element_object(parent, idx, a);
	return a;
}				/* instantiate_array_element1 */

jsobjtype instantiate_array_element(jsobjtype parent, int idx,
				    const char *classname)
{
	JS::RootedObject p(jcx, (JSObject *) parent);
	return instantiate_array_element1(p, idx, classname);
}				/* instantiate_array_element */

static void
set_array_element_object1(js::HandleObject parent, int idx,
			  JS::HandleObject child)
{
	js::RootedValue v(jcx);
	JSBool found;
	v = OBJECT_TO_JSVAL(child);
	JS_HasElement(jcx, parent, idx, &found);
	if (found) {
		if (JS_SetElement(jcx, parent, idx, v.address()) == JS_FALSE)
			misconfigure();
	} else {
		if (JS_DefineElement(jcx, parent, idx, v, NULL, NULL, PROP_STD)
		    == JS_FALSE)
			misconfigure();
	}
}				/* set_array_element_object1 */

int set_array_element_object(jsobjtype parent, int idx, jsobjtype child)
{
	JS::RootedObject p(jcx, (JSObject *) parent);
	JS::RootedObject c(jcx, (JSObject *) child);
	set_array_element_object1(p, idx, c);
	return 0;
}				/* set_array_element_object */

static JSObject *get_array_element_object1(JS::HandleObject parent, int idx)
{
	js::RootedValue v(jcx);
	JS::RootedObject child(jcx);
	if (JS_GetElement(jcx, parent, idx, v.address()) == JS_FALSE)
		return 0;	/* perhaps out of range */
	if (!v.isObject()) {
		fprintf(stderr, "JS DOM arrays should contain only objects\n");
		exit(9);
	}
	child = JSVAL_TO_OBJECT(v);
	return child;
}				/* get_array_element_object1 */

jsobjtype get_array_element_object(jsobjtype parent, int idx)
{
	JS::RootedObject p(jcx, (JSObject *) parent);
	return get_array_element_object1(p, idx);
}				/* get_array_element_object */

/*********************************************************************
ebjs.c allows for geting and setting array elements of all types,
however the DOM only uses objects. Being lazy, I will simply
implement objects. You can add other types later.
*********************************************************************/

/*********************************************************************
run a javascript function and return the result.
If the result is an object then the pointer, as a string, is returned.
The string is always allocated, you must free it.
At entry, propval, if nonzero, is a | separated list of arguments
to the function, assuming all args are objects.
*********************************************************************/

static char *run_function(JS::HandleObject parent, const char *name)
{
	js::RootedValue v(jcx);
	bool rc;
	JSBool found;
	const char *s;

	proptype = EJ_PROP_NONE;
	JS_HasProperty(jcx, parent, name, &found);
	if (!found) {
		nzFree(propval);
		propval = 0;
		return NULL;
	}

	if (!propval) {
		rc = JS_CallFunctionName(jcx, parent, name, 0, emptyArgs,
					 v.address());
	} else {
		int argc = 0;
/* lazy, a hard limit of 20 arguments */
		jsval argv[20 + 1];
		const char *t;
		JSObject *o;
		for (s = propval; *s; s = t) {
			if (argc == 20)
				break;
			t = strchr(s, '|') + 1;
			o = string2pointer(s);
			argv[argc++] = OBJECT_TO_JSVAL(o);
		}
		argv[argc] = jsval();
		nzFree(propval);
		propval = 0;
		rc = JS_CallFunctionName(jcx, parent, name, argc, argv,
					 v.address());
	}

	if (!rc)
		return NULL;
	proptype = val_proptype(v);
	if (proptype == EJ_PROP_NONE)
		return NULL;
	if (v.isObject())
		s = pointer2string(JSVAL_TO_OBJECT(v));
	else
		s = stringize(v);
	return cloneString(s);
}				/* run_function */

/* process each message from edbrowse and respond appropriately */
static void processMessage(void)
{
	JSAutoRequest autoreq(jcx);
	JSAutoCompartment ac(jcx, winobj);
/* head.obj should be a valid object or 0 */
	JS::RootedObject parent(jcx, (JSObject *) head.obj);
	JS::RootedObject child(jcx);
	JSObject *chp;
	js::RootedValue v(jcx);
	const char *s;
	bool rc;		/* return code */
	bool setret;		/* does setting a property produce a return? */
	unsigned len;		/* array length */

	switch (head.cmd) {
	case EJ_CMD_SCRIPT:
		propval = 0;
		s = runscript;
/* Sometimes Mac puts these three chars at the start of a text file. */
		if (!strncmp(s, "\xef\xbb\xbf", 3))
			s += 3;
		head.n = 0;
		head.proplength = 0;
		if (JS_EvaluateScript(jcx, parent, s, strlen(s),
				      "foo", head.lineno, v.address())) {
			if (v != JSVAL_VOID) {
				s = 0;
				JS::RootedString str(jcx);
				str = JS_ValueToString(jcx, v);
				if (str)
					s = JS_c_str(str);
				if (s && !*s) {
					cnzFree(s);
					s = 0;
				}
				head.n = 1;
				if (s)
					head.proplength = strlen(s);
			}
		}
		nzFree(runscript);
		runscript = 0;
		writeHeader();
		if (head.proplength) {
			writeToEb(s, head.proplength);
			cnzFree(s);
		}
		break;

	case EJ_CMD_HASPROP:
		head.proptype = find_proptype(parent, membername);
		nzFree(membername);
		membername = 0;
		head.n = head.proplength = 0;
		writeHeader();
		break;

	case EJ_CMD_DELPROP:
		JS_DeleteProperty(jcx, parent, membername);
		nzFree(membername);
		membername = 0;
		head.n = head.proplength = 0;
		writeHeader();
		break;

	case EJ_CMD_GETPROP:
		propval = get_property_string1(parent, membername);
		nzFree(membername);
		membername = 0;
		head.n = head.proplength = 0;
		head.proptype = EJ_PROP_NONE;
		if (propval) {
			head.proplength = strlen(propval);
			head.proptype = proptype;
		}
		writeHeader();
		if (propval)
			writeToEb(propval, head.proplength);
		nzFree(propval);
		propval = 0;
		break;

	case EJ_CMD_SETPROP:
/* does this set_property return something? */
		setret = false;
		if (head.proptype == EJ_PROP_ARRAY
		    || head.proptype == EJ_PROP_INSTANCE)
			setret = true;
		setter_suspend = true;
		set_property_generic(parent, membername);
		setter_suspend = false;
		nzFree(membername);
		membername = 0;
propreturn:
		head.n = head.proplength = 0;
		if (setret) {
			if (propval)
				head.proplength = strlen(propval);
		} else {
			nzFree(propval);
			propval = 0;
		}
		writeHeader();
		if (setret && propval) {
			writeToEb(propval, head.proplength);
			nzFree(propval);
			propval = 0;
		}
		break;

	case EJ_CMD_GETAREL:
		child = get_array_element_object1(parent, head.n);
		propval = 0;	/* should already be 0 */
		head.proplength = 0;
		if (child) {
			propval = cloneString(pointer2string(*child.address()));
			head.proplength = strlen(propval);
			head.proptype = EJ_PROP_OBJECT;
		}
		writeHeader();
		if (propval)
			writeToEb(propval, head.proplength);
		nzFree(propval);
		propval = 0;
		break;

	case EJ_CMD_SETAREL:
		setret = false;
		if (head.proptype == EJ_PROP_INSTANCE) {
			JSClass *cp = classByName(propval);
			nzFree(propval);
			propval = 0;
			child = JS_NewObject(jcx, cp, NULL, parent);
			if (!child)
				misconfigure();
			else
				set_array_element_object1(parent, head.n,
							  child);
			setret = true;
			propval = cloneString(pointer2string(*child.address()));
		}
		if (head.proptype == EJ_PROP_OBJECT && propval) {
			chp = string2pointer(propval);
			child = chp;
			set_array_element_object1(parent, head.n, child);
			nzFree(propval);
			propval = 0;
		}
		goto propreturn;

	case EJ_CMD_ARLEN:
		if (JS_GetArrayLength(jcx, parent, &len) == JS_FALSE)
			head.n = -1;
		else
			head.n = len;
		head.proplength = 0;
		writeHeader();
		break;

	case EJ_CMD_CALL:
		propval = run_function(parent, membername);
		nzFree(membername);
		membername = 0;
		head.proplength = head.n = 0;
		if (propval)
			head.proplength = strlen(propval);
		head.proptype = proptype;
		writeHeader();
		if (propval)
			writeToEb(propval, head.proplength);
		nzFree(propval);
		propval = 0;
		break;

	default:
		fprintf(stderr, "Unexpected message command %d from edbrowse\n",
			head.cmd);
		exit(6);
	}
}				/* processMessage */
