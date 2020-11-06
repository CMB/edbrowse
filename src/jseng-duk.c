/*********************************************************************
This is the back-end process for javascript.
This is the server, and edbrowse is the client.
We receive commands from edbrowse,
getting and setting properties for various DOM objects.

This is the duktape version.
If you package this with the duktape js libraries,
you will need to include the MIT open source license,
along with the GPL, general public license.

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

#ifdef DOSLIKE
#include "vsprtf.h"
#endif // DOSLIKE

#include <duktape.h>

static void processError(duk_context * cx);
static void jsInterruptCheck(duk_context * cx);

static duk_ret_t native_error_stub_0(duk_context * cx)
{
	return 0;
}

static duk_ret_t native_error_stub_1(duk_context * cx)
{
	i_puts(MSG_CompileError);
	return 0;
}

const char *jsSourceFile;	// sourcefile providing the javascript
int jsLineno;			// line number

static char *errorMessage;
static char *effects;
static int eff_l;
#define effectString(s) stringAndString(&effects, &eff_l, (s))
#define effectChar(s) stringAndChar(&effects, &eff_l, (s))

static duk_context *context0;
static jsobjtype context0_obj;

/* wrappers around duktape alloc functions: add our own header */
struct jsdata_wrap {
	union {
		uint64_t header;
		Tag *t;
	} u;
	char data[0];
};
#define jsdata_of(p) ((struct jsdata_wrap*)((char*)(p)-sizeof(struct jsdata_wrap)))

static void *watch_malloc(void *udata, size_t n)
{
	struct jsdata_wrap *w = malloc(n + sizeof(struct jsdata_wrap));
	if (!w)
		return NULL;
	w->u.t = 0;
	return w->data;
}

static void *watch_realloc(void *udata, void *p, size_t n)
{
	struct jsdata_wrap *w;

	if (!p)
		return watch_malloc(udata, n);

	w = jsdata_of(p);
	if (w->u.t != 0)
		debugPrint(1,
			   "realloc with a watched pointer, shouldn't happen");
	w = realloc(w, n + sizeof(struct jsdata_wrap));
	return w->data;
}

static void watch_free(void *udata, void *p)
{
	Tag *t;
	struct jsdata_wrap *w;

	if (!p)
		return;

	w = jsdata_of(p);
	t = w->u.t;
	free(w);
	if (t) {
		debugPrint(4, "gc %p", p);
		t->jslink = false;
		killTag(t);
	}
}

void connectTagObject(Tag *t, jsobjtype p)
{
	struct jsdata_wrap *w = jsdata_of(p);
	if (w->u.t)
		debugPrint(1, "multiple tags connect to js pointer %p", p);
	w->u.t = t;
	t->jv = p;
	t->jslink = true;
	set_property_number_0(t->f0->cx, p, "eb$seqno", t->seqno);
	set_property_number_0(t->f0->cx, p, "eb$gsn", t->gsn);
}

void disconnectTagObject(Tag *t)
{
	struct jsdata_wrap *w;
	jsobjtype p = t->jv;
	if (!p)
		return;
	w = jsdata_of(p);
	if (w->u.t == NULL)
		debugPrint(1, "tag already disconnected from pointer %p", p);
	else if (w->u.t != t)
		debugPrint(1,
			   "tag disconnecting from pointer %p which is connected to some other tag",
			   p);
	w->u.t = NULL;
	t->jv = NULL;
	t->jslink = false;
}

int js_main(void)
{
	effects = initString(&eff_l);
	context0 =
	    duk_create_heap(watch_malloc, watch_realloc, watch_free, 0, 0);
	if (!context0) {
		fprintf(stderr,
			"Cannot create javascript runtime environment\n");
		return 4;
	}
	duk_push_global_object(context0);
	duk_push_false(context0);
	duk_put_prop_string(context0, -2, "compiled");
	context0_obj = duk_get_heapptr(context0, -1);
	duk_pop(context0);
	return 0;
}				/* js_main */

// base64 encode
static duk_ret_t native_btoa(duk_context * cx)
{
	char *t;
	const char *s = duk_get_string(cx, 0);
	if (!s)
		s = emptyString;
	t = base64Encode(s, strlen(s), false);
	duk_pop(cx);
	duk_push_string(cx, t);
	nzFree(t);
	return 1;
}

// base64 decode
static duk_ret_t native_atob(duk_context * cx)
{
	char *t1, *t2;
	const char *s = duk_get_string(cx, 0);
	if (!s)
		s = emptyString;
	t1 = cloneString(s);
	duk_pop(cx);
	t2 = t1 + strlen(t1);
	base64Decode(t1, &t2);
// ignore errors for now.
	*t2 = 0;
	duk_push_string(cx, t1);
	nzFree(t1);
	return 1;
}

static duk_ret_t native_new_location(duk_context * cx)
{
	const char *s = duk_safe_to_string(cx, -1);
	if (s && *s) {
		char *t = cloneString(s);
/* url on one line, name of window on next line */
		char *u = strchr(t, '\n');
		*u++ = 0;
		debugPrint(4, "window %s|%s", t, u);
		javaOpensWindow(t, u);
		nzFree(t);
	}
	return 0;
}

static duk_ret_t native_mywin(duk_context * cx)
{
	duk_push_global_object(cx);
	return 1;
}

static duk_ret_t native_mydoc(duk_context * cx)
{
	duk_get_global_string(cx, "document");
	return 1;
}

static duk_ret_t native_hasfocus(duk_context * cx)
{
	duk_push_boolean(cx, foregroundWindow);
	return 1;
}

static duk_ret_t native_puts(duk_context * cx)
{
	const char *s = duk_safe_to_string(cx, -1);
	if (!s)
		s = emptyString;
	puts(s);
	return 0;
}

// write local file
static duk_ret_t native_wlf(duk_context * cx)
{
	const char *s = duk_safe_to_string(cx, 0);
	int len = strlen(s);
	const char *filename = duk_safe_to_string(cx, 1);
	int fh;
	bool safe = false;
	if (stringEqual(filename, "from") || stringEqual(filename, "jslocal"))
		safe = true;
	if (filename[0] == 'f') {
		int i;
		for (i = 1; isdigit(filename[i]); ++i) ;
		if (i > 1 && (stringEqual(filename + i, ".js") ||
			      stringEqual(filename + i, ".css")))
			safe = true;
	}
	if (!safe)
		return 0;
	fh = open(filename, O_CREAT | O_TRUNC | O_WRONLY | O_TEXT, MODE_rw);
	if (fh < 0) {
		fprintf(stderr, "cannot create file %s\n", filename);
		return 0;
	}
	if (write(fh, s, len) < len)
		fprintf(stderr, "cannot write file %s\n", filename);
	close(fh);
	if (stringEqual(filename, "jslocal"))
		writeShortCache();
	return 0;
}

static duk_ret_t native_media(duk_context * cx)
{
	const char *s = duk_safe_to_string(cx, 0);
	bool rc = false;
	if (s && *s) {
		char *t = cloneString(s);
		rc = matchMedia(t);
		nzFree(t);
	}
	duk_pop(cx);
	duk_push_boolean(cx, rc);
	return 1;
}

static duk_ret_t native_logputs(duk_context * cx)
{
	int minlev = duk_get_int(cx, 0);
	const char *s = duk_safe_to_string(cx, 1);
	duk_remove(cx, 0);
	if (debugLevel >= minlev && s && *s)
		debugPrint(3, "%s", s);
	jsInterruptCheck(cx);
	return 0;
}

static duk_ret_t native_prompt(duk_context * cx)
{
	const char *msg = 0;
	const char *answer = 0;
	int top = duk_get_top(cx);
	char inbuf[80];
	if (top > 0) {
		msg = duk_safe_to_string(cx, 0);
		if (top > 1)
			answer = duk_safe_to_string(cx, 1);
	}
	if (msg && *msg) {
		char c, *s;
		printf("%s", msg);
/* If it doesn't end in space or question mark, print a colon */
		c = msg[strlen(msg) - 1];
		if (!isspace(c)) {
			if (!ispunct(c))
				printf(":");
			printf(" ");
		}
		if (answer && *answer)
			printf("[%s] ", answer);
		fflush(stdout);
		if (!fgets(inbuf, sizeof(inbuf), stdin))
			exit(5);
		s = inbuf + strlen(inbuf);
		if (s > inbuf && s[-1] == '\n')
			*--s = 0;
		if (inbuf[0])
			answer = inbuf;
	}
	duk_pop_n(cx, top);
	duk_push_string(cx, answer);
	return 1;
}

static duk_ret_t native_confirm(duk_context * cx)
{
	const char *msg = duk_safe_to_string(cx, 0);
	bool answer = false, first = true;
	char c = 'n';
	char inbuf[80];
	if (msg && *msg) {
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
	}
	duk_pop(cx);
	if (c == 'y' || c == 'Y')
		answer = true;
	duk_push_boolean(cx, answer);
	return 1;
}

/* represent an object pointer in ascii */
static const char *pointer2string(const jsobjtype obj)
{
	static char pbuf[32];
	sprintf(pbuf, "%p", obj);
	return pbuf;
}				/* pointer2string */

// Sometimes control c can interrupt long running javascript, if the script
// calls our native methods.
static void jsInterruptCheck(duk_context * cx)
{
	if (!intFlag)
		return;
	duk_get_global_string(cx, "eb$stopexec");
// this next line should fail and stop the script!
// Assuming we aren't in a try{} block.
	duk_call(cx, 0);
// It didn't stop the script, oh well.
	duk_pop(cx);
}

