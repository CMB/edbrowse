/*********************************************************************
This is the back-end process for javascript.
We receive calls from edbrowse,
getting and setting properties for various DOM objects.
This is the duktape version.
If you package this with the duktape js libraries,
you will need to include the MIT open source license,
along with the GPL, general public license.
*********************************************************************/

#include "eb.h"

#ifdef DOSLIKE
#include "vsprtf.h"
#endif // DOSLIKE

#include <duktape.h>

static void processError(duk_context * cx);
static void jsInterruptCheck(duk_context * cx);
// The level 0 functions live right next to the engine, and in the interest
// of encapsulation, they should not be called outside of this file.
static enum ej_proptype typeof_property_0(jsobjtype cx, jsobjtype obj, const char *name) ;
static bool has_property_0(jsobjtype cx, jsobjtype obj, const char *name) ;
static void delete_property_0(jsobjtype cx, jsobjtype obj, const char *name) ;
static char *get_property_string_0(jsobjtype cx, jsobjtype obj, const char *name) ;
static int get_property_number_0(jsobjtype cx, jsobjtype parent, const char *name) ;
static bool get_property_bool_0(jsobjtype cx, jsobjtype parent, const char *name) ;
static jsobjtype get_property_object_0(jsobjtype cx, jsobjtype parent, const char *name) ;
static jsobjtype get_array_element_object_0(jsobjtype cx, jsobjtype obj, int idx) ;
static int set_property_string_0(jsobjtype cx, jsobjtype obj, const char *name, const char *value) ;
static int set_property_number_0(jsobjtype cx, jsobjtype obj, const char *name, int n) ;
static int set_property_bool_0(jsobjtype cx, jsobjtype obj, const char *name, bool n) ;
static int set_property_object_0(jsobjtype cx, jsobjtype parent, const char *name, jsobjtype child) ;
static jsobjtype instantiate_array_0(jsobjtype cx, jsobjtype parent, const char *name) ;
static int set_array_element_object_0(jsobjtype cx, jsobjtype array, int idx, jsobjtype child) ;
static jsobjtype instantiate_array_element_0(jsobjtype cx, jsobjtype array, int idx, const char *classname) ;
static jsobjtype instantiate_0(jsobjtype cx, jsobjtype parent, const char *name, const char *classname) ;
static int get_arraylength_0(jsobjtype cx, jsobjtype a);
static bool run_function_bool_0(jsobjtype cx, jsobjtype obj, const char *name);
static int run_function_onearg_0(jsobjtype cx, jsobjtype obj, const char *name, jsobjtype o);
static void run_function_onestring_0(jsobjtype cx, jsobjtype parent, const char *name, const char *s);
static bool run_event_0(jsobjtype cx, jsobjtype obj, const char *pname, const char *evname);
static Tag *tagFromObject(jsobjtype v);
static Frame *thisFrame(duk_context *cx, const char *whence);

// some do-nothing functions
static duk_ret_t nat_void(duk_context * cx)
{
	return 0;
}

static duk_ret_t nat_null(duk_context * cx)
{
	duk_push_null(cx);
	return 1;
}

static duk_ret_t nat_true(duk_context * cx)
{
	duk_push_true(cx);
	return 1;
}

static duk_ret_t nat_false(duk_context * cx)
{
	duk_push_false(cx);
	return 1;
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
		t->jv = 0;
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
// Below a frame, t could be a manufactured document for the new window.
// We don't want to set eb$seqno in this case.
	if(t->action != TAGACT_DOC) {
		set_property_number_0(t->f0->cx, p, "eb$seqno", t->seqno);
		set_property_number_0(t->f0->cx, p, "eb$gsn", t->gsn);
	}
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

static int js_main(void)
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
}

// base64 encode
static duk_ret_t nat_btoa(duk_context * cx)
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
static duk_ret_t nat_atob(duk_context * cx)
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

static duk_ret_t nat_new_location(duk_context * cx)
{
	const char *s = duk_safe_to_string(cx, -1);
	if (s && *s) {
		char *t = cloneString(s);
/* url on one line, name of window on next line */
		char *u = strchr(t, '\n');
		*u++ = 0;
		debugPrint(4, "window %s|%s", t, u);
		domOpensWindow(t, u);
		nzFree(t);
	}
	return 0;
}

static duk_ret_t nat_mywin(duk_context * cx)
{
	duk_push_global_object(cx);
	return 1;
}

static duk_ret_t nat_mydoc(duk_context * cx)
{
	duk_get_global_string(cx, "document");
	return 1;
}

static duk_ret_t nat_hasfocus(duk_context * cx)
{
	duk_push_boolean(cx, foregroundWindow);
	return 1;
}

static duk_ret_t nat_puts(duk_context * cx)
{
	const char *s = duk_safe_to_string(cx, -1);
	if (!s)
		s = emptyString;
	puts(s);
	return 0;
}

// write local file
static duk_ret_t nat_wlf(duk_context * cx)
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

static duk_ret_t nat_media(duk_context * cx)
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

static duk_ret_t nat_logputs(duk_context * cx)
{
	int minlev = duk_get_int(cx, 0);
	const char *s = duk_safe_to_string(cx, 1);
	duk_remove(cx, 0);
	if (debugLevel >= minlev && s && *s)
		debugPrint(3, "%s", s);
	jsInterruptCheck(cx);
	return 0;
}

static duk_ret_t nat_prompt(duk_context * cx)
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

static duk_ret_t nat_confirm(duk_context * cx)
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
	Tag *t;
	const char *h = duk_safe_to_string(cx, -1);
	if (!h)			// should never happen
		h = emptyString;
	debugPrint(5, "setter h in");
	jsInterruptCheck(cx);
	duk_push_this(cx);
// remove the preexisting children.
	if (duk_get_prop_string(cx, -1, "childNodes") && duk_is_array(cx, -1)) {
		c1 = duk_get_heapptr(cx, -1);
	} else {
// no child nodes array, don't do anything.
// This should never happen.
		duk_pop_n(cx, 3);
		debugPrint(5, "setter h fail");
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
		t = tagFromObject(thisobj);
	if(t) {
		html_from_setter(t, run);
	} else {
		debugPrint(1, "innerHTML finds no tag, cannot parse");
	}
	nzFree(run);
	debugPrint(5, "setter h out");

	run_function_onearg_0(cx, cf->winobj, "textarea$html$crossover",
				thisobj);

// mutation fix up from native code
	duk_push_heapptr(cx, cf->winobj);
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
	char *k;
	Tag *t;
	const char *h = duk_safe_to_string(cx, -1);
	if (!h)			// should never happen
		h = emptyString;
	debugPrint(5, "setter v in");
	k = cloneString(h);
	duk_push_this(cx);
	duk_insert(cx, -2);
	duk_put_prop_string(cx, -2, "val$ue");
	thisobj = duk_get_heapptr(cx, -1);
	duk_pop(cx);
	prepareForField(k);
	t = tagFromObject(thisobj);
	if(t) {
		debugPrint(4, "value tag %d=%s", t->seqno, k);
		domSetsTagValue(t, k);
	}
	nzFree(k);
	debugPrint(5, "setter v out");
	return 0;
}

static int frameContractLine(int lineNumber);
static int frameExpandLine(int ln, Tag *t);

static void forceFrameExpand(Tag *t)
{
	Frame *save_cf = cf;
	const char *save_src = jsSourceFile;
	int save_lineno = jsLineno;
	bool save_plug = pluginsOn;
	pluginsOn = false;
	frameExpandLine(0, t);
	cf = save_cf;
	jsSourceFile = save_src;
	jsLineno = save_lineno;
	pluginsOn = save_plug;
}

// contentDocument getter setter; this is a bit complicated.
static duk_ret_t getter_cd(duk_context * cx)
{
	jsobjtype thisobj;
	Tag *t;
	jsInterruptCheck(cx);
	duk_push_this(cx);
	thisobj = duk_get_heapptr(cx, -1);
	duk_pop(cx);
	t = tagFromObject(thisobj);
	if(!t)
		goto fail;
	if(!t->f1)
		forceFrameExpand(t);
	if(!t->f1 || !t->f1->docobj) // should not happen
		goto fail;
	duk_push_heapptr(cx, t->f1->docobj);
	return 1;
fail:
	duk_push_null(cx);
	return 1;
}

static duk_ret_t getter_cw(duk_context * cx)
{
	jsobjtype thisobj;
	Tag *t;
	jsInterruptCheck(cx);
	duk_push_this(cx);
	thisobj = duk_get_heapptr(cx, -1);
	duk_pop(cx);
	t = tagFromObject(thisobj);
	if(!t)
		goto fail;
	if(!t->f1)
		forceFrameExpand(t);
	if(!t->f1 || !t->f1->winobj) // should not happen
		goto fail;
	duk_push_heapptr(cx, t->f1->winobj);
	return 1;
fail:
	duk_push_null(cx);
	return 1;
}

/*********************************************************************
I'm putting the frame expand stuff here cause there really isn't a good place
to put it, and it sort of goes with the unframe native methods below.
Plus forceFrameExpand() when you access contentDocument or contentWindow
through the getters above. So I think it belongs here.
frameExpand(expand, start, end)
Pass a range of lines; you can expand all the frames in one go.
Return false if there is a problem fetching a web page,
or if none of the lines are frames.
If first argument expand is false then we are contracting.
Call either frameExpandLine or frameContractLine on each line in the range.
frameExpandLine takes a line number or a tag, not both.
One or the other will be 0.
If a line number, it comes from a user command, you asked to expand the frame.
If the tag is not null, it is from a getter,
javascript is trying to access objects within that frame,
and now we need to expand it.
*********************************************************************/

bool frameExpand(bool expand, int ln1, int ln2)
{
	int ln;			/* line number */
	int problem = 0, p;
	bool something_worked = false;

	for (ln = ln1; ln <= ln2; ++ln) {
		if (expand)
			p = frameExpandLine(ln, NULL);
		else
			p = frameContractLine(ln);
		if (p > problem)
			problem = p;
		if (p == 0)
			something_worked = true;
	}

	if (something_worked && problem < 3)
		problem = 0;
	if (problem == 1)
		setError(expand ? MSG_NoFrame1 : MSG_NoFrame2);
	if (problem == 2)
		setError(MSG_FrameNoURL);
	return (problem == 0);
}				/* frameExpand */

/* Problems: 0, frame expanded successfully.
 1 line is not a frame.
 2 frame doesn't have a valid url.
 3 Problem fetching the rul or rendering the page.  */
