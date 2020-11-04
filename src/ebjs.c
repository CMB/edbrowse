/* ebjs.c: edbrowse javascript engine interface.
 *
 * Launch the js engine process and communicate with it to build
 * js objects and run js code.
 * Also provide some wrapper functions like get_property_string,
 * so that edbrowse can call functions to manipulate js objects,
 * thus hiding the details of sending messages to the js process
 * and receiving replies from same. */

#include "eb.h"

#include <stdarg.h>

/* If connection is lost, mark all js sessions as dead. */
static void markAllDead(void)
{
	int cx;			/* edbrowse context */
	struct ebWindow *w;
	Frame *f;
	bool killed = false;

	for (cx = 1; cx <= maxSession; ++cx) {
		w = sessionList[cx].lw;
		while (w) {
			for (f = &w->f0; f; f = f->next) {
				if (!f->winobj)
					continue;
				f->winobj = 0;
				f->docobj = 0;
				f->cx = 0;
				killed = true;
			}
			w = w->prev;
		}
	}

	if (killed)
		i_puts(MSG_JSCloseSessions);
}				/* markAllDead */

static int js_pid;

/* Start the js process. */
static void js_start(void)
{
	debugPrint(5, "setting of communication channels for javascript");
	if (js_main()) {
		i_puts(MSG_JSEngineRun);
		markAllDead();
	} else {
		js_pid = 1;
	}
}				/* js_start */

/* Javascript has changed an input field */
static void javaSetsInner(jsobjtype v, const char *newtext);
void javaSetsTagVar(jsobjtype v, const char *newtext)
{
	Tag *t = tagFromJavaVar(v);
	if (!t)
		return;
	if (t->itype == INP_HIDDEN || t->itype == INP_RADIO
	    || t->itype == INP_FILE)
		return;
	if (t->itype == INP_TA) {
		javaSetsInner(v, newtext);
		return;
	}
	nzFree(t->value);
	t->value = cloneString(newtext);
}				/* javaSetsTagVar */

static void javaSetsInner(jsobjtype v, const char *newtext)
{
	int side;
	Tag *t = tagFromJavaVar(v);
	if (!t)
		return;
/* the tag should always be a textarea tag. */
	if (t->action != TAGACT_INPUT || t->itype != INP_TA) {
		debugPrint(3,
			   "innerText is applied to tag %d that is not a textarea.",
			   t->seqno);
		return;
	}
	side = t->lic;
	if (side <= 0 || side >= MAXSESSION || side == context)
		return;
	if (sessionList[side].lw == NULL)
		return;
	if (cw->browseMode)
		i_printf(MSG_BufferUpdated, side);
	sideBuffer(side, newtext, -1, 0);
}				/* javaSetsInner */

/* start a document.write */
void dwStart(void)
{
	if (cf->dw)
		return;
	cf->dw = initString(&cf->dw_l);
	stringAndString(&cf->dw, &cf->dw_l, "<!DOCTYPE public><body>");
}				/* dwStart */

static const char *debugString(const char *v)
{
	if (!v)
		return emptyString;
	if (strlen(v) > 100)
		return "long";
	return v;
}				/* debugString */

// Create a js context for the current frame.
// The corresponding js context will be stored in cf->cx.
// We don't pass frame as a parameter, cf (current frame) is assumed.
void createJavaContext(void)
{
	if (!allowJS)
		return;

	if (!js_pid)
		js_start();

	debugPrint(5, "> create context for session %d", context);
	createJavaContext_0(cf);
	if (cf->cx) {
		debugPrint(5, "< ok");
		setupJavaDom();
	} else {
		debugPrint(5, "< error");
		i_puts(MSG_JavaContextError);
	}
}				/* createJavaContext */

/*********************************************************************
This is unique among all the wrappered calls in that it can be made for
a window that is not the current window.
You can free another window, or a whole stack of windows, by typeing
q2 while in session 1.
*********************************************************************/

void freeJavaContext(Frame *f)
{
	if (!f->cx)
		return;
	debugPrint(5, "> free frame %p", f);
	freeJavaContext_0(f->cx);
	f->cx = f->winobj = f->docobj = 0;
	debugPrint(5, "< ok");
	cssFree(f);
}				/* freeJavaContext */