static duk_ret_t getter_innerHTML(duk_context * cx)
{
	duk_push_this(cx);
	duk_get_prop_string(cx, -1, "inner$HTML");
	duk_remove(cx, -2);
	return 1;
}

static duk_ret_t setter_innerHTML(duk_context * cx)
{
	jsobjtype thisobj, c1, c2;
	char *run;
	int run_l;
	const char *h = duk_safe_to_string(cx, -1);
	if (!h)			// should never happen
		h = emptyString;
	debugPrint(5, "setter h 1");
	jsInterruptCheck(cx);
	duk_push_this(cx);
// remove the preexisting children.
	if (duk_get_prop_string(cx, -1, "childNodes") && duk_is_array(cx, -1)) {
		c1 = duk_get_heapptr(cx, -1);
	} else {
// no child nodes array, don't do anything.
// This should never happen.
		duk_pop_n(cx, 3);
		debugPrint(5, "setter h 3");
		return 0;
	}
// hold this away from garbage collection
	duk_put_prop_string(cx, -2, "old$cn");
// stack now holds html and this
// make new childNodes array
	duk_get_global_string(cx, "Array");
	duk_pnew(cx, 0);
	c2 = duk_get_heapptr(cx, -1);
	duk_put_prop_string(cx, -2, "childNodes");
// stack now holds html and this
	duk_insert(cx, -2);
	duk_put_prop_string(cx, -2, "inner$HTML");
// stack now holds this

	thisobj = duk_get_heapptr(cx, -1);

// Put some tags around the html, so tidy can parse it.
	run = initString(&run_l);
	stringAndString(&run, &run_l, "<!DOCTYPE public><body>\n");
	stringAndString(&run, &run_l, h);
	if (*h && h[strlen(h) - 1] != '\n')
		stringAndChar(&run, &run_l, '\n');
	stringAndString(&run, &run_l, "</body>");

// now turn the html into objects
	html_from_setter(thisobj, run);
	nzFree(run);
	debugPrint(5, "setter h 2");

	run_function_onearg_0(cx, context0_obj, "textarea$html$crossover",
				thisobj);

// mutation fix up from native code
	duk_push_heapptr(cx, context0_obj);
	duk_get_prop_string(cx, -1, "mutFixup");
	if (duk_is_function(cx, -1)) {
		duk_push_heapptr(cx, thisobj);
		duk_push_false(cx);
		duk_push_heapptr(cx, c2);
		duk_push_heapptr(cx, c1);
		duk_call(cx, 4);
	}
// stack is this mw$ retval
	duk_pop_2(cx);
	duk_del_prop_string(cx, -1, "old$cn");
	duk_pop(cx);

	return 0;
}

static duk_ret_t getter_value(duk_context * cx)
{
	duk_push_this(cx);
	duk_get_prop_string(cx, -1, "val$ue");
	duk_remove(cx, -2);
	return 1;
}

static duk_ret_t setter_value(duk_context * cx)
{
	jsobjtype thisobj;
	char *t;
	const char *h = duk_safe_to_string(cx, -1);
	if (!h)			// should never happen
		h = emptyString;
	debugPrint(5, "setter v 1");
	t = cloneString(h);
	duk_push_this(cx);
	duk_insert(cx, -2);
	duk_put_prop_string(cx, -2, "val$ue");
	thisobj = duk_get_heapptr(cx, -1);
	duk_pop(cx);
	prepareForField(t);
	debugPrint(4, "value %p=%s", thisobj, t);
	javaSetsTagVar(thisobj, t);
	nzFree(t);
	debugPrint(5, "setter v 2");
	return 0;
}

static void forceFrameExpand(duk_context * cx, jsobjtype thisobj)
{
	Frame *save_cf = cf;
	const char *save_src = jsSourceFile;
	int save_lineno = jsLineno;
	bool save_plug = pluginsOn;
	duk_push_true(cx);
	duk_put_prop_string(cx, -2, "eb$auto");
	pluginsOn = false;
	whichproc = 'e';
	frameExpandLine(0, thisobj);
	whichproc = 'j';
	cf = save_cf;
	jsSourceFile = save_src;
	jsLineno = save_lineno;
	pluginsOn = save_plug;
}

// contentDocument getter setter; this is a bit complicated.
static duk_ret_t getter_cd(duk_context * cx)
{
	bool found;
	jsobjtype thisobj;
	jsInterruptCheck(cx);
	duk_push_this(cx);
	thisobj = duk_get_heapptr(cx, -1);
	found = duk_get_prop_string(cx, -1, "eb$auto");
	duk_pop(cx);
	if (!found)
		forceFrameExpand(cx, thisobj);
	duk_get_prop_string(cx, -1, "content$Document");
	duk_remove(cx, -2);
	return 1;
}

// You can't really change contentDocument; this is a stub.
static duk_ret_t setter_cd(duk_context * cx)
{
	return 0;
}

static duk_ret_t getter_cw(duk_context * cx)
{
	bool found;
	jsobjtype thisobj;
	jsInterruptCheck(cx);
	duk_push_this(cx);
	thisobj = duk_get_heapptr(cx, -1);
	found = duk_get_prop_string(cx, -1, "eb$auto");
	duk_pop(cx);
	if (!found)
		forceFrameExpand(cx, thisobj);
	duk_get_prop_string(cx, -1, "content$Window");
	duk_remove(cx, -2);
	return 1;
}

// You can't really change contentWindow; this is a stub.
static duk_ret_t setter_cw(duk_context * cx)
{
	return 0;
}

static duk_ret_t native_unframe(duk_context * cx)
{
	if (duk_is_object(cx, 0))
		unframe(duk_get_heapptr(cx, 0), duk_get_heapptr(cx, 1));
	duk_pop_2(cx);
	return 0;
}

static duk_ret_t native_unframe2(duk_context * cx)
{
	if (duk_is_object(cx, 0))
		unframe2(duk_get_heapptr(cx, 0));
	duk_pop(cx);
	return 0;
}

static void linkageNow(duk_context * cx, char linkmode, jsobjtype o)
{
	jsInterruptCheck(cx);
	debugPrint(4, "linkset %s", effects + 2);
	javaSetsLinkage(false, linkmode, o, strchr(effects, ',') + 1);
	nzFree(effects);
	effects = initString(&eff_l);
}

static duk_ret_t native_log_element(duk_context * cx)
{
	jsobjtype newobj = duk_get_heapptr(cx, -2);
	const char *tag = duk_get_string(cx, -1);
	char e[60];
	if (!newobj || !tag)
		return 0;
	debugPrint(5, "log el 1");
	jsInterruptCheck(cx);
// pass the newly created node over to edbrowse
	sprintf(e, "l{c|%s,%s 0x0, 0x0, ", pointer2string(newobj), tag);
	effectString(e);
	linkageNow(cx, 'c', newobj);
	duk_pop(cx);
// create the innerHTML member with its setter, this has to be done in C.
	duk_push_string(cx, "innerHTML");
	duk_push_c_function(cx, getter_innerHTML, 0);
	duk_push_c_function(cx, setter_innerHTML, 1);
	duk_def_prop(cx, -4,
		     (DUK_DEFPROP_HAVE_SETTER | DUK_DEFPROP_HAVE_GETTER |
		      DUK_DEFPROP_SET_ENUMERABLE));
	duk_push_string(cx, emptyString);
	duk_put_prop_string(cx, -2, "inner$HTML");
	duk_pop(cx);
	debugPrint(5, "log el 2");
	return 0;
}

/* like the function in ebjs.c, but a different name */
static const char *fakePropName(void)
{
	static char fakebuf[24];
	static int idx = 0;
	++idx;
	sprintf(fakebuf, "cg$$%d", idx);
	return fakebuf;
}

static void set_timeout(duk_context * cx, bool isInterval)
{
	jsobjtype to;		// timer object
	bool cc_error = false;
	int top = duk_get_top(cx);
	int n = 1000;		/* default number of milliseconds */
	char fname[48];		/* function name */
	const char *fstr;	/* function string */
	const char *s, *fpn;

	if (top == 0)
		return;		// no args

	debugPrint(5, "timer 1");
// if second parameter is missing, leave milliseconds at 1000.
	if (top > 1) {
		n = duk_get_int(cx, 1);
		duk_pop_n(cx, top - 1);
	}
// now the function is the only thing on the stack.

	if (duk_is_function(cx, 0)) {
		duk_push_string(cx, "?");
		duk_put_prop_string(cx, -2, "body");
// We use to extract the function name in moz js, don't know how to do it here.
		strcpy(fname, "javascript()");
	} else if (duk_is_string(cx, 0)) {
// need to make a copy of the source code.
		char *body = cloneString(duk_get_string(cx, 0));
// pull the function name out of the string, if that makes sense.
		fstr = body;
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
// compile the string under the filename timer
		duk_push_string(cx, "timer");
		if (duk_pcompile(cx, 0)) {
			processError(cx);
			cc_error = true;
			duk_push_c_function(cx, native_error_stub_0, 0);
		}
// Now looks like a function object, just like the previous case.
		duk_push_string(cx, body);
		duk_put_prop_string(cx, -2, "body");
		nzFree(body);
	} else {
// oops, not a function or a string.
		duk_pop(cx);
		return;
	}

	duk_push_global_object(cx);
	fpn = fakePropName();
	if (cc_error)
		debugPrint(3, "compile error on timer %s", fpn);
	duk_push_string(cx, fpn);
// Create a timer object.
	duk_get_global_string(cx, "Timer");
	if (duk_pnew(cx, 0)) {
		processError(cx);
		duk_pop_n(cx, 3);
		goto done;
	}
// stack now has function global fakePropertyName timer-object.
// classs is milliseconds, for debugging
	duk_push_int(cx, n);
	duk_put_prop_string(cx, -2, "class");
	to = duk_get_heapptr(cx, -1);
// protect this timer from the garbage collector.
	duk_def_prop(cx, 1,
		     (DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_SET_ENUMERABLE |
		      DUK_DEFPROP_CLEAR_WRITABLE |
		      DUK_DEFPROP_SET_CONFIGURABLE));
	duk_pop(cx);		// don't need global any more

// function is contained in an ontimer handler
	duk_push_heapptr(cx, to);
	duk_insert(cx, 0);	// switch places
// now stack is timer_object function
	duk_push_string(cx, "ontimer");
	duk_insert(cx, 1);
	duk_def_prop(cx, 0,
		     (DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_CLEAR_ENUMERABLE |
		      DUK_DEFPROP_SET_WRITABLE |
		      DUK_DEFPROP_CLEAR_CONFIGURABLE));
	duk_push_string(cx, fpn);
	duk_put_prop_string(cx, -2, "backlink");
// leaves just the timer object on the stack, which is what we want.

	javaSetsTimeout(n, fname, to, isInterval);

done:
	debugPrint(5, "timer 2");
}