static int frameExpandLine(int ln, Tag *t)
{
	pst line;
	int tagno, start;
	const char *s, *jssrc = 0;
	char *a;
	Frame *save_cf, *new_cf, *last_f;
	uchar save_local;
	bool fromget = !ln;
	Tag *cdt;	// contentDocument tag

	if(!t) {
		line = fetchLine(ln, -1);
		s = stringInBufLine((char *)line, "Frame ");
		if (!s)
			return 1;
		if ((s = strchr(s, InternalCodeChar)) == NULL)
			return 2;
		tagno = strtol(s + 1, (char **)&s, 10);
		if (tagno < 0 || tagno >= cw->numTags || *s != '{')
			return 2;
		t = tagList[tagno];
	}

	if (t->action != TAGACT_FRAME)
		return 1;

/* the easy case is if it's already been expanded before, we just unhide it. */
	if (t->f1) {
// If js is accessing objects in this frame, that doesn't mean we unhide it.
		if (!fromget)
			t->contracted = false;
		return 0;
	}

// maybe we tried to expand it and it failed
	if(t->expf)
		return 0;
	t->expf = true;

// Check with js first, in case it changed.
	if ((a = get_property_url_t(t, false)) && *a) {
		nzFree(t->href);
		t->href = a;
	}
	s = t->href;

// javascript in the src, what is this for?
	if (s && !strncmp(s, "javascript:", 11)) {
		jssrc = s;
		s = 0;
	}

	if (!s) {
// No source. If this is your request then return an error.
// But if we're dipping into the objects then it needs to expand
// into a separate window, a separate js space, with an empty body.
		if (!fromget && !jssrc)
			return 2;
// After expansion we need to be able to expand it,
// because there's something there, well maybe.
		t->href = cloneString("#");
// jssrc is the old href and now we are responsible for it
	}

	save_cf = cf = t->f0;
/* have to push a new frame before we read the web page */
	for (last_f = &(cw->f0); last_f->next; last_f = last_f->next) ;
	last_f->next = cf = allocZeroMem(sizeof(Frame));
	cf->owner = cw;
	cf->frametag = t;
	cf->gsn = ++gfsn;
	debugPrint(2, "fetch frame %s",
		   (s ? s : (jssrc ? "javascript" : "empty")));

	if (s) {
		bool rc = readFileArgv(s, (fromget ? 2 : 1));
		if (!rc) {
/* serverData was never set, or was freed do to some other error. */
/* We just need to pop the frame and return. */
			fileSize = -1;	/* don't print 0 */
			nzFree(cf->fileName);
			free(cf);
			last_f->next = 0;
			cf = save_cf;
			return 3;
		}

       /*********************************************************************
readFile could return success and yet serverData is null.
This happens if httpConnect did something other than fetching data,
like playing a stream. Does that happen, even in a frame?
It can, if the frame is a youtube video, which is not unusual at all.
So check for serverData null here. Once again we pop the frame.
*********************************************************************/

		if (serverData == NULL) {
			nzFree(cf->fileName);
			free(cf);
			last_f->next = 0;
			cf = save_cf;
			fileSize = -1;
			return 0;
		}
	} else {
		serverData = cloneString("<body></body>");
		serverDataLen = strlen(serverData);
	}

	new_cf = cf;
	if (changeFileName) {
		nzFree(cf->fileName);
		cf->fileName = changeFileName;
		cf->uriEncoded = true;
		changeFileName = 0;
	} else {
		cf->fileName = cloneString(s);
	}

/* don't print the size of what we just fetched */
	fileSize = -1;

/* If we got some data it has to be html.
 * I should check for that, something like htmlTest(),
 * but I'm too lazy to do that right now, so I'll just assume it's good.
 * Also, we have verified content-type = text/html, so that's pretty good. */

	cf->hbase = cloneString(cf->fileName);
	save_local = browseLocal;
	browseLocal = !isURL(cf->fileName);
	prepareForBrowse(serverData, serverDataLen);
	if (javaOK(cf->fileName))
		createJSContext(cf);
	nzFree(newlocation);	/* should already be 0 */
	newlocation = 0;

	start = cw->numTags;
	cdt = newTag(cf, "Document");
	cdt->parent = t, t->firstchild = cdt;
	cdt->attributes = allocZeroMem(sizeof(char*));
	cdt->atvals = allocZeroMem(sizeof(char*));
/* call the tidy parser to build the html nodes */
	html2nodes(serverData, true);
	nzFree(serverData);	/* don't need it any more */
	serverData = 0;
	htmlGenerated = false;
	htmlNodesIntoTree(start + 1, cdt);
	prerender(0);

/*********************************************************************
At this point cdt->step is 1; the html tree is built, but not decorated.
cdt doesn't have or need an object; it's a place holder.
*********************************************************************/
	cdt->step = 2;

	if (cf->docobj) {
		decorate(0);
		set_basehref(cf->hbase);
		run_function_bool_0(cf->cx, cf->winobj, "eb$qs$start");
		if (jssrc)
			jsRunScriptWin(jssrc, "frame.src", 1);
		runScriptsPending(true);
		runOnload();
		runScriptsPending(false);
		set_property_string_0(cf->cx, cf->docobj, "readyState", "complete");
		run_event_doc(cf, "document", "onreadystatechange");
		runScriptsPending(false);
		rebuildSelectors();
	}
	cnzFree(jssrc);

	if (cf->fileName) {
		int j = strlen(cf->fileName);
		cf->fileName = reallocMem(cf->fileName, j + 8);
		strcat(cf->fileName, ".browse");
	}

	t->f1 = cf;
	if (fromget)
		t->contracted = true;
	if (new_cf->docobj) {
		jsobjtype cdo;	// contentDocument object
		cdo = new_cf->docobj;
		disconnectTagObject(cdt);
		connectTagObject(cdt, cdo);
		set_property_bool_t(t, "eb$expf", true);
// run the frame onload function if it is there.
// I assume it should run in the higher frame.
		run_event_t(t, t->info->name, "onload");
	}

// success, frame is expanded
	cf = save_cf;
	browseLocal = save_local;
	return 0;
}

static int frameContractLine(int ln)
{
	Tag *t = line2frame(ln);
	if (!t)
		return 1;
	t->contracted = true;
	return 0;
}				/* frameContractLine */

bool reexpandFrame(void)
{
	int j, start;
	Tag *frametag;
	Tag *cdt;	// contentDocument tag
	uchar save_local;
	Frame *save_cf = cf;
	bool rc;

// I don't know why cf would ever not be newloc_f.
	cf = newloc_f;
	frametag = cf->frametag;
	cdt = frametag->firstchild;

// Cut away our tree nodes from the previous document, which are now inaccessible.
	underKill(cdt);

	delTimers(cf);
	freeJSContext(cf);
	nzFree(cf->dw);
	cf->dw = 0;
	nzFree(cf->hbase);
	cf->hbase = 0;
	nzFree(cf->fileName);
	cf->fileName = newlocation;
	newlocation = 0;
	cf->uriEncoded = false;
	nzFree(cf->firstURL);
	cf->firstURL = 0;
	rc = readFileArgv(cf->fileName, 2);
	if (!rc) {
/* serverData was never set, or was freed do to some other error. */
		fileSize = -1;	/* don't print 0 */
		cf = save_cf;
		return false;
	}

	if (serverData == NULL) {
/* frame replaced itself with a playable stream, what to do? */
		fileSize = -1;
		cf = save_cf;
		return true;
	}

	if (changeFileName) {
		nzFree(cf->fileName);
		cf->fileName = changeFileName;
		cf->uriEncoded = true;
		changeFileName = 0;
	}

	/* don't print the size of what we just fetched */
	fileSize = -1;

	cf->hbase = cloneString(cf->fileName);
	save_local = browseLocal;
	browseLocal = !isURL(cf->fileName);
	prepareForBrowse(serverData, serverDataLen);
	if (javaOK(cf->fileName))
		createJSContext(cf);

	start = cw->numTags;
/* call the tidy parser to build the html nodes */
	html2nodes(serverData, true);
	nzFree(serverData);	/* don't need it any more */
	serverData = 0;
	htmlGenerated = false;
	htmlNodesIntoTree(start, cdt);
	cdt->step = 0;
	prerender(0);
	cdt->step = 2;

	if (cf->docobj) {
		decorate(0);
		set_basehref(cf->hbase);
		run_function_bool_0(cf->cx, cf->winobj, "eb$qs$start");
		runScriptsPending(true);
		runOnload();
		runScriptsPending(false);
		set_property_string_0(cf->cx, cf->docobj, "readyState", "complete");
		run_event_doc(cf, "document", "onreadystatechange");
		runScriptsPending(false);
		rebuildSelectors();
	}

	j = strlen(cf->fileName);
	cf->fileName = reallocMem(cf->fileName, j + 8);
	strcat(cf->fileName, ".browse");

	if (cf->docobj) {
		jsobjtype cdo;	// contentDocument object
		cdo = cf->docobj;
		disconnectTagObject(cdt);
		connectTagObject(cdt, cdo);
	}

	cdt->style = 0;
	browseLocal = save_local;
		cf = save_cf;
	return true;
}

static bool remember_contracted;

static duk_ret_t nat_unframe(duk_context * cx)
{
	if (duk_is_object(cx, 0)) {
		jsobjtype fobj = duk_get_heapptr(cx, 0);
		int i, n;
		Tag *t, *cdt;
		Frame *f, *f1;
		t = tagFromObject(fobj);
		if (!t) {
			debugPrint(1, "unframe couldn't find tag");
			goto done;
		}
		                if (!(cdt = t->firstchild) || cdt->action != TAGACT_DOC) {
			                        debugPrint(1, "unframe child tag isn't right");
			goto done;
		}
		underKill(cdt);
		disconnectTagObject(cdt);
		f1 = t->f1;
		t->f1 = 0;
		remember_contracted = t->contracted;
		if (f1 == cf) {
			debugPrint(1, "deleting the current frame, this shouldn't happen");
			goto done;
		}
		for (f = &(cw->f0); f; f = f->next)
			if (f->next == f1)
				break;
		if (!f) {
			debugPrint(1, "unframe can't find prior frame to relink");
			goto done;
		}
		f->next = f1->next;
		delTimers(f1);
		freeJSContext(f1);
		nzFree(f1->dw);
		nzFree(f1->hbase);
		nzFree(f1->fileName);
		nzFree(f1->firstURL);
		free(f1);
	// cdt use to belong to f1, which no longer exists.
		cdt->f0 = f;		// back to its parent frame
	// A running frame could create nodes in its parent frame, or any other frame.
		n = 0;
		for (i = 0; i < cw->numTags; ++i) {
			t = tagList[i];
			if (t->f0 == f1)
				t->f0 = f, ++n;
		}
		if (n)
			debugPrint(3, "%d nodes pushed up to the parent frame", n);
	}
done:
	duk_pop(cx);
	return 0;
}

