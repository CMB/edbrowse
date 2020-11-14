// This program originally came from the Mozilla site, for moz 52.
// https://developer.mozilla.org/en-US/docs/Mozilla/Projects/SpiderMonkey/How_to_embed_the_JavaScript_engine
// I tweaked it for moz 60.
// Then I added so much stuff to it you'd hardly recognize it.
// It has become my sandbox.

#include <jsapi.h>
#include <js/Initialization.h>

static JSClassOps global_ops = {
// different members in different versions, so specfiy explicitly
    trace : JS_GlobalObjectTraceHook
};

/* The class of the global object. */
static JSClass global_class = {
    "global",
    JSCLASS_GLOBAL_FLAGS,
    &global_ops
};

// A few functions from the edbrowse world, so I can write and test
// some other functions that rely on those functions.
// Jump in with both feet and see if we can swallow the edbrowse header files.

#include "eb.h"

static struct ebWindow win0;
struct ebWindow *cw = &win0;
Frame *cf = &win0.f0;
int context = 0;
char whichproc = 'e';
bool allowJS = true;
bool showHover, doColors;
char *dbarea, *dblogin, *dbpw;	/* to log into the database */
bool fetchBlobColumns;
void setDataSource(char *v){}
bool binaryDetect = true;
bool sendReferrer, allowRedirection, curlAuthNegotiate, ftpActive;
bool htmlGenerated;
uchar browseLocal;
char *changeFileName;
char *sslCerts;
char *configFile, *addressFile, *cookieFile;
char *recycleBin, *sigFile, *sigFileEnd;
char *ebUserDir;
char *cacheDir;
int cacheSize, cacheCount;
bool inInput, listNA;
pst linePending;
int fileSize;
int displayLength = 500;
char *newlocation;
Frame *newloc_f;
bool newloc_r;
int newloc_d;
bool runEbFunction(const char *line){ return true; }
bool inputReadLine;
bool caseInsensitive, searchStringsAll, searchWrap = true;
const char *jsSourceFile;
int jsLineno;
int gfsn;
struct MACCOUNT accounts[MAXACCOUNT];
int localAccount, maxAccount;
struct MIMETYPE mimetypes[MAXMIME];
int maxMime;
const char *version = "3.7.7";
volatile bool intFlag;
time_t intStart;
bool ismc, isimap, passMail;
char *mailDir, *mailUnread, *mailStash, *mailReply;
struct ebSession sessionList[10], *cs;
int maxSession;
int webTimeout = 30, mailTimeout = 30;
int verifyCertificates = 1;
char *userAgents[MAXAGENT + 1];
char *currentAgent;
void ebClose(int n) { exit(n); }
Tag *newTag(const Frame *f, const char *tagname) { puts("new tag abort"); exit(4); }
void domOpensWindow(const char *href, const char *u) { printf("set to open %s\n", href); }
void htmlInputHelper(Tag *t) { }
void formControl(Tag *t, bool namecheck) { }
Tag *findOpenTag(Tag *t, int action) { return NULL; }
Tag *findOpenList(Tag *t){return 0;}
void writeShortCache(void) { }
void html_from_setter( jsobjtype innerParent, const char *h) { printf("expand %s\n", h); }
bool matchMedia(char *t) { printf("match media %s\n", t); return false; }
void cssDocLoad(jsobjtype thisobj, char *s, bool pageload) { puts("css doc load"); }
void cssApply(jsobjtype thisobj, jsobjtype node, jsobjtype destination) { puts("css apply"); }
void cssText(jsobjtype node, const char *rulestring) { puts("css text"); }
void underKill(Tag *t) { }
void cssFree(Frame *f) { }
void rebuildSelectors(void) { puts("rebuild selectors"); }
void decorate(int start) { puts("decorate"); }
void freeTags(struct ebWindow *w) {}
void prerender(int start) { puts("prerender"); }
void htmlNodesIntoTree(int start, Tag *attach) { puts("tags into tree"); }
void html2nodes(const char *htmltext, bool startpage) { puts("htnl 2 nodes"); }
bool javaOK(const char *url) { return true; }
bool mustVerifyHost(const char *url) { return false; }
const char *findAgentForURL(const char *url) { return 0; }
const char eol[] = "\r\n";
const char *findProxyForURL(const char *url) { return 0; }
const char *mailRedirect(const char *to, const char *from, 			 const char *reply, const char *subj) { return 0; }
void readConfigFile(void) { }
void setTagAttr(Tag *t, const char *name, char *val) { nzFree(val); }
const char *attribVal(const Tag *t, const char *name){ return 0;}
void set_basehref(const char *b){}
void traverseAll(int start){}
nodeFunction traverse_callback;
const char *fetchReplace(const char *u){return 0;}
void initTagArray(void){}
const struct tagInfo availableTags[1] = {
	{"html", "html", TAGACT_HTML}};
const char *const inp_types[1] = {"X"};
const char *const inp_others[1] = {"x"};
char *displayOptions(const Tag *sel) {return 0;}

// Here begins code that can eventually move to jseng-moz.cpp,
// or maybe html.cpp or somewhere.

static JSContext *cxa; // the context for all
// I'll still use cx when it is passed in, as it must be for native methods.
// But it will be equal to cxa.

// Rooting window, to hold on to any objects that edbrowse might access.
static JS::RootedObject *rw0;

// Master window, with large and complex functions that we want to
// compile and store only once. Unfortunately, as of moz 60,
// it seems we can't do this with classes. Objects must instantiate
// from a class in the same window.    idk
static JS::RootedObject *mw0;

/*********************************************************************
The _o methods are the lowest level, calling upon the engine.
They take JSObject as parameter, thus the _o nomenclature.
These have to be in this file and have to understand the moz js objects
and values, and the calls to the js engine.
Each of these functions assumes you are already in a compartment.
If you're not, something will seg fault somewhere along the line!
*********************************************************************/

// Convert engine property type to an edbrowse property type.
static enum ej_proptype top_proptype(JS::HandleValue v)
{
bool isarray;

switch(JS_TypeOfValue(cxa, v)) {
// This enum isn't in every version; leave it to default.
// case JSTYPE_UNDEFINED: return "undefined"; break;

case JSTYPE_FUNCTION:
return EJ_PROP_FUNCTION;

case JSTYPE_OBJECT:
JS_IsArrayObject(cxa, v, &isarray);
return isarray ? EJ_PROP_ARRAY : EJ_PROP_OBJECT;

case JSTYPE_STRING:
return EJ_PROP_STRING;

case JSTYPE_NUMBER:
return v.isInt32() ? EJ_PROP_INT : EJ_PROP_FLOAT;

case JSTYPE_BOOLEAN:
return EJ_PROP_BOOL;

// null is returned as object and doesn't trip this case, for some reason
case JSTYPE_NULL:
return EJ_PROP_NULL;

case JSTYPE_LIMIT:
case JSTYPE_SYMBOL:
default:
return EJ_PROP_NONE;
}
}

enum ej_proptype typeof_property_o(JS::HandleObject parent, const char *name)
{
JS::RootedValue v(cxa);
if(!JS_GetProperty(cxa, parent, name, &v))
return EJ_PROP_NONE;
return top_proptype(v);
}

static void uptrace(JS::HandleObject start);
static void processError(void);
static void jsInterruptCheck(void);

bool has_property_o(JS::HandleObject parent, const char *name)
{
bool found;
JS_HasProperty(cxa, parent, name, &found);
return found;
}

void delete_property_o(JS::HandleObject parent, const char *name)
{
JS_DeleteProperty(cxa, parent, name);
}

int get_arraylength_o(JS::HandleObject a)
{
unsigned length;
if(!JS_GetArrayLength(cxa, a, &length))
return -1;
return length;
}

JSObject *get_array_element_object_o(JS::HandleObject parent, int idx)
{
JS::RootedValue v(cxa);
if(!JS_GetElement(cxa, parent, idx, &v) ||
!v.isObject())
return NULL;
JS::RootedObject o(cxa);
JS_ValueToObject(cxa, v, &o);
return o;
}

/*********************************************************************
This returns the string equivalent of the js value, but use with care.
It's only good til the next call to stringize, then it will be trashed.
If you want the result longer than that, you better copy it.
*********************************************************************/

static const char *stringize(JS::HandleValue v)
{
	static char buf[48];
	static const char *dynamic;
	int n;
	double d;
	JSString *str;
bool ok;

if(v.isNull())
return "null";

switch(JS_TypeOfValue(cxa, v)) {
// This enum isn't in every version; leave it to default.
// case JSTYPE_UNDEFINED: return "undefined"; break;

case JSTYPE_OBJECT:
case JSTYPE_FUNCTION:
// invoke toString
{
JS::RootedObject p(cxa);
JS_ValueToObject(cxa, v, &p);
JS::RootedValue tos(cxa); // toString
ok = JS_CallFunctionName(cxa, p, "toString", JS::HandleValueArray::empty(), &tos);
if(ok && tos.isString()) {
cnzFree(dynamic);
str = tos.toString();
dynamic = JS_EncodeString(cxa, str);
return dynamic;
}
}
return "object";

case JSTYPE_STRING:
cnzFree(dynamic);
str = v.toString();
dynamic = JS_EncodeString(cxa, str);
return dynamic;

case JSTYPE_NUMBER:
if(v.isInt32())
sprintf(buf, "%d", v.toInt32());
else sprintf(buf, "%f", v.toDouble());
return buf;

case JSTYPE_BOOLEAN: return v.toBoolean() ? "true" : "false";

// null is returned as object and doesn't trip this case, for some reason
case JSTYPE_NULL: return "null";

// don't know what symbol is
case JSTYPE_SYMBOL: return "symbol";

case JSTYPE_LIMIT: return "limit";

default: return "undefined";
}
}

/* Return a property as a string, if it is
 * string compatible. The string is allocated, free it when done. */
char *get_property_string_o(JS::HandleObject parent, const char *name)
{
JS::RootedValue v(cxa);
if(!JS_GetProperty(cxa, parent, name, &v))
return NULL;
return cloneString(stringize(v));
}

// Return a pointer to the JSObject. You need to dump this directly into
// a RootedObject.
JSObject *get_property_object_o(JS::HandleObject parent, const char *name)
{
JS::RootedValue v(cxa);
if(!JS_GetProperty(cxa, parent, name, &v) ||
!v.isObject())
return NULL;
JS::RootedObject obj(cxa);
JS_ValueToObject(cxa, v, &obj);
JSObject *j = obj; // This pulls the object pointer out for us
return j;
}

JSObject *get_property_function_o(JS::HandleObject parent, const char *name)
{
JS::RootedValue v(cxa);
if(!JS_GetProperty(cxa, parent, name, &v))
return NULL;
JS::RootedObject obj(cxa);
JS_ValueToObject(cxa, v, &obj);
if(!JS_ObjectIsFunction(cxa, obj))
return NULL;
JSObject *j = obj; // This pulls the object pointer out for us
return j;
}

// Return href for a url. This string is allocated.
// Could be form.action, image.src, a.href; so this isn't a trivial routine.
// This isn't inline efficient, but it is rarely called.
char *get_property_url_o(JS::HandleObject parent, bool action)
{
	enum ej_proptype t;
JS::RootedObject uo(cxa);	/* url object */
	if (action) {
		t = typeof_property_o(parent, "action");
		if (t == EJ_PROP_STRING)
			return get_property_string_o(parent, "action");
		if (t != EJ_PROP_OBJECT)
			return 0;
		uo = get_property_object_o(parent, "action");
	} else {
		t = typeof_property_o(parent, "href");
		if (t == EJ_PROP_STRING)
			return get_property_string_o(parent, "href");
		if (t == EJ_PROP_OBJECT)
			uo = get_property_object_o(parent, "href");
		else if (t)
			return 0;
		if (!uo) {
			t = typeof_property_o(parent, "src");
			if (t == EJ_PROP_STRING)
				return get_property_string_o(parent, "src");
			if (t == EJ_PROP_OBJECT)
				uo = get_property_object_o(parent, "src");
		}
	}
if(!uo)
		return 0;
/* should this be href$val? */
	return get_property_string_o(uo, "href");
}

int get_property_number_o(JS::HandleObject parent, const char *name)
{
JS::RootedValue v(cxa);
if(!JS_GetProperty(cxa, parent, name, &v))
return -1;
if(v.isInt32()) return v.toInt32();
return -1;
}