static duk_ret_t native_setTimeout(duk_context * cx)
{
	set_timeout(cx, false);
	return 1;
}

static duk_ret_t native_setInterval(duk_context * cx)
{
	set_timeout(cx, true);
	return 1;
}

static duk_ret_t native_clearTimeout(duk_context * cx)
{
	jsobjtype obj = duk_get_heapptr(cx, 0);
	if (!obj)
		return 0;
	javaSetsTimeout(0, "-", obj, false);
	return 0;
}

static duk_ret_t native_win_close(duk_context * cx)
{
	i_puts(MSG_PageDone);
// I should probably freeJavaContext and close down javascript,
// but not sure I can do that while the js function is still running.
	return 0;
}

// find the frame, in the current window, that goes with this.
// Used by document.write to put the html in the right frame.
static Frame *thisFrame(duk_context * cx)
{
	jsobjtype thisobj;
	Frame *f;
	duk_push_this(cx);
	thisobj = duk_get_heapptr(cx, -1);
	duk_pop(cx);
	for (f = &(cw->f0); f; f = f->next) {
		if (f->docobj == thisobj)
			break;
	}
	return f;
}

static void dwrite(duk_context * cx, bool newline)
{
	int top = duk_get_top(cx);
	const char *s;
	Frame *f, *save_cf = cf;
	if (top) {
		duk_push_string(cx, emptyString);
		duk_insert(cx, 0);
		duk_join(cx, top);
	} else {
		duk_push_string(cx, emptyString);
	}
	s = duk_get_string(cx, 0);
	if (!s || !*s)
		return;
	debugPrint(4, "dwrite:%s", s);
	f = thisFrame(cx);
	if (!f)
		debugPrint(3,
			   "no frame found for document.write, using the default");
	else {
#if 0
		if (f != cf)
			debugPrint(3, "document.write on a different frame");
#endif
		cf = f;
	}
	dwStart();
	stringAndString(&cf->dw, &cf->dw_l, s);
	if (newline)
		stringAndChar(&cf->dw, &cf->dw_l, '\n');
	cf = save_cf;
}

static duk_ret_t native_doc_write(duk_context * cx)
{
	dwrite(cx, false);
	return 0;
}

static duk_ret_t native_doc_writeln(duk_context * cx)
{
	dwrite(cx, true);
	return 0;
}

// We need to call and remember up to 3 node names, and then embed
// them in the side effects string, after all duktape calls have been made.
static const char *embedNodeName(duk_context * cx, jsobjtype obj)
{
	static char buf1[MAXTAGNAME], buf2[MAXTAGNAME], buf3[MAXTAGNAME];
	char *b;
	static int cycle = 0;
	const char *nodeName = 0;
	int length;

	if (++cycle == 4)
		cycle = 1;
	if (cycle == 1)
		b = buf1;
	if (cycle == 2)
		b = buf2;
	if (cycle == 3)
		b = buf3;
	*b = 0;

	duk_push_heapptr(cx, obj);
	if (duk_get_prop_string(cx, -1, "nodeName"))
		nodeName = duk_get_string(cx, -1);
	if (nodeName) {
		length = strlen(nodeName);
		if (length >= MAXTAGNAME)
			length = MAXTAGNAME - 1;
		strncpy(b, nodeName, length);
		b[length] = 0;
	}
	duk_pop_2(cx);
	caseShift(b, 'l');
	return b;
}				/* embedNodeName */

static void append0(duk_context * cx, bool side)
{
	unsigned i, length;
	jsobjtype child, thisobj;
	char *e;
	const char *thisname, *childname;

/* we need one argument that is an object */
	if (duk_get_top(cx) != 1 || !duk_is_object(cx, 0))
		return;

	debugPrint(5, "append 1");
	child = duk_get_heapptr(cx, 0);
	duk_push_this(cx);
	thisobj = duk_get_heapptr(cx, -1);
	if (!duk_get_prop_string(cx, -1, "childNodes") || !duk_is_array(cx, -1)) {
		duk_pop_2(cx);
		goto done;
	}
	length = duk_get_length(cx, -1);
// see if it's already there.
	for (i = 0; i < length; ++i) {
		duk_get_prop_index(cx, -1, i);
		if (child == duk_get_heapptr(cx, -1)) {
// child was already there, just return.
			duk_pop_n(cx, 3);
			goto done;
		}
		duk_pop(cx);
	}

// add child to the end
	duk_push_heapptr(cx, child);
	duk_put_prop_index(cx, -2, length);
	duk_pop(cx);
	duk_push_string(cx, "parentNode");
	duk_insert(cx, 1);
	duk_def_prop(cx, 0,
		     (DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_SET_ENUMERABLE |
		      DUK_DEFPROP_SET_WRITABLE | DUK_DEFPROP_SET_CONFIGURABLE));

	if (!side)
		goto done;

/* pass this linkage information back to edbrowse, to update its dom tree */
	thisname = embedNodeName(cx, thisobj);
	childname = embedNodeName(cx, child);
	asprintf(&e, "l{a|%s,%s ", pointer2string(thisobj), thisname);
	effectString(e);
	free(e);
	effectString(pointer2string(child));
	effectChar(',');
	effectString(childname);
	effectString(" 0x0, ");
	linkageNow(cx, 'a', thisobj);

done:
	debugPrint(5, "append 2");
}

static duk_ret_t native_apch1(duk_context * cx)
{
	append0(cx, false);
	return 1;
}

static duk_ret_t native_apch2(duk_context * cx)
{
	append0(cx, true);
	return 1;
}

static duk_ret_t native_insbf(duk_context * cx)
{
	unsigned i, length;
	int mark;
	jsobjtype child, item, thisobj, h;
	char *e;
	const char *thisname, *childname, *itemname;

/* we need two objects */
	if (duk_get_top(cx) != 2 ||
	    !duk_is_object(cx, 0) || !duk_is_object(cx, 1))
		return 0;

	debugPrint(5, "before 1");
	child = duk_get_heapptr(cx, 0);
	item = duk_get_heapptr(cx, 1);
	duk_push_this(cx);
	thisobj = duk_get_heapptr(cx, -1);
	duk_get_prop_string(cx, -1, "childNodes");
	if (!duk_is_array(cx, -1)) {
		duk_pop_n(cx, 3);
		goto done;
	}
	length = duk_get_length(cx, -1);
	mark = -1;
	for (i = 0; i < length; ++i) {
		duk_get_prop_index(cx, -1, i);
		h = duk_get_heapptr(cx, -1);
		if (child == h) {
			duk_pop_n(cx, 4);
			goto done;
		}
		if (h == item)
			mark = i;
		duk_pop(cx);
	}

	if (mark < 0) {
		duk_pop_n(cx, 3);
		goto done;
	}

/* push the other elements down */
	for (i = length; i > (unsigned)mark; --i) {
		duk_get_prop_index(cx, -1, i - 1);
		duk_put_prop_index(cx, -2, i);
	}
/* and place the child */
	duk_push_heapptr(cx, child);
	duk_put_prop_index(cx, -2, mark);
	duk_pop(cx);
	duk_push_string(cx, "parentNode");
	duk_insert(cx, -2);
	duk_remove(cx, 1);
	duk_def_prop(cx, 0,
		     (DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_SET_ENUMERABLE |
		      DUK_DEFPROP_SET_WRITABLE | DUK_DEFPROP_SET_CONFIGURABLE));

/* pass this linkage information back to edbrowse, to update its dom tree */
	thisname = embedNodeName(cx, thisobj);
	childname = embedNodeName(cx, child);
	itemname = embedNodeName(cx, item);
	asprintf(&e, "l{b|%s,%s ", pointer2string(thisobj), thisname);
	effectString(e);
	free(e);
	effectString(pointer2string(child));
	effectChar(',');
	effectString(childname);
	effectChar(' ');
	effectString(pointer2string(item));
	effectChar(',');
	effectString(itemname);
	effectChar(' ');
	linkageNow(cx, 'b', thisobj);

done:
	debugPrint(5, "before 2");
	return 1;
}