static duk_ret_t nat_unframe2(duk_context * cx)
{
	if (duk_is_object(cx, 0)) {
		jsobjtype fobj = duk_get_heapptr(cx, 0);
		Tag *t = tagFromObject(fobj);
		if(t)
			t->contracted = remember_contracted;
	}
	duk_pop(cx);
	return 0;
}

// If we stay with duktape, optimize this routine with seqno and gsn,
// the way I did in the mozilla version.
static Tag *tagFromObject(jsobjtype v)
{
	Tag *t = 0;
	int i;
	if (!tagList)
		i_printfExit(MSG_NullListInform);
	if(!v) {
		debugPrint(1, "tagFromObject(null)");
		return 0;
	}
	for (i = 0; i < cw->numTags; ++i) {
		t = tagList[i];
		if (t->jv == v && !t->dead)
			return t;
	}
	debugPrint(1, "tagFromObject() returns null");
	return 0;
}

// Create a new tag for this pointer, only from document.createElement().
static Tag *tagFromObject2(jsobjtype v, const char *tagname)
{
	Tag *t;
	if (!tagname)
		return 0;
	t = newTag(cf, tagname);
	if (!t) {
		debugPrint(3, "cannot create tag node %s", tagname);
		return 0;
	}
	connectTagObject(t, v);
/* this node now has a js object, don't decorate it again. */
	t->step = 2;
/* and don't render it unless it is linked into the active tree */
	t->deleted = true;
	return t;
}				/* tagFromObject2 */

static void domSetsLinkage(bool after, char type, jsobjtype p_j, const char *rest)
{
	Tag *parent, *add, *before, *c, *t;
	jsobjtype *a_j, *b_j;
	jsobjtype cx;
	char p_name[MAXTAGNAME], a_name[MAXTAGNAME], b_name[MAXTAGNAME];
	int action;
	char *jst;		// javascript string

// Some functions in third.js create, link, and then remove nodes, before
// there is a document. Don't run any side effects in this case.
	if (!cw->tags)
		return;

	sscanf(rest, "%s %p,%s %p,%s ", p_name, &a_j, a_name, &b_j, b_name);
	if (type == 'c') {	/* create */
		parent = tagFromObject2(p_j, p_name);
		if (parent) {
			debugPrint(4, "linkage, %s %d created",
				   p_name, parent->seqno);
			if (parent->action == TAGACT_INPUT) {
// we need to establish the getter and setter for value
				set_property_string_0(parent->f0->cx,
				parent->jv, "value", emptyString);
			}
		}
		return;
	}

	parent = tagFromObject(p_j);
/* options are relinked by rebuildSelectors, not here. */
	if (stringEqual(p_name, "option"))
		return;

	if (stringEqual(a_name, "option"))
		return;

	add = tagFromObject(a_j);
	if (!parent || !add)
		return;

	if (type == 'r') {
/* add is a misnomer here, it's being removed */
		add->deleted = true;
		debugPrint(4, "linkage, %s %d removed from %s %d",
			   a_name, add->seqno, p_name, parent->seqno);
		add->parent = NULL;
		if (parent->firstchild == add)
			parent->firstchild = add->sibling;
		else {
			c = parent->firstchild;
			if (c) {
				for (; c->sibling; c = c->sibling) {
					if (c->sibling != add)
						continue;
					c->sibling = add->sibling;
					break;
				}
			}
		}
		add->sibling = NULL;
		return;
	}

/* check and see if this link would turn the tree into a circle, whence
 * any subsequent traversal would fall into an infinite loop.
 * Child node must not have a parent, and, must not link into itself.
 * Oddly enough the latter seems to happen on acid3.acidtests.org,
 * linking body into body, and body at the top has no parent,
 * so passes the "no parent" test, whereupon I had to add the second test. */
	if (add->parent || add == parent) {
		if (debugLevel >= 3) {
			debugPrint(3,
				   "linkage cycle, cannot link %s %d into %s %d",
				   a_name, add->seqno, p_name, parent->seqno);
			if (type == 'b') {
				before = tagFromObject(b_j);
				debugPrint(3, "before %s %d", b_name,
					   (before ? before->seqno : -1));
			}
			if (add->parent)
				debugPrint(3,
					   "the child already has parent %s %d",
					   add->parent->info->name,
					   add->parent->seqno);
			debugPrint(3,
				   "Aborting the link, some data may not be rendered.");
		}
		return;
	}

	if (type == 'b') {	/* insertBefore */
		before = tagFromObject(b_j);
		if (!before)
			return;
		debugPrint(4, "linkage, %s %d linked into %s %d before %s %d",
			   a_name, add->seqno, p_name, parent->seqno,
			   b_name, before->seqno);
		c = parent->firstchild;
		if (!c)
			return;
		if (c == before) {
			parent->firstchild = add;
			add->sibling = before;
			goto ab;
		}
		while (c->sibling && c->sibling != before)
			c = c->sibling;
		if (!c->sibling)
			return;
		c->sibling = add;
		add->sibling = before;
		goto ab;
	}

/* type = a, appendchild */
	debugPrint(4, "linkage, %s %d linked into %s %d",
		   a_name, add->seqno, p_name, parent->seqno);
	if (!parent->firstchild)
		parent->firstchild = add;
	else {
		c = parent->firstchild;
		while (c->sibling)
			c = c->sibling;
		c->sibling = add;
	}

ab:
	add->parent = parent;
	add->deleted = false;

	t = add;
	debugPrint(4, "fixup %s %d", a_name, t->seqno);
	action = t->action;
	cx = t->f0->cx;
	t->name = get_property_string_0(cx, t->jv, "name");
	t->id = get_property_string_0(cx, t->jv, "id");
	t->jclass = get_property_string_t(t, "class");

	switch (action) {
	case TAGACT_INPUT:
		jst = get_property_string_t(t, "type");
		setTagAttr(t, "type", jst);
		t->value = get_property_string_t(t, "value");
		htmlInputHelper(t);
		break;

	case TAGACT_OPTION:
		if (!t->value)
			t->value = emptyString;
		if (!t->textval)
			t->textval = emptyString;
		break;

	case TAGACT_TA:
		t->action = TAGACT_INPUT;
		t->itype = INP_TA;
		t->value = get_property_string_t(t, "value");
		if (!t->value)
			t->value = emptyString;
// Need to create the side buffer here.
		formControl(t, true);
		break;

	case TAGACT_SELECT:
		t->action = TAGACT_INPUT;
		t->itype = INP_SELECT;
		if (typeof_property_0(cx, t->jv, "multiple"))
			t->multiple = true;
		formControl(t, true);
		break;

	case TAGACT_TR:
		t->controller = findOpenTag(t, TAGACT_TABLE);
		break;

	case TAGACT_TD:
		t->controller = findOpenTag(t, TAGACT_TR);
		break;

	}			/* switch */
}

static void linkageNow(duk_context * cx, char linkmode, jsobjtype o)
{
	jsInterruptCheck(cx);
	debugPrint(4, "linkset %s", effects + 2);
	domSetsLinkage(false, linkmode, o, strchr(effects, ',') + 1);
	nzFree(effects);
	effects = initString(&eff_l);
}

static duk_ret_t nat_log_element(duk_context * cx)
{
	jsobjtype newobj = duk_get_heapptr(cx, -2);
	const char *tag = duk_get_string(cx, -1);
	char e[60];
	if (!newobj || !tag)
		return 0;
	debugPrint(5, "log in");
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
	debugPrint(5, "log out");
	return 0;
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

	debugPrint(5, "timer in");
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
			duk_push_c_function(cx, nat_void, 0);
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
	duk_push_number(cx, ++timer_sn);
	duk_put_prop_string(cx, -2, "tsn");
// leaves just the timer object on the stack, which is what we want.

	domSetsTimeout(n, fname, fpn, isInterval);

done:
	debugPrint(5, "timer out");
}

static duk_ret_t nat_setTimeout(duk_context * cx)
{
	set_timeout(cx, false);
	return 1;
}

static duk_ret_t nat_setInterval(duk_context * cx)
{
	set_timeout(cx, true);
	return 1;
}

static duk_ret_t nat_clearTimeout(duk_context * cx)
{
	int tsn;
	char *fpn; // fake prop name
	jsobjtype obj = duk_get_heapptr(cx, 0);
	if (!obj)
		return 0;
	tsn = get_property_number_0(cx, obj, "tsn");
	fpn = get_property_string_0(cx, obj, "backlink");
	domSetsTimeout(tsn, "-", fpn, false);
	nzFree(fpn);
	return 0;
}

static duk_ret_t nat_win_close(duk_context * cx)
{
	i_puts(MSG_PageDone);
// I should probably freeJSContext and close down javascript,
// but not sure I can do that while the js function is still running.
	return 0;
}

// find the frame, in the current window, that goes with this.
// Used by document.write to put the html in the right frame.
static Frame *doc2frame(duk_context * cx)
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
	f = doc2frame(cx);
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

static duk_ret_t nat_doc_write(duk_context * cx)
{
	dwrite(cx, false);
	return 0;
}

static duk_ret_t nat_doc_writeln(duk_context * cx)
{
	dwrite(cx, true);
	return 0;
}

static Frame *win2frame(duk_context * cx)
{
	jsobjtype thisobj;
	Frame *f;
	duk_push_this(cx);
	thisobj = duk_get_heapptr(cx, -1);
	duk_pop(cx);
	for (f = &(cw->f0); f; f = f->next) {
		if (f->winobj == thisobj)
			break;
	}
	return f;
}

static duk_ret_t nat_parent(duk_context * cx)
{
	Frame *current = win2frame(cx);
	if(!current) {
		duk_push_undefined(cx);
		return 1;
	}
if(current == &(cw->f0)) {
		duk_push_heapptr(cx, current->winobj);
		return 1;
	}
	if(!current->frametag) // should not happen
		duk_push_undefined(cx);
	else
		duk_push_heapptr(cx, current->frametag->f0->winobj);
	return 1;
}

static duk_ret_t nat_fe(duk_context * cx)
{
	Frame *current = win2frame(cx);
	if(!current || current == &(cw->f0) || !current->frametag) {
		duk_push_undefined(cx);
		return 1;
	}
	duk_push_heapptr(cx, current->frametag->jv);
	return 1;
}

