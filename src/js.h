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

typedef JS::Heap < JSObject * >HeapRootedObject;

#define PROP_FIXED (JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT)

#include <string>

using namespace std;

struct ebWindowJSState {
	JSContext *jcx;		/* javascript context */
	HeapRootedObject jwin;	/* Window (AKA the global object) */
	HeapRootedObject jdoc;	/* document object */
	HeapRootedObject jwloc;	/* javascript window.location */
	HeapRootedObject jdloc;	/* javascript document.location */
	HeapRootedObject uo;	/* javascript url object. */
};

/*********************************************************************
Place this macro at the top of any function that is invoked
from outside the javascript world.
It is a gateway that returns if javascript has died for this session,
or sets the compartment if javascript is still alive.
It returns retval, which is empty if the function is void.
Thus you can't put retval in parentheses,
even though it is usually good practice to do so.
*********************************************************************/

#define SWITCH_COMPARTMENT(retval) if (cw->jss == NULL) return retval; \
JSAutoCompartment ac(cw->jss->jcx, cw->jss->jwin)

/* Prototypes of functions that are only used by the javascript layer. */
/* These can refer to javascript types in the javascript api. */
#include "ebjs.p"

#endif
