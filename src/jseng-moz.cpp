/*********************************************************************
This is the back-end process for javascript.
We receive calls from edbrowse,
getting and setting properties for various DOM objects.
This is the mozilla spider monkey version.
If you package this with the mozilla libraries,
you will need to include the mozilla public license,
along with the GPL, general public license.
*********************************************************************/

#include <jsapi.h>
#include <js/Initialization.h>

#include "eb.h"

static JSClassOps global_ops = {
// different members in different versions, so specify explicitly
    trace : JS_GlobalObjectTraceHook
};

/* The class of the global object. */
static JSClass global_class = {
    "global",
    JSCLASS_GLOBAL_FLAGS,
    &global_ops
};

// This will become static, but global for now so I can play with hello.cpp.
JSContext *cxa; // the context for all
// I'll still use cx when it is passed in, as it must be for native methods.
// But it will be equal to cxa.

struct cro { // chain of rooted objects
	struct cro *prev;
	JS::RootedObject *m;
	bool inuse;
	bool global;
};
typedef struct cro Cro;
static Cro *o_tail; // chain of globals and tags

static void last3(void *v)
{
	unsigned long l = (unsigned long)v;
	l %= 1000;
	printf("%03lu", l);
}

// for debugging
void rootchain(void)
{
	JS::RootedObject o(cxa);
	void **l;
	int n;
	Cro *u;
	n = 0, l = (void**)&o;
#if 1
	while(l) {
last3(l[0]), printf(":"), last3(l[1]), puts("");
++n, l = (void**)l[1];
}
#else
	while(l) ++n, l = (void**)l[1];
#endif
	printf("chain %d|", n);
	n = 0, u = o_tail;
	while(u) ++n, u = u->prev;
	printf("%d", n);
	l = (void**)&o;
	if(l[1] == 0 && !o_tail || l[1] == (void*)(o_tail->m))
	printf(" flat");
	JS::RootedValue v(cxa);
	n = 0, l = (void**)&v;
	while(l) ++n, l = (void**)l[1];
	printf(" v %d\n", n);
}

// unravle the objects of our tags, our frames, if we can
void jsUnroot(void)
{
	if(!o_tail)
		return; // nothing to undo
	{
	JS::RootedObject z(cxa);
	void **l = (void**)&z;
	if(l[1] != (void*)o_tail->m) {
		debugPrint(1, "extra rooted objects found by jsUnroot()");
		return; // not flat
	}
	}
// now o_tail is at the top of the rooting stack
	Cro *u = o_tail;
	while(u && !u->inuse) {
		Cro *v = u->prev;
		delete u->m;
		free(u);
		o_tail = u = v;
	}
}

// Rooting window, to hold on to any objects that edbrowse might access.
static JS::RootedObject *rw0;

static void jsRootAndMove(void ** dest, JSObject *j, bool isglobal)
{
	Cro *u = (Cro *)allocMem(sizeof(Cro));
	u->m = new JS::RootedObject(cxa, j);
	u->prev = o_tail;
	u->inuse = true;
	u->global = isglobal;
// ok that was the easy part; the next part is really cheeky!
// Move this root down through the other rooted objects on the stack,
// and down to o_tail.
	void **top = (void**)rw0;
	if(o_tail)
		top = (void**)(o_tail->m);
	void **k = (void**)u->m;
	if(top == (void**)k[1]) // already in position
		goto done;
	{
	JS::RootedObject z(cxa);
	void **l = (void**)&z;
	void **find = (void**)(k[1]);
	l[1] = (void*)find; // this cuts m out of the chain
	while(find[1] != (void*)top) {
		find = (void**)(find[1]);
		if(find)
			continue;
		printf("root chain can't find the top, don't know what to do, have to abort\nRemember, this only works on moz60 or above\n");
		exit(1);
	}
	find[1] = (void*)k;
	k[1] = (void*)top;
	}
done:
	*dest = o_tail = u;
}

// dereference this if you want to abort and gdb and trace
static char *bad_ptr;

/*********************************************************************
The _0 methods are the lowest level, calling upon the engine.
They take JSObjects as parameter,
and must be in c++ and must understand the mozilla api.
These should not be called from anywhere outside this file.
Each of these functions assumes you are already in a compartment.
If you're not, something will seg fault somewhere along the line!
*********************************************************************/

static enum ej_proptype typeof_property_0(JS::HandleObject parent, const char *name);
static bool has_property_0(JS::HandleObject parent, const char *name);
static void delete_property_0(JS::HandleObject parent, const char *name);
static int get_arraylength_0(JS::HandleObject a);
static JSObject *get_array_element_object_0(JS::HandleObject parent, int idx);
static char *get_property_string_0(JS::HandleObject parent, const char *name);
static JSObject *get_property_object_0(JS::HandleObject parent, const char *name);
static JSObject *get_property_function_0(JS::HandleObject parent, const char *name);
static char *get_property_url_0(JS::HandleObject parent, bool action);
static int get_property_number_0(JS::HandleObject parent, const char *name);
static double get_property_float_0(JS::HandleObject parent, const char *name);
static bool get_property_bool_0(JS::HandleObject parent, const char *name);
static void set_property_number_0(JS::HandleObject parent, const char *name,  int n);
static void set_property_float_0(JS::HandleObject parent, const char *name,  double d);
static void set_property_bool_0(JS::HandleObject parent, const char *name,  bool b);
static void set_property_string_0(JS::HandleObject parent, const char *name, const char *value);
static void set_property_object_0(JS::HandleObject parent, const char *name,  JS::HandleObject child);
static void set_array_element_object_0(JS::HandleObject parent, int idx, JS::HandleObject child);
static void set_property_function_0(JS::HandleObject parent, const char *name, const char *body);
static JSObject *instantiate_0(JS::HandleObject parent, const char *name, const char *classname);
static JSObject *instantiate_array_element_0(JS::HandleObject parent, int idx, 			      const char *classname);
static JSObject *instantiate_array_0(JS::HandleObject parent, const char *name);
static bool run_function_bool_0(JS::HandleObject parent, const char *name);
static int run_function_onearg_0(JS::HandleObject parent, const char *name, JS::HandleObject a);
static void run_function_onestring_0(JS::HandleObject parent, const char *name, const char *a);
static bool run_event_0(JS::HandleObject obj, const char *pname, const char *evname);

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

static enum ej_proptype typeof_property_0(JS::HandleObject parent, const char *name)
{
JS::RootedValue v(cxa);
if(!JS_GetProperty(cxa, parent, name, &v))
return EJ_PROP_NONE;
return top_proptype(v);
}

static void uptrace(JS::HandleObject start);
static void processError(void);
static void jsInterruptCheck(void);
static Tag *tagFromObject(JS::HandleObject o);
static JSObject *tagToObject(const Tag *t);
extern JSObject *frameToCompartment(const Frame *f);
#define tagToCompartment(t) frameToCompartment((t)->f0)

static bool has_property_0(JS::HandleObject parent, const char *name)
{
bool found;
JS_HasProperty(cxa, parent, name, &found);
return found;
}

static void delete_property_0(JS::HandleObject parent, const char *name)
{
JS_DeleteProperty(cxa, parent, name);
}

static int get_arraylength_0(JS::HandleObject a)
{
unsigned length;
if(!JS_GetArrayLength(cxa, a, &length))
return -1;
return length;
}

static JSObject *get_array_element_object_0(JS::HandleObject parent, int idx)
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

const char *stringize(JS::HandleValue v)
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