static duk_ret_t native_removeChild(duk_context * cx)
{
	unsigned i, length;
	int mark;
	jsobjtype child, thisobj, h;
	char *e;
	const char *thisname, *childname;

	debugPrint(5, "remove 1");
// top of stack must be the object to remove.
	if (!duk_is_object(cx, -1))
		goto fail;
	child = duk_get_heapptr(cx, -1);
	duk_push_this(cx);
	thisobj = duk_get_heapptr(cx, -1);
	duk_get_prop_string(cx, -1, "childNodes");
	if (!duk_is_array(cx, -1)) {
		duk_pop_2(cx);
		goto fail;
	}
	length = duk_get_length(cx, -1);
	mark = -1;
	for (i = 0; i < length; ++i) {
		duk_get_prop_index(cx, -1, i);
		h = duk_get_heapptr(cx, -1);
		if (h == child)
			mark = i;
		duk_pop(cx);
		if (mark >= 0)
			break;
	}

	if (mark < 0) {
		duk_pop_2(cx);
		goto fail;
	}

/* push the other elements down */
	for (i = mark + 1; i < length; ++i) {
		duk_get_prop_index(cx, -1, i);
		duk_put_prop_index(cx, -2, i - 1);
	}
	duk_set_length(cx, -1, length - 1);
	duk_pop_2(cx);
// missing parentnode must always be null
	duk_push_null(cx);
	duk_put_prop_string(cx, -2, "parentNode");

/* pass this linkage information back to edbrowse, to update its dom tree */
	thisname = embedNodeName(cx, thisobj);
	childname = embedNodeName(cx, child);
	asprintf(&e, "l{r|%s,%s ", pointer2string(thisobj), thisname);
	effectString(e);
	free(e);
	effectString(pointer2string(child));
	effectChar(',');
	effectString(childname);
	effectString(" 0x0, ");
	linkageNow(cx, 'r', thisobj);

	debugPrint(5, "remove 2");
// mutation fix up from native code
	duk_push_heapptr(cx, context0_obj);
	duk_get_prop_string(cx, -1, "mutFixup");
	if (duk_is_function(cx, -1)) {
		duk_push_heapptr(cx, thisobj);
		duk_push_false(cx);
// exception here, push an integer where the node was.
		duk_push_int(cx, mark);
		duk_push_heapptr(cx, child);
		duk_call(cx, 4);
	}
	duk_pop_2(cx);
	return 1;

fail:
	debugPrint(5, "remove 2");
	duk_pop(cx);
	duk_push_null(cx);
	return 1;
}

static duk_ret_t native_fetchHTTP(duk_context * cx)
{
	jsobjtype thisobj;
	struct i_get g;
	const char *incoming_url = duk_safe_to_string(cx, 0);
	const char *incoming_method = duk_get_string(cx, 1);
	const char *incoming_headers = duk_get_string(cx, 2);
	const char *incoming_payload = duk_get_string(cx, 3);
	char *outgoing_xhrheaders = NULL;
	char *outgoing_xhrbody = NULL;
	char *a = NULL, methchar = '?';
	bool rc, async;

	debugPrint(5, "xhr 1");
	duk_push_this(cx);
	thisobj = duk_get_heapptr(cx, -1);
	duk_get_prop_string(cx, -1, "async");
	async = duk_get_boolean(cx, -1);
	duk_pop_2(cx);
	if (!down_jsbg)
		async = false;

// asynchronous xhr before browse and after browse go down different paths.
// So far I can't get the before browse path to work,
// at least on nasa.gov, which has lots of xhrs in its onload code.
// It pushes things over to timers, which work, but the page is rendered
// shortly after browse time instead of at browse time, which is annoying.
	if (!cw->browseMode)
		async = false;

	if (!incoming_url)
		incoming_url = emptyString;
	if (incoming_payload && *incoming_payload) {
		if (incoming_method && stringEqualCI(incoming_method, "post"))
			methchar = '\1';
		if (asprintf(&a, "%s%c%s",
			     incoming_url, methchar, incoming_payload) < 0)
			i_printfExit(MSG_MemAllocError, 50);
		incoming_url = a;
	}

	debugPrint(3, "xhr send %s", incoming_url);

// async and sync are completely different
	if (async) {
		const char *fpn = fakePropName();
// I'm going to put the tag in cf, the current frame, and hope that's right,
// hope that xhr runs in a script that runs in the current frame.
		Tag *t =     newTag(cf, cw->browseMode ? "object" : "script");
		t->deleted = true;	// do not render this tag
		t->step = 3;
		t->async = true;
		t->inxhr = true;
		t->f0 = cf;
		connectTagObject(t, thisobj);
		duk_pop_n(cx, 4);
// This routine will return, and javascript might stop altogether; do we need
// to protect this object from garbage collection?
		duk_push_global_object(cx);
		duk_push_this(cx);
		duk_push_string(cx, fpn);
		duk_def_prop(cx, 0,
			     (DUK_DEFPROP_HAVE_VALUE |
			      DUK_DEFPROP_SET_ENUMERABLE |
			      DUK_DEFPROP_CLEAR_WRITABLE |
			      DUK_DEFPROP_SET_CONFIGURABLE));
		duk_pop(cx);	// don't need global any more
		duk_push_this(cx);
		duk_push_string(cx, fpn);
		duk_put_prop_string(cx, 0, "backlink");
		duk_pop(cx);
// That takes care of garbage collection.
// Now everything has to be allocated.
		t->href = (a ? a : cloneString(incoming_url));
// overloading the innerHTML field
		t->innerHTML = cloneString(incoming_headers);
		if (cw->browseMode)
			scriptSetsTimeout(t);
		pthread_create(&t->loadthread, NULL, httpConnectBack3,
			       (void *)t);
		duk_push_string(cx, "async");
		return 1;
	}

	memset(&g, 0, sizeof(g));
	g.thisfile = cf->fileName;
	g.uriEncoded = true;
	g.url = incoming_url;
	g.custom_h = incoming_headers;
	g.headers_p = &outgoing_xhrheaders;
	rc = httpConnect(&g);
	outgoing_xhrbody = g.buffer;
	nzFree(a);
	if (intFlag) {
		duk_get_global_string(cx, "eb$stopexec");
// this next line should fail and stop the script!
		duk_call(cx, 0);
// It didn't stop the script, oh well.
		duk_pop(cx);
	}
	if (outgoing_xhrheaders == NULL)
		outgoing_xhrheaders = emptyString;
	if (outgoing_xhrbody == NULL)
		outgoing_xhrbody = emptyString;
	duk_pop_n(cx, 4);
	duk_push_string(cx, "");
	duk_push_string(cx, "\r\n\r\n");
	duk_push_int(cx, rc);
	duk_push_int(cx, g.code);
	duk_push_string(cx, outgoing_xhrheaders);
	duk_join(cx, 3);
	duk_push_string(cx, outgoing_xhrbody);
	duk_join(cx, 2);
	nzFree(outgoing_xhrheaders);
	nzFree(outgoing_xhrbody);

	debugPrint(5, "xhr 2");
	return 1;
}

static duk_ret_t native_resolveURL(duk_context * cx)
{
	const char *base = duk_get_string(cx, -2);
	const char *rel = duk_get_string(cx, -1);
	char *outgoing_url;
	if (!base)
		base = emptyString;
	if (!rel)
		rel = emptyString;
	outgoing_url = resolveURL(base, rel);
	if (outgoing_url == NULL)
		outgoing_url = emptyString;
	duk_pop_2(cx);
	duk_push_string(cx, outgoing_url);
	nzFree(outgoing_url);
	return 1;
}

static duk_ret_t native_formSubmit(duk_context * cx)
{
	jsobjtype thisobj;
	duk_push_this(cx);
	thisobj = duk_get_heapptr(cx, -1);
	duk_pop(cx);
	debugPrint(4, "submit %p", thisobj);
	javaSubmitsForm(thisobj, false);
	return 0;
}

static duk_ret_t native_formReset(duk_context * cx)
{
	jsobjtype thisobj;
	duk_push_this(cx);
	thisobj = duk_get_heapptr(cx, -1);
	duk_pop(cx);
	debugPrint(4, "reset %p", thisobj);
	javaSubmitsForm(thisobj, true);
	return 0;
}

/*********************************************************************
Maintain a copy of the cookie string that is relevant for this web page.
Include a leading semicolon, looking like
; foo=73838; bar=j_k_qqr; bas=21998999
The setter folds a new cookie into this string,
and also passes the cookie back to edbrowse to put in the cookie jar.
*********************************************************************/

static char *cookieCopy;
static int cook_l;

static void startCookie(void)
{
	const char *url = cf->fileName;
	bool secure = false;
	const char *proto;
	char *s;
	nzFree(cookieCopy);
	cookieCopy = initString(&cook_l);
	stringAndString(&cookieCopy, &cook_l, "; ");
	if (url) {
		proto = getProtURL(url);
		if (proto && stringEqualCI(proto, "https"))
			secure = true;
		sendCookies(&cookieCopy, &cook_l, url, secure);
		if (memEqualCI(cookieCopy, "; cookie: ", 10)) {	// should often happen
			strmove(cookieCopy + 2, cookieCopy + 10);
			cook_l -= 8;
		}
		if ((s = strstr(cookieCopy, "\r\n"))) {
			*s = 0;
			cook_l -= 2;
		}
	}
}

static duk_ret_t native_getcook(duk_context * cx)
{
	startCookie();
	duk_push_string(cx, cookieCopy + 2);
	return 1;
}

static duk_ret_t native_setcook(duk_context * cx)
{
	const char *newcook = duk_get_string(cx, 0);
	debugPrint(5, "cook 1");
	if (newcook) {
		const char *s = strchr(newcook, '=');
		if(s && s > newcook) {
			duk_get_global_string(cx, "eb$url");
			receiveCookie(duk_get_string(cx, -1), newcook);
			duk_pop(cx);
		}
	}
	debugPrint(5, "cook 2");
	return 0;
}

