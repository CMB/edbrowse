/*********************************************************************
This is the back-end process for javascript.
We receive calls from edbrowse,
getting and setting properties for various DOM objects.
This is the quick js version.
If you package this with the quick js libraries,
you will need to include the MIT open source license,
along with the GPL, general public license, for edbrowse.
*********************************************************************/

#include "eb.h"

#ifdef DOSLIKE
#include "vsprtf.h"
#endif // DOSLIKE

#include <quickjs/quickjs-libc.h>

// to track down memory leaks
#define LEAK
#ifdef LEAK
// the quick js pointer
struct qjp { struct qjp *next; void *ptr; short count; short lineno; };
typedef struct qjp QJP;
static QJP *qbase;

static void grab2(JSValueConst v, int lineno)
{
	QJP *s, *s2 = 0;
	void *p;
	if(!JS_IsObject(v))
		return;
	p = JS_VALUE_GET_OBJ(v);
	debugPrint(7, "%p<%d", p, lineno);
// this isn't efficient at all, but probably won't be in the production system
	for(s=qbase; s; s=s->next) {
		if(s->ptr == p && s->lineno == lineno) {
			++s->count;
			return;
		}
		s2 = s;
	}
	s = (QJP*) allocMem(sizeof(QJP));
	s->count = 1, s->ptr = p, s->next = 0, s->lineno = lineno;
	if(s2)
		s2->next = s;
	else
		qbase = s;
}

static void trackPointer(void *p)
{
	QJP *s;
	for(s = qbase; s; s = s->next)
		if(!p || s->ptr == p) {
			char mult[8];
			int z = s->count;
			char c = (z > 0 ? '<' : '>');
			if(z < 0)
				z = -z;
			mult[0] = 0;
			if(z > 1)
				sprintf(mult, "*%d", z);
			debugPrint(3, "%p%c%d%s", s->ptr, c, s->lineno, mult);
		}
}

static void release2(JSValueConst v, int lineno)
{
	QJP *s, *s2 = 0, *s3;
	int n = 0;
	bool adjusted = false;
	void *p;
	if(!JS_IsObject(v))
		return;
	p = JS_VALUE_GET_OBJ(v);
	debugPrint(7, "%p>%d", p, lineno);
	for(s=qbase; s; s=s->next) {
		if(s->ptr == p && s->lineno == lineno) {
			--s->count;
			adjusted = true;
		}
		if(p == s->ptr)
			n += s->count;
		s2 = s;
	}

	if(adjusted)
		goto check_n;

	s = (QJP*) allocMem(sizeof(QJP));
	s->count = -1, s->ptr = p, s->next = 0, s->lineno = lineno;
	--n;
	if(s2)
		s2->next = s;
	else
		qbase = s;

check_n:
	if(n < 0) {
		  debugPrint(1, "quick js pointer underflow, edbrowse is probably going to abort.");
		trackPointer(p);
	}

if(n)
		return;

// this release balances the calls to this pointer, clear them out
	s2 = 0;
	for(s = qbase; s; s = s3) {
		s3 = s->next;
		if(s->ptr == p) {
			if(s2)
				s2->next = s3;
			else
				qbase = s3;
			free(s);
			continue;
		}
		s2 = s;
	}
}

static void grabover(void)
{
	if(qbase) {
		  debugPrint(1, "quick js pointer overflow, edbrowse is probably going to abort.");
		trackPointer(0);
	}
}

#define grab(v) grab2(v, __LINE__)
#define release(v) release2(v, __LINE__)
#else
#define grab(v)
#define release(v)
#define grabover()
#endif

#define JS_Release(c, v) release(v),JS_FreeValue(c, v)

static void processError(JSContext * cx);
static void uptrace(JSContext * cx, JSValueConst node);
static void jsInterruptCheck(JSContext * cx);
static Tag *tagFromObject(JSValueConst v);
static int run_function_onearg(JSContext *cx, JSValueConst parent, const char *name, JSValueConst child);
static bool run_event(JSContext *cx, JSValueConst obj, const char *pname, const char *evname);

// The level 0 functions live right next to the engine, and in the interest
// of encapsulation, they should not be called outside of this file.
// Thus they are static.
// Some wrappers around these end in _t, with a tag argument,
// and these are global and can be called from outside.
// Other wrappers end in _win for window or _doc for document.

// determine the type of the element managed by JSValue
static enum ej_proptype top_proptype(JSContext *cx, JSValueConst v)
{
	double d;
	int n;
	if(JS_IsNull(v))
		return EJ_PROP_NULL;
	if(JS_IsArray(cx, v))
		return EJ_PROP_ARRAY;
	if(JS_IsFunction(cx, v))
		return EJ_PROP_FUNCTION;
	if(JS_IsBool(v))
		return EJ_PROP_BOOL;
	if(JS_IsNumber(v)) {
		JS_ToFloat64(cx, &d, v);
		n = d;
		return (n == d ? EJ_PROP_INT : EJ_PROP_FLOAT);
	}
	if(JS_IsString(v))
		return EJ_PROP_STRING;
	if(JS_IsObject(v))
		return EJ_PROP_OBJECT;
	return EJ_PROP_NONE;	// don't know
}

static enum ej_proptype typeof_property(JSContext *cx, JSValueConst parent, const char *name)
{
	JSValue v = JS_GetPropertyStr(cx, parent, name);
	enum ej_proptype l = top_proptype(cx, v);
	grab(v);
	JS_Release(cx, v);
	return l;
}

enum ej_proptype typeof_property_t(const Tag *t, const char *name)
{
if(!t->jslink || !allowJS)
return EJ_PROP_NONE;
return typeof_property(t->f0->cx, *((JSValue*)t->jv), name);
}

/* Return a property as a string, if it is
 * string compatible. The string is allocated, free it when done. */
static char *get_property_string(JSContext *cx, JSValueConst parent, const char *name)
{
	JSValue v = JS_GetPropertyStr(cx, parent, name);
	const char *s;
	char *s0 = NULL;
	enum ej_proptype proptype = top_proptype(cx, v);
	grab(v);
	if (proptype != EJ_PROP_NONE) {
		s = JS_ToCString(cx, v);
		s0 = cloneString(s);
		JS_FreeCString(cx, s);
		if (!s0)
			s0 = emptyString;
	}
	JS_Release(cx, v);
	return s0;
}

char *get_property_string_t(const Tag *t, const char *name)
{
if(!t->jslink || !allowJS)
return 0;
return get_property_string(t->f0->cx, *((JSValue*)t->jv), name);
}

static bool get_property_bool(JSContext *cx, JSValue parent, const char *name)
{
	JSValue v = JS_GetPropertyStr(cx, parent, name);
	bool b = false;
	grab(v);
	if(JS_IsBool(v))
		b = JS_ToBool(cx, v);
	if(JS_IsNumber(v)) {
// 0 is false all others are true.
		int32_t n = 0;
		JS_ToInt32(cx, &n, v);
		b = !!n;
	}
	JS_Release(cx, v);
	return b;
}

bool get_property_bool_t(const Tag *t, const char *name)
{
if(!t->jslink || !allowJS)
return false;
return get_property_bool(t->f0->cx, *((JSValue*)t->jv), name);
}

static int get_property_number(JSContext *cx, JSValueConst parent, const char *name)
{
	JSValue v = JS_GetPropertyStr(cx, parent, name);
	int32_t n = -1;
	grab(v);
	if(JS_IsNumber(v))
// This will truncate if the number is floating point, I think
		JS_ToInt32(cx, &n, v);
	JS_Release(cx, v);
	return n;
}

int get_property_number_t(const Tag *t, const char *name)
{
if(!t->jslink || !allowJS)
return -1;
return get_property_number(t->f0->cx, *((JSValue*)t->jv), name);
}

// This returns 0 if there is no such property, or it isn't an object.
// should this return 0 for null, which is tehcnically an object?
// How bout function or array?
// The object returned is a duplicate and must be freed.
static JSValue get_property_object(JSContext *cx, JSValueConst parent, const char *name)
{
	JSValue v = JS_GetPropertyStr(cx, parent, name);
	grab(v);
	if(JS_IsObject(v))
		return v;
	JS_Release(cx, v);
	return JS_UNDEFINED;
}

// return -1 for error
static int get_arraylength(JSContext *cx, JSValueConst a)
{
	if(!JS_IsArray(cx, a))
		return -1;
	return get_property_number(cx, a, "length");
}

// quick seems to have no direct way to access a.length or a[i],
// so I just access a[7] like 7 is a property, and hope it works.
static JSValue get_array_element_object(JSContext *cx, JSValue parent, int idx)
{
	JSAtom a = JS_NewAtomUInt32(cx, idx);
	JSValue v = JS_GetProperty(cx, parent, a);
	grab(v);
	JS_FreeAtom(cx, a);
	return v;
}

/* Get the url from a url object, special wrapper.
 * Owner object is passed, look for obj.href, obj.src, or obj.action.
 * Return that if it's a string, or its member href if it is a url.
 * The result, coming from get_property_string, is allocated. */
static char *get_property_url(JSContext *cx, JSValueConst owner, bool action)
{
	enum ej_proptype mtype;	/* member type */
	JSValue uo = JS_UNDEFINED;	/* url object */
	char *s;
	if (action) {
		mtype = typeof_property(cx, owner, "action");
		if (mtype == EJ_PROP_STRING)
			return get_property_string(cx, owner, "action");
		if (mtype != EJ_PROP_OBJECT)
			return 0;
		uo = get_property_object(cx, owner, "action");
	} else {
		mtype = typeof_property(cx, owner, "href");
		if (mtype == EJ_PROP_STRING)
			return get_property_string(cx, owner, "href");
		if (mtype == EJ_PROP_OBJECT)
			uo = get_property_object(cx, owner, "href");
		else if (mtype)
			return 0;
		if (JS_IsUndefined(uo)) {
			mtype = typeof_property(cx, owner, "src");
			if (mtype == EJ_PROP_STRING)
				return get_property_string(cx, owner, "src");
			if (mtype == EJ_PROP_OBJECT)
				uo = get_property_object(cx, owner, "src");
		}
	}
	if (JS_IsUndefined(uo))
		return 0;
	s = get_property_string(cx, uo, "href$val");
	JS_Release(cx, uo);
	return s;
}

char *get_property_url_t(const Tag *t, bool action)
{
if(!t->jslink || !allowJS)
return 0;
return get_property_url(t->f0->cx, *((JSValue*)t->jv), action);
}

char *get_dataset_string_t(const Tag *t, const char *p)
{
	JSContext *cx = t->f0->cx;
	char *v;
	if(!t->jslink)
		return 0;
	if (!strncmp(p, "data-", 5)) {
		char *k;
		JSValue ds = get_property_object(cx, *((JSValue*)t->jv), "dataset");
		if(JS_IsUndefined(ds))
			return 0;
		k = cloneString(p + 5);
		camelCase(k);
		v = get_property_string(cx, ds, k);
		nzFree(k);
		JS_Release(cx, ds);
	} else
		v = get_property_string(cx, *((JSValue*)t->jv), p);
	return v;
}

char *get_style_string_t(const Tag *t, const char *name)
{
	JSContext *cx = t->f0->cx;
	JSValue so; // style object
	char *result;
	if(!t->jslink || !allowJS)
		return 0;
	so = get_property_object(cx, *((JSValue*)t->jv), "style");
	if(JS_IsUndefined(so))
		return 0;
	result = get_property_string(cx, so, name);
	JS_Release(cx, so);
	return result;
}

// Before we write set_property_string, we need some getters and setters.

static JSValue getter_innerHTML(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	return JS_GetPropertyStr(cx, this, "inner$HTML");
}

static JSValue setter_innerHTML(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	JSValue c1, c2;
	char *run;
	int run_l;
	Tag *t;
	const char *h = JS_ToCString(cx, argv[0]);
	if (!h)			// should never happen
		return JS_UNDEFINED;
	debugPrint(5, "setter h in");
	jsInterruptCheck(cx);
// remove the preexisting children.
	c1 = JS_GetPropertyStr(cx, this, "childNodes");
	grab(c1);
	if(!JS_IsArray(cx, c1)) {
// no child nodes array, don't do anything.
// This should never happen.
		debugPrint(5, "setter h fail");
		JS_Release(cx, c1);
		JS_FreeCString(cx, h);
		return JS_UNDEFINED;
	}
// make new childNodes array
	c2 = JS_NewArray(cx);
	grab(c2);
	JS_SetPropertyStr(cx, this, "childNodes", JS_DupValue(cx, c2));
	JS_SetPropertyStr(cx, this, "inner$HTML", JS_NewAtomString(cx, h));

// Put some tags around the html, so tidy can parse it.
	run = initString(&run_l);
	stringAndString(&run, &run_l, "<!DOCTYPE public><body>\n");
	stringAndString(&run, &run_l, h);
	if (*h && h[strlen(h) - 1] != '\n')
		stringAndChar(&run, &run_l, '\n');
	stringAndString(&run, &run_l, "</body>");

// now turn the html into objects
		t = tagFromObject(this);
	if(t) {
		html_from_setter(t, run);
	} else {
		debugPrint(1, "innerHTML finds no tag, cannot parse");
	}
	nzFree(run);
	debugPrint(5, "setter h out");

	run_function_onearg(cx, *((JSValue*)cf->winobj), "textarea$html$crossover", this);

// mutation fix up from native code
	{
		JSValue g = JS_GetGlobalObject(cx), r;
		JSAtom a = JS_NewAtom(cx, "mutFixup");
		JSValue l[4];
		l[0] = this;
		l[1] = JS_FALSE;
		l[2] = c2;
		l[3] = c1;
		r = JS_Invoke(cx, g, a, 4, l);
// worked, didn't work, I don't care.
		JS_FreeValue(cx, r);
		JS_FreeValue(cx, g);
		JS_FreeAtom(cx, a);
	}

		JS_Release(cx, c2);
		JS_Release(cx, c1);
		JS_FreeCString(cx, h);
	return JS_UNDEFINED;
}