default: return 0;
}
}

/* Return a property as a string, if it is
 * string compatible. The string is allocated, free it when done. */
static char *get_property_string_0(JS::HandleObject parent, const char *name)
{
JS::RootedValue v(cxa);
if(!JS_GetProperty(cxa, parent, name, &v))
return NULL;
return cloneString(stringize(v));
}

// Return a pointer to the JSObject. You need to dump this directly into
// a RootedObject.
static JSObject *get_property_object_0(JS::HandleObject parent, const char *name)
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

static JSObject *get_property_function_0(JS::HandleObject parent, const char *name)
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
static char *get_property_url_0(JS::HandleObject parent, bool action)
{
	enum ej_proptype t;
JS::RootedObject uo(cxa);	/* url object */
	if (action) {
		t = typeof_property_0(parent, "action");
		if (t == EJ_PROP_STRING)
			return get_property_string_0(parent, "action");
		if (t != EJ_PROP_OBJECT)
			return 0;
		uo = get_property_object_0(parent, "action");
	} else {
		t = typeof_property_0(parent, "href");
		if (t == EJ_PROP_STRING)
			return get_property_string_0(parent, "href");
		if (t == EJ_PROP_OBJECT)
			uo = get_property_object_0(parent, "href");
		else if (t)
			return 0;
		if (!uo) {
			t = typeof_property_0(parent, "src");
			if (t == EJ_PROP_STRING)
				return get_property_string_0(parent, "src");
			if (t == EJ_PROP_OBJECT)
				uo = get_property_object_0(parent, "src");
		}
	}
if(!uo)
		return 0;
/* should this be href$val? */
	return get_property_string_0(uo, "href");
}

static int get_property_number_0(JS::HandleObject parent, const char *name)
{
JS::RootedValue v(cxa);
if(!JS_GetProperty(cxa, parent, name, &v))
return -1;
if(v.isInt32()) return v.toInt32();
return -1;
}

static double get_property_float_0(JS::HandleObject parent, const char *name)
{
JS::RootedValue v(cxa);
if(!JS_GetProperty(cxa, parent, name, &v))
return 0.0; // should this be nan
if(v.isDouble()) return v.toDouble();
return 0.0;
}

static bool get_property_bool_0(JS::HandleObject parent, const char *name)
{
JS::RootedValue v(cxa);
if(!JS_GetProperty(cxa, parent, name, &v))
return false;
if(v.isBoolean()) return v.toBoolean();
return false;
}

#define JSPROP_STD JSPROP_ENUMERATE

static void set_property_number_0(JS::HandleObject parent, const char *name,  int n)
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

static void set_property_float_0(JS::HandleObject parent, const char *name,  double d)
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

static void set_property_bool_0(JS::HandleObject parent, const char *name,  bool b)
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
	debugPrint(5, "setter v in");
        JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
JS_SetProperty(cx, thisobj, "val$ue", args[0]);
	prepareForField(k);
	Tag *t = tagFromObject(thisobj);
	if(t) {
		debugPrint(4, "value tag %d=%s", t->seqno, k);
		domSetsTagValue(t, k);
	}
	nzFree(k);
	debugPrint(5, "setter v out");
	return true;
}

static int frameExpandLine(int ln, JS::HandleObject fo);
static int frameContractLine(int lineNumber);

static void forceFrameExpand(JS::HandleObject thisobj)
{
	Frame *save_cf = cf;
	bool save_plug = pluginsOn;
set_property_bool_0(thisobj, "eb$auto", true);
	pluginsOn = false;
	frameExpandLine(0, thisobj);
	cf = save_cf;
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

static bool setter_innerHTML(JSContext *cx, unsigned argc, JS::Value *vp)
{
if(argc != 1)
return true; // should never happen
  JS::CallArgs args = CallArgsFromVp(argc, vp);
	Tag *t;
const char *h = stringize(args[0]);
if(!h)
h = emptyString;
jsInterruptCheck();
	debugPrint(5, "setter h in");

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
JS::RootedObject cna2(cxa, instantiate_array_0(thisobj, "childNodes"));
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
	t = tagFromObject(thisobj);
	if(t) {
		html_from_setter(t, run);
	} else {
		                debugPrint(1, "innerHTML finds no tag, cannot parse");
	}
	nzFree(run);
	debugPrint(5, "setter h out");

JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
run_function_onearg_0(g, "textarea$html$crossover", thisobj);

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
	debugPrint(5, "setter h fail");
args.rval().setUndefined();
return true;
}

static void set_property_string_0(JS::HandleObject parent, const char *name, const char *value)
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

static void set_property_object_0(JS::HandleObject parent, const char *name,  JS::HandleObject child)
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

static void set_array_element_object_0(JS::HandleObject parent, int idx, JS::HandleObject child)
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

static void set_property_function_0(JS::HandleObject parent, const char *name, const char *body)
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
set_property_object_0(parent, name, fo);
}

static JSObject *instantiate_0(JS::HandleObject parent, const char *name,
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

static JSObject *instantiate_array_element_0(JS::HandleObject parent,
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

static JSObject *instantiate_array_0(JS::HandleObject parent, const char *name)
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
// I assume this instantiates in the current compartment
a = JS_NewArrayObject(cxa, 0);
v = JS::ObjectValue(*a);
	JS_DefineProperty(cxa, parent, name, v, JSPROP_STD);
	return a;
}

// run a function with no arguments, that returns bool
static bool run_function_bool_0(JS::HandleObject parent, const char *name)
{
bool rc = false;
	int dbl = 3;		// debug level
	int seqno = -1;
	if (stringEqual(name, "ontimer")) {
// even at debug level 3, I don't want to see
// the execution messages for each timer that fires
		dbl = 4;
seqno = get_property_number_0(parent, "tsn");
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

bool run_function_bool_t(const Tag *t, const char *name)
{
if(!t->jslink || !allowJS)
return false;
JSAutoCompartment ac(cxa, tagToCompartment(t));
JS::RootedObject obj(cxa, tagToObject(t));
	return run_function_bool_0(obj, name);
}

bool run_function_bool_win(const Frame *f, const char *name)
{
if(!f->jslink || !allowJS)
return false;
JSAutoCompartment ac(cxa, frameToCompartment(f));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa)); // global
	return run_function_bool_0(g, name);
}

void run_ontimer(const Frame *f, const char *backlink)
{
JSAutoCompartment ac(cxa, frameToCompartment(f));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
JS::RootedObject to(cxa, get_property_object_0(g, backlink));
if(!to) {
  debugPrint(3, "could not find timer backlink %s", backlink);
		return;
	}
	run_event_0(to, "timer", "ontimer");
}

static int run_function_onearg_0(JS::HandleObject parent, const char *name,
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
JSAutoCompartment ac(cxa, tagToCompartment(t));
JS::RootedObject obj(cxa, tagToObject(t));
JS::RootedObject obj2(cxa, tagToObject(t2));
	return run_function_onearg_0(obj, name, obj2);
}

int run_function_onearg_win(const Frame *f, const char *name, const Tag *t2)
{
if(!f->jslink || !t2->jslink || !allowJS)
return -1;
JSAutoCompartment ac(cxa, frameToCompartment(f));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa)); // global
JS::RootedObject obj2(cxa, tagToObject(t2));
	return run_function_onearg_0(g, name, obj2);
}

