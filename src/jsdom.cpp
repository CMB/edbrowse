/* jsdom.cpp
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
#include <jsfriendapi.h>
#include <iostream>

int jsPool = 32;		/* size of js memory space in megabytes */

static JSRuntime *jrt;		/* our js runtime environment */
static const size_t gStackChunkSize = 8192;
static const char *emptyParms[] = { 0 };
static jsval emptyArgs[] = { jsval() };

static void
my_ErrorReporter(JSContext * cx, const char *message, JSErrorReport * report)
{
	if (report && report->errorNumber == JSMSG_OUT_OF_MEMORY ||
	    message && strstr(message, "out of memory")) {
		i_puts(MSG_JavaMemError);
		javaSessionFail();
	} else if (debugLevel < 2)
		goto done;
	if (ismc)
		goto done;

	if (!report) {
		cerr << message << endl;
		goto done;
	}

/* Conditionally ignore reported warnings. */
	if (JSREPORT_IS_WARNING(report->flags)) {
		if (browseLocal)
			goto done;
	}

	if (report->filename)
		cerr << report->filename << ": ";
	if (report->lineno)
		cerr << report->lineno << ": ";
	cerr << message << endl;

done:
	if (report)
		report->flags = 0;
}				/* my_ErrorReporter */

/* clean up if javascript fails */
void javaSessionFail()
{
	i_puts(MSG_JSSessionFail);
	cw->js_failed = eb_true;
}

JSString *our_JS_NewStringCopyN(JSContext * cx, const char *s, size_t n)
{
/* Fixme this is too simple.  We need to decode UTF8 to JSCHAR, for proper
 * unicode handling.  E.G., JS C strings are not UTF8, but the user has
 * a UTF8 locale. */
	return JS_NewStringCopyN(cw->jss->jcx, s, n);
}				/* our_JS_NewStringCopyN */

JSString *our_JS_NewStringCopyZ(JSContext * cx, const char *s)
{
	size_t len = strlen(s);
	return our_JS_NewStringCopyN(cw->jss->jcx, s, len);
}				/* our_JS_NewStringCopyZ */

char *our_JSEncodeString(js::HandleString str)
{
	size_t encodedLength = JS_GetStringEncodingLength(cw->jss->jcx, str);
	char *buffer = (char *)allocMem(encodedLength + 1);
	buffer[encodedLength] = '\0';
	size_t result =
	    JS_EncodeStringToBuffer(cw->jss->jcx, str, buffer, encodedLength);
	if (result == (size_t) - 1)
		javaSessionFail();
	return buffer;
}				/* our_JSEncodeString */

static char *transcode_get_js_bytes(js::HandleString s)
{
/* again, assume that the UTF8 mess will hopefully be ok,
we really should switch to unicode capable js functions at some stage */
	return our_JSEncodeString(s);
}				/* our_JS_GetTranscodedBytes */

/*********************************************************************
When an element is created without a name, it is not linked to its
owner (via that name), and can be cleared via garbage collection.
This is a disaster!
Create a fake name, so we can attach the element.
*********************************************************************/

static const char *fakePropName(void)
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
	JS_PropertyStub,
	JS_DeletePropertyStub,
	JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub,
	JS_ResolveStub,
	JS_ConvertStub,
	NULL,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool window_ctor(JSContext * cx, unsigned int argc, jsval * vp)
{
	char *newloc = 0;
	const char *winname = 0;
	JS::RootedString str(cx);
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JSObject & callee = args.callee();
	jsval callee_val = JS::ObjectValue(callee);
	JS::RootedObject newwin(cx,
				JS_NewObjectForConstructor(cx, &window_class,
							   &callee_val));
	if (newwin == NULL) {
		javaSessionFail();
		return JS_FALSE;
	}
	if (args.length() > 0 && (str = JS_ValueToString(cx, args[0]))) {
		newloc = our_JSEncodeString(str);
	}
	if (args.length() > 1 && (str = JS_ValueToString(cx, args[1]))) {
		winname = transcode_get_js_bytes(str);
	}
/* third argument is attributes, like window size and location, that we don't care about. */
	javaOpensWindow(newloc, winname);
	nzFree(newloc);
	establish_property_object(newwin, "opener", cw->jss->jwin);
	if (cw->js_failed)
		return JS_FALSE;
	args.rval().set(OBJECT_TO_JSVAL(newwin));
	return JS_TRUE;
}				/* window_ctor */

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

