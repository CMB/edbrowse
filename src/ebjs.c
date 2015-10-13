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
	bool killed = false;

	for (cx = 1; cx < MAXSESSION; ++cx) {
		w = sessionList[cx].lw;
		if (!w)
			continue;
		if (w->winobj) {
			w->winobj = 0;
			w->docobj = 0;
			w->jcx = 0;
			killed = true;
		}
		while (w != sessionList[cx].fw) {
			w = w->prev;
			if (w->winobj) {
				w->winobj = 0;
				w->docobj = 0;
				w->jcx = 0;
				killed = true;
			}
		}
	}

	if (killed)
		i_puts(MSG_JSCloseSessions);
}				/* markAllDead */

/* communication pipes with the js process */
static int pipe_in[2], pipe_out[2];
static int js_pid;
static struct EJ_MSG head;

/* Start the js process. */
static void js_start(void)
{
#ifdef DOSLIKE
	debugPrint(5, "TODO: setting of communication channels for javascript");
#else // !DOSLIKE
	int pid;
	char arg1[8], arg2[8], arg3[8];

	if (js_pid)
		return;		/* already running */

/* doesn't hurt to do this more than once */
	signal(SIGPIPE, SIG_IGN);

	debugPrint(5, "setting of communication channels for javascript");

	if (pipe(pipe_in)) {
		i_puts(MSG_JSEnginePipe);
		allowJS = false;
		return;
	}

	if (pipe(pipe_out)) {
		i_puts(MSG_JSEnginePipe);
		allowJS = false;
		close(pipe_in[0]);
		close(pipe_in[1]);
		return;
	}

	pid = fork();
	if (pid < 0) {
		i_puts(MSG_JSEngineFork);
		allowJS = false;
		close(pipe_in[0]);
		close(pipe_in[1]);
		close(pipe_out[0]);
		close(pipe_out[1]);
		return;
	}

	if (pid) {		/* parent */
		js_pid = pid;
		close(pipe_in[1]);
		close(pipe_out[0]);
		return;
	}

/* child here, exec the back end js process */
	close(pipe_in[0]);
	close(pipe_out[1]);
	sprintf(arg1, "%d", pipe_out[0]);
	sprintf(arg2, "%d", pipe_in[1]);
	sprintf(arg3, "%d", jsPool);
	debugPrint(5, "spawning edbrowse-js %s %s %s", arg1, arg2, arg3);
	execlp("edbrowse-js", "edbrowse-js", arg1, arg2, arg3, 0);

/* oops, process did not exec */
/* write a message from this child, saying js would not exec */
/* The process just started; head is zero */
	head.magic = EJ_MAGIC;
	head.highstat = EJ_HIGH_PROC_FAIL;
	head.lowstat = EJ_LOW_EXEC;
	write(pipe_in[1], &head, sizeof(head));
	exit(90);
#endif // DOSLIKE y/n
}				/* js_start */

/* Shut down the js process, although if we got here,
 * it's probably dead anyways. */
static void js_kill(void)
{
	if (!js_pid)
		return;

#ifndef DOSLIKE
	close(pipe_in[0]);
	close(pipe_out[1]);
	kill(js_pid, SIGTERM);
	js_pid = 0;
#endif // #ifndef DOSLIKE

}				/* js_kill */

/* String description of side effects, as a result of running js code. */
static char *effects;
/* source file containing the js code */
static const char *jsSourceFile;
/* queue of edbrowse buffer changes produced by running js - see eb.h */
struct listHead inputChangesPending = {
	&inputChangesPending, &inputChangesPending
};

/* Javascript has changed an input field */
static void javaSetsTagVar(jsobjtype v, const char *newtext)
{
	struct inputChange *ic;
	struct htmlTag *t = tagFromJavaVar(v);
	if (!t)
		return;
	if (t->itype == INP_HIDDEN || t->itype == INP_RADIO)
		return;
	if (t->itype == INP_TA) {
		debugPrint(3, "textarea.value is being updated");
		return;
	}
	nzFree(t->value);
	t->value = cloneString(newtext);
}				/* javaSetsTagVar */

static void javaSetsInner(jsobjtype v, const char *newtext, char c)
{
	struct inputChange *ic;
	struct htmlTag *t = tagFromJavaVar(v);
	if (!t)
		return;
	ic = allocMem(sizeof(struct inputChange) + strlen(newtext));
	ic->t = t;
	ic->tagno = t->seqno;
	ic->major = 'i';
	ic->minor = c;
	strcpy(ic->value, newtext);
	addToListBack(&inputChangesPending, ic);
}				/* javaSetsInner */

/* start a document.write */
void dwStart(void)
{
	if (cw->dw)
		return;
	cw->dw = initString(&cw->dw_l);
	stringAndString(&cw->dw, &cw->dw_l,
			"<!DOCTYPE public><body>");
}				/* dwStart */