int run_function_onearg_doc(const Frame *f, const char *name, const Tag *t2)
{
if(!f->jslink || !t2->jslink || !allowJS)
return -1;
JSAutoCompartment ac(cxa, frameToCompartment(f));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa)); // global
JS::RootedObject doc(cxa, get_property_object_0(g, "document"));
JS::RootedObject obj2(cxa, tagToObject(t2));
	return run_function_onearg_0(doc, name, obj2);
}

static void run_function_onestring_0(JS::HandleObject parent, const char *name,
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

void run_function_onestring_t(const Tag *t, const char *name, const char *s)
{
if(!t->jslink || !allowJS)
return;
JSAutoCompartment ac(cxa, tagToCompartment(t));
JS::RootedObject obj(cxa, tagToObject(t));
	run_function_onestring_0(obj, name, s);
}

/*********************************************************************
The _t functions take a tag and bounce through the object
linked to that tag. These correspond to the _0 functions but we may not
need all of them.
Unlike the _0 functions, the _t functions set the compartment
according to t->f0.
*********************************************************************/

bool has_property_t(const Tag *t, const char *name)
{
if(!t->jslink || !allowJS)
return false;
JSAutoCompartment ac(cxa, tagToCompartment(t));
JS::RootedObject obj(cxa, tagToObject(t));
if(!obj)
return false;
return has_property_0(obj, name);
}

bool has_property_win(const Frame *f, const char *name)
{
if(!f->jslink || !allowJS)
return false;
JSAutoCompartment ac(cxa, frameToCompartment(f));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
return has_property_0(g, name);
}

void set_property_object_t(const Tag *t, const char *name, const Tag *t2)
{
if(!t->jslink || !t2->jslink || !allowJS)
return;
JSAutoCompartment ac(cxa, tagToCompartment(t));
JS::RootedObject obj(cxa, tagToObject(t));
JS::RootedObject obj2(cxa, tagToObject(t2));
	set_property_object_0(obj, name, obj2);
}

char *get_property_url_t(const Tag *t, bool action)
{
if(!t->jslink || !allowJS)
return 0;
JSAutoCompartment ac(cxa, tagToCompartment(t));
JS::RootedObject obj(cxa, tagToObject(t));
if(!obj)
return 0;
return get_property_url_0(obj, action);
}

char *get_property_string_t(const Tag *t, const char *name)
{
if(!t->jslink || !allowJS)
return 0;
JSAutoCompartment ac(cxa, tagToCompartment(t));
JS::RootedObject obj(cxa, tagToObject(t));
if(!obj)
return 0;
return get_property_string_0(obj, name);
}

char *get_dataset_string_t(const Tag *t, const char *p)
{
	char *r;
if(!t->jslink || !allowJS)
return 0;
JSAutoCompartment ac(cxa, tagToCompartment(t));
JS::RootedObject obj(cxa, tagToObject(t));
if(!obj)
return 0;
	if (!strncmp(p, "data-", 5)) {
		char *k = cloneString(p + 5);
JS::RootedObject ds(cxa, get_property_object_0(obj, "dataset"));
		if(!ds)
			return 0;
		camelCase(k);
		r = get_property_string_0(ds, k);
		nzFree(k);
	} else
		r = get_property_string_0(obj, p);
	return r;
}

bool get_property_bool_t(const Tag *t, const char *name)
{
if(!t->jslink || !allowJS)
return false;
JSAutoCompartment ac(cxa, tagToCompartment(t));
JS::RootedObject obj(cxa, tagToObject(t));
if(!obj)
return false;
return get_property_bool_0(obj, name);
}

int get_property_number_t(const Tag *t, const char *name)
{
if(!t->jslink || !allowJS)
return -1;
JSAutoCompartment ac(cxa, tagToCompartment(t));
JS::RootedObject obj(cxa, tagToObject(t));
if(!obj)
return -1;
return get_property_number_0(obj, name);
}

enum ej_proptype typeof_property_t(const Tag *t, const char *name)
{
if(!t->jslink || !allowJS)
return EJ_PROP_NONE;
JSAutoCompartment ac(cxa, tagToCompartment(t));
JS::RootedObject obj(cxa, tagToObject(t));
if(!obj)
return EJ_PROP_NONE;
return typeof_property_0(obj, name);
}

char *get_style_string_t(const Tag *t, const char *name)
{
if(!t->jslink || !allowJS)
return 0;
JSAutoCompartment ac(cxa, tagToCompartment(t));
JS::RootedObject obj(cxa, tagToObject(t));
if(!obj)
return 0;
JS::RootedObject style(cxa, get_property_object_0(obj, "style"));
if(!style)
return 0;
return get_property_string_0(style, name);
}

void delete_property_t(const Tag *t, const char *name)
{
if(!t->jslink || !allowJS)
return;
JSAutoCompartment ac(cxa, tagToCompartment(t));
JS::RootedObject obj(cxa, tagToObject(t));
if(obj)
delete_property_0(obj, name);
}

void delete_property_win(const Frame *f, const char *name)
{
if(!f->jslink || !allowJS)
return;
JSAutoCompartment ac(cxa, frameToCompartment(f));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa)); // global
delete_property_0(g, name);
}

void delete_property_doc(const Frame *f, const char *name)
{
if(!f->jslink || !allowJS)
return;
JSAutoCompartment ac(cxa, frameToCompartment(f));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa)); // global
JS::RootedObject doc(cxa, get_property_object_0(g, "document"));
if(doc)
delete_property_0(doc, name);
}

void set_property_string_t(const Tag *t, const char *name, const char *v)
{
if(!t->jslink || !allowJS)
return;
JSAutoCompartment ac(cxa, tagToCompartment(t));
JS::RootedObject obj(cxa, tagToObject(t));
if(!obj)
return;
set_property_string_0(obj, name, v);
}

void set_dataset_string_t(const Tag *t, const char *name, const char *v)
{
	if(!t->jslink || !allowJS)
		return;
	JSAutoCompartment ac(cxa, tagToCompartment(t));
	JS::RootedObject obj(cxa, tagToObject(t));
	if(!obj)
		return;
	JS::RootedObject dso(cxa, get_property_object_0(obj, "dataset"));
	if(dso)
		set_property_string_0(dso, name, v);
}

void set_property_bool_t(const Tag *t, const char *name, bool v)
{
if(!t->jslink || !allowJS)
return;
JSAutoCompartment ac(cxa, tagToCompartment(t));
JS::RootedObject obj(cxa, tagToObject(t));
if(!obj)
return;
set_property_bool_0(obj, name, v);
}

void set_property_number_t(const Tag *t, const char *name, int v)
{
if(!t->jslink || !allowJS)
return;
JSAutoCompartment ac(cxa, tagToCompartment(t));
JS::RootedObject obj(cxa, tagToObject(t));
if(!obj)
return;
set_property_number_0(obj, name, v);
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
static const char *run_script_0(JS::HandleObject obj, const char *s, const char *filename, int lineno)
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
	debugPrint(5, "> script win:");
	JSAutoCompartment ac(cxa, frameToCompartment(cf));
	JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa)); // global
	run_script_0(g, str, filename, lineno);
	debugPrint(5, "< ok");
}

void jsRunScript_t(const Tag *t, const char *str, const char *filename, 		 int lineno)
{
if(!t->jslink || !allowJS)
return;
JSAutoCompartment ac(cxa, tagToCompartment(t));
JS::RootedObject tojb(cxa, tagToObject(t));
	run_script_0(tojb, str, filename, lineno);
}