static JSBool setAttribute(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	js::RootedValue v1(cx), v2(cx);
	if (args.length() != 2 || !JSVAL_IS_STRING(args[0])) {
		JS_ReportError(cx, "unexpected arguments to setAttribute()");
	} else {
		v1 = args[0];
		v2 = args[1];
		const char *prop = stringize(v1);
		if (JS_DefineProperty(cx, obj, prop, v2, NULL, NULL, PROP_FIXED)
		    == JS_FALSE) {
			javaSessionFail();
			return JS_FALSE;
		}
	}
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* setAttribute */

static JSBool appendChild(JSContext * cx, unsigned int argc, jsval * vp)
{
	unsigned length;
	js::RootedValue v(cx);
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
	if (JS_GetProperty(cx, obj, "elements", v.address()) == JS_FALSE) {
		javaSessionFail();
		return JS_FALSE;
	}
	JS::RootedObject elar(cx, JSVAL_TO_OBJECT(v));
	if (elar == NULL) {
		javaSessionFail();
		return JS_FALSE;
	}
	if (JS_GetArrayLength(cx, elar, &length) == JS_FALSE) {
		javaSessionFail();
		return JS_FALSE;
	}
	if (JS_DefineElement(cx, elar, length,
			     (args.length() > 0 ? args[0] : JSVAL_NULL), NULL,
			     NULL, JSPROP_ENUMERATE) == JS_FALSE) {
		javaSessionFail();
		return JS_FALSE;
	}
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* appendChild */

static JSBool win_close(JSContext * cx, unsigned int argc, jsval * vp)
{
/* It's too confusing to just close the window */
	i_puts(MSG_PageDone);
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* win_close */

static JSBool win_alert(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	char *msg = NULL;
	JS::RootedString str(cx);
	if (args.length() > 0 && (str = JS_ValueToString(cx, args[0]))) {
		msg = transcode_get_js_bytes(str);
	}
	if (msg) {
		puts(msg);
		nzFree(msg);
	}
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* win_alert */

static JSBool win_prompt(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	char *msg = EMPTYSTRING;
	char *answer = EMPTYSTRING;
	JS::RootedString str(cx);
	char inbuf[80];
	char *s;
	char c;

	if (args.length() > 0 && (str = JS_ValueToString(cx, args[0]))) {
		msg = transcode_get_js_bytes(str);
	}
	if (args.length() > 1 && (str = JS_ValueToString(cx, args[1]))) {
		answer = transcode_get_js_bytes(str);
	}

	printf("%s", msg);
/* If it doesn't end in space or question mark, print a colon */
	c = 'x';
	if (*msg)
		c = msg[strlen(msg) - 1];
	if (!isspaceByte(c)) {
		if (!ispunctByte(c))
			printf(":");
		printf(" ");
	}
	if (answer)
		printf("[%s] ", answer);
	fflush(stdout);
	if (!fgets(inbuf, sizeof(inbuf), stdin))
		exit(1);
	s = inbuf + strlen(inbuf);
	if (s > inbuf && s[-1] == '\n')
		*--s = 0;
	if (inbuf[0]) {
		nzFree(answer);	/* Don't need the default answer anymore. */
		answer = inbuf;
	}
	args.rval().set(STRING_TO_JSVAL(our_JS_NewStringCopyZ(cx, answer)));
	return JS_TRUE;
}				/* win_prompt */

static JSBool win_confirm(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	char *msg = EMPTYSTRING;
	JS::RootedString str(cx);
	char inbuf[80];
	char c;
	eb_bool first = eb_true;

	if (args.length() > 0 && (str = JS_ValueToString(cx, args[0]))) {
		msg = transcode_get_js_bytes(str);
	}

	while (eb_true) {
		printf("%s", msg);
		c = 'x';
		if (*msg)
			c = msg[strlen(msg) - 1];
		if (!isspaceByte(c)) {
			if (!ispunctByte(c))
				printf(":");
			printf(" ");
		}
		if (!first)
			printf("[y|n] ");
		first = eb_false;
		fflush(stdout);
		if (!fgets(inbuf, sizeof(inbuf), stdin))
			exit(1);
		c = *inbuf;
		if (c && strchr("nNyY", c))
			break;
	}

	c = tolower(c);
	if (c == 'y')
		args.rval().set(JSVAL_TRUE);
	else
		args.rval().set(JSVAL_FALSE);
	nzFree(msg);
	return JS_TRUE;
}				/* win_confirm */

static JSClass timer_class = {
	"Timer",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub,
	JS_DeletePropertyStub,
	JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub,
	JS_ResolveStub,
	JS_ConvertStub,
	NULL,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

/* Set a timer or an interval */
static JSObject *setTimeout(unsigned int argc, jsval * argv, eb_bool isInterval)
{
	JS::RootedValue v0(cw->jss->jcx), v1(cw->jss->jcx);
	JS::RootedObject fo(cw->jss->jcx), to(cw->jss->jcx);
	int n;			/* number of milliseconds */
	char fname[48];		/* function name */
	const char *fstr;	/* function string */
	const char *methname = (isInterval ? "setInterval" : "setTimeout");
	char *allocatedName = NULL;
	char *s = NULL;

	if (!parsePage) {
		JS_ReportError(cw->jss->jcx,
			       "cannot use %s() to delay the execution of a function",
			       methname);
		return NULL;
	}

	if (argc != 2 || !JSVAL_IS_INT(argv[1]))
		goto badarg;

	v0 = argv[0];
	v1 = argv[1];
	n = JSVAL_TO_INT(v1);
	if (JSVAL_IS_STRING(v0) ||
	    v0.isObject() &&
	    JS_ValueToObject(cw->jss->jcx, v0, fo.address()) &&
	    JS_ObjectIsFunction(cw->jss->jcx, fo)) {

/* build the tag object and link it to window */
		to = JS_NewObject(cw->jss->jcx, &timer_class, NULL,
				  cw->jss->jwin);
		if (to == NULL) {
abort:
			javaSessionFail();
			return NULL;
		}
		v1 = OBJECT_TO_JSVAL(to);
		if (JS_DefineProperty
		    (cw->jss->jcx, cw->jss->jwin, fakePropName(), v1, NULL,
		     NULL, JSPROP_READONLY | JSPROP_PERMANENT) == JS_FALSE)
			goto abort;
		if (fo) {
/* Extract the function name, which requires several steps */
			js::RootedFunction f(cw->jss->jcx,
					     JS_ValueToFunction(cw->jss->jcx,
								OBJECT_TO_JSVAL
								(fo)));
			JS::RootedString jss(cw->jss->jcx, JS_GetFunctionId(f));
			if (jss)
				allocatedName = our_JSEncodeString(jss);
			s = allocatedName;
/* Remember that unnamed functions are named anonymous. */
			if (!s || !*s || stringEqual(s, "anonymous"))
/* avoid compiler warning about string conversion */
				s = (char *)string("javascript").c_str();
			int len = strlen(s);
			if (len > sizeof(fname) - 4)
				len = sizeof(fname) - 4;
			strncpy(fname, s, len);
			nzFree(allocatedName);
			fname[len] = 0;
			strcat(fname, "()");
			fstr = fname;
			establish_property_object(to, "onclick", fo);
			if (cw->js_failed)
				return NULL;
		} else {
/* compile the function from the string */
			fstr = stringize(v0);
			if (JS_CompileFunction(cw->jss->jcx, to, "onclick", 0, emptyParms,	/* no named parameters */
					       fstr, strlen(fstr), "onclick",
					       1) == NULL) {
				JS_ReportError(cw->jss->jcx,
					       "error compiling function in %s()",
					       methname);
				return NULL;
			}
		}

		javaSetsTimeout(n, fstr, to, isInterval);
		return to;
	}

badarg:
	JS_ReportError(cw->jss->jcx, "invalid arguments to %s()", methname);
	return NULL;
}				/* setTimeout */

static JSBool win_sto(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject obj(cw->jss->jcx,
			     setTimeout(args.length(), args.array(), eb_false));
	if (obj == NULL)
		return JS_FALSE;
	args.rval().set(OBJECT_TO_JSVAL(obj));
	return JS_TRUE;
}				/* win_sto */

static JSBool win_intv(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject obj(cw->jss->jcx,
			     setTimeout(args.length(), args.array(), eb_true));
	if (obj == NULL)
		return JS_FALSE;
	args.rval().set(OBJECT_TO_JSVAL(obj));
	return JS_TRUE;
}				/* win_intv */

static JSFunctionSpec window_methods[] = {
	JS_FS("alert", win_alert, 1, 0),
	JS_FS("prompt", win_prompt, 2, 0),
	JS_FS("confirm", win_confirm, 1, 0),
	JS_FS("setTimeout", win_sto, 2, 0),
	JS_FS("setInterval", win_intv, 2, 0),
	JS_FS("close", win_close, 0, 0),
	JS_FS("focus", nullFunction, 0, 0),
	JS_FS("blur", nullFunction, 0, 0),
	JS_FS("scroll", nullFunction, 0, 0),
	JS_FS_END
};

static JSClass doc_class = {
	"Document",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub,
	JS_DeletePropertyStub,
	JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub,
	JS_ResolveStub,
	JS_ConvertStub,
	NULL,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static void dwrite2(const char *s)
{
	if (!cw->dw) {
		cw->dw = initString(&cw->dw_l);
		stringAndString(&cw->dw, &cw->dw_l, "<docwrite>");
	}
	stringAndString(&cw->dw, &cw->dw_l, s);
}				/* dwrite2 */

static void dwrite1(unsigned int argc, jsval * argv, eb_bool newline)
{
	int i;
	char *msg;
	JS::RootedString str(cw->jss->jcx);
	for (i = 0; i < argc; ++i) {
		if ((str = JS_ValueToString(cw->jss->jcx, argv[i])) &&
		    (msg = transcode_get_js_bytes(str))) {
			dwrite2(msg);
			nzFree(msg);
		}
	}
	if (newline)
		dwrite2("\n");
}				/* dwrite1 */

static JSBool doc_write(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	dwrite1(args.length(), args.array(), eb_false);
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* doc_write */

static JSBool
setter_innerHTML(JSContext * cx, JS::Handle < JSObject * >obj,
		 JS::Handle < jsid > id, JSBool strict,
		 JS::MutableHandle < jsval > vp)
{
	const char *s = stringize(vp);
	if (s && strlen(s)) {
		dwrite2(parsePage ? "<hr>\n" : "<html>\n");
		dwrite2(s);
		if (s[strlen(s) - 1] != '\n')
			dwrite2("\n");
	}
/* The string has already been updated in the object. */
	return JS_TRUE;
}				/* setter_innerHTML */

static JSBool
setter_innerText(JSContext * cx, JS::Handle < JSObject * >obj,
		 JS::Handle < jsid > id, JSBool strict,
		 JS::MutableHandle < jsval > vp)
{
	char *s;
	if (!JSVAL_IS_STRING(vp))
		return JS_FALSE;
	js::RootedString jstr(cx, JSVAL_TO_STRING(vp));
	s = our_JSEncodeString(jstr);
	nzFree(s);
	i_puts(MSG_InnerText);
/* The string has already been updated in the object. */
	return JS_TRUE;
}				/* setter_innerText */

static JSBool doc_writeln(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	dwrite1(args.length(), args.array(), eb_true);
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* doc_writeln */

static JSFunctionSpec doc_methods[] = {
	JS_FS("focus", nullFunction, 0, 0),
	JS_FS("blur", nullFunction, 0, 0),
	JS_FS("open", nullFunction, 0, 0),
	JS_FS("close", nullFunction, 0, 0),
	JS_FS("write", doc_write, 0, 0),
	JS_FS("writeln", doc_writeln, 0, 0),
	JS_FS_END
};

static JSClass element_class = {
	"Element",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub,
	JS_DeletePropertyStub,
	JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub,
	JS_ResolveStub,
	JS_ConvertStub,
	NULL,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSFunctionSpec element_methods[] = {
	JS_FS("focus", nullFunction, 0, 0),
	JS_FS("blur", nullFunction, 0, 0),
	JS_FS_END
};

static JSClass form_class = {
	"Form",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub,
	JS_DeletePropertyStub,
	JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub,
	JS_ResolveStub,
	JS_ConvertStub,
	NULL,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool form_submit(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
	javaSubmitsForm(obj, eb_false);
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* form_submit */

static JSBool form_reset(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
	javaSubmitsForm(obj, eb_true);
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* form_reset */

static JSFunctionSpec form_methods[] = {
	JS_FS("submit", form_submit, 0, 0),
	JS_FS("reset", form_reset, 0, 0),
	JS_FS_END
};

static JSClass body_class = {
	"Body",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
	NULL,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSFunctionSpec body_methods[] = {
	JS_FS("setAttribute", setAttribute, 2, 0),
	JS_FS("appendChild", appendChild, 1, 0),
	JS_FS_END
};

static JSClass head_class = {
	"Head",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
	NULL,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSFunctionSpec head_methods[] = {
	JS_FS("setAttribute", setAttribute, 2, 0),
	JS_FS("appendChild", appendChild, 1, 0),
	JS_FS_END
};

static JSClass meta_class = {
	"Meta",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
	NULL,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

/* Don't be confused; this is for <link>, not <a> */
static JSClass link_class = {
	"Link",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
	NULL, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSClass script_class = {
	"Script",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
	NULL, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSFunctionSpec link_methods[] = {
	JS_FS("setAttribute", setAttribute, 2, 0),
	JS_FS_END
};

static JSClass image_class = {
	"Image",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
	NULL, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSClass frame_class = {
	"Frame",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
	NULL, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSClass anchor_class = {
	"Anchor",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
	NULL, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSClass table_class = {
	"Table",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
	NULL, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSClass trow_class = {
	"Trow",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
	NULL, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSClass cell_class = {
	"Cell",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
	NULL, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSClass div_class = {
	"Div",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
	NULL, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSClass span_class = {
	"Span",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
	NULL, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSClass area_class = {
	"Area",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
	NULL, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSClass option_class = {
	"Option",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
	NULL, JSCLASS_NO_OPTIONAL_MEMBERS
};

struct DOMCLASS {
	JSClass *obj_class;
	JSFunctionSpec *methods;
	JSNative constructor;
	int nargs;
};
// set of constructors
static JSBool element_ctor(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JSObject & callee = args.callee();
	jsval callee_val = JS::ObjectValue(callee);
	JS::RootedObject newobj(cx,
				JS_NewObjectForConstructor(cx, &element_class,
							   &callee_val));
	if (newobj == NULL) {
		javaSessionFail();
		return JS_FALSE;
	}
	args.rval().set(OBJECT_TO_JSVAL(newobj));
	return JS_TRUE;
}

static JSBool body_ctor(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JSObject & callee = args.callee();
	jsval callee_val = JS::ObjectValue(callee);
	JS::RootedObject newobj(cx,
				JS_NewObjectForConstructor(cx, &body_class,
							   &callee_val));
	if (newobj == NULL) {
		javaSessionFail();
		return JS_FALSE;
	}
	args.rval().set(OBJECT_TO_JSVAL(newobj));
	return JS_TRUE;
}

static JSBool head_ctor(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JSObject & callee = args.callee();
	jsval callee_val = JS::ObjectValue(callee);
	JS::RootedObject newobj(cx,
				JS_NewObjectForConstructor(cx, &head_class,
							   &callee_val));
	if (newobj == NULL) {
		javaSessionFail();
		return JS_FALSE;
	}
	args.rval().set(OBJECT_TO_JSVAL(newobj));
	return JS_TRUE;
}

static JSBool form_ctor(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JSObject & callee = args.callee();
	jsval callee_val = JS::ObjectValue(callee);
	JS::RootedObject newobj(cx,
				JS_NewObjectForConstructor(cx, &form_class,
							   &callee_val));
	if (newobj == NULL) {
		javaSessionFail();
		return JS_FALSE;
	}
	args.rval().set(OBJECT_TO_JSVAL(newobj));
	return JS_TRUE;
}

static JSBool link_ctor(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JSObject & callee = args.callee();
	jsval callee_val = JS::ObjectValue(callee);
	JS::RootedObject newobj(cx,
				JS_NewObjectForConstructor(cx, &link_class,
							   &callee_val));
	if (newobj == NULL) {
		javaSessionFail();
		return JS_FALSE;
	}
	args.rval().set(OBJECT_TO_JSVAL(newobj));
	return JS_TRUE;
}

static JSBool script_ctor(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JSObject & callee = args.callee();
	jsval callee_val = JS::ObjectValue(callee);
	JS::RootedObject newobj(cx,
				JS_NewObjectForConstructor(cx, &script_class,
							   &callee_val));
	if (newobj == NULL) {
		javaSessionFail();
		return JS_FALSE;
	}
	args.rval().set(OBJECT_TO_JSVAL(newobj));
	return JS_TRUE;
}

static JSBool image_ctor(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JSObject & callee = args.callee();
	jsval callee_val = JS::ObjectValue(callee);
	JS::RootedObject newobj(cx,
				JS_NewObjectForConstructor(cx, &image_class,
							   &callee_val));
	if (newobj == NULL) {
		javaSessionFail();
		return JS_FALSE;
	}
	args.rval().set(OBJECT_TO_JSVAL(newobj));
	return JS_TRUE;
}

static JSBool anchor_ctor(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JSObject & callee = args.callee();
	jsval callee_val = JS::ObjectValue(callee);
	JS::RootedObject newobj(cx,
				JS_NewObjectForConstructor(cx, &anchor_class,
							   &callee_val));
	if (newobj == NULL) {
		javaSessionFail();
		return JS_FALSE;
	}
	args.rval().set(OBJECT_TO_JSVAL(newobj));
	return JS_TRUE;
}

static JSBool option_ctor(JSContext * cx, unsigned int argc, jsval * vp)
{
	char *text = 0;
	char *value = 0;
	JS::RootedString str(cx);
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JSObject & callee = args.callee();
	jsval callee_val = JS::ObjectValue(callee);
	JS::RootedObject newopt(cx,
				JS_NewObjectForConstructor(cx, &option_class,
							   &callee_val));
	if (newopt == NULL) {
		javaSessionFail();
		return JS_FALSE;
	}
	if (args.length() > 0 && (str = JS_ValueToString(cx, args[0]))) {
		text = our_JSEncodeString(str);
	}
	if (args.length() > 1 && (str = JS_ValueToString(cx, args[1]))) {
		value = transcode_get_js_bytes(str);
	}
	if (text) {
		establish_property_string(newopt, "text", text, eb_true);
		nzFree(text);
	}
	if (value) {
		establish_property_string(newopt, "value", value, eb_true);
		nzFree(value);
	}
	establish_property_bool(newopt, "selected", eb_false, eb_false);
	establish_property_bool(newopt, "defaultSelected", eb_false, eb_true);
	args.rval().set(OBJECT_TO_JSVAL(newopt));
	return JS_TRUE;
}				/* option_ctor */

static struct DOMCLASS domClasses[] = {
	{&element_class, element_methods, element_ctor, 0},
	{&form_class, form_methods, form_ctor},
	{&body_class, body_methods, body_ctor},
	{&head_class, head_methods},
	{&meta_class},
	{&link_class, link_methods, link_ctor, 0},
	{&image_class, 0, image_ctor, 1},
	{&frame_class},
	{&anchor_class, 0, anchor_ctor, 1},
	{&table_class},
	{&div_class},
	{&area_class},
	{&span_class},
	{&trow_class},
	{&cell_class},
	{&option_class, 0, option_ctor, 2},
	{&script_class, 0, script_ctor},
	{0}
};

void createJavaContext(struct ebWindowJSState **pstate)
{
	struct ebWindowJSState *state = new ebWindowJSState;
/* navigator mime types and plugins */
	const char *itemname;
	int i;
	extern const char startWindowJS[];
	jsval rval;
	struct MIMETYPE *mt;
	const char *languages[] = { 0,
		"english", "french", "portuguese", "polish"
	};

	if (!jrt) {
/* space configurable by jsPool; default is 32 meg */
		jrt =
		    JS_NewRuntime(jsPool * 1024L * 1024L, JS_NO_HELPER_THREADS);
	}
	if (!jrt) {
		i_puts(MSG_JavaMemError);
		delete state;
		*pstate = NULL;
		return;
	}

	state->jcx = JS_NewContext(jrt, gStackChunkSize);
	if (!state->jcx) {
		i_puts(MSG_JavaContextError);
		delete state;
		*pstate = NULL;
		return;
	}
	JSAutoRequest autoreq(state->jcx);
/* It's looking good. */
	*pstate = state;
	JS_SetErrorReporter(state->jcx, my_ErrorReporter);
	JS_SetOptions(state->jcx, JSOPTION_VAROBJFIX);
/* Create the Window object, which is the global object in DOM. */
	state->jwin = JS_NewGlobalObject(state->jcx, &window_class, NULL);
	if (!state->jwin) {
		i_puts(MSG_JavaWindowError);
		*pstate = NULL;
		JS_DestroyContext(state->jcx);
		delete state;
		return;
	}

/* enter the compartment for this object for the duration of this function */
	JSAutoCompartment ac(state->jcx, state->jwin);
/* now set the state->jwin object as global */
	JS_SetGlobalObject(state->jcx, state->jwin);
/* Math, Date, Number, String, etc */
	if (!JS_InitStandardClasses(state->jcx, state->jwin)) {
		i_puts(MSG_JavaClassError);
abort:
		javaSessionFail();
		return;
	}
/* initialise the window class */
	if (JS_InitClass
	    (state->jcx, state->jwin, NULL, &window_class, window_ctor, 3, NULL,
	     window_methods, NULL, NULL) == NULL)
		goto abort;
/* Ok, but the global object was created before the class,
 * so it doesn't have its methods yet. */
	if (JS_DefineFunctions(state->jcx, state->jwin, window_methods) ==
	    JS_FALSE)
		goto abort;
/* if js dies these'll just silently return */
	establish_property_object(state->jwin, "window", state->jwin);
	establish_property_object(state->jwin, "self", state->jwin);
	establish_property_object(state->jwin, "parent", state->jwin);
	establish_property_object(state->jwin, "top", state->jwin);

/* Other classes that we'll need. */
	for (i = 0; domClasses[i].obj_class; ++i) {
		if (JS_InitClass(state->jcx, state->jwin, 0,
				 domClasses[i].obj_class,
				 domClasses[i].constructor, domClasses[i].nargs,
				 NULL, domClasses[i].methods, NULL,
				 NULL) == NULL)
			goto abort;
	}

	if (!initLocationClass())
		goto abort;

/* document under window */
	if (JS_InitClass(state->jcx, state->jwin, 0, &doc_class, NULL, 0,
			 NULL, doc_methods, NULL, NULL) == NULL)
		goto abort;

	state->jdoc = JS_NewObject(state->jcx, &doc_class, NULL, state->jwin);
	if (!state->jdoc) {
		i_puts(MSG_JavaObjError);
		goto abort;
	}
	establish_property_object(state->jwin, "document", state->jdoc);

	establish_property_string(state->jdoc, "referrer", cw->referrer,
				  eb_true);
	establish_property_url(state->jdoc, "URL", cw->fileName, eb_true);
	establish_property_url(state->jdoc, "location", cw->fileName, eb_false);
	establish_property_url(state->jwin, "location", cw->firstURL, eb_false);
	establish_property_string(state->jdoc, "domain",
				  getHostURL(cw->fileName), eb_false);
	if (cw->js_failed)
		return;

	JS::RootedObject nav(state->jcx, JS_NewObject(state->jcx, 0, 0,
						      state->jwin));
	if (nav == NULL)
		goto abort;
	establish_property_object(state->jwin, "navigator", nav);
	if (cw->js_failed)
		return;
/* most of the navigator is in startwindow.js; the language items are here. */
	establish_property_string(nav, "userLanguage", languages[eb_lang],
				  eb_true);
	establish_property_string(nav, "language", languages[eb_lang], eb_true);
	if (cw->js_failed)
		return;

/* Build the array of mime types and plugins,
 * according to the entries in the config file. */
	JS::RootedObject navpi(state->jcx, establish_property_array(nav,
								    "plugins"));
	if (navpi == NULL)
		goto abort;
	JS::RootedObject navmt(state->jcx, establish_property_array(nav,
								    "mimeTypes"));
	if (navmt == NULL)
		goto abort;
	mt = mimetypes;
	for (i = 0; i < maxMime; ++i, ++mt) {
/* po is the plugin object and mo is the mime object */
		JS::RootedObject mo(state->jcx), po(state->jcx);
		JS::RootedValue mov(state->jcx), pov(state->jcx);
		js::RootedString jstr(state->jcx);
		int len;

		po = JS_NewObject(state->jcx, 0, 0, nav);
		if (po == NULL)
			goto abort;
		pov = OBJECT_TO_JSVAL(po);
		if (JS_DefineElement(state->jcx, navpi, i, pov, NULL, NULL,
				     PROP_FIXED) == JS_FALSE)
			goto abort;
		mo = JS_NewObject(state->jcx, 0, 0, nav);
		if (mo == NULL)
			goto abort;
		mov = OBJECT_TO_JSVAL(mo);
		if (JS_DefineElement(state->jcx, navmt, i, mov, NULL, NULL,
				     PROP_FIXED) == JS_FALSE)
			goto abort;
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
		if (cw->js_failed)
			return;
/* For the name, how about the program without its options? */
		len = strcspn(mt->program, " \t");
		js::RootedValue po_name_v(state->jcx,
					  STRING_TO_JSVAL(our_JS_NewStringCopyN
							  (state->jcx,
							   mt->program, len)));
		if (JS_DefineProperty(state->jcx, po, "name", po_name_v, 0, 0,
				      PROP_FIXED) == JS_FALSE)
			goto abort;
	}

	JS::RootedObject hist(state->jcx, JS_NewObject(state->jcx, 0, 0,
						       state->jwin));
	if (hist == NULL)
		goto abort;
	establish_property_object(state->jwin, "history", hist);
	establish_property_string(hist, "current", cw->fileName, eb_true);
	if (cw->js_failed)
		return;
/* Since there is no history in edbrowse, the rest is left to startwindow.js */

/* the js window/document setup script */
	if (JS_EvaluateScript(state->jcx, state->jwin, startWindowJS,
			      strlen(startWindowJS), "startwindow", 1,
			      &rval) == JS_FALSE)
		goto abort;
}				/* createJavaContext */

// free java context and associated state
void freeJavaContext(struct ebWindowJSState *state)
{
	if (state == NULL)
		return;
	JSContext *oldcontext = state->jcx;
	delete state;		// clear the heap rooted things in state first
	if (oldcontext != NULL) {
		if (js::GetContextCompartment((const JSContext *)oldcontext) !=
		    NULL)
			JS_LeaveCompartment(oldcontext, NULL);
		JS_DestroyContext(oldcontext);
	}
}				/* freeJavaContext */

void
establish_innerHTML(JS::HandleObject jv, const char *start, const char *end,
		    eb_bool is_ta)
{
	SWITCH_COMPARTMENT();
	JS::RootedObject o(cw->jss->jcx);
	JS::RootedValue v(cw->jss->jcx);

/* null start means the pointer has been corrupted by a document.write() call */
	if (!start)
		start = end = EMPTYSTRING;
	v = STRING_TO_JSVAL(our_JS_NewStringCopyN
			    (cw->jss->jcx, start, end - start));
	if (JS_DefineProperty(cw->jss->jcx, jv, "innerHTML", v, NULL,
			      (is_ta ? setter_innerText : setter_innerHTML),
			      JSPROP_ENUMERATE | JSPROP_PERMANENT) == JS_FALSE)
	{
		javaSessionFail();
		return;
	}
	if (is_ta) {
		if (JS_DefineProperty(cw->jss->jcx, jv, "innerText", v,
				      NULL, setter_innerText,
				      JSPROP_ENUMERATE | JSPROP_PERMANENT) ==
		    JS_FALSE) {
			javaSessionFail();
			return;
		}
	}

/* Anything with an innerHTML might also have a style. */
	o = JS_NewObject(cw->jss->jcx, 0, 0, jv);
	if (o == NULL) {
		javaSessionFail();
		return;
	}
	v = OBJECT_TO_JSVAL(o);
	if (JS_DefineProperty(cw->jss->jcx, jv, "style", v, NULL, NULL,
			      JSPROP_ENUMERATE) == JS_FALSE) {
		javaSessionFail();
		return;
	}
}				/* establish_innerHTML */

eb_bool
javaParseExecute(JS::HandleObject obj, const char *str, const char *filename,
		 int lineno)
{
	SWITCH_COMPARTMENT(eb_false);
	JSBool ok;
	eb_bool rc = eb_false;
	js::RootedValue rval(cw->jss->jcx);

/* Sometimes Mac puts these three chars at the start of a text file. */
	if (!strncmp(str, "\xef\xbb\xbf", 3))
		str += 3;

	debugPrint(6, "javascript:\n%s", str);
	ok = JS_EvaluateScript(cw->jss->jcx, obj, str, strlen(str),
			       filename, lineno, rval.address());
	if (ok) {
		rc = eb_true;
		if (JSVAL_IS_BOOLEAN(rval))
			rc = JSVAL_TO_BOOLEAN(rval);
	}
	return rc;
}				/* javaParseExecute */

eb_bool
javaParseExecuteGlobal(const char *str, const char *filename, int lineno)
{
	return javaParseExecute(cw->jss->jwin, str, filename, lineno);
}				/* javaParseExecuteGlobal */

/* link a frame, span, anchor, etc to the document object model */
void domLink(const char *classname,	/* instantiate this class */
	     const char *href, const char *list,	/* next member of this array */
	     JS::HandleObject owner, int radiosel)
{
	SWITCH_COMPARTMENT();

	JS::RootedObject w(cw->jss->jcx), alist(cw->jss->jcx),
	    master(cw->jss->jcx);
	JS::RootedObject v(cw->jss->jcx);
	unsigned length, attr = PROP_FIXED;
	JSClass *cp;
	JS::RootedValue vv(cw->jss->jcx), listv(cw->jss->jcx);
	eb_bool dupname = eb_false;
	int i;

/* some strings from the html tag */
	const char *symname = topTag->name;
	const char *idname = topTag->id;
	const char *href_url = topTag->href;
	const char *htmlclass = topTag->classname;

/* find the class */
	for (i = 0; cp = domClasses[i].obj_class; ++i)
		if (stringEqual(cp->name, classname))
			break;

	if (symname) {
		JSBool found;
		JS_HasProperty(cw->jss->jcx, owner, symname, &found);
		if (found) {

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

			if (stringEqual(symname, "action")) {
				JS::RootedObject ao(cw->jss->jcx);	/* action object */
				if (JS_GetProperty(cw->jss->jcx, owner,
						   symname,
						   vv.address()) == JS_FALSE)
					goto abort;
				ao = JSVAL_TO_OBJECT(vv);
/* actioncrash tells me if we've already had this collision */
				JS_HasProperty(cw->jss->jcx, ao, "actioncrash",
					       &found);
				if (!found) {
					if (JS_DeleteProperty(cw->jss->jcx,
							      owner,
							      symname) ==
					    JS_FALSE)
						goto abort;
/* gc will clean this up later */
/* advance, as though this were not found */
					goto afterfound;
				}
			}

			if (radiosel == 1) {
				if (JS_GetProperty(cw->jss->jcx, owner,
						   symname,
						   vv.address()) == JS_FALSE)
					goto abort;
				v = JSVAL_TO_OBJECT(vv);
			} else {
				dupname = eb_true;
			}
		}
	}

afterfound:
	if (!v) {
		if (radiosel) {
			v = JS_NewArrayObject(cw->jss->jcx, 0, NULL);
			if (v == NULL)
				goto abort;
			if (radiosel == 1) {
				establish_property_string(v, "type", "radio",
							  eb_true);
				if (cw->js_failed)
					return;
			} else {
/* self-referencing - hope this is ok */
				establish_property_object(v, "options", v);
				establish_property_number(v, "selectedIndex",
							  -1, eb_false);
				if (cw->js_failed)
					return;
// not the normal pathway; we have to create our own element methods here.
				if (JS_DefineFunction(cw->jss->jcx, v, "focus",
						      nullFunction, 0,
						      PROP_FIXED) == NULL)
					goto abort;
				if (JS_DefineFunction(cw->jss->jcx, v, "blur",
						      nullFunction, 0,
						      PROP_FIXED) == NULL)
					goto abort;
			}
		} else {
			v = JS_NewObject(cw->jss->jcx, cp, NULL, owner);
			if (v == NULL)
				goto abort;
		}
		vv = OBJECT_TO_JSVAL(v);

/* if no name, then use id as name */
		if (!symname && idname) {
/*********************************************************************
This function call makes a direct link from the owner, usually the form,
to this tag under the name given by the id attribute,
if there is no name attribute.
Some websites use this link to get to the form element, but,
it should not displace the submit or reset functions, or the action parameter.
Example www.startpage.com, where id=submit
*********************************************************************/
			if (!stringEqual(idname, "submit") &&
			    !stringEqual(idname, "reset") &&
			    !stringEqual(idname, "action"))
				if (JS_DefineProperty(cw->jss->jcx, owner,
						      idname, vv, NULL, NULL,
						      attr) == JS_FALSE)
					goto abort;
		} else if (symname && !dupname) {
			if (JS_DefineProperty(cw->jss->jcx, owner, symname,
					      vv, NULL, NULL, attr) == JS_FALSE)
				goto abort;
			if (stringEqual(symname, "action"))
				establish_property_bool(v, "actioncrash",
							eb_true, eb_true);
			if (cw->js_failed)
				return;

/* link to document.all */
			if (JS_GetProperty(cw->jss->jcx, cw->jss->jdoc, "all",
					   listv.address()) == JS_FALSE)
				goto abort;
			master = JSVAL_TO_OBJECT(listv);
			establish_property_object(master, symname, v);
			if (cw->js_failed)
				return;
		} else {
/* tie this to something, to protect it from gc */
			if (JS_DefineProperty(cw->jss->jcx, owner,
					      fakePropName(), vv, NULL, NULL,
					      JSPROP_READONLY |
					      JSPROP_PERMANENT) == JS_FALSE)
				goto abort;
		}

		if (list) {
			if (JS_GetProperty(cw->jss->jcx, owner, list,
					   listv.address()) == JS_FALSE)
				goto abort;
			alist = JSVAL_TO_OBJECT(listv);
		}
		if (alist) {
			JS_GetArrayLength(cw->jss->jcx, alist, &length);
			if (JS_DefineElement
			    (cw->jss->jcx, alist, length, vv, NULL, NULL,
			     attr) == JS_FALSE)
				goto abort;
			if (symname && !dupname)
				establish_property_object(alist, symname, v);
			if (cw->js_failed)
				return;
			if (idname
			    && (!symname || !stringEqual(symname, idname)))
				establish_property_object(alist, idname, v);
			if (cw->js_failed)
				return;
		}		/* list indicated */
	}

	if (radiosel == 1) {
/* drop down to the element within the radio array, and return that element */
/* w becomes the object associated with this radio button */
/* v is, by assumption, an array */
		JS_GetArrayLength(cw->jss->jcx, v, &length);
		w = JS_NewObject(cw->jss->jcx, &element_class, NULL, owner);
		if (w == NULL)
			goto abort;
		vv = OBJECT_TO_JSVAL(w);
		if (JS_DefineElement
		    (cw->jss->jcx, v, length, vv, NULL, NULL, attr) == JS_FALSE)
			goto abort;
		v = w;
	}

	if (symname)
		establish_property_string(v, "name", symname, eb_true);
	if (cw->js_failed)
		return;
	if (idname) {
/* v.id becomes idname, and idMaster.idname becomes v
 * In case of forms, v.id should remain undefined.  So we can have
 * a form field named "id". */
		if (!stringEqual(classname, "Form"))
			establish_property_string(v, "id", idname, eb_true);
		if (cw->js_failed)
			return;
		if (JS_GetProperty(cw->jss->jcx, cw->jss->jdoc, "idMaster",
				   listv.address()) == JS_FALSE)
			goto abort;
		master = JSVAL_TO_OBJECT(listv);
		establish_property_object(master, idname, v);
		if (cw->js_failed)
			return;
	}

/* I use to set id = "" if no id tag, but I'm not sure why. */
#if 0
	if (!idname) {
		if (!stringEqual(classname, "Form"))
			establish_property_string(v, "id", EMPTYSTRING,
						  eb_true);
		if (cw->js_failed)
			return;
	}
#endif

	if (href && href_url) {
		establish_property_url(v, href, href_url, eb_false);
		if (cw->js_failed)
			return;
	}

	if (cp == &element_class) {
/* link back to the form that owns the element */
		establish_property_object(v, "form", owner);
		if (cw->js_failed)
			return;
	}

	if (htmlclass) {
		establish_property_string(v, "className", htmlclass, eb_false);
		if (cw->js_failed)
			return;
	}

	topTag->jv = v;
	makeParentNode(topTag);
	if (cw->js_failed)
		return;

	if (stringEqual(classname, "Body")) {
/* here are a few attributes that come in with the body */
		establish_property_object(cw->jss->jdoc, "body", v);
		establish_property_object(cw->jss->jdoc, "documentElement", v);
		establish_property_number(v, "clientHeight", 768, eb_true);
		establish_property_number(v, "clientWidth", 1024, eb_true);
		establish_property_number(v, "offsetHeight", 768, eb_true);
		establish_property_number(v, "offsetWidth", 1024, eb_true);
		establish_property_number(v, "scrollHeight", 768, eb_true);
		establish_property_number(v, "scrollWidth", 1024, eb_true);
		establish_property_number(v, "scrollTop", 0, eb_true);
		establish_property_number(v, "scrollLeft", 0, eb_true);
		if (cw->js_failed)
			return;
	}

	if (stringEqual(classname, "Head")) {
		establish_property_object(cw->jss->jdoc, "head", v);
	}

	return;

abort:
	javaSessionFail();
}				/* domLink */