/*********************************************************************
Process the side effects of running js. These are:
w{ document.write() strings that fold back into html }
n{ new window() that may open a new edbrowse buffer }
t{ timer or interval calling a js function }
v{ javascript changes the value of an input field }
c{set cookie}
i{ innnerHtml or innerText }
f{ form submit or reset }
l{ linking objects together in a tree }
Any or all of these could be coded in the side effects string.
*********************************************************************/

static void processEffects(void)
{
	char *s, *t, *v;
	char c;
	jsobjtype p;
	int n;

	if (!effects)
		return;

	s = effects;
	while (c = *s) {	/* another effect */
		s += 2;
		v = strstr(s, "`~@}");	/* end marker */
/* There should always be an end marker -
 * unless there is a spurious null in the string. */
		if (!v)
			break;
		*v = 0;

		switch (c) {
		case 'w':	/* document.write */
			dwStart();
			stringAndString(&cw->dw, &cw->dw_l, s);
			break;

		case 'n':	/* new window */
/* url on one line, name of window on next line */
			t = strchr(s, '\n');
			*t = 0;
			javaOpensWindow(s, t + 1);
			break;

		case 'v':	/* value = "foo" */
			t = strchr(s, '=');
			*t++ = 0;
			sscanf(s, "%p", &p);
			prepareForField(t);
			javaSetsTagVar(p, t);
			break;

		case 't':	/* js timer */
			n = strtol(s, &t, 10);
			s = t + 1;
			t = strchr(s, '|');
			*t++ = 0;
			v[-2] = 0;
			sscanf(t, "%p", &p);
			javaSetsTimeout(n, s, p, v[-1] - '0');
			break;

		case 'c':	/* cookie */
/* Javascript does some modest syntax checking on the cookie before
 * passing it back to us, so I'm just going to assume it works. */
			receiveCookie(cw->fileName, s);
			break;

		case 'f':
			c = *s++;
			sscanf(s, "%p", &p);
			javaSubmitsForm(p, (c == 'r'));
			break;

		case 'i':
			c = *s++;
/* h = inner html, t = inner text */
			t = strchr(s, '|');
			*t++ = 0;
			sscanf(s, "%p", &p);
			javaSetsInner(p, t, c);
			break;

		case 'l':
			c = *s;
			s += 2;
			sscanf(s, "%p", &p);
			s = strchr(s, ',') + 1;
			javaSetsLinkage(c, p, s);
			break;

		}		/* switch */

/* skip past end marker + newline */
		s = v + 5;
	}			/* loop over effects */

	free(effects);
	effects = 0;
}				/* processEffects */

/* Read some data from the js process.
 * Close things down if there is any trouble from the read.
 * Returns 0 for ok or -1 for bad read. */
static int readFromJS(void *data_p, int n)
{
	int rc;
	if (n == 0)
		return 0;
	rc = read(pipe_in[0], data_p, n);
	debugPrint(7, "js read %d", rc);
	if (rc == n)
		return 0;
/* Oops - can't read from the process any more */
	i_puts(MSG_JSEngineRW);
	js_kill();
	markAllDead();
	return -1;
}				/* readFromJS */

static int writeToJS(const void *data_p, int n)
{
	int rc;
	if (n == 0)
		return 0;
	rc = write(pipe_out[1], data_p, n);
	if (rc == n)
		return 0;
/* Oops - can't write to the process any more */
	js_kill();
/* this call will print an error message for you */
	markAllDead();
	return -1;
}				/* writeToJS */

static char *propval;		/* property value, allocated */
static enum ej_proptype proptype;

/* Read the entire message from js, then take action.
 * Thus messages will remain in sync. */
static int readMessage(void)
{
	int l;
	char *msg;		/* error message from js */

	if (readFromJS(&head, sizeof(head)) < 0)
		return -1;	/* read failed */

	if (head.magic != EJ_MAGIC) {
/* this should never happen */
		js_kill();
		i_puts(MSG_JSEngineSync);
		markAllDead();
		return -1;
	}

	if (head.highstat >= EJ_HIGH_HEAP_FAIL) {
		js_kill();
/* perhaps a helpful message, before we close down js sessions */
		if (head.highstat == EJ_HIGH_PROC_FAIL)
			allowJS = false;
		if (head.lowstat == EJ_LOW_EXEC)
			i_puts(MSG_JSEngineExec);
		if (head.lowstat == EJ_LOW_MEMORY)
			i_puts(MSG_JavaMemError);
		if (head.lowstat == EJ_LOW_RUNTIME)
			i_puts(MSG_JSEngineRun);
		if (head.lowstat == EJ_LOW_SYNC)
			i_puts(MSG_JSEngineSync);
		markAllDead();
		return -1;
	}

	if (head.side) {
		effects = allocMem(head.side + 1);
		if (readFromJS(effects, head.side) < 0) {
			free(effects);
			effects = 0;
			return -1;
		}
		effects[head.side] = 0;
		if (debugLevel >= 4)
			printf("< side effects\n%s", effects);
		processEffects();
	}

/* next grab the error message, if there is one */
	l = head.msglen;
	if (l) {
		msg = allocMem(l + 1);
		if (readFromJS(msg, l)) {
			free(msg);
			return -1;
		}
		msg[l] = 0;
		if (debugLevel >= 3) {
/* print message, this will be in English, and mostly for our debugging */
			if (jsSourceFile)
				printf("%s line %d: ", jsSourceFile,
				       head.lineno);
			printf("%s\n", msg);
		}
		free(msg);
	}

/*  Read in the requested property, if there is one.
 * The calling function must handle the property. */
	l = head.proplength;
	proptype = head.proptype;
	if (l) {
		propval = allocMem(l + 1);
		if (readFromJS(propval, l)) {
			free(propval);
			propval = 0;
			return -1;
		}
		propval[l] = 0;
	}

	if (head.highstat == EJ_HIGH_CX_FAIL) {
		if (head.lowstat == EJ_LOW_VARS)
			i_puts(MSG_JSEngineVars);
		if (head.lowstat == EJ_LOW_CX)
			i_puts(MSG_JavaContextError);
		if (head.lowstat == EJ_LOW_WIN)
			i_puts(MSG_JavaWindowError);
		if (head.lowstat == EJ_LOW_DOC)
			i_puts(MSG_JavaObjError);
		if (head.lowstat == EJ_LOW_CLOSE)
			i_puts(MSG_PageDone);
		else
			i_puts(MSG_JSSessionFail);
		freeJavaContext(cw);
/* should I free and zero the property at this point? */
	}

	return 0;
}				/* readMessage */

