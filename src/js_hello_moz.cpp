// This program originally came from the Mozilla site, for moz 52.
// https://developer.mozilla.org/en-US/docs/Mozilla/Projects/SpiderMonkey/How_to_embed_the_JavaScript_engine
// I tweaked it for moz 60.
// Then I added so much stuff to it you'd hardly recognize it.

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
if(JS_TypeOfValue(cx, args[0]) ==  JSTYPE_STRING) {
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
if(JS_TypeOfValue(cx, args[0]) ==  JSTYPE_STRING) {
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

static char emptyString[] = "";
static void nzFree(void *s)
{
	if (s && s != emptyString)
		free(s);
}
void cnzFree(const void *v)
{
	nzFree((void *)v);
}

// Here begins code that can eventually move to jseng-moz.cpp

/*********************************************************************
This returns the string equivalent of the js value, but use with care.
It's only good til the next call to stringize, then it will be trashed.
If you want the result longer than that, you better copy it.
*********************************************************************/

static const char *stringize(JSContext *cx, JS::HandleValue v)
{
	static char buf[48];
	static const char *dynamic;
	int n;
	double d;
	JSString *str;
bool ok;

if(v.isNull())
return "null";

switch(JS_TypeOfValue(cx, v)) {
// This enum isn't in every version; leave it to default.
// case JSTYPE_UNDEFINED: return "undefined"; break;

case JSTYPE_OBJECT:
case JSTYPE_FUNCTION:
// invoke toString
{
JS::RootedObject p(cx);
JS_ValueToObject(cx, v, &p);
JS::RootedValue tos(cx); // toString
ok = JS_CallFunctionName(cx, p, "toString", JS::HandleValueArray::empty(), &tos);
if(ok && tos.isString()) {
cnzFree(dynamic);
str = tos.toString();
dynamic = JS_EncodeString(cx, str);
return dynamic;
}
}
return "object";

case JSTYPE_STRING:
cnzFree(dynamic);
str = v.toString();
dynamic = JS_EncodeString(cx, str);
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

static bool nat_puts(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
if(argc >= 1) puts(stringize(cx, args[0]));
args.rval().setUndefined();
  return true;
}

static char cookieCopy[] = "; ";

static bool nat_getcook(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
JS::RootedString m(cx, JS_NewStringCopyZ(cx, cookieCopy));
args.rval().setString(m);
  return true;
}

static bool nat_setcook(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
// This only works for strings
if(argc >= 1 && JS_TypeOfValue(cx, args[0]) ==  JSTYPE_STRING) {
JSString *s = args[0].toString();
char *es = JS_EncodeString(cx, s);
//foldinCookie(cx, es);
free(es);
}
args.rval().setUndefined();
  return true;
}

static bool nat_formSubmit(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
        JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
//        javaSubmitsForm(thisobj, false);
args.rval().setUndefined();
  return true;
}

static bool nat_formReset(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);
        JS::RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
//        javaSubmitsForm(thisobj, true);
args.rval().setUndefined();
  return true;
}

static bool nat_qsa(JSContext *cx, unsigned argc, JS::Value *vp)
{
char *selstring = NULL;
JS::RootedObject start(cx);
  JS::CallArgs args = CallArgsFromVp(argc, vp);
if(argc >= 1 && JS_TypeOfValue(cx, args[0]) ==  JSTYPE_STRING) {
JSString *s = args[0].toString();
selstring = JS_EncodeString(cx, s);
}
if(argc >= 2 && args[1].isObject()) {
JS_ValueToObject(cx, args[1], &start);
} else {
start = JS_THIS_OBJECT(cx, vp);
}
// call querySelectorAll in css.c
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
  JS_FN("querySelectorAll", nat_qsa, 1, 0),
  JS_FN("querySelector", nat_stub, 1, 0),
  JS_FN("querySelector0", nat_stub, 1, 0),
  JS_FN("eb$cssText", nat_stub, 1, 0),
  JS_FN("my$win", nat_mywin, 0, 0),
  JS_FN("my$doc", nat_mydoc, 0, 0),
  JS_FS_END
};

static JSFunctionSpec nativeMethodsDocument[] = {
  JS_FN("eb$apch1", nat_stub, 1, 0),
  JS_FN("eb$apch2", nat_stub, 1, 0),
  JS_FN("eb$insbf", nat_stub, 1, 0),
  JS_FN("removeChild", nat_stub, 1, 0),
  JS_FS_END
};

// I don't understand any of this. Code from:
// http://mozilla.6506.n7.nabble.com/what-is-the-replacement-of-JS-SetErrorReporter-in-spidermonkey-60-td379888.html
// I assume all these variables are somehow on stack
// and get freed when the function returns.
static void ReportJSException(JSContext *cx)
{
if(JS_IsExceptionPending(cx)) {
JS::RootedValue exception(cx);
if(JS_GetPendingException(cx,&exception) &&
exception.isObject()) {
// I don't think we need this line.
// JS::AutoSaveExceptionState savedExc(cx);
JS::Rooted<JSObject*> exceptionObject(cx,
&exception.toObject());
JSErrorReport *what =
JS_ErrorFromException(cx,exceptionObject);
if(what) {
puts(what->message().c_str());
// what->filename what->lineno
}
}
JS_ClearPendingException(cx);
}
}

// Keep some rooted objects out of scope, so we can track their pointers.
// The three global objects and the 3 document objects.
static JS::RootedObject *groot[3];
static JS::RootedObject *docroot[3];

int main(int argc, const char *argv[])
{
bool iaflag = false; // interactive
int i, top;
JSContext *cx;
bool ok;
const char *script, *filename;
int lineno;
      JSString *str;

if(argc > 1 && !strcmp(argv[1], "-i")) iaflag = true;
top = iaflag ? 3 : 1;

    JS_Init();

// Mozilla assumes one context per thread; we can run all of edbrowse
// inside one context; I think.
cx = JS_NewContext(JS::DefaultHeapMaxBytes);
if(!cx) return 1;
    if (!JS::InitSelfHostedCode(cx))         return 1;

for(i=0; i<top; ++i) {
      JSAutoRequest ar(cx);
      JS::CompartmentOptions options;
groot[i] = new       JS::RootedObject(cx, JS_NewGlobalObject(cx, &global_class, nullptr, JS::FireOnNewGlobalHook, options));
      if (!groot[i])
          return 1;
        JSAutoCompartment ac(cx, *groot[i]);
        JS_InitStandardClasses(cx, *groot[i]);
JS_DefineFunctions(cx, *groot[i], nativeMethodsWindow);

if(i) {
/*********************************************************************
Not the first compartment.
Link back to the master window.
Warning! This architecture produces a seg fault on mozjs 60 when
this program terminates and we clean up.
Creating a js class in one compartment, then instantiating an object from
that class in another compartment, seems to screw up the heap in some way.
Use this file instead of startwindow.js to demonstrate it;
then you don't need third.js or endwindow.js.

if(!mw0.compiled)
mw0.CSSStyleDeclaration = function(){ };
CSSStyleDeclaration = mw0.CSSStyleDeclaration;
document.style = new CSSStyleDeclaration;
mw0.compiled = true;
*********************************************************************/

JS::RootedValue objval(cx);
objval = JS::ObjectValue(**groot[0]);
if(!JS_DefineProperty(cx, *groot[i], "mw0", objval,
(JSPROP_READONLY|JSPROP_PERMANENT))) {
puts("unable to create mw0");
return 1;
}
objval = JS::ObjectValue(**groot[i]);
if(!JS_DefineProperty(cx, *groot[i], "window", objval,
(JSPROP_READONLY|JSPROP_PERMANENT))) {
puts("unable to create window");
return 1;
}

// time for document under window
docroot[i] = new JS::RootedObject(cx, JS_NewObject(cx, nullptr));
objval = JS::ObjectValue(**docroot[i]);
if(!JS_DefineProperty(cx, *groot[i], "document", objval,
(JSPROP_READONLY|JSPROP_PERMANENT))) {
puts("unable to create document");
return 1;
}
JS_DefineFunctions(cx, *docroot[i], nativeMethodsDocument);

// read in startwindow.js
        filename = "startwindow.js";
        lineno = 1;
        JS::CompileOptions opts(cx);
        opts.setFileAndLine(filename, lineno);
        ok = JS::Evaluate(cx, opts, filename, &objval);
if(!ok) {
ReportJSException(cx);
return 2;
}

// If you want to back it off, use endwindow.js instead of third.js
        filename = "third.js";
        lineno = 1;
        opts.setFileAndLine(filename, lineno);
        ok = JS::Evaluate(cx, opts, filename, &objval);
if(!ok) {
ReportJSException(cx);
return 2;
}

}
}

i = 0; // back to the master window

{
      JSAutoRequest ar(cx);
        JSAutoCompartment ac(cx, *groot[i]);
      JS::RootedValue v(cx);
        script = "letterInc('gdkkn')+letterDec('!xpsme') + ', it is '+new Date()";
        filename = "noname";
        lineno = 1;
        JS::CompileOptions opts(cx);
        opts.setFileAndLine(filename, lineno);
        ok = JS::Evaluate(cx, opts, script, strlen(script), &v);
        if (!ok)
          return 1;
str = v.toString();
// str seems to be internal to v, or manage by v;
// if I try delete str, free() says invalid pointer.
char *es = JS_EncodeString(cx, str);
      printf("%s\n", es);
// should we use delete or free here; either seems to work.
free(es);
// The original hello program didn't free es, but who cares, exit(0),
// thus these little sample programs sometimes leave out important details.
}

if(iaflag) {
char line[500];
// end with control d, EOF
while(fgets(line, sizeof(line), stdin)) {
// should check for line too long here
      JSAutoRequest ar(cx);

// change context?
if(line[0] == 'e' &&
line[1] >= '1' && line[1] <= '3' &&
isspace(line[2])) {
printf("context %c\n", line[1]);
i = line[1] - '1';
if(!i) continue;
// verify the permanency of object pointers
        JSAutoCompartment bc(cx, *groot[i]);
      JS::RootedValue v(cx);
        if (JS_GetProperty(cx, *groot[i], "document", &v) &&
v.isObject()) {
JS::RootedObject new_d(cx);
JS_ValueToObject(cx, v, &new_d);
JSObject *j1 = *docroot[i];
JSObject *j2 = new_d;
if(j1 == j2)
continue;
puts("object pointer mismatch, document pointer has changed!");
printf("%p versus %p\n", j1 ,j2);
return 3;
}
puts("document object is lost!");
return 3;
}

        JSAutoCompartment ac(cx, *groot[i]);
      JS::RootedValue v(cx);
script = line;
        filename = "noname";
        lineno = 1;
        JS::CompileOptions opts(cx);
        opts.setFileAndLine(filename, lineno);
        ok = JS::Evaluate(cx, opts, script, strlen(script), &v);
if(!ok) {
ReportJSException(cx);
} else {
puts(stringize(cx, v));
}
}
}

// release the rooted objects from their object pointers.
// Just trying to avoid an argument with the cleanup process
// when we destroy the context.
for(i=0; i<top; ++i) {
if(i) *docroot[i] = nullptr;
*groot[i] = nullptr;
}

JS_DestroyContext(cx);
    JS_ShutDown();
    return 0;
}