char *jsRunScriptWinResult(const char *str,
const char *filename, 			int lineno)
{
if(!cf->jslink || !allowJS)
return 0;
JSAutoCompartment ac(cxa, frameToCompartment(cf));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa)); // global
const char *s = run_script_0(g, str, filename, lineno);
// This is and has to be copied in the duktape world,
// so we do the same here for consistency.
return cloneString(s);
}

// Like any JSObject * return, you must put it directly into a rooted object.
static JSObject *create_event_0(JS::HandleObject parent, const char *evname)
{
JS::RootedObject e(cxa);
	const char *evname1 = evname;
	if (evname[0] == 'o' && evname[1] == 'n')
		evname1 += 2;
// gc$event protects from garbage collection
	e = instantiate_0(parent, "gc$event", "Event");
	set_property_string_0(e, "type", evname1);
	return e;
}

static void unlink_event_0(JS::HandleObject parent)
{
	delete_property_0(parent, "gc$event");
}

static bool run_event_0(JS::HandleObject obj, const char *pname, const char *evname)
{
	int rc;
	JS::RootedObject eo(cxa);	// created event object
	if(typeof_property_0(obj, evname) != EJ_PROP_FUNCTION)
		return true;
	if (debugLevel >= 3) {
		if (debugEvent) {
			int seqno = get_property_number_0(obj, "eb$seqno");
			debugPrint(3, "trigger %s tag %d %s", pname, seqno, evname);
		}
	}
	eo = create_event_0(obj, evname);
	set_property_object_0(eo, "target", obj);
	set_property_object_0(eo, "currentTarget", obj);
	set_property_number_0(eo, "eventPhase", 2);
	rc = run_function_onearg_0(obj, evname, eo);
	unlink_event_0(obj);
// no return or some other return is treated as true in this case
	if (rc < 0)
		rc = true;
	return rc;
}

bool run_event_t(const Tag *t, const char *pname, const char *evname)
{
	if (!allowJS || !t->jslink)
		return true;
JSAutoCompartment ac(cxa, tagToCompartment(t));
JS::RootedObject tagobj(cxa, tagToObject(t));
	return run_event_0(tagobj, pname, evname);
}

// execute script.text code, wrapper around run_script_o
void jsRunData(const Tag *t, const char *filename, int lineno)
{
	bool rc;
	const char *s;
	if (!allowJS || !t->jslink)
		return;
	debugPrint(5, "> script tag %d:", t->seqno);
        JSAutoCompartment ac(cxa, tagToCompartment(t));
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
JS::RootedObject doc(cxa, get_property_object_0(g, "document"));
set_property_object_0(doc, "currentScript", to);
// running the script will almost surely run stringize again
char *s2 = cloneString(s1);
run_script_0(g, s2, filename, lineno);
delete_property_0(doc, "currentScript");
// onload handler? Should this run even if the script fails?
// Right now it does.
	if (t->js_file && !isDataURI(t->href) &&
	typeof_property_0(to, "onload") == EJ_PROP_FUNCTION)
		run_event_0(to, "script", "onload");
	debugPrint(5, "< ok");
}

bool run_event_win(const Frame *f, const char *pname, const char *evname)
{
	if (!allowJS || !f->jslink)
		return true;
JSAutoCompartment ac(cxa, frameToCompartment(f));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
	return run_event_0(g, pname, evname);
}

bool run_event_doc(const Frame *f, const char *pname, const char *evname)
{
	if (!allowJS || !f->jslink)
		return true;
JSAutoCompartment ac(cxa, frameToCompartment(f));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
JS::RootedObject doc(cxa, get_property_object_0(g, "document"));
	return run_event_0(doc, pname, evname);
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
	e = create_event_0(tagobj, name);
	rc = run_function_onearg_0(tagobj, "dispatchEvent", e);
	if (rc && get_property_bool_0(e, "prev$default"))
		rc = false;
	unlink_event_0(tagobj);
	return rc;
}

void set_property_bool_win(const Frame *f, const char *name, bool v)
{
if(!f->jslink || !allowJS)
return;
JSAutoCompartment ac(cxa, frameToCompartment(f));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
	set_property_bool_0(g, name, v);
}

void set_property_string_win(const Frame *f, const char *name, const char *v)
{
if(!f->jslink || !allowJS)
return;
JSAutoCompartment ac(cxa, frameToCompartment(f));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
	set_property_string_0(g, name, v);
}

void set_property_string_doc(const Frame *f, const char *name, const char *v)
{
JSAutoCompartment ac(cxa, frameToCompartment(f));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
JS::RootedObject doc(cxa, get_property_object_0(g, "document"));
	set_property_string_0(doc, name, v);
}

static void connectTagObject(Tag *t, JS::HandleObject o)
{
	jsRootAndMove(&(t->jv), o, false);
        JSAutoCompartment ac(cxa, tagToCompartment(t));
JS_DefineProperty(cxa, o, "eb$seqno", t->seqno,
(JSPROP_READONLY|JSPROP_PERMANENT));
JS_DefineProperty(cxa, o, "eb$gsn", t->gsn,
(JSPROP_READONLY|JSPROP_PERMANENT));
t->jslink = true;
}

void disconnectTagObject(Tag *t)
{
	if(!t->jslink)
		return; // already disconnected
	Cro *u = (Cro *)t->jv;
	u->inuse = false;
	t->jslink = false;
	t->jv = 0;
}

// This is ugly and inefficient
static Tag *tagFromObject(JS::HandleObject o)
{
	int i;
	JSObject *j = o;
	if (!tagList)
		i_printfExit(MSG_NullListInform);
	if(!j) {
		debugPrint(1, "tagFromObject(null)");
		return 0;
	}
	for (i = 0; i < cw->numTags; ++i) {
		Tag *t = tagList[i];
		if(t->jslink &&
		*(((Cro*)t->jv)->m) == j &&
		!t->dead)
			return t;
	}
	debugPrint(1, "tagFromObject() returns null");
	return 0;
}

// inverse of the above
static JSObject *tagToObject(const Tag *t)
{
	if(!t->jslink)
		return 0;
	return *(((Cro *)t->jv)->m);
}

/*********************************************************************
This function is usually called to set a compartment for a frame.
Unlike most functions in this file, I am not assuming a preexisting compartment.
Therefore, I link to the root window before doing anything.
And I always return some compartment, because any compartment
is better than none.
*********************************************************************/

