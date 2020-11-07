// This program originally came from the Mozilla site, for moz 52.
// https://developer.mozilla.org/en-US/docs/Mozilla/Projects/SpiderMonkey/How_to_embed_the_JavaScript_engine
// I tweaked it for moz 60.
// Then I added so much stuff to it you'd hardly recognize it.
// It has become my sandbox.

#include <jsapi.h>
#include <js/Initialization.h>

#include <ctype.h>

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

// couple of native methods.
// increment the ascii letters of a string.  "hat" becomes "ibu"
static bool nat_letterInc(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
// This only works for strings
if(argc >= 1 && args[0].isString()) {
JSString *s = args[0].toString();
// believe s is implicitly inside args[0], thus delete s isn't necessary, and blows up.
char *es = JS_EncodeString(cx, s);
for(int i = 0; es[i]; ++i) ++es[i];
JS::RootedString m(cx, JS_NewStringCopyZ(cx, es));
args.rval().setString(m);
free(es);
} else {
args.rval().setUndefined();
}
  return true;
}

// decrement the ascii letters of a string.
static bool nat_letterDec(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
// This only works for strings
if(argc >= 1 && args[0].isString()) {
JSString *s = args[0].toString();
char *es = JS_EncodeString(cx, s);
for(int i = 0; es[i]; ++i) --es[i];
JS::RootedString m(cx, JS_NewStringCopyZ(cx, es));
args.rval().setString(m);
free(es);
} else {
args.rval().setUndefined();
}
  return true;
}

// A few functions from the edbrowse world, so I can write and test
// some other functions that rely on those functions.
// Jump in with both feet and see if we can swallow the edbrowse header files.

#include "eb.h"

void caseShift(char *s, char action) { }

char emptyString[] = "";
char *initString(int *l)
{
	*l = 0;
	return emptyString;
}
void stringAndString(char **s, int *l, const char *t)  { }

void nzFree(void *s)
{
	if (s && s != emptyString)
		free(s);
}
void cnzFree(const void *v)
{
	nzFree((void *)v);
}

void *allocMem(size_t n)
{
	void *s;
	if (!n)
		return emptyString;
	if (!(s = malloc(n)))
		i_printfExit(MSG_MemAllocError, n);
	return s;
}				/* allocMem */

char *allocString(size_t n)
{
	return (char *)allocMem(n);
}				/* allocString */

void *reallocMem(void *p, size_t n)
{
	void *s;
	if (!n)
		i_printfExit(MSG_ReallocP);
	if (!p)
		i_printfExit(MSG_Realloc0, n);
	if (p == emptyString) {
		p = allocMem(n);
// keep the null byte that was present in emptyString.
// fileIntoMemory() needs this to keep null on the end of an empty file
// that was just read into memory.
		*(char *)p = 0;
	}
	if (!(s = realloc(p, n)))
		i_printfExit(MSG_ErrorRealloc, n);
	return s;
}				/* reallocMem */

bool stringEqual(const char *s, const char *t)
{
	if (s == t)
		return true;
	if (!s || !t)
		return false;
	if (strcmp(s, t))
		return false;
	return true;
}

bool stringEqualCI(const char *s, const char *t)
{
	char c, d;
/* if two pointers are equal we can return */
	if (s == t)
		return true;
/* if one is NULL then the strings can't be equal */
	if (!s || !t)
		return false;
	while ((c = *s) && (d = *t)) {
		if (islower(c))
			c = toupper(c);
		if (islower(d))
			d = toupper(d);
		if (c != d)
			return false;
		++s, ++t;
	}
	if (*s)
		return false;
	if (*t)
		return false;
	return true;
}				/* stringEqualCI */

bool memEqualCI(const char *s, const char *t, int len)
{
	char c, d;
	if (s == t)
		return true;
	if (!s || !t)
		return false;
	while (len--) {
		c = *s, d = *t;
		if (islowerByte(c))
			c = toupper(c);
		if (islowerByte(d))
			d = toupper(d);
		if (c != d)
			return false;
		++s, ++t;
	}
	return true;
}				/* memEqualCI */

char *strmove(char *dest, const char *src)
{
	return (char *)memmove(dest, src, strlen(src) + 1);
}				/* strmove */