static int writeHeader(void)
{
	head.magic = EJ_MAGIC;
	head.jcx = cw->jcx;
	head.winobj = cw->winobj;
	head.docobj = cw->docobj;
	return writeToJS(&head, sizeof(head));
}				/* writeHeader */

static const char *debugString(const char *v)
{
	if (!v)
		return emptyString;
	if (strlen(v) > 100)
		return "long";
	return v;
}				/* debugString */

/* If debug is at least 5, show a simple acknowledgement or error
 * from the js process. */
static void ack5(void)
{
	if (debugLevel < 5)
		return;
	printf("<");
	if (head.highstat)
		printf(" error %d|%d", head.highstat, head.lowstat);
	if (propval)
		printf(" %s\n", debugString(propval));
	else if (head.cmd == EJ_CMD_HASPROP)
		printf(" %d\n", head.proptype);
	else
		puts(" ok");
}				/* ack5 */

/* Create a js context for the current window.
 * The corresponding js context will be stored in cw->jcx. */
void createJavaContext(void)
{
	if (!allowJS)
		return;

	js_start();

	debugPrint(5, "> create context for session %d", context);

	memset(&head, 0, sizeof(head));
	head.cmd = EJ_CMD_CREATE;
	if (writeHeader())
		return;
	if (readMessage())
		return;
	ack5();

	if (head.highstat)
		return;

/* Copy the context pointer back to edbrowse. */
	cw->jcx = head.jcx;
	cw->winobj = head.winobj;
	cw->docobj = head.docobj;

	setupJavaDom();
}				/* createJavaContext */

/*********************************************************************
This is unique among all the wrappered calls in that it can be made for
a window that is not the current window.
You can free another window, or a whole stack of windows, by typeing
q2 while in session 1.
Thus I build the message header here, instead of using the standard
writeHeader() function above,
* whibuilds the message assuming cw.
*********************************************************************/

void freeJavaContext(struct ebWindow *w)
{
	if (!w->winobj)
		return;

	debugPrint(5, "> free context session %d", context);

	head.magic = EJ_MAGIC;
	head.cmd = EJ_CMD_DESTROY;
	head.jcx = w->jcx;
	head.winobj = w->winobj;
	head.docobj = w->docobj;
	if (writeToJS(&head, sizeof(head)))
		return;
	if (readMessage())
		return;
	ack5();
	w->jcx = w->winobj = 0;
}				/* freeJavaContext */

void js_shutdown(void)
{
	if (!js_pid)		/* js not running */
		return;
	debugPrint(5, "> js shutdown");
	head.magic = EJ_MAGIC;
	head.cmd = EJ_CMD_EXIT;
	head.jcx = 0;
	head.winobj = 0;
	head.docobj = 0;
	writeToJS(&head, sizeof(head));
}				/* js_shutdown */

/* After fork, the child process does not need to talk to js */
void js_disconnect(void)
{
	if (!js_pid)
		return;
	close(pipe_in[0]);
	close(pipe_out[1]);
	js_pid = 0;
}				/* js_disconnect */

/* Run some javascript code under the current window */
/* Pass the return value of the script back as a string. */
char *jsRunScriptResult(jsobjtype obj, const char *str, const char *filename,
			int lineno)
{
	int rc;
	char *s;

	if (!allowJS || !cw->winobj || !obj)
		return 0;

	if (!str || !str[0])
		return 0;

	debugPrint(5, "> script:");
	debugPrint(6, "%s", str);

	head.cmd = EJ_CMD_SCRIPT;
	head.obj = obj;		/* this, in js */
	head.proplength = strlen(str);
	head.lineno = lineno;
	if (writeHeader())
		return 0;
/* and send the script to execute */
	if (writeToJS(str, head.proplength))
		return 0;
	jsSourceFile = filename;
	rc = readMessage();
	jsSourceFile = 0;
	if (rc)
		return 0;
	ack5();

	s = propval;
	propval = 0;

	if (head.n) {		/* a real result */
		if (!s)
			s = emptyString;
	} else {
		nzFree(s);
		s = 0;
	}
	return s;
}				/* jsRunScriptResult */