static duk_ret_t nat_top(duk_context * cx)
{
	duk_push_heapptr(cx, cw->f0.winobj);
	return 1;
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

	debugPrint(5, "append in");
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
	debugPrint(5, "append out");
}

static duk_ret_t nat_apch1(duk_context * cx)
{
	append0(cx, false);
	return 1;
}

static duk_ret_t nat_apch2(duk_context * cx)
{
	append0(cx, true);
	return 1;
}

static duk_ret_t nat_insbf(duk_context * cx)
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

	debugPrint(5, "before in");
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
	debugPrint(5, "before out");
	return 1;
}

static duk_ret_t nat_removeChild(duk_context * cx)
{
	unsigned i, length;
	int mark;
	jsobjtype child, thisobj, h;
	char *e;
	const char *thisname, *childname;

	debugPrint(5, "remove in");
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

	debugPrint(5, "remove out");
// mutation fix up from native code
	duk_push_heapptr(cx, cf->winobj);
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
	debugPrint(5, "remove fail");
	duk_pop(cx);
	duk_push_null(cx);
	return 1;
}

static duk_ret_t nat_fetchHTTP(duk_context * cx)
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

	debugPrint(5, "xhr in");
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

	debugPrint(5, "xhr out");
	return 1;
}

static duk_ret_t nat_resolveURL(duk_context * cx)
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

static duk_ret_t nat_formSubmit(duk_context * cx)
{
Tag *t;
	jsobjtype thisobj;
	duk_push_this(cx);
	thisobj = duk_get_heapptr(cx, -1);
t = tagFromObject(thisobj);
	duk_pop(cx);
	if(t && t->action == TAGACT_FORM) {
		debugPrint(3, "submit form tag %d", t->seqno);
		domSubmitsForm(t, false);
	} else {
		debugPrint(3, "submit form tag not found");
	}
	return 0;
}