/* Run some javascript code under the current window */
/* Pass the return value of the script back as a string. */
char *jsRunScriptResult(const Frame *f, jsobjtype obj, const char *str,
const char *filename, 			int lineno)
{
	char *result;

// this never runs from the j process.
	if (whichproc != 'e') {
		debugPrint(1, "jsRunScript run from the js process");
		return NULL;
	}

	if (!allowJS || !f->winobj)
		return NULL;
	if (!str || !str[0])
		return NULL;

	debugPrint(5, "> script:");
	jsSourceFile = filename;
	jsLineno = lineno;
	whichproc = 'j';
	result = run_script_0(f->cx, str);
	whichproc = 'e';
	jsSourceFile = NULL;
	debugPrint(5, "< ok");
	return result;
}				/* jsRunScriptResult */

/* like the above but throw away the result */
void jsRunScript(const Frame *f, jsobjtype obj, const char *str,
const char *filename, 		 int lineno)
{
	char *s = jsRunScriptResult(f, obj, str, filename, lineno);
	nzFree(s);
}				/* jsRunScript */

void jsRunData(const Frame *f, jsobjtype obj,
const char *filename, int lineno)
{
// this never runs from the j process.
	if (whichproc != 'e') {
		debugPrint(1, "jsRunData run from the js process");
		return;
	}
	if (!allowJS || !f->winobj || !obj)
		return;
	debugPrint(5, "> script:");
	jsSourceFile = filename;
	jsLineno = lineno;
	whichproc = 'j';
	run_data_0(f->cx, obj);
	whichproc = 'e';
	jsSourceFile = NULL;
	debugPrint(5, "< ok");
}

/* does the member exist in the object or its prototype? */
bool has_property(const Frame *f, jsobjtype obj, const char *name)
{
	bool p = false;
	if (!obj) {
		debugPrint(3, "has_property(0, %s)", name);
		return false;
	}
	if (whichproc == 'j')
		return has_property_0(f->cx, obj, name);
	if (!allowJS || !f->winobj)
		return false;
	debugPrint(5, "> has %s", name);
	p = has_property_0(f->cx, obj, name);
	debugPrint(5, "< %s", (p ? "true" : "false"));
	return p;
}				/* has_property */

/* return the type of the member */
enum ej_proptype typeof_property(const Frame *f, jsobjtype obj, const char *name)
{
	enum ej_proptype p;
	if (!obj) {
		debugPrint(3, "typeof_property(0, %s)", name);
		return EJ_PROP_NONE;
	}
	if (whichproc == 'j')
		return typeof_property_0(f->cx, obj, name);
	if (!allowJS || !f->winobj)
		return EJ_PROP_NONE;
	debugPrint(5, "> has %s", name);
	p = typeof_property_0(f->cx, obj, name);
	debugPrint(5, "< %d", p);
	return p;
}				/* typeof_property */

void delete_property(const Frame *f, jsobjtype obj, const char *name)
{
	if (!obj) {
		debugPrint(3, "delete_property(0, %s)", name);
		return;
	}
	if (whichproc == 'j') {
		delete_property_0(f->cx, obj, name);
		return;
	}
	if (!allowJS || !f->winobj)
		return;
	debugPrint(5, "> delete %s", name);
	delete_property_0(f->cx, obj, name);
	debugPrint(5, "< ok");
}				/* delete_property */

// allocated; the caller must free it
char *get_property_string(const Frame *f, jsobjtype obj, const char *name)
{
	char *s;
	if (!allowJS || !f->winobj)
		return 0;
	if (!obj) {
		debugPrint(3, "get_property_string(0, %s)", name);
		return 0;
	}
	if (whichproc == 'j')
		return get_property_string_0(f->cx, obj, name);
	debugPrint(5, "> get %s", name);
	s = get_property_string_0(f->cx, obj, name);
	debugPrint(5, "< %s", debugString(s));
	return s;
}				/* get_property_string */

int get_property_number(const Frame *f, jsobjtype obj, const char *name)
{
	int n = -1;
	if (!obj) {
		debugPrint(3, "get_property_number(0, %s)", name);
		return -1;
	}
	if (whichproc == 'j')
		return get_property_number_0(f->cx, obj, name);
	if (!allowJS || !f->winobj)
		return -1;
	debugPrint(5, "> get %s", name);
	n = get_property_number_0(f->cx, obj, name);
	debugPrint(5, "< %d", n);
	return n;
}				/* get_property_number */

double get_property_float(const Frame *f, jsobjtype obj, const char *name)
{
	double n = 0.0;
	if (!obj) {
		debugPrint(3, "get_property_float(0, %s)", name);
		return n;
	}
	if (whichproc == 'j')
		return get_property_float_0(f->cx, obj, name);
	if (!allowJS || !f->winobj)
		return n;
	debugPrint(5, "> get %s", name);
	n = get_property_float_0(f->cx, obj, name);
	debugPrint(5, "< %lf", n);
	return n;
}				/* get_property_float */

