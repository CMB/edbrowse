/*********************************************************************
This is the back-end process for javascript.
This is the server, and edbrowse is the client.
We receive interprocess messages from edbrowse,
getting and setting properties for various DOM objects.

This is the duktape version.
If you package this with the mozilla js libraries,
you will need to include the MIT open source license,
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

#include <malloc.h>

#ifdef DOSLIKE
#include "vsprtf.h"
#endif // DOSLIKE

#include <duktape.h>

static void usage(void)
{
	fprintf(stderr, "Usage:  edbrowse-js pipe_in pipe_out\n");
	exit(1);
}				/* usage */

/* arguments, as indicated by the above */
static int pipe_in, pipe_out;

/* Here is an instance of the edbrowse window that exists for parsing
 * html from inside javascript. It belongs to edbrowse-js,
 * and isn't associated with a particular buffer on the edbrowse side. */
static struct ebWindow in_js_cw;

static void readMessage(void);
void processMessage1(void);
static void processMessage2(void);
static void createContext(void);
static void writeHeader(void);

static duk_context *jcx;
static jsobjtype winobj;	/* window object */
static jsobjtype docobj;	/* document object */

static struct ebWindow *save_cw;
static struct ebFrame *save_cf;
static bool save_ref, save_plug;

static void cwSetup(void)
{
	if (js1) {
		save_cw = cw;
		save_cf = cf;
		cw = &in_js_cw;
		cf = &(cw->f0);
		cf->owner = cw;
	}
	cf->winobj = winobj;
	cf->docobj = docobj;
	cf->hbase = get_property_string_nat(winobj, "eb$base");
	cf->baseset = true;
}				/* cwSetup */

static void cwBringdown(void)
{
	freeTags(cw);
	nzFree(cw->ft);		/* title could have been set by prerender */
	cw->ft = 0;
	nzFree(cf->hbase);
	cf->hbase = 0;
	if (js1)
		cw = save_cw, cf = save_cf;
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
static duk_context *context0;

/* wrappers around duktape alloc functions: add our own header */
struct jsdata_wrap {
	uint64_t header;
	char data[0];
};
#define container_of(addr, type, member) ({                     \
        const typeof(((type *) 0)->member) * __mptr = (addr);   \
        (type *)((char *) __mptr - offsetof(type, member)); })

static void *watch_malloc(void *udata, size_t n)
{
	struct jsdata_wrap *w = malloc(n + sizeof(struct jsdata_wrap));

	if (!w)
		return NULL;

	w->header = 0;
	return w->data;
}

static void *watch_realloc(void *udata, void *p, size_t n)
{
	struct jsdata_wrap *w;

	if (!p)
		return watch_malloc(udata, n);

	w = container_of(p, struct jsdata_wrap, data);

	if (w->header != 0)
		debugPrint(1,
			   "realloc with a watched pointer, shouldn't happen");

	w = realloc(w, n + sizeof(struct jsdata_wrap));
	return w->data;
}

static void watch_free(void *udata, void *p)
{
	int i;
	char e[40];
	struct jsdata_wrap *w;

	if (!p)
		return;

	w = container_of(p, struct jsdata_wrap, data);
	i = w->header;
	free(w);
	if (!i)
		return;

	sprintf(e, "g{%p", p);
	effectString(e);
	endeffect();
	if (js1) {
		effects[eff_l - 1] = 0;
		debugPrint(4, "%s", effects);
		garbageSweep1(p);
		nzFree(effects);
		effects = initString(&eff_l);
	}
}

// Wrapper around get_heapptr()
static void *watch_heapptr(int idx)
{
	void *p = duk_get_heapptr(jcx, idx);
// p could be null if the entity on the stack is not an object.
	if (p) {
		struct jsdata_wrap *w =
		    container_of(p, struct jsdata_wrap, data);
		w->header = 1;
	}
	return p;
}

int js_main(int argc, char **argv)
{
	if (!js1) {
		if (argc != 2)
			usage();
		pipe_in = stringIsNum(argv[0]);
		pipe_out = stringIsNum(argv[1]);
		if (pipe_in < 0 || pipe_out < 0)
			usage();
	}

	effects = initString(&eff_l);

	context0 =
	    duk_create_heap(watch_malloc, watch_realloc, watch_free, 0, 0);
	if (!context0) {
		fprintf(stderr,
			"Cannot create javascript runtime environment\n");
		if (!js1) {
/* send a message to edbrowse, so it can disable javascript,
 * so we don't get this same error on every browse. */
			head.highstat = EJ_HIGH_PROC_FAIL;
			head.lowstat = EJ_LOW_RUNTIME;
			writeHeader();
			exit(4);
		} else {
			return 4;
		}
	}

	if (js1)
		return 0;

	cw = &in_js_cw;
	cf = &(cw->f0);
	cf->owner = cw;

	readConfigFile();
	setupEdbrowseCache();
	eb_curl_global_init();
	cookiesFromJar();
	pluginsOn = false;
	sendReferrer = false;

/* edbrowse catches interrupt, this process ignores it. */
/* Use quit to terminate, or kill from another console. */
/* If edbrowse quits then this process also quits via broken pipe. */
	signal(SIGINT, SIG_IGN);

	while (true)
		processMessage1();
}				/* js_main */

/* read from and write to edbrowse */

static void readFromEb(void *data_p, int n)
{
	int rc;
	unsigned char *bytes_p = (unsigned char *)data_p;
	if (n == 0)
		return;
	if (js1) {
		memcpy(data_p, ipm_c, n);
		ipm_c += n;
		return;
	}
	while (n > 0) {
		rc = read(pipe_in, bytes_p, n);
		if (rc <= 0) {
/* Oops - can't read from the process any more */
			exit(2);
		}
		n -= rc;
		bytes_p += rc;
	}
}				/* readFromEb */

static void writeToEb(const void *data_p, int n)
{
	int rc;
	if (n == 0)
		return;
	if (js1) {
		stringAndBytes(&ipm, &ipm_l, data_p, n);
		return;
	}
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

	if (js1)
		ipm = initString(&ipm_l);
	writeToEb(&head, sizeof(head));

// send out the error message and side effects, if present.
// Edbrowse will expect these before any returned values.
// If one process, then there will be no side effects, since they happen
// in real time. In other words, the effects string is empty.
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

/* this is allocated */
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

	if (js1)
		ipm_c = ipm;
	readFromEb(&head, sizeof(head));

	if (head.magic != EJ_MAGIC) {
		fprintf(stderr,
			"Messages between js and edbrowse are out of sync\n");
		exit(3);
	}

	cmd = head.cmd;

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
	    cmd == EJ_CMD_CALL || cmd == EJ_CMD_VARUPDATE) {
		proptype = head.proptype;
		if (head.proplength)
			propval = readString(head.proplength);
	}