static duk_ret_t nat_formReset(duk_context * cx)
{
	jsobjtype thisobj;
	Tag *t;
	duk_push_this(cx);
	thisobj = duk_get_heapptr(cx, -1);
t = tagFromObject(thisobj);
	duk_pop(cx);
	if(t && t->action == TAGACT_FORM) {
		debugPrint(3, "reset form tag %d", t->seqno);
		domSubmitsForm(t, true);
	} else {
		debugPrint(3, "reset form tag not found");
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

static duk_ret_t nat_getcook(duk_context * cx)
{
	startCookie();
	duk_push_string(cx, cookieCopy + 2);
	return 1;
}

static duk_ret_t nat_setcook(duk_context * cx)
{
	const char *newcook = duk_get_string(cx, 0);
	debugPrint(5, "cook in");
	if (newcook) {
		const char *s = strchr(newcook, '=');
		if(s && s > newcook) {
			duk_get_global_string(cx, "eb$url");
			receiveCookie(duk_get_string(cx, -1), newcook);
			duk_pop(cx);
		}
	}
	debugPrint(5, "cook out");
	return 0;
}

static duk_ret_t nat_css_start(duk_context * cx)
{
// The selection string has to be allocated - css will use it in place,
// then free it later.
	cssDocLoad(duk_get_number(cx, 0), cloneString(duk_get_string(cx, 1)),
		   duk_get_boolean(cx, 2));
	return 0;
}

// turn an array of html tags into an array of objects.
// Leave the array on the duktape stack.
static void objectize(duk_context *cx, Tag **tlist)
{
	int i, j;
	const Tag *t;
	        duk_get_global_string(cx, "Array");
	duk_new(cx, 0);
	if(!tlist)
		return;
	for (i = j = 0; (t = tlist[i]); ++i) {
		if (!t->jslink)	// should never happen
			continue;
		duk_push_heapptr(cx, t->jv);
		duk_put_prop_index(cx, -2, j);
		++j;
	}
}

// Turn start into a tag, or 0 if start is doc or win for the current frame.
// Return false if we can't turn it into a tag within the current window.
static bool rootTag(jsobjtype start, Tag **tp)
{
	Tag *t;
	*tp = 0;
	if(!start || start == cf->winobj || start == cf->docobj)
		return true;
	t = tagFromObject(start);
	if(!t)
		return false;
	*tp = t;
	return true;
}

// querySelectorAll
static duk_ret_t nat_qsa(duk_context * cx)
{
	jsobjtype start = 0;
	Tag **tlist, *t;
	const char *selstring = duk_get_string(cx, 0);
	int top = duk_get_top(cx);
	if (top > 2) {
		duk_pop_n(cx, top - 2);
		top = 2;
	}
	if (top == 2) {
		if (duk_is_object(cx, 1))
			start = duk_get_heapptr(cx, 1);
	}
	if (!start) {
		duk_push_this(cx);
		start = duk_get_heapptr(cx, -1);
		duk_pop(cx);
	}
	jsInterruptCheck(cx);
	if(!rootTag(start, &t)) {
		duk_pop_n(cx, top);
		objectize(cx, 0);
		return 1;
	}
	tlist = querySelectorAll(selstring, t);
	duk_pop_n(cx, top);
	objectize(cx, tlist);
	nzFree(tlist);
	return 1;
}

// querySelector
static duk_ret_t nat_qs(duk_context * cx)
{
	jsobjtype start = 0;
	Tag *t;
	const char *selstring = duk_get_string(cx, 0);
	int top = duk_get_top(cx);
	if (top > 2) {
		duk_pop_n(cx, top - 2);
		top = 2;
	}
	if (top == 2) {
		if (duk_is_object(cx, 1))
			start = duk_get_heapptr(cx, 1);
	}
	if (!start) {
		duk_push_this(cx);
		start = duk_get_heapptr(cx, -1);
		duk_pop(cx);
	}
	jsInterruptCheck(cx);
	if(!rootTag(start, &t)) {
		duk_pop_n(cx, top);
		duk_push_undefined(cx);
		return 1;
	}
	t = querySelector(selstring, t);
	duk_pop_n(cx, top);
	if(t && t->jslink)
		duk_push_heapptr(cx, t->jv);
	else
		duk_push_undefined(cx);
	return 1;
}

// querySelector0
static duk_ret_t nat_qs0(duk_context * cx)
{
	jsobjtype start;
	Tag *t;
	bool rc;
	const char *selstring = duk_get_string(cx, 0);
	duk_push_this(cx);
	start = duk_get_heapptr(cx, -1);
	duk_pop(cx);
	jsInterruptCheck(cx);
	if(!rootTag(start, &t)) {
		duk_pop(cx);
		duk_push_false(cx);
		return 1;
	}
	rc = querySelector0(selstring, t);
	duk_pop(cx);
	duk_push_boolean(cx, rc);
	return 1;
}

static duk_ret_t nat_cssApply(duk_context * cx)
{
	jsobjtype node;
	Tag *t;
	jsInterruptCheck(cx);
	node = duk_get_heapptr(cx, 1);
	t = tagFromObject(node);
	if(t)
		cssApply(duk_get_number(cx, 0), t);
	else
		debugPrint(3, "eb$cssApply is passed an object that does not correspond to an html tag");
	duk_pop_2(cx);
	return 0;
}

static duk_ret_t nat_cssText(duk_context * cx)
{
	const char *rulestring = duk_get_string(cx, 0);
	cssText(rulestring);
	duk_pop(cx);
	return 0;
}

/*********************************************************************
This is a wrapper around document.method, mostly, but also node.method
and mabye even window.method. It is unnecessary in duktape.
It is needed in mozilla, so I include it here for consistency.
If you want to grok it's purpose, read the comments in jseng-moz.cpp.
Although it's not needed here, it might not be a bad idea anyways.
subframe.document.createElement creates a new tag, and t->f0 is set to the
current frame, and shouldn't that current frame be the subframe?
So shouldn't we use these wrappers to reset cf?
*********************************************************************/

static Frame *thisFrame(duk_context *cx, const char *whence)
{
	Frame *f;
	Tag *t;
	jsobjtype thisobj;
	duk_push_this(cx);
	if(duk_has_prop_string(cx, -1, "eb$ctx")) {
		bool d = duk_has_prop_string(cx, -1, "eb$seqno");
		f = d ? doc2frame(cx) : win2frame(cx);
		duk_pop(cx);
		if(!f) {
			debugPrint(3, "cannot connect %s to its frame",
			(d ? "document" : "window"));
			f = cf;
		}
		if(f != cf)
			debugPrint(4, "%s frame %d>%d", whence, cf->gsn, f->gsn);
		return f;
	}
// better be associated with a tag
	thisobj = duk_get_heapptr(cx, -1);
	duk_pop(cx);
	if((t = tagFromObject(thisobj))) {
		if(t->f0 != cf)
			debugPrint(4, "%s frame %d>%d", whence, cf->gsn, t->f0->gsn);
		return t->f0;
	}
	debugPrint(3, "cannot connect node.method to its frame");
	return cf;
}

// not used by duktape, yet, so ignore the compiler warning.
static void docWrap(duk_context *cx, const char *fn)
{
	int top = duk_get_top(cx);
	Frame *save_cf = cf;
cf = thisFrame(cx, fn);
	duk_push_this(cx);
	duk_get_prop_string(cx, -1, fn);
duk_insert(cx, 0);
	duk_insert(cx, 1);
	if (!duk_pcall_method(cx, top)) {
// the return, if you care about it, is left on the stack
		cf = save_cf;
		return;
	}
	if (intFlag)
		i_puts(MSG_Interrupted);
	processError(cx);
	duk_push_undefined(cx);
	cf = save_cf;
}

static void createJSContext_0(Frame *f)
{
	duk_context * cx;
	duk_push_thread_new_globalenv(context0);
	cx = f->cx = duk_get_context(context0, -1);
	if (!cx)
		return;
	debugPrint(3, "create js context %d", f->gsn);
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
	duk_push_c_function(cx, nat_new_location, 1);
	duk_put_global_string(cx, "eb$newLocation");
	duk_push_c_function(cx, nat_mywin, 0);
	duk_put_global_string(cx, "my$win");
	duk_push_c_function(cx, nat_mydoc, 0);
	duk_put_global_string(cx, "my$doc");
	duk_push_c_function(cx, nat_puts, 1);
	duk_put_global_string(cx, "eb$puts");
	duk_push_c_function(cx, nat_wlf, 2);
	duk_put_global_string(cx, "eb$wlf");
	duk_push_c_function(cx, nat_media, 1);
	duk_put_global_string(cx, "eb$media");
	duk_push_c_function(cx, nat_btoa, 1);
	duk_put_global_string(cx, "btoa");
	duk_push_c_function(cx, nat_atob, 1);
	duk_put_global_string(cx, "atob");
	duk_push_c_function(cx, nat_void, 0);
	duk_put_global_string(cx, "eb$voidfunction");
	duk_push_c_function(cx, nat_null, 0);
	duk_put_global_string(cx, "eb$nullfunction");
	duk_push_c_function(cx, nat_true, 0);
	duk_put_global_string(cx, "eb$truefunction");
	duk_push_c_function(cx, nat_false, 0);
	duk_put_global_string(cx, "eb$falsefunction");
	duk_push_c_function(cx, nat_void, 0);
	duk_put_global_string(cx, "scroll");
	duk_push_c_function(cx, nat_void, 0);
	duk_put_global_string(cx, "scrollTo");
	duk_push_c_function(cx, nat_void, 0);
	duk_put_global_string(cx, "scrollBy");
	duk_push_c_function(cx, nat_void, 0);
	duk_put_global_string(cx, "scrollByLines");
	duk_push_c_function(cx, nat_void, 0);
	duk_put_global_string(cx, "scrollByPages");
	duk_push_c_function(cx, nat_void, 0);
	duk_put_global_string(cx, "focus");
	duk_push_c_function(cx, nat_void, 0);
	duk_put_global_string(cx, "blur");
	duk_push_c_function(cx, nat_unframe, 1);
	duk_put_global_string(cx, "eb$unframe");
	duk_push_c_function(cx, nat_unframe2, 1);
	duk_put_global_string(cx, "eb$unframe2");
	duk_push_c_function(cx, nat_logputs, 2);
	duk_put_global_string(cx, "eb$logputs");
	duk_push_c_function(cx, nat_prompt, DUK_VARARGS);
	duk_put_global_string(cx, "prompt");
	duk_push_c_function(cx, nat_confirm, 1);
	duk_put_global_string(cx, "confirm");
	duk_push_c_function(cx, nat_log_element, 2);
	duk_put_global_string(cx, "eb$logElement");
	duk_push_c_function(cx, nat_setTimeout, DUK_VARARGS);
	duk_put_global_string(cx, "setTimeout");
	duk_push_c_function(cx, nat_setInterval, DUK_VARARGS);
	duk_put_global_string(cx, "setInterval");
	duk_push_c_function(cx, nat_clearTimeout, 1);
	duk_put_global_string(cx, "clearTimeout");
	duk_push_c_function(cx, nat_clearTimeout, 1);
	duk_put_global_string(cx, "clearInterval");
	duk_push_c_function(cx, nat_win_close, 0);
	duk_put_global_string(cx, "close");
	duk_push_c_function(cx, nat_fetchHTTP, 4);
	duk_put_global_string(cx, "eb$fetchHTTP");
	duk_push_c_function(cx, nat_parent, 0);
	duk_put_global_string(cx, "eb$parent");
	duk_push_c_function(cx, nat_top, 0);
	duk_put_global_string(cx, "eb$top");
	duk_push_c_function(cx, nat_fe, 0);
	duk_put_global_string(cx, "eb$frameElement");
	duk_push_c_function(cx, nat_resolveURL, 2);
	duk_put_global_string(cx, "eb$resolveURL");
	duk_push_c_function(cx, nat_formSubmit, 0);
	duk_put_global_string(cx, "eb$formSubmit");
	duk_push_c_function(cx, nat_formReset, 0);
	duk_put_global_string(cx, "eb$formReset");
	duk_push_c_function(cx, nat_getcook, 0);
	duk_put_global_string(cx, "eb$getcook");
	duk_push_c_function(cx, nat_setcook, 1);
	duk_put_global_string(cx, "eb$setcook");
	duk_push_c_function(cx, getter_cd, 0);
	duk_put_global_string(cx, "eb$getter_cd");
	duk_push_c_function(cx, getter_cw, 0);
	duk_put_global_string(cx, "eb$getter_cw");
	duk_push_c_function(cx, nat_css_start, 3);
	duk_put_global_string(cx, "eb$cssDocLoad");
	duk_push_c_function(cx, nat_qsa, DUK_VARARGS);
	duk_put_global_string(cx, "querySelectorAll");
	duk_push_c_function(cx, nat_qs, DUK_VARARGS);
	duk_put_global_string(cx, "querySelector");
	duk_push_c_function(cx, nat_qs0, 1);
	duk_put_global_string(cx, "querySelector0");
	duk_push_c_function(cx, nat_cssApply, 2);
	duk_put_global_string(cx, "eb$cssApply");
	duk_push_c_function(cx, nat_cssText, 1);
	duk_put_global_string(cx, "eb$cssText");

	duk_push_heapptr(cx, f->docobj);	// native document methods

	duk_push_c_function(cx, nat_hasfocus, 0);
	duk_put_prop_string(cx, -2, "hasFocus");
	duk_push_c_function(cx, nat_doc_write, DUK_VARARGS);
	duk_put_prop_string(cx, -2, "write");
	duk_push_c_function(cx, nat_doc_writeln, DUK_VARARGS);
	duk_put_prop_string(cx, -2, "writeln");
	duk_push_c_function(cx, nat_apch1, 1);
	duk_put_prop_string(cx, -2, "eb$apch1");
	duk_push_c_function(cx, nat_apch2, 1);
	duk_put_prop_string(cx, -2, "eb$apch2");
	duk_push_c_function(cx, nat_insbf, 2);
	duk_put_prop_string(cx, -2, "eb$insbf");
	duk_push_c_function(cx, nat_removeChild, 1);
	duk_put_prop_string(cx, -2, "removeChild");
	duk_push_c_function(cx, nat_void, 0);
	duk_put_prop_string(cx, -2, "focus");
	duk_push_c_function(cx, nat_void, 0);
	duk_put_prop_string(cx, -2, "blur");
	duk_push_c_function(cx, nat_void, 0);
	duk_put_prop_string(cx, -2, "close");

// document.eb$ctx is the context number
	duk_push_string(cx, "eb$ctx");
	duk_push_number(cx, f->gsn);
	duk_def_prop(cx, 0,
		     (DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_SET_ENUMERABLE |
		      DUK_DEFPROP_CLEAR_WRITABLE |
		      DUK_DEFPROP_CLEAR_CONFIGURABLE));
// document.eb$seqno = 0
	duk_push_string(cx, "eb$seqno");
	duk_push_number(cx, 0);
	duk_def_prop(cx, 0,
		     (DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_SET_ENUMERABLE |
		      DUK_DEFPROP_CLEAR_WRITABLE |
		      DUK_DEFPROP_CLEAR_CONFIGURABLE));

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

// Sequence is to set f->fileName, then createContext(), so for a short time,
// we can rely on that variable.
// Let's make it more permanent, per context.
// Has to be nonwritable for security reasons.
	duk_push_string(cx, "eb$url");
	duk_push_string(cx, f->fileName);
	duk_def_prop(cx, 0,
		     (DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_SET_ENUMERABLE |
		      DUK_DEFPROP_CLEAR_WRITABLE |
		      DUK_DEFPROP_CLEAR_CONFIGURABLE));
	duk_push_string(cx, "eb$ctx");
	duk_push_number(cx, f->gsn);
	duk_def_prop(cx, 0,
		     (DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_SET_ENUMERABLE |
		      DUK_DEFPROP_CLEAR_WRITABLE |
		      DUK_DEFPROP_CLEAR_CONFIGURABLE));

	duk_pop(cx);
}

static void setup_window_2(void);
static bool js_running;
void createJSContext(Frame *f)
{
	if (!allowJS)
		return;
	if (!js_running) {
		if (js_main())
			i_puts(MSG_JSEngineRun);
		else
			js_running = true;
	}
	createJSContext_0(f);
	if (f->cx) {
		f->jslink = true;
		setup_window_2();
	} else {
		i_puts(MSG_JavaContextError);
	}
}

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

static void setup_window_2(void)
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
	extern const char startWindowJS[];
	extern const char thirdJS[];

	set_property_object_0(cx, w, "window", w);

/* the js window/document setup script.
 * These are all the things that do not depend on the platform,
 * OS, configurations, etc. */
	jsRunScriptWin(startWindowJS, "StartWindow", 1);
	jsRunScriptWin(thirdJS, "Third", 1);

	nav = get_property_object_0(cx, w, "navigator");
	if (nav == NULL)
		return;
/* some of the navigator is in startwindow.js; the runtime properties are here. */
	set_property_string_0(cx, nav, "userLanguage", supported_languages[eb_lang]);
	set_property_string_0(cx, nav, "language", supported_languages[eb_lang]);
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
	jsRunScriptWin(
		    "window.location.replace = document.location.replace = function(s) { this.href = s; };Object.defineProperty(window.location,'replace',{enumerable:false});Object.defineProperty(document.location,'replace',{enumerable:false});",
		    "locreplace", 1);
	set_property_string_0(cx, d, "domain", getHostURL(cf->fileName));
// These calls are redundent unless this is the first window
	if (debugClone)
		set_property_bool_0(cx, w, "cloneDebug", true);
	if (debugEvent)
		set_property_bool_0(cx, w, "eventDebug", true);
	if (debugThrow)
		set_property_bool_0(cx, w, "throwDebug", true);
}

static void freeJSContext_0(Frame *f)
{
	duk_context *cx = f->cx;
	int i, top = duk_get_top(context0);
	for (i = 0; i < top; ++i) {
		if (cx == duk_get_context(context0, i)) {
			duk_remove(context0, i);
			debugPrint(3, "remove js context %d", f->gsn);
			break;
		}
	}
}

void freeJSContext(Frame *f)
{
	if (!f->jslink)
		return;
	freeJSContext_0(f);
	f->cx = f->winobj = f->docobj = 0;
	f->jslink = false;
	cssFree(f);
}

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
}

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

static enum ej_proptype typeof_property_0(jsobjtype cx0, jsobjtype parent, const char *name)
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

static bool has_property_0(jsobjtype cx0, jsobjtype parent, const char *name)
{
	duk_context * cx = cx0;
	bool l;
	duk_push_heapptr(cx, parent);
	l = duk_has_prop_string(cx, -1, name);
	duk_pop(cx);
	return l;
}

bool has_property_t(const Tag *t, const char *name)
{
	if(!t->jslink || !allowJS)
		return false;
	return has_property_0(t->f0->cx, t->jv, name);
}

bool has_property_win(const Frame *f, const char *name)
{
	if(!f->jslink || !allowJS)
		return false;
	return has_property_0(f->cx, f->winobj, name);
}

static void delete_property_0(jsobjtype cx0, jsobjtype parent, const char *name)
{
	duk_context * cx = cx0;
	duk_push_heapptr(cx, parent);
	duk_del_prop_string(cx, -1, name);
	duk_pop(cx);
}