static JSValue getter_value(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	return JS_GetPropertyStr(cx, this, "val$ue");
}

static JSValue setter_value(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	char *k;
	Tag *t;
	const char *h = JS_ToCString(cx, argv[0]);
	if (!h)			// should never happen
		return JS_UNDEFINED;
	debugPrint(5, "setter v in");
	JS_SetPropertyStr(cx, this, "val$ue", JS_NewAtomString(cx, h));
	k = cloneString(h);
	prepareForField(k);
	JS_FreeCString(cx, h);
	t = tagFromObject(this);
	if(t) {
		debugPrint(4, "value tag %d=%s", t->seqno, k);
		domSetsTagValue(t, k);
	}
	nzFree(k);
	debugPrint(5, "setter v out");
	return JS_UNDEFINED;
}

static void set_property_string(JSContext *cx, JSValueConst parent, const char *name,
			    const char *value)
{
	bool defset = false;
	JSCFunction *getter = 0;
	JSCFunction *setter = 0;
	const char *altname;
	if (stringEqual(name, "innerHTML"))
		getter = getter_innerHTML,
		setter = setter_innerHTML,
		    altname = "inner$HTML";
	if (stringEqual(name, "value")) {
// Only meaningful in the Element class
		JSValue dc = JS_GetPropertyStr(cx, parent, "dom$class");
		const char *dcs = JS_ToCString(cx, dc);
		grab(dc);
		if(stringEqual(dcs, "Element"))
			getter = getter_value,
			setter = setter_value,
			    altname = "val$ue";
		JS_FreeCString(cx, dcs);
		JS_Release(cx, dc);
	}
	if (setter) {
// see if we already did this - does the property show up as a string?
		if(typeof_property(cx, parent, name) != EJ_PROP_STRING)
			defset = true;
	}
	if (defset) {
		JSAtom a = JS_NewAtom(cx, name);
		JS_DefinePropertyGetSet(cx, parent, a,
		JS_NewCFunction(cx, getter, "get", 0),
		JS_NewCFunction(cx, setter, "set", 0),
		JS_PROP_ENUMERABLE);
		JS_FreeAtom(cx, a);
	}
	if (!value)
		value = emptyString;
	JS_SetPropertyStr(cx, parent, (setter ? altname : name), JS_NewAtomString(cx, value));
}

void set_property_string_t(const Tag *t, const char *name, const char * v)
{
	if(!t->jslink || !allowJS)
		return;
	set_property_string(t->f0->cx, *((JSValue*)t->jv), name, v);
}

void set_property_string_win(const Frame *f, const char *name, const char *v)
{
	set_property_string(f->cx, *((JSValue*)f->winobj), name, v);
}

void set_property_string_doc(const Frame *f, const char *name, const char *v)
{
	set_property_string(f->cx, *((JSValue*)f->docobj), name, v);
}

static void set_property_bool(JSContext *cx, JSValueConst parent, const char *name, bool n)
{
	JS_SetPropertyStr(cx, parent, name, JS_NewBool(cx, n));
}

void set_property_bool_t(const Tag *t, const char *name, bool v)
{
	if(!t->jslink || !allowJS)
		return;
	set_property_bool(t->f0->cx, *((JSValue*)t->jv), name, v);
}

void set_property_bool_win(const Frame *f, const char *name, bool v)
{
	set_property_bool(f->cx, *((JSValue*)f->winobj), name, v);
}

static void set_property_number(JSContext *cx, JSValueConst parent, const char *name, int n)
{
	JS_SetPropertyStr(cx, parent, name, JS_NewInt32(cx, n));
}

void set_property_number_t(const Tag *t, const char *name, int v)
{
	if(!t->jslink || !allowJS)
		return;
	set_property_number(t->f0->cx, *((JSValue*)t->jv), name, v);
}

// the next two functions duplicate the object value;
// you are still responsible for the original.
static void set_property_object(JSContext *cx, JSValueConst parent, const char *name, JSValueConst child)
{
	JS_SetPropertyStr(cx, parent, name, JS_DupValue(cx, child));
}

void set_property_object_t(const Tag *t, const char *name, const Tag *t2)
{
	if (!allowJS || !t->jslink || !t2->jslink)
		return;
	set_property_object(t->f0->cx, *((JSValue*)t->jv), name, *((JSValue*)t2->jv));
}

static void set_array_element_object(JSContext *cx, JSValueConst parent, int idx, JSValueConst child)
{
	JSAtom a = JS_NewAtomUInt32(cx, idx);
	JS_SetProperty(cx, parent, a, JS_DupValue(cx, child));
	JS_FreeAtom(cx, a);
}

void set_dataset_string_t(const Tag *t, const char *name, const char *v)
{
	JSContext *cx;
	JSValue dso; // dataset object
	if(!t->jslink || !allowJS)
		return;
	cx = t->f0->cx;
	dso = get_property_object(cx, *((JSValue*)t->jv), "dataset");
	if(!JS_IsUndefined(dso)) {
		set_property_string(cx, dso, name, v);
		JS_Release(cx, dso);
	}
}

static void delete_property(JSContext *cx, JSValueConst parent, const char *name)
{
	JSAtom a = JS_NewAtom(cx, name);
	JS_DeleteProperty(cx, parent, a, 0);
	JS_FreeAtom(cx, a);
}

void delete_property_t(const Tag *t, const char *name)
{
	if(!t->jslink || !allowJS)
		return;
	delete_property(t->f0->cx, *((JSValue*)t->jv), name);
}

void delete_property_win(const Frame *f, const char *name)
{
	if(!f->jslink || !allowJS)
		return;
	delete_property(f->cx, *((JSValue*)f->winobj), name);
}

void delete_property_doc(const Frame *f, const char *name)
{
	if(!f->jslink || !allowJS)
		return;
	delete_property(f->cx, *((JSValue*)f->docobj), name);
}

static JSValue instantiate_array(JSContext *cx, JSValueConst parent, const char *name)
{
	debugPrint(5, "new Array");
	JSValue a = JS_NewArray(cx);
	grab(a);
	set_property_object(cx, parent, name, a);
	return a;
}

static JSValue instantiate(JSContext *cx, JSValueConst parent, const char *name,
			  const char *classname)
{
	JSValue o; // the new object
	if (!classname) {
		debugPrint(5, "new Object");
		o = JS_NewObject(cx);
		grab(o);
	} else {
		debugPrint(5, "new %s", classname);
		JSValue g= JS_GetGlobalObject(cx);
		JSValue v, l[1];
		v = JS_GetPropertyStr(cx, g, classname);
		JS_FreeValue(cx, g);
		grab(v);
		if(!JS_IsFunction(cx, v)) {
			debugPrint(3, "no such class %s", classname);
			JS_Release(cx, v);
			return JS_UNDEFINED;
		}
		o = JS_CallConstructor(cx, v, 0, l);
		grab(o);
		JS_Release(cx, v);
		if(JS_IsException(o)) {
			if (intFlag)
				i_puts(MSG_Interrupted);
			processError(cx);
			debugPrint(3, "failure on new %s()", classname);
			uptrace(cx, parent);
			JS_Release(cx, o);
			return JS_UNDEFINED;
		}
	}
	set_property_object(cx, parent, name, o);
	return o;
}