bool get_property_bool(const Frame *f, jsobjtype obj, const char *name)
{
	bool n = false;
	if (!obj) {
		debugPrint(3, "get_property_bool(0, %s)", name);
		return n;
	}
	if (whichproc == 'j')
		return get_property_bool_0(f->cx, obj, name);
	if (!allowJS || !f->winobj)
		return n;
	debugPrint(5, "> get %s", name);
	n = get_property_bool_0(f->cx, obj, name);
	debugPrint(5, "< %s", (n ? "treu" : "false"));
	return n;
}				/* get_property_bool */

/* get a js object as a member of another object */
jsobjtype get_property_object(const Frame *f, jsobjtype parent, const char *name)
{
	jsobjtype child = 0;
	if (!parent) {
		debugPrint(3, "get_property_object(0, %s)", name);
		return child;
	}
	if (whichproc == 'j')
		return get_property_object_0(f->cx, parent, name);
	if (!allowJS || !f->winobj)
		return child;
	debugPrint(5, "> get %s", name);
	child = get_property_object_0(f->cx, parent, name);
	debugPrint(5, "< %p", child);
	return child;
}				/* get_property_object */

jsobjtype get_property_function(const Frame *f, jsobjtype parent, const char *name)
{
	jsobjtype child = 0;
	if (!parent) {
		debugPrint(3, "get_property_function(0, %s)", name);
		return child;
	}
	if (whichproc == 'j')
		return get_property_function_0(f->cx, parent, name);
	if (!allowJS || !f->winobj)
		return child;
	debugPrint(5, "> get %s", name);
	child = get_property_function_0(f->cx, parent, name);
	debugPrint(5, "< %p", child);
	return child;
}				/* get_property_function */

jsobjtype get_array_element_object(const Frame *f, jsobjtype obj, int idx)
{
	jsobjtype p = 0;
	if (!allowJS || !f->winobj)
		return p;
	if (!obj) {
		debugPrint(3, "get_array_element_object(0, %d)", idx);
		return p;
	}
	if (whichproc == 'j')
		return get_array_element_object_0(f->cx, obj, idx);
	debugPrint(5, "> get [%d]", idx);
	p = get_array_element_object_0(f->cx, obj, idx);
	debugPrint(5, "< %p", p);
	return p;
}				/* get_array_element_object */

int set_property_string(const Frame *f, jsobjtype obj, const char *name, const char *value)
{
	int rc;
	if (!allowJS || !f->winobj)
		return -1;
	if (!obj) {
		debugPrint(3, "set_property_string(0, %s, %s)", name, value);
		return -1;
	}
	if (whichproc == 'j')
		return set_property_string_0(f->cx, obj, name, value);
	if (value == NULL)
		value = emptyString;
	debugPrint(5, "> set %s=%s", name, debugString(value));
	rc = set_property_string_0(f->cx, obj, name, value);
	debugPrint(5, "< ok");
	return rc;
}				/* set_property_string */

int set_property_number(const Frame *f, jsobjtype obj, const char *name, int n)
{
	int rc;
	if (!allowJS || !f->winobj)
		return -1;
	if (!obj) {
		debugPrint(3, "set_property_number(0, %s, %d)", name, n);
		return -1;
	}
	if (whichproc == 'j')
		return set_property_number_0(f->cx, obj, name, n);
	debugPrint(5, "> set %s=%d", name, n);
	rc = set_property_number_0(f->cx, obj, name, n);
	debugPrint(5, "< ok");
	return rc;
}				/* set_property_number */

int set_property_float(const Frame *f, jsobjtype obj, const char *name, double n)
{
	int rc;
	if (!allowJS || !f->winobj)
		return -1;
	if (!obj) {
		debugPrint(3, "set_property(0, %s, %lf)", name, n);
		return -1;
	}
	if (whichproc == 'j')
		return set_property_float_0(f->cx, obj, name, n);
	debugPrint(5, "> set %s=%lf", name, n);
	rc = set_property_float_0(f->cx, obj, name, n);
	debugPrint(5, "< ok");
	return rc;
}				/* set_property_float */

int set_property_bool(const Frame *f, jsobjtype obj, const char *name, bool n)
{
	int rc;
	if (!allowJS || !f->winobj)
		return -1;
	if (!obj) {
		debugPrint(3, "set_property(0, %s, %d)", name, n);
		return -1;
	}
	if (whichproc == 'j')
		return set_property_bool_0(f->cx, obj, name, n);
	debugPrint(5, "> set %s=%s", name, (n ? "true" : "false"));
	rc = set_property_bool_0(f->cx, obj, name, n);
	debugPrint(5, "< ok");
	return rc;
}				/* set_property_bool */