static int get_arraylength_0(jsobjtype cx0, jsobjtype a)
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
}

/* Return a property as a string, if it is
 * string compatible. The string is allocated, free it when done. */
static char *get_property_string_0(jsobjtype cx0, jsobjtype parent, const char *name)
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
}

/* Get the url from a url object, special wrapper.
 * Owner object is passed, look for obj.href, obj.src, or obj.action.
 * Return that if it's a string, or its member href if it is a url.
 * The result, coming from get_property_string, is allocated. */
static char *get_property_url_0(jsobjtype cx, jsobjtype owner, bool action)
{
	enum ej_proptype mtype;	/* member type */
	jsobjtype uo = 0;	/* url object */
	if (action) {
		mtype = typeof_property_0(cx, owner, "action");
		if (mtype == EJ_PROP_STRING)
			return get_property_string_0(cx, owner, "action");
		if (mtype != EJ_PROP_OBJECT)
			return 0;
		uo = get_property_object_0(cx, owner, "action");
	} else {
		mtype = typeof_property_0(cx, owner, "href");
		if (mtype == EJ_PROP_STRING)
			return get_property_string_0(cx, owner, "href");
		if (mtype == EJ_PROP_OBJECT)
			uo = get_property_object_0(cx, owner, "href");
		else if (mtype)
			return 0;
		if (!uo) {
			mtype = typeof_property_0(cx, owner, "src");
			if (mtype == EJ_PROP_STRING)
				return get_property_string_0(cx, owner, "src");
			if (mtype == EJ_PROP_OBJECT)
				uo = get_property_object_0(cx, owner, "src");
		}
	}
	if (uo == NULL)
		return 0;
/* should this be href$val? */
	return get_property_string_0(cx, uo, "href");
}

static jsobjtype get_property_object_0(jsobjtype cx0, jsobjtype parent, const char *name)
{
	duk_context * cx = cx0;
	jsobjtype o = NULL;
	duk_push_heapptr(cx, parent);
	duk_get_prop_string(cx, -1, name);
	if (duk_is_object(cx, -1))
		o = duk_get_heapptr(cx, -1);
	duk_pop_2(cx);
	return o;
}

#if 0
static jsobjtype get_property_function_0(jsobjtype cx0, jsobjtype parent, const char *name)
{
	duk_context * cx = cx0;
	jsobjtype o = NULL;
	duk_push_heapptr(cx, parent);
	duk_get_prop_string(cx, -1, name);
	if (duk_is_function(cx, -1))
		o = duk_get_heapptr(cx, -1);
	duk_pop_2(cx);
	return o;
}
#endif

static int get_property_number_0(jsobjtype cx0, jsobjtype parent, const char *name)
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
}

#if 0
static double get_property_float_0(jsobjtype cx0, jsobjtype parent, const char *name)
{
	duk_context * cx = cx0;
	double d = -1;
	duk_push_heapptr(cx, parent);
	duk_get_prop_string(cx, -1, name);
	if (duk_is_number(cx, -1))
		d = duk_get_number(cx, -1);
	duk_pop_2(cx);
	return d;
}
#endif

static bool get_property_bool_0(jsobjtype cx0, jsobjtype parent, const char *name)
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
}

static int set_property_string_0(jsobjtype cx0, jsobjtype parent, const char *name,
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
}

static int set_property_bool_0(jsobjtype cx0, jsobjtype parent, const char *name, bool n)
{
	duk_context * cx = cx0;
	duk_push_heapptr(cx, parent);
	duk_push_boolean(cx, n);
	duk_put_prop_string(cx, -2, name);
	duk_pop(cx);
	return 0;
}

static int set_property_number_0(jsobjtype cx0, jsobjtype parent, const char *name, int n)
{
	duk_context * cx = cx0;
	duk_push_heapptr(cx, parent);
	duk_push_int(cx, n);
	duk_put_prop_string(cx, -2, name);
	duk_pop(cx);
	return 0;
}

#if 0
static int set_property_float_0(jsobjtype cx0, jsobjtype parent, const char *name, double n)
{
	duk_context * cx = cx0;
	duk_push_heapptr(cx, parent);
	duk_push_number(cx, n);
	duk_put_prop_string(cx, -2, name);
	duk_pop(cx);
	return 0;
}
#endif

static int set_property_object_0(jsobjtype cx0, jsobjtype parent, const char *name, jsobjtype child)
{
	duk_context * cx = cx0;
	duk_push_heapptr(cx, parent);
	duk_push_heapptr(cx, child);
	duk_put_prop_string(cx, -2, name);
	duk_pop(cx);
	return 0;
}

void set_property_object_t(const Tag *t, const char *name, const Tag *t2)
{
	if (!allowJS || !t->jslink || !t2->jslink)
		return;
	set_property_object_0(t->f0->cx, t->jv, name, t2->jv);
}

#if 0
static duk_ret_t nat_error_stub_1(duk_context * cx)
{
	i_puts(MSG_CompileError);
	return 0;
}

// handler.toString = function() { return this.body; }
static duk_ret_t nat_fntos(duk_context * cx)
{
	duk_push_this(cx);
	duk_get_prop_string(cx, -1, "body");
	duk_remove(cx, -2);
	return 1;
}

static int set_property_function_0(jsobjtype cx0, jsobjtype parent, const char *name,
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
		duk_push_c_function(cx, nat_error_stub_1, 0);
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
	duk_push_c_function(cx, nat_fntos, 0);
	duk_put_prop_string(cx, -2, "toString");
	duk_push_heapptr(cx, parent);
	duk_insert(cx, -2);	// switch places
	duk_put_prop_string(cx, -2, name);
	duk_pop(cx);
	return 0;
}
#endif