double get_property_float_o(JS::HandleObject parent, const char *name)
{
JS::RootedValue v(cxa);
if(!JS_GetProperty(cxa, parent, name, &v))
return 0.0; // should this be nan
if(v.isDouble()) return v.toDouble();
return 0.0;
}

bool get_property_bool_o(JS::HandleObject parent, const char *name)
{
JS::RootedValue v(cxa);
if(!JS_GetProperty(cxa, parent, name, &v))
return false;
if(v.isBoolean()) return v.toBoolean();
return false;
}

#define JSPROP_STD JSPROP_ENUMERATE

void set_property_number_o(JS::HandleObject parent, const char *name,  int n)
{
JS::RootedValue v(cxa);
	bool found;
v.setInt32(n);
	JS_HasProperty(cxa, parent, name, &found);
	if (found)
JS_SetProperty(cxa, parent, name, v);
else
JS_DefineProperty(cxa, parent, name, v, JSPROP_STD);
}

void set_property_float_o(JS::HandleObject parent, const char *name,  double d)
{
JS::RootedValue v(cxa);
	bool found;
v.setDouble(d);
	JS_HasProperty(cxa, parent, name, &found);
	if (found)
JS_SetProperty(cxa, parent, name, v);
else
JS_DefineProperty(cxa, parent, name, v, JSPROP_STD);
}

void set_property_bool_o(JS::HandleObject parent, const char *name,  bool b)
{
JS::RootedValue v(cxa);
	bool found;
v.setBoolean(b);
	JS_HasProperty(cxa, parent, name, &found);
	if (found)
JS_SetProperty(cxa, parent, name, v);
else
JS_DefineProperty(cxa, parent, name, v, JSPROP_STD);
}

// Before we can approach set_property_string, we need some setters.
// Example: the value property, when set, uses a setter to push that value
// through to edbrowse, where you can see it.

static Tag *tagFromObject(JS::HandleObject o);

static void domSetsInner(JS::HandleObject ival, const char *newtext)
{
	int side;
	Tag *t = tagFromObject(ival);
	if (!t)
		return;
// the tag should always be a textarea
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
// Not sure how we could not be browsing
	if (cw->browseMode)
		i_printf(MSG_BufferUpdated, side);
	sideBuffer(side, newtext, -1, 0);
}

static void domSetsTagValue(JS::HandleObject ival, const char *newtext)
{
	Tag *t = tagFromObject(ival);
	if (!t)
		return;
// dom often changes the value of hidden fiels;
// it should not change values of radio through this mechanism,
// and should never change a file field for security reasons.
	if (t->itype == INP_HIDDEN || t->itype == INP_RADIO
	    || t->itype == INP_FILE)
		return;
	if (t->itype == INP_TA) {
		domSetsInner(ival, newtext);
		return;
	}
	nzFree(t->value);
	t->value = cloneString(newtext);
}

static bool getter_value(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
        JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
JS::RootedValue newv(cxa);
if(!JS_GetProperty(cx, thisobj, "val$ue", &newv)) {
// We shouldn't be here; there should be a val$ue to read
newv.setString(JS_GetEmptyString(cx));
}
args.rval().set(newv);
return true;
}

static bool setter_value(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
// should we be setting result to anything?
args.rval().setUndefined();
if(argc != 1)
return true; // should never happen
const char *h = stringize(args[0]);
if(!h)
h = emptyString;
	char *k = cloneString(h);
	debugPrint(5, "setter v 1");
        JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
JS_SetProperty(cx, thisobj, "val$ue", args[0]);
	prepareForField(k);
if(debugLevel >= 4) {
int esn = get_property_number_o(thisobj, "eb$seqno");
	debugPrint(4, "value tag %d=%s", esn, k);
}
	domSetsTagValue(thisobj, k);
	nzFree(k);
	debugPrint(5, "setter v 2");
	return true;
}

static int frameExpandLine(int ln, JS::HandleObject fo);
static int frameContractLine(int lineNumber);

static void forceFrameExpand(JS::HandleObject thisobj)
{
	Frame *save_cf = cf;
	const char *save_src = jsSourceFile;
	int save_lineno = jsLineno;
	bool save_plug = pluginsOn;
set_property_bool_o(thisobj, "eb$auto", true);
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
static bool getter_cd(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
	bool found;
	jsInterruptCheck();
        JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
JS_HasProperty(cx, thisobj, "eb$auto", &found);
	if (!found)
		forceFrameExpand(thisobj);
JS::RootedValue v(cx);
JS_GetProperty(cx, thisobj, "content$Document", &v);
args.rval().set(v);
return true;
}

// You can't really change contentDocument; we'll use
// nat_stub for the setter instead.
static bool nat_stub(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
args.rval().setUndefined();
  return true;
}


// contentWindow getter setter; this is a bit complicated.
static bool getter_cw(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
	bool found;
	jsInterruptCheck();
        JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
JS_HasProperty(cx, thisobj, "eb$auto", &found);
	if (!found)
		forceFrameExpand(thisobj);
JS::RootedValue v(cx);
JS_GetProperty(cx, thisobj, "content$Window", &v);
args.rval().set(v);
return true;
}

// You can't really change contentWindow; we'll use
// nat_stub for the setter instead.

static bool getter_innerHTML(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
        JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
JS::RootedValue newv(cxa);
if(!JS_GetProperty(cx, thisobj, "inner$HTML", &newv)) {
// We shouldn't be here; there should be an inner$HTML to read
newv.setString(JS_GetEmptyString(cx));
}
args.rval().set(newv);
return true;
}

// These are temporary declarations, as they should live in ebprot.h
JSObject *instantiate_array_o(JS::HandleObject parent, const char *name);
int run_function_onearg_o(JS::HandleObject parent, const char *name, JS::HandleObject a);
void createJSContext(Frame *f);
void freeJSContext(Frame *f);

static bool setter_innerHTML(JSContext *cx, unsigned argc, JS::Value *vp)
{
if(argc != 1)
return true; // should never happen
  JS::CallArgs args = CallArgsFromVp(argc, vp);
const char *h = stringize(args[0]);
if(!h)
h = emptyString;
jsInterruptCheck();
	debugPrint(5, "setter h 1");

	{ // scope
bool isarray;
        JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
// remove the preexisting children.
      JS::RootedValue v(cx);
        if (!JS_GetProperty(cx, thisobj, "childNodes", &v) ||
!v.isObject())
		goto fail;
JS_IsArrayObject(cx, v, &isarray);
if(!isarray)
goto fail;
JS::RootedObject cna(cx); // child nodes array
JS_ValueToObject(cx, v, &cna);
// hold this away from garbage collection
JS_SetProperty(cxa, thisobj, "old$cn", v);
JS_DeleteProperty(cxa, thisobj, "childNodes");
// make new childNodes array
JS::RootedObject cna2(cxa, instantiate_array_o(thisobj, "childNodes"));
JS_SetProperty(cx, thisobj, "inner$HTML", args[0]);

// Put some tags around the html, so tidy can parse it.
	char *run;
	int run_l;
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

JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
run_function_onearg_o(g, "textarea$html$crossover", thisobj);

// mutFixup(this, false, cna2, cna);
JS::AutoValueArray<4> ma(cxa); // mutfix arguments
ma[3].set(v);
ma[0].setObject(*thisobj);
ma[1].setBoolean(false);
ma[2].setObject(*cna);
JS_CallFunctionName(cxa, g, "mutFixup", ma, &v);

JS_DeleteProperty(cxa, thisobj, "old$cn");
args.rval().setUndefined();
return true;
	}

fail:
	debugPrint(5, "setter h 3");
args.rval().setUndefined();
return true;
}

void set_property_string_o(JS::HandleObject parent, const char *name, const char *value)
{
	bool found;
	JSNative getter = NULL;
	JSNative setter = NULL;
	const char *altname = 0;
// Have to put value into a js value
if(!value) value = emptyString;
JS::RootedString m(cxa, JS_NewStringCopyZ(cxa, value));
JS::RootedValue ourval(cxa);
ourval.setString(m);
// now look for setters
	if (stringEqual(name, "innerHTML"))
		setter = setter_innerHTML, getter = getter_innerHTML,
		    altname = "inner$HTML";
	if (stringEqual(name, "value")) {
// Only do this for input, i.e. class Element
JS::RootedValue dcv(cxa);
if(JS_GetProperty(cxa, parent, "dom$class", &dcv) &&
dcv.isString()) {
JSString *str = dcv.toString();
char *es = JS_EncodeString(cxa, str);
if(stringEqual(es, "Element"))
			setter = setter_value,
			    getter = getter_value,
altname = "val$ue";
free(es);
	}
	}
if(!altname) altname = name;
JS_HasProperty(cxa, parent, altname, &found);
if(found) {
JS_SetProperty(cxa, parent, altname, ourval);
return;
}
// Ok I thought sure I'd need to set JSPROP_GETTER|JSPROP_SETTER
// but that causes a seg fault.
#if MOZJS_MAJOR_VERSION >= 60
if(setter)
JS_DefineProperty(cxa, parent, name, getter, setter, JSPROP_STD);
#else
if(setter)
JS_DefineProperty(cxa, parent, name, 0, JSPROP_STD, getter, setter);
#endif
JS_DefineProperty(cxa, parent, altname, ourval, JSPROP_STD);
}

void set_property_object_o(JS::HandleObject parent, const char *name,  JS::HandleObject child)
{
JS::RootedValue v(cxa);
	bool found;

	JS_HasProperty(cxa, parent, name, &found);

// Special code for frame.contentDocument
	if (stringEqual(name, "contentDocument")) {
// Is it really a Frame object?
JS_GetProperty(cxa, parent, "dom$class", &v);
if(stringEqual(stringize(v), "Frame")) {
#if MOZJS_MAJOR_VERSION >= 60
JS_DefineProperty(cxa, parent, name, getter_cd, nat_stub, JSPROP_STD);
#else
JS_DefineProperty(cxa, parent, name, 0, JSPROP_STD, getter_cd, nat_stub);
#endif
name = "content$Document";
found = false;
}
}

	if (stringEqual(name, "contentWindow")) {
// Is it really a Frame object?
JS_GetProperty(cxa, parent, "dom$class", &v);
if(stringEqual(stringize(v), "Frame")) {
#if MOZJS_MAJOR_VERSION >= 60
JS_DefineProperty(cxa, parent, name, getter_cw, nat_stub, JSPROP_STD);
#else
JS_DefineProperty(cxa, parent, name, 0, JSPROP_STD, getter_cw, nat_stub);
#endif
name = "content$Window";
found = false;
}
}

v = JS::ObjectValue(*child);
	if (found)
JS_SetProperty(cxa, parent, name, v);
else
JS_DefineProperty(cxa, parent, name, v, JSPROP_STD);
}

void set_array_element_object_o(JS::HandleObject parent, int idx, JS::HandleObject child)
{
bool found;
JS::RootedValue v(cxa);
v = JS::ObjectValue(*child);
JS_HasElement(cxa, parent, idx, &found);
if(found)
JS_SetElement(cxa, parent, idx, v);
else
JS_DefineElement(cxa, parent, idx, v, JSPROP_STD);
}

static bool nat_null(JSContext *cx, unsigned argc, JS::Value *vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	args.rval().setNull();
	return true;
}

void set_property_function_o(JS::HandleObject parent, const char *name, const char *body)
{
JS::RootedFunction f(cxa);
	if (!body || !*body) {
// null or empty function, function will return null.
f = JS_NewFunction(cxa, nat_null, 0, 0, name);
} else {
JS::AutoObjectVector envChain(cxa);
JS::CompileOptions opts(cxa);
if(!JS::CompileFunction(cxa, envChain, opts, name, 0, nullptr, body, strlen(body), &f)) {
		processError();
		debugPrint(3, "compile error for %s(){%s}", name, body);
f = JS_NewFunction(cxa, nat_null, 0, 0, name);
}
	}
JS::RootedObject fo(cxa, JS_GetFunctionObject(f));
set_property_object_o(parent, name, fo);
}

JSObject *instantiate_o(JS::HandleObject parent, const char *name,
			      const char *classname)
{
	JS::RootedValue v(cxa);
	JS::RootedObject a(cxa);
bool found;
	JS_HasProperty(cxa, parent, name, &found);
	if (found) {
JS_GetProperty(cxa, parent, name, &v);
		if (v.isObject()) {
// I'm going to assume it is of the proper class
JS_ValueToObject(cxa, v, &a);
			return a;
		}
		JS_DeleteProperty(cxa, parent, name);
	}
if(!classname || !*classname) {
a = JS_NewObject(cxa, nullptr);
} else {
// find the class for classname
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
if(!JS_GetProperty(cxa, g, classname, &v) ||
!v.isObject())
return 0;
// I could extract the object and verify with
// JS_ObjectIsFunction(), but I'll just assume it is.
if(!JS::Construct(cxa, v, JS::HandleValueArray::empty(), &a)) {
		debugPrint(3, "failure on %s = new %s", name,    classname);
		uptrace(parent);
return 0;
}
}
v = JS::ObjectValue(*a);
	JS_DefineProperty(cxa, parent, name, v, JSPROP_STD);
	return a;
}

JSObject *instantiate_array_element_o(JS::HandleObject parent,
int idx, 			      const char *classname)
{
	JS::RootedValue v(cxa);
	JS::RootedObject a(cxa);
bool found;
	JS_HasElement(cxa, parent, idx, &found);
	if (found) {
JS_GetElement(cxa, parent, idx, &v);
		if (v.isObject()) {
// I'm going to assume it is of the proper class
JS_ValueToObject(cxa, v, &a);
			return a;
		}
v.setUndefined();
JS_SetElement(cxa, parent, idx, v);
	}
if(!classname || !*classname) {
a = JS_NewObject(cxa, nullptr);
} else {
// find the class for classname
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
if(!JS_GetProperty(cxa, g, classname, &v) ||
!v.isObject())
return 0;
// I could extract the object and verify with
// JS_ObjectIsFunction(), but I'll just assume it is.
if(!JS::Construct(cxa, v, JS::HandleValueArray::empty(), &a)) {
		debugPrint(3, "failure on [%d] = new %s", idx,    classname);
		uptrace(parent);
return 0;
}
}
v = JS::ObjectValue(*a);
if(found)
JS_SetElement(cxa, parent, idx, v);
else
JS_DefineElement(cxa, parent, idx, v, JSPROP_STD);
	return a;
}

JSObject *instantiate_array_o(JS::HandleObject parent, const char *name)
{
	JS::RootedValue v(cxa);
	JS::RootedObject a(cxa);
bool found, isarray;
	JS_HasProperty(cxa, parent, name, &found);
	if (found) {
		if (v.isObject()) {
JS_IsArrayObject(cxa, v, &isarray);
if(isarray) {
JS_ValueToObject(cxa, v, &a);
			return a;
		}
		}
		JS_DeleteProperty(cxa, parent, name);
	}
a = JS_NewArrayObject(cxa, 0);
v = JS::ObjectValue(*a);
	JS_DefineProperty(cxa, parent, name, v, JSPROP_STD);
	return a;
}

// run a function with no arguments, that returns bool
bool run_function_bool_o(JS::HandleObject parent, const char *name)
{
bool rc = false;
	int dbl = 3;		// debug level
	int seqno = -1;
	if (stringEqual(name, "ontimer")) {
// even at debug level 3, I don't want to see
// the execution messages for each timer that fires
		dbl = 4;
seqno = get_property_number_o(parent, "tsn");
}
	if (seqno > 0)
		debugPrint(dbl, "exec %s timer %d", name, seqno);
	else
		debugPrint(dbl, "exec %s", name);
JS::RootedValue retval(cxa);
bool ok = JS_CallFunctionName(cxa, parent, name, JS::HandleValueArray::empty(), &retval);
		debugPrint(dbl, "exec complete");
if(!ok) {
// error in execution
	if (intFlag)
		i_puts(MSG_Interrupted);
	processError();
	debugPrint(3, "failure on %s()", name);
	uptrace(parent);
	debugPrint(3, "exec complete");
return false;
} // error
if(retval.isBoolean())
return retval.toBoolean();
if(retval.isInt32())
return !!retval.toInt32();
if(!retval.isString())
return false;
const char *s = stringize(retval);
// anything but false or the empty string is considered true
if(!*s || stringEqual(s, "false"))
return false;
return true;
}

static JSObject *tagToObject(const Tag *t);
static JSObject *frameToCompartment(const Frame *f);

bool run_function_bool_t(const Tag *t, const char *name)
{
if(!t->jslink || !allowJS)
return false;
JSAutoCompartment ac(cxa, frameToCompartment(t->f0));
JS::RootedObject obj(cxa, tagToObject(t));
	return run_function_bool_o(obj, name);
}

bool run_function_bool_win(const Frame *f, const char *name)
{
if(!f->jslink || !allowJS)
return false;
JSAutoCompartment ac(cxa, frameToCompartment(f));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa)); // global
	return run_function_bool_o(g, name);
}