JSObject *frameToCompartment(const Frame *f)
{
	if(f->jslink && f->winobj)
		return *(((Cro*)f->winobj)->m);
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
	connectTagObject(t, o);
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
				set_property_string_0(p_j,
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
	t->name = get_property_string_0(a_j, "name");
	t->id = get_property_string_0(a_j, "id");
	t->jclass = get_property_string_0(a_j, "class");

	switch (action) {
	case TAGACT_INPUT:
		jst = get_property_string_0(a_j, "type");
		setTagAttr(t, "type", jst);
		t->value = get_property_string_0(a_j, "value");
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
		t->value = get_property_string_0(a_j, "value");
		if (!t->value)
			t->value = emptyString;
// Need to create the side buffer here.
		formControl(t, true);
		break;

	case TAGACT_SELECT:
		t->action = TAGACT_INPUT;
		t->itype = INP_SELECT;
		if (typeof_property_0(a_j, "multiple"))
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
	debugPrint(5, "log in");
JS::RootedObject obj(cxa);
JS_ValueToObject(cxa, args[0], &obj);
// this call creates the getter and setter for innerHTML
set_property_string_0(obj, "innerHTML", emptyString);
const char *tagname = stringize(args[1]);
domSetsLinkage('c', obj, tagname);
	debugPrint(5, "log out");
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

static void set_timer(JSContext *cx, unsigned argc, JS::Value *vp, bool isInterval)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
args.rval().setUndefined();
JS::RootedObject to(cx); // timer object
JS::RootedObject fo(cx); // function object
	int n = 1000;		/* default number of milliseconds */
if(!argc)
return;
	debugPrint(5, "timer in");
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
	debugPrint(5, "timer fail");
return;
}
fo = JS_GetFunctionObject(f);
} else return;

JS::RootedObject g(cx, JS::CurrentGlobalOrNull(cx));
const char *	fpn = fakePropName();
// create the timer object and also protect it from gc
// by linking it to window, through the fake property name.
to = instantiate_0(g, fpn, "Timer");
// classs is milliseconds, for debugging
set_property_number_0(to, "class", n);
set_property_object_0(to, "ontimer", fo);
set_property_string_0(to, "backlink", fpn);
set_property_number_0(to, "tsn", ++timer_sn);
args.rval().setObject(*to);
	debugPrint(5, "timer out");
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
int tsn = get_property_number_0(to, "tsn");
char * fpn = get_property_string_0(to, "backlink");
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
	int n = args[0].toInt32();
// The selection string has to be allocated - css will use it in place,
// then free it later.
	char *s = cloneString(stringize(args[1]));
	bool r = args[2].toBoolean();
	cssDocLoad(n, s, r);
	args.rval().setUndefined();
	return true;
}

static bool nat_cssApply(JSContext *cx, unsigned argc, JS::Value *vp)
{
	  JS::CallArgs args = CallArgsFromVp(argc, vp);
	int n = args[0].toInt32();
	JS::RootedObject node(cx);
	JS_ValueToObject(cx, args[1], &node);
	Tag *t = tagFromObject(node);
	if(t)
		cssApply(n, t);
	else
		debugPrint(3, "eb$cssApply is passed an object that does not correspond to an html tag");
	args.rval().setUndefined();
	return true;
}

static bool nat_cssText(JSContext *cx, unsigned argc, JS::Value *vp)
{
	  JS::CallArgs args = CallArgsFromVp(argc, vp);
	const char *rules = emptyString;
	if(argc >= 1)
		rules = stringize(args[0]);
	cssText(rules);
	args.rval().setUndefined();
	return true;
}

// turn an array of html tags into an array of objects.
// You need to dump the returned pointer into a rooted object.
static JSObject * objectize(JSContext *cx, Tag **tlist)
{
	int i, j;
	const Tag *t;
JS::RootedObject ao(cx, JS_NewArrayObject(cx, 0));
	if(!tlist)
return ao;
JS::RootedValue v(cx);
JS::RootedObject tobj(cx);
	for (i = j = 0; (t = tlist[i]); ++i) {
		if (!t->jslink)	// should never happen
			continue;
if(!(tobj = tagToObject(t)))
continue;
v.setObject(*tobj);
JS_DefineElement(cx, ao, j, v, JSPROP_STD);
		++j;
	}
return ao;
}

// Turn start into a tag, or 0 if start is doc or win for the current frame.
// Return false if we can't turn this into a tag within the current window.
static bool rootTag(JS::HandleObject start, Tag **tp)
{
	Tag *t;
	JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
	JS::RootedObject doc(cxa, get_property_object_0(g, "document"));
	*tp = 0;
// assume this is 0 when querySelectorAll is called from the window
	if(!start || start == g || start == doc)
		return true;
	t = tagFromObject(start);
	if(!t)
		return false;
	*tp = t;
	return true;
}

// querySelectorAll
static bool nat_qsa(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
char *selstring = NULL;
	Tag *t;
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
JS::RootedObject ao(cx);
	if(!rootTag(start, &t)) {
nzFree(selstring);
ao = objectize(cx, 0);
args.rval().setObject(*ao);
return true;
}
	Tag **tlist = querySelectorAll(selstring, t);
nzFree(selstring);
ao = objectize(cx, tlist);
args.rval().setObject(*ao);
	nzFree(tlist);
	  return true;
}

static bool nat_qs(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
char *selstring = NULL;
	Tag *t;
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
	if(!rootTag(start, &t)) {
nzFree(selstring);
		args.rval().setUndefined();
return true;
}
t = querySelector(selstring, t);
nzFree(selstring);
	if(t && t->jslink) {
JS::RootedObject tobj(cx, tagToObject(t));
args.rval().setObject(*tobj);
	} else
		args.rval().setUndefined();
	return true;
}

static bool nat_qs0(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
char *selstring = NULL;
bool rc = false;
	Tag *t;
        JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
if(argc >= 1 && args[0].isString()) {
JSString *s = args[0].toString();
selstring = JS_EncodeString(cx, s);
}
jsInterruptCheck();
	if(!rootTag(thisobj, &t)) {
nzFree(selstring);
args.rval().setBoolean(rc);
return true;
}
	rc = querySelector0(selstring, t);
nzFree(selstring);
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
}

/* Problems: 0, frame expanded successfully.
 1 line is not a frame.
 2 frame doesn't have a valid url.
 3 Problem fetching the rul or rendering the page.  */

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

	JSAutoCompartment bc(cxa, tagToCompartment(t));

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
		set_property_object_0(cwo, "parent", prev);
// And top points to the top.
JS::RootedObject top(cxa, get_property_object_0(prev, "top"));
set_property_object_0(cwo, "top", top);
// frame tag object
fto = tagToObject(t);
		set_property_object_0(cwo, "frameElement", fto);
		run_function_bool_0(cwo, "eb$qs$start");
		if(jssrc)
			jsRunScriptWin(jssrc, "frame.src", 1);
		runScriptsPending(true);
		runOnload();
		runScriptsPending(false);
// get current document object from current window object
cdo = get_property_object_0(cwo, "document");
		set_property_string_0(cdo, "readyState", "complete");
		run_event_0(cdo, "document", "onreadystatechange");
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
		connectTagObject(cdt, cdo);
		cdt->style = 0;
		cdt->ssn = 0;
// Should I switch this tag into the new frame? I don't really know.
		cdt->f0 = new_cf;
		set_property_object_0(fto, "content$Window", cwo);
		set_property_object_0(fto, "content$Document", cdo);
JS::RootedObject cna(cxa);	// childNodes array
		cna = get_property_object_0(fto, "childNodes");
		set_array_element_object_0(cna, 0, cdo);
		set_property_object_0(cdo, "parentNode", fto);
// run the frame onload function if it is there.
// I assume it should run in the higher frame.
// Hope so cause that is the current compartment.
		run_event_0(fto, t->info->name, "onload");
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
	save_top = get_property_object_0(g, "top");
	save_parent = get_property_object_0(g, "parent");
	save_fe = get_property_object_0(g, "frameElement");

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
		set_property_object_0(g, "top", save_top);
		set_property_object_0(g, "parent", save_parent);
		set_property_object_0(g, "frameElement", save_fe);
		run_function_bool_0(g, "eb$qs$start");
		runScriptsPending(true);
		runOnload();
		runScriptsPending(false);
doc = get_property_object_0(g, "document");
		set_property_string_0(doc, "readyState", "complete");
		run_event_0(doc, "document", "onreadystatechange");
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
		connectTagObject(cdt, doc);
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
		set_property_object_0(save_fe, "content$Window", g);
		set_property_object_0(save_fe, "content$Document", doc);
		cna = get_property_object_0(save_fe, "childNodes");
		set_array_element_object_0(cna, 0, doc);
		set_property_object_0(doc, "parentNode", save_fe);
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
		connectTagObject(cdt, newdoc);
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

	debugPrint(5, "append in");
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
	debugPrint(5, "append out");