int set_property_object(const Frame *f, jsobjtype parent, const char *name, jsobjtype child)
{
	int rc;
	if (!allowJS || !f->winobj)
		return -1;
	if (!parent) {
		debugPrint(3, "set_property_object(0, %s, %p)", name, child);
		return -1;
	}
	if (whichproc == 'j')
		return set_property_object_0(f->cx, parent, name, child);
	debugPrint(5, "> set %s=%p", name, child);
	rc = set_property_object_0(f->cx, parent, name, child);
	debugPrint(5, "< ok");
	return rc;
}				/* set_property_object */

jsobjtype instantiate_array(const Frame *f, jsobjtype parent, const char *name)
{
	jsobjtype p = 0;
	if (!allowJS || !f->winobj)
		return p;
	if (!parent) {
		debugPrint(3, "instantiate_array(0, %s)", name);
		return p;
	}
	if (whichproc == 'j')
		return instantiate_array_0(f->cx, parent, name);
	debugPrint(5, "> new array %s", name);
	p = instantiate_array_0(f->cx, parent, name);
	debugPrint(5, "< ok");
	return p;
}				/* instantiate_array */

int set_array_element_object(const Frame *f, jsobjtype array, int idx, jsobjtype child)
{
	int rc;
	if (!allowJS || !f->winobj)
		return -1;
	if (!array) {
		debugPrint(3, "set_array_element_object(0, %d)", idx);
		return -1;
	}
	if (whichproc == 'j')
		return set_array_element_object_0(f->cx, array, idx, child);
	debugPrint(5, "> set [%d]=%p", idx, child);
	rc = set_array_element_object_0(f->cx, array, idx, child);
	debugPrint(5, "< ok");
	return rc;
}				/* set_array_element_object */

jsobjtype instantiate_array_element(const Frame *f, jsobjtype array, int idx,
				    const char *classname)
{
	jsobjtype p = 0;
	if (!allowJS || !f->winobj)
		return p;
	if (!array) {
		debugPrint(3, "instantiate_array_element(0, %d, %s)", idx,
			   classname);
		return p;
	}
	if (whichproc == 'j')
		return instantiate_array_element_0(f->cx, array, idx, classname);
	debugPrint(5, "> set [%d]=%s", idx, classname);
	p = instantiate_array_element_0(f->cx, array, idx, classname);
	debugPrint(5, "< ok");
	return p;
}				/* instantiate_array_element */

/* Instantiate a new object from a given class.
 * Return is NULL if there is a js disaster.
 * Set classname = NULL for a generic object. */
jsobjtype instantiate(const Frame *f, jsobjtype parent, const char *name, const char *classname)
{
	jsobjtype p = 0;
	if (!allowJS || !f->winobj)
		return p;
	if (!parent) {
		debugPrint(3, "instantiate(0, %s, %s)", name, classname);
		return p;
	}
	if (whichproc == 'j')
		return instantiate_0(f->cx, parent, name, classname);
	debugPrint(5, "> instantiate %s %s", name,
		   (classname ? classname : "object"));
	p = instantiate_0(f->cx, parent, name, classname);
	debugPrint(5, "< ok");
	return p;
}				/* instantiate */

int set_property_function(const Frame *f, jsobjtype parent, const char *name, const char *body)
{
	int rc;
	if (!allowJS || !f->winobj)
		return -1;
	if (!parent) {
		debugPrint(3, "set_property_function(0, %s)", name);
		return -1;
	}
	if (whichproc == 'j')
		return set_property_function_0(f->cx, parent, name, body);
	if (!body)
		body = emptyString;
	debugPrint(5, "> set %s=%s", name, debugString(body));
	rc = set_property_function_0(f->cx, parent, name, body);
	debugPrint(5, "< ok");
	return rc;
}				/* set_property_function */

int get_arraylength(const Frame *f, jsobjtype a)
{
	int l;
	if (!allowJS || !f->winobj)
		return -1;
	if (!a) {
		debugPrint(3, "get_arraylength(0)");
		return -1;
	}
	if (whichproc == 'j')
		return get_arraylength_0(f->cx, a);
	debugPrint(5, "> get length");
	l = get_arraylength_0(f->cx, a);
	debugPrint(5, "< ok");
	return l;
}				/* get_arraylength */

