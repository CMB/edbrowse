// This program came from the Mozilla site, for moz 52.
// https://developer.mozilla.org/en-US/docs/Mozilla/Projects/SpiderMonkey/How_to_embed_the_JavaScript_engine
// I tweaked it for moz 60.

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
static bool letterInc(JSContext *cx, unsigned argc, JS::Value *vp)
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
static bool letterDec(JSContext *cx, unsigned argc, JS::Value *vp)
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

static JSFunctionSpec nativeMethods[] = {
  JS_FN("letterInc", letterInc, 1, 0),
  JS_FN("letterDec", letterDec, 1, 0),
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

static JSRuntime *jrt;		/* our js runtime environment */

int main(int argc, const char *argv[])
{
bool iaflag; // interactive
int ci, cl;
JSContext *cxlist[3], *cx;
JSObject *glist[3], *g;
bool ok;
const char *script, *filename;
int lineno;

if(argc > 1 && !strcmp(argv[1], "-i")) iaflag = true;

    JS_Init();

cl = iaflag ? 3 : 1;

// opening a second context causes a seg fault on moz 60.
// No clue why. For now this has to run on moz 52.

for(ci=0; ci<cl; ++ci) {
if(!ci) { // first one
cx = JS_NewContext(JS::DefaultHeapMaxBytes);
} else {
// It seems not to matter whether we make independent contextts,
// or contexts under a parent context.
//cx = JS_NewContext(JS::DefaultHeapMaxBytes);
cx = JS_NewContext(JS::DefaultHeapMaxBytes, JS::DefaultNurseryBytes, cxlist[0]);
}
cxlist[ci] = cx;
    if (!cx)
        return 1;
    if (!JS::InitSelfHostedCode(cx))
        return 1;
      JSAutoRequest ar(cx);
      JS::CompartmentOptions options;
      JS::RootedObject global(cx, JS_NewGlobalObject(cx, &global_class, nullptr, JS::FireOnNewGlobalHook, options));
      if (!global)
          return 1;
g = glist[ci] = global;
        JSAutoCompartment ac(cx, g);
// why can't I just pass g?
// Why do I need a rooted object, or handle or some such?
        JS_InitStandardClasses(cx, global);
JS_DefineFunctions(cx, global, nativeMethods);
// link back to master window
if(ci) {
JS::RootedValue mw0val(cx);
mw0val = JS::ObjectValue(*glist[0]);
if(!JS_DefineProperty(cx, global, "mw0", mw0val,
(JSPROP_READONLY|JSPROP_PERMANENT))) {
puts("unable to create mw0");
return 1;
}
}
}

//  puts("after 3 loop");

{
cx = cxlist[0];
g = glist[0];
      JSAutoRequest ar(cx);
        JSAutoCompartment ac(cx, g);
//      JS::RootedValue rval(cx);
// This works, but the GC rooting guide suggests doing it a different way,
// which also works. I don't know if this new approach is part of 60.
// Well the guide recommends it so I'm trying it.
// This reminds me of memory::unique_ptr, wrapper around a pointer,
// so things are disposed of when they go out of scope.
      JS::RootedValue v(cx);
      JS::MutableHandleValue rval = &v;

        script = "letterInc('gdkkn')+letterDec('!xpsme') + ', it is '+new Date()";
        filename = "noname";
        lineno = 1;
        JS::CompileOptions opts(cx);
        opts.setFileAndLine(filename, lineno);
// The call to Evaluate was passed &rval when rval was the rooted value;
// now it's rval when using the handle wrapper around the value.
// The prototype in jsapi.h expects JS::MutableHandleValue rval as fifth parameter.
// Why then does the old way work?
// Because JS::RootedValue * casts properly into JS::MutableHandleValue, I guess.
        ok = JS::Evaluate(cx, opts, script, strlen(script), rval);
        if (!ok)
          return 1;
      JSString *str = rval.toString();
// str seems to be internal to rval, or manage by rval;
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
while(fgets(line, sizeof(line), stdin)) {
// should check for line too long here
      JSAutoRequest ar(cx);
        JSAutoCompartment ac(cx, g);
      JS::RootedValue v(cx);
      JS::MutableHandleValue rval = &v;
JSString *str;
char *es;

// change context?
if(line[0] == 'e' &&
line[1] >= '1' && line[1] <= '3' &&
isspace(line[2])) {
printf("context %c\n", line[1]);
cx = cxlist[line[1] - '1'];
g = glist[line[1] - '1'];
continue;
}

script = line;
        filename = "noname";
        lineno = 1;
        JS::CompileOptions opts(cx);
        opts.setFileAndLine(filename, lineno);
        ok = JS::Evaluate(cx, opts, script, strlen(script), rval);
if(!ok) {
ReportJSException(cx);
} else {
switch(JS_TypeOfValue(cx, rval)) {
// This enum isn't in every version
// case JSTYPE_UNDEFINED: puts("undefined"); break;
case JSTYPE_OBJECT: puts("object"); break;
case JSTYPE_FUNCTION: puts("function"); break;
case JSTYPE_STRING:
str = rval.toString();
es = JS_EncodeString(cx, str);
      printf("%s\n", es);
free(es);
break;
case JSTYPE_NUMBER:
if(rval.isInt32())
printf("%d\n", rval.toInt32());
else printf("%f\n", rval.toDouble());
break;
case JSTYPE_BOOLEAN: puts(rval.toBoolean() ? "true" : "false"); break;
// null is returned as object and doesn't trip this case, for some reason
case JSTYPE_NULL: puts("null"); break;
case JSTYPE_SYMBOL: puts("symbol"); break;
case JSTYPE_LIMIT: puts("limit"); break;
default: puts("undefined"); break;
}
}
}
}

// closing the second context causes a seg fault
for(ci=cl-1; ci>=0; --ci) {
  printf("down %d\n", ci);
    JS_DestroyContext(cxlist[ci]);
  puts("gone");
}

    JS_ShutDown();
    return 0;
}