char *cloneString(const char *s)
{
	char *t;
	unsigned len;
	if (!s)
		return 0;
	if (!*s)
		return emptyString;
	len = strlen(s) + 1;
	t = allocString(len);
	strcpy(t, s);
	return t;
}				/* cloneString */

void debugPrint(int lev, const char *msg, ...)
{
puts(msg);
}

void i_printfExit(int crap, ...) { printf(" i_printfExit %d\n", crap); exit(4); }
void i_printf(int crap, ...) { printf(" i_printf %d\n", crap); }
void i_puts(int crap) { printf(" i_puts %d\n", crap); }

int sideBuffer(int cx, const char *text, int textlen, const char *bufname) { puts("side buffer"); return 0; }

static struct ebWindow win0;
struct ebWindow *cw = &win0;
Frame *cf = &win0.f0;
int context = 0, debugLevel = 3;
FILE *debugFile;
volatile bool intFlag;
struct ebSession sessionList[10];

Tag *newTag(const Frame *f, const char *tagname) { puts("new tag abort"); exit(4); }

void domSubmitsForm(JSObject *form, bool reset) { }
void prepareForField(char *h) { }
void htmlInputHelper(Tag *t) { }
void formControl(Tag *t, bool namecheck) { }
Tag *findOpenTag(Tag *t, int action) { return NULL; }
const char *getProtURL(const char *url)  { return "http"; }
void sendCookies(char **s, int *l, const char *url, bool issecure)  { }
bool receiveCookie(const char *url, const char *str)  { return true; }
void writeShortCache(void) { }

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

void set_property_object_o(JS::HandleObject parent, const char *name,  JS::HandleObject child)
{
JS::RootedValue v(cxa, JS::ObjectValue(*child));
	bool found;
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
if(argc != 1)
return true; // should never happen
  JS::CallArgs args = CallArgsFromVp(argc, vp);
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
// should we be setting result to anything?
args.rval().setUndefined();
	debugPrint(5, "setter v 2");
	return true;
}

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

static bool setter_innerHTML(JSContext *cx, unsigned argc, JS::Value *vp)
{
if(argc != 1)
return true; // should never happen
  JS::CallArgs args = CallArgsFromVp(argc, vp);
const char *h = stringize(args[0]);
if(!h)
h = emptyString;
	debugPrint(5, "setter h 1");
        JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
JS_SetProperty(cx, thisobj, "inner$HTML", args[0]);
	debugPrint(5, "setter h 2");
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
JS_DefineProperty(cxa, parent, name, getter, setter,
JSPROP_STD);
#else
if(setter)
JS_DefineProperty(cxa, parent, name, 0, JSPROP_STD,
getter, setter);
#endif
JS_DefineProperty(cxa, parent, altname, ourval, JSPROP_STD);
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
JS::Construct(cxa, v, JS::HandleValueArray::empty(), &a);
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
JS::Construct(cxa, v, JS::HandleValueArray::empty(), &a);
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

void connectTagObject(Tag *t, JS::HandleObject o)
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

static JSObject *frameToObject(int sn)
{
char buf[16];
sprintf(buf, "g%d", sn);
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
	connectTagObject(t, o);
// this node now has a js object, don't decorate it again.
	t->step = 2;
// and don't render it unless it is linked into the active tree.
	t->deleted = true;
	return t;
}

// Value is already allocated, name is not.
// So far only used by domSetsLinkage.
static void setTagAttr(Tag *t, const char *name, char *val)
{
	int nattr = 0;		/* number of attributes */
	int i = -1;
	if (!val)
		return;
	if (t->attributes) {
		for (nattr = 0; t->attributes[nattr]; ++nattr)
			if (stringEqualCI(name, t->attributes[nattr]))
				i = nattr;
	}
	if (i >= 0) {
		cnzFree(t->atvals[i]);
		t->atvals[i] = val;
		return;
	}
/* push */
	if (!nattr) {
		t->attributes = (const char**) allocMem(sizeof(char *) * 2);
		t->atvals = (const char**) allocMem(sizeof(char *) * 2);
	} else {
		t->attributes = (const char**) reallocMem(t->attributes, sizeof(char *) * (nattr + 2));
		t->atvals = (const char**) reallocMem(t->atvals, sizeof(char *) * (nattr + 2));
	}
	t->attributes[nattr] = cloneString(name);
	t->atvals[nattr] = val;
	++nattr;
	t->attributes[nattr] = 0;
	t->atvals[nattr] = 0;
}				/* setTagAttr */

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
domSubmitsForm(thisobj, false);
args.rval().setUndefined();
  return true;
}