static bool run_event_o(JS::HandleObject obj, const char *pname, const char *evname);

void run_ontimer(const Frame *f, const char *backlink)
{
JSAutoCompartment ac(cxa, frameToCompartment(f));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
JS::RootedObject to(cxa, get_property_object_o(g, backlink));
if(!to) {
  debugPrint(3, "could not find timer backlink %s", backlink);
		return;
	}
	run_event_o(to, "timer", "ontimer");
}

int run_function_onearg_o(JS::HandleObject parent, const char *name,
JS::HandleObject a)
{
JS::RootedValue retval(cxa);
JS::RootedValue v(cxa);
JS::AutoValueArray<1> args(cxa);
args[0].setObject(*a);
bool ok = JS_CallFunctionName(cxa, parent, name, args, &retval);
if(!ok) {
// error in execution
	if (intFlag)
		i_puts(MSG_Interrupted);
	processError();
	debugPrint(3, "failure on %s(object)", name);
	uptrace(parent);
return -1;
} // error
if(retval.isBoolean())
return retval.toBoolean();
if(retval.isInt32())
return retval.toInt32();
return -1;
}

int run_function_onearg_t(const Tag *t, const char *name, const Tag *t2)
{
if(!t->jslink || !t2->jslink || !allowJS)
return -1;
JSAutoCompartment ac(cxa, frameToCompartment(t->f0));
JS::RootedObject obj(cxa, tagToObject(t));
JS::RootedObject obj2(cxa, tagToObject(t2));
	return run_function_onearg_o(obj, name, obj2);
}

int run_function_onearg_win(const Frame *f, const char *name, const Tag *t2)
{
if(!f->jslink || !t2->jslink || !allowJS)
return -1;
JSAutoCompartment ac(cxa, frameToCompartment(f));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa)); // global
JS::RootedObject obj2(cxa, tagToObject(t2));
	return run_function_onearg_o(g, name, obj2);
}

void run_function_onestring_o(JS::HandleObject parent, const char *name,
const char *a)
{
JS::RootedValue retval(cxa);
JS::AutoValueArray<1> args(cxa);
JS::RootedString m(cxa, JS_NewStringCopyZ(cxa, a));
args[0].setString(m);
bool ok = JS_CallFunctionName(cxa, parent, name, args, &retval);
if(!ok) {
// error in execution
	if (intFlag)
		i_puts(MSG_Interrupted);
	processError();
	debugPrint(3, "failure on %s(%s)", name, a);
	uptrace(parent);
} // error
}

/*********************************************************************
The _t functions take a tag and bounce through the object
linked to that tag. These correspond to the _o functions but we may not
need all of them.
Unlike the _o functions, the _t functions set the compartment
according to t->f0.
*********************************************************************/

char *get_property_url_t(const Tag *t, bool action)
{
if(!t->jslink || !allowJS)
return 0;
JSAutoCompartment ac(cxa, frameToCompartment(t->f0));
JS::RootedObject obj(cxa, tagToObject(t));
if(!obj)
return 0;
return get_property_url_o(obj, action);
}

char *get_property_string_t(const Tag *t, const char *name)
{
if(!t->jslink || !allowJS)
return 0;
JSAutoCompartment ac(cxa, frameToCompartment(t->f0));
JS::RootedObject obj(cxa, tagToObject(t));
if(!obj)
return 0;
return get_property_string_o(obj, name);
}

bool get_property_bool_t(const Tag *t, const char *name)
{
if(!t->jslink || !allowJS)
return false;
JSAutoCompartment ac(cxa, frameToCompartment(t->f0));
JS::RootedObject obj(cxa, tagToObject(t));
if(!obj)
return false;
return get_property_bool_o(obj, name);
}

int get_property_number_t(const Tag *t, const char *name)
{
if(!t->jslink || !allowJS)
return -1;
JSAutoCompartment ac(cxa, frameToCompartment(t->f0));
JS::RootedObject obj(cxa, tagToObject(t));
if(!obj)
return -1;
return get_property_number_o(obj, name);
}

enum ej_proptype typeof_property_t(const Tag *t, const char *name)
{
if(!t->jslink || !allowJS)
return EJ_PROP_NONE;
JSAutoCompartment ac(cxa, frameToCompartment(t->f0));
JS::RootedObject obj(cxa, tagToObject(t));
if(!obj)
return EJ_PROP_NONE;
return typeof_property_o(obj, name);
}

char *get_style_string_t(const Tag *t, const char *name)
{
if(!t->jslink || !allowJS)
return 0;
JSAutoCompartment ac(cxa, frameToCompartment(t->f0));
JS::RootedObject obj(cxa, tagToObject(t));
if(!obj)
return 0;
JS::RootedObject style(cxa, get_property_object_o(obj, "style"));
if(!style)
return 0;
return get_property_string_o(style, name);
}

void delete_property_t(const Tag *t, const char *name)
{
if(!t->jslink || !allowJS)
return;
JSAutoCompartment ac(cxa, frameToCompartment(t->f0));
JS::RootedObject obj(cxa, tagToObject(t));
if(obj)
delete_property_o(obj, name);
}

void delete_property_win(const Frame *f, const char *name)
{
if(!f->jslink || !allowJS)
return;
JSAutoCompartment ac(cxa, frameToCompartment(f));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa)); // global
delete_property_o(g, name);
}

void delete_property_doc(const Frame *f, const char *name)
{
if(!f->jslink || !allowJS)
return;
JSAutoCompartment ac(cxa, frameToCompartment(f));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa)); // global
JS::RootedObject doc(cxa, get_property_object_o(g, "document"));
if(doc)
delete_property_o(doc, name);
}

void set_property_string_t(const Tag *t, const char *name, const char *v)
{
if(!t->jslink || !allowJS)
return;
JSAutoCompartment ac(cxa, frameToCompartment(t->f0));
JS::RootedObject obj(cxa, tagToObject(t));
if(!obj)
return;
set_property_string_o(obj, name, v);
}

void set_property_bool_t(const Tag *t, const char *name, bool v)
{
if(!t->jslink || !allowJS)
return;
JSAutoCompartment ac(cxa, frameToCompartment(t->f0));
JS::RootedObject obj(cxa, tagToObject(t));
if(!obj)
return;
set_property_bool_o(obj, name, v);
}

void set_property_number_t(const Tag *t, const char *name, int v)
{
if(!t->jslink || !allowJS)
return;
JSAutoCompartment ac(cxa, frameToCompartment(t->f0));
JS::RootedObject obj(cxa, tagToObject(t));
if(!obj)
return;
set_property_number_o(obj, name, v);
}

/*********************************************************************
Node has encountered an error, perhaps in its handler.
Find the location of this node within the dom tree.
As you climb up the tree, check for parentNode = null.
null is an object so it passes the object test.
This should never happen, but does in http://4x4dorogi.net
Also check for recursion.
If there is an error fetching nodeName or class, e.g. when the node is null,
(if we didn't check for parentNode = null in the above),
then asking for nodeName causes yet another runtime error.
This invokes our machinery again, including uptrace if debug is on,
and it invokes the js engine again as well.
The resulting core dump has the stack so corrupted, that gdb is hopelessly confused.
*********************************************************************/

static void uptrace(JS::HandleObject start)
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
JS::RootedValue v(cxa);
JS::RootedObject node(cxa);
node = start;
	while (true) {
		const char *nn, *cn;	// node name class name
		char nnbuf[MAXTAGNAME];
if(JS_GetProperty(cxa, node, "nodeName", &v) && v.isString())
nn = stringize(v);
		else
			nn = "?";
		strncpy(nnbuf, nn, MAXTAGNAME);
		nnbuf[MAXTAGNAME - 1] = 0;
		if (!nnbuf[0])
			strcpy(nnbuf, "?");
if(JS_GetProperty(cxa, node, "class", &v) && v.isString())
cn = stringize(v);
		else
			cn = emptyString;
		debugPrint(3, "%s.%s", nnbuf, (cn[0] ? cn : "?"));
if(!JS_GetProperty(cxa, node, "parentNode", &v) || !v.isObject()) {
// we're done.
			break;
		}
		t = top_proptype(v);
		if(t == EJ_PROP_NULL) {
			debugPrint(3, "null");
			break;
		}
		if(t != EJ_PROP_OBJECT) {
			debugPrint(3, "parentNode not object, type %d", t);
			break;
		}
JS_ValueToObject(cxa, v, &node);
	}
	debugPrint(3, "end uptrace");
	infunction = false;
}