/* like the above but throw away the result */
void jsRunScript(jsobjtype obj, const char *str, const char *filename,
		 int lineno)
{
	char *s = jsRunScriptResult(obj, str, filename, lineno);
	nzFree(s);
}				/* jsRunScript */

/* does the member exist? */
enum ej_proptype has_property(jsobjtype obj, const char *name)
{
	if (!allowJS || !cw->winobj || !obj)
		return EJ_PROP_NONE;

	debugPrint(5, "> has %s", name);

	head.cmd = EJ_CMD_HASPROP;
	head.n = strlen(name);
	head.obj = obj;
	if (writeHeader())
		return EJ_PROP_NONE;
	if (writeToJS(name, head.n))
		return EJ_PROP_NONE;
	if (readMessage())
		return EJ_PROP_NONE;
	ack5();
	return head.proptype;
}				/* has_property */

void delete_property(jsobjtype obj, const char *name)
{
	if (!allowJS || !cw->winobj || !obj)
		return;

	debugPrint(5, "> delete %s", name);

	head.cmd = EJ_CMD_DELPROP;
	head.obj = obj;
	head.n = strlen(name);
	if (writeHeader())
		return;
	if (writeToJS(name, head.n))
		return;
	if (readMessage())
		return;
	ack5();
}				/* delete_property */

/* Get a property from an object, js will tell us the type. */
static int get_property(jsobjtype obj, const char *name)
{
	propval = 0;		/* should already be 0 */
	if (!allowJS || !cw->winobj || !obj)
		return -1;

	debugPrint(5, "> get %s", name);

	head.cmd = EJ_CMD_GETPROP;
	head.n = strlen(name);
	head.obj = obj;
	if (writeHeader())
		return -1;
	if (writeToJS(name, head.n))
		return -1;
	if (readMessage())
		return -1;
	ack5();
	return 0;
}				/* get_property */

/* Some type specific wrappers around the above.
 * First is string; the caller must free it. */
char *get_property_string(jsobjtype obj, const char *name)
{
	char *s;
	get_property(obj, name);
	s = propval;
	propval = 0;
	if (!s && proptype == EJ_PROP_STRING)
		s = emptyString;
	return s;
}				/* get_property_string */

int get_property_number(jsobjtype obj, const char *name)
{
	int n = -1;
	get_property(obj, name);
	if (!propval)
		return n;
	if (stringIsNum(propval))
		n = atoi(propval);
	free(propval);
	propval = 0;
	return n;
}				/* get_property_number */

double get_property_float(jsobjtype obj, const char *name)
{
	double n = 0.0, d;
	get_property(obj, name);
	if (!propval)
		return n;
	if (stringIsFloat(propval, &d))
		n = d;
	free(propval);
	propval = 0;
	return n;
}				/* get_property_float */

bool get_property_bool(jsobjtype obj, const char *name)
{
	bool n = false;
	get_property(obj, name);
	if (!propval)
		return n;
	if (stringEqual(propval, "true") || stringEqual(propval, "1"))
		n = true;
	free(propval);
	propval = 0;
	return n;
}				/* get_property_bool */

/* get a js object, as a member of another object */
jsobjtype get_property_object(jsobjtype parent, const char *name)
{
	jsobjtype child = 0;
	get_property(parent, name);
	if (!propval)
		return child;
	if (proptype == EJ_PROP_OBJECT || proptype == EJ_PROP_ARRAY)
		sscanf(propval, "%p", &child);
	free(propval);
	propval = 0;
	return child;
}				/* get_property_object */

jsobjtype get_property_function(jsobjtype parent, const char *name)
{
	jsobjtype child = 0;
	get_property(parent, name);
	if (!propval)
		return child;
	if (proptype == EJ_PROP_FUNCTION)
		sscanf(propval, "%p", &child);
	free(propval);
	propval = 0;
	return child;
}				/* get_property_function */

/* Get an element of an array, again a string representation. */
static int get_array_element(jsobjtype obj, int idx)
{
	propval = 0;
	if (!allowJS || !cw->winobj || !obj)
		return -1;

	debugPrint(5, "> get [%d]", idx);

	head.cmd = EJ_CMD_GETAREL;
	head.n = idx;
	head.obj = obj;
	if (writeHeader())
		return -1;
	if (readMessage())
		return -1;
	ack5();
	return 0;
}				/* get_array_element */

jsobjtype get_array_element_object(jsobjtype obj, int idx)
{
	jsobjtype p = 0;
	get_array_element(obj, idx);
	if (!propval)
		return p;
	if (proptype == EJ_PROP_OBJECT || proptype == EJ_PROP_ARRAY)
		sscanf(propval, "%p", &p);
	free(propval);
	propval = 0;
	return p;
}				/* get_array_element_object */