static bool nat_formReset(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
        JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
domSubmitsForm(thisobj, true);
args.rval().setUndefined();
  return true;
}

static bool nat_qsa(JSContext *cx, unsigned argc, JS::Value *vp)
{
char *selstring = NULL;
JS::RootedObject start(cx);
  JS::CallArgs args = CallArgsFromVp(argc, vp);
if(argc >= 1 && args[0].isString()) {
JSString *s = args[0].toString();
selstring = JS_EncodeString(cx, s);
}
if(argc >= 2 && args[1].isObject()) {
JS_ValueToObject(cx, args[1], &start);
} else {
start = JS_THIS_OBJECT(cx, vp);
}
//` call querySelectorAll in css.c
free(selstring);
// return empty array for now. I don't understand this, But I guess it works.
// Is there an easier or safer way?
JS::RootedValue aov(cx); // array object value
aov = JS::ObjectValue(*JS_NewArrayObject(cx, 0));
args.rval().set(aov);
  return true;
}

static bool nat_mywin(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
JS::RootedValue v(cx);
v = JS::ObjectValue(*JS::CurrentGlobalOrNull(cx));
args.rval().set(v);
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

// mutFixup, not yet implemented

// return the child upon success
args.rval().set(args[0]);
	debugPrint(5, "remove 2");
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

// just stubs from here on out.
static bool nat_stub(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
args.rval().setUndefined();
  return true;
}

static JSFunctionSpec nativeMethodsWindow[] = {
  JS_FN("letterInc", nat_letterInc, 1, 0),
  JS_FN("letterDec", nat_letterDec, 1, 0),
  JS_FN("eb$puts", nat_puts, 1, 0),
  JS_FN("eb$getcook", nat_getcook, 0, 0),
  JS_FN("eb$setcook", nat_setcook, 1, 0),
  JS_FN("eb$formSubmit", nat_formSubmit, 1, 0),
  JS_FN("eb$formReset", nat_formReset, 1, 0),
  JS_FN("eb$wlf", nat_wlf, 2, 0),
  JS_FN("querySelectorAll", nat_qsa, 1, 0),
  JS_FN("querySelector", nat_stub, 1, 0),
  JS_FN("querySelector0", nat_stub, 1, 0),
  JS_FN("eb$cssText", nat_stub, 1, 0),
  JS_FN("my$win", nat_mywin, 0, 0),
  JS_FN("my$doc", nat_mydoc, 0, 0),
  JS_FN("eb$logElement", nat_logElement, 2, 0),
  JS_FS_END
};

static JSFunctionSpec nativeMethodsDocument[] = {
  JS_FN("eb$apch1", nat_apch1, 1, 0),
  JS_FN("eb$apch2", nat_apch2, 1, 0),
  JS_FN("eb$insbf", nat_insbf, 1, 0),
  JS_FN("removeChild", nat_removeChild, 1, 0),
  JS_FS_END
};

static void execFile(const char *filename);

// This is an edbrowse context, in a frame,
// nothing like the Mozilla js context.
bool createJSContext(int sn)
{
char buf[16];
sprintf(buf, "g%d", sn);
debugPrint(3, "create js context", sn);
      JS::CompartmentOptions options;
JSObject *g = JS_NewGlobalObject(cxa, &global_class, nullptr, JS::FireOnNewGlobalHook, options);
if(!g)
return false;
JS::RootedObject global(cxa, g);
        JSAutoCompartment ac(cxa, g);
        JS_InitStandardClasses(cxa, global);
JS_DefineFunctions(cxa, global, nativeMethodsWindow);

JS::RootedValue objval(cxa); // object as value
objval = JS::ObjectValue(*global);
if(!JS_DefineProperty(cxa, *rw0, buf, objval, JSPROP_STD))
return false;

// Link back to the master window.
objval = JS::ObjectValue(**mw0);
if(!JS_DefineProperty(cxa, global, "mw$", objval,
(JSPROP_READONLY|JSPROP_PERMANENT)))
return false;

// Link to root window, debugging only.
// Don't do this in production; it's a huge security risk!
objval = JS::ObjectValue(**rw0);
JS_DefineProperty(cxa, global, "rw0", objval,
(JSPROP_READONLY|JSPROP_PERMANENT));

// window
objval = JS::ObjectValue(*global);
if(!JS_DefineProperty(cxa, global, "window", objval,
(JSPROP_READONLY|JSPROP_PERMANENT)))
return false;

// time for document under window
JS::RootedObject docroot(cxa, JS_NewObject(cxa, nullptr));
objval = JS::ObjectValue(*docroot);
if(!JS_DefineProperty(cxa, global, "document", objval,
(JSPROP_READONLY|JSPROP_PERMANENT)))
return false;
JS_DefineFunctions(cxa, docroot, nativeMethodsDocument);

set_property_number_o(global, "eb$ctx", sn);
set_property_number_o(docroot, "eb$seqno", 0);
// Sequence is to set cf->fileName, then createContext(), so for a short time,
// we can rely on that variable.
// Let's make it more permanent, per context.
// Has to be nonwritable for security reasons.
JS::RootedValue v(cxa);
JS::RootedString m(cxa, JS_NewStringCopyZ(cxa, cf->fileName));
v.setString(m);
JS_DefineProperty(cxa, global, "eb$url", v,
(JSPROP_READONLY|JSPROP_PERMANENT));

execFile("startwindow.js");
return true;
}

void destroyJSContext(int sn)
{
char buf[16];
sprintf(buf, "g%d", sn);
debugPrint(3, "remove js context", sn);
        JSAutoCompartment ac(cxa, *rw0);
JS_DeleteProperty(cxa, *rw0, buf);
}

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
static void execFile(const char *filename)
{
        JS::CompileOptions opts(cxa);
        opts.setFileAndLine(filename, 1);
JS::RootedValue v(cxa);
        bool ok = JS::Evaluate(cxa, opts, filename, &v);
if(!ok) {
ReportJSException();
exit(2);
}
}

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
execFile("master.js");
execFile("third.js");
	}

