/* js.h, a laywer between edbrowse and the javascript engine.
 * This is a form of encapsulation, and it has proven to be vital,
 * since Mozilla js changes quite often, and those changes
 * can be comfined to jsdom.cpp, jsloc.cpp, and html.cpp.
 * No other file need know about the javascript API,
 * and no other file should include jsapi.h. */

#ifndef JS_H
#define JS_H

#include <limits>
/* work around a bug where the standard UINT32_MAX isn't defined, I really hope this is correct */
#ifndef UINT32_MAX
#define UINT32_MAX std::numeric_limits<uint32_t>::max()
#endif
/* now we can include our jsapi */
#include <jsapi.h>

typedef JS::Heap<JSObject *> HeapRootedObject;

struct ebWindowJSState {
    JSContext *jcx;		/* javascript context */
    HeapRootedObject  jwin;		/* Window (AKA the global object) */
    HeapRootedObject  jdoc;		/* document object */
    HeapRootedObject jwloc;		/* javascript window.location */
    HeapRootedObject jdloc;		/* javascript document.location */
    HeapRootedObject uo;			/* javascript url object. */
};


/* Prototypes of functions that are only used by the javascript layer. */
/* These can refer to javascript types in the javascript api. */
#include "ebjs.p"
#endif