static int set_property(jsobjtype obj, const char *name,
			const char *value, enum ej_proptype proptype)
{
	int l;

	if (!allowJS || !cw->winobj || !obj)
		return -1;

	debugPrint(5, "> set %s=%s", name, debugString(value));

	head.cmd = EJ_CMD_SETPROP;
	head.obj = obj;
	head.proptype = proptype;
	head.proplength = strlen(value);
	head.n = strlen(name);
	if (writeHeader())
		return -1;
	if (writeToJS(name, head.n))
		return -1;
	if (writeToJS(value, strlen(value)))
		return -1;
	if (proptype == EJ_PROP_FUNCTION)
		jsSourceFile = name;
	if (readMessage())
		return -1;
	jsSourceFile = NULL;
	ack5();

	return 0;
}				/* set_property */

int set_property_string(jsobjtype obj, const char *name, const char *value)
{
	if (value == NULL)
		value = emptyString;
	return set_property(obj, name, value, EJ_PROP_STRING);
}				/* set_property_string */

int set_property_number(jsobjtype obj, const char *name, int n)
{
	char buf[20];
	sprintf(buf, "%d", n);
	return set_property(obj, name, buf, EJ_PROP_INT);
}				/* set_property_number */

int set_property_float(jsobjtype obj, const char *name, double n)
{
	char buf[32];
	sprintf(buf, "%lf", n);
	return set_property(obj, name, buf, EJ_PROP_FLOAT);
}				/* set_property_float */

int set_property_bool(jsobjtype obj, const char *name, bool n)
{
	char buf[8];
	strcpy(buf, (n ? "1" : "0"));
	return set_property(obj, name, buf, EJ_PROP_BOOL);
}				/* set_property_bool */

int set_property_object(jsobjtype parent, const char *name, jsobjtype child)
{
	char buf[32];
	sprintf(buf, "%p", child);
	return set_property(parent, name, buf, EJ_PROP_OBJECT);
}				/* set_property_object */

jsobjtype instantiate_array(jsobjtype parent, const char *name)
{
	jsobjtype p = 0;

	if (!allowJS || !cw->winobj || !parent)
		return 0;

	debugPrint(5, "> new array %s", name);

	head.cmd = EJ_CMD_SETPROP;
	head.obj = parent;
	head.proptype = EJ_PROP_ARRAY;
	head.proplength = 0;
	head.n = strlen(name);
	if (writeHeader())
		return 0;
	if (writeToJS(name, head.n))
		return 0;
	if (readMessage())
		return 0;
	ack5();

	if (propval) {
		sscanf(propval, "%p", &p);
		free(propval);
		propval = 0;
	}

	return p;
}				/* instantiate_array */

static int set_array_element(jsobjtype array, int idx,
			     const char *value, enum ej_proptype proptype)
{
	int l;

	if (!allowJS || !cw->winobj || !array)
		return -1;

	debugPrint(5, "> set [%d]=%s", idx, debugString(value));

	head.cmd = EJ_CMD_SETAREL;
	head.obj = array;
	head.proptype = proptype;
	head.proplength = 0;
	if (value)
		head.proplength = strlen(value);
	head.n = idx;
	if (writeHeader())
		return -1;
	if (writeToJS(value, head.proplength))
		return -1;
	if (readMessage())
		return -1;
	ack5();

	return 0;
}				/* set_array_element */

int set_array_element_object(jsobjtype array, int idx, jsobjtype child)
{
	char buf[32];
	sprintf(buf, "%p", child);
	return set_array_element(array, idx, buf, EJ_PROP_OBJECT);
}				/* set_array_element_object */

jsobjtype instantiate_array_element(jsobjtype array, int idx,
				    const char *classname)
{
	jsobjtype p = 0;
	set_array_element(array, idx, classname, EJ_PROP_INSTANCE);
	if (!propval)
		return p;
	sscanf(propval, "%p", &p);
	nzFree(propval);
	propval = 0;
	return p;
}				/* instantiate_array_element */

/* Instantiate a new object from a given class.
 * Return is NULL if there is a js disaster.
 * Set classname = NULL for a generic object. */
jsobjtype instantiate(jsobjtype parent, const char *name, const char *classname)
{
	jsobjtype p = 0;

	if (!allowJS || !cw->winobj || !parent)
		return 0;

	debugPrint(5, "> instantiate %s %s", name,
		   (classname ? classname : "object"));

	head.cmd = EJ_CMD_SETPROP;
	head.obj = parent;
	head.proptype = EJ_PROP_INSTANCE;
	if (!classname)
		classname = emptyString;
	head.proplength = strlen(classname);
	head.n = strlen(name);
	if (writeHeader())
		return 0;
	if (writeToJS(name, head.n))
		return 0;
	if (writeToJS(classname, head.proplength))
		return 0;
	if (readMessage())
		return 0;
	ack5();

	if (propval) {
		sscanf(propval, "%p", &p);
		free(propval);
		propval = 0;
	}

	return p;
}				/* instantiate */