/*********************************************************************
Error object is at the top of the duktape stack.
Extract the line number, call stack, and error message,
the latter being error.toString().
Print the message if debugLevel is 3 or higher.
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

static bool run_function_bool_0(jsobjtype cx0, jsobjtype parent, const char *name)
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
		debugPrint(3, "no such function %s", name);
		duk_pop_2(cx);
		return false;
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
	return false;
}

bool run_function_bool_t(const Tag *t, const char *name)
{
	if (!allowJS || !t->jslink)
		return false;
	return run_function_bool_0(t->f0->cx, t->jv, name);
}

bool run_function_bool_win(const Frame *f, const char *name)
{
	if (!allowJS || !f->jslink)
		return false;
	return run_function_bool_0(f->cx, f->winobj, name);
}

void run_ontimer(const Frame *f, const char *backlink)
{
	jsobjtype cx = f->cx;
	jsobjtype to; // timer object
	if(!duk_get_global_string(cx, backlink)) {
  debugPrint(3, "could not find timer backlink %s", backlink);
		duk_pop(cx);
		return;
	}
	to = duk_get_heapptr(cx, -1);
	run_event_0(cx, to, "timer", "ontimer");
	duk_pop(cx);
}

// The single argument to the function has to be an object.
// Returns -1 if the return is not int or bool
static int run_function_onearg_0(jsobjtype cx0, jsobjtype parent, const char *name, jsobjtype child)
{
	duk_context * cx = cx0;
	int rc = -1;
	duk_push_heapptr(cx, parent);
	if (!duk_get_prop_string(cx, -1, name) || !duk_is_function(cx, -1)) {
		debugPrint(3, "no such function %s", name);
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
}

int run_function_onearg_t(const Tag *t, const char *name, const Tag *t2)
{
	if (!allowJS || !t->jslink || !t2->jslink)
		return -1;
	return run_function_onearg_0(t->f0->cx, t->jv, name, t2->jv);
}

int run_function_onearg_win(const Frame *f, const char *name, const Tag *t2)
{
	if (!allowJS || !f->jslink || !t2->jslink)
		return -1;
	return run_function_onearg_0(f->cx, f->winobj, name, t2->jv);
}

int run_function_onearg_doc(const Frame *f, const char *name, const Tag *t2)
{
	if (!allowJS || !f->jslink || !t2->jslink)
		return -1;
	return run_function_onearg_0(f->cx, f->docobj, name, t2->jv);
}

// The single argument to the function has to be a string.
static void run_function_onestring_0(jsobjtype cx0, jsobjtype parent, const char *name,
				const char *s)
{
	duk_context * cx = cx0;
	duk_push_heapptr(cx, parent);
	if (!duk_get_prop_string(cx, -1, name) || !duk_is_function(cx, -1)) {
		debugPrint(3, "no such function %s", name);
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
}

void run_function_onestring_t(const Tag *t, const char *name, const char *s)
{
	if (!allowJS || !t->jslink)
		return;
	run_function_onestring_0(t->f0->cx, t->jv, name, s);
}

static jsobjtype instantiate_array_0(jsobjtype cx0, jsobjtype parent, const char *name)
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
}

static jsobjtype instantiate_0(jsobjtype cx0, jsobjtype parent, const char *name,
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
}

static jsobjtype instantiate_array_element_0(jsobjtype cx0, jsobjtype parent, int idx,
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

static int set_array_element_object_0(jsobjtype cx0, jsobjtype parent, int idx, jsobjtype child)
{
	duk_context * cx = cx0;
	duk_push_heapptr(cx, parent);
	duk_push_heapptr(cx, child);
	duk_put_prop_index(cx, -2, idx);
	duk_pop(cx);
	return 0;
}

static jsobjtype get_array_element_object_0(jsobjtype cx0, jsobjtype parent, int idx)
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

static char *run_script_0(jsobjtype cx0, const char *s)
{
	duk_context * cx = cx0;
	char *result = 0;
	bool rc;
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
	return result;
}

// execute script.text code; more efficient than the above.
void jsRunData(const Tag *t, const char *filename, int lineno)
{
	bool rc;
	const char *s;
	jsobjtype cx;
	if (!allowJS || !t->jslink)
		return;
	debugPrint(5, "> script:");
	cx = t->f0->cx;
	jsSourceFile = filename;
	jsLineno = lineno;
	duk_push_heapptr(cx, t->jv);
	if (!duk_get_prop_string(cx, -1, "text")) {
// no data
fail:
		duk_pop_2(cx);
	jsSourceFile = 0;
		return;
	}
	s = duk_safe_to_string(cx, -1);
	if (!s || !*s)
		goto fail;
// have to set currentScript
	duk_push_heapptr(cx, t->f0->docobj);
	duk_push_heapptr(cx, t->jv);
	duk_put_prop_string(cx, -2, "currentScript");
	duk_pop(cx);
// defer to the earlier routine if there are breakpoints
	if (strstr(s, "bp@(") || strstr(s, "trace@(")) {
		char *result = run_script_0(cx, s);
		nzFree(result);
	} else {
	rc = duk_peval_string(cx, s);
	if (intFlag)
		i_puts(MSG_Interrupted);
	if (!rc) {
		duk_pop(cx);
	} else {
		processError(cx);
	}
}
	jsSourceFile = NULL;
	duk_push_heapptr(cx, t->f0->docobj);
	duk_del_prop_string(cx, -1, "currentScript");
	duk_pop(cx);
// onload handler? Should this run even if the script fails?
// Right now it does.
	if (t->js_file && !isDataURI(t->href) &&
	typeof_property_0(cx, t->jv, "onload") == EJ_PROP_FUNCTION)
		run_event_0(cx, t->jv, "script", "onload");
	debugPrint(5, "< ok");
	duk_pop_2(cx);
}

static jsobjtype create_event_0(jsobjtype cx, jsobjtype parent, const char *evname)
{
	jsobjtype e;
	const char *evname1 = evname;
	if (evname[0] == 'o' && evname[1] == 'n')
		evname1 += 2;
// gc$event protects from garbage collection
	e = instantiate_0(cx, parent, "gc$event", "Event");
	set_property_string_0(cx, e, "type", evname1);
	return e;
}

static void unlink_event_0(jsobjtype cx, jsobjtype parent)
{
	delete_property_0(cx, parent, "gc$event");
}

static bool run_event_0(jsobjtype cx, jsobjtype obj, const char *pname, const char *evname)
{
	int rc;
	jsobjtype eo;	// created event object
	if(typeof_property_0(cx, obj, evname) != EJ_PROP_FUNCTION)
		return true;
	if (debugLevel >= 3) {
		if (debugEvent) {
			int seqno = get_property_number_0(cx, obj, "eb$seqno");
			debugPrint(3, "trigger %s tag %d %s", pname, seqno, evname);
		}
	}
	eo = create_event_0(cx, obj, evname);
	set_property_object_0(cx, eo, "target", obj);
	set_property_object_0(cx, eo, "currentTarget", obj);
	set_property_number_0(cx, eo, "eventPhase", 2);
	rc = run_function_onearg_0(cx, obj, evname, eo);
	unlink_event_0(cx, obj);
// no return or some other return is treated as true in this case
	if (rc < 0)
		rc = true;
	return rc;
}

bool run_event_t(const Tag *t, const char *pname, const char *evname)
{
	if (!allowJS || !t->jslink)
		return true;
	return run_event_0(t->f0->cx, t->jv, pname, evname);
}

bool run_event_win(const Frame *f, const char *pname, const char *evname)
{
	if (!allowJS || !f->jslink)
		return true;
	return run_event_0(f->cx, f->winobj, pname, evname);
}

bool run_event_doc(const Frame *f, const char *pname, const char *evname)
{
	if (!allowJS || !f->jslink)
		return true;
	return run_event_0(f->cx, f->docobj, pname, evname);
}

bool bubble_event_t(const Tag *t, const char *name)
{
	jsobjtype cx;
	jsobjtype e;		// the event object
	bool rc;
	if (!allowJS || !t->jslink)
		return true;
	cx = t->f0->cx;
	e = create_event_0(cx, t->jv, name);
	rc = run_function_onearg_0(cx, t->jv, "dispatchEvent", e);
	if (rc && get_property_bool_0(cx, e, "prev$default"))
		rc = false;
	unlink_event_0(cx, t->jv);
	return rc;
}

void set_property_bool_win(const Frame *f, const char *name, bool v)
{
	set_property_bool_0(f->cx, f->winobj, name, v);
}

bool get_property_bool_t(const Tag *t, const char *name)
{
if(!t->jslink || !allowJS)
return false;
return get_property_bool_0(t->f0->cx, t->jv, name);
}

enum ej_proptype typeof_property_t(const Tag *t, const char *name)
{
if(!t->jslink || !allowJS)
return EJ_PROP_NONE;
return typeof_property_0(t->f0->cx, t->jv, name);
}

int get_property_number_t(const Tag *t, const char *name)
{
if(!t->jslink || !allowJS)
return -1;
return get_property_number_0(t->f0->cx, t->jv, name);
}

char *get_property_string_t(const Tag *t, const char *name)
{
if(!t->jslink || !allowJS)
return 0;
return get_property_string_0(t->f0->cx, t->jv, name);
}

char *get_dataset_string_t(const Tag *t, const char *p)
{
	jsobjtype cx = t->f0->cx;
	char *v;
	if(!t->jslink)
		return 0;
	if (!strncmp(p, "data-", 5)) {
		char *k = cloneString(p + 5);
		jsobjtype ds = get_property_object_0(cx, t->jv, "dataset");
		if(!ds)
			return 0;
		camelCase(k);
		v = get_property_string_0(cx, ds, k);
		nzFree(k);
	} else
		v = get_property_string_0(cx, t->jv, p);
	return v;
}

char *get_property_url_t(const Tag *t, bool action)
{
if(!t->jslink || !allowJS)
return 0;
return get_property_url_0(t->f0->cx, t->jv, action);
}

char *get_style_string_t(const Tag *t, const char *name)
{
	jsobjtype so; // style object
	if(!t->jslink || !allowJS)
		return 0;
	so = get_property_object_0(t->f0->cx, t->jv, "style");
	return so ? get_property_string_0(t->f0->cx, so, name) : 0;
}

void delete_property_t(const Tag *t, const char *name)
{
	if(!t->jslink || !allowJS)
		return;
	delete_property_0(t->f0->cx, t->jv, name);
}

void delete_property_win(const Frame *f, const char *name)
{
	if(!f->jslink || !allowJS)
		return;
	delete_property_0(f->cx, f->winobj, name);
}

void delete_property_doc(const Frame *f, const char *name)
{
	if(!f->jslink || !allowJS)
		return;
	delete_property_0(f->cx, f->docobj, name);
}

void set_property_bool_t(const Tag *t, const char *name, bool v)
{
	if(!t->jslink || !allowJS)
		return;
	set_property_bool_0(t->f0->cx, t->jv, name, v);
}

void set_property_number_t(const Tag *t, const char *name, int v)
{
	if(!t->jslink || !allowJS)
		return;
	set_property_number_0(t->f0->cx, t->jv, name, v);
}

void set_property_string_t(const Tag *t, const char *name, const char * v)
{
	if(!t->jslink || !allowJS)
		return;
	set_property_string_0(t->f0->cx, t->jv, name, v);
}

void set_dataset_string_t(const Tag *t, const char *name, const char *v)
{
	jsobjtype dso; // dataset object
	if(!t->jslink || !allowJS)
		return;
	dso = get_property_object_0(t->f0->cx, t->jv, "dataset");
	if(dso)
		set_property_string_0(t->f0->cx, dso, name, v);
}

void set_property_string_win(const Frame *f, const char *name, const char *v)
{
	set_property_string_0(f->cx, f->winobj, name, v);
}

void set_property_string_doc(const Frame *f, const char *name, const char *v)
{
	set_property_string_0(f->cx, f->docobj, name, v);
}

// Run some javascript code under the named object, usually window.
// Pass the return value of the script back as a string.
static char *jsRunScriptResult(const Frame *f, jsobjtype obj, const char *str,
const char *filename, 			int lineno)
{
	char *result;
	if (!allowJS || !f->winobj)
		return NULL;
	if (!str || !str[0])
		return NULL;
	debugPrint(5, "> script:");
	jsSourceFile = filename;
	jsLineno = lineno;
	result = run_script_0(f->cx, str);
	jsSourceFile = NULL;
	debugPrint(5, "< ok");
	return result;
}				/* jsRunScriptResult */

/* like the above but throw away the result */
void jsRunScriptWin(const char *str, const char *filename, 		 int lineno)
{
	char *s = jsRunScriptResult(cf, cf->winobj, str, filename, lineno);
	nzFree(s);
}

void jsRunScript_t(const Tag *t, const char *str, const char *filename, 		 int lineno)
{
	char *s = jsRunScriptResult(t->f0, t->f0->winobj, str, filename, lineno);
	nzFree(s);
}

char *jsRunScriptWinResult(const char *str,
const char *filename, 			int lineno)
{
return jsRunScriptResult(cf, cf->winobj, str, filename, lineno);
}

// Functions that help decorate the DOM tree, called from decorate.c.

void establish_js_option(Tag *t, Tag *sel)
{
	duk_context *cx = cf->cx; // context
	int idx = t->lic;
	jsobjtype oa;		// option array
	jsobjtype oo;		// option object
	jsobjtype so;		// style object
	jsobjtype ato;		// attributes object
	jsobjtype fo;		// form object
	jsobjtype selobj = sel->jv; // select object

	if(!sel->jslink)
		return;

	if ((oa = get_property_object_0(cx, selobj, "options")) == NULL)
		return;
	if ((oo = instantiate_array_element_0(cx, oa, idx, "Option")) == NULL)
		return;
	set_property_object_0(cx, oo, "parentNode", oa);
/* option.form = select.form */
	fo = get_property_object_0(cx, selobj, "form");
	if (fo)
		set_property_object_0(cx, oo, "form", fo);
	instantiate_array_0(cx, oo, "childNodes");
	ato = instantiate_0(cx, oo, "attributes", "NamedNodeMap");
	set_property_object_0(cx, ato, "owner", oo);
	so = instantiate_0(cx, oo, "style", "CSSStyleDeclaration");
	set_property_object_0(cx, so, "element", oo);

connectTagObject(t, oo);
}

void establish_js_textnode(Tag *t, const char *fpn)
{
	duk_context *cx = cf->cx;
	jsobjtype so, ato;
	 jsobjtype tagobj = instantiate_0(cx, cf->winobj, fpn, "TextNode");
	instantiate_array_0(cx, tagobj, "childNodes");
	ato = instantiate_0(cx, tagobj, "attributes", "NamedNodeMap");
	set_property_object_0(cx, ato, "owner", tagobj);
	so = instantiate_0(cx, tagobj, "style", "CSSStyleDeclaration");
	set_property_object_0(cx, so, "element", tagobj);
	connectTagObject(t, tagobj);
}

static void processStyles(jsobjtype so, const char *stylestring)
{
	char *workstring = cloneString(stylestring);
	char *s;		// gets truncated to the style name
	char *sv;
	char *next;
	for (s = workstring; *s; s = next) {
		next = strchr(s, ';');
		if (!next) {
			next = s + strlen(s);
		} else {
			*next++ = 0;
			skipWhite2(&next);
		}
		sv = strchr(s, ':');
		// if there was something there, but it didn't
		// adhere to the expected syntax, skip this pair
		if (sv) {
			*sv++ = '\0';
			skipWhite2(&sv);
			trimWhite(s);
			trimWhite(sv);
// the property name has to be nonempty
			if (*s) {
				camelCase(s);
				set_property_string_0(cf->cx, so, s, sv);
// Should we set a specification level here, perhaps high,
// so the css sheets don't overwrite it?
// sv + "$$scy" = 99999;
			}
		}
	}
	nzFree(workstring);
}