// return the child that was appended
args.rval().set(args[0]);
return;

fail:
	debugPrint(5, "append fail");
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

	debugPrint(5, "remove in");
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
	debugPrint(5, "remove out");

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
	debugPrint(5, "remove fail");
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

	debugPrint(5, "before in");
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
	debugPrint(5, "before out");
return true;

fail:
	debugPrint(5, "remove fail");
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

	debugPrint(5, "xhr in");
JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
        JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
	if (down_jsbg)
async = get_property_bool_0(thisobj, "async");

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
		connectTagObject(t, thisobj);
// This routine will return, and javascript might stop altogether; do we need
// to protect this object from garbage collection?
set_property_object_0(global, fpn, thisobj);
set_property_string_0(thisobj, "backlink", fpn);

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

	debugPrint(5, "xhr out");
JS::RootedString m(cx, JS_NewStringCopyZ(cx, a));
args.rval().setString(m);
nzFree(a);
return true;
}

static Frame *thisFrame(JS::HandleObject thisobj)
{
int my_sn = get_property_number_0(thisobj, "eb$ctx");
	Frame *f;
	for (f = &(cw->f0); f; f = f->next)
if(f->gsn == my_sn)
break;
	return f;
}

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

static void js_start(void)
{
	    JS_Init();
// Mozilla assumes one context per thread; we can run all of edbrowse
// inside one context; I think.
	cxa = JS_NewContext(JS::DefaultHeapMaxBytes);
	if(!cxa || !JS::InitSelfHostedCode(cxa)) {
		debugPrint(1, "failure to start the mozilla js engine, javascript will not work.");
		allowJS = false;
		return;
	}

// make rooting window
	      JS::CompartmentOptions options;
	rw0 = new       JS::RootedObject(cxa, JS_NewGlobalObject(cxa, &global_class, nullptr, JS::FireOnNewGlobalHook, options));
	        JSAutoCompartment ac(cxa, *rw0);
	        JS_InitStandardClasses(cxa, *rw0);
}

static void setup_window_2(int sn);

// This is an edbrowse context, in a frame,
// unrelated to the mozilla js context.
void createJSContext(Frame *f)
{
	if(!allowJS)
		return;
	if(!cxa)
		js_start();

debugPrint(3, "create js context %d", f->gsn);
      JS::CompartmentOptions options;
JSObject *g = JS_NewGlobalObject(cxa, &global_class, nullptr, JS::FireOnNewGlobalHook, options);
	if(!g) {
		debugPrint(1, "Failure to create javascript compartment; javascript is disabled.");
		allowJS = false;
		return;
	}
	f->jslink = true;

	jsRootAndMove(&(f->winobj), g, true);

JS::RootedObject global(cxa, g);
        JSAutoCompartment ac(cxa, g);
        JS_InitStandardClasses(cxa, global);
JS_DefineFunctions(cxa, global, nativeMethodsWindow);

// window
JS::RootedValue v(cxa);
v = JS::ObjectValue(*global);
JS_DefineProperty(cxa, global, "window", v,
(JSPROP_READONLY|JSPROP_PERMANENT));

// time for document under window
JS::RootedObject docroot(cxa, JS_NewObject(cxa, nullptr));
v = JS::ObjectValue(*docroot);
JS_DefineProperty(cxa, global, "document", v,
(JSPROP_READONLY|JSPROP_PERMANENT|JSPROP_ENUMERATE));
JS_DefineFunctions(cxa, docroot, nativeMethodsDocument);

set_property_number_0(docroot, "eb$seqno", 0);
set_property_number_0(docroot, "eb$ctx", f->gsn);
// Sequence is to set f->fileName, then createContext(), so for a short time,
// we can rely on that variable.
// Let's make it more permanent, per context.
// Has to be nonwritable for security reasons.
JS::RootedString m(cxa, JS_NewStringCopyZ(cxa, f->fileName));
v.setString(m);
JS_DefineProperty(cxa, global, "eb$url", v,
(JSPROP_READONLY|JSPROP_PERMANENT));

// what use to be the master window
JS::RootedObject mw(cxa, JS_NewObject(cxa, nullptr));
v = JS::ObjectValue(*mw);
JS_DefineProperty(cxa, global, "mw$", v,
(JSPROP_READONLY|JSPROP_PERMANENT|JSPROP_ENUMERATE));

setup_window_2(f->gsn);
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

static void setup_window_2(int sn)
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
	extern const char thirdJS[];

// startwindow.js stored as an internal string
	jsRunScriptWin(startWindowJS, "startwindow.js", 1);

// third party debugging stuff
	if(sn == 1)
		jsRunScriptWin(thirdJS, "third.js", 1);

	nav = get_property_object_0(g, "navigator");
	if (!nav)
		return;
// some of the navigator is in startwindow.js; the runtime properties are here.
	set_property_string_0(nav, "userLanguage", supported_languages[eb_lang]);
	set_property_string_0(nav, "language", supported_languages[eb_lang]);
	set_property_string_0(nav, "appVersion", version);
	set_property_string_0(nav, "vendorSub", version);
	set_property_string_0(nav, "userAgent", currentAgent);
	uname(&ubuf);
	set_property_string_0(nav, "oscpu", ubuf.sysname);
	set_property_string_0(nav, "platform", ubuf.machine);

/* Build the array of mime types and plugins,
 * according to the entries in the config file. */
	navpi = get_property_object_0(nav, "plugins");
	navmt = get_property_object_0(nav, "mimeTypes");
	if (!navpi || !navmt)
		return;
	mt = mimetypes;
	for (i = 0; i < maxMime; ++i, ++mt) {
		int len;
/* po is the plugin object and mo is the mime object */
JS::RootedObject 		po(cxa, instantiate_array_element_0(navpi, i, 0));
JS::RootedObject 		mo(cxa, instantiate_array_element_0(navmt, i, 0));
if(!po || !mo)
			return;
		set_property_object_0(mo, "enabledPlugin", po);
		set_property_string_0(mo, "type", mt->type);
		set_property_object_0(navmt, mt->type, mo);
		set_property_string_0(mo, "description", mt->desc);
		set_property_string_0(mo, "suffixes", mt->suffix);
/* I don't really have enough information from the config file to fill
 * in the attributes of the plugin object.
 * I'm just going to fake it.
 * Description will be the same as that of the mime type,
 * and the filename will be the program to run.
 * No idea if this is right or not. */
		set_property_string_0(po, "description", mt->desc);
		set_property_string_0(po, "filename", mt->program);
/* For the name, how about the program without its options? */
		len = strcspn(mt->program, " \t");
		save_c = mt->program[len];
		mt->program[len] = 0;
		set_property_string_0(po, "name", mt->program);
		mt->program[len] = save_c;
	}

	hist = get_property_object_0(g, "history");
	if (!hist)
		return;
	set_property_string_0(hist, "current", cf->fileName);

JS::RootedObject doc(cxa, get_property_object_0(g, "document"));
	set_property_string_0(doc, "referrer", cw->referrer);
	set_property_string_0(doc, "URL", cf->fileName);
	set_property_string_0(doc, "location", cf->fileName);
	set_property_string_0(g, "location", cf->fileName);
jsRunScriptWin(
		    "window.location.replace = document.location.replace = function(s) { this.href = s; };Object.defineProperty(window.location,'replace',{enumerable:false});Object.defineProperty(document.location,'replace',{enumerable:false});",
		    "locreplace", 1);
	set_property_string_0(doc, "domain", getHostURL(cf->fileName));
	if (debugClone)
		set_property_bool_0(g, "cloneDebug", true);
	if (debugEvent)
		set_property_bool_0(g, "eventDebug", true);
	if (debugThrow)
		set_property_bool_0(g, "throwDebug", true);
}