int set_property_function(jsobjtype parent, const char *name, const char *body)
{
	if (!body)
		body = emptyString;
	return set_property(parent, name, body, EJ_PROP_FUNCTION);
/* should this really return the function created, like instantiate()? */
}				/* set_property_function */

/* call javascript function with arguments, but all args must be objects */
static int run_function(jsobjtype obj, const char *name, int argc,
			const jsobjtype * argv)
{
	int rc;
	propval = 0;		/* should already be 0 */
	if (!allowJS || !cw->winobj || !obj)
		return -1;

	debugPrint(5, "> call %s(%d)", name, argc);

	if (argc) {
		int i, l;
		char oval[20];
		propval = initString(&l);
		for (i = 0; i < argc; ++i) {
			sprintf(oval, "%p|", argv[i]);
			stringAndString(&propval, &l, oval);
		}
	}

	head.cmd = EJ_CMD_CALL;
	head.n = strlen(name);
	head.obj = obj;
	head.proplength = 0;
	if (propval)
		head.proplength = strlen(propval);
	if (writeHeader())
		return -1;
	if (writeToJS(name, head.n))
		return -1;
	if (propval) {
		rc = writeToJS(propval, head.proplength);
		nzFree(propval);
		propval = 0;
		if (rc)
			return -1;
	}
	if (readMessage())
		return -1;
	ack5();
	return 0;
}				/* run_function */

int get_arraylength(jsobjtype a)
{
	head.cmd = EJ_CMD_ARLEN;
	head.obj = a;
	if (writeHeader())
		return -1;
	if (readMessage())
		return -1;
	ack5();
	return head.n;
}				/* get_arraylength */

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
	set_property_string(cw->docobj, "base$href$", h);
}				/* set_basehref */

/* The object is a select-one field in the form, and this function returns
 * object.options[selectedIndex].value */
char *get_property_option(jsobjtype obj)
{
	int n;
	jsobjtype oa;		/* option array */
	jsobjtype oo;		/* option object */
	char *val;

	if (!allowJS || !cw->winobj || !obj)
		return 0;

	n = get_property_number(obj, "selectedIndex");
	if (n < 0)
		return 0;
	oa = get_property_object(obj, "options");
	if (!oa)
		return 0;
	oo = get_array_element_object(oa, n);
	if (!oo)
		return 0;
	return get_property_string(oo, "value");
}				/* get_property_option */

/*********************************************************************
When an element is created without a name, it is not linked to its
owner (via that name), and could be cleared via garbage collection.
This is a disaster!
Create a fake name, so we can attach the element.
*********************************************************************/

/* set document.cookie to the cookies relevant to this url */
static void docCookie(jsobjtype d)
{
	int cook_l;
	char *cook = initString(&cook_l);
	const char *url = cw->fileName;
	bool secure = false;
	const char *proto;
	char *s;

	if (url) {
		proto = getProtURL(url);
		if (proto && stringEqualCI(proto, "https"))
			secure = true;
		sendCookies(&cook, &cook_l, url, secure);
		if (memEqualCI(cook, "cookie: ", 8)) {	/* should often happen */
			strmove(cook, cook + 8);
		}
		if (s = strstr(cook, "\r\n"))
			*s = 0;
	}

	set_property_string(d, "cookie", cook);
	nzFree(cook);
}				/* docCookie */

#ifdef DOSLIKE			// port of uname(p), and struct utsname
struct utsname {
	char sysname[32];
	char machine[32];
};

int uname(struct utsname *pun)
{
	memset(pun, 0, sizeof(struct utsname));

	return 0;
}

#else // !DOSLIKE - // port of uname(p), and struct utsname
#include <sys/utsname.h>
#endif // DOSLIKE y/n // port of uname(p), and struct utsname

/* After createJavaContext, set up the document object and other variables
 * and methods that are base for client side DOM. */