static duk_ret_t native_css_start(duk_context * cx)
{
	cssDocLoad(duk_get_heapptr(cx, 0), cloneString(duk_get_string(cx, 1)),
		   duk_get_boolean(cx, 2));
	return 0;
}

// querySelectorAll
static duk_ret_t native_qsa(duk_context * cx)
{
	jsobjtype root = 0, ao;
	const char *selstring = duk_get_string(cx, 0);
	int top = duk_get_top(cx);
	if (top > 2) {
		duk_pop_n(cx, top - 2);
		top = 2;
	}
	if (top == 2) {
		if (duk_is_object(cx, 1))
			root = duk_get_heapptr(cx, 1);
	}
	if (!root) {
		duk_push_this(cx);
		root = duk_get_heapptr(cx, -1);
		duk_pop(cx);
	}
	jsInterruptCheck(cx);
	ao = querySelectorAll(selstring, root);
	duk_pop_n(cx, top);
	duk_push_heapptr(cx, ao);
	return 1;
}

// querySelector
static duk_ret_t native_qs(duk_context * cx)
{
	jsobjtype root = 0, ao;
	const char *selstring = duk_get_string(cx, 0);
	int top = duk_get_top(cx);
	if (top > 2) {
		duk_pop_n(cx, top - 2);
		top = 2;
	}
	if (top == 2) {
		if (duk_is_object(cx, 1))
			root = duk_get_heapptr(cx, 1);
	}
	if (!root) {
		duk_push_this(cx);
		root = duk_get_heapptr(cx, -1);
		duk_pop(cx);
	}
	jsInterruptCheck(cx);
	ao = querySelector(selstring, root);
	duk_pop_n(cx, top);
	if (ao)
		duk_push_heapptr(cx, ao);
	else
		duk_push_undefined(cx);
	return 1;
}

// querySelector0
static duk_ret_t native_qs0(duk_context * cx)
{
	jsobjtype root;
	bool rc;
	const char *selstring = duk_get_string(cx, 0);
	duk_push_this(cx);
	root = duk_get_heapptr(cx, -1);
	duk_pop(cx);
	jsInterruptCheck(cx);
	rc = querySelector0(selstring, root);
	duk_pop(cx);
	duk_push_boolean(cx, rc);
	return 1;
}

static duk_ret_t native_cssApply(duk_context * cx)
{
	jsInterruptCheck(cx);
	if (duk_is_object(cx, 1) && duk_is_object(cx, 2))
		cssApply(duk_get_heapptr(cx, 0), duk_get_heapptr(cx, 1),
			 duk_get_heapptr(cx, 2));
	duk_pop_n(cx, 3);
	return 0;
}

static duk_ret_t native_cssText(duk_context * cx)
{
	jsobjtype thisobj;
	const char *rulestring = duk_get_string(cx, 0);
	duk_push_this(cx);
	thisobj = duk_get_heapptr(cx, -1);
	cssText(thisobj, rulestring);
	duk_pop_2(cx);
	return 0;
}

void createJavaContext_0(Frame *f)
{
	static int seqno;
	duk_context * cx;

	duk_push_thread_new_globalenv(context0);
	cx = f->cx = duk_get_context(context0, -1);
	if (!cx)
		return;
	debugPrint(3, "create js context %d", duk_get_top(context0) - 1);
// the global object, which will become window,
// and the document object.
	duk_push_global_object(cx);
	f->winobj = duk_get_heapptr(cx, 0);
	duk_push_string(cx, "document");
	duk_push_object(cx);
	f->docobj = duk_get_heapptr(cx, 2);
	duk_def_prop(cx, 0,
		     (DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_SET_ENUMERABLE |
		      DUK_DEFPROP_CLEAR_WRITABLE |
		      DUK_DEFPROP_CLEAR_CONFIGURABLE));
	duk_pop(cx);

// bind native functions here
	duk_push_c_function(cx, native_new_location, 1);
	duk_put_global_string(cx, "eb$newLocation");
	duk_push_c_function(cx, native_mywin, 0);
	duk_put_global_string(cx, "my$win");
	duk_push_c_function(cx, native_mydoc, 0);
	duk_put_global_string(cx, "my$doc");
	duk_push_c_function(cx, native_puts, 1);
	duk_put_global_string(cx, "eb$puts");
	duk_push_c_function(cx, native_wlf, 2);
	duk_put_global_string(cx, "eb$wlf");
	duk_push_c_function(cx, native_media, 1);
	duk_put_global_string(cx, "eb$media");
	duk_push_c_function(cx, native_btoa, 1);
	duk_put_global_string(cx, "btoa");
	duk_push_c_function(cx, native_atob, 1);
	duk_put_global_string(cx, "atob");
	duk_push_c_function(cx, native_unframe, 2);
	duk_put_global_string(cx, "eb$unframe");
	duk_push_c_function(cx, native_unframe2, 1);
	duk_put_global_string(cx, "eb$unframe2");
	duk_push_c_function(cx, native_logputs, 2);
	duk_put_global_string(cx, "eb$logputs");
	duk_push_c_function(cx, native_prompt, DUK_VARARGS);
	duk_put_global_string(cx, "prompt");
	duk_push_c_function(cx, native_confirm, 1);
	duk_put_global_string(cx, "confirm");
	duk_push_c_function(cx, native_log_element, 2);
	duk_put_global_string(cx, "eb$logElement");
	duk_push_c_function(cx, native_setTimeout, DUK_VARARGS);
	duk_put_global_string(cx, "setTimeout");
	duk_push_c_function(cx, native_setInterval, DUK_VARARGS);
	duk_put_global_string(cx, "setInterval");
	duk_push_c_function(cx, native_clearTimeout, 1);
	duk_put_global_string(cx, "clearTimeout");
	duk_push_c_function(cx, native_clearTimeout, 1);
	duk_put_global_string(cx, "clearInterval");
	duk_push_c_function(cx, native_win_close, 0);
	duk_put_global_string(cx, "close");
	duk_push_c_function(cx, native_fetchHTTP, 4);
	duk_put_global_string(cx, "eb$fetchHTTP");
	duk_push_c_function(cx, native_resolveURL, 2);
	duk_put_global_string(cx, "eb$resolveURL");
	duk_push_c_function(cx, native_formSubmit, 0);
	duk_put_global_string(cx, "eb$formSubmit");
	duk_push_c_function(cx, native_formReset, 0);
	duk_put_global_string(cx, "eb$formReset");
	duk_push_c_function(cx, native_getcook, 0);
	duk_put_global_string(cx, "eb$getcook");
	duk_push_c_function(cx, native_setcook, 1);
	duk_put_global_string(cx, "eb$setcook");
	duk_push_c_function(cx, getter_cd, 0);
	duk_put_global_string(cx, "eb$getter_cd");
	duk_push_c_function(cx, getter_cw, 0);
	duk_put_global_string(cx, "eb$getter_cw");
	duk_push_c_function(cx, native_css_start, 3);
	duk_put_global_string(cx, "eb$cssDocLoad");
	duk_push_c_function(cx, native_qsa, DUK_VARARGS);
	duk_put_global_string(cx, "querySelectorAll");
	duk_push_c_function(cx, native_qs, DUK_VARARGS);
	duk_put_global_string(cx, "querySelector");
	duk_push_c_function(cx, native_qs0, 1);
	duk_put_global_string(cx, "querySelector0");
	duk_push_c_function(cx, native_cssApply, 3);
	duk_put_global_string(cx, "eb$cssApply");
	duk_push_c_function(cx, native_cssText, 1);
	duk_put_global_string(cx, "eb$cssText");

	duk_push_heapptr(cx, f->docobj);	// native document methods

	duk_push_c_function(cx, native_hasfocus, 0);
	duk_put_prop_string(cx, -2, "hasFocus");
	duk_push_c_function(cx, native_doc_write, DUK_VARARGS);
	duk_put_prop_string(cx, -2, "write");
	duk_push_c_function(cx, native_doc_writeln, DUK_VARARGS);
	duk_put_prop_string(cx, -2, "writeln");
	duk_push_c_function(cx, native_apch1, 1);
	duk_put_prop_string(cx, -2, "eb$apch1");
	duk_push_c_function(cx, native_apch2, 1);
	duk_put_prop_string(cx, -2, "eb$apch2");
	duk_push_c_function(cx, native_insbf, 2);
	duk_put_prop_string(cx, -2, "eb$insbf");
	duk_push_c_function(cx, native_removeChild, 1);
	duk_put_prop_string(cx, -2, "removeChild");

// document.ctx$ is the context number
	duk_push_number(cx, ++seqno);
	duk_put_prop_string(cx, -2, "ctx$");
// document.eb$seqno = 0
	duk_push_number(cx, 0);
	duk_put_prop_string(cx, -2, "eb$seqno");

	duk_pop(cx); // document

// Link to the master context, i.e. the master window.
// This is denoted mw$ throughout.
// For security reasons, it is only used for third party deminimization
// and other debugging tools.
// It is a huge security risk to share dom classes via this mechanism,
// even though it would be more efficient.
	duk_push_global_object(cx);
	duk_push_string(cx, "mw$");
	duk_push_heapptr(cx, context0_obj);
	duk_def_prop(cx, -3,
		     (DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_SET_ENUMERABLE |
		      DUK_DEFPROP_CLEAR_WRITABLE |
		      DUK_DEFPROP_CLEAR_CONFIGURABLE));
	duk_pop(cx);

// Sequence is to set cf->fileName, then createContext(), so for a short time,
// we can rely on that variable.
// Let's make it more permanent, per context.
// Has to be nonwritable for security reasons.
	duk_push_global_object(cx);
	duk_push_string(cx, "eb$url");
	duk_push_string(cx, cf->fileName);
	duk_def_prop(cx, -3,
		     (DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_SET_ENUMERABLE |
		      DUK_DEFPROP_CLEAR_WRITABLE |
		      DUK_DEFPROP_CLEAR_CONFIGURABLE));
	duk_pop(cx);

// setupJavaDom() in ebjs.c does the rest.
}				/* createJavaContext_0 */