void freeJSContext(Frame *f)
{
	debugPrint(5, "> free frame %d", f->gsn);
	if(f->jslink) {
		debugPrint(3, "remove js context %d", f->gsn);
		Cro *u = (Cro *)f->winobj;
		u->inuse = false;
		 f->jslink = false;
	}
	f->cx = f->winobj = f->docobj = 0;
	debugPrint(5, "< ok");
	cssFree(f);
}				/* freeJSContext */

void establish_js_option(Tag *t, Tag *sel)
{
	int idx = t->lic;
	JS::RootedObject oa(cxa);		// option array
	JS::RootedObject oo(cxa);		// option object
	JS::RootedObject so(cxa);		// style object
	JS::RootedObject ato(cxa);		// attributes object
	JS::RootedObject fo(cxa);		// form object
	JS::RootedObject selobj(cxa); // select object

	if(!sel->jslink)
		return;

        JSAutoCompartment ac(cxa, tagToCompartment(sel));
	selobj = tagToObject(sel);
	if (!(oa = get_property_object_0(selobj, "options")))
		return;
	if (!(oo = instantiate_array_element_0(oa, idx, "Option")))
		return;
	set_property_object_0(oo, "parentNode", oa);
/* option.form = select.form */
	fo = get_property_object_0(selobj, "form");
	if (fo)
		set_property_object_0(oo, "form", fo);
	instantiate_array_0(oo, "childNodes");
	ato = instantiate_0(oo, "attributes", "NamedNodeMap");
	set_property_object_0(ato, "owner", oo);
	so = instantiate_0(oo, "style", "CSSStyleDeclaration");
	set_property_object_0(so, "element", oo);

connectTagObject(t, oo);
}

void establish_js_textnode(Tag *t, const char *fpn)
{
	JS::RootedObject so(cxa);		// style object
	JS::RootedObject ato(cxa);		// attributes object
	        JSAutoCompartment ac(cxa, tagToCompartment(t));
	JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
JS::RootedObject tagobj(cxa,  instantiate_0(g, fpn, "TextNode"));
	instantiate_array_0(tagobj, "childNodes");
	ato = instantiate_0(tagobj, "attributes", "NamedNodeMap");
	set_property_object_0(ato, "owner", tagobj);
	so = instantiate_0(tagobj, "style", "CSSStyleDeclaration");
	set_property_object_0(so, "element", tagobj);
	connectTagObject(t, tagobj);
}

static void processStyles(JS::HandleObject so, const char *stylestring)
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
				set_property_string_0(so, s, sv);
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
	JS::RootedObject owner(cxa);
	JS::RootedObject alist(cxa);
	JS::RootedObject io(cxa); // the input object
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
	JS::RootedObject so(cxa);	/* obj.style */
	JS::RootedObject ato(cxa);	/* obj.attributes */
	char upname[MAXTAGNAME];

	debugPrint(5, "domLink %s.%d name %s",
		   classname, extra, (symname ? symname : emptyString));
	extra &= 6;
	        JSAutoCompartment ac(cxa, frameToCompartment(cf));
	JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
	JS::RootedObject doc(cxa, get_property_object_0(g, "document"));
	if(owntag)
		owner = tagToObject(owntag);
if(extra == 2)
		owner = g;
if(extra == 4)
		owner = doc;

	if (symname && typeof_property_0(owner, symname)) {
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
			if(!(io = get_property_object_0(owner, symname)))
				return;
		} else {
/* don't know why the duplicate name */
			dupname = true;
		}
	}

/* The input object is nonzero if&only if the input is a radio button,
 * and not the first button in the set, thus it isce the array containing
 * these buttons. */
	if (!io) {
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
			if (membername && has_property_0(owner, membername)) {
				debugPrint(3, "membername overload %s.%s",
					   classname, membername);
				membername = 0;
			}
		}
		if (!membername)
			membername = fakePropName(), fakeName = true;

		if (isradio) {	// the first radio button
			if(!(io = instantiate_array_0(
			(fakeName ? g : owner), membername)))
				return;
			set_property_string_0(io, "type", "radio");
		} else {
/* A standard input element, just create it. */
			JS::RootedObject ca(cxa);  // childNodes array
			if(!(io = instantiate_0(
(fakeName ? g : owner), membername, classname)))
				return;
/* not an array; needs the childNodes array beneath it for the children */
			ca = instantiate_array_0(io, "childNodes");
// childNodes and options are the same for Select
			if (stringEqual(classname, "Select"))
				set_property_object_0(io, "options", ca);
		}

/* deal with the 'styles' here.
object will get 'style' regardless of whether there is
anything to put under it, just like it gets childNodes whether
or not there are any.  After that, there is a conditional step.
If this node contains style='' of one or more name-value pairs,
call out to process those and add them to the object.
Don't do any of this if the tag is itself <style>. */
		if (t->action != TAGACT_STYLE) {
			so = instantiate_0(io, "style", "CSSStyleDeclaration");
			set_property_object_0(so, "element", io);
/* now if there are any style pairs to unpack,
 processStyles can rely on obj.style existing */
			if (stylestring)
				processStyles(so, stylestring);
		}

/* Other attributes that are expected by pages, even if they
 * aren't populated at domLink-time */
		if (!tcn)
			tcn = emptyString;
		set_property_string_0(io, "class", tcn);
		set_property_string_0(io, "last$class", tcn);
		ato = instantiate_0(io, "attributes", "NamedNodeMap");
		set_property_object_0(ato, "owner", io);
		set_property_object_0(io, "ownerDocument", doc);
		instantiate_0(io, "dataset", 0);

// only anchors with href go into links[]
		if (list && stringEqual(list, "links") &&
		    !attribPresent(t, "href"))
			list = 0;
		if (list)
			alist = get_property_object_0(owner, list);
		if (alist) {
			if((length = get_arraylength_0(alist)) < 0)
				return;
			set_array_element_object_0(alist, length, io);
			if (symname && !dupname
			    && !has_property_0(alist, symname))
				set_property_object_0(alist, symname, io);
#if 0
			if (idname && symname != idname
			    && !has_property_0(alist, idname))
				set_property_object_0(alist, idname, io);
#endif
		}		/* list indicated */
	}

	if (isradio) {
// drop down to the element within the radio array, and return that element.
// io becomes the object associated with this radio button.
// At present, io is an array.
		if((length = get_arraylength_0(io)) < 0)
			return;
		if(!(io = instantiate_array_element_0(io, length, "Element")))
			return;
	}

	set_property_string_0(io, "name", (symname ? symname : emptyString));
	set_property_string_0(io, "id", (idname ? idname : emptyString));
	set_property_string_0(io, "last$id", (idname ? idname : emptyString));

	if (href && href_url)
// This use to be instantiate_url, but with the new side effects
// on Anchor, Image, etc, we can just set the string.
		set_property_string_0(io, href, href_url);

	if (t->action == TAGACT_INPUT) {
/* link back to the form that owns the element */
		set_property_object_0(io, "form", owner);
	}

	connectTagObject(t, io);

	strcpy(upname, t->info->name);
	caseShift(upname, 'u');