static JSValue instantiate_array_element(JSContext *cx, JSValueConst parent, int idx,
					const char *classname)
{
	JSValue o; // the new object
	if (!classname) {
		o = JS_NewObject(cx);
		debugPrint(5, "new Object for %d", idx);
		grab(o);
	} else {
		debugPrint(5, "new %s for %d", classname, idx);
		JSValue g = JS_GetGlobalObject(cx);
		JSValue v, l[1];
		v = JS_GetPropertyStr(cx, g, classname);
		JS_FreeValue(cx, g);
		grab(v);
		if(!JS_IsFunction(cx, v)) {
			debugPrint(3, "no such class %s", classname);
			JS_Release(cx, v);
			return JS_UNDEFINED;
		}
		o = JS_CallConstructor(cx, v, 0, l);
		grab(o);
		JS_Release(cx, v);
		if(JS_IsException(o)) {
			if (intFlag)
				i_puts(MSG_Interrupted);
			processError(cx);
			debugPrint(3, "failure on new %s()", classname);
			uptrace(cx, parent);
			JS_Release(cx, o);
			return JS_UNDEFINED;
		}
	}
	set_array_element_object(cx, parent, idx, o);
	return o;
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
Function returns a bogus type like object. true
Function returns undefined. true
Function doesn't exist. false.
Function encounters an error during execution. false.
*********************************************************************/

static bool run_function_bool(JSContext *cx, JSValueConst parent, const char *name)
{
	int dbl = 3;		// debug level
	int32_t seqno = -1;
		JSValue v, r, l[1];
	if (stringEqual(name, "ontimer")) {
		dbl = 4;
		v = JS_GetPropertyStr(cx, parent, "tsn");
		grab(v);
		if(JS_IsNumber(v))
			JS_ToInt32(cx, &seqno, v);
		JS_Release(cx, v);
	}
	v = JS_GetPropertyStr(cx, parent, name);
	grab(v);
	if(!JS_IsFunction(cx, v)) {
		debugPrint(3, "no such function %s", name);
		JS_Release(cx, v);
		return false;
	}
	if (seqno > 0)
		debugPrint(dbl, "exec %s timer %d", name, seqno);
	else
		debugPrint(dbl, "exec %s", name);
	r = JS_Call(cx, v, parent, 0, l);
	grab(r);
	JS_Release(cx, v);
	if(!JS_IsException(r)) {
		bool rc = false;
		debugPrint(dbl, "exec complete");
		if(JS_IsBool(r))
			rc = JS_ToBool(cx, r);
		if(JS_IsNumber(r)) {
			int32_t n = 1;
			JS_ToInt32(cx, &n, r);
			rc = !!n;
		}
		JS_Release(cx, r);
		return rc;
	}
// error in execution
	if (intFlag)
		i_puts(MSG_Interrupted);
	processError(cx);
	debugPrint(3, "failure on %s()", name);
	uptrace(cx, parent);
	debugPrint(3, "exec complete");
	return false;
}

bool run_function_bool_t(const Tag *t, const char *name)
{
	if (!allowJS || !t->jslink)
		return false;
	return run_function_bool(t->f0->cx, *((JSValue*)t->jv), name);
}

bool run_function_bool_win(const Frame *f, const char *name)
{
	if (!allowJS || !f->jslink)
		return false;
	return run_function_bool(f->cx, *((JSValue*)f->winobj), name);
}

void run_ontimer(const Frame *f, const char *backlink)
{
	JSContext *cx = f->cx;
// timer object from its backlink
	JSValue to = get_property_object(cx, *((JSValue*)f->winobj), backlink);
	if(JS_IsUndefined(to)) {
		debugPrint(3, "could not find timer backlink %s", backlink);
		return;
	}
	run_event(cx, to, "timer", "ontimer");
	JS_Release(cx, to);
}

// The single argument to the function has to be an object.
// Returns -1 if the return is not int or bool
static int run_function_onearg(JSContext *cx, JSValueConst parent, const char *name, JSValueConst child)
{
		JSValue v, r, l[1];
	v = JS_GetPropertyStr(cx, parent, name);
	grab(v);
	if(!JS_IsFunction(cx, v)) {
		debugPrint(3, "no such function %s", name);
		JS_Release(cx, v);
		return -1;
	}
	l[0] = child;
	r = JS_Call(cx, v, parent, 1, l);
	grab(r);
	JS_Release(cx, v);
	if(!JS_IsException(r)) {
		int rc = -1;
		int32_t n = -1;
		if(JS_IsBool(r))
			rc = JS_ToBool(cx, r);
		if(JS_IsNumber(r)) {
			JS_ToInt32(cx, &n, r);
			rc = n;
		}
		JS_Release(cx, r);
		return rc;
	}
// error in execution
	if (intFlag)
		i_puts(MSG_Interrupted);
	processError(cx);
	debugPrint(3, "failure on %s(obj)", name);
	uptrace(cx, parent);
	JS_Release(cx, r);
	return -1;
}

int run_function_onearg_t(const Tag *t, const char *name, const Tag *t2)
{
	if (!allowJS || !t->jslink || !t2->jslink)
		return -1;
	return run_function_onearg(t->f0->cx, *((JSValue*)t->jv), name, *((JSValue*)t2->jv));
}

int run_function_onearg_win(const Frame *f, const char *name, const Tag *t2)
{
	if (!allowJS || !f->jslink || !t2->jslink)
		return -1;
	return run_function_onearg(f->cx, *((JSValue*)f->winobj), name, *((JSValue*)t2->jv));
}

int run_function_onearg_doc(const Frame *f, const char *name, const Tag *t2)
{
	if (!allowJS || !f->jslink || !t2->jslink)
		return -1;
	return run_function_onearg(f->cx, *((JSValue*)f->docobj), name, *((JSValue*)t2->jv));
}

// The single argument to the function has to be a string.
static void run_function_onestring(JSContext *cx, JSValueConst parent, const char *name,
				const char *s)
{
	JSValue v, r, l[1];
	v = JS_GetPropertyStr(cx, parent, name);
	grab(v);
	if(!JS_IsFunction(cx, v)) {
		debugPrint(3, "no such function %s", name);
		JS_Release(cx, v);
		return;
	}
	l[0] = JS_NewAtomString(cx, s);
	grab(l[0]);
	r = JS_Call(cx, v, parent, 1, l);
	grab(r);
	JS_Release(cx, v);
	JS_Release(cx, l[0]);
	if(!JS_IsException(r)) {
		JS_Release(cx, r);
		return;
	}
// error in execution
	if (intFlag)
		i_puts(MSG_Interrupted);
	processError(cx);
	debugPrint(3, "failure on %s(%s)", name, s);
	uptrace(cx, parent);
	JS_Release(cx, r);
}

void run_function_onestring_t(const Tag *t, const char *name, const char *s)
{
	if (!allowJS || !t->jslink)
		return;
	run_function_onestring(t->f0->cx, *((JSValue*)t->jv), name, s);
}

static char *run_script(JSContext *cx, const char *s)
{
	char *result = 0;
	JSValue r;
	char *s2 = 0;
	const char *s3;

// special debugging code to replace bp@ and trace@ with expanded macros.
// Warning: breakpoints and tracing can change the flow of execution
// in unusual cases, e.g. when a js verifyer checks f.toString(),
// and of course it will be very different with the debugging stuff in it.
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

	s3 = (s2 ? s2 : s);
	r = JS_Eval(cx, s3, strlen(s3),
	(jsSourceFile ? jsSourceFile : "internal"), JS_EVAL_TYPE_GLOBAL);
	grab(r);
	nzFree(s2);
	if (intFlag)
		i_puts(MSG_Interrupted);
	if(!JS_IsException(r)) {
		s = JS_ToCString(cx, r);
		if(s && *s)
			result = cloneString(s);
		JS_FreeCString(cx, s);
	} else {
		processError(cx);
	}
	JS_Release(cx, r);
	return result;
}

// execute script.text code; more efficient than the above.
void jsRunData(const Tag *t, const char *filename, int lineno)
{
	JSValue v;
	const char *s;
	JSContext *cx;
	if (!allowJS || !t->jslink)
		return;
	debugPrint(5, "> script:");
	cx = t->f0->cx;
	jsSourceFile = filename;
	jsLineno = lineno;
	v = JS_GetPropertyStr(cx, *((JSValue*)t->jv), "text");
	grab(v);
	if(!JS_IsString(v)) {
// no data
		jsSourceFile = 0;
		JS_Release(cx, v);
		return;
	}
	s = JS_ToCString(cx, v);
	if (!s || !*s) {
		jsSourceFile = 0;
		JS_FreeCString(cx, s);
		JS_Release(cx, v);
		return;
	}
// have to set currentScript
	JS_SetPropertyStr(cx, *((JSValue*)t->f0->docobj), "currentScript", JS_DupValue(cx, *((JSValue*)t->jv)));
// defer to the earlier routine if there are breakpoints
	if (strstr(s, "bp@(") || strstr(s, "trace@(")) {
		char *result = run_script(cx, s);
		nzFree(result);
	} else {
		JSValue r = JS_Eval(cx, s, strlen(s),
		(jsSourceFile ? jsSourceFile : "internal"), JS_EVAL_TYPE_GLOBAL);
		grab(r);
		if (intFlag)
			i_puts(MSG_Interrupted);
		if(JS_IsException(r))
			processError(cx);
		JS_Release(cx, r);
	}
	JS_FreeCString(cx, s);
	jsSourceFile = NULL;
	delete_property(cx, *((JSValue*)t->f0->docobj), "currentScript");
// onload handler? Should this run even if the script fails?
// Right now it does.
	if (t->js_file && !isDataURI(t->href) &&
	typeof_property(cx, *((JSValue*)t->jv), "onload") == EJ_PROP_FUNCTION)
		run_event(cx, *((JSValue*)t->jv), "script", "onload");
	debugPrint(5, "< ok");
}

// Run some javascript code under the named object, usually window.
// Pass the return value of the script back as a string.
static char *jsRunScriptResult(const Frame *f, JSValue obj, const char *str,
const char *filename, 			int lineno)
{
	char *result;
	if (!allowJS || !f->jslink)
		return NULL;
	if (!str || !str[0])
		return NULL;
	debugPrint(5, "> script:");
	jsSourceFile = filename;
	jsLineno = lineno;
	result = run_script(f->cx, str);
	jsSourceFile = NULL;
	debugPrint(5, "< ok");
	return result;
}

/* like the above but throw away the result */
void jsRunScriptWin(const char *str, const char *filename, 		 int lineno)
{
	char *s = jsRunScriptResult(cf, *((JSValue*)cf->winobj), str, filename, lineno);
	nzFree(s);
}

void jsRunScript_t(const Tag *t, const char *str, const char *filename, 		 int lineno)
{
	char *s = jsRunScriptResult(t->f0, *((JSValue*)t->f0->winobj), str, filename, lineno);
	nzFree(s);
}

char *jsRunScriptWinResult(const char *str,
const char *filename, 			int lineno)
{
return jsRunScriptResult(cf, *((JSValue*)cf->winobj), str, filename, lineno);
}

static JSValue create_event(JSContext *cx, JSValueConst parent, const char *evname)
{
	JSValue e;
	const char *evname1 = evname;
	if (evname[0] == 'o' && evname[1] == 'n')
		evname1 += 2;
// gc$event protects from garbage collection
	e = instantiate(cx, parent, "gc$event", "Event");
	set_property_string(cx, e, "type", evname1);
	return e;
}

static void unlink_event(JSContext *cx, JSValueConst parent)
{
	delete_property(cx, parent, "gc$event");
}

static bool run_event(JSContext *cx, JSValueConst obj, const char *pname, const char *evname)
{
	int rc;
	JSValue eo;	// created event object
	if(typeof_property(cx, obj, evname) != EJ_PROP_FUNCTION)
		return true;
	if (debugLevel >= 3) {
		if (debugEvent) {
			int seqno = get_property_number(cx, obj, "eb$seqno");
			debugPrint(3, "trigger %s tag %d %s", pname, seqno, evname);
		}
	}
	eo = create_event(cx, obj, evname);
	set_property_object(cx, eo, "target", obj);
	set_property_object(cx, eo, "currentTarget", obj);
	set_property_number(cx, eo, "eventPhase", 2);
	rc = run_function_onearg(cx, obj, evname, eo);
	unlink_event(cx, obj);
	JS_Release(cx, eo);
// no return or some other return is treated as true in this case
	if (rc < 0)
		rc = true;
	return rc;
}

bool run_event_t(const Tag *t, const char *pname, const char *evname)
{
	if (!allowJS || !t->jslink)
		return true;
	return run_event(t->f0->cx, *((JSValue*)t->jv), pname, evname);
}

bool run_event_win(const Frame *f, const char *pname, const char *evname)
{
	if (!allowJS || !f->jslink)
		return true;
	return run_event(f->cx, *((JSValue*)f->winobj), pname, evname);
}

bool run_event_doc(const Frame *f, const char *pname, const char *evname)
{
	if (!allowJS || !f->jslink)
		return true;
	return run_event(f->cx, *((JSValue*)f->docobj), pname, evname);
}

bool bubble_event_t(const Tag *t, const char *name)
{
	JSContext *cx;
	JSValue e;		// the event object
	bool rc;
	if (!allowJS || !t->jslink)
		return true;
	cx = t->f0->cx;
	e = create_event(cx, *((JSValue*)t->jv), name);
	rc = run_function_onearg(cx, *((JSValue*)t->jv), "dispatchEvent", e);
	if (rc && get_property_bool(cx, e, "prev$default"))
		rc = false;
	unlink_event(cx, *((JSValue*)t->jv));
	JS_Release(cx, e);
	return rc;
}

/*********************************************************************
This is for debugging, if a function or handler fails.
Climb up the tree to see where you are, similar to uptrace in startwindow.js.
As you climb up the tree, check for parentNode = null.
null is an object so it passes the object test.
This should never happen, but does in http://4x4dorogi.net
Also check for recursion.
If there is an error fetching nodeName or class, e.g. when the node is null,
(if we didn't check for parentNode = null in the above website),
then asking for nodeName causes yet another runtime error.
This invokes our machinery again, including uptrace if debug is on,
and it invokes the quick machinery again as well.
The resulting core dump has the stack so corrupted, that gdb is hopelessly confused.
*********************************************************************/

static void uptrace(JSContext * cx, JSValueConst node)
{
	static bool infunction = false;
	JSValue pn; // parent node
	enum ej_proptype pntype; // parent node type
	bool first = true;
	if (debugLevel < 3)
		return;
	if(infunction) {
		debugPrint(3, "uptrace recursion; this is unrecoverable!");
		exit(1);
	}
	infunction = true;
	while (true) {
		const char *nn = 0, *cn = 0;	// node name class name
		JSValue nnv, cnv;
		char nnbuf[20];
		nnv = JS_GetPropertyStr(cx, node, "nodeName");
		grab(nnv);
		if(JS_IsString(nnv))
			nn = JS_ToCString(cx, nnv);
		nnbuf[0] = 0;
		if(nn) {
			strncpy(nnbuf, nn, 20);
			nnbuf[20 - 1] = 0;
			JS_FreeCString(cx, nn);
		}
		if (!nnbuf[0])
			strcpy(nnbuf, "?");
		JS_Release(cx, nnv);
		cnv = JS_GetPropertyStr(cx, node, "class");
		grab(cnv);
		if(JS_IsString(cnv))
			cn = JS_ToCString(cx, cnv);
		debugPrint(3, "%s.%s", nnbuf,
		((cn && cn[0]) ? cn : "?"));
		if(cn)
			JS_FreeCString(cx, cn);
		JS_Release(cx, cnv);
		pn = JS_GetPropertyStr(cx, node, "parentNode");
		grab(pn);
		if(!first)
			JS_Release(cx, node);
		first = false;
		pntype = top_proptype(cx, pn);
		if(pntype == EJ_PROP_NONE)
			break;
		if(pntype == EJ_PROP_NULL) {
			debugPrint(3, "null");
			JS_Release(cx, pn);
			break;
		}
		if(pntype != EJ_PROP_OBJECT) {
			debugPrint(3, "parentNode not object, type %d", pntype);
			JS_Release(cx, pn);
			break;
		}
// it's an object and we're ok to climb
		node = pn;
	}
	debugPrint(3, "end uptrace");
	infunction = false;
}

/*********************************************************************
Exception has been produced.
Print the error message, including line number, and send to the debug log.
I don't know how to do this, so for now, just making a standard call.
*********************************************************************/

static void processError(JSContext * cx)
{
	JSValue exc;
	const char *msg, *stack = 0;
	JSValue sv; // stack value
	int lineno = 0;
	if (debugLevel < 3)
		return;
	exc = JS_GetException(cx);
	if(!JS_IsObject(exc))
		return; // this should never happen
	msg = JS_ToCString(cx, exc); // this runs ext.toString()
	sv = JS_GetPropertyStr(cx, exc, "stack");
	if(JS_IsString(sv))
		stack = JS_ToCString(cx, sv);
	if(stack && jsSourceFile) {
// pull line number out of the stack trace; this assumes a particular format.
// First line is first stack frame, and should be @ function (file:line)
// But what if file contains : or other punctuations?
// I'll make a modest effort to guard against that.
		const char *p = strchr(stack, '\n');
		if(p) {
			if(p > stack && p[-1] == ')') --p;
			while(p > stack && isdigit(p[-1])) --p;
			if(p > stack && p[-1] == ':') --p;
			if(*p == ':' && isdigit(p[1]))
				lineno = atoi(p+1);
			if(lineno < 0) lineno = 0;
		}
	}
// in the duktape version, the line number was off by 1, so I adjusted it;
// in quick, the line number is accurate, so I have to unadjust it.
	if(lineno)
		debugPrint(3, "%s line %d: %s", jsSourceFile, lineno + jsLineno - 1, msg);
	else
		debugPrint(3, "%s", msg);
	if(stack)
		debugPrint(4, "%s", stack);
	if(stack)
		JS_FreeCString(cx, stack);
	JS_FreeCString(cx, msg);
	JS_FreeValue(cx, sv);
	JS_FreeValue(cx, exc);
}

// This function takes over the JSValue, the caller should not free it.
// disconnect TagObject will free it.
static void connectTagObject(Tag *t, JSValue p)
{
	JSContext *cx = t->f0->cx;
	t->jv = allocMem(sizeof(p));
	*((JSValue*)t->jv) = p;
	t->jslink = true;
	debugPrint(6, "connect %d %s", t->seqno, t->info->name);
// Below a frame, t could be a manufactured document for the new window.
// We don't want to set eb$seqno in this case.
	if(t->action != TAGACT_DOC) {
		JS_DefinePropertyValueStr(cx, p, "eb$seqno", JS_NewInt32(cx, t->seqno), 0);
		JS_DefinePropertyValueStr(cx, p, "eb$gsn", JS_NewInt32(cx, t->gsn), 0);
	}
}

void disconnectTagObject(Tag *t)
{
	if (!t->jslink)
		return;
	JS_Release(t->f0->cx, *((JSValue*)t->jv));
	free(t->jv);
	t->jv = 0;
	t->jslink = false;
	debugPrint(6, "disconnect %d %s", t->seqno, t->info->name);
}

// this is for frame expansion
void reconnectTagObject(Tag *t)
{
	JSValue cdo;	// contentDocument object
	cdo = JS_DupValue(cf->cx, *((JSValue*)cf->docobj));
// this duplication represents a regrab on the document object.
// It will be freed when the frame is freed, and when the document tag is disconnected.
	grab(cdo);
	disconnectTagObject(t);
	connectTagObject(t, cdo);
}

static Tag *tagFromObject(JSValueConst v)
{
	int i;
	if (!tagList)
		i_printfExit(MSG_NullListInform);
	if(!JS_IsObject(v)) {
		debugPrint(1, "tagFromObject(nothing)");
		return 0;
	}
	for (i = 0; i < cw->numTags; ++i) {
		Tag *t = tagList[i];
		if (t->jslink && JS_VALUE_GET_OBJ(*((JSValue*)t->jv)) == JS_VALUE_GET_OBJ(v) && !t->dead)
			return t;
	}
	debugPrint(1, "tagFromObject() returns null");
	return 0;
}

// Create a new tag for this pointer, only used by document.createElement().
static Tag *tagFromObject2(JSValueConst v, const char *tagname)
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
}