/* and that's the whole message */
	if (js1) {
		nzFree(ipm);
		ipm = 0;
	}
}				/* readMessage */

#if 0
static void misconfigure(int n)
{
/* there may already be a larger error */
	if (head.highstat > EJ_HIGH_CX_FAIL)
		return;
	head.highstat = EJ_HIGH_CX_FAIL;
	head.lowstat = EJ_LOW_VARS;
	head.lineno = n;
}				/* misconfigure */
#endif

static duk_ret_t native_new_location(duk_context * cx)
{
	const char *s = duk_to_string(cx, -1);
	if (s && *s) {
		effectString("n{");	// }
		effectString(s);
		endeffect();
		if (js1) {
			char *t;
			effects[eff_l - 1] = 0;
			debugPrint(4, "%s", effects);
			effects[eff_l - 5] = 0;
/* url on one line, name of window on next line */
			t = strchr(effects, '\n');
			*t = 0;
			javaOpensWindow(effects + 2, t + 1);
			nzFree(effects);
			effects = initString(&eff_l);
		}
	}
	return 0;
}

static duk_ret_t native_puts(duk_context * cx)
{
	printf("%s\n", duk_to_string(cx, -1));
	return 0;
}

static duk_ret_t native_logputs(duk_context * cx)
{
	int minlev = duk_get_int(cx, 0);
	const char *s = duk_to_string(cx, 1);
	duk_remove(cx, 0);
	if (debugLevel >= minlev && s && *s)
		puts(s);
	return 0;
}