// DocType has nodeType = 10, see startwindow.js
	if(t->action != TAGACT_DOCTYPE) {
		set_property_string_0(io, "nodeName", upname);
		set_property_string_0(io, "tagName", upname);
		set_property_number_0(io, "nodeType", 1);
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

static void rebuildSelector(Tag *sel, JS::HandleObject oa, int len2)
{
	int i2 = 0;
	bool check2;
	char *s;
	const char *selname;
	bool changed = false;
	Tag *t, *t0 = 0;
	JS::RootedObject oo(cxa);		/* option object */
	JS::RootedObject selobj(cxa);		/* select object */
	JS::RootedObject tobj(cxa);

	selname = sel->name;
	if (!selname)
		selname = "?";
	debugPrint(4, "testing selector %s %d", selname, len2);
	sel->lic = (sel->multiple ? 0 : -1);
	t = cw->optlist;

	        JSAutoCompartment ac(cxa, tagToCompartment(sel));
	selobj = tagToObject(sel);

	while (t && i2 < len2) {
		t0 = t;
/* there is more to both lists */
		if (t->controller != sel) {
			t = t->same;
			continue;
		}

/* find the corresponding option object */
		if (!(oo = get_array_element_object_0(oa, i2))) {
/* Wow this shouldn't happen. */
/* Guess I'll just pretend the array stops here. */
			len2 = i2;
			break;
		}

		tobj = tagToObject(t);
		if (tobj != oo) {
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

		t->rchecked = get_property_bool_0(oo, "defaultSelected");
		check2 = get_property_bool_0(oo, "selected");
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
		s = get_property_string_0(oo, "text");
		if ((s && !t->textval) || !stringEqual(t->textval, s)) {
			nzFree(t->textval);
			t->textval = s;
			changed = true;
		} else
			nzFree(s);
		s = get_property_string_0(oo, "value");
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
			if (!(oo = get_array_element_object_0(oa, i2)))
				break;
			t = newTag(sel->f0, "option");
			t->lic = i2;
			t->controller = sel;
			connectTagObject(t, oo);
			t->step = 2;	// already decorated
			t->textval = get_property_string_0(oo, "text");
			t->value = get_property_string_0(oo, "value");
			t->checked = get_property_bool_0(oo, "selected");
			if (t->checked) {
				if (sel->multiple)
					++sel->lic;
				else
					sel->lic = i2;
			}
			t->rchecked = get_property_bool_0(oo, "defaultSelected");
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
		set_property_number_0(selobj, "selectedIndex", sel->lic);
}				/* rebuildSelector */

void rebuildSelectors(void)
{
	int i1;
	Tag *t;
	JS::RootedObject oa(cxa);		/* option array */
	JS::RootedObject tobj(cxa);		/* option array */
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

		JSAutoCompartment ac(cxa, tagToCompartment(t));

/* there should always be an options array, if not then move on */
		tobj = tagToObject(t);
		if (!(oa = get_property_object_0(tobj, "options")))
			continue;
		if ((len = get_arraylength_0(oa)) < 0)
			continue;
		rebuildSelector(t, oa, len);
	}
}

// Some primitives needed by css.c. These bounce through window.soj$
// These are called from do_rules(), which is called from a native method,
// So a compartment is always set, I guess.
static const char soj[] = "soj$";
static void sofail() { debugPrint(3, "no style object"); }

bool has_gcs(const char *name)
{
	bool found;
	JS::RootedValue v(cxa);
	JS::RootedObject j(cxa);
//	        JSAutoCompartment ac(cxa, frameToCompartment(cf));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
	if(!JS_GetProperty(cxa, g, soj, &v)) {
		sofail();
		return false;
	}
	JS_ValueToObject(cxa, v, &j);
	JS_HasProperty(cxa, j, name, &found);
	return found;
}

enum ej_proptype typeof_gcs(const char *name)
{
	enum ej_proptype l;
	JS::RootedValue v(cxa);
	JS::RootedObject j(cxa);
//	        JSAutoCompartment ac(cxa, frameToCompartment(cf));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
	if(!JS_GetProperty(cxa, g, soj, &v)) {
		sofail();
		return EJ_PROP_NONE;
	}
	JS_ValueToObject(cxa, v, &j);
	if(!JS_GetProperty(cxa, j, name, &v))
		return EJ_PROP_NONE;
	return top_proptype(v);
}

int get_gcs_number(const char *name)
{
	JS::RootedValue v(cxa);
	JS::RootedObject j(cxa);
//	        JSAutoCompartment ac(cxa, frameToCompartment(cf));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
	if(!JS_GetProperty(cxa, g, soj, &v)) {
		sofail();
		return -1;
	}
	JS_ValueToObject(cxa, v, &j);
	if(JS_GetProperty(cxa, j, name, &v) &&
	v.isInt32())
		return v.toInt32();
	return -1;
}

void set_gcs_number(const char *name, int n)
{
	bool found;
	JS::RootedValue v(cxa);
	JS::RootedObject j(cxa);
//	        JSAutoCompartment ac(cxa, frameToCompartment(cf));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
	if(!JS_GetProperty(cxa, g, soj, &v)) {
		sofail();
		return;
	}
	JS_ValueToObject(cxa, v, &j);
	v.setInt32(n);
	JS_HasProperty(cxa, j, name, &found);
	if (found)
		JS_SetProperty(cxa, j, name, v);
	else
		JS_DefineProperty(cxa, j, name, v, JSPROP_STD);
}

void set_gcs_bool(const char *name, bool b)
{
	bool found;
	JS::RootedValue v(cxa);
	JS::RootedObject j(cxa);
//	        JSAutoCompartment ac(cxa, frameToCompartment(cf));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
	if(!JS_GetProperty(cxa, g, soj, &v)) {
		sofail();
		return;
	}
	JS_ValueToObject(cxa, v, &j);
	v.setBoolean(b);
	JS_HasProperty(cxa, j, name, &found);
	if (found)
		JS_SetProperty(cxa, j, name, v);
	else
		JS_DefineProperty(cxa, j, name, v, JSPROP_STD);
}

void set_gcs_string(const char *name, const char *s)
{
	bool found;
	JS::RootedValue v(cxa);
	JS::RootedObject j(cxa);
//	        JSAutoCompartment ac(cxa, frameToCompartment(cf));
JS::RootedObject g(cxa, JS::CurrentGlobalOrNull(cxa));
	if(!JS_GetProperty(cxa, g, soj, &v)) {
		sofail();
		return;
	}
	JS_ValueToObject(cxa, v, &j);
	if(!s) s = emptyString;
	JS::RootedString m(cxa, JS_NewStringCopyZ(cxa, s));
	v.setString(m);
	JS_HasProperty(cxa, j, name, &found);
	if (found)
		JS_SetProperty(cxa, j, name, v);
	else
		JS_DefineProperty(cxa, j, name, v, JSPROP_STD);
}

// edbrowse is about to exit; close down javascript.
void jsClose(void)
{
// see if javascript is running.
	if(!cxa)
		return;
	jsUnroot();
	if(o_tail) {
		debugPrint(1, "javascript tag objects remain");
// can't really trust the shutdown process at this point
		return;
	}
// rooted objects have to free in the reverse (stack) order.
	delete rw0;
	JS_DestroyContext(cxa);
	    JS_ShutDown();
}