void domLink(Tag *t, const char *classname,	/* instantiate this class */
		    const char *href, const char *list,	/* next member of this array */
		    const Tag * owntag, int extra)
{
	jsobjtype cx = cf->cx;
	jsobjtype owner;
	jsobjtype alist = 0;
	jsobjtype io = 0;	/* input object */
	int length;
	bool dupname = false, fakeName = false;
	uchar isradio = (extra&1);
// some strings from the html tag
	const char *symname = t->name;
	const char *idname = t->id;
	const char *membername = 0;	/* usually symname */
	const char *href_url = t->href;
	const char *tcn = t->jclass;
	const char *stylestring = attribVal(t, "style");
	jsobjtype so = 0;	/* obj.style */
	jsobjtype ato = 0;	/* obj.attributes */
	char upname[MAXTAGNAME];

	debugPrint(5, "domLink %s.%d name %s",
		   classname, extra, (symname ? symname : emptyString));
	extra &= 6;
	if(owntag)
		owner = owntag->jv;
if(extra == 2)
		owner = cf->winobj;
if(extra == 4)
		owner = cf->docobj;

	if (symname && typeof_property_0(cx, owner, symname)) {
/*********************************************************************
This could be a duplicate name.
Yes, that really happens.
Link to the first tag having this name,
and link the second tag under a fake name so gc won't throw it away.
Or - it could be a duplicate name because multiple radio buttons
all share the same name.
The first time we create the array,
and thereafter we just link under that array.
Or - and this really does happen -
an input tag could have the name action, colliding with form.action.
don't overwrite form.action, or anything else that pre-exists.
*********************************************************************/

		if (isradio) {
/* name present and radio buttons, name should be the array of buttons */
			if(!(io = get_property_object_0(cx, owner, symname)))
				return;
		} else {
/* don't know why the duplicate name */
			dupname = true;
		}
	}

/* The input object is nonzero if&only if the input is a radio button,
 * and not the first button in the set, thus it isce the array containing
 * these buttons. */
	if (io == NULL) {
/*********************************************************************
Ok, the above condition does not hold.
We'll be creating a new object under owner, but through what name?
The name= tag, unless it's a duplicate,
or id= if there is no name=, or a fake name just to protect it from gc.
That's how it was for a long time, but I think we only do this on form.
*********************************************************************/
		if (t->action == TAGACT_INPUT && list) {
			if (!symname && idname)
				membername = idname;
			else if (symname && !dupname)
				membername = symname;
/* id= or name= must not displace submit, reset, or action in form.
 * Example www.startpage.com, where id=submit.
 * nor should it collide with another attribute, such as document.cookie and
 * <div ID=cookie> in www.orange.com.
 * This call checks for the name in the object and its prototype. */
			if (membername && has_property_0(cx, owner, membername)) {
				debugPrint(3, "membername overload %s.%s",
					   classname, membername);
				membername = 0;
			}
		}
		if (!membername)
			membername = fakePropName(), fakeName = true;

		if (isradio) {	// the first radio button
			if(!(io = instantiate_array_0(cx,
			(fakeName ? cf->winobj : owner), membername)))
				return;
			set_property_string_0(cx, io, "type", "radio");
		} else {
		jsobjtype ca;	// child array
/* A standard input element, just create it. */
			if(!(io = instantiate_0(cx,
(fakeName ? cf->winobj : owner), membername, classname)))
				return;
// Not an array; needs the childNodes array beneath it for the children.
			ca = instantiate_array_0(cx, io, "childNodes");
// childNodes and options are the same for Select
			if (stringEqual(classname, "Select"))
				set_property_object_0(cx, io, "options", ca);
		}

/* deal with the 'styles' here.
object will get 'style' regardless of whether there is
anything to put under it, just like it gets childNodes whether
or not there are any.  After that, there is a conditional step.
If this node contains style='' of one or more name-value pairs,
call out to process those and add them to the object.
Don't do any of this if the tag is itself <style>. */
		if (t->action != TAGACT_STYLE) {
			so = instantiate_0(cx, io, "style", "CSSStyleDeclaration");
			set_property_object_0(cx, so, "element", io);
/* now if there are any style pairs to unpack,
 processStyles can rely on obj.style existing */
			if (stylestring)
				processStyles(so, stylestring);
		}

/* Other attributes that are expected by pages, even if they
 * aren't populated at domLink-time */
		if (!tcn)
			tcn = emptyString;
		set_property_string_0(cx, io, "class", tcn);
		set_property_string_0(cx, io, "last$class", tcn);
		ato = instantiate_0(cx, io, "attributes", "NamedNodeMap");
		set_property_object_0(cx, ato, "owner", io);
		set_property_object_0(cx, io, "ownerDocument", cf->docobj);
		instantiate_0(cx, io, "dataset", 0);

// only anchors with href go into links[]
		if (list && stringEqual(list, "links") &&
		    !attribPresent(t, "href"))
			list = 0;
		if (list)
			alist = get_property_object_0(cx, owner, list);
		if (alist) {
			if((length = get_arraylength_0(cx, alist)) < 0)
				return;
			set_array_element_object_0(cx, alist, length, io);
			if (symname && !dupname
			    && !has_property_0(cx, alist, symname))
				set_property_object_0(cx, alist, symname, io);
#if 0
			if (idname && symname != idname
			    && !has_property_0(cx, alist, idname))
				set_property_object_0(cx, alist, idname, io);
#endif
		}		/* list indicated */
	}

	if (isradio) {
// drop down to the element within the radio array, and return that element.
// io becomes the object associated with this radio button.
// At present, io is an array.
		if((length = get_arraylength_0(cx, io)) < 0)
			return;
		if(!(io = instantiate_array_element_0(cx, io, length, "Element")))
			return;
	}

	set_property_string_0(cx, io, "name", (symname ? symname : emptyString));
	set_property_string_0(cx, io, "id", (idname ? idname : emptyString));
	set_property_string_0(cx, io, "last$id", (idname ? idname : emptyString));

	if (href && href_url)
// This use to be instantiate_url, but with the new side effects
// on Anchor, Image, etc, we can just set the string.
		set_property_string_0(cx, io, href, href_url);

	if (t->action == TAGACT_INPUT) {
/* link back to the form that owns the element */
		set_property_object_0(cx, io, "form", owner);
	}

	connectTagObject(t, io);

	strcpy(upname, t->info->name);
	caseShift(upname, 'u');
// DocType has nodeType = 10, see startwindow.js
	if(t->action != TAGACT_DOCTYPE) {
		set_property_string_0(cx, io, "nodeName", upname);
		set_property_string_0(cx, io, "tagName", upname);
		set_property_number_0(cx, io, "nodeType", 1);
	}
}

/*********************************************************************
Javascript sometimes builds or rebuilds a submenu, based upon your selection
in a primary menu. These new options must map back to html tags,
and then to the dropdown list as you interact with the form.
This is tested in jsrt - select a state,
whereupon the colors below, that you have to choose from, can change.
This does not easily fold into rerender(),
it must be rerun after javascript activity, e.g. in jSideEffects().
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
	domSetsTagValue(sel, s);
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

	for (i1 = 0; i1 < cw->numTags; ++i1) {
		t = tagList[i1];
		if (!t->jslink)
			continue;
		if (t->action != TAGACT_INPUT)
			continue;
		if (t->itype != INP_SELECT)
			continue;
#if 0
		if(!tagIsRooted(t))
			continue;
#endif

/* there should always be an options array, if not then move on */
		if (!(oa = get_property_object_0(t->f0->cx, t->jv, "options")))
			continue;
		if ((len = get_arraylength_0(t->f0->cx, oa)) < 0)
			continue;
		rebuildSelector(t, oa, len);
	}
}

// Some primitives needed by css.c. These bounce through window.soj$
static const char soj[] = "soj$";
static void sofail() { debugPrint(3, "no style object"); }

bool has_gcs(const char *name)
{
	duk_context * cx = cf->cx;
	if(!duk_get_global_string(cx, soj)) {
		sofail();
		duk_pop(cx);
		return false;
	}
	        bool l = duk_has_prop_string(cx, -1, name);
	duk_pop(cx);
	return l;
}

enum ej_proptype typeof_gcs(const char *name)
{
	bool rc;
	enum ej_proptype l;
	duk_context * cx = cf->cx;
	if(!duk_get_global_string(cx, soj)) {
		sofail();
		duk_pop(cx);
		return EJ_PROP_NONE;
	}
	        rc = duk_get_prop_string(cx, -1, name);
	        l = rc ? top_proptype(cx) : 0;
	duk_pop_2(cx);
	return l;
}

int get_gcs_number(const char *name)
{
	duk_context * cx = cf->cx;
	int l = -1;
	bool rc;
	if(!duk_get_global_string(cx, soj)) {
		sofail();
		duk_pop(cx);
		return -1;
	}
	rc = duk_get_prop_string(cx, -1, name);
	if(rc && duk_is_number(cx, -1))
		l = duk_get_number(cx, -1);
	duk_pop_2(cx);
	return l;
}

void set_gcs_number(const char *name, int n)
{
	duk_context * cx = cf->cx;
	if(!duk_get_global_string(cx, soj)) {
		sofail();
		duk_pop(cx);
		return;
	}
	duk_push_int(cx, n);
	duk_put_prop_string(cx, -2, name);
	duk_pop(cx);
}

void set_gcs_bool(const char *name, bool v)
{
	duk_context * cx = cf->cx;
	if(!duk_get_global_string(cx, soj)) {
		sofail();
		duk_pop(cx);
		return;
	}
	duk_push_boolean(cx, v);
	duk_put_prop_string(cx, -2, name);
	duk_pop(cx);
}

void set_gcs_string(const char *name, const char *s)
{
	duk_context * cx = cf->cx;
	if(!duk_get_global_string(cx, soj)) {
		sofail();
		duk_pop(cx);
		return;
	}
	duk_push_string(cx, s);
	duk_put_prop_string(cx, -2, name);
	duk_pop(cx);
}

void jsUnroot(void)
{
// this function isn't needed in the duktape world
}

void jsClose(void)
{
	if(js_running)
		duk_destroy_heap(context0);
}