void freeJavaContext_0(jsobjtype cx)
{
	int i, top = duk_get_top(context0);
	for (i = 0; i < top; ++i) {
		if (cx == duk_get_context(context0, i)) {
			duk_remove(context0, i);
			debugPrint(3, "remove js context %d", i);
			break;
		}
	}
}				/* freeJavaContext_0 */

// determine the type of the element on the top of the stack.
static enum ej_proptype top_proptype(duk_context * cx)
{
	double d;
	int n;
	switch (duk_get_type(cx, -1)) {
	case DUK_TYPE_NUMBER:
		d = duk_get_number(cx, -1);
		n = d;
		return (n == d ? EJ_PROP_INT : EJ_PROP_FLOAT);
	case DUK_TYPE_STRING:
		return EJ_PROP_STRING;
	case DUK_TYPE_BOOLEAN:
		return EJ_PROP_BOOL;
	case DUK_TYPE_OBJECT:
		if (duk_is_function(cx, -1))
			return EJ_PROP_FUNCTION;
		if (duk_is_array(cx, -1))
			return EJ_PROP_ARRAY;
		return EJ_PROP_OBJECT;
	case DUK_TYPE_NULL:
		return EJ_PROP_NULL;
	}
	return EJ_PROP_NONE;	/* don't know */
}				/* top_proptype */

/*********************************************************************
http://theconversation.com/how-to-build-a-moon-base-120259
creates a script object with getter on src, which throws an error if src
is empty or syntactically invalid.
I check for src in prepareScript(), to see if I need to load the page.
It's just a get call, but the getter throws an error,
and the get call is unprotected, and edbrowse aborts. Ouch!
Here is a simple get property call that is  called through
duk_safe_call() and thus protected.
I hope it doesn't introduce too much overhead, because it is almost never
needed, but neither do I want edbrowse to abort!
*********************************************************************/

static duk_ret_t protected_get(duk_context * cx, void *udata)
{
	const char *name = udata;
	duk_get_prop_string(cx, -1, name);
	return 1;
}

enum ej_proptype typeof_property_0(jsobjtype cx0, jsobjtype parent, const char *name)
{
	duk_context * cx = cx0;
	enum ej_proptype l;
	int rc;
	duk_push_heapptr(cx, parent);
	rc = duk_safe_call(cx, protected_get, (void *)name, 0, 1);
	l = rc ? 0 : top_proptype(cx);
	duk_pop_2(cx);
	return l;
}

bool has_property_0(jsobjtype cx0, jsobjtype parent, const char *name)
{
	duk_context * cx = cx0;
	bool l;
	duk_push_heapptr(cx, parent);
	l = duk_has_prop_string(cx, -1, name);
	duk_pop(cx);
	return l;
}

void delete_property_0(jsobjtype cx0, jsobjtype parent, const char *name)
{
	duk_context * cx = cx0;
	duk_push_heapptr(cx, parent);
	duk_del_prop_string(cx, -1, name);
	duk_pop(cx);
}				/* delete_property_0 */

int get_arraylength_0(jsobjtype cx0, jsobjtype a)
{
	duk_context * cx = cx0;
	int l;
	duk_push_heapptr(cx, a);
	if (duk_is_array(cx, -1))
		l = duk_get_length(cx, -1);
	else
		l = -1;
	duk_pop(cx);
	return l;
}				/* get_arraylength_0 */

/* Return a property as a string, if it is
 * string compatible. The string is allocated, free it when done. */
char *get_property_string_0(jsobjtype cx0, jsobjtype parent, const char *name)
{
	duk_context * cx = cx0;
	const char *s;
	char *s0;
	enum ej_proptype proptype;
	duk_push_heapptr(cx, parent);
	duk_get_prop_string(cx, -1, name);
	proptype = top_proptype(cx);
	if (proptype == EJ_PROP_NONE) {
		duk_pop_2(cx);
		return NULL;
	}
	if (duk_is_object(cx, -1)) {
/* special code here to return the object pointer */
/* That's what edbrowse is going to want. */
		jsobjtype o = duk_get_heapptr(cx, -1);
		s = pointer2string(o);
	} else
		s = duk_safe_to_string(cx, -1);
	if (!s)
		s = emptyString;
	s0 = cloneString(s);
	duk_pop_2(cx);
	return s0;
}				/* get_property_string_0 */

jsobjtype get_property_object_0(jsobjtype cx0, jsobjtype parent, const char *name)
{
	duk_context * cx = cx0;
	jsobjtype o = NULL;
	duk_push_heapptr(cx, parent);
	duk_get_prop_string(cx, -1, name);
	if (duk_is_object(cx, -1))
		o = duk_get_heapptr(cx, -1);
	duk_pop_2(cx);
	return o;
}				/* get_property_object_0 */

jsobjtype get_property_function_0(jsobjtype cx0, jsobjtype parent, const char *name)
{
	duk_context * cx = cx0;
	jsobjtype o = NULL;
	duk_push_heapptr(cx, parent);
	duk_get_prop_string(cx, -1, name);
	if (duk_is_function(cx, -1))
		o = duk_get_heapptr(cx, -1);
	duk_pop_2(cx);
	return o;
}				/* get_property_function_0 */

int get_property_number_0(jsobjtype cx0, jsobjtype parent, const char *name)
{
	duk_context * cx = cx0;
	int n = -1;
	duk_push_heapptr(cx, parent);
	duk_get_prop_string(cx, -1, name);
	if (duk_is_number(cx, -1)) {
		double d = duk_get_number(cx, -1);
		n = d;		// truncate
	}
	duk_pop_2(cx);
	return n;
}				/* get_property_number_0 */

double get_property_float_0(jsobjtype cx0, jsobjtype parent, const char *name)
{
	duk_context * cx = cx0;
	double d = -1;
	duk_push_heapptr(cx, parent);
	duk_get_prop_string(cx, -1, name);
	if (duk_is_number(cx, -1))
		d = duk_get_number(cx, -1);
	duk_pop_2(cx);
	return d;
}				/* get_property_float_0 */

bool get_property_bool_0(jsobjtype cx0, jsobjtype parent, const char *name)
{
	duk_context * cx = cx0;
	bool b = false;
	duk_push_heapptr(cx, parent);
	duk_get_prop_string(cx, -1, name);
	if (duk_is_number(cx, -1)) {
		if (duk_get_number(cx, -1))
			b = true;
	}
	if (duk_is_boolean(cx, -1)) {
		if (duk_get_boolean(cx, -1))
			b = true;
	}
	duk_pop_2(cx);
	return b;
}				/* get_property_bool_0 */

int set_property_string_0(jsobjtype cx0, jsobjtype parent, const char *name,
			    const char *value)
{
	duk_context * cx = cx0;
	bool defset = false;
	duk_c_function setter = NULL;
	duk_c_function getter = NULL;
	const char *altname;
	duk_push_heapptr(cx, parent);
	if (stringEqual(name, "innerHTML"))
		setter = setter_innerHTML, getter = getter_innerHTML,
		    altname = "inner$HTML";
	if (stringEqual(name, "value")) {
// This one is complicated. If option.value had side effects,
// that would only serve to confuse.
		bool valsetter = true;
		duk_get_global_string(cx, "Option");
		if (duk_instanceof(cx, -2, -1))
			valsetter = false;
		duk_pop(cx);
		duk_get_global_string(cx, "Select");
		if (duk_instanceof(cx, -2, -1)) {
			valsetter = false;
			puts("select.value set! This shouldn't happen.");
		}
		duk_pop(cx);
		if (valsetter)
			setter = setter_value,
			    getter = getter_value, altname = "val$ue";
	}
	if (setter) {
		if (!duk_get_prop_string(cx, -1, name))
			defset = true;
		duk_pop(cx);
	}
	if (defset) {
		duk_push_string(cx, name);
		duk_push_c_function(cx, getter, 0);
		duk_push_c_function(cx, setter, 1);
		duk_def_prop(cx, -4,
			     (DUK_DEFPROP_HAVE_SETTER | DUK_DEFPROP_HAVE_GETTER
			      | DUK_DEFPROP_SET_ENUMERABLE));
	}
	if (!value)
		value = emptyString;
	duk_push_string(cx, value);
	duk_put_prop_string(cx, -2, (setter ? altname : name));
	duk_pop(cx);
	return 0;
}				/* set_property_string_0 */

int set_property_bool_0(jsobjtype cx0, jsobjtype parent, const char *name, bool n)
{
	duk_context * cx = cx0;
	duk_push_heapptr(cx, parent);
	duk_push_boolean(cx, n);
	duk_put_prop_string(cx, -2, name);
	duk_pop(cx);
	return 0;
}				/* set_property_bool_0 */

int set_property_number_0(jsobjtype cx0, jsobjtype parent, const char *name, int n)
{
	duk_context * cx = cx0;
	duk_push_heapptr(cx, parent);
	duk_push_int(cx, n);
	duk_put_prop_string(cx, -2, name);
	duk_pop(cx);
	return 0;
}				/* set_property_number_0 */