/* run a function with no args that returns a boolean */
bool run_function_bool(const Frame *f, jsobjtype obj, const char *name)
{
	bool rc;
	if (!allowJS || !f->winobj)
		return false;
	if (!obj) {
		debugPrint(3, "run_function_bool(0, %s", name);
		return false;
	}
	if (intFlag)
		return false;
	if (whichproc == 'j')
		return run_function_bool_0(f->cx, obj, name);
	debugPrint(5, "> function %s", name);
	whichproc = 'j';
	rc = run_function_bool_0(f->cx, obj, name);
	whichproc = 'e';
	debugPrint(5, "< %s", (rc ? "true" : "false"));
	return rc;
}				/* run_function_bool */

jsobjtype create_event(const Frame *f, jsobjtype parent, const char *evname)
{
	jsobjtype e;
	const char *evname1 = evname;
	if (evname[0] == 'o' && evname[1] == 'n')
		evname1 += 2;
// gc$event protects from garbage collection
	e = instantiate(f, parent, "gc$event", "Event");
	set_property_string(f, e, "type", evname1);
	return e;
}

void unlink_event(const Frame *f, jsobjtype parent)
{
	delete_property(f, parent, "gc$event");
}

bool run_event_bool(const Frame *f, jsobjtype obj, const char *pname, const char *evname)
{
	int rc;
	jsobjtype eo;	// created event object
	if (!handlerPresent(f, obj, evname))
		return true;
	if (debugLevel >= 3) {
		bool evdebug = get_property_bool(f, f->winobj, "eventDebug");
		if (evdebug) {
			int seqno = get_property_number(f, obj, "eb$seqno");
			debugPrint(3, "trigger %s tag %d %s", pname, seqno, evname);
		}
	}
	eo = create_event(f, obj, evname);
	set_property_object(f, eo, "target", obj);
	set_property_object(f, eo, "currentTarget", obj);
	set_property_number(f, eo, "eventPhase", 2);
	rc = run_function_onearg(f, obj, evname, eo);
	unlink_event(f, obj);
// no return or some other return is treated as true in this case
	if (rc < 0)
		rc = true;
	return rc;
}

int run_function_onearg(const Frame *f, jsobjtype obj, const char *name, jsobjtype a)
{
	int rc;
	if (!allowJS || !f->winobj)
		return 0;
	if (!obj) {
		debugPrint(3, "run_function_onearg(0, %s", name);
		return 0;
	}
	if (whichproc == 'j')
		return run_function_onearg_0(f->cx, obj, name, a);
	debugPrint(5, "> function %s", name);
	whichproc = 'j';
	rc = run_function_onearg_0(f->cx, obj, name, a);
	whichproc = 'e';
	debugPrint(5, "< ok");
	return rc;
}

void run_function_onestring(const Frame *f, jsobjtype obj, const char *name, const char *s)
{
	if (!allowJS || !f->winobj)
		return;
	if (!obj) {
		debugPrint(3, "run_function_onestring(0, %s", name);
		return;
	}
	if (whichproc == 'j') {
		run_function_onestring_0(f->cx, obj, name, s);
		return;
	}
	debugPrint(5, "> function %s", name);
	whichproc = 'j';
	run_function_onestring_0(f->cx, obj, name, s);
	whichproc = 'e';
	debugPrint(5, "< ok");
}

/*********************************************************************
Everything beyond this point is, perhaps, part of a DOM support layer
above what has come before.
Still, these are library-like routines that are used repeatedly
by other files, particularly html.c and decorate.c.
*********************************************************************/

/* pass, to the js process, the filename,
 * or the <base href=url>, for relative url resolution on innerHTML.
 * This has to be retained per edbrowse buffer. */
void set_basehref(const char *h)
{
	if (!h)
		h = emptyString;
	set_property_string(cf, cf->winobj, "eb$base", h);
// This is special code for snapshot simulations.
// If the file jslocal is present, push base over to window.location etc,
// as though you were running that page.
	if (!access("jslocal", 4) && h[0]) {
		run_function_bool(cf, cf->winobj, "eb$base$snapshot");
		nzFree(cf->fileName);
		cf->fileName = cloneString(h);
	}
}				/* set_basehref */

#ifdef DOSLIKE			// port of uname(p), and struct utsname
struct utsname {
	char sysname[32];
	char machine[32];
};

int uname(struct utsname *pun)
{
	memset(pun, 0, sizeof(struct utsname));
	// TODO: WIN32: maybe fill in sysname, and machine...
	return 0;
}