// some do-nothing native methods
static JSValue nat_void(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	return JS_UNDEFINED;
}

static JSValue nat_null(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	return JS_NULL;
}

static JSValue nat_true(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	return JS_TRUE;
}

static JSValue nat_false(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	return JS_FALSE;
}

const char *jsSourceFile;	// sourcefile providing the javascript
int jsLineno;			// line number
static JSRuntime *jsrt;
static bool js_running;
static JSContext *mwc; // master window context

static int js_main(void)
{
	jsrt = JS_NewRuntime();
	if (!jsrt) {
		fprintf(stderr, "Cannot create javascript runtime environment\n");
		return -1;
	}
	mwc = JS_NewContext(jsrt);
	return 0;
}

// base64 encode
static JSValue nat_btoa(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	char *t;
	const char *s = emptyString;
	JSValue v;
	if(argc >= 1)
		s = JS_ToCString(cx, argv[0]);
	t = base64Encode(s, strlen(s), false);
	if(argc >= 1)
		JS_FreeCString(cx, s);
	v = JS_NewAtomString(cx, t);
	nzFree(t);
	return v;
}

// base64 decode
static JSValue nat_atob(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	char *t1, *t2;
	const char *s = emptyString;
	JSValue v;
	if(argc >= 1)
		s = JS_ToCString(cx, argv[0]);
	t1 = cloneString(s);
	if(argc >= 1)
		JS_FreeCString(cx, s);
	t2 = t1 + strlen(t1);
	base64Decode(t1, &t2);
// ignore errors for now.
	*t2 = 0;
	v = JS_NewAtomString(cx, t1);
	nzFree(t1);
	return v;
}

static JSValue nat_new_location(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	const char *s = emptyString;
	if(argc >= 1)
		s = JS_ToCString(cx, argv[0]);
	if (s && *s) {
		char *t = cloneString(s);
/* url on one line, name of window on next line */
		char *u = strchr(t, '\n');
		*u++ = 0;
		debugPrint(4, "window %s|%s", t, u);
		domOpensWindow(t, u);
		nzFree(t);
	}
	if(argc >= 1)
		JS_FreeCString(cx, s);
	return JS_UNDEFINED;
}

static JSValue nat_mywin(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	return JS_GetGlobalObject(cx);
}

static JSValue nat_mydoc(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
// I wish there was a JS_GetPropertyGlobalStr.
	JSValue g = JS_GetGlobalObject(cx);
	JSValue doc = JS_GetPropertyStr(cx, g, "document");
	JS_FreeValue(cx, g);
	return doc;
}

static JSValue nat_hasFocus(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	return JS_NewBool(cx, foregroundWindow);
}

static JSValue nat_puts(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	if(argc >= 1) {
		    const char *str = JS_ToCString(cx, argv[0]);
		            if (str) {
			printf("%s", str);
			JS_FreeCString(cx, str);
		            }
	        }
	printf("\n");
	    return JS_UNDEFINED;
}

// write local file
static JSValue nat_wlf(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	const char *s = JS_ToCString(cx, argv[0]);
	int len = strlen(s);
	const char *filename = JS_ToCString(cx, argv[1]);
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
		goto done;
	fh = open(filename, O_CREAT | O_TRUNC | O_WRONLY | O_TEXT, MODE_rw);
	if (fh < 0) {
		printf("cannot create file %s\n", filename);
		goto done;
	}
	if (write(fh, s, len) < len)
		printf("cannot write file %s\n", filename);
	close(fh);
	if (stringEqual(filename, "jslocal"))
		writeShortCache();

done:
	JS_FreeCString(cx, s);
	JS_FreeCString(cx, filename);
	return JS_UNDEFINED;
}

static JSValue nat_media(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	const char *s = JS_ToCString(cx, argv[0]);
	bool rc = false;
	if (s && *s) {
		char *t = cloneString(s);
		rc = matchMedia(t);
		nzFree(t);
	}
	JS_FreeCString(cx, s);
	return JS_NewBool(cx, rc);
}

static JSValue nat_logputs(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	const char *s = JS_ToCString(cx, argv[1]);
	int32_t minlev = 99;
	JS_ToInt32(cx, &minlev, argv[0]);
	if (debugLevel >= minlev && s && *s)
		debugPrint(3, "%s", s);
	jsInterruptCheck(cx);
	JS_FreeCString(cx, s);
	return JS_UNDEFINED;
}

static JSValue nat_prompt(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	const char *msg = 0;
	const char *answer = 0;
	const char *retval = emptyString;
	char inbuf[80];
	JSValue v;
	if (argc > 0)
		msg = JS_ToCString(cx, argv[0]);
	if (argc > 1)
		answer = JS_ToCString(cx, argv[1]);
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
// chomp
		s = inbuf + strlen(inbuf);
		if (s > inbuf && s[-1] == '\n')
			*--s = 0;
		retval = inbuf[0] ? inbuf : answer;
// no answer and no input could leave retval null
		if(!retval)
			retval = emptyString;
	}
	v = JS_NewAtomString(cx, retval);
	if(argc > 0)
		JS_FreeCString(cx, msg);
	if(argc > 1)
		JS_FreeCString(cx, answer);
	return v;
}

static JSValue nat_confirm(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	const char *msg = 0;
	bool answer = false, first = true;
	char c = 'n';
	char inbuf[80];
	if(argc > 0)
		msg = JS_ToCString(cx, argv[0]);
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
	if (c == 'y' || c == 'Y')
		answer = true;
	if(argc > 0)
		JS_FreeCString(cx, msg);
	return JS_NewBool(cx, answer);
}

// Sometimes control c can interrupt long running javascript, if the script
// calls our native methods.
static void jsInterruptCheck(JSContext * cx)
{
	if (!intFlag)
		return;
// throw an exception here; not sure how to do that yet.
  puts("js interrupt throw exception not yet implemented.");
// That should stop things, unless we're in a try catch block.
// It didn't stop the script, oh well.
}

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
static JSValue getter_cd(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	Tag *t;
	jsInterruptCheck(cx);
	t = tagFromObject(this);
	if(!t)
		goto fail;
	if(!t->f1)
		forceFrameExpand(t);
	if(!t->f1 || !t->f1->jslink) // should not happen
		goto fail;
// we have to pass a copy of the document object, so we can retain the original
	return JS_DupValue(cx, *((JSValue*)t->f1->docobj));
fail:
	return JS_NULL;
}

static JSValue getter_cw(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	Tag *t;
	jsInterruptCheck(cx);
	t = tagFromObject(this);
	if(!t)
		goto fail;
	if(!t->f1)
		forceFrameExpand(t);
	if(!t->f1 || !t->f1->jslink) // should not happen
		goto fail;
// we have to pass a copy of the window object, so we can retain the original
	return JS_DupValue(cx, *((JSValue*)t->f1->winobj));
fail:
	return JS_NULL;
}

static bool remember_contracted;