static void processError(void)
{
if(!JS_IsExceptionPending(cxa))
return;
JS::RootedValue exception(cxa);
if(JS_GetPendingException(cxa,&exception) &&
exception.isObject()) {
// I don't think we need this line.
// JS::AutoSaveExceptionState savedExc(cxa);
JS::RootedObject exceptionObject(cxa,
&exception.toObject());
JSErrorReport *what = JS_ErrorFromException(cxa,exceptionObject);
if(what) {
	if (debugLevel >= 3) {
/* print message, this will be in English, and mostly for our debugging */
		if (what->filename && !stringEqual(what->filename, "noname")) {
			if (debugFile)
				fprintf(debugFile, "%s line %d: ", what->filename, what->lineno);
			else
				printf("%s line %d: ", what->filename, what->lineno);
		}
		debugPrint(3, "%s", what->message().c_str());
	}
}
}
JS_ClearPendingException(cxa);
}

static void jsInterruptCheck(void)
{
if(intFlag) {
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa)); // global
JS::RootedValue v(cxa);
// this next line should fail and stop the script!
// Assuming we aren't in a try{} block.
JS_CallFunctionName(cxa, g, "eb$stopexec", JS::HandleValueArray::empty(), &v);
// It didn't stop the script, oh well.
}
}

// Returns the result of the script as a string, from stringize(), not allocated,
// copy it if you want to keep it any longer then the next call to stringize.
// This function nad it's duktape counterpart ignores obj
// Assumes the appropriate commpartment has been set.
static const char *run_script_o(JS::HandleObject obj, const char *s, const char *filename, int lineno)
{
	char *s2 = 0;

if(!s || !*s)
return 0;

// special debugging code to replace bp@ and trace@ with expanded macros.
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

        JS::CompileOptions opts(cxa);
        opts.setFileAndLine(filename, lineno);
JS::RootedValue v(cxa);
if(s2) s = s2;
        bool ok = JS::Evaluate(cxa, opts, s, strlen(s), &v);
	nzFree(s2);
	if (intFlag)
		i_puts(MSG_Interrupted);
	if (ok) {
s = stringize(v);
		if (s && !*s)
			s = 0;
return s;
}
		processError();
return 0;
	}

/* like the above but throw away the result */
void jsRunScriptWin(const char *str, const char *filename, 		 int lineno)
{
if(!cf->jslink || !allowJS)
return;
JSAutoCompartment ac(cxa, frameToCompartment(cf));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa)); // global
	run_script_o(g, str, filename, lineno);
}

void jsRunScript_t(const Tag *t, const char *str, const char *filename, 		 int lineno)
{
if(!t->jslink || !allowJS)
return;
JSAutoCompartment ac(cxa, frameToCompartment(t->f0));
JS::RootedObject tojb(cxa, tagToObject(t));
	run_script_o(tojb, str, filename, lineno);
}

char *jsRunScriptWinResult(const char *str,
const char *filename, 			int lineno)
{
if(!cf->jslink || !allowJS)
return 0;
JSAutoCompartment ac(cxa, frameToCompartment(cf));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa)); // global
const char *s = run_script_o(g, str, filename, lineno);
// This is and has to be copied in the duktape world,
// so we do the same here for consistency.
return cloneString(s);
}

// Like any JSObject * return, you must put it directly into a rooted object.
static JSObject *create_event_o(JS::HandleObject parent, const char *evname)
{
JS::RootedObject e(cxa);
	const char *evname1 = evname;
	if (evname[0] == 'o' && evname[1] == 'n')
		evname1 += 2;
// gc$event protects from garbage collection
	e = instantiate_o(parent, "gc$event", "Event");
	set_property_string_o(e, "type", evname1);
	return e;
}

static void unlink_event_o(JS::HandleObject parent)
{
	delete_property_o(parent, "gc$event");
}

static bool run_event_o(JS::HandleObject obj, const char *pname, const char *evname)
{
	int rc;
	JS::RootedObject eo(cxa);	// created event object
	if(typeof_property_o(obj, evname) != EJ_PROP_FUNCTION)
		return true;
	if (debugLevel >= 3) {
		if (debugEvent) {
			int seqno = get_property_number_o(obj, "eb$seqno");
			debugPrint(3, "trigger %s tag %d %s", pname, seqno, evname);
		}
	}
	eo = create_event_o(obj, evname);
	set_property_object_o(eo, "target", obj);
	set_property_object_o(eo, "currentTarget", obj);
	set_property_number_o(eo, "eventPhase", 2);
	rc = run_function_onearg_o(obj, evname, eo);
	unlink_event_o(obj);
// no return or some other return is treated as true in this case
	if (rc < 0)
		rc = true;
	return rc;
}

bool run_event_t(const Tag *t, const char *pname, const char *evname)
{
	if (!allowJS || !t->jslink)
		return true;
JSAutoCompartment ac(cxa, frameToCompartment(t->f0));
JS::RootedObject tagobj(cxa, tagToObject(t));
	return run_event_o(tagobj, pname, evname);
}

// execute script.text code, wrapper around run_script_o
void jsRunData(const Tag *t, const char *filename, int lineno)
{
	bool rc;
	const char *s;
	if (!allowJS || !t->jslink)
		return;
	debugPrint(5, "> script:");
JS::RootedObject to(cxa, tagToObject(t));
JS::RootedValue v(cxa);
if(!JS_GetProperty(cxa, to, "text", &v) ||
!v.isString()) // no data
		return;
const char *s1 = stringize(v);
	if (!s1 || !*s1)
return;
// have to set currentScript
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
JS::RootedObject doc(cxa, get_property_object_o(g, "document"));
set_property_object_o(doc, "currentScript", to);
// running the script will almost surely run stringize again
char *s2 = cloneString(s1);
run_script_o(g, s2, filename, lineno);
delete_property_o(doc, "currentScript");
// onload handler? Should this run even if the script fails?
// Right now it does.
	if (t->js_file && !isDataURI(t->href) &&
	typeof_property_o(to, "onload") == EJ_PROP_FUNCTION)
		run_event_o(to, "script", "onload");
	debugPrint(5, "< ok");
}

bool run_event_win(const Frame *f, const char *pname, const char *evname)
{
	if (!allowJS || !f->jslink)
		return true;
JSAutoCompartment ac(cxa, frameToCompartment(f));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
	return run_event_o(g, pname, evname);
}

bool run_event_doc(const Frame *f, const char *pname, const char *evname)
{
	if (!allowJS || !f->jslink)
		return true;
JSAutoCompartment ac(cxa, frameToCompartment(f));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
JS::RootedObject doc(cxa, get_property_object_o(g, "document"));
	return run_event_o(doc, pname, evname);
}

bool bubble_event_t(const Tag *t, const char *name)
{
JS::RootedObject e(cxa); // the event object
	bool rc;
	if (!allowJS || !t->jslink)
		return true;
Frame *f = t->f0;
JSAutoCompartment ac(cxa, frameToCompartment(f));
JS::RootedObject tagobj(cxa, tagToObject(t));
	e = create_event_o(tagobj, name);
	rc = run_function_onearg_o(tagobj, "dispatchEvent", e);
	if (rc && get_property_bool_o(e, "prev$default"))
		rc = false;
	unlink_event_o(tagobj);
	return rc;
}

void set_master_bool(const char *name, bool v)
{
JSAutoCompartment ac(cxa, *mw0);
	set_property_bool_o(*mw0, name, v);
}

void set_property_bool_win(const Frame *f, const char *name, bool v)
{
JSAutoCompartment ac(cxa, frameToCompartment(f));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
	set_property_bool_o(g, name, v);
}

void set_property_string_win(const Frame *f, const char *name, const char *v)
{
JSAutoCompartment ac(cxa, frameToCompartment(f));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
	set_property_string_o(g, name, v);
}

void connectTagObject1(Tag *t, JS::HandleObject o)
{
char buf[16];
sprintf(buf, "o%d", t->gsn);
JS::RootedValue v(cxa);
v = JS::ObjectValue(*o);
JS_DefineProperty(cxa, *rw0, buf, v,
JSPROP_STD);
JS_DefineProperty(cxa, o, "eb$seqno", t->seqno,
(JSPROP_READONLY|JSPROP_PERMANENT));
JS_DefineProperty(cxa, o, "eb$gsn", t->gsn,
(JSPROP_READONLY|JSPROP_PERMANENT));
t->jslink = true;
}

void disconnectTagObject(Tag *t)
{
char buf[16];
sprintf(buf, "o%d", t->gsn);
JS_DeleteProperty(cxa, *rw0, buf);
t->jslink = false;
}

// I don't have any reverse pointers, so I'm just going to scan the list.
// This doesn't come up all that often.
// I assume we are in a compartment.
static Tag *tagFromObject(JS::HandleObject o)
{
	Tag *t;
	int i, gsn;
	if (!tagList)
		i_printfExit(MSG_NullListInform);
JS::RootedValue v(cxa);
if(!JS_GetProperty(cxa, o, "eb$gsn", &v) ||
!v.isInt32())
return NULL;
gsn = v.toInt32();
	for (i = 0; i < cw->numTags; ++i) {
		t = tagList[i];
if(t->dead) // not sure how this would happen
continue;
if(t->gsn == gsn)
			return t;
	}
	return 0;
}

// inverse of the above
static JSObject *tagToObject(const Tag *t)
{
if(!t->jslink)
return 0;
char buf[16];
sprintf(buf, "o%d", t->gsn);
JS::RootedValue v(cxa);
JS::RootedObject o(cxa);
if(JS_GetProperty(cxa, *rw0, buf, &v) &&
v.isObject()) {
JS_ValueToObject(cxa, v, &o);
// cast from rooted object to JSObject *
return o;
}
return 0;
}

/*********************************************************************
This function is usually called to set a compartment for a frame.
Unlike most functions in this file, I am not assuming a preexisting compartment.
Therefore, I link to the root window before doing anything.
And I always return some compartment, because any compartment
is better than none.
*********************************************************************/

static JSObject *frameToCompartment(const Frame *f)
{
if(!f->jslink)
goto fail;
{
char buf[16];
sprintf(buf, "g%d", f->gsn);
JS::RootedValue v(cxa);
JS::RootedObject o(cxa);
	        JSAutoCompartment ac(cxa, *rw0);
if(JS_GetProperty(cxa, *rw0, buf, &v) &&
v.isObject()) {
JS_ValueToObject(cxa, v, &o);
// cast from rooted object to JSObject *
return o;
}
}
fail:
debugPrint(1, "Warning: no compartment for frame %d", f->gsn);
return *rw0;
}

// Create a new tag for this pointer, only called from document.createElement().
static Tag *tagFromObject2(JS::HandleObject o, const char *tagname)
{
	Tag *t;
	if (!tagname)
		return 0;
	t = newTag(cf, tagname);
	if (!t) {
		debugPrint(3, "cannot create tag node %s", tagname);
		return 0;
	}
	connectTagObject1(t, o);
// this node now has a js object, don't decorate it again.
	t->step = 2;
// and don't render it unless it is linked into the active tree.
	t->deleted = true;
	return t;
}

// We need to call and remember up to 3 node names, to carry dom changes
// across to html. As in parent.insertBefore(newChild, existingChild);
// These names are passed into domSetsLinkage().
static const char *embedNodeName(JS::HandleObject obj)
{
	static char buf1[MAXTAGNAME], buf2[MAXTAGNAME], buf3[MAXTAGNAME];
	char *b;
	static int cycle = 0;
	const char *nodeName;
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

	{ // scope
JS::RootedValue v(cxa);
if(!JS_GetProperty(cxa, obj, "nodeName", &v))
goto done;
if(!v.isString())
goto done;
JSString *str = v.toString();
nodeName = JS_EncodeString(cxa, str);
		length = strlen(nodeName);
		if (length >= MAXTAGNAME)
			length = MAXTAGNAME - 1;
		strncpy(b, nodeName, length);
		b[length] = 0;
cnzFree(nodeName);
	caseShift(b, 'l');
	}

done:
	return b;
}				/* embedNodeName */