#else // !DOSLIKE - // port of uname(p), and struct utsname
#include <sys/utsname.h>
#endif // DOSLIKE y/n // port of uname(p), and struct utsname

/* After createJavaContext, set up the document object and other variables
 * and methods that are base for client side DOM. */
void setupJavaDom(void)
{
	jsobjtype w = cf->winobj;	// window object
	jsobjtype d = cf->docobj;	// document object
	jsobjtype cx = cf->cx;	// current context
	jsobjtype nav;		// navigator object
	jsobjtype navpi;	// navigator plugins
	jsobjtype navmt;	// navigator mime types
	jsobjtype hist;		// history object
	struct MIMETYPE *mt;
	struct utsname ubuf;
	int i;
	char save_c;
	static const char *const languages[] = { 0,
		"english", "french", "portuguese", "polish",
		"german", "russian", "italian",
	};
	extern const char startWindowJS[];
	extern const char thirdJS[];

	set_property_object_0(cx, w, "window", w);

/* the js window/document setup script.
 * These are all the things that do not depend on the platform,
 * OS, configurations, etc. */
	jsRunScript(cf, w, startWindowJS, "StartWindow", 1);
	jsRunScript(cf, w, thirdJS, "Third", 1);

	nav = get_property_object_0(cx, w, "navigator");
	if (nav == NULL)
		return;
/* some of the navigator is in startwindow.js; the runtime properties are here. */
	set_property_string_0(cx, nav, "userLanguage", languages[eb_lang]);
	set_property_string_0(cx, nav, "language", languages[eb_lang]);
	set_property_string_0(cx, nav, "appVersion", version);
	set_property_string_0(cx, nav, "vendorSub", version);
	set_property_string_0(cx, nav, "userAgent", currentAgent);
	uname(&ubuf);
	set_property_string_0(cx, nav, "oscpu", ubuf.sysname);
	set_property_string_0(cx, nav, "platform", ubuf.machine);

/* Build the array of mime types and plugins,
 * according to the entries in the config file. */
	navpi = get_property_object_0(cx, nav, "plugins");
	navmt = get_property_object_0(cx, nav, "mimeTypes");
	if (navpi == NULL || navmt == NULL)
		return;
	mt = mimetypes;
	for (i = 0; i < maxMime; ++i, ++mt) {
		int len;
/* po is the plugin object and mo is the mime object */
		jsobjtype po = instantiate_array_element_0(cx, navpi, i, 0);
		jsobjtype mo = instantiate_array_element_0(cx, navmt, i, 0);
		if (po == NULL || mo == NULL)
			return;
		set_property_object_0(cx, mo, "enabledPlugin", po);
		set_property_string_0(cx, mo, "type", mt->type);
		set_property_object_0(cx, navmt, mt->type, mo);
		set_property_string_0(cx, mo, "description", mt->desc);
		set_property_string_0(cx, mo, "suffixes", mt->suffix);
/* I don't really have enough information from the config file to fill
 * in the attributes of the plugin object.
 * I'm just going to fake it.
 * Description will be the same as that of the mime type,
 * and the filename will be the program to run.
 * No idea if this is right or not. */
		set_property_string_0(cx, po, "description", mt->desc);
		set_property_string_0(cx, po, "filename", mt->program);
/* For the name, how about the program without its options? */
		len = strcspn(mt->program, " \t");
		save_c = mt->program[len];
		mt->program[len] = 0;
		set_property_string_0(cx, po, "name", mt->program);
		mt->program[len] = save_c;
	}

	hist = get_property_object_0(cx, w, "history");
	if (hist == NULL)
		return;
	set_property_string_0(cx, hist, "current", cf->fileName);

	set_property_string_0(cx, d, "referrer", cw->referrer);
	set_property_string_0(cx, d, "URL", cf->fileName);
	set_property_string_0(cx, d, "location", cf->fileName);
	set_property_string_0(cx, w, "location", cf->fileName);
	jsRunScript(cf, w,
		    "window.location.replace = document.location.replace = function(s) { this.href = s; };Object.defineProperty(window.location,'replace',{enumerable:false});Object.defineProperty(document.location,'replace',{enumerable:false});",
		    "locreplace", 1);
	set_property_string_0(cx, d, "domain", getHostURL(cf->fileName));
	if (debugClone)
		set_property_bool_0(cx, w, "cloneDebug", true);
	if (debugEvent)
		set_property_bool_0(cx, w, "eventDebug", true);
	if (debugThrow)
		set_property_bool_0(cx, w, "throwDebug", true);
}				/* setupJavaDom */