int set_property_float_0(jsobjtype cx0, jsobjtype parent, const char *name, double n)
{
	duk_context * cx = cx0;
	duk_push_heapptr(cx, parent);
	duk_push_number(cx, n);
	duk_put_prop_string(cx, -2, name);
	duk_pop(cx);
	return 0;
}				/* set_property_float_0 */

int set_property_object_0(jsobjtype cx0, jsobjtype parent, const char *name, jsobjtype child)
{
	duk_context * cx = cx0;
	duk_push_heapptr(cx, parent);

// Special code for frame.contentDocument
	if (stringEqual(name, "contentDocument")) {
		bool rc;
		duk_get_global_string(cx, "Frame");
		rc = duk_instanceof(cx, -2, -1);
		duk_pop(cx);
		if (rc) {
			duk_push_string(cx, name);
			duk_push_c_function(cx, getter_cd, 0);
			duk_push_c_function(cx, setter_cd, 1);
			duk_def_prop(cx, -4,
				     (DUK_DEFPROP_HAVE_SETTER |
				      DUK_DEFPROP_HAVE_GETTER |
				      DUK_DEFPROP_SET_ENUMERABLE));
			name = "content$Document";
		}
	}

	if (stringEqual(name, "contentWindow")) {
		bool rc;
		duk_get_global_string(cx, "Frame");
		rc = duk_instanceof(cx, -2, -1);
		duk_pop(cx);
		if (rc) {
			duk_push_string(cx, name);
			duk_push_c_function(cx, getter_cw, 0);
			duk_push_c_function(cx, setter_cw, 1);
			duk_def_prop(cx, -4,
				     (DUK_DEFPROP_HAVE_SETTER |
				      DUK_DEFPROP_HAVE_GETTER |
				      DUK_DEFPROP_SET_ENUMERABLE));
			name = "content$Window";
		}
	}

	duk_push_heapptr(cx, child);
	duk_put_prop_string(cx, -2, name);
	duk_pop(cx);
	return 0;
}				/* set_property_object_0 */

// handler.toString = function() { return this.body; }
static duk_ret_t native_fntos(duk_context * cx)
{
	duk_push_this(cx);
	duk_get_prop_string(cx, -1, "body");
	duk_remove(cx, -2);
	return 1;
}

int set_property_function_0(jsobjtype cx0, jsobjtype parent, const char *name,
			      const char *body)
{
	duk_context * cx = cx0;
	char *body2, *s;
	int l;
	if (!body || !*body) {
// null or empty function, function will return null.
		body = "null";
	}
	duk_push_string(cx, body);
	duk_push_string(cx, name);
	if (duk_pcompile(cx, 0)) {
		processError(cx);
		debugPrint(3, "compile error for %p.%s", parent, name);
		duk_push_c_function(cx, native_error_stub_1, 0);
	}
// At this point I have to undo the mashinations performed by handlerSet().
	s = body2 = cloneString(body);
	l = strlen(s);
	if (l > 16 && stringEqual(s + l - 16, " }.bind(this))()")) {
		s[l - 16] = 0;
		if (!strncmp(s, "(function(){", 12))
			s += 12;
	}
	duk_push_string(cx, s);
	nzFree(body2);
	duk_put_prop_string(cx, -2, "body");
	duk_push_c_function(cx, native_fntos, 0);
	duk_put_prop_string(cx, -2, "toString");
	duk_push_heapptr(cx, parent);
	duk_insert(cx, -2);	// switch places
	duk_put_prop_string(cx, -2, name);
	duk_pop(cx);
	return 0;
}

/*********************************************************************
Error object is at the top of the duktape stack.
Extract the line number, call stack, and error message,
the latter being error.toString().
Leave the result in errorMessage, which is sent to edbrowse in the 2 process
model, or printed right now if JS1 is set.
Pop the error object when done.
*********************************************************************/

static void processError(duk_context * cx)
{
	const char *callstack = emptyString;
	int offset = 0;
	char *cut, *s;

	if (duk_get_prop_string(cx, -1, "lineNumber"))
		offset = duk_get_int(cx, -1);
	duk_pop(cx);

	if (duk_get_prop_string(cx, -1, "stack"))
		callstack = duk_to_string(cx, -1);
	nzFree(errorMessage);
	errorMessage = cloneString(duk_to_string(cx, -2));
	if (strstr(errorMessage, "callstack") && strlen(callstack)) {
// this is rare.
		nzFree(errorMessage);
		errorMessage = cloneString(callstack);
	}
	if (offset) {
		jsLineno += (offset - 1);
// symtax error message includes the relative line number, which is confusing
// since edbrowse prints the absolute line number.
		cut = strstr(errorMessage, " (line ");
		if (cut) {
			s = cut + 7;
			while (isdigit(*s))
				++s;
			if (stringEqual(s, ")"))
				*cut = 0;
		}
	}
	duk_pop(cx);

	if (debugLevel >= 3) {
/* print message, this will be in English, and mostly for our debugging */
		if (jsSourceFile) {
			if (debugFile)
				fprintf(debugFile, "%s line %d: ",
					jsSourceFile, jsLineno);
			else
				printf("%s line %d: ", jsSourceFile, jsLineno);
		}
		debugPrint(3, "%s", errorMessage);
	}
	free(errorMessage);
	errorMessage = 0;
}

/*********************************************************************
No arguments; returns abool.
This function is typically used for handlers: onclick, onchange, onsubmit, onload, etc.
The return value is sometimes significant.
If a hyperlink has an onclick function, and said function returns false,
the hyperlink is not followed.
If onsubmit returns false the form does not submit.
And yet this opens a can of worms. Here is my default behavior for corner cases.
I generally want the browser to continue, unless the function
explicitly says false.
Edbrowse should do as much as it can for the casual user.
Javascript function returns boolean. Pass this value back.
Function returns number. nonzero is true and zero is false.
Function returns string. "false" is false and everything else is true.
Function returns a bogus type like object. true
Function returns undefined. true
Function doesn't exist. true, unless debugging.
Function encounters an error during execution. true, unless debugging.
*********************************************************************/

/*********************************************************************
For debugging; please leave the stack the way you found it.
As you climb up the tree, check for parentNode = null.
null is an object so it passes the object test.
This should never happen, but does in http://4x4dorogi.net
Also check for recursion.
If there is an error fetching nodeName or class, e.g. when the node is null,
(if we didn't check for parentNode = null in the above website),
then asking for nodeName causes yet another runtime error.
This invokes our machinery again, including uptrace if debug is on,
and it invokes the duktape machinery again as well.
The resulting core dump has the stack so corrupted, that gdb is hopelessly confused.
*********************************************************************/

static void uptrace(duk_context * cx, jsobjtype node)
{
	static bool infunction = false;
	int t;
	if (debugLevel < 3)
		return;
	if(infunction) {
		debugPrint(3, "uptrace recursion; this is unrecoverable!");
		exit(1);
	}
	infunction = true;
	duk_push_heapptr(cx, node);
	while (true) {
		const char *nn, *cn;	// node name class name
		char nnbuf[20];
		if (duk_get_prop_string(cx, -1, "nodeName"))
			nn = duk_to_string(cx, -1);
		else
			nn = "?";
		strncpy(nnbuf, nn, 20);
		nnbuf[20 - 1] = 0;
		if (!nnbuf[0])
			strcpy(nnbuf, "?");
		duk_pop(cx);
		if (duk_get_prop_string(cx, -1, "class"))
			cn = duk_to_string(cx, -1);
		else
			cn = "?";
		debugPrint(3, "%s.%s", nnbuf, (cn[0] ? cn : "?"));
		duk_pop(cx);
		if (!duk_get_prop_string(cx, -1, "parentNode")) {
// we're done.
			duk_pop_2(cx);
			break;
		}
		duk_remove(cx, -2);
		t = top_proptype(cx);
		if(t == EJ_PROP_NULL) {
			debugPrint(3, "null");
			duk_pop(cx);
			break;
		}
		if(t != EJ_PROP_OBJECT) {
			debugPrint(3, "parentNode not object, type %d", t);
			duk_pop(cx);
			break;
		}
	}
	debugPrint(3, "end uptrace");
	infunction = false;
}

bool run_function_bool_0(jsobjtype cx0, jsobjtype parent, const char *name)
{
	duk_context * cx = cx0;
	int dbl = 3;		// debug level
	int seqno = -1;
	duk_push_heapptr(cx, parent);
	if (stringEqual(name, "ontimer")) {
		dbl = 4;
		if (duk_get_prop_string(cx, -1, "tsn"))
			seqno = duk_get_int(cx, -1);
		duk_pop(cx);
	}
	if (!duk_get_prop_string(cx, -1, name) || !duk_is_function(cx, -1)) {
#if 0
		if (!errorMessage)
			asprintf(&errorMessage, "no such function %s", name);
#endif
		duk_pop_2(cx);
		return (debugLevel < 3);
	}
	duk_insert(cx, -2);
	if (seqno > 0)
		debugPrint(dbl, "exec %s timer %d", name, seqno);
	else
		debugPrint(dbl, "exec %s", name);
	if (!duk_pcall_method(cx, 0)) {
		bool rc = true;
		debugPrint(dbl, "exec complete");
		if (duk_is_boolean(cx, -1))
			rc = duk_get_boolean(cx, -1);
		if (duk_is_number(cx, -1))
			rc = (duk_get_number(cx, -1) != 0);
		if (duk_is_string(cx, -1)) {
			const char *b = duk_get_string(cx, -1);
			if (stringEqualCI(b, "false"))
				rc = false;
		}
		duk_pop(cx);
		return rc;
	}
// error in execution
	if (intFlag)
		i_puts(MSG_Interrupted);
	processError(cx);
	debugPrint(3, "failure on %p.%s()", parent, name);
	uptrace(cx, parent);
	debugPrint(3, "exec complete");
	return (debugLevel < 3);
}				/* run_function_bool_0 */