void setupJavaDom(void)
{
	jsobjtype w = cw->winobj;	// window object
	jsobjtype d = cw->docobj;	// document object
	jsobjtype nav;		// navigator object
	jsobjtype navpi;	// navigator plugins
	jsobjtype navmt;	// navigator mime types
	jsobjtype hist;		// history object
	struct MIMETYPE *mt;
	struct utsname ubuf;
	int i;
	char save_c;
	static const char *languages[] = { 0,
		"english", "french", "portuguese", "polish",
		"german", "russian",
	};
	extern const char *startWindowJS;

/* self reference through several names */
	set_property_object(w, "window", w);
	set_property_object(w, "self", w);
	set_property_object(w, "parent", w);
	set_property_object(w, "top", w);

	nav = instantiate(w, "navigator", 0);
	if (!nav)
		return;
/* some of the navigator is in startwindow.js; the runtime properties are here. */
	set_property_string(nav, "userLanguage", languages[eb_lang]);
	set_property_string(nav, "language", languages[eb_lang]);
	set_property_string(nav, "appVersion", version);
	set_property_string(nav, "vendorSub", version);
	set_property_string(nav, "userAgent", currentAgent);
	uname(&ubuf);
	set_property_string(nav, "oscpu", ubuf.sysname);
	set_property_string(nav, "platform", ubuf.machine);

/* Build the array of mime types and plugins,
 * according to the entries in the config file. */
	navpi = instantiate_array(nav, "plugins");
	if (navpi == NULL)
		return;
	navmt = instantiate_array(nav, "mimeTypes");
	if (navmt == NULL)
		return;
	mt = mimetypes;
	for (i = 0; i < maxMime; ++i, ++mt) {
		int len;
/* po is the plugin object and mo is the mime object */
		jsobjtype po = instantiate_array_element(navpi, i, 0);
		jsobjtype mo = instantiate_array_element(navmt, i, 0);
		if (po == NULL || mo == NULL)
			return;
		set_property_object(mo, "enabledPlugin", po);
		set_property_string(mo, "type", mt->type);
		set_property_object(navmt, mt->type, mo);
		set_property_string(mo, "description", mt->desc);
		set_property_string(mo, "suffixes", mt->suffix);
/* I don't really have enough information from the config file to fill
 * in the attributes of the plugin object.
 * I'm just going to fake it.
 * Description will be the same as that of the mime type,
 * and the filename will be the program to run.
 * No idea if this is right or not. */
		set_property_string(po, "description", mt->desc);
		set_property_string(po, "filename", mt->program);
/* For the name, how about the program without its options? */
		len = strcspn(mt->program, " \t");
		save_c = mt->program[len];
		mt->program[len] = 0;
		set_property_string(po, "name", mt->program);
		mt->program[len] = save_c;
	}

	hist = instantiate(w, "history", 0);
	if (hist == NULL)
		return;
	set_property_string(hist, "current", cw->fileName);
/* Since there is no history in edbrowse, the rest is left to startwindow.js */

/* the js window/document setup script.
 * These are all the things that do not depend on the platform,
 * OS, configurations, etc. */
	jsRunScript(w, startWindowJS, "StartWindow", 1);

// Document properties that must be set after startwindow.js.
// Most of these use the setters in the URL class.
	set_property_string(d, "referrer", cw->referrer);
	instantiate_url(d, "URL", cw->fileName);
	instantiate_url(d, "location", cw->fileName);
	instantiate_url(w, "location", cw->fileName);
	set_property_string(d, "domain", getHostURL(cw->fileName));
	docCookie(d);
}				/* setupJavaDom */

/* Get the url from a url object, special wrapper.
 * Owner object is passed, look for obj.href, obj.src, or obj.action.
 * Return that if it's a string, or its member href if it is a url.
 * The result, coming from get_property_string, is allocated. */
char *get_property_url(jsobjtype owner, bool action)
{
	enum ej_proptype mtype;	/* member type */
	jsobjtype uo = 0;	/* url object */

	if (action) {
		mtype = has_property(owner, "action");
		if (mtype == EJ_PROP_STRING)
			return get_property_string(owner, "action");
		if (mtype != EJ_PROP_OBJECT)
			return 0;
		uo = get_property_object(owner, "action");
		if (has_property(uo, "actioncrash"))
			return 0;
	} else {
		mtype = has_property(owner, "href");
		if (mtype == EJ_PROP_STRING)
			return get_property_string(owner, "href");
		if (mtype == EJ_PROP_OBJECT)
			uo = get_property_object(owner, "href");
		else if (mtype)
			return 0;
		if (!uo) {
			mtype = has_property(owner, "src");
			if (mtype == EJ_PROP_STRING)
				return get_property_string(owner, "src");
			if (mtype == EJ_PROP_OBJECT)
				uo = get_property_object(owner, "src");
		}
	}

	if (uo == NULL)
		return 0;
/* should this be href$val? */
	return get_property_string(uo, "href");
}				/* get_property_url */

/*********************************************************************
Javascript sometimes builds or rebuilds a submenu, based upon your selection
in a primary menu. These new options must map back to html tags,
and then to the dropdown list as you interact with the form.
This is tested in jsrt - select a state,
whereupon the colors below, that you have to choose from, can change.

Someday the entire page will be rerendered based upon the js tree,
which could be modified in almost any way, but today I only look at
changing menus, because that is the high runner case.
Besides, it use to seg fault when I didn't watch for this.
*********************************************************************/