/* Get the url from a url object, special wrapper.
 * Owner object is passed, look for obj.href, obj.src, or obj.action.
 * Return that if it's a string, or its member href if it is a url.
 * The result, coming from get_property_string, is allocated. */
char *get_property_url(const Frame *f, jsobjtype owner, bool action)
{
	enum ej_proptype mtype;	/* member type */
	jsobjtype uo = 0;	/* url object */
	if (action) {
		mtype = typeof_property(f, owner, "action");
		if (mtype == EJ_PROP_STRING)
			return get_property_string(f, owner, "action");
		if (mtype != EJ_PROP_OBJECT)
			return 0;
		uo = get_property_object(f, owner, "action");
	} else {
		mtype = typeof_property(f, owner, "href");
		if (mtype == EJ_PROP_STRING)
			return get_property_string(f, owner, "href");
		if (mtype == EJ_PROP_OBJECT)
			uo = get_property_object(f, owner, "href");
		else if (mtype)
			return 0;
		if (!uo) {
			mtype = typeof_property(f, owner, "src");
			if (mtype == EJ_PROP_STRING)
				return get_property_string(f, owner, "src");
			if (mtype == EJ_PROP_OBJECT)
				uo = get_property_object(f, owner, "src");
		}
	}

	if (uo == NULL)
		return 0;
/* should this be href$val? */
	return get_property_string(f, uo, "href");
}				/* get_property_url */

/*********************************************************************
See if a tag object is still rooted on the js side.
If not, it could be garbage collected away. We could be accessing a bad pointer.
Worse, it could be reallocated to a new object,
so we're not even accessing the object we think we are.
Imagine a timer fires and js rearranges the entire tree, but we haven't
rerendered yet. You type g on a link.
That line isn't even there, the tag is obsolete, its pointer is obsolete.
Check for that here in the only way I think is safe, from the top.
However, if objects are rooted in some way,
so that they can't go away without being released,
then there is an easier way. Start with the given node and climb up
through parentNode, looking for a global object.
*********************************************************************/

bool tagIsRooted(Tag *t)
{
Tag *u, *v = 0, *w;

	for(u = t; u; v = u, u = u->parent) {
		u->lic = -1;
		if(!v)
			continue;
		for(w = u->firstchild; w; w = w->sibling) {
			++u->lic;
			if(w == v)
				break;
		}
		if(!w) // this should never happen!
			goto fail;
// lic is the count of the child in the chain
	}
	u = v;

/*********************************************************************
We're at the top. Should be html.
There's no other <html> tag, even if the page has subframes,
so this should be a rock solid test.
*********************************************************************/

	if(u->action != TAGACT_HTML)
		goto fail;

// Now climb down the chain from u to t.
// I don't know why we would ever click on or even examine a tag under <head>,
// but I guess I'll allow for the possibility.
	if(u->lic == 0) // head
		u = u->firstchild;
	else if(u->lic == 1) // body
		u = u->firstchild->sibling;
	else // should never happen
		goto fail;

	while(true) {
		int i, len;
		jsobjtype cn; // child nodes
// Imagine removing an object from the tree, allocating a new one, and by sheer
// bad luck, the new object gets the same pointer. Then put it back in the
// same place in the tree. I've seen it happen.
// Use our sseqno to defend against this.
		if(get_property_number_0(u->f0->cx, u->jv, "eb$seqno") != u->seqno)
			goto fail;
		if(u == t)
			break;
		i = 0;
		v = u->firstchild;
		while(++i <= u->lic)
			v = v->sibling;
		if(!v->jv)
			goto fail;
// find v->jv in the children of u.
		if(!(cn = get_property_object_0(u->f0->cx, u->jv, "childNodes")))
			goto fail;
		len = get_arraylength_0(u->f0->cx, cn);
		for(i = 0; i < len; ++i)
			if(get_array_element_object_0(u->f0->cx, cn, i) == v->jv) // found it
				break;
		if(i == len)
			goto fail; // not found
		u = v;
	}

	debugPrint(4, "%s %d is rooted", t->info->name, t->seqno);
	return true; // properly rooted

fail:
	debugPrint(3, "%s %d is not rooted", t->info->name, t->seqno);
	return false;
}

/*********************************************************************
Javascript sometimes builds or rebuilds a submenu, based upon your selection
in a primary menu. These new options must map back to html tags,
and then to the dropdown list as you interact with the form.
This is tested in jsrt - select a state,
whereupon the colors below, that you have to choose from, can change.
This does not easily fold into rerender(),
since the line <> in the buffer looks exactly the same,
so this tells you the options underneath have changed.
*********************************************************************/