static JSValue nat_unframe(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	if(argc >= 1 && JS_IsObject(argv[0])) {
		int i, n;
		Tag *t, *cdt;
		Frame *f, *f1;
		t = tagFromObject(argv[0]);
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
	return JS_UNDEFINED;
}

static JSValue nat_unframe2(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	if(argc >= 1 && JS_IsObject(argv[0])) {
		Tag *t = tagFromObject(argv[0]);
		if(t)
			t->contracted = remember_contracted;
	}
	return JS_UNDEFINED;
}

// We need to call and remember up to 3 node names, to carry dom changes
// across to html. As in parent.insertBefore(newChild, existingChild);
// These names are passed into domSetsLinkage().
static const char *embedNodeName(JSContext * cx, JSValueConst obj)
{
	static char buf1[MAXTAGNAME], buf2[MAXTAGNAME], buf3[MAXTAGNAME];
	char *b;
	static int cycle = 0;
	const char *nodeName;
	int length;
	JSValue v;

	if (++cycle == 4)
		cycle = 1;
	if (cycle == 1)
		b = buf1;
	if (cycle == 2)
		b = buf2;
	if (cycle == 3)
		b = buf3;
	*b = 0;

	v = 	JS_GetPropertyStr(cx, obj, "nodeName");
	grab(v);
	nodeName = JS_ToCString(cx, v);
	if(nodeName) {
		length = strlen(nodeName);
		if (length >= MAXTAGNAME)
			length = MAXTAGNAME - 1;
		strncpy(b, nodeName, length);
		b[length] = 0;
		JS_FreeCString(cx, nodeName);
	}
	JS_Release(cx, v);
	caseShift(b, 'l');
	return b;
}

static void domSetsLinkage(char type,
JSValueConst p_j, const char *p_name,
JSValueConst a_j, const char *a_name,
JSValueConst b_j, const char *b_name)
{
	Tag *parent, *add, *before, *c, *t;
	int action;
	char *jst;		// javascript string
	JSContext *cx = cf->cx;

// Some functions in third.js create, link, and then remove nodes, before
// there is a document. Don't run any side effects in this case.
	if (!cw->tags)
		return;

jsInterruptCheck(cx);

	if (type == 'c') {	/* create */
		parent = tagFromObject2(JS_DupValue(cx, p_j), p_name);
		if (parent) {
			debugPrint(4, "linkage, %s %d created",
				   p_name, parent->seqno);
// creating the new tag, with t->jv, represents a regrab
			grab(p_j);
			if (parent->action == TAGACT_INPUT) {
// we need to establish the getter and setter for value
				set_property_string(parent->f0->cx,
				*((JSValue*)parent->jv), "value", emptyString);
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
	cx = t->f0->cx;
	t->name = get_property_string(cx, *((JSValue*)t->jv), "name");
	t->id = get_property_string(cx, *((JSValue*)t->jv), "id");
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
		if (typeof_property(cx, *((JSValue*)t->jv), "multiple"))
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
static void domSetsLinkage1(char type,
JSValueConst p_j, const char *p_name,
JSValueConst a_j, const char *a_name)
{
domSetsLinkage(type, p_j, p_name, a_j, a_name, JS_UNDEFINED, emptyString);
}

static void domSetsLinkage2(char type,
JSValueConst p_j, const char *p_name)
{
domSetsLinkage(type, p_j, p_name, JS_UNDEFINED, emptyString, JS_UNDEFINED, emptyString);
}

static JSValue nat_log_element(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	JSValue newobj = argv[0];
	const char *tagname = JS_ToCString(cx, argv[1]);
	if (JS_IsUndefined(newobj) || !tagname)
		return JS_UNDEFINED;
	debugPrint(5, "log in");
	jsInterruptCheck(cx);
// create the innerHTML member with its setter, this has to be done in C.
	set_property_string(cx, newobj, "innerHTML", emptyString);
// pass the newly created node over to edbrowse
	domSetsLinkage2('c', newobj, tagname);
	JS_FreeCString(cx, tagname);
	debugPrint(5, "log out");
	return JS_UNDEFINED;
}

static void set_timeout(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv, bool isInterval)
{
	JSValue to;		// timer object
	JSValue fo;		// function object
// fo is handled differently, I don't grab and release as there will
// be just one at the end, and it will become a timer property.
	JSValue g;		// global object
	bool cc_error = false;
	int32_t n = 1000;		// default number of milliseconds
	JSValue r = JS_UNDEFINED;
	const char *body; // function body
	char fname[48];		/* function name */
	const char *fstr;	/* function string */
	const char *s, *fpn;

	if (argc == 0)
		return;		// no args

	debugPrint(5, "timer in");
// if second parameter is missing, leave milliseconds at 1000.
	if (argc > 1 && JS_IsNumber(argv[1]))
		JS_ToInt32(cx, &n, argv[1]);

	if(JS_IsFunction(cx, argv[0])) {
		fo = JS_DupValue(cx, argv[0]);
		JSAtom a = JS_NewAtom(cx, "toString");
		JSValue list[1];
		r = JS_Invoke(cx, argv[0], a, 0, list);
		grab(r);
		JS_FreeAtom(cx, a);
		body = 0;
		if(JS_IsString(r))
			body = JS_ToCString(cx, r);
	} else if (JS_IsString(argv[0])) {
// compile the string to get a funct.
// I do this in C in the other engines, but can't figure it out in quick, so,
// instead I use the js function that I already wrote.
		JSValue l[2];
		JSAtom a = JS_NewAtom(cx, "handle$cc");
		body = JS_ToCString(cx, argv[0]);
		g = JS_GetGlobalObject(cx);
		l[0] = argv[0];
		l[1] = g;
		fo = JS_Invoke(cx, g, a, 2, l);
		JS_FreeAtom(cx, a);
		JS_FreeValue(cx, g);
		if (JS_IsException(fo)) {
			processError(cx);
			cc_error = true;
			JS_FreeValue(cx, fo);
			fo = JS_NewCFunction(cx, nat_void, "void", 0);
		}
		if (!JS_IsFunction(cx, fo)) {
			debugPrint(3, "compiled string '%s' does not produce a function", body);
			cc_error = true;
			JS_FreeValue(cx, fo);
			fo = JS_NewCFunction(cx, nat_void, "void", 0);
		}
// Now looks like a function object, just like the previous case.
	} else {
// oops, not a function or a string.
		return;
	}

// pull the function name out of the body, if that makes sense.
	strcpy(fname, "?");
	if((fstr = body)) {
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
	}
	if(body)
		JS_FreeCString(cx, body);
	JS_Release(cx, r);

	g = JS_GetGlobalObject(cx);
	fpn = fakePropName();
	if (cc_error)
		debugPrint(3, "compile error on timer %s", fpn);
// Create a timer object.
	to = instantiate(cx, g, fpn, "Timer");
	if (JS_IsException(to)) {
		processError(cx);
		JS_FreeValue(cx, fo);
		goto done;
	}
// classs is overloaded with milliseconds, for debugging
	JS_SetPropertyStr(cx, to, "class", JS_NewInt32(cx, n));
// function is contained in an ontimer handler
// don't free fo after this line
	JS_SetPropertyStr(cx, to, "ontimer", fo);
	JS_SetPropertyStr(cx, to, "backlink", JS_NewAtomString(cx, fpn));
	JS_SetPropertyStr(cx, to, "tsn", JS_NewInt32(cx, ++timer_sn));

	domSetsTimeout(n, fname, fpn, isInterval);

done:
	JS_Release(cx, to);
	JS_FreeValue(cx, g);
	debugPrint(5, "timer out");
}

static JSValue nat_setTimeout(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	set_timeout(cx, this, argc, argv, false);
	return JS_UNDEFINED;
}

static JSValue nat_setInterval(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	set_timeout(cx, this, argc, argv, true);
	return JS_UNDEFINED;
}

static JSValue nat_clearTimeout(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	int tsn;
	char *fpn; // fake prop name
	if(!argc || !JS_IsObject(argv[0]))
		return JS_UNDEFINED;
	tsn = get_property_number(cx, argv[0], "tsn");
	fpn = get_property_string(cx, argv[0], "backlink");
	domSetsTimeout(tsn, "-", fpn, false);
	nzFree(fpn);
	return JS_UNDEFINED;
}

static JSValue nat_win_close(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	i_puts(MSG_PageDone);
// I should probably freeJSContext and close down javascript,
// but not sure I can do that while the js function is still running.
	return JS_UNDEFINED;
}

// find the frame, in the current window, that goes with this.
// Used by document.write to put the html in the right frame.
static Frame *doc2frame(JSValueConst this)
{
	Frame *f;
	for (f = &(cw->f0); f; f = f->next) {
		if (JS_VALUE_GET_OBJ(*((JSValue*)f->docobj)) == JS_VALUE_GET_OBJ(this))
			break;
	}
	return f;
}

static void dwrite(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv, bool newline)
{
	char *s;
	int s_l;
	Frame *f, *save_cf = cf;
	int i;
	s = initString(&s_l);
	for(i=0; i<argc; ++i) {
		const char *h = JS_ToCString(cx, argv[i]);
		if(h) {
			stringAndString(&s, &s_l, h);
			JS_FreeCString(cx, h);
		}
	}
	if(newline)
		stringAndChar(&s, &s_l, '\n');
	debugPrint(4, "dwrite:%s", s);
	f = doc2frame(this);
	if (!f)
		debugPrint(3, "no frame found for document.write, using the default");
	else {
		if (f != cf)
			debugPrint(4, "document.write on a different frame");
		cf = f;
	}
	dwStart();
	stringAndString(&cf->dw, &cf->dw_l, s);
	cf = save_cf;
}

static JSValue nat_doc_write(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	dwrite(cx, this, argc, argv, false);
	return JS_UNDEFINED;
}

static JSValue nat_doc_writeln(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	dwrite(cx, this, argc, argv, true);
	return JS_UNDEFINED;
}

static Frame *win2frame(JSValueConst this)
{
	Frame *f;
	for (f = &(cw->f0); f; f = f->next) {
		if (JS_VALUE_GET_OBJ(*((JSValue*)f->winobj)) == JS_VALUE_GET_OBJ(this))
			break;
	}
	return f;
}

static JSValue nat_parent(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	Frame *current = win2frame(this);
	if(!current)
		return JS_UNDEFINED;
if(current == &(cw->f0))
		return JS_DupValue(cx, *((JSValue*)current->winobj));
	if(!current->frametag) // should not happen
		return JS_UNDEFINED;
	return JS_DupValue(cx, *((JSValue*)current->frametag->f0->winobj));
}

static JSValue nat_fe(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	Frame *current = win2frame(this);
	if(!current || current == &(cw->f0) || !current->frametag)
		return JS_UNDEFINED;
	return JS_DupValue(cx, *((JSValue*)current->frametag->jv));
}

static JSValue nat_top(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	return JS_DupValue(cx, *((JSValue*)cw->f0.winobj));
}

static bool append0(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv, bool side)
{
	int i, length;
	JSValue child, cn;
	const char *thisname, *childname;
	bool rc = false;

/* we need one argument that is an object */
	if (argc != 1 || !JS_IsObject(argv[0]))
		return false;

	debugPrint(5, "append in");
	child = argv[0];
	cn = JS_GetPropertyStr(cx, this, "childNodes");
	grab(cn);
	if(!JS_IsArray(cx, cn))
		goto done;

	rc = true;
	length = get_arraylength(cx, cn);
// see if it's already there.
	for (i = 0; i < length; ++i) {
		JSValue v = get_array_element_object(cx, cn, i);
		bool same = (JS_VALUE_GET_OBJ(v) == JS_VALUE_GET_OBJ(child));
		JS_Release(cx, v);
		if(same)
// child was already there, just return.
// Were we suppose to move it to the end? idk
			goto done;
	}

// add child to the end
	set_array_element_object(cx, cn, length, child);
set_property_object(cx, child, "parentNode", this);
	rc = true;

	if (!side)
		goto done;

/* pass this linkage information back to edbrowse, to update its dom tree */
	thisname = embedNodeName(cx, this);
	childname = embedNodeName(cx, child);
	domSetsLinkage1('a', this, thisname, child, childname);

done:
	JS_Release(cx, cn);
	debugPrint(5, "append out");
	return rc;
}

static JSValue nat_apch1(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	if(append0(cx, this, argc, argv, false))
		return JS_DupValue(cx, argv[0]);
	return JS_NULL;
}

static JSValue nat_apch2(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	if(append0(cx, this, argc, argv, true))
		return JS_DupValue(cx, argv[0]);
	return JS_NULL;
}

static JSValue nat_insbf(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	int i, length, mark;
	JSValue child, item, cn;
	const char *thisname, *childname, *itemname;
	bool rc = false;

/* we need two objects */
	if (argc != 2 ||
	    !JS_IsObject(argv[0]) || !JS_IsObject(argv[1]))
		return JS_NULL;

	debugPrint(5, "before in");
	child = argv[0];
	item = argv[1];
	cn = JS_GetPropertyStr(cx, this, "childNodes");
	grab(cn);
	if(!JS_IsArray(cx, cn))
		goto done;
	rc = true;
	length = get_arraylength(cx, cn);
	mark = -1;
	for (i = 0; i < length; ++i) {
		bool same1, same2;
		JSValue v = get_array_element_object(cx, cn, i);
		same1 = (JS_VALUE_GET_OBJ(v) == JS_VALUE_GET_OBJ(child));
		same2 = (JS_VALUE_GET_OBJ(v) == JS_VALUE_GET_OBJ(item));
		JS_Release(cx, v);
		if (same1)
// should we move it to before item?
			goto done;
		if (same2)
			mark = i;
	}

	if (mark < 0) {
		rc = false;
		goto done;
	}

/* push the other elements down */
	for (i = length; i > mark; --i) {
		JSValue v = get_array_element_object(cx, cn, i - 1);
		set_array_element_object(cx, cn, i, v);
		JS_Release(cx, v);
	}
/* and place the child */
	set_array_element_object(cx, cn, mark, child);
set_property_object(cx, child, "parentNode", this);

/* pass this linkage information back to edbrowse, to update its dom tree */
	thisname = embedNodeName(cx, this);
	childname = embedNodeName(cx, child);
	itemname = embedNodeName(cx, item);
	domSetsLinkage('b', this, thisname, child, childname, item, itemname);

done:
	JS_Release(cx, cn);
	debugPrint(5, "before out");
	return (rc ? JS_DupValue(cx, argv[0]) : JS_NULL);
}

static JSValue nat_removeChild(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	int i, length, mark;
	JSValue child, cn;
	const char *thisname, *childname;

	debugPrint(5, "remove in");
	if (!JS_IsObject(argv[0]))
		return JS_NULL;
	child = argv[0];
	cn = JS_GetPropertyStr(cx, this, "childNodes");
	grab(cn);
	if(!JS_IsArray(cx, cn))
		goto fail;
	length = get_arraylength(cx, cn);
	mark = -1;
	for (i = 0; i < length; ++i) {
		JSValue v = get_array_element_object(cx, cn, i);
		bool same = (JS_VALUE_GET_OBJ(v) == JS_VALUE_GET_OBJ(child));
		JS_Release(cx, v);
		if(same) {
			mark = i;
			break;
		}
	}

	if (mark < 0)
		goto fail;

/* push the other elements down */
	for (i = mark + 1; i < length; ++i) {
		JSValue v = get_array_element_object(cx, cn, i);
		set_array_element_object(cx, cn, i - 1, v);
		JS_Release(cx, v);
	}
	set_property_number(cx, cn, "length", length - 1);
// missing parentnode must always be null
JS_SetPropertyStr(cx, child, "parentNode", JS_NULL);

/* pass this linkage information back to edbrowse, to update its dom tree */
	thisname = embedNodeName(cx, this);
	childname = embedNodeName(cx, child);
	domSetsLinkage1('r', this, thisname, child, childname);

	debugPrint(5, "remove out");
// mutation fix up from native code
	{
		JSValue g = JS_GetGlobalObject(cx), r;
		JSAtom a = JS_NewAtom(cx, "mutFixup");
		JSValue l[4];
		l[0] = this;
		l[1] = JS_FALSE;
// exception here, push an integer where the node was.
		l[2] = JS_NewInt32(cx, mark);
		l[3] = child;
		r = JS_Invoke(cx, g, a, 4, l);
// worked, didn't work, I don't care.
		JS_FreeValue(cx, r);
		JS_FreeValue(cx, g);
		JS_FreeAtom(cx, a);
	}

	JS_Release(cx, cn);
	return JS_DupValue(cx, argv[0]);

fail:
	debugPrint(5, "remove fail");
	JS_Release(cx, cn);
	return JS_NULL;
}

static JSValue nat_fetchHTTP(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	struct i_get g;
	const char *incoming_url = JS_ToCString(cx, argv[0]);
	const char *incoming_method = JS_ToCString(cx, argv[1]);
	const char *incoming_headers = JS_ToCString(cx, argv[2]);
	const char *incoming_payload = JS_ToCString(cx, argv[3]);
	char *outgoing_xhrheaders = NULL;
	char *outgoing_xhrbody = NULL;
	char *a;
	bool rc, async;
	char *s;
	int s_l;
	JSValue u;

	debugPrint(5, "xhr in");
	async = get_property_bool(cx, this, "async");
	if (!down_jsbg)
		async = false;

// asynchronous xhr before browse and after browse go down different paths.
// So far I can't get the before browse path to work,
// at least on nasa.gov, which has lots of xhrs in its onload code.
// It pushes things over to timers, which work, but the page is rendered
// shortly after browse time instead of at browse time, which is annoying.
	if (!cw->browseMode)
		async = false;

	if(JS_IsString(argv[1]) && JS_IsString(argv[3]) &&
	incoming_payload && *incoming_payload &&
		incoming_method && stringEqualCI(incoming_method, "post")) {
		if (asprintf(&a, "%s\1%s",
			     incoming_url, incoming_payload) < 0)
			i_printfExit(MSG_MemAllocError, 50);
	} else {
	a = cloneString(incoming_url);
	}
	JS_FreeCString(cx, incoming_payload);
	JS_FreeCString(cx, incoming_method);
	JS_FreeCString(cx, incoming_url);
	incoming_url = a; // now it's our allocated string

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
		connectTagObject(t, JS_DupValue(cx, this));
		grab(this);
// This routine will return, and javascript might stop altogether; do we need
// to protect this object from garbage collection?
// I don't think so, since t->jv is holding it, but it's legacy code,
// and other engines don't work that way, so I'll leave it be for now.
		JSValue g = JS_GetGlobalObject(cx);
		set_property_object(cx, g, fpn, this);
		set_property_string(cx, this, "backlink", fpn);
		JS_FreeValue(cx, g);
		t->href = (char*)incoming_url;
// t now has responsibility for incoming_url
// overloading the innerHTML field
		t->innerHTML = emptyString;
		if(JS_IsString(argv[2]))
			t->innerHTML = cloneString(incoming_headers);
		JS_FreeCString(cx, incoming_headers);
		if (cw->browseMode)
			scriptSetsTimeout(t);
		pthread_create(&t->loadthread, NULL, httpConnectBack3,
			       (void *)t);
		return JS_NewAtomString(cx, "async");
	}

	memset(&g, 0, sizeof(g));
	g.thisfile = cf->fileName;
	g.uriEncoded = true;
	g.url = incoming_url;
	g.custom_h = incoming_headers;
	g.headers_p = &outgoing_xhrheaders;
	rc = httpConnect(&g);
	outgoing_xhrbody = g.buffer;
	cnzFree(incoming_url);
	JS_FreeCString(cx, incoming_headers);
	jsInterruptCheck(cx);
	if (outgoing_xhrheaders == NULL)
		outgoing_xhrheaders = emptyString;
	if (outgoing_xhrbody == NULL)
		outgoing_xhrbody = emptyString;
	s = initString(&s_l);
	stringAndNum(&s, &s_l, rc);
	stringAndString(&s, &s_l, "\r\n\r\n");
	stringAndNum(&s, &s_l, g.code);
	stringAndString(&s, &s_l, "\r\n\r\n");
	stringAndString(&s, &s_l, outgoing_xhrheaders);
	stringAndString(&s, &s_l, outgoing_xhrbody);
	nzFree(outgoing_xhrheaders);
	nzFree(outgoing_xhrbody);

	debugPrint(5, "xhr out");
	u = JS_NewAtomString(cx, s);
	nzFree(s);
	return u;
}

static JSValue nat_resolveURL(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	const char *base = JS_ToCString(cx, argv[0]);
	const char *rel = JS_ToCString(cx, argv[1]);
	char *outgoing_url;
	JSValue u;
	outgoing_url = resolveURL(base, rel);
	if (outgoing_url == NULL)
		outgoing_url = emptyString;
	JS_FreeCString(cx, base);
	JS_FreeCString(cx, rel);
	u = JS_NewAtomString(cx, outgoing_url);
	nzFree(outgoing_url);
	return u;
}

static JSValue nat_formSubmit(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
Tag *t = tagFromObject(this);
	if(t && t->action == TAGACT_FORM) {
		debugPrint(3, "submit form tag %d", t->seqno);
		domSubmitsForm(t, false);
	} else {
		debugPrint(3, "submit form tag not found");
	}
	return JS_UNDEFINED;
}

static JSValue nat_formReset(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
Tag *t = tagFromObject(this);
	if(t && t->action == TAGACT_FORM) {
		debugPrint(3, "reset form tag %d", t->seqno);
		domSubmitsForm(t, true);
	} else {
		debugPrint(3, "reset form tag not found");
	}
	return JS_UNDEFINED;
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

// This doesn't work properly it you get or set frames[0].contentDocument.cookie
// Fix this some day!
static JSValue nat_getcook(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	startCookie();
	return JS_NewAtomString(cx, cookieCopy);
}

static JSValue nat_setcook(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	const char *newcook = JS_ToCString(cx, argv[0]);
	debugPrint(5, "cook in");
	if (newcook) {
		const char *s = strchr(newcook, '=');
		if(s && s > newcook) {
			JSValue v = JS_GetPropertyStr(cx, *((JSValue*)cf->winobj), "eb$url");
			const char *u = JS_ToCString(cx, v);
			receiveCookie(u, newcook);
			JS_FreeCString(cx, u);
			JS_FreeValue(cx, v);
		}
	}
	JS_FreeCString(cx, newcook);
	debugPrint(5, "cook out");
	return JS_UNDEFINED;
}

static JSValue nat_css_start(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
// The selection string has to be allocated - css will use it in place,
// then free it later.
	int32_t n;
	const char *s;
	int b;
	JS_ToInt32(cx, &n, argv[0]);
	s = JS_ToCString(cx, argv[1]);
	b = JS_ToBool(cx, argv[2]);
	cssDocLoad(n, cloneString(s), b);
	JS_FreeCString(cx, s);
	return JS_UNDEFINED;
}

// turn an array of html tags into an array of objects.
static JSValue objectize(JSContext *cx, Tag **tlist)
{
	int i, j;
	const Tag *t;
	JSValue a = JS_NewArray(cx);
	if(!tlist)
		return a;
	for (i = j = 0; (t = tlist[i]); ++i) {
		if (!t->jslink)	// should never happen
			continue;
		set_array_element_object(cx, a, j, *((JSValue*)t->jv));
		++j;
	}
	return a;
}

// Turn start into a tag, or 0 if start is doc or win for the current frame.
// Return false if we can't turn it into a tag within the current window.
static bool rootTag(JSValue start, Tag **tp)
{
	Tag *t;
	*tp = 0;
	if(JS_IsUndefined(start) ||
	JS_VALUE_GET_OBJ(start) == JS_VALUE_GET_OBJ(*((JSValue*)cf->winobj)) ||
	JS_VALUE_GET_OBJ(start) == JS_VALUE_GET_OBJ(*((JSValue*)cf->docobj)))
		return true;
	t = tagFromObject(start);
	if(!t)
		return false;
	*tp = t;
	return true;
}

// querySelectorAll
static JSValue nat_qsa(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	JSValue start = JS_UNDEFINED, a;
	Tag **tlist, *t;
	const char *selstring = JS_ToCString(cx, argv[0]);
	if (argc >= 2) {
		if (JS_IsObject(argv[1]))
			start = argv[1];
	}
	if (JS_IsUndefined(start))
		start = this;
	jsInterruptCheck(cx);
// node.querySelectorAll makes this equal to node.
// If you just call querySelectorAll, this is undefined.
// Then there's window.querySelectorAll and document.querySelectorAll
// rootTag() checks for all these cases.
	if(!rootTag(start, &t)) {
		a = objectize(cx, 0);
	} else {
		tlist = querySelectorAll(selstring, t);
		a = objectize(cx, tlist);
		nzFree(tlist);
	}
	JS_FreeCString(cx, selstring);
	return a;
}

// querySelector
static JSValue nat_qs(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	JSValue start = JS_UNDEFINED;
	Tag *t;
	const char *selstring = JS_ToCString(cx, argv[0]);
	if (argc >= 2) {
		if (JS_IsObject(argv[1]))
			start = argv[1];
	}
	if (JS_IsUndefined(start))
		start = this;
	jsInterruptCheck(cx);
	if(!rootTag(start, &t)) {
		JS_FreeCString(cx, selstring);
		return JS_UNDEFINED;
	}
	t = querySelector(selstring, t);
	JS_FreeCString(cx, selstring);
	if(t && t->jslink)
		return JS_DupValue(cx, *((JSValue*)t->jv));
	return JS_UNDEFINED;
}

// querySelector0
static JSValue nat_qs0(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	JSValue start;
	Tag *t;
	bool rc;
	const char *selstring = JS_ToCString(cx, argv[0]);
	start = this;
	jsInterruptCheck(cx);
	if(!rootTag(start, &t)) {
		JS_FreeCString(cx, selstring);
		return JS_FALSE;
	}
	rc = querySelector0(selstring, t);
	JS_FreeCString(cx, selstring);
	return JS_NewBool(cx, rc);
}

static JSValue nat_cssApply(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	int32_t n;
	JSValue node = argv[1];
	Tag *t;
	jsInterruptCheck(cx);
	JS_ToInt32(cx, &n, argv[0]);
	t = tagFromObject(node);
	if(t)
		cssApply(n, t);
	else
		debugPrint(3, "eb$cssApply is passed an object that does not correspond to an html tag");
	return JS_UNDEFINED;
}

static JSValue nat_cssText(JSContext * cx, JSValueConst this, int argc, JSValueConst *argv)
{
	const char *rulestring = JS_ToCString(cx, argv[0]);
	cssText(rulestring);
	JS_FreeCString(cx, rulestring);
	return JS_UNDEFINED;
}

static void createJSContext_0(Frame *f)
{
	JSContext * cx;
	JSValue g, d;
	if(!js_running)
		return;
	cx = f->cx = JS_NewContext(jsrt);
	if (!cx)
		return;
	debugPrint(3, "create js context %d", f->gsn);
// the global object, which will become window,
// and the document object.
	f->winobj = allocMem(sizeof(JSValue));
	*((JSValue*)f->winobj) = g = JS_GetGlobalObject(cx);
	grab(g);
	f->docobj = allocMem(sizeof(JSValue));
	*((JSValue*)f->docobj) = d = JS_NewObject(cx);
	grab(d);
	JS_DefinePropertyValueStr(cx, g, "document", JS_DupValue(cx, d),
	JS_PROP_ENUMERABLE);
// link to the master window
	JS_DefinePropertyValueStr(cx, g, "mw$", JS_GetGlobalObject(mwc), 0);

// bind native functions here
    JS_DefinePropertyValueStr(cx, g, "eb$newLocation",
JS_NewCFunction(cx, nat_new_location, "new_location", 1), 0);
    JS_DefinePropertyValueStr(cx, g, "my$win",
JS_NewCFunction(cx, nat_mywin, "mywin", 0), 0);
    JS_DefinePropertyValueStr(cx, g, "my$doc",
JS_NewCFunction(cx, nat_mydoc, "mydoc", 0), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$puts",
JS_NewCFunction(cx, nat_puts, "puts", 1), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$wlf",
JS_NewCFunction(cx, nat_wlf, "wlf", 2), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$media",
JS_NewCFunction(cx, nat_media, "media", 1), 0);
    JS_DefinePropertyValueStr(cx, g, "btoa",
JS_NewCFunction(cx, nat_btoa, "btoa", 1), 0);
    JS_DefinePropertyValueStr(cx, g, "atob",
JS_NewCFunction(cx, nat_atob, "atob", 1), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$voidfunction",
JS_NewCFunction(cx, nat_void, "void", 0), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$nullfunction",
JS_NewCFunction(cx, nat_null, "null", 0), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$truefunction",
JS_NewCFunction(cx, nat_true, "true", 0), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$falsefunction",
JS_NewCFunction(cx, nat_false, "false", 0), 0);
    JS_DefinePropertyValueStr(cx, g, "scroll",
JS_NewCFunction(cx, nat_void, "scroll", 0), 0);
    JS_DefinePropertyValueStr(cx, g, "scrollTo",
JS_NewCFunction(cx, nat_void, "scrollTo", 0), 0);
    JS_DefinePropertyValueStr(cx, g, "scrollBy",
JS_NewCFunction(cx, nat_void, "scrollBy", 0), 0);
    JS_DefinePropertyValueStr(cx, g, "scrollByLines",
JS_NewCFunction(cx, nat_void, "scrollByLines", 0), 0);
    JS_DefinePropertyValueStr(cx, g, "scrollByPages",
JS_NewCFunction(cx, nat_void, "scrollByPages", 0), 0);
    JS_DefinePropertyValueStr(cx, g, "focus",
JS_NewCFunction(cx, nat_void, "focus", 0), 0);
    JS_DefinePropertyValueStr(cx, g, "blur",
JS_NewCFunction(cx, nat_void, "blur", 0), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$unframe",
JS_NewCFunction(cx, nat_unframe, "unframe", 1), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$unframe2",
JS_NewCFunction(cx, nat_unframe2, "unframe2", 1), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$logputs",
JS_NewCFunction(cx, nat_logputs, "logputs", 2), 0);
    JS_DefinePropertyValueStr(cx, g, "prompt",
JS_NewCFunction(cx, nat_prompt, "prompt", 2), 0);
    JS_DefinePropertyValueStr(cx, g, "confirm",
JS_NewCFunction(cx, nat_confirm, "confirm", 1), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$logElement",
JS_NewCFunction(cx, nat_log_element, "log_element", 2), 0);
    JS_DefinePropertyValueStr(cx, g, "setTimeout",
JS_NewCFunction(cx, nat_setTimeout, "setTimeout", 2), 0);
    JS_DefinePropertyValueStr(cx, g, "clearTimeout",
JS_NewCFunction(cx, nat_clearTimeout, "clearTimeout", 1), 0);
    JS_DefinePropertyValueStr(cx, g, "setInterval",
JS_NewCFunction(cx, nat_setInterval, "setInterval", 2), 0);
    JS_DefinePropertyValueStr(cx, g, "clearInterval",
JS_NewCFunction(cx, nat_clearTimeout, "clearInterval", 1), 0);
    JS_DefinePropertyValueStr(cx, g, "close",
JS_NewCFunction(cx, nat_win_close, "win_close", 0), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$fetchHTTP",
JS_NewCFunction(cx, nat_fetchHTTP, "fetchHTTP", 4), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$parent",
JS_NewCFunction(cx, nat_parent, "parent", 0), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$top",
JS_NewCFunction(cx, nat_top, "top", 0), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$frameElement",
JS_NewCFunction(cx, nat_fe, "fe", 0), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$resolveURL",
JS_NewCFunction(cx, nat_resolveURL, "resolveURL", 2), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$formSubmit",
JS_NewCFunction(cx, nat_formSubmit, "formSubmit", 0), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$formReset",
JS_NewCFunction(cx, nat_formReset, "formReset", 0), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$getcook",
JS_NewCFunction(cx, nat_getcook, "getcook", 0), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$setcook",
JS_NewCFunction(cx, nat_setcook, "setcook", 1), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$getter_cd",
JS_NewCFunction(cx, getter_cd, "getter_cd", 0), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$getter_cw",
JS_NewCFunction(cx, getter_cw, "getter_cw", 0), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$cssDocLoad",
JS_NewCFunction(cx, nat_css_start, "css_start", 3), 0);
    JS_DefinePropertyValueStr(cx, g, "querySelectorAll",
JS_NewCFunction(cx, nat_qsa, "qsa", 2), 0);
    JS_DefinePropertyValueStr(cx, g, "querySelector",
JS_NewCFunction(cx, nat_qs, "qs", 2), 0);
    JS_DefinePropertyValueStr(cx, g, "querySelector0",
JS_NewCFunction(cx, nat_qs0, "qs0", 1), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$cssApply",
JS_NewCFunction(cx, nat_cssApply, "cssApply", 2), 0);
    JS_DefinePropertyValueStr(cx, g, "eb$cssText",
JS_NewCFunction(cx, nat_cssText, "cssText", 1), 0);

// native document methods
    JS_DefinePropertyValueStr(cx, d, "hasFocus",
JS_NewCFunction(cx, nat_hasFocus, "hasFocus", 0), 0);
    JS_DefinePropertyValueStr(cx, d, "write",
JS_NewCFunction(cx, nat_doc_write, "doc_write", 0), 0);
    JS_DefinePropertyValueStr(cx, d, "writeln",
JS_NewCFunction(cx, nat_doc_writeln, "doc_writeln", 0), 0);
    JS_DefinePropertyValueStr(cx, d, "eb$apch1",
JS_NewCFunction(cx, nat_apch1, "apch1", 1), 0);
    JS_DefinePropertyValueStr(cx, d, "eb$apch2",
JS_NewCFunction(cx, nat_apch2, "apch2", 1), 0);
    JS_DefinePropertyValueStr(cx, d, "eb$insbf",
JS_NewCFunction(cx, nat_insbf, "insbf", 2), 0);
    JS_DefinePropertyValueStr(cx, d, "removeChild",
JS_NewCFunction(cx, nat_removeChild, "removeChild", 1), 0);
    JS_DefinePropertyValueStr(cx, d, "focus",
JS_NewCFunction(cx, nat_void, "docfocus", 0), 0);
    JS_DefinePropertyValueStr(cx, d, "blur",
JS_NewCFunction(cx, nat_void, "docblur", 0), 0);
    JS_DefinePropertyValueStr(cx, d, "close",
JS_NewCFunction(cx, nat_void, "docclose", 0), 0);

// document.eb$ctx is the context number
	JS_DefinePropertyValueStr(cx, d, "eb$ctx", JS_NewInt32(cx, f->gsn), 0);
// document.eb$seqno = 0
	JS_DefinePropertyValueStr(cx, d, "eb$seqno", JS_NewInt32(cx, 0), 0);

// Sequence is to set f->fileName, then createContext(), so for a short time,
// we can rely on that variable.
// Let's make it more permanent, per context.
// Has to be nonwritable for security reasons.
// Could be null, e.g. an empty frame, but we can't pass null to quick.
	JS_DefinePropertyValueStr(cx, g, "eb$url",
	JS_NewAtomString(cx, (f->fileName ? f->fileName : emptyString)), 0);
	JS_DefinePropertyValueStr(cx, g, "eb$ctx", JS_NewInt32(cx, f->gsn), 0);
}

static void setup_window_2(void);
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
	JSValue w = *((JSValue*)cf->winobj);	// window object
	JSValue d = *((JSValue*)cf->docobj);	// document object
	JSContext *cx = cf->cx;	// current context
	JSValue nav;		// navigator object
	JSValue navpi;	// navigator plugins
	JSValue navmt;	// navigator mime types
	JSValue hist;		// history object
	struct MIMETYPE *mt;
	struct utsname ubuf;
	int i;
	char save_c;
	extern const char startWindowJS[];
	extern const char thirdJS[];

	set_property_object(cx, w, "window", w);

/* the js window/document setup script.
 * These are all the things that do not depend on the platform,
 * OS, configurations, etc. */
	jsRunScriptWin(startWindowJS, "StartWindow", 1);
// deminimization debugging is large and slow to parse,
// thus it goes in the master window, once, and shared by all windows.
	jsRunScriptWin(thirdJS, "Third", 1);

	nav = get_property_object(cx, w, "navigator");
	if (JS_IsUndefined(nav))
		return;
/* some of the navigator is in startwindow.js; the runtime properties are here. */
	set_property_string(cx, nav, "userLanguage", supported_languages[eb_lang]);
	set_property_string(cx, nav, "language", supported_languages[eb_lang]);
	set_property_string(cx, nav, "appVersion", version);
	set_property_string(cx, nav, "vendorSub", version);
	set_property_string(cx, nav, "userAgent", currentAgent);
	uname(&ubuf);
	set_property_string(cx, nav, "oscpu", ubuf.sysname);
	set_property_string(cx, nav, "platform", ubuf.machine);

/* Build the array of mime types and plugins,
 * according to the entries in the config file. */
	navpi = get_property_object(cx, nav, "plugins");
	navmt = get_property_object(cx, nav, "mimeTypes");
	if (JS_IsUndefined(navpi) || JS_IsUndefined(navmt))
		return;
	mt = mimetypes;
	for (i = 0; i < maxMime; ++i, ++mt) {
		int len;
/* po is the plugin object and mo is the mime object */
		JSValue po = instantiate_array_element(cx, navpi, i, 0);
		JSValue mo = instantiate_array_element(cx, navmt, i, 0);
		if (JS_IsUndefined(po) || JS_IsUndefined(mo))
			return;
		set_property_object(cx, mo, "enabledPlugin", po);
		set_property_string(cx, mo, "type", mt->type);
		set_property_object(cx, navmt, mt->type, mo);
		set_property_string(cx, mo, "description", mt->desc);
		set_property_string(cx, mo, "suffixes", mt->suffix);
/* I don't really have enough information from the config file to fill
 * in the attributes of the plugin object.
 * I'm just going to fake it.
 * Description will be the same as that of the mime type,
 * and the filename will be the program to run.
 * No idea if this is right or not. */
		set_property_string(cx, po, "description", mt->desc);
		set_property_string(cx, po, "filename", mt->program);
/* For the name, how about the program without its options? */
		len = strcspn(mt->program, " \t");
		save_c = mt->program[len];
		mt->program[len] = 0;
		set_property_string(cx, po, "name", mt->program);
		mt->program[len] = save_c;
		JS_Release(cx, mo);
		JS_Release(cx, po);
	}
	JS_Release(cx, navpi);
	JS_Release(cx, navmt);
	JS_Release(cx, nav);

	hist = get_property_object(cx, w, "history");
	if (JS_IsUndefined(hist))
		return;
	set_property_string(cx, hist, "current", cf->fileName);
	JS_Release(cx, hist);

	set_property_string(cx, d, "referrer", cw->referrer);
	set_property_string(cx, d, "URL", cf->fileName);
	set_property_string(cx, d, "location", cf->fileName);
	set_property_string(cx, w, "location", cf->fileName);
	jsRunScriptWin(
		    "window.location.replace = document.location.replace = function(s) { this.href = s; };Object.defineProperty(window.location,'replace',{enumerable:false});Object.defineProperty(document.location,'replace',{enumerable:false});",
		    "locreplace", 1);
	set_property_string(cx, d, "domain", getHostURL(cf->fileName));
	if (debugClone)
		set_property_bool(cx, w, "cloneDebug", true);
	if (debugEvent)
		set_property_bool(cx, w, "eventDebug", true);
	if (debugThrow)
		set_property_bool(cx, w, "throwDebug", true);
}

void freeJSContext(Frame *f)
{
	JSContext *cx;
	if (!f->jslink)
		return;
	cssFree(f);
	cx = f->cx;
	JS_Release(cx, *((JSValue*)f->winobj));
	free(f->winobj);
	JS_Release(cx, *((JSValue*)f->docobj));
	free(f->docobj);
	f->winobj = f->docobj = 0;
	f->cx = 0;
	JS_FreeContext(cx);
	debugPrint(3, "remove js context %d", f->gsn);
	f->jslink = false;
}

static bool has_property(JSContext *cx, JSValueConst parent, const char *name)
{
	JSAtom a = JS_NewAtom(cx, name);
	bool l = JS_HasProperty(cx, parent, a);
	JS_FreeAtom(cx, a);
	return l;
}

bool has_property_t(const Tag *t, const char *name)
{
	if(!t->jslink || !allowJS)
		return false;
	return has_property(t->f0->cx, *((JSValue*)t->jv), name);
}

bool has_property_win(const Frame *f, const char *name)
{
	if(!f->jslink || !allowJS)
		return false;
	return has_property(f->cx, *((JSValue*)f->winobj), name);
}

// Functions that help decorate the DOM tree, called from decorate.c.

void establish_js_option(Tag *t, Tag *sel)
{
	JSContext *cx = cf->cx; // context
	int idx = t->lic;
	JSValue oa;		// option array
	JSValue oo;		// option object
	JSValue so;		// style object
	JSValue ato;		// attributes object
	JSValue fo;		// form object
	JSValue cn; // childNodes
	JSValue selobj = *((JSValue*)sel->jv); // select object

	if(!sel->jslink)
		return;

	oa = get_property_object(cx, selobj, "options");
	if(JS_IsUndefined(oa))
		return;
	if(!JS_IsArray(cx, oa)) {
		JS_Release(cx, oa);
		return;
	}
	oo = instantiate_array_element(cx, oa, idx, "Option");
	set_property_object(cx, oo, "parentNode", oa);
/* option.form = select.form */
	fo = get_property_object(cx, selobj, "form");
	if(!JS_IsUndefined(fo)) {
		set_property_object(cx, oo, "form", fo);
		JS_Release(cx, fo);
	}
	cn = instantiate_array(cx, oo, "childNodes");
	ato = instantiate(cx, oo, "attributes", "NamedNodeMap");
	set_property_object(cx, ato, "owner", oo);
	so = instantiate(cx, oo, "style", "CSSStyleDeclaration");
	set_property_object(cx, so, "element", oo);

connectTagObject(t, oo);

	JS_Release(cx, so);
	JS_Release(cx, ato);
	JS_Release(cx, cn);
	JS_Release(cx, oa);
}

void establish_js_textnode(Tag *t, const char *fpn)
{
	JSContext *cx = cf->cx;
	JSValue so, ato, cn;
	 JSValue tagobj = instantiate(cx, *((JSValue*)cf->winobj), fpn, "TextNode");
	cn = instantiate_array(cx, tagobj, "childNodes");
	ato = instantiate(cx, tagobj, "attributes", "NamedNodeMap");
	set_property_object(cx, ato, "owner", tagobj);
	so = instantiate(cx, tagobj, "style", "CSSStyleDeclaration");
	set_property_object(cx, so, "element", tagobj);
	connectTagObject(t, tagobj);
	JS_Release(cx, so);
	JS_Release(cx, ato);
	JS_Release(cx, cn);
}

static void processStyles(JSValueConst so, const char *stylestring)
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
				set_property_string(cf->cx, so, s, sv);
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
	JSContext *cx = cf->cx;
	JSValue owner;
	JSValue alist = JS_UNDEFINED;
	JSValue io = JS_UNDEFINED;	// input object
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
	JSValue so;	/* obj.style */
	JSValue ato;	/* obj.attributes */
	JSValue ds; // dataset
	JSValue ca; // child array
	char upname[MAXTAGNAME];
	char classtweak[MAXTAGNAME + 4];

	debugPrint(5, "domLink %s.%d name %s",
		   classname, extra, (symname ? symname : emptyString));
	extra &= 6;

	if(stringEqual(classname, "HTMLElement") ||
	stringEqual(classname, "CSSStyleDeclaration"))
		strcpy(classtweak, classname);
	else
		sprintf(classtweak, "z$%s", classname);

	if(owntag)
		owner = *((JSValue*)owntag->jv);
if(extra == 2)
		owner = *((JSValue*)cf->winobj);
if(extra == 4)
		owner = *((JSValue*)cf->docobj);

	if (symname && typeof_property(cx, owner, symname)) {
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
			io = get_property_object(cx, owner, symname);
			if(JS_IsUndefined(io))
				return;
		} else {
// don't know why the duplicate name
			dupname = true;
		}
	}

/* The input object is nonzero if&only if the input is a radio button,
 * and not the first button in the set, thus it isce the array containing
 * these buttons. */
	if (JS_IsUndefined(io)) {
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
			if (membername && has_property(cx, owner, membername)) {
				debugPrint(3, "membername overload %s.%s",
					   classname, membername);
				membername = 0;
			}
		}
		if (!membername)
			membername = fakePropName(), fakeName = true;

		if (isradio) {	// the first radio button
			io = instantiate_array(cx,
			(fakeName ? *((JSValue*)cf->winobj) : owner), membername);
			if(JS_IsUndefined(io))
				return;
			set_property_string(cx, io, "type", "radio");
		} else {
		JSValue ca;	// child array
/* A standard input element, just create it. */
			io = instantiate(cx,
(fakeName ? *((JSValue*)cf->winobj) : owner), membername, classtweak);
			if(JS_IsUndefined(io))
				return;
// Not an array; needs the childNodes array beneath it for the children.
			ca = instantiate_array(cx, io, "childNodes");
// childNodes and options are the same for Select
			if (stringEqual(classname, "Select"))
				set_property_object(cx, io, "options", ca);
			JS_Release(cx, ca);
		}

/* deal with the 'styles' here.
object will get 'style' regardless of whether there is
anything to put under it, just like it gets childNodes whether
or not there are any.  After that, there is a conditional step.
If this node contains style='' of one or more name-value pairs,
call out to process those and add them to the object.
Don't do any of this if the tag is itself <style>. */
		if (t->action != TAGACT_STYLE) {
			so = instantiate(cx, io, "style", "CSSStyleDeclaration");
			set_property_object(cx, so, "element", io);
/* now if there are any style pairs to unpack,
 processStyles can rely on obj.style existing */
			if (stylestring)
				processStyles(so, stylestring);
			JS_Release(cx, so);
		}

/* Other attributes that are expected by pages, even if they
 * aren't populated at domLink-time */
		if (!tcn)
			tcn = emptyString;
		set_property_string(cx, io, "class", tcn);
		set_property_string(cx, io, "last$class", tcn);
		ato = instantiate(cx, io, "attributes", "NamedNodeMap");
		set_property_object(cx, ato, "owner", io);
		JS_Release(cx, ato);
		set_property_object(cx, io, "ownerDocument", *((JSValue*)cf->docobj));
		ds = instantiate(cx, io, "dataset", 0);
		JS_Release(cx, ds);

// only anchors with href go into links[]
		if (list && stringEqual(list, "links") &&
		    !attribPresent(t, "href"))
			list = 0;
		if (list)
			alist = get_property_object(cx, owner, list);
		if (!JS_IsUndefined(alist)) {
			length = get_arraylength(cx, alist);
			set_array_element_object(cx, alist, length, io);
			if (symname && !dupname
			    && !has_property(cx, alist, symname))
				set_property_object(cx, alist, symname, io);
#if 0
			if (idname && symname != idname
			    && !has_property(cx, alist, idname))
				set_property_object(cx, alist, idname, io);
#endif
			JS_Release(cx, alist);
		}		/* list indicated */
	}

	if (isradio) {
// drop down to the element within the radio array, and return that element.
// io becomes the object associated with this radio button.
// At present, io is an array.
// Borrow ca, so we can free things.
		length = get_arraylength(cx, io);
		ca = instantiate_array_element(cx, io, length, "z$Element");
		JS_Release(cx, io);
		if(JS_IsUndefined(ca))
			return;
		io = ca;
		so = instantiate(cx, io, "style", "CSSStyleDeclaration");
		set_property_object(cx, so, "element", io);
		JS_Release(cx, so);
	}

	set_property_string(cx, io, "name", (symname ? symname : emptyString));
	set_property_string(cx, io, "id", (idname ? idname : emptyString));
	set_property_string(cx, io, "last$id", (idname ? idname : emptyString));

	if (href && href_url)
// This use to be instantiate_url, but with the new side effects
// on Anchor, Image, etc, we can just set the string.
		set_property_string(cx, io, href, href_url);

	if (t->action == TAGACT_INPUT) {
/* link back to the form that owns the element */
		set_property_object(cx, io, "form", owner);
	}

	strcpy(upname, t->info->name);
	caseShift(upname, 'u');
// DocType has nodeType = 10, see startwindow.js
	if(t->action != TAGACT_DOCTYPE) {
		set_property_string(cx, io, "nodeName", upname);
		set_property_string(cx, io, "tagName", upname);
		set_property_number(cx, io, "nodeType", 1);
	}
	connectTagObject(t, io);
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

static void rebuildSelector(Tag *sel, JSValue oa, int len2)
{
	int i2 = 0;
	bool check2;
	char *s;
	const char *selname;
	bool changed = false;
	Tag *t, *t0 = 0;
	JSValue oo;		/* option object */
	JSContext *cx = sel->f0->cx;

	selname = sel->name;
	if (!selname)
		selname = "?";
	debugPrint(4, "testing selector %s %d", selname, len2);
	sel->lic = (sel->multiple ? 0 : -1);
	t = cw->optlist;

	while (t && i2 < len2) {
		bool connect_o = false;
		t0 = t;
/* there is more to both lists */
		if (t->controller != sel) {
			t = t->same;
			continue;
		}

/* find the corresponding option object */
		oo = get_array_element_object(cx, oa, i2);
		if(JS_IsUndefined(oo)) {
// Wow this shouldn't happen.
// Guess I'll just pretend the array stops here.
			len2 = i2;
			break;
		}

		if (JS_VALUE_GET_OBJ(*((JSValue*)t->jv)) != JS_VALUE_GET_OBJ(oo)) {
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
			connect_o = true;
		}

		t->rchecked = get_property_bool(cx, oo, "defaultSelected");
		check2 = get_property_bool(cx, oo, "selected");
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
		s = get_property_string(cx, oo, "text");
		if ((s && !t->textval) || !stringEqual(t->textval, s)) {
			nzFree(t->textval);
			t->textval = s;
			changed = true;
		} else
			nzFree(s);
		s = get_property_string(cx, oo, "value");
		if ((s && !t->value) || !stringEqual(t->value, s)) {
			nzFree(t->value);
			t->value = s;
		} else
			nzFree(s);
		t = t->same;
		if(!connect_o)
			JS_Release(cx, oo);
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
			oo = get_array_element_object(cx, oa, i2);
			if(JS_IsUndefined(oo))
				break;
			t = newTag(sel->f0, "option");
			t->lic = i2;
			t->controller = sel;
			connectTagObject(t, oo);
			t->step = 2;	// already decorated
			t->textval = get_property_string(cx, oo, "text");
			t->value = get_property_string(cx, oo, "value");
			t->checked = get_property_bool(cx, oo, "selected");
			if (t->checked) {
				if (sel->multiple)
					++sel->lic;
				else
					sel->lic = i2;
			}
			t->rchecked = get_property_bool(cx, oo, "defaultSelected");
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
		set_property_number(cx, *((JSValue*)sel->jv), "selectedIndex", sel->lic);
}				/* rebuildSelector */

void rebuildSelectors(void)
{
	int i1;
	Tag *t;
	JSContext *cx;
	JSValue oa;		/* option array */
	int len;		/* length of option array */

	for (i1 = 0; i1 < cw->numTags; ++i1) {
		t = tagList[i1];
		if (!t->jslink)
			continue;
		if (t->action != TAGACT_INPUT)
			continue;
		if (t->itype != INP_SELECT)
			continue;

// there should always be an options array, if not then move on
	cx = t->f0->cx;
		oa = get_property_object(cx, *((JSValue*)t->jv), "options");
		if(JS_IsUndefined(oa))
			continue;
		if ((len = get_arraylength(cx, oa)) < 0)
			continue;
		rebuildSelector(t, oa, len);
		JS_Release(cx, oa);
	}
}

// Some primitives needed by css.c. These bounce through window.soj$
static const char soj[] = "soj$";
static void sofail() { debugPrint(3, "no style object"); }

bool has_gcs(const char *name)
{
	JSContext * cx = cf->cx;
	bool l;
	JSValue g = JS_GetGlobalObject(cx), j;
	j = get_property_object(cx,  g, soj);
	if(JS_IsUndefined(j)) {
		sofail();
		JS_FreeValue(cx, g);
		return false;
	}
	        l = has_property(cx, j, name);
	JS_Release(cx, j);
	JS_FreeValue(cx, g);
	return l;
}

enum ej_proptype typeof_gcs(const char *name)
{
	enum ej_proptype l;
	JSContext * cx = cf->cx;
	JSValue g = JS_GetGlobalObject(cx), j;
	j = get_property_object(cx,  g, soj);
	if(JS_IsUndefined(j)) {
		sofail();
		JS_FreeValue(cx, g);
		return EJ_PROP_NONE;
	}
	        l = typeof_property(cx, j, name);
	JS_Release(cx, j);
	JS_FreeValue(cx, g);
	return l;
}

int get_gcs_number(const char *name)
{
	JSContext * cx = cf->cx;
	int l = -1;
	JSValue g = JS_GetGlobalObject(cx), j;
	j = get_property_object(cx,  g, soj);
	if(JS_IsUndefined(j)) {
		sofail();
		JS_FreeValue(cx, g);
		return -1;
	}
		l = get_property_number(cx, j, name);
	JS_Release(cx, j);
	JS_FreeValue(cx, g);
	return l;
}

void set_gcs_number(const char *name, int n)
{
	JSContext * cx = cf->cx;
	JSValue g = JS_GetGlobalObject(cx), j;
	j = get_property_object(cx,  g, soj);
	if(JS_IsUndefined(j)) {
		sofail();
		JS_FreeValue(cx, g);
		return;
	}
	set_property_number(cx, j, name, n);
	JS_Release(cx, j);
	JS_FreeValue(cx, g);
}

void set_gcs_bool(const char *name, bool v)
{
	JSContext * cx = cf->cx;
	JSValue g = JS_GetGlobalObject(cx), j;
	j = get_property_object(cx,  g, soj);
	if(JS_IsUndefined(j)) {
		sofail();
		JS_FreeValue(cx, g);
		return;
	}
	set_property_bool(cx, j, name, v);
	JS_Release(cx, j);
	JS_FreeValue(cx, g);
}

void set_gcs_string(const char *name, const char *s)
{
	JSContext * cx = cf->cx;
	JSValue g = JS_GetGlobalObject(cx), j;
	j = get_property_object(cx,  g, soj);
	if(JS_IsUndefined(j)) {
		sofail();
		JS_FreeValue(cx, g);
		return;
	}
	set_property_string(cx, j, name, s);
	JS_Release(cx, j);
	JS_FreeValue(cx, g);
}

void jsUnroot(void)
{
// this function isn't needed in the quick world
}

void jsClose(void)
{
	if(js_running) {
		JS_FreeContext(mwc);
		grabover();
		JS_FreeRuntime(jsrt);
	}
}