for(c=0; c<top; ++c) {
sprintf(buf, "session %d", c+1);
cf->fileName = buf;
if(!createJSContext(c))
printf("create failed on %d\n", c+1);
}

c = 0; // back to the first window
//  puts("after loop");

{
JS::RootedValue v(cxa);
JS::RootedObject co(cxa); // current object
sprintf(buf, "g%d", c);
// WARNING! You have to be in a compartment to do the next GetProperty.
// If you're not, it will work, but something will seg fault later on,
// and it will be near impossible to debug.
	{
        JSAutoCompartment bc(cxa, *rw0);
if(!JS_GetProperty(cxa, *rw0, buf, &v) ||
!v.isObject()) {
printf("can't find global %s\n", buf);
exit(3);
}
JS_ValueToObject(cxa, v, &co);
}
        JSAutoCompartment ac(cxa, co);
execScript("letterInc('gdkkn')+letterDec('!xpsme') + ', it is '+new Date()");
}

if(iaflag) {
char line[500];
// end with control d, EOF
while(fgets(line, sizeof(line), stdin)) {
// should check for line too long here

// change context?
if(line[0] == 'e' &&
line[1] >= '1' && line[1] <= '3' &&
isspace(line[2])) {
printf("context %c\n", line[1]);
c = line[1] - '1';
continue;
}

JS::RootedValue v(cxa);
JS::RootedObject co(cxa); // current object
sprintf(buf, "g%d", c);
	{
        JSAutoCompartment bc(cxa, *rw0);
if(!JS_GetProperty(cxa, *rw0, buf, &v) ||
!v.isObject()) {
printf("can't find global %s\n", buf);
continue;
}
JS_ValueToObject(cxa, v, &co);
	}
        JSAutoCompartment ac(cxa, co);
execScript(line);
}
}

// I should be able to remove globals in any order, need not be a stack
for(c=0; c<top; ++c)
destroyJSContext(c);

// rooted objects have to free in the reverse (stack) order.
delete mw0;
delete rw0;

puts("destroy");
JS_DestroyContext(cxa);
    JS_ShutDown();
    return 0;
}