static void rebuildSelector(Tag *sel, jsobjtype oa, int len2)
{
	int i2 = 0;
	bool check2;
	char *s;
	const char *selname;
	bool changed = false;
	Tag *t, *t0 = 0;
	jsobjtype oo;		/* option object */
	jsobjtype cx = sel->f0->cx;

	selname = sel->name;
	if (!selname)
		selname = "?";
	debugPrint(4, "testing selector %s %d", selname, len2);
	sel->lic = (sel->multiple ? 0 : -1);
	t = cw->optlist;

	while (t && i2 < len2) {
		t0 = t;
/* there is more to both lists */
		if (t->controller != sel) {
			t = t->same;
			continue;
		}

/* find the corresponding option object */
		if ((oo = get_array_element_object_0(cx, oa, i2)) == NULL) {
/* Wow this shouldn't happen. */
/* Guess I'll just pretend the array stops here. */
			len2 = i2;
			break;
		}

		if (t->jv != oo) {
			debugPrint(5, "oo switch");
/*********************************************************************
Ok, we freed up the old options, and garbage collection
could well kill the tags that went with these options,
i.e. the tags we're looking at now.
I'm bringing the tags back to life.
*********************************************************************/
			t->dead = false;
			disconnectTagObject(t);
			connectTagObject(t, oo);
		}

		t->rchecked = get_property_bool_0(cx, oo, "defaultSelected");
		check2 = get_property_bool_0(cx, oo, "selected");
		if (check2) {
			if (sel->multiple)
				++sel->lic;
			else
				sel->lic = i2;
		}
		++i2;
		if (t->checked != check2)
			changed = true;
		t->checked = check2;
		s = get_property_string_0(cx, oo, "text");
		if ((s && !t->textval) || !stringEqual(t->textval, s)) {
			nzFree(t->textval);
			t->textval = s;
			changed = true;
		} else
			nzFree(s);
		s = get_property_string_0(cx, oo, "value");
		if ((s && !t->value) || !stringEqual(t->value, s)) {
			nzFree(t->value);
			t->value = s;
		} else
			nzFree(s);
		t = t->same;
	}

/* one list or the other or both has run to the end */
	if (i2 == len2) {
		for (; t; t = t->same) {
			if (t->controller != sel) {
				t0 = t;
				continue;
			}
/* option is gone in js, disconnect this option tag from its select */
			disconnectTagObject(t);
			t->controller = 0;
			t->action = TAGACT_NOP;
			if (t0)
				t0->same = t->same;
			else
				cw->optlist = t->same;
			changed = true;
		}
	} else if (!t) {
		for (; i2 < len2; ++i2) {
			if ((oo = get_array_element_object_0(cx, oa, i2)) == NULL)
				break;
			t = newTag(sel->f0, "option");
			t->lic = i2;
			t->controller = sel;
			connectTagObject(t, oo);
			t->step = 2;	// already decorated
			t->textval = get_property_string_0(cx, oo, "text");
			t->value = get_property_string_0(cx, oo, "value");
			t->checked = get_property_bool_0(cx, oo, "selected");
			if (t->checked) {
				if (sel->multiple)
					++sel->lic;
				else
					sel->lic = i2;
			}
			t->rchecked = get_property_bool_0(cx, oo, "defaultSelected");
			changed = true;
		}
	}

	if (!changed)
		return;
	debugPrint(4, "selector %s has changed", selname);

	s = displayOptions(sel);
	if (!s)
		s = emptyString;
	javaSetsTagVar(sel->jv, s);
	nzFree(s);

	if (!sel->multiple)
		set_property_number_0(cx, sel->jv, "selectedIndex", sel->lic);
}				/* rebuildSelector */

void rebuildSelectors(void)
{
	int i1;
	Tag *t;
	jsobjtype oa;		/* option array */
	int len;		/* length of option array */

	if (!isJSAlive)
		return;

	for (i1 = 0; i1 < cw->numTags; ++i1) {
		t = tagList[i1];
		if (!t->jv)
			continue;
		if (t->action != TAGACT_INPUT)
			continue;
		if (t->itype != INP_SELECT)
			continue;
		if(!tagIsRooted(t))
			continue;

/* there should always be an options array, if not then move on */
		if ((oa = get_property_object_0(t->f0->cx, t->jv, "options")) == NULL)
			continue;
		if ((len = get_arraylength_0(t->f0->cx, oa)) < 0)
			continue;
		rebuildSelector(t, oa, len);
	}

}				/* rebuildSelectors */
