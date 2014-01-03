/* js.h, a laywer between edbrowse and the javascript engine.
 * This is a form of encapsulation, and it has proven to be vital,
 * since Mozilla js changes quite often, and those changes
 * can be combined to this file, jsdom.c and jsloc.c.
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
/* globals */
extern JSRuntime *jrt; /* javascript runtime */
extern JSContext *jcx;		/* javascript context */
extern JSObject *jwloc;		/* javascript window.location */
extern JSObject *jdloc;		/* javascript document.location */

/* Prototypes of functions that are only used by the javascript layer. */
/* These can refer to javascript types in the javascript api. */
JSString * our_JS_NewStringCopyN(JSContext * cx, const char *s, size_t n) ;
JSString * our_JS_NewStringCopyZ(JSContext * cx, const char *s) ;
char * our_JSEncodeString(JSString *str) ;
const char * stringize(jsval v) ;
void initLocationClass(void) ;

#endif