static void rebuildSelector(struct htmlTag *sel, jsobjtype oa, int len2)
{
	int i1, i2, len1;
	bool check2;
	char *s;
	const char *selname;
	bool changed = false;
	struct htmlTag *t;
	jsobjtype oo;		/* option object */

	len1 = cw->numTags;
	i1 = i2 = 0;
	selname = sel->name;
	if (!selname)
		selname = "?";
	debugPrint(4, "testing selector %s %d %d", selname, len1, len2);

	sel->lic = (sel->multiple ? 0 : -1);

	while (i1 < len1 && i2 < len2) {
/* there is more to both lists */
		t = tagList[i1++];
		if (t->action != TAGACT_OPTION)
			continue;
		if (t->controller != sel)
			continue;

/* find the corresponding option object */
		if ((oo = get_array_element_object(oa, i2)) == NULL) {
/* Wow this shouldn't happen. */
/* Guess I'll just pretend the array stops here. */
			len2 = i2;
			--i1;
			break;
		}

		t->jv = oo;	/* should already equal oo */
		t->rchecked = get_property_bool(oo, "defaultSelected");
		check2 = get_property_bool(oo, "selected");
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
		s = get_property_string(oo, "text");
		if (s && !t->textval || !stringEqual(t->textval, s)) {
			nzFree(t->textval);
			t->textval = s;
			changed = true;
		} else
			nzFree(s);
		s = get_property_string(oo, "value");
		if (s && !t->value || !stringEqual(t->value, s)) {
			nzFree(t->value);
			t->value = s;
		} else
			nzFree(s);
	}

/* one list or the other or both has run to the end */
	if (i2 == len2) {
		for (; i1 < len1; ++i1) {
			t = tagList[i1];
			if (t->action != TAGACT_OPTION)
				continue;
			if (t->controller != sel)
				continue;
/* option is gone in js, disconnect this option tag from its select */
			t->jv = 0;
			t->controller = 0;
			t->action = TAGACT_NOP;
			changed = true;
		}
	} else if (i1 == len1) {
		for (; i2 < len2; ++i2) {
			if ((oo = get_array_element_object(oa, i2)) == NULL)
				break;
			t = newTag("option");
			t->lic = i2;
			t->controller = sel;
			t->jv = oo;
			t->textval = get_property_string(oo, "text");
			t->value = get_property_string(oo, "value");
			t->checked = get_property_bool(oo, "selected");
			if (t->checked) {
				if (sel->multiple)
					++sel->lic;
				else
					sel->lic = i2;
			}
			t->rchecked = get_property_bool(oo, "defaultSelected");
			changed = true;
		}
	}

	if (!changed)
		return;
	debugPrint(4, "selector %s has changed", selname);

/* If js change the menu, it should have also changed select.value
 * according to the checked options, but did it?
 * Don't know, so I'm going to do it here. */
	s = displayOptions(sel);
	if (!s)
		s = emptyString;
	set_property_string(sel->jv, "value", s);
	javaSetsTagVar(sel->jv, s);
	nzFree(s);

	if (!sel->multiple)
		set_property_number(sel->jv, "selectedIndex", sel->lic);
}				/* rebuildSelector */

void rebuildSelectors(void)
{
	int i1;
	struct htmlTag *t;
	jsobjtype oa;		/* option array */
	int len;		/* length of option array */

	for (i1 = 0; i1 < cw->numTags; ++i1) {
		t = tagList[i1];
		if (!t->jv)
			continue;
		if (t->action != TAGACT_INPUT)
			continue;
		if (t->itype != INP_SELECT)
			continue;

/* there should always be an options array, if not then move on */
		if ((oa = get_property_object(t->jv, "options")) == NULL)
			continue;
		if ((len = get_arraylength(oa)) < 0)
			continue;
		rebuildSelector(t, oa, len);
	}

}				/* rebuildSelectors */

/* run a function with no args that returns an object */
jsobjtype run_function_object(jsobjtype obj, const char *name)
{
	run_function(obj, name, 0, NULL);
	if (!propval)
		return NULL;
	if (head.proptype == EJ_PROP_OBJECT || head.proptype == EJ_PROP_ARRAY) {
		jsobjtype p;
		sscanf(propval, "%p", &p);
		nzFree(propval);
		propval = 0;
		return p;
	}
/* wrong type, just return NULL */
	nzFree(propval);
	propval = 0;
	return NULL;
}				/* run_function_object */

/* run a function with no args that returns a boolean */
bool run_function_bool(jsobjtype obj, const char *name)
{
	run_function(obj, name, 0, NULL);
	if (!propval)
		return false;
	if (head.proptype == EJ_PROP_BOOL) {
		bool rc = (propval[0] == '1');
		nzFree(propval);
		propval = 0;
		return rc;
	}
/* wrong type, just return false */
	nzFree(propval);
	propval = 0;
	return false;
}				/* run_function_bool */

void run_function_objargs(jsobjtype obj, const char *name, int nargs, ...)
{
/* lazy, limit of 20 args */
	jsobjtype argv[20];
	int i;
	va_list p;

	if (nargs > 20) {
		puts("more than 20 args to a javascript function");
		return;
	}

	va_start(p, nargs);
	for (i = 0; i < nargs; ++i)
		argv[i] = va_arg(p, jsobjtype);
	va_end(p);

	run_function(obj, name, nargs, argv);

/* return is thrown away; this is a void function */
	nzFree(propval);
	propval = 0;
}				/* run_function_objargs */

void run_function_onearg(jsobjtype obj, const char *name, jsobjtype a)
{
	run_function_objargs(obj, name, 1, a);
}				/* run_function_onearg */