static duk_ret_t native_prompt(duk_context * cx)
{
	const char *msg = 0;
	const char *answer = 0;
	int top = duk_get_top(cx);
	char inbuf[80];
	if (top > 0) {
		msg = duk_to_string(cx, 0);
		if (top > 1)
			answer = duk_to_string(cx, 1);
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
	const char *msg = duk_to_string(cx, 0);
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

static jsobjtype string2pointer(const char *s)
{
	jsobjtype p;
	sscanf(s, "%p", &p);
	return p;
}				/* string2pointer */

static duk_ret_t getter_innerHTML(duk_context * cx)
{
	duk_push_this(cx);
	duk_get_prop_string(cx, -1, "inner$HTML");
	duk_remove(cx, -2);
	return 1;
}

static duk_ret_t setter_innerHTML(duk_context * cx)
{
	jsobjtype thisobj;
	char *run;
	int run_l;
	const char *h = duk_to_string(cx, -1);
	if (!h)			// should never happen
		h = emptyString;
	debugPrint(5, "setter h 1");
	duk_push_this(cx);
// remove the preexisting children.
	if (duk_get_prop_string(cx, -1, "childNodes") && duk_is_array(cx, -1)) {
#if 0
		int l = duk_get_length(cx, -1);
// More efficient to remove them last to first.
		while (l--) {
			duk_get_prop_index(cx, -1, l);
			native_removeChild(cx);
		}
#else
		duk_set_length(cx, -1, 0);
#endif
	} else {
// no child nodes array, don't do anything.
// This should never happen.
		duk_pop_n(cx, 3);
		debugPrint(5, "setter h 3");
		return 0;
	}
	duk_pop(cx);
// stack now holds html and this
	duk_insert(cx, -2);
	duk_put_prop_string(cx, -2, "inner$HTML");

	thisobj = watch_heapptr(-1);
	duk_pop(cx);

// Put some tags around the html, so tidy can parse it.
	run = initString(&run_l);
	stringAndString(&run, &run_l, "<!DOCTYPE public><body>\n");
	stringAndString(&run, &run_l, h);
	if (*h && h[strlen(h) - 1] != '\n')
		stringAndChar(&run, &run_l, '\n');
	stringAndString(&run, &run_l, "</body>");

// now turn the html into objects
	cwSetup();
	html_from_setter(thisobj, run);

	effectString("i{h");	// }
	effectString(pointer2string(thisobj));
	effectChar('|');
	effectString(run);
	effectChar('@');
	packDecoration();
	cwBringdown();
	endeffect();
	nzFree(run);

	if (js1) {
		effects[eff_l - 1] = 0;
		debugPrint(4, "%s", effects);
		effects[eff_l - 5] = 0;
		char *t = strchr(effects, '|') + 1;
		javaSetsInner(thisobj, t, 'h');
		nzFree(effects);
		effects = initString(&eff_l);
	}

	debugPrint(5, "setter h 2");
	return 0;
}

static duk_ret_t getter_innerText(duk_context * cx)
{
	duk_push_this(cx);
	duk_get_prop_string(cx, -1, "inner$Text");
	duk_remove(cx, -2);
	return 1;
}

static duk_ret_t setter_innerText(duk_context * cx)
{
	jsobjtype thisobj;
	const char *h = duk_to_string(cx, -1);
	if (!h)			// should never happen
		h = emptyString;
	debugPrint(5, "setter t 1");
	duk_push_this(cx);
	duk_insert(cx, -2);
	duk_put_prop_string(cx, -2, "inner$Text");
	thisobj = watch_heapptr(-1);
	duk_pop(cx);
	effectString("i{t");	// }
	effectString(pointer2string(thisobj));
	effectChar('|');
	effectString(h);
	endeffect();
	if (js1) {
		effects[eff_l - 1] = 0;
		debugPrint(4, "%s", effects);
		effects[eff_l - 5] = 0;
		char *t = strchr(effects, '|') + 1;
		javaSetsInner(thisobj, t, 't');
		nzFree(effects);
		effects = initString(&eff_l);
	}
	debugPrint(5, "setter t 2");
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
	const char *h = duk_to_string(cx, -1);
	if (!h)			// should never happen
		h = emptyString;
	debugPrint(5, "setter v 1");
	duk_push_this(cx);
	duk_insert(cx, -2);
	duk_put_prop_string(cx, -2, "val$ue");
	thisobj = watch_heapptr(-1);
	duk_pop(cx);
	effectString("v{");	// }
	effectString(pointer2string(thisobj));
	effectChar('=');
	effectString(h);
	endeffect();
	if (js1) {
		char *t = strchr(effects, '=');
		effects[eff_l - 1] = 0;
		debugPrint(4, "%s", effects);
		effects[eff_l - 5] = 0;
		*t++ = 0;
		prepareForField(t);
		javaSetsTagVar(thisobj, t);
		nzFree(effects);
		effects = initString(&eff_l);
	}
	debugPrint(5, "setter v 2");
	return 0;
}

static void linkageNow(char linkmode, jsobjtype o)
{
	effects[eff_l - 1] = 0;
	debugPrint(4, "%s", effects);
	effects[eff_l - 5] = 0;
	javaSetsLinkage(false, linkmode, o, strchr(effects, ',') + 1);
	nzFree(effects);
	effects = initString(&eff_l);
}

static duk_ret_t native_log_element(duk_context * cx)
{
	jsobjtype newobj = watch_heapptr(-2);
	const char *tag = duk_get_string(cx, -1);
	char e[60];
	debugPrint(5, "log el 1");
// pass the newly created node over to edbrowse
	sprintf(e, "l{c|%s,%s 0x0, 0x0, ", pointer2string(newobj), tag);
	effectString(e);
	endeffect();
	if (js1)
		linkageNow('c', newobj);
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
	int top = duk_get_top(cx);
	int n = 1000;		/* default number of milliseconds */
	char nstring[20];
	char fname[48];		/* function name */
	const char *fstr;	/* function string */
	const char *s;

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
// We use to extract the function name in moz js, don't know how to do it here.
		strcpy(fname, "javascript()");
	} else if (duk_is_string(cx, 0)) {
// pull the function name out of the string, if that makes sense.
		fstr = duk_get_string(cx, 0);
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
		duk_pcompile(cx, 0);
// Now looks like a function object, just like the previous case.
	} else {
// oops, not a function or a string.
		duk_pop(cx);
		return;
	}

	duk_push_global_object(cx);
	duk_push_string(cx, fakePropName());
// Create a timer object.
	duk_get_global_string(cx, "Timer");
	duk_new(cx, 0);
// stack now has function global fakePropertyName timer-object.
	to = watch_heapptr(-1);
// protect this timer from the garbage collector.
	duk_def_prop(cx, 1,
		     (DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_SET_ENUMERABLE |
		      DUK_DEFPROP_CLEAR_WRITABLE |
		      DUK_DEFPROP_SET_CONFIGURABLE));
	duk_pop(cx);		// don't need global any more

// function is onclickable
	duk_push_heapptr(cx, to);
	duk_insert(cx, 0);	// switch places
// now stack is timer_object function
	duk_push_string(cx, "onclick");
	duk_insert(cx, 1);
	duk_def_prop(cx, 0,
		     (DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_CLEAR_ENUMERABLE |
		      DUK_DEFPROP_SET_WRITABLE |
		      DUK_DEFPROP_CLEAR_CONFIGURABLE));
// leaves just the timer object on the stack, which is what we want.

	sprintf(nstring, "t{%d|", n);	// }
	effectString(nstring);
	effectString(fname);
	effectChar('|');
	effectString(pointer2string(to));
	effectChar('|');
	effectChar((isInterval ? '1' : '0'));
	endeffect();

	if (js1) {
		struct inputChange *ic;
		effects[eff_l - 1] = 0;
		debugPrint(4, "%s", effects);
		ic = allocMem(sizeof(struct inputChange) + strlen(fname));
		ic->major = 't';
		ic->minor = (isInterval ? '1' : '0');
		ic->f0 = cf;
// Yeah I know, this isn't a pointer to htmlTag.
		ic->t = to;
		ic->tagno = n;
		strcpy(ic->value, fname);
		addToListBack(&inputChangesPending, ic);
		nzFree(effects);
		effects = initString(&eff_l);
	}

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
	jsobjtype obj = watch_heapptr(0);
	char nstring[60];
	sprintf(nstring, "t{0|-|%s|0", pointer2string(obj));	// }
	effectString(nstring);
	endeffect();

	if (js1) {
		struct inputChange *ic;
		effects[eff_l - 1] = 0;
		debugPrint(4, "%s", effects);
		ic = allocMem(sizeof(struct inputChange));
		ic->major = 't';
		ic->minor = '0';
		ic->f0 = cf;
		ic->t = obj;
		ic->tagno = 0;
		strcpy(ic->value, "-");
		addToListBack(&inputChangesPending, ic);
		nzFree(effects);
		effects = initString(&eff_l);
	}
// We should unlink this from window, so gc can clean it up.
// We'd have to save the fakePropName to do that.
	return 0;
}

static duk_ret_t native_win_close(duk_context * cx)
{
	if (head.highstat <= EJ_HIGH_CX_FAIL) {
		head.highstat = EJ_HIGH_CX_FAIL;
		head.lowstat = EJ_LOW_CLOSE;
	}
	return 0;
}

static void dwrite(duk_context * cx, bool newline)
{
	int top = duk_get_top(cx);
	const char *s;
	if (top) {
		duk_push_string(cx, emptyString);
		duk_insert(cx, 0);
		duk_join(cx, top);
	} else {
		duk_push_string(cx, emptyString);
	}
	s = duk_get_string(cx, 0);
	effectString("w{");	// }
	effectString(s);
	if (newline)
		effectChar('\n');
	endeffect();
	if (js1) {
		effects[eff_l - 1] = 0;
		debugPrint(4, "%s", effects);
		effects[eff_l - 5] = 0;
		dwStart();
		stringAndString(&cf->dw, &cf->dw_l, effects + 2);
		nzFree(effects);
		effects = initString(&eff_l);
	}
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
static const char *embedNodeName(jsobjtype obj)
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

	duk_push_heapptr(jcx, obj);
	if (duk_get_prop_string(jcx, -1, "nodeName"))
		nodeName = duk_get_string(jcx, -1);
	if (nodeName) {
		length = strlen(nodeName);
		if (length >= MAXTAGNAME)
			length = MAXTAGNAME - 1;
		strncpy(b, nodeName, length);
		b[length] = 0;
	}
	duk_pop_2(jcx);
	return b;
}				/* embedNodeName */

static void append0(duk_context * cx, bool side)
{
	unsigned i, length;
	jsobjtype child, thisobj;
	char e[40];
	const char *thisname, *childname;

/* we need one argument that is an object */
	if (duk_get_top(cx) != 1 || !duk_is_object(cx, 0))
		return;

	debugPrint(5, "append 1");
	child = watch_heapptr(0);
	duk_push_this(cx);
	thisobj = watch_heapptr(-1);
	if (!duk_get_prop_string(cx, -1, "childNodes") || !duk_is_array(cx, -1)) {
		duk_pop_2(cx);
		goto done;
	}
	length = duk_get_length(cx, -1);
// see if it's already there.
	for (i = 0; i < length; ++i) {
		duk_get_prop_index(cx, -1, i);
		if (child == watch_heapptr(-1)) {
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
	thisname = embedNodeName(thisobj);
	childname = embedNodeName(child);
	sprintf(e, "l{a|%s,%s ", pointer2string(thisobj), thisname);
	effectString(e);
	effectString(pointer2string(child));
	effectChar(',');
	effectString(childname);
	effectString(" 0x0, ");
	endeffect();
	if (js1)
		linkageNow('a', thisobj);

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
	char e[40];
	const char *thisname, *childname, *itemname;

/* we need two objects */
	if (duk_get_top(cx) != 2 ||
	    !duk_is_object(cx, 0) || !duk_is_object(cx, 1))
		return 0;

	debugPrint(5, "before 1");
	child = watch_heapptr(0);
	item = watch_heapptr(1);
	duk_push_this(cx);
	thisobj = watch_heapptr(-1);
	duk_get_prop_string(cx, -1, "childNodes");
	if (!duk_is_array(cx, -1)) {
		duk_pop_n(cx, 3);
		goto done;
	}
	length = duk_get_length(cx, -1);
	mark = -1;
	for (i = 0; i < length; ++i) {
		duk_get_prop_index(cx, -1, i);
		h = watch_heapptr(-1);
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
	thisname = embedNodeName(thisobj);
	childname = embedNodeName(child);
	itemname = embedNodeName(item);
	sprintf(e, "l{b|%s,%s ", pointer2string(thisobj), thisname);
	effectString(e);
	effectString(pointer2string(child));
	effectChar(',');
	effectString(childname);
	effectChar(' ');
	effectString(pointer2string(item));
	effectChar(',');
	effectString(itemname);
	effectChar(' ');
	endeffect();
	if (js1)
		linkageNow('b', thisobj);

done:
	debugPrint(5, "before 2");
	return 1;
}

static duk_ret_t native_removeChild(duk_context * cx)
{
	unsigned i, length;
	int mark;
	jsobjtype child, thisobj, h;
	char e[40];
	const char *thisname, *childname;

	debugPrint(5, "remove 1");
// top of stack must be the object to remove.
	if (!duk_is_object(cx, -1))
		goto done;
	child = watch_heapptr(-1);
	duk_push_this(cx);
	thisobj = watch_heapptr(-1);
	duk_get_prop_string(cx, -1, "childNodes");
	if (!duk_is_array(cx, -1)) {
		duk_pop_2(cx);
		goto done;
	}
	length = duk_get_length(cx, -1);
	mark = -1;
	for (i = 0; i < length; ++i) {
		duk_get_prop_index(cx, -1, i);
		h = watch_heapptr(-1);
		if (h == child)
			mark = i;
		duk_pop(cx);
		if (mark >= 0)
			break;
	}

	if (mark < 0) {
		duk_pop_2(cx);
		goto done;
	}

/* push the other elements down */
	for (i = mark + 1; i < length; --i) {
		duk_get_prop_index(cx, -1, i);
		duk_put_prop_index(cx, -2, i - 1);
	}
	duk_set_length(cx, -1, length - 1);
	duk_pop_2(cx);
	duk_del_prop_string(cx, -1, "parentNode");

/* pass this linkage information back to edbrowse, to update its dom tree */
	thisname = embedNodeName(thisobj);
	childname = embedNodeName(child);
	sprintf(e, "l{r|%s,%s ", pointer2string(thisobj), thisname);
	effectString(e);
	effectString(pointer2string(child));
	effectChar(',');
	effectString(childname);
	effectString(" 0x0, ");
	endeffect();
	if (js1)
		linkageNow('r', thisobj);

done:
	duk_pop(cx);		// the argument
	debugPrint(5, "remove 2");
	return 0;
}

static duk_ret_t native_fetchHTTP(duk_context * cx)
{
	debugPrint(5, "xhr 1");
	if (allowXHR) {
		const char *incoming_url = duk_to_string(cx, 0);
		const char *incoming_method = duk_get_string(cx, 1);
//              const char *incoming_headers = duk_get_string(cx, 2);
		const char *incoming_payload = duk_get_string(cx, 3);
		char *outgoing_xhrheaders = NULL;
		char *outgoing_xhrbody = NULL;
		int responseLength = 0;
		char *a = NULL, methchar = '?';

		if (incoming_payload && *incoming_payload) {
			if (incoming_method
			    && stringEqualCI(incoming_method, "post"))
				methchar = '\1';
			if (asprintf(&a, "%s%c%s",
				     incoming_url, methchar,
				     incoming_payload) < 0)
				i_printfExit(MSG_MemAllocError, 50);
			incoming_url = a;
		}

		if (js1) {
			save_plug = pluginsOn;
			save_ref = sendReferrer;
		}
		httpConnect(incoming_url, false, false, true,
			    &outgoing_xhrheaders, &outgoing_xhrbody,
			    &responseLength);
		nzFree(a);
		if (js1) {
			pluginsOn = save_plug;
			sendReferrer = save_ref;
		}
		if (outgoing_xhrheaders == NULL)
			outgoing_xhrheaders = emptyString;
		if (outgoing_xhrbody == NULL)
			outgoing_xhrbody = emptyString;
		duk_pop_n(cx, 4);
		duk_push_string(cx, "");
		duk_push_string(cx, outgoing_xhrheaders);
		duk_push_string(cx, outgoing_xhrbody);
		duk_join(cx, 2);
		nzFree(outgoing_xhrheaders);
		nzFree(outgoing_xhrbody);
// http fetch could bring new cookies into the current window.
// Can I just call startCookie() again to refresh the cookie copy?
	} else {
		duk_pop_n(cx, 4);
		duk_push_string(cx, emptyString);
	}

	debugPrint(5, "xhr 2");
	return 1;
}

static duk_ret_t native_resolveURL(duk_context * cx)
{
	const char *base;
	const char *rel = duk_get_string(cx, 0);
	char *outgoing_url;

	duk_get_global_string(cx, "eb$base");
	base = duk_get_string(cx, -1);
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
	thisobj = watch_heapptr(-1);
	duk_pop(cx);
	effectString("f{s");	// }
	effectString(pointer2string(thisobj));
	endeffect();
	if (js1) {
		effects[eff_l - 1] = 0;
		debugPrint(4, "%s", effects);
		javaSubmitsForm(thisobj, false);
		nzFree(effects);
		effects = initString(&eff_l);
	}
	return 0;
}

static duk_ret_t native_formReset(duk_context * cx)
{
	jsobjtype thisobj;
	duk_push_this(cx);
	thisobj = watch_heapptr(-1);
	duk_pop(cx);
	effectString("f{r");	// }
	effectString(pointer2string(thisobj));
	endeffect();
	if (js1) {
		effects[eff_l - 1] = 0;
		debugPrint(4, "%s", effects);
		javaSubmitsForm(thisobj, true);
		nzFree(effects);
		effects = initString(&eff_l);
	}
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
	nzFree(cookieCopy);
	cookieCopy = initString(&cook_l);
	stringAndString(&cookieCopy, &cook_l, "; ");
	const char *url = cf->fileName;
	bool secure = false;
	const char *proto;
	char *s;

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
	if (!s || s == nc + 2) {
		nzFree(nc);
		return false;
	}

	duk_get_global_string(jcx, "eb$url");
	receiveCookie(duk_get_string(jcx, -1), newcook);
	duk_pop(jcx);
	if (!js1) {
// pass back to the edbrowse process
		effectString("c{");	// }
		effectString(newcook);
		endeffect();
	}

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
	if (cook_l == 2)	// empty
		stringAndString(&cookieCopy, &cook_l, nc + 2);
	else
		stringAndString(&cookieCopy, &cook_l, nc);
	nzFree(nc);
	return true;
}				/* foldinCookie */

static duk_ret_t native_getcook(duk_context * cx)
{
	duk_push_string(cx, cookieCopy + 2);
	return 1;
}

static duk_ret_t native_setcook(duk_context * cx)
{
	const char *newcook = duk_get_string(cx, 0);
	debugPrint(5, "cook 1");
	if (newcook) {
		foldinCookie(newcook);
	}
	debugPrint(5, "cook 2");
	return 0;
}

static void createContext(void)
{
	duk_push_thread_new_globalenv(context0);
	jcx = duk_get_context(context0, -1);
	if (!jcx) {
		head.highstat = EJ_HIGH_HEAP_FAIL;
		head.lowstat = EJ_LOW_CX;
		return;
	}
	debugPrint(3, "create js context %d", duk_get_top(context0) - 1);
// the global object, which will become window,
// and the document object.
	duk_push_global_object(jcx);
	winobj = watch_heapptr(0);
	duk_push_string(jcx, "document");
	duk_push_object(jcx);
	docobj = watch_heapptr(2);
	duk_def_prop(jcx, 0,
		     (DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_SET_ENUMERABLE |
		      DUK_DEFPROP_CLEAR_WRITABLE |
		      DUK_DEFPROP_CLEAR_CONFIGURABLE));
	duk_pop(jcx);

// bind native functions here
	duk_push_c_function(jcx, native_new_location, 1);
	duk_put_global_string(jcx, "eb$newLocation");
	duk_push_c_function(jcx, native_puts, 1);
	duk_put_global_string(jcx, "eb$puts");
	duk_push_c_function(jcx, native_logputs, 2);
	duk_put_global_string(jcx, "eb$logputs");
	duk_push_c_function(jcx, native_prompt, DUK_VARARGS);
	duk_put_global_string(jcx, "prompt");
	duk_push_c_function(jcx, native_confirm, 1);
	duk_put_global_string(jcx, "confirm");
	duk_push_c_function(jcx, native_log_element, 2);
	duk_put_global_string(jcx, "eb$logElement");
	duk_push_c_function(jcx, native_setTimeout, DUK_VARARGS);
	duk_put_global_string(jcx, "setTimeout");
	duk_push_c_function(jcx, native_setInterval, DUK_VARARGS);
	duk_put_global_string(jcx, "setInterval");
	duk_push_c_function(jcx, native_clearTimeout, 1);
	duk_put_global_string(jcx, "clearTimeout");
	duk_push_c_function(jcx, native_clearTimeout, 1);
	duk_put_global_string(jcx, "clearInterval");
	duk_push_c_function(jcx, native_win_close, 0);
	duk_put_global_string(jcx, "close");
	duk_push_c_function(jcx, native_fetchHTTP, 4);
	duk_put_global_string(jcx, "eb$fetchHTTP");
	duk_push_c_function(jcx, native_resolveURL, 1);
	duk_put_global_string(jcx, "eb$resolveURL");
	duk_push_c_function(jcx, native_formSubmit, 0);
	duk_put_global_string(jcx, "eb$formSubmit");
	duk_push_c_function(jcx, native_formReset, 0);
	duk_put_global_string(jcx, "eb$formReset");
	duk_push_c_function(jcx, native_getcook, 0);
	duk_put_global_string(jcx, "eb$getcook");
	duk_push_c_function(jcx, native_setcook, 1);
	duk_put_global_string(jcx, "eb$setcook");

	duk_push_heapptr(jcx, docobj);	// native document methods
	duk_push_c_function(jcx, native_doc_write, DUK_VARARGS);
	duk_put_prop_string(jcx, -2, "write");
	duk_push_c_function(jcx, native_doc_writeln, DUK_VARARGS);
	duk_put_prop_string(jcx, -2, "writeln");
	duk_push_c_function(jcx, native_apch1, 1);
	duk_put_prop_string(jcx, -2, "eb$apch1");
	duk_push_c_function(jcx, native_apch2, 1);
	duk_put_prop_string(jcx, -2, "eb$apch2");
	duk_push_c_function(jcx, native_insbf, 2);
	duk_put_prop_string(jcx, -2, "eb$insbf");
	duk_push_c_function(jcx, native_removeChild, 1);
	duk_put_prop_string(jcx, -2, "removeChild");
	duk_pop(jcx);

// Sequence is to set cf->fileName, then createContext(), so for a short time,
// we can rely on that variable.
// Let's make it more permanent, per context.
// Has to be nonwritable for security reasons.
	duk_push_global_object(jcx);
	duk_push_string(jcx, "eb$url");
	duk_push_string(jcx, cf->fileName);
	duk_def_prop(jcx, -3,
		     (DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_SET_ENUMERABLE |
		      DUK_DEFPROP_CLEAR_WRITABLE |
		      DUK_DEFPROP_CLEAR_CONFIGURABLE));
	duk_pop(jcx);

	startCookie();		// so document.cookie will work properly

// setupJavaDom() in ebjs.c does the rest.
}				/* createContext */

// determine the type of the element on the top of the stack.
static enum ej_proptype top_proptype(void)
{
	double d;
	int n;
	switch (duk_get_type(jcx, -1)) {
	case DUK_TYPE_NUMBER:
		d = duk_get_number(jcx, -1);
		n = d;
		return (n == d ? EJ_PROP_INT : EJ_PROP_FLOAT);
	case DUK_TYPE_STRING:
		return EJ_PROP_STRING;
	case DUK_TYPE_BOOLEAN:
		return EJ_PROP_BOOL;
	case DUK_TYPE_OBJECT:
		if (duk_is_function(jcx, -1))
			return EJ_PROP_FUNCTION;
		if (duk_is_array(jcx, -1))
			return EJ_PROP_ARRAY;
		return EJ_PROP_OBJECT;
	}
	return EJ_PROP_NONE;	/* don't know */
}				/* top_proptype */

enum ej_proptype has_property_nat(jsobjtype parent, const char *name)
{
	enum ej_proptype l;
	duk_push_heapptr(jcx, parent);
	duk_get_prop_string(jcx, -1, name);
	l = top_proptype();
	duk_pop_2(jcx);
	return l;
}

void delete_property_nat(jsobjtype parent, const char *name)
{
	duk_push_heapptr(jcx, parent);
	duk_del_prop_string(jcx, -1, name);
	duk_pop(jcx);
}				/* delete_property_nat */

int get_arraylength_nat(jsobjtype a)
{
	int l;
	duk_push_heapptr(jcx, a);
	if (duk_is_array(jcx, -1))
		l = duk_get_length(jcx, -1);
	else
		l = -1;
	duk_pop(jcx);
	return l;
}				/* get_arraylength_nat */

/* Return a property as a string, if it is
 * string compatible. The string is allocated, free it when done. */
char *get_property_string_nat(jsobjtype parent, const char *name)
{
	const char *s;
	char *s0;
	duk_push_heapptr(jcx, parent);
	duk_get_prop_string(jcx, -1, name);
	proptype = top_proptype();
	if (proptype == EJ_PROP_NONE) {
		duk_pop_2(jcx);
		return NULL;
	}
	if (duk_is_object(jcx, -1)) {
/* special code here to return the object pointer */
/* That's what edbrowse is going to want. */
		jsobjtype o = watch_heapptr(-1);
		s = pointer2string(o);
	} else
		s = duk_to_string(jcx, -1);
	s0 = cloneString(s);
	duk_pop_2(jcx);
	return s0;
}				/* get_property_string_nat */

jsobjtype get_property_object_nat(jsobjtype parent, const char *name)
{
	jsobjtype o = NULL;
	duk_push_heapptr(jcx, parent);
	duk_get_prop_string(jcx, -1, name);
	if (duk_is_object(jcx, -1))
		o = watch_heapptr(-1);
	duk_pop_2(jcx);
	return o;
}				/* get_property_object_nat */

jsobjtype get_property_function_nat(jsobjtype parent, const char *name)
{
	jsobjtype o = NULL;
	duk_push_heapptr(jcx, parent);
	duk_get_prop_string(jcx, -1, name);
	if (duk_is_function(jcx, -1))
		o = watch_heapptr(-1);
	duk_pop_2(jcx);
	return o;
}				/* get_property_function_nat */

int get_property_number_nat(jsobjtype parent, const char *name)
{
	int n = -1;
	duk_push_heapptr(jcx, parent);
	duk_get_prop_string(jcx, -1, name);
	if (duk_is_number(jcx, -1)) {
		double d = duk_get_number(jcx, -1);
		n = d;		// truncate
	}
	duk_pop_2(jcx);
	return n;
}				/* get_property_number_nat */

double get_property_float_nat(jsobjtype parent, const char *name)
{
	double d = -1;
	duk_push_heapptr(jcx, parent);
	duk_get_prop_string(jcx, -1, name);
	if (duk_is_number(jcx, -1))
		d = duk_get_number(jcx, -1);
	duk_pop_2(jcx);
	return d;
}				/* get_property_float_nat */

bool get_property_bool_nat(jsobjtype parent, const char *name)
{
	bool b = false;
	duk_push_heapptr(jcx, parent);
	duk_get_prop_string(jcx, -1, name);
	if (duk_is_number(jcx, -1)) {
		if (duk_get_number(jcx, -1))
			b = true;
	}
	if (duk_is_boolean(jcx, -1)) {
		if (duk_get_boolean(jcx, -1))
			b = true;
	}
	duk_pop_2(jcx);
	return b;
}				/* get_property_bool_nat */

int set_property_string_nat(jsobjtype parent, const char *name,
			    const char *value)
{
	bool defset = false;
	duk_c_function setter = NULL;
	duk_c_function getter = NULL;
	const char *altname;
	duk_push_heapptr(jcx, parent);
	if (stringEqual(name, "innerHTML"))
		setter = setter_innerHTML, getter = getter_innerHTML,
		    altname = "inner$HTML";
	if (stringEqual(name, "innerText"))
		setter = setter_innerText, getter = getter_innerText,
		    altname = "inner$Text";
	if (stringEqual(name, "value")) {
// This one is complicated. If option.value had side effects,
// that would only serve to confuse.
		bool valsetter = true;
		duk_get_global_string(jcx, "Option");
		if (duk_is_array(jcx, -2) || duk_instanceof(jcx, -2, -1))
			valsetter = false;
		duk_pop(jcx);
		if (valsetter)
			setter = setter_value,
			    getter = getter_value, altname = "val$ue";
	}
	if (setter) {
		if (!duk_get_prop_string(jcx, -1, name))
			defset = true;
		duk_pop(jcx);
	}
	if (defset) {
		duk_push_string(jcx, name);
		duk_push_c_function(jcx, getter, 0);
		duk_push_c_function(jcx, setter, 1);
		duk_def_prop(jcx, -4,
			     (DUK_DEFPROP_HAVE_SETTER | DUK_DEFPROP_HAVE_GETTER
			      | DUK_DEFPROP_SET_ENUMERABLE));
	}
	if (!value)
		value = emptyString;
	duk_push_string(jcx, value);
	duk_put_prop_string(jcx, -2, (setter ? altname : name));
	duk_pop(jcx);
	return 0;
}				/* set_property_string_nat */

int set_property_bool_nat(jsobjtype parent, const char *name, bool n)
{
	duk_push_heapptr(jcx, parent);
	duk_push_boolean(jcx, n);
	duk_put_prop_string(jcx, -2, name);
	duk_pop(jcx);
	return 0;
}				/* set_property_bool_nat */

int set_property_number_nat(jsobjtype parent, const char *name, int n)
{
	duk_push_heapptr(jcx, parent);
	duk_push_int(jcx, n);
	duk_put_prop_string(jcx, -2, name);
	duk_pop(jcx);
	return 0;
}				/* set_property_number_nat */

int set_property_float_nat(jsobjtype parent, const char *name, double n)
{
	duk_push_heapptr(jcx, parent);
	duk_push_number(jcx, n);
	duk_put_prop_string(jcx, -2, name);
	duk_pop(jcx);
	return 0;
}				/* set_property_float_nat */

int set_property_object_nat(jsobjtype parent, const char *name, jsobjtype child)
{
	duk_push_heapptr(jcx, parent);
	duk_push_heapptr(jcx, child);
	duk_put_prop_string(jcx, -2, name);
	duk_pop(jcx);
	return 0;
}				/* set_property_object_nat */

int set_property_function_nat(jsobjtype parent, const char *name,
			      const char *body)
{
	if (!body || !*body) {
// null or empty function, just return null.
		body = "null";
	}
	duk_push_string(jcx, body);
	duk_push_string(jcx, name);
	duk_pcompile(jcx, 0);
	duk_push_heapptr(jcx, parent);
	duk_insert(jcx, -2);	// switch places
	duk_put_prop_string(jcx, -2, name);
	duk_pop(jcx);
	return 0;
}

/*********************************************************************
Error object is at the top of the duktape stack.
Extract the line number, call stack, and error message,
the latter being error.toString().
Leave the result in errorMessage, which is automatically sent to edbrowse.
Pop the error object when done.
*********************************************************************/

static void processError(void)
{
	const char *callstack = emptyString;
	int offset = 0;
	char *cut, *s;
	if (duk_get_prop_string(jcx, -1, "lineNumber"))
		offset = duk_get_int(jcx, -1);
	duk_pop(jcx);
	if (duk_get_prop_string(jcx, -1, "stack"))
		callstack = duk_to_string(jcx, -1);
	nzFree(errorMessage);
	errorMessage = cloneString(duk_to_string(jcx, -2));
	if (strstr(errorMessage, "callstack") && strlen(callstack)) {
// this is rare.
		nzFree(errorMessage);
		errorMessage = cloneString(callstack);
	}
	if (offset) {
		head.lineno += (offset - 1);
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
	duk_pop(jcx);
}

// No arguments; returns abool.
bool run_function_bool_nat(jsobjtype parent, const char *name)
{
	duk_push_heapptr(jcx, parent);
	if (!duk_get_prop_string(jcx, -1, name) || !duk_is_function(jcx, -1)) {
#if 0
		if (!errorMessage)
			asprintf(&errorMessage, "no such function %s", name);
#endif
		duk_pop_2(jcx);
		return false;
	}
	duk_insert(jcx, -2);
	if (!duk_pcall_method(jcx, 0)) {
		bool rc = false;
		if (duk_is_boolean(jcx, -1))
			rc = duk_get_boolean(jcx, -1);
		duk_pop(jcx);
		return rc;
	}
// error in execution
	processError();
	return false;
}				/* run_function_bool_nat */

// The single argument to the function has to be an object.
void run_function_onearg_nat(jsobjtype parent, const char *name,
			     jsobjtype child)
{
	duk_push_heapptr(jcx, parent);
	if (!duk_get_prop_string(jcx, -1, name) || !duk_is_function(jcx, -1)) {
#if 0
		if (!errorMessage)
			asprintf(&errorMessage, "no such function %s", name);
#endif
		duk_pop_2(jcx);
		return;
	}
	duk_insert(jcx, -2);
	duk_push_heapptr(jcx, child);	// child is the only argument
	if (!duk_pcall_method(jcx, 1)) {
// Don't care about the return.
		duk_pop(jcx);
		return;
	}
// error in execution
	processError();
}				/* run_function_onearg_nat */

jsobjtype instantiate_array_nat(jsobjtype parent, const char *name)
{
	jsobjtype a;
	duk_push_heapptr(jcx, parent);
	if (duk_get_prop_string(jcx, -1, name) && duk_is_array(jcx, -1)) {
		a = watch_heapptr(-1);
		duk_pop_2(jcx);
		return a;
	}
	duk_pop(jcx);
	duk_get_global_string(jcx, "Array");
	duk_new(jcx, 0);
	a = watch_heapptr(-1);
	duk_put_prop_string(jcx, -2, name);
	duk_pop(jcx);
	return a;
}				/* instantiate_array_nat */

jsobjtype instantiate_nat(jsobjtype parent, const char *name,
			  const char *classname)
{
	jsobjtype a;
	duk_push_heapptr(jcx, parent);
	if (duk_get_prop_string(jcx, -1, name) && duk_is_object(jcx, -1)) {
// I'll assume the object is of the proper class.
		a = watch_heapptr(-1);
		duk_pop_2(jcx);
		return a;
	}
	duk_pop(jcx);
	if (!classname)
		classname = "Object";
	if (!duk_get_global_string(jcx, classname)) {
		fprintf(stderr, "unknown class %s, cannot instantiate\n",
			classname);
		exit(8);
	}
	duk_new(jcx, 0);
	a = watch_heapptr(-1);
	duk_put_prop_string(jcx, -2, name);
	duk_pop(jcx);
	return a;
}				/* instantiate_nat */

jsobjtype instantiate_array_element_nat(jsobjtype parent, int idx,
					const char *classname)
{
	jsobjtype a;
	if (!classname)
		classname = "Object";
	duk_push_heapptr(jcx, parent);
	duk_get_global_string(jcx, classname);
	duk_new(jcx, 0);
	a = watch_heapptr(-1);
	duk_put_prop_index(jcx, -2, idx);
	duk_pop(jcx);
	return a;
}

int set_array_element_object_nat(jsobjtype parent, int idx, jsobjtype child)
{
	duk_push_heapptr(jcx, parent);
	duk_push_heapptr(jcx, child);
	duk_put_prop_index(jcx, -2, idx);
	duk_pop(jcx);
	return 0;
}

jsobjtype get_array_element_object_nat(jsobjtype parent, int idx)
{
	jsobjtype a = 0;
	duk_push_heapptr(jcx, parent);
	duk_get_prop_index(jcx, -1, idx);
	if (duk_is_object(jcx, -1))
		a = watch_heapptr(-1);
	duk_pop_2(jcx);
	return a;
}

/* based on propval and proptype */
static void set_property_generic(jsobjtype parent, const char *name)
{
	int n;
	double d;
	jsobjtype child, newobj;

	switch (proptype) {
	case EJ_PROP_STRING:
		set_property_string_nat(parent, name, propval);
		break;

	case EJ_PROP_INT:
		n = atoi(propval);
		set_property_number_nat(parent, name, n);
		break;

	case EJ_PROP_BOOL:
		n = atoi(propval);
		set_property_bool_nat(parent, name, n);
		break;

	case EJ_PROP_FLOAT:
		d = atof(propval);
		set_property_float_nat(parent, name, d);
		break;

	case EJ_PROP_OBJECT:
		child = string2pointer(propval);
		set_property_object_nat(parent, name, child);
		break;

	case EJ_PROP_INSTANCE:
		newobj = instantiate_nat(parent, name, propval);
		nzFree(propval);
childreturn:
		propval = cloneString(pointer2string(newobj));
		break;

	case EJ_PROP_ARRAY:
		newobj = instantiate_array_nat(parent, name);
		goto childreturn;

	case EJ_PROP_FUNCTION:
		set_property_function_nat(parent, name, propval);
		break;

	default:
		fprintf(stderr, "Unexpected property type %d from edbrowse\n",
			proptype);
		exit(7);
	}
}				/* set_property_generic */

/*********************************************************************
run a javascript function and return the result.
If the result is an object then the pointer, as a string, is returned.
The string is always allocated, you must free it.
At entry, propval, if nonzero, is a | separated list of arguments
to the function, assuming all args are objects.
*********************************************************************/

static char *run_function(jsobjtype parent, const char *name)
{
	bool rc;
	int argc = 0;
	const char *s, *t;
	char *s0;
	jsobjtype o;

	proptype = EJ_PROP_NONE;
	duk_push_heapptr(jcx, parent);
	if (!duk_get_prop_string(jcx, -1, name) || !duk_is_function(jcx, -1)) {
		nzFree(propval);
		propval = 0;
#if 0
		if (!errorMessage)
			asprintf(&errorMessage, "no such function %s", name);
#endif
		duk_pop_2(jcx);
		return NULL;
	}
// switch function and parent so that parent becomes the this binding.
	duk_insert(jcx, -2);

	if (!propval)
		propval = emptyString;
	for (s = propval; *s; s = t) {
		t = strchr(s, '|') + 1;
		o = string2pointer(s);
		duk_push_heapptr(jcx, o);
		++argc;
	}
	nzFree(propval);
	propval = 0;

	rc = duk_pcall_method(jcx, argc);
	if (rc) {		// error during function execution
		processError();
		return 0;
	}

	proptype = top_proptype();
	if (proptype == EJ_PROP_NONE) {
		duk_pop(jcx);
		return NULL;
	}
	if (duk_is_object(jcx, -1)) {
		o = watch_heapptr(-1);
		s = pointer2string(o);
	} else {
		s = duk_to_string(jcx, -1);
	}
	s0 = cloneString(s);
	duk_pop(jcx);
	return s0;
}				/* run_function */

/* process each message from edbrowse and respond appropriately */
void processMessage1(void)
{
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
		return;
	}

	jcx = (duk_context *) head.jcx;
	winobj = head.winobj;
	docobj = head.docobj;

	if (head.cmd == EJ_CMD_DESTROY) {
		int i, top = duk_get_top(context0);
		for (i = 0; i < top; ++i) {
			if (jcx == duk_get_context(context0, i)) {
				duk_remove(context0, i);
				debugPrint(3, "remove js context %d", i);
				break;
			}
		}
		head.n = head.proplength = 0;
		writeHeader();
		return;
	}

	if (head.cmd == EJ_CMD_VARUPDATE) {
		switch (head.lineno) {
			char *t;
		case EJ_VARUPDATE_XHR:
			allowXHR = head.n;
			break;
		case EJ_VARUPDATE_DEBUG:
			debugLevel = head.n;
			break;
		case EJ_VARUPDATE_VERIFYCERT:
			verifyCertificates = head.n;
			break;
		case EJ_VARUPDATE_USERAGENT:
			t = userAgents[head.n];
			if (t)
				currentAgent = t;
			break;
		case EJ_VARUPDATE_CURLAUTHNEG:
			curlAuthNegotiate = head.n;
			break;
		case EJ_VARUPDATE_FILENAME:
			nzFree(cf->fileName);
			cf->fileName = propval;
			break;
		case EJ_VARUPDATE_DEBUGFILE:
			setDebugFile(propval);
			nzFree(propval);
			break;
		}

		head.n = head.proplength = 0;
		propval = 0;
//                      no acknowledgement needed
//                      writeHeader();
		return;
	}

	processMessage2();
}

static void processMessage2(void)
{
/* head.obj should be a valid object or 0 */
	jsobjtype parent = head.obj;
	jsobjtype child;
	const char *s;
	bool rc;		/* return code */
	bool setret;		/* does setting a property produce a return? */

	switch (head.cmd) {
	case EJ_CMD_SCRIPT:
		propval = 0;
		s = runscript;
/* skip past utf8 byte order mark if present */
		if (!strncmp(s, "\xef\xbb\xbf", 3))
			s += 3;
		head.n = 0;
		head.proplength = 0;
		proptype = EJ_PROP_NONE;
		rc = duk_peval_string(jcx, s);
		s = 0;
		if (!rc) {
			head.n = 1;
			s = duk_to_string(jcx, -1);
			if (s && !*s)
				s = 0;
			if (s)
				head.proplength = strlen(s);
		} else {
// error in executing the script.
			processError();
		}
		nzFree(runscript);
		runscript = 0;
		writeHeader();
		if (head.proplength)
			writeToEb(s, head.proplength);
		duk_pop(jcx);
		{
			const char *gc = getenv("JSGC");
			if (gc && *gc)
				duk_gc(jcx, 0);
		}
		break;

	case EJ_CMD_HASPROP:
		head.proptype = has_property_nat(parent, membername);
		nzFree(membername);
		membername = 0;
		head.n = head.proplength = 0;
		writeHeader();
		break;

	case EJ_CMD_DELPROP:
		delete_property_nat(parent, membername);
		nzFree(membername);
		membername = 0;
		head.n = head.proplength = 0;
		writeHeader();
		break;

	case EJ_CMD_GETPROP:
		propval = get_property_string_nat(parent, membername);
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
		set_property_generic(parent, membername);
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
		child = get_array_element_object_nat(parent, head.n);
		propval = 0;	/* should already be 0 */
		head.proplength = 0;
		if (child) {
			propval = cloneString(pointer2string(child));
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
			child = instantiate_array_element_nat(parent,
							      head.n, propval);
			nzFree(propval);
			propval = 0;
			setret = true;
			propval = cloneString(pointer2string(child));
		}
		if (head.proptype == EJ_PROP_OBJECT && propval) {
			child = string2pointer(propval);
			set_array_element_object_nat(parent, head.n, child);
			nzFree(propval);
			propval = 0;
		}
		goto propreturn;

	case EJ_CMD_ARLEN:
		head.n = get_arraylength_nat(parent);
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
}				/* processMessage2 */