static void domSetsLinkage(char type,
JS::HandleObject p_j, const char *p_name,
JS::HandleObject a_j, const char *a_name,
JS::HandleObject b_j, const char *b_name)
{
	Tag *parent, *add, *before, *c, *t;
	int action;
	char *jst;		// javascript string

// Some functions in third.js create, link, and then remove nodes, before
// there is a document. Don't run any side effects in this case.
	if (!cw->tags)
		return;

jsInterruptCheck();

	if (type == 'c') {	/* create */
		parent = tagFromObject2(p_j, p_name);
		if (parent) {
			debugPrint(4, "linkage, %s %d created",
				   p_name, parent->seqno);
			if (parent->action == TAGACT_INPUT) {
// we need to establish the getter and setter for value
				set_property_string_o(p_j,
"value", emptyString);
			}
		}
		return;
	}

/* options are relinked by rebuildSelectors, not here. */
	if (stringEqual(p_name, "option"))
		return;
	if (stringEqual(a_name, "option"))
		return;

	parent = tagFromObject(p_j);
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
	t->name = get_property_string_o(a_j, "name");
	t->id = get_property_string_o(a_j, "id");
	t->jclass = get_property_string_o(a_j, "class");

	switch (action) {
	case TAGACT_INPUT:
		jst = get_property_string_o(a_j, "type");
		setTagAttr(t, "type", jst);
		t->value = get_property_string_o(a_j, "value");
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
		t->value = get_property_string_o(a_j, "value");
		if (!t->value)
			t->value = emptyString;
// Need to create the side buffer here.
		formControl(t, true);
		break;

	case TAGACT_SELECT:
		t->action = TAGACT_INPUT;
		t->itype = INP_SELECT;
		if (typeof_property_o(a_j, "multiple"))
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

// as above, with fewer parameters
static void domSetsLinkage(char type,
JS::HandleObject p_j, const char *p_name,
JS::HandleObject a_j, const char *a_name)
{
JS::RootedObject b_j(cxa);
domSetsLinkage(type, p_j, p_name, a_j, a_name, b_j, emptyString);
}

static void domSetsLinkage(char type,
JS::HandleObject p_j, const char *p_name)
{
JS::RootedObject a_j(cxa);
JS::RootedObject b_j(cxa);
domSetsLinkage(type, p_j, p_name, a_j, emptyString, b_j, emptyString);
}

static bool nat_logElement(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
args.rval().setUndefined();
if(argc != 2 ||
!args[0].isObject() || !args[1].isString())
return true;
	debugPrint(5, "log el 1");
JS::RootedObject obj(cxa);
JS_ValueToObject(cxa, args[0], &obj);
// this call creates the getter and setter for innerHTML
set_property_string_o(obj, "innerHTML", emptyString);
const char *tagname = stringize(args[1]);
domSetsLinkage('c', obj, tagname);
	debugPrint(5, "log el 2");
return true;
}

static bool nat_puts(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
if(argc >= 1) puts(stringize(args[0]));
args.rval().setUndefined();
  return true;
}

static bool nat_logputs(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
if(argc >= 2 && args[0].isInt32() && args[1].isString()) {
int lev = args[0].toInt32();
const char *s = stringize(args[1]);
	if (debugLevel >= lev && s && *s)
		debugPrint(3, "%s", s);
	jsInterruptCheck();
}
args.rval().setUndefined();
  return true;
}

static bool nat_prompt(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
	char *msg = 0;
	const char *answer = 0;
	char inbuf[80];
	if (argc > 0) {
		msg = cloneString(stringize(args[0]));
		if (argc > 1)
			answer = stringize(args[1]);
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
nzFree(msg);
if(!answer) answer = emptyString;
JS::RootedString m(cx, JS_NewStringCopyZ(cx, answer));
args.rval().setString(m);
return true;
}

static bool nat_confirm(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
	const char *msg = 0;
	bool answer = false, first = true;
	char c = 'n';
	char inbuf[80];
	if (argc > 0) {
		msg = stringize(args[0]);
	if (msg && *msg) {
		while (true) {
			printf("%s", msg);
			c = msg[strlen(msg) - 1];
			if (!isspace(c)) {
				if (!ispunct(c))
					printf(":");
				printf(" ");
			}
			if (first)
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
	}
	if (c == 'y' || c == 'Y')
		answer = true;
args.rval().setBoolean(answer);
return true;
}

static bool nat_winclose(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
	i_puts(MSG_PageDone);
// I should probably free the window and close down the script,
// but not sure I can do that while the js function is still running.
args.rval().setUndefined();
return true;
}

static bool nat_hasFocus(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
args.rval().setBoolean(foregroundWindow);
return true;
}

static bool nat_newloc(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
if(argc == 1) {
	const char *s = stringize(args[0]);
	if (s && *s) {
		char *t = cloneString(s);
// url on one line, name of window on next line
		char *u = strchr(t, '\n');
if(u)
		*u++ = 0;
else
u = emptyString;
		debugPrint(4, "window %s|%s", t, u);
		domOpensWindow(t, u);
		nzFree(t);
	}
	}
args.rval().setUndefined();
return true;
}

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

static bool nat_getcook(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
startCookie();
JS::RootedString m(cx, JS_NewStringCopyZ(cx, cookieCopy));
args.rval().setString(m);
  return true;
}

static bool nat_setcook(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
if(argc >= 1 && args[0].isString()) {
JSString *str = args[0].toString();
char *newcook = JS_EncodeString(cx, str);
char *s = strchr(newcook, '=');
if(s && s > newcook) {
JS::RootedValue v(cx);
JS::RootedObject g(cx, JS::CurrentGlobalOrNull(cx)); // global
if(JS_GetProperty(cx, g, "eb$url", &v) &&
v.isString()) {
JSString *str = v.toString();
char *es = JS_EncodeString(cx, str);
	receiveCookie(es, newcook);
free(es);
}
}
free(newcook);
}
args.rval().setUndefined();
  return true;
}

static bool nat_formSubmit(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
        JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
Tag *t = tagFromObject(thisobj);
	if(t && t->action == TAGACT_FORM) {
		debugPrint(3, "submit form tag %d", t->seqno);
		domSubmitsForm(t, false);
	} else {
		debugPrint(3, "submit form tag not found");
	}
args.rval().setUndefined();
  return true;
}

static bool nat_formReset(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
        JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
Tag *t = tagFromObject(thisobj);
	if(t && t->action == TAGACT_FORM) {
		debugPrint(3, "reset form tag %d", t->seqno);
		domSubmitsForm(t, true);
	} else {
		debugPrint(3, "reset form tag not found");
	}
args.rval().setUndefined();
  return true;
}

static bool nat_media(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
bool rc = false;
if(argc == 1 && args[0].isString()) {
		char *t = cloneString(stringize(args[0]));
		rc = matchMedia(t);
		nzFree(t);
	}
args.rval().setBoolean(rc);
return true;
}

const char *fakePropName(void)
{
	static char fakebuf[24];
	static int idx = 0;
	++idx;
	sprintf(fakebuf, "gc$$%d", idx);
	return fakebuf;
}

static void set_timer(JSContext *cx, unsigned argc, JS::Value *vp, bool isInterval)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
args.rval().setUndefined();
JS::RootedObject to(cx); // timer object
JS::RootedObject fo(cx); // function object
	int n = 1000;		/* default number of milliseconds */
if(!argc)
return;
	debugPrint(5, "timer 1");
// if second parameter is missing, leave milliseconds at 1000.
if(argc >= 2 && args[1].isInt32())
n = args[1].toInt32();
if(args[0].isObject()) {
// it's an object, should be a function, I'll check this time.
JS_ValueToObject(cx, args[0], &fo);
if(!JS_ObjectIsFunction(cx, fo))
return;
} else if(args[0].isString()) {
const char *source = stringize(args[0]);
JS::AutoObjectVector envChain(cx);
JS::CompileOptions opts(cx);
JS::RootedFunction f(cxa);
if(!JS::CompileFunction(cx, envChain, opts, "timer", 0, nullptr, source, strlen(source), &f)) {
		processError();
		debugPrint(3, "compile error for timer(){%s}", source);
	debugPrint(5, "timer 3");
return;
}
fo = JS_GetFunctionObject(f);
} else return;

JS::RootedObject g(cx, JS::CurrentGlobalOrNull(cx));
const char *	fpn = fakePropName();
// create the timer object and also protect it from gc
// by linking it to window, through the fake property name.
to = instantiate_o(g, fpn, "Timer");
// classs is milliseconds, for debugging
set_property_number_o(to, "class", n);
set_property_object_o(to, "ontimer", fo);
set_property_string_o(to, "backlink", fpn);
set_property_number_o(to, "tsn", ++timer_sn);
args.rval().setObject(*to);
	debugPrint(5, "timer 2");
	domSetsTimeout(n, "+", fpn, isInterval);
}

static bool nat_timer(JSContext *cx, unsigned argc, JS::Value *vp)
{
set_timer(cx, argc, vp, false);
  return true;
}

static bool nat_interval(JSContext *cx, unsigned argc, JS::Value *vp)
{
set_timer(cx, argc, vp, true);
  return true;
}

static bool nat_cleartimer(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
if(argc >= 1 && args[0].isObject()) {
JS::RootedObject to(cx);
JS_ValueToObject(cx, args[0], &to);
int tsn = get_property_number_o(to, "tsn");
char * fpn = get_property_string_o(to, "backlink");
// this call will unlink from the global, so gc can get rid of the timer object
	domSetsTimeout(tsn, "-", fpn, false);
nzFree(fpn);
}
args.rval().setUndefined();
  return true;
}

static bool nat_cssStart(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
args.rval().setUndefined();
JS::RootedObject start(cx);
JS_ValueToObject(cx, args[0], &start);
// cssDocLoad is a lot of code, and it may well call get_property_string,
// which calls stringize, so just to be safe, I'll copy it.
char *s = cloneString(stringize(args[1]));
bool r = args[2].toBoolean();
cssDocLoad(start, s, r);
nzFree(s);
return true;
}

static bool nat_cssApply(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
args.rval().setUndefined();
JS::RootedObject top(cx);
JS_ValueToObject(cx, args[0], &top);
JS::RootedObject node(cx);
JS_ValueToObject(cx, args[1], &node);
JS::RootedObject dest(cx);
JS_ValueToObject(cx, args[2], &dest);
cssApply(top, node, dest);
return true;
}

static bool nat_cssText(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
args.rval().setUndefined();
        JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
const char *rules = emptyString;
if(argc >= 1)
rules = stringize(args[0]);
cssText(thisobj, rules);
return true;
}

static bool nat_qsa(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
char *selstring = NULL;
JS::RootedObject start(cx);
if(argc >= 1 && args[0].isString()) {
JSString *s = args[0].toString();
selstring = JS_EncodeString(cx, s);
}
if(argc >= 2 && args[1].isObject())
JS_ValueToObject(cx, args[1], &start);
if(!start)
start = JS_THIS_OBJECT(cx, vp);
jsInterruptCheck();
//` call querySelectorAll in css.c
free(selstring);
// return empty array for now
args.rval().setObject(*JS_NewArrayObject(cx, 0));
  return true;
}

static bool nat_qs(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
char *selstring = NULL;
JS::RootedObject start(cx);
if(argc >= 1 && args[0].isString()) {
JSString *s = args[0].toString();
selstring = JS_EncodeString(cx, s);
}
if(argc >= 2 && args[1].isObject())
JS_ValueToObject(cx, args[1], &start);
if(!start)
start = JS_THIS_OBJECT(cx, vp);
jsInterruptCheck();
//` call querySelector in css.c
free(selstring);
// return undefined for now
args.rval().setUndefined();
return true;
}

static bool nat_qs0(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
char *selstring = NULL;
bool rc = false;
        JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
if(argc >= 1 && args[0].isString()) {
JSString *s = args[0].toString();
selstring = JS_EncodeString(cx, s);
}
jsInterruptCheck();
/*
if(selstring && *selstring)
	rc = querySelector0(selstring, root);
*/
free(selstring);
args.rval().setBoolean(rc);
return true;
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
frameExpandLine takes a line number or an object, not both.
One or the other will be 0.
If a line number, it comes from a user command, you asked to expand the frame.
If the object is not null, it is from a getter,
javascript is trying to access objects within that frame,
and now we need to expand it.
We're in a compartment, but who knows which one.
If you're expanding all the frames on the page, and some are in the top frame
and some are in lower frames, then the compartment will not be right for all of them.
If from a getter, the compartment is probably for the containing frame,
and will be wrong when you expand this frame.
For reading properties it doesn't matter, but when you expand
the frame, fetch the web page, create the dom, convert html into objects,
all that, the compartment MUST correspond to the global object of that frame.
*********************************************************************/

bool frameExpand(bool expand, int ln1, int ln2)
{
	int ln;			/* line number */
	int problem = 0, p;
	bool something_worked = false;

// make sure we're in a compartment
	        JSAutoCompartment ac(cxa, *rw0);

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

// We're in some compartment, the root compartment if t is nonzero
static int frameExpandLine(int ln, JS::HandleObject fo)
{
	bool fromget = !ln;
	pst line;
	int tagno, start;
	const char *s, *jssrc = 0;
	char *a;
	Tag *t;
	Frame *save_cf, *new_cf, *last_f;
	uchar save_local;
	Tag *cdt;	// contentDocument tag
	bool jslink; // frame tag is linked to objects
	JSObject *comp; // compartment

	if (fromget) {
		t = tagFromObject(fo);
		if (!t)
			return 1;
		jslink = true;
	} else {
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
		jslink = t->jslink;
	}

	if (t->action != TAGACT_FRAME)
		return 1;

// the easy case is if it's already been expanded before, we just unhide it.
// Remember that f1 is the subordinate frame, if t is a <frame> tag.
	if (t->f1) {
// If js is accessing objects in this frame, that doesn't mean we unhide it.
		if (!fromget)
			t->contracted = false;
		return 0;
	}

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
	last_f->next = cf = (Frame *)allocZeroMem(sizeof(Frame));
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
/* call the tidy parser to build the html nodes */
	html2nodes(serverData, true);
	nzFree(serverData);	/* don't need it any more */
	serverData = 0;
	htmlGenerated = false;
// in the edbrowse world, the only child of the frame tag
// is the contentDocument tag.
	cdt = t->firstchild;
// the placeholder document node will soon be orphaned.
	delete_property_t(cdt, "parentNode");
	htmlNodesIntoTree(start, cdt);
	cdt->step = 0;
	prerender(0);

/*********************************************************************
At this point cdt->step is 1; the html tree is built, but not decorated.
I already put the object on cdt manually. Besides, we don't want to set up
the fake cdt object and the getter that auto-expands the frame,
we did that before and now it's being expanded. So bump step up to 2.
*********************************************************************/
	cdt->step = 2;

JS::RootedObject cwo(cxa); // the content window object
JS::RootedObject cdo(cxa); // the content document object
JS::RootedObject prev(cxa); // the previous frame
JS::RootedObject fto(cxa); // the frame tag object

	if (cf->jslink) {
// global for the current frame becomes the content window object
cwo = frameToCompartment(cf);
JSAutoCompartment ac(cxa, cwo);
// and for the previous frame
prev = frameToCompartment(save_cf);
		decorate(0);
		set_basehref(cf->hbase);
// parent points to the containing frame.
		set_property_object_o(cwo, "parent", prev);
// And top points to the top.
JS::RootedObject top(cxa, get_property_object_o(prev, "top"));
set_property_object_o(cwo, "top", top);
// frame tag object
fto = tagToObject(t);
		set_property_object_o(cwo, "frameElement", fto);
		run_function_bool_o(cwo, "eb$qs$start");
		if(jssrc)
			jsRunScriptWin(jssrc, "frame.src", 1);
		runScriptsPending(true);
		runOnload();
		runScriptsPending(false);
// get current document object from current window object
cdo = get_property_object_o(cwo, "document");
		set_property_string_o(cdo, "readyState", "complete");
		run_event_o(cdo, "document", "onreadystatechange");
		runScriptsPending(false);
		rebuildSelectors();
	}
	cnzFree(jssrc);

	if (cf->fileName) {
		int j = strlen(cf->fileName);
		cf->fileName = (char *)reallocMem(cf->fileName, j + 8);
		strcat(cf->fileName, ".browse");
	}

	t->f1 = cf;
	cf = save_cf;
	browseLocal = save_local;
	if (fromget)
		t->contracted = true;
	if (new_cf->jslink) {
// Be in the compartment of the higher frame.
JSAutoCompartment ac(cxa, prev);
		disconnectTagObject(cdt);
		connectTagObject1(cdt, cdo);
		cdt->style = 0;
		cdt->ssn = 0;
// Should I switch this tag into the new frame? I don't really know.
		cdt->f0 = new_cf;
		set_property_object_o(fto, "content$Window", cwo);
		set_property_object_o(fto, "content$Document", cdo);
JS::RootedObject cna(cxa);	// childNodes array
		cna = get_property_object_o(fto, "childNodes");
		set_array_element_object_o(cna, 0, cdo);
		set_property_object_o(cdo, "parentNode", fto);
// run the frame onload function if it is there.
// I assume it should run in the higher frame.
// Hope so cause that is the current compartment.
		run_event_o(fto, t->info->name, "onload");
	}

// success, frame is expanded
	return 0;
}

// This is called when js runs window.location = new_url
// which replaces the page but the page is in a frame.
bool reexpandFrame(void)
{
	int j, start;
	Tag *frametag;
	Tag *cdt;	// contentDocument tag
	uchar save_local;
	bool rc;
	JS::RootedObject save_top(cxa), save_parent(cxa), save_fe(cxa);

	cf = newloc_f;
	frametag = cf->frametag;
	cdt = frametag->firstchild;
// I think we can do this part in any compartment
	JS::RootedObject g(cxa, frameToCompartment(cf));
JS::RootedObject doc(cxa);
	save_top = get_property_object_o(g, "top");
	save_parent = get_property_object_o(g, "parent");
	save_fe = get_property_object_o(g, "frameElement");

// Cut away our tree nodes from the previous document, which are now inaccessible.
	underKill(cdt);

// the previous document node will soon be orphaned.
	delete_property_t(cdt, "parentNode");
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
		return false;
	}

	if (serverData == NULL) {
/* frame replaced itself with a playable stream, what to do? */
		fileSize = -1;
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

	if (cf->jslink) {
// we better be in the compartment of the new web page
g = frameToCompartment(cf);
JSAutoCompartment ac(cxa, g);
		decorate(0);
		set_basehref(cf->hbase);
		set_property_object_o(g, "top", save_top);
		set_property_object_o(g, "parent", save_parent);
		set_property_object_o(g, "frameElement", save_fe);
		run_function_bool_o(g, "eb$qs$start");
		runScriptsPending(true);
		runOnload();
		runScriptsPending(false);
doc = get_property_object_o(g, "document");
		set_property_string_o(doc, "readyState", "complete");
		run_event_o(doc, "document", "onreadystatechange");
		runScriptsPending(false);
		rebuildSelectors();
	}

	j = strlen(cf->fileName);
	cf->fileName = (char *)reallocMem(cf->fileName, j + 8);
	strcat(cf->fileName, ".browse");
	browseLocal = save_local;

	if (cf->jslink) {
// Remember, g is the new content window object,
// and doc is the new content document object.
		Frame *save_cf;
		disconnectTagObject(cdt);
		connectTagObject1(cdt, doc);
		cdt->style = 0;
		cdt->ssn = 0;
// it should already be set to cf, since this is a replacement
		cdt->f0 = cf;
JS::RootedObject cna(cxa);	// childNodes array
// have to point contentDocument to the new document object,
// but that requires a change of context.
		save_cf = cf;
		cf = frametag->f0;
{
JSAutoCompartment ac(cxa, frameToCompartment(cf));
// save_fe is conveniently the object that goes with frametag
		set_property_object_o(save_fe, "content$Window", g);
		set_property_object_o(save_fe, "content$Document", doc);
		cna = get_property_object_o(save_fe, "childNodes");
		set_array_element_object_o(cna, 0, doc);
		set_property_object_o(doc, "parentNode", save_fe);
}
		cf = save_cf;
	}

	return true;
}

static int frameContractLine(int ln)
{
	Tag *t = line2frame(ln);
	if (!t)
		return 1;
	t->contracted = true;
	return 0;
}

static bool remember_contracted;

static bool nat_unframe(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
	if(argc == 2 && args[0].isObject() && args[1].isObject()) {
JS::RootedObject fobj(cx), newdoc(cx);
JS_ValueToObject(cx, args[0], &fobj);
JS_ValueToObject(cx, args[1], &newdoc);
		int i, n;
		Tag *t, *cdt;
		Frame *f, *f1;
		t = tagFromObject(fobj);
		if (!t) {
			debugPrint(1, "unframe couldn't find tag");
			goto done;
		}
		if (!(cdt = t->firstchild) || cdt->action != TAGACT_DOC ||
		cdt->sibling || !(tagToObject(cdt))) {
			debugPrint(1, "unframe child tag isn't right");
			goto done;
		}
		underKill(cdt);
		disconnectTagObject(cdt);
		connectTagObject1(cdt, newdoc);
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
args.rval().setUndefined();
return true;
}

static bool nat_unframe2(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
if(argc == 1 && args[0].isObject()) {
JS::RootedObject fobj(cx);
JS_ValueToObject(cx, args[0], &fobj);
	Tag *t = tagFromObject(fobj);
if(t)
	t->contracted = remember_contracted;
}
args.rval().setUndefined();
return true;
}

static bool nat_resolve(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
if(argc == 2 && args[0].isString() && args[1].isString()) {
char * base = cloneString(stringize(args[0]));
const char * rel = stringize(args[1]);
	if (!base)
		base = emptyString;
	if (!rel)
		rel = emptyString;
	char *outgoing_url = resolveURL(base, rel);
	if (outgoing_url == NULL)
		outgoing_url = emptyString;
JS::RootedString m(cx, JS_NewStringCopyZ(cx, outgoing_url));
args.rval().setString(m);
	nzFree(outgoing_url);
return true;
}
args.rval().setUndefined();
return true;
}

static bool nat_mywin(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
args.rval().setObject(*JS::CurrentGlobalOrNull(cx));
  return true;
}

static bool nat_mydoc(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
JS::RootedObject g(cx); // global
g = JS::CurrentGlobalOrNull(cx);
JS::RootedValue v(cx);
        if (JS_GetProperty(cx, g, "document", &v) &&
v.isObject()) {
args.rval().set(v);
} else {
// no document; this should never happen.
args.rval().setUndefined();
}
  return true;
}

// This is really native apch1 and apch2, so just carry cx along.
static void append0(JSContext *cx, unsigned argc, JS::Value *vp, bool side)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
	unsigned i, length;
	const char *thisname, *childname;
bool isarray;

	debugPrint(5, "append 1");
// we need one argument that is an object
if(argc != 1 || !args[0].isObject())
goto fail;

	{ // scope
JS::RootedObject child(cx);
JS_ValueToObject(cx, args[0], &child);
        JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
      JS::RootedValue v(cx);
        if (!JS_GetProperty(cx, thisobj, "childNodes", &v) ||
!v.isObject())
		goto fail;
JS_IsArrayObject(cx, v, &isarray);
if(!isarray)
goto fail;
JS::RootedObject cna(cx); // child nodes array
JS_ValueToObject(cx, v, &cna);
if(!JS_GetArrayLength(cx, cna, &length))
goto fail;
// see if child is already there.
	for (i = 0; i < length; ++i) {
if(!JS_GetElement(cx, cna, i, &v))
continue; // should never happen
if(!v.isObject())
continue; // should never happen
JS::RootedObject elem(cx);
JS_ValueToObject(cx, v, &elem);
// overloaded == compares the object pointers inside the rooted structures
if(elem == child) {
// child was already there, am I suppose to move it to the end?
// I don't know, I just return.
			goto done;
		}
	}

// add child to the end
JS_DefineElement(cx, cna, length, args[0], JSPROP_STD);
v = JS::ObjectValue(*thisobj);
JS_DefineProperty(cx, child, "parentNode", v, JSPROP_STD);

	if (!side)
		goto done;

// pass this linkage information back to edbrowse, to update its dom tree
	thisname = embedNodeName(thisobj);
	childname = embedNodeName(child);
domSetsLinkage('a', thisobj, thisname, child, childname);
	}

done:
	debugPrint(5, "append 2");
// return the child that was appended
args.rval().set(args[0]);
return;

fail:
	debugPrint(5, "append 3");
args.rval().setNull();
}

static bool nat_apch1(JSContext *cx, unsigned argc, JS::Value *vp)
{
append0(cx, argc, vp, false);
  return true;
}

static bool nat_apch2(JSContext *cx, unsigned argc, JS::Value *vp)
{
append0(cx, argc, vp, true);
  return true;
}

static bool nat_removeChild(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
	unsigned i, length;
	const char *thisname, *childname;
int mark;
bool isarray;

	debugPrint(5, "remove 1");
// we need one argument that is an object
if(argc != 1 || !args[0].isObject())
		goto fail;

	{ // scope
JS::RootedObject child(cx);
JS_ValueToObject(cx, args[0], &child);
        JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
      JS::RootedValue v(cx);
        if (!JS_GetProperty(cx, thisobj, "childNodes", &v) ||
!v.isObject())
		goto fail;
JS_IsArrayObject(cx, v, &isarray);
if(!isarray)
goto fail;
JS::RootedObject cna(cx); // child nodes array
JS_ValueToObject(cx, v, &cna);
if(!JS_GetArrayLength(cx, cna, &length))
goto fail;
// see if child is already there.
mark = -1;
	for (i = 0; i < length; ++i) {
if(!JS_GetElement(cx, cna, i, &v))
continue; // should never happen
if(!v.isObject())
continue; // should never happen
JS::RootedObject elem(cx);
JS_ValueToObject(cx, v, &elem);
// overloaded == compares the object pointers inside the rooted structures
if(elem == child) {
mark = i;
break;
}
	}
if(mark < 0)
goto fail;

// pull the other elements down
	for (i = mark + 1; i < length; ++i) {
JS_GetElement(cx, cna, i, &v);
JS_SetElement(cx, cna, i-1, v);
}
JS_SetArrayLength(cx, cna, length-1);
// missing parentnode must always be null
v.setNull();
JS_SetProperty(cx, child, "parentNode", v);

// pass this linkage information back to edbrowse, to update its dom tree
	thisname = embedNodeName(thisobj);
	childname = embedNodeName(child);
domSetsLinkage('r', thisobj, thisname, child, childname);

// return the child upon success
args.rval().set(args[0]);
	debugPrint(5, "remove 2");

// mutFixup(this, false, mark, child);
// This illustrates why most of our dom is writtten in javascript.
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
JS::AutoValueArray<4> ma(cxa); // mutfix arguments
// what's wrong with assigning this directly to ma[0]?
ma[0].setObject(*thisobj);
ma[1].setBoolean(false);
ma[2].setInt32(mark);
ma[3].set(args[0]);
JS_CallFunctionName(cxa, g, "mutFixup", ma, &v);

return true;
	}

fail:
	debugPrint(5, "remove 3");
args.rval().setNull();
  return true;
}

// low level insert before
static bool nat_insbf(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
	unsigned i, length;
	const char *thisname, *childname, *itemname;
int mark;
bool isarray;

	debugPrint(5, "before 1");
// we need two objects
if(argc != 2 || !args[0].isObject() || !args[1].isObject())
		goto fail;

	{ // scope
JS::RootedObject child(cx);
JS_ValueToObject(cx, args[0], &child);
JS::RootedObject item(cx);
JS_ValueToObject(cx, args[1], &item);
        JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
      JS::RootedValue v(cx);
        if (!JS_GetProperty(cx, thisobj, "childNodes", &v) ||
!v.isObject())
		goto fail;
JS_IsArrayObject(cx, v, &isarray);
if(!isarray)
goto fail;
JS::RootedObject cna(cx); // child nodes array
JS_ValueToObject(cx, v, &cna);
if(!JS_GetArrayLength(cx, cna, &length))
goto fail;
// see if child or item is already there.
mark = -1;
	for (i = 0; i < length; ++i) {
if(!JS_GetElement(cx, cna, i, &v))
continue; // should never happen
if(!v.isObject())
continue; // should never happen
JS::RootedObject elem(cx);
JS_ValueToObject(cx, v, &elem);
if(elem == child) {
// already there; should we move it?
// I don't know, so I just don't do anything.
goto done;
}
if(elem == item)
mark = i;
	}
if(mark < 0)
goto fail;

// push the other elements up
JS_SetArrayLength(cx, cna, length+1);
        for (i = length; i > (unsigned)mark; --i) {
JS_GetElement(cx, cna, i-1, &v);
JS_SetElement(cx, cna, i, v);
}

// add child in position
JS_DefineElement(cx, cna, mark, args[0], JSPROP_STD);
v = JS::ObjectValue(*thisobj);
JS_DefineProperty(cx, child, "parentNode", v, JSPROP_STD);

// pass this linkage information back to edbrowse, to update its dom tree
	thisname = embedNodeName(thisobj);
	childname = embedNodeName(child);
	itemname = embedNodeName(item);
domSetsLinkage('b', thisobj, thisname, child, childname, item, itemname);
	}

done:
// return the child upon success
args.rval().set(args[0]);
	debugPrint(5, "before 2");
return true;

fail:
	debugPrint(5, "remove 3");
args.rval().setNull();
  return true;
}

// This is for the snapshot() feature; write a local file
static bool nat_wlf(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
args.rval().setUndefined();
if(argc != 2 ||
!args[0].isString() || !args[1].isString())
return true;
const char *filename = stringize(args[1]);
	int fh;
	bool safe = false;
	if (stringEqual(filename, "from") || stringEqual(filename, "jslocal"))
		safe = true;
	else if (filename[0] == 'f') {
int i;
		for (i = 1; isdigit(filename[i]); ++i) ;
		if (i > 1 && (stringEqual(filename + i, ".js") ||
			      stringEqual(filename + i, ".css")))
			safe = true;
	}
	if (!safe)
		return true;
	fh = open(filename, O_CREAT | O_TRUNC | O_WRONLY | O_TEXT, MODE_rw);
	if (fh < 0) {
		fprintf(stderr, "cannot create file %s\n", filename);
		return true;
	}
// save filename before the next stringize call
char *filecopy = cloneString(filename);
const char *s = stringize(args[0]);
	int len = strlen(s);
	if (write(fh, s, len) < len)
		fprintf(stderr, "cannot write file %s\n", filecopy);
	close(fh);
	if (stringEqual(filecopy, "jslocal"))
		writeShortCache();
free(filecopy);
	return true;
}

static bool nat_fetch(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
	struct i_get g;
	char *incoming_url = cloneString(stringize(args[0]));
	char *incoming_method = cloneString(stringize(args[1]));
	char *incoming_headers = cloneString(stringize(args[2]));
	char *incoming_payload = cloneString(stringize(args[3]));
	char *outgoing_xhrheaders = NULL;
	char *outgoing_xhrbody = NULL;
	char *a = NULL, methchar = '?';
	bool rc, async = false;

	debugPrint(5, "xhr 1");
JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
        JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
	if (down_jsbg)
async = get_property_bool_o(thisobj, "async");

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
nzFree(incoming_url);
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
		connectTagObject1(t, thisobj);
// This routine will return, and javascript might stop altogether; do we need
// to protect this object from garbage collection?
set_property_object_o(global, fpn, thisobj);
set_property_string_o(thisobj, "backlink", fpn);

t->href = incoming_url;
// overloading the innerHTML field
		t->innerHTML = incoming_headers;
nzFree(incoming_payload);
nzFree(incoming_method);
		if (cw->browseMode)
			scriptSetsTimeout(t);
		pthread_create(&t->loadthread, NULL, httpConnectBack3,
			       (void *)t);
args.rval().setBoolean(async);
return true;
}

// no async stuff, do the xhr now
	memset(&g, 0, sizeof(g));
	g.thisfile = cf->fileName;
	g.uriEncoded = true;
	g.url = incoming_url;
	g.custom_h = incoming_headers;
	g.headers_p = &outgoing_xhrheaders;
	rc = httpConnect(&g);
	outgoing_xhrbody = g.buffer;
jsInterruptCheck();
	if (outgoing_xhrheaders == NULL)
		outgoing_xhrheaders = emptyString;
	if (outgoing_xhrbody == NULL)
		outgoing_xhrbody = emptyString;
asprintf(&a, "%d\r\n\r\n%d\r\n\r\n%s%s",
rc, g.code, outgoing_xhrheaders, outgoing_xhrbody);
	nzFree(outgoing_xhrheaders);
	nzFree(outgoing_xhrbody);
nzFree(incoming_url);
nzFree(incoming_method);
nzFree(incoming_headers);
nzFree(incoming_payload);

	debugPrint(5, "xhr 2");
JS::RootedString m(cx, JS_NewStringCopyZ(cx, a));
args.rval().setString(m);
nzFree(a);
return true;
}

static Frame *thisFrame(JS::HandleObject thisobj)
{
int my_sn = get_property_number_o(thisobj, "eb$ctx");
	Frame *f;
	for (f = &(cw->f0); f; f = f->next)
if(f->gsn == my_sn)
break;
	return f;
}

/* start a document.write */
void dwStart(void)
{
	if (cf->dw)
		return;
	cf->dw = initString(&cf->dw_l);
	stringAndString(&cf->dw, &cf->dw_l, "<!DOCTYPE public><body>");
}				/* dwStart */

static void dwrite(JSContext *cx, unsigned argc, JS::Value *vp, bool newline)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
args.rval().setUndefined();
        JS::RootedObject thisobj(cxa, JS_THIS_OBJECT(cxa, vp));
int a_l;
char *a = initString(&a_l);
for(int i=0; i<argc; ++i)
stringAndString(&a, &a_l, stringize(args[i]));
	Frame *f, *save_cf = cf;
	f = thisFrame(thisobj);
	if (!f)
		debugPrint(3,    "no frame found for document.write, using the default");
	else {
		if (f != cf)
			debugPrint(3, "document.write on a different frame");
		cf = f;
	}
	dwStart();
	stringAndString(&cf->dw, &cf->dw_l, a);
	if (newline)
		stringAndChar(&cf->dw, &cf->dw_l, '\n');
	cf = save_cf;
}

static bool nat_write(JSContext *cx, unsigned argc, JS::Value *vp)
{
dwrite(cx, argc, vp, false);
  return true;
}

static bool nat_writeln(JSContext *cx, unsigned argc, JS::Value *vp)
{
dwrite(cx, argc, vp, true);
  return true;
}

static JSFunctionSpec nativeMethodsWindow[] = {
  JS_FN("eb$puts", nat_puts, 1, 0),
  JS_FN("eb$logputs", nat_logputs, 2, 0),
  JS_FN("prompt", nat_prompt, 1, 0),
  JS_FN("confirm", nat_confirm, 1, 0),
  JS_FN("close", nat_winclose, 0, 0),
  JS_FN("eb$newLocation", nat_newloc, 1, 0),
  JS_FN("eb$getcook", nat_getcook, 0, 0),
  JS_FN("eb$setcook", nat_setcook, 1, 0),
  JS_FN("eb$formSubmit", nat_formSubmit, 1, 0),
  JS_FN("eb$formReset", nat_formReset, 1, 0),
  JS_FN("eb$wlf", nat_wlf, 2, 0),
  JS_FN("eb$media", nat_media, 1, 0),
  JS_FN("eb$unframe", nat_unframe, 2, 0),
  JS_FN("eb$unframe2", nat_unframe2, 1, 0),
  JS_FN("eb$resolveURL", nat_resolve, 2, 0),
  JS_FN("setTimeout", nat_timer, 2, 0),
  JS_FN("setInterval", nat_interval, 2, 0),
  JS_FN("clearTimeout", nat_cleartimer, 1, 0),
  JS_FN("clearInterval", nat_cleartimer, 1, 0),
  JS_FN("eb$cssDocLoad", nat_cssStart, 3, 0),
  JS_FN("eb$cssApply", nat_cssApply, 3, 0),
  JS_FN("eb$cssText", nat_cssText, 1, 0),
  JS_FN("querySelectorAll", nat_qsa, 2, 0),
  JS_FN("querySelector", nat_qs, 2, 0),
  JS_FN("querySelector0", nat_qs0, 1, 0),
  JS_FN("my$win", nat_mywin, 0, 0),
  JS_FN("my$doc", nat_mydoc, 0, 0),
  JS_FN("eb$logElement", nat_logElement, 2, 0),
  JS_FN("eb$getter_cd", getter_cd, 0, 0),
  JS_FN("eb$getter_cw", getter_cw, 1, 0),
  JS_FN("eb$fetchHTTP", nat_fetch, 4, 0),
  JS_FS_END
};

static JSFunctionSpec nativeMethodsDocument[] = {
  JS_FN("hasFocus", nat_hasFocus, 0, 0),
  JS_FN("eb$apch1", nat_apch1, 1, 0),
  JS_FN("eb$apch2", nat_apch2, 1, 0),
  JS_FN("eb$insbf", nat_insbf, 1, 0),
  JS_FN("removeChild", nat_removeChild, 1, 0),
  JS_FN("write", nat_write, 0, 0),
  JS_FN("writeln", nat_writeln, 0, 0),
  JS_FS_END
};

static void setup_window_2(void);

// This is an edbrowse context, in a frame,
// nothing like the Mozilla js context.
void createJSContext(Frame *f)
{
int sn = f->gsn;
char buf[16];
sprintf(buf, "g%d", sn);
debugPrint(3, "create js context %d", sn);
      JS::CompartmentOptions options;
JSObject *g = JS_NewGlobalObject(cxa, &global_class, nullptr, JS::FireOnNewGlobalHook, options);
if(!g)
return;
JS::RootedObject global(cxa, g);
        JSAutoCompartment ac(cxa, g);
        JS_InitStandardClasses(cxa, global);
JS_DefineFunctions(cxa, global, nativeMethodsWindow);

JS::RootedValue objval(cxa); // object as value
objval = JS::ObjectValue(*global);
if(!JS_DefineProperty(cxa, *rw0, buf, objval, JSPROP_STD))
return;
f->jslink = true;

// Link back to the master window.
objval = JS::ObjectValue(**mw0);
JS_DefineProperty(cxa, global, "mw$", objval,
(JSPROP_READONLY|JSPROP_PERMANENT));

// Link to root window, debugging only.
// Don't do this in production; it's a huge security risk!
objval = JS::ObjectValue(**rw0);
JS_DefineProperty(cxa, global, "rw0", objval,
(JSPROP_READONLY|JSPROP_PERMANENT));

// window
objval = JS::ObjectValue(*global);
JS_DefineProperty(cxa, global, "window", objval,
(JSPROP_READONLY|JSPROP_PERMANENT));

// time for document under window
JS::RootedObject docroot(cxa, JS_NewObject(cxa, nullptr));
objval = JS::ObjectValue(*docroot);
JS_DefineProperty(cxa, global, "document", objval,
(JSPROP_READONLY|JSPROP_PERMANENT));
JS_DefineFunctions(cxa, docroot, nativeMethodsDocument);

set_property_number_o(docroot, "eb$seqno", 0);
set_property_number_o(docroot, "eb$ctx", sn);
// Sequence is to set f->fileName, then createContext(), so for a short time,
// we can rely on that variable.
// Let's make it more permanent, per context.
// Has to be nonwritable for security reasons.
JS::RootedValue v(cxa);
JS::RootedString m(cxa, JS_NewStringCopyZ(cxa, f->fileName));
v.setString(m);
JS_DefineProperty(cxa, global, "eb$url", v,
(JSPROP_READONLY|JSPROP_PERMANENT));

setup_window_2();
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
JS::RootedObject nav(cxa); // navigator object
JS::RootedObject navpi(cxa); // navigator plugins
JS::RootedObject navmt(cxa); // navigator mime types
JS::RootedObject hist(cxa); // history object
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
	struct MIMETYPE *mt;
	struct utsname ubuf;
	int i;
	char save_c;
	extern const char startWindowJS[];

// startwindow.js stored as an internal string
jsRunScriptWin(startWindowJS, "startwindow.js", 1);

	nav = get_property_object_o(g, "navigator");
	if (!nav)
		return;
// some of the navigator is in startwindow.js; the runtime properties are here.
	set_property_string_o(nav, "userLanguage", supported_languages[eb_lang]);
	set_property_string_o(nav, "language", supported_languages[eb_lang]);
	set_property_string_o(nav, "appVersion", version);
	set_property_string_o(nav, "vendorSub", version);
	set_property_string_o(nav, "userAgent", currentAgent);
	uname(&ubuf);
	set_property_string_o(nav, "oscpu", ubuf.sysname);
	set_property_string_o(nav, "platform", ubuf.machine);

/* Build the array of mime types and plugins,
 * according to the entries in the config file. */
	navpi = get_property_object_o(nav, "plugins");
	navmt = get_property_object_o(nav, "mimeTypes");
	if (!navpi || !navmt)
		return;
	mt = mimetypes;
	for (i = 0; i < maxMime; ++i, ++mt) {
		int len;
/* po is the plugin object and mo is the mime object */
JS::RootedObject 		po(cxa, instantiate_array_element_o(navpi, i, 0));
JS::RootedObject 		mo(cxa, instantiate_array_element_o(navmt, i, 0));
if(!po || !mo)
			return;
		set_property_object_o(mo, "enabledPlugin", po);
		set_property_string_o(mo, "type", mt->type);
		set_property_object_o(navmt, mt->type, mo);
		set_property_string_o(mo, "description", mt->desc);
		set_property_string_o(mo, "suffixes", mt->suffix);
/* I don't really have enough information from the config file to fill
 * in the attributes of the plugin object.
 * I'm just going to fake it.
 * Description will be the same as that of the mime type,
 * and the filename will be the program to run.
 * No idea if this is right or not. */
		set_property_string_o(po, "description", mt->desc);
		set_property_string_o(po, "filename", mt->program);
/* For the name, how about the program without its options? */
		len = strcspn(mt->program, " \t");
		save_c = mt->program[len];
		mt->program[len] = 0;
		set_property_string_o(po, "name", mt->program);
		mt->program[len] = save_c;
	}

	hist = get_property_object_o(g, "history");
	if (!hist)
		return;
	set_property_string_o(hist, "current", cf->fileName);

JS::RootedObject doc(cxa, get_property_object_o(g, "document"));
	set_property_string_o(doc, "referrer", cw->referrer);
	set_property_string_o(doc, "URL", cf->fileName);
	set_property_string_o(doc, "location", cf->fileName);
	set_property_string_o(g, "location", cf->fileName);
jsRunScriptWin(
		    "window.location.replace = document.location.replace = function(s) { this.href = s; };Object.defineProperty(window.location,'replace',{enumerable:false});Object.defineProperty(document.location,'replace',{enumerable:false});",
		    "locreplace", 1);
	set_property_string_o(doc, "domain", getHostURL(cf->fileName));
	if (debugClone)
		set_master_bool("cloneDebug", true);
	if (debugEvent)
		set_master_bool("eventDebug", true);
	if (debugThrow)
		set_master_bool("throwDebug", true);
}

// the garbage collector can eat this window
static void unlinkJSContext(int sn)
{
char buf[16];
sprintf(buf, "g%d", sn);
debugPrint(3, "remove js context %d", sn);
// I think we're already in a compartment, but just to be safe...
        JSAutoCompartment ac(cxa, *rw0);
JS_DeleteProperty(cxa, *rw0, buf);
}

void freeJSContext(Frame *f)
{
	debugPrint(5, "> free frame %d", f->gsn);
	if(f->jslink) {
		unlinkJSContext(f->gsn);
		 f->jslink = false;
	}
	f->cx = f->winobj = f->docobj = 0;
	debugPrint(5, "< ok");
	cssFree(f);
}				/* freeJSContext */

// Now we go back to the stand alone hello program.

// I don't understand any of this. Code from:
// http://mozilla.6506.n7.nabble.com/what-is-the-replacement-of-JS-SetErrorReporter-in-spidermonkey-60-td379888.html
// I assume all these variables are somehow on stack
// and get freed when the function returns.
static void ReportJSException(void)
{
if(JS_IsExceptionPending(cxa)) {
JS::RootedValue exception(cxa);
if(JS_GetPendingException(cxa,&exception) &&
exception.isObject()) {
// I don't think we need this line.
// JS::AutoSaveExceptionState savedExc(cxa);
JS::RootedObject exceptionObject(cxa,
&exception.toObject());
JSErrorReport *what =
JS_ErrorFromException(cxa,exceptionObject);
if(what) {
if(!stringEqual(what->filename, "noname"))
printf("%s line %d: ", what->filename, what->lineno);
puts(what->message().c_str());
// what->filename what->lineno
}
}
JS_ClearPendingException(cxa);
}
}

// This assumes you are in the compartment where you want to exec the file
static void execScript(const char *script)
{
        JS::CompileOptions opts(cxa);
        opts.setFileAndLine("noname", 0);
JS::RootedValue v(cxa);
        bool ok = JS::Evaluate(cxa, opts, script, strlen(script), &v);
if(!ok)
ReportJSException();
else
puts(stringize(v));
}

int main(int argc, const char *argv[])
{
bool iaflag = false; // interactive
int c; // compartment
int top; // number of windows
char buf[16];

// It's a test program, let's see the stuff.
debugLevel = 5;
selectLanguage();

static char myhome[] = "/snork";
home = myhome;

if(argc > 1 && !strcmp(argv[1], "-i")) iaflag = true;
top = iaflag ? 3 : 1;

    JS_Init();
// Mozilla assumes one context per thread; we can run all of edbrowse
// inside one context; I think.
cxa = JS_NewContext(JS::DefaultHeapMaxBytes);
if(!cxa) return 1;
    if (!JS::InitSelfHostedCode(cxa))         return 1;

// make rooting window and master window
	{
      JS::CompartmentOptions options;
rw0 = new       JS::RootedObject(cxa, JS_NewGlobalObject(cxa, &global_class, nullptr, JS::FireOnNewGlobalHook, options));
      if (!rw0)
          return 1;
        JSAutoCompartment ac(cxa, *rw0);
        JS_InitStandardClasses(cxa, *rw0);
	}

	{
	extern const char thirdJS[];
      JS::CompartmentOptions options;
mw0 = new       JS::RootedObject(cxa, JS_NewGlobalObject(cxa, &global_class, nullptr, JS::FireOnNewGlobalHook, options));
      if (!mw0)
          return 1;
        JSAutoCompartment ac(cxa, *mw0);
        JS_InitStandardClasses(cxa, *mw0);
JS_DefineFunctions(cxa, *mw0, nativeMethodsWindow);
// Link yourself to the master window.
JS::RootedValue objval(cxa); // object as value
objval = JS::ObjectValue(**mw0);
JS_DefineProperty(cxa, *mw0, "mw$", objval,
(JSPROP_READONLY|JSPROP_PERMANENT));
// need document, just for its native methods
JS::RootedObject docroot(cxa, JS_NewObject(cxa, nullptr));
objval = JS::ObjectValue(*docroot);
JS_DefineProperty(cxa, *mw0, "document", objval,
(JSPROP_READONLY|JSPROP_PERMANENT));
JS_DefineFunctions(cxa, docroot, nativeMethodsDocument);
// Do we need anything in the master window, besides our third party debugging tools?
jsRunScriptWin(thirdJS, "third.js", 1);
	}

for(c=0; c<top; ++c) {
sprintf(buf, "session %d", c+1);
cf->fileName = buf;
cf->gsn = c+1;
cf->owner = cw;
cf->jslink = false;
createJSContext(cf);
if(!cf->jslink) {
printf("create failed on %d\n", c+1); 
return 3;
}
}

c = 0; // back to the first window
//  puts("after loop");

{
static char tempname[] = "session 1";
cf->gsn = 1;
cf->fileName = tempname;
        JSAutoCompartment ac(cxa, frameToCompartment(cf));
execScript("'hello world, it is '+new Date()");
}

if(iaflag) {
// end with control d, EOF
char *line;
while(line = (char*)inputLine()) {
perl2c(line);
if(stringEqual(line, "q") || stringEqual(line, "qt"))
break;

// show context
if(stringEqual(line, "e")) {
puts(cf->fileName);
continue;
}

// change context?
if(line[0] == 'e' &&
line[1] >= '1' && line[1] <= '3' &&
!line[2]) {
static char tempname[16];
sprintf(tempname, "session %c", line[1]);
puts(tempname);
cf->fileName = tempname;
c = line[1] - '1';
cf->gsn = c+1;
continue;
}

	{
        JSAutoCompartment ac(cxa, frameToCompartment(cf));
JS::RootedValue v(cxa);
if(line[0] == '<') {
char *data;
int datalen;
if(fileIntoMemory(line+1, &data, &datalen)) {
jsRunScriptWin(data, line+1, 1);
nzFree(data);
puts("ok");
} else {
printf("cannot open %s\n", line+1);
}
} else {
char *res = jsRunScriptWinResult(line, "noname", 0);
if(res) puts(res);
nzFree(res);
}
}
}
}

// I should be able to remove globals in any order, need not be a stack
for(c=0; c<top; ++c)
unlinkJSContext(c+1);

// rooted objects have to free in the reverse (stack) order.
delete mw0;
delete rw0;

puts("destroy");
JS_DestroyContext(cxa);
    JS_ShutDown();
    return 0;
}