// The single argument to the function has to be an object.
// Returns -1 if the return is not int or bool
int run_function_onearg_0(jsobjtype cx0, jsobjtype parent, const char *name, jsobjtype child)
{
	duk_context * cx = cx0;
	int rc = -1;
	duk_push_heapptr(cx, parent);
	if (!duk_get_prop_string(cx, -1, name) || !duk_is_function(cx, -1)) {
#if 0
		if (!errorMessage)
			asprintf(&errorMessage, "no such function %s", name);
#endif
		duk_pop_2(cx);
		return rc;
	}
	duk_insert(cx, -2);
	duk_push_heapptr(cx, child);	// child is the only argument
	if (!duk_pcall_method(cx, 1)) {
// See if return is int or bool
		enum ej_proptype t = top_proptype(cx);
		if (t == EJ_PROP_BOOL)
			rc = duk_get_boolean(cx, -1);
		if (t == EJ_PROP_INT)
			rc = duk_get_number(cx, -1);
		duk_pop(cx);
		return rc;
	}
// error in execution
	if (intFlag)
		i_puts(MSG_Interrupted);
	processError(cx);
	debugPrint(3, "failure on %p.%s[]", parent, name);
	uptrace(cx, parent);
	return rc;
}				/* run_function_onearg_0 */

// The single argument to the function has to be a string.
void run_function_onestring_0(jsobjtype cx0, jsobjtype parent, const char *name,
				const char *s)
{
	duk_context * cx = cx0;
	duk_push_heapptr(cx, parent);
	if (!duk_get_prop_string(cx, -1, name) || !duk_is_function(cx, -1)) {
#if 0
		if (!errorMessage)
			asprintf(&errorMessage, "no such function %s", name);
#endif
		duk_pop_2(cx);
		return;
	}
	duk_insert(cx, -2);
	duk_push_string(cx, s);	// s is the only argument
	if (!duk_pcall_method(cx, 1)) {
		duk_pop(cx);
		return;
	}
// error in execution
	if (intFlag)
		i_puts(MSG_Interrupted);
	processError(cx);
	debugPrint(3, "failure on %p.%s[]", parent, name);
	uptrace(cx, parent);
}				/* run_function_onestring_0 */

jsobjtype instantiate_array_0(jsobjtype cx0, jsobjtype parent, const char *name)
{
	duk_context * cx = cx0;
	jsobjtype a;
	duk_push_heapptr(cx, parent);
	if (duk_get_prop_string(cx, -1, name) && duk_is_array(cx, -1)) {
		a = duk_get_heapptr(cx, -1);
		duk_pop_2(cx);
		return a;
	}
	duk_pop(cx);
	duk_get_global_string(cx, "Array");
	if (duk_pnew(cx, 0)) {
		processError(cx);
		debugPrint(3, "failure on %p.%s = []", parent, name);
		uptrace(cx, parent);
		duk_pop(cx);
		return 0;
	}
	a = duk_get_heapptr(cx, -1);
	duk_put_prop_string(cx, -2, name);
	duk_pop(cx);
	return a;
}				/* instantiate_array_0 */

jsobjtype instantiate_0(jsobjtype cx0, jsobjtype parent, const char *name,
			  const char *classname)
{
	duk_context * cx = cx0;
	jsobjtype a;
	duk_push_heapptr(cx, parent);
	if (duk_get_prop_string(cx, -1, name) && duk_is_object(cx, -1)) {
// I'll assume the object is of the proper class.
		a = duk_get_heapptr(cx, -1);
		duk_pop_2(cx);
		return a;
	}
	duk_pop(cx);
	if (!classname)
		classname = "Object";
	if (!duk_get_global_string(cx, classname)) {
		fprintf(stderr, "unknown class %s, cannot instantiate\n",
			classname);
		exit(8);
	}
	if (duk_pnew(cx, 0)) {
		processError(cx);
		debugPrint(3, "failure on %p.%s = new %s", parent, name,
			   classname);
		uptrace(cx, parent);
		duk_pop(cx);
		return 0;
	}
	a = duk_get_heapptr(cx, -1);
	duk_put_prop_string(cx, -2, name);
	duk_pop(cx);
	return a;
}				/* instantiate_0 */

jsobjtype instantiate_array_element_0(jsobjtype cx0, jsobjtype parent, int idx,
					const char *classname)
{
	duk_context * cx = cx0;
	jsobjtype a;
	if (!classname)
		classname = "Object";
	duk_push_heapptr(cx, parent);
	duk_get_global_string(cx, classname);
	if (duk_pnew(cx, 0)) {
		processError(cx);
		debugPrint(3, "failure on %p[%d] = new %s", parent, idx,
			   classname);
		uptrace(cx, parent);
		duk_pop(cx);
		return 0;
	}
	a = duk_get_heapptr(cx, -1);
	duk_put_prop_index(cx, -2, idx);
	duk_pop(cx);
	return a;
}

int set_array_element_object_0(jsobjtype cx0, jsobjtype parent, int idx, jsobjtype child)
{
	duk_context * cx = cx0;
	duk_push_heapptr(cx, parent);
	duk_push_heapptr(cx, child);
	duk_put_prop_index(cx, -2, idx);
	duk_pop(cx);
	return 0;
}

jsobjtype get_array_element_object_0(jsobjtype cx0, jsobjtype parent, int idx)
{
	duk_context * cx = cx0;
	jsobjtype a = 0;
	duk_push_heapptr(cx, parent);
	duk_get_prop_index(cx, -1, idx);
	if (duk_is_object(cx, -1))
		a = duk_get_heapptr(cx, -1);
	duk_pop_2(cx);
	return a;
}

char *run_script_0(jsobjtype cx0, const char *s)
{
	duk_context * cx = cx0;
	char *result = 0;
	bool rc;
	const char *gc;
	char *s2 = 0;

// special debugging code to replace bp@ and trace@ with expanded macros.
// Warning: breakpoints and tracing can change the flow of execution
// prior to duktape commit 67c891d9e075cc49281304ff5955cae24faa1496
	if (strstr(s, "bp@(") || strstr(s, "trace@(")) {
		int l;
		const char *u, *v1, *v2;
		s2 = initString(&l);
		u = s;
		while (true) {
			v1 = strstr(u, "bp@(");
			v2 = strstr(u, "trace@(");
			if (v1 && v2 && v2 < v1)
				v1 = v2;
			if (!v1)
				v1 = v2;
			if (!v1)
				break;
			stringAndBytes(&s2, &l, u, v1 - u);
			stringAndString(&s2, &l, (*v1 == 'b' ?
						  ";(function(arg$,l$ne){if(l$ne) alert('break at line ' + l$ne); while(true){var res = prompt('bp'); if(!res) continue; if(res === '.') break; try { res = eval(res); alert(res); } catch(e) { alert(e.toString()); }}}).call(this,(typeof arguments=='object'?arguments:[]),\""
						  :
						  ";(function(arg$,l$ne){ if(l$ne === step$go||typeof step$exp==='string'&&eval(step$exp)) step$l = 2; if(step$l == 0) return; if(step$l == 1) { alert3(l$ne); return; } if(l$ne) alert('break at line ' + l$ne); while(true){var res = prompt('bp'); if(!res) continue; if(res === '.') break; try { res = eval(res); alert(res); } catch(e) { alert(e.toString()); }}}).call(this,(typeof arguments=='object'?arguments:[]),\""));
			v1 = strchr(v1, '(') + 1;
			v2 = strchr(v1, ')');
			stringAndBytes(&s2, &l, v1, v2 - v1);
			stringAndString(&s2, &l, "\");");
			u = ++v2;
		}
		stringAndString(&s2, &l, u);
	}

	rc = duk_peval_string(cx, (s2 ? s2 : s));
	nzFree(s2);
	if (intFlag)
		i_puts(MSG_Interrupted);
	if (!rc) {
		s = duk_safe_to_string(cx, -1);
		if (s && !*s)
			s = 0;
		if (s)
			result = cloneString(s);
		duk_pop(cx);
	} else {
		processError(cx);
	}
	gc = getenv("JSGC");
	if (gc && *gc)
		duk_gc(cx, 0);
	return result;
}

// execute script.text code; more efficient than the above.
void run_data_0(jsobjtype cx0, jsobjtype o)
{
	duk_context * cx = cx0;
	bool rc;
	const char *s, *gc;
	duk_push_heapptr(cx, o);
	if (!duk_get_prop_string(cx, -1, "text")) {
// no data
		duk_pop_2(cx);
		return;
	}
	s = duk_safe_to_string(cx, -1);
	if (!s || !*s)
		return;
// defer to the earlier routine if there are breakpoints
	if (strstr(s, "bp@(") || strstr(s, "trace@(")) {
		run_script_0(cx, s);
		duk_pop_2(cx);
		return;
	}
	rc = duk_peval_string(cx, s);
	if (intFlag)
		i_puts(MSG_Interrupted);
	if (!rc) {
		duk_pop_n(cx, 3);
	} else {
		processError(cx);
		duk_pop_2(cx);
	}
	gc = getenv("JSGC");
	if (gc && *gc)
		duk_gc(cx, 0);
}

