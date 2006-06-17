/* jsloc.c
* Javascript support, the URL class and the location object.
 * The cookie string, and other things with get/set side effects.
* Copyright (c) Karl Dahlke, 2006
* This file is part of the edbrowse project, released under GPL.
*/

#include "eb.h"

#include "jsapi.h"
#include "jsprf.h"

#define PROP_FIXED (JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT)


/*********************************************************************
The URL class, and the associated window.location object, is tricky,
with lots of interacting properties and setter functions.
*********************************************************************/

static const char *emptyParms[] = { 0 };
static jsval emptyArgs[] = { 0 };

static void url_initialize(const char *url, bool readonly, bool exclude_href);

const char *
stringize(long v)
{
    static char buf[24];
    int n;
    jsdouble d;
    if(JSVAL_IS_STRING(v)) {
	return JS_GetStringBytes(JSVAL_TO_STRING(v));
    }
    if(JSVAL_IS_INT(v)) {
	n = JSVAL_TO_INT(v);
	sprintf(buf, "%d", n);
	return buf;
    }
    if(JSVAL_IS_DOUBLE(v)) {
	d = *JSVAL_TO_DOUBLE(v);
	n = d;
	if(n == d)
	    sprintf(buf, "%d", n);
	else
	    sprintf(buf, "%lf", d);
	return buf;
    }
/* Sorry, I don't look for object.toString() */
    return 0;			/* failed */
}				/* stringize */

static JSClass url_class = {
    "URL",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

/* To builld path names, host names, etc. */
static char urlbuffer[512];
static JSObject *uo;		/* the url object */
static char *uo_href;
static bool setter_suspend;

/* Are we modifying window.location? */
/*Return false if we are, because that will put a stop to javascript. */
static bool
isWinLoc(void)
{
    if(uo != jwloc && uo != jdloc) {
	nzFree(uo_href);
	uo_href = 0;
	return true;
    }
/* This call frees t, or takes it over, so you should not free it here. */
    gotoLocation(uo_href, 0, true);
    uo_href = 0;
    return false;
}				/* isWinLoc */

/* Converting to a string just pulls out the href property */
static JSBool
loc_toString(JSContext * cx, JSObject * obj, uintN argc, jsval * argv,
   jsval * rval)
{
    JS_GetProperty(jcx, obj, "href", rval);
    return JS_TRUE;
}				/* loc_toString */

static JSBool
loc_reload(JSContext * cx, JSObject * obj, uintN argc, jsval * argv,
   jsval * rval)
{
    const char *s = firstURL();
    if(s && isURL(s))
	gotoLocation(cloneString(s), 0, true);
    else
	JS_ReportError(jcx, "location.reload() cannot find a url to refresh");
    return JS_FALSE;
}				/* loc_reload */

static JSBool
loc_replace(JSContext * cx, JSObject * obj, uintN argc, jsval * argv,
   jsval * rval)
{
    const char *s;
    if(argc > 0 && JSVAL_IS_STRING(argv[0])) {
	s = stringize(argv[0]);
	if(isURL(s)) {
	    gotoLocation(cloneString(s), 0, true);
	    return JS_FALSE;
	}
    }
    JS_ReportError(jcx,
       "argument to location.replace() does not look like a url");
    return JS_FALSE;
}				/* loc_replace */

/* Put a url together from its pieces, after something has changed. */
static void
build_url(int exception, const char *e)
{
    jsval v;
    const char *prot, *slashes, *host, *pathname, *pathslash, *search, *hash;
    char *new_url;
    static const char *const noslashes[] = {
	"mailto", "telnet", "javascript", 0
    };
    setter_suspend = true;
/* I'm a little worried about the first one being freed while I'm
 * getting the next one.
 * I just don't know that much about the js heap. */
    if(exception == 1)
	prot = e;
    else {
	JS_GetProperty(jcx, uo, "protocol", &v);
	prot = stringize(v);
    }
    slashes = EMPTYSTRING;
    if(stringInListCI(noslashes, prot) < 0)
	slashes = "//";
    if(exception == 2)
	host = e;
    else {
	JS_GetProperty(jcx, uo, "host", &v);
	host = stringize(v);
    }
    if(exception == 3)
	pathname = e;
    else {
	JS_GetProperty(jcx, uo, "pathname", &v);
	pathname = stringize(v);
    }
    pathslash = EMPTYSTRING;
    if(pathname[0] != '/')
	pathslash = "/";
    if(exception == 4)
	search = e;
    else {
	JS_GetProperty(jcx, uo, "search", &v);
	search = stringize(v);
    }
    if(exception == 5)
	hash = e;
    else {
	JS_GetProperty(jcx, uo, "hash", &v);
	hash = stringize(v);
    }
    new_url =
       JS_smprintf("%s%s%s%s%s%s%s", prot, slashes, host, pathslash, pathname,
       search, hash);
    v = STRING_TO_JSVAL(JS_NewStringCopyZ(jcx, new_url));
    JS_SetProperty(jcx, uo, "href", &v);
/* I want control over this string */
    uo_href = cloneString(new_url);
    JS_smprintf_free(new_url);
    setter_suspend = false;
}				/* build_url */

/* Rebuild host, because hostname or port changed. */
static void
build_host(int exception, const char *hostname, int port)
{
    jsval v;
    const char *oldhost;
    setter_suspend = true;
    if(exception == 1) {
	JS_GetProperty(jcx, uo, "port", &v);
	port = JSVAL_TO_INT(v);
    } else {
	JS_GetProperty(jcx, uo, "hostname", &v);
	hostname = stringize(v);
    }
    JS_GetProperty(jcx, uo, "host", &v);
    oldhost = stringize(v);
    if(exception == 2 || strchr(oldhost, ':'))
	sprintf(urlbuffer, "%s:%d", hostname, port);
    else
	strcpy(urlbuffer, hostname);
    if(strlen(urlbuffer) >= sizeof (urlbuffer))
	errorPrint("@hostname:port is too long");
    v = STRING_TO_JSVAL(JS_NewStringCopyZ(jcx, urlbuffer));
    JS_SetProperty(jcx, uo, "host", &v);
    setter_suspend = false;
}				/* build_host */

/* define or set a local property */
static void
loc_def_set(const char *name, const char *s,
   JSBool(*setter) (JSContext *, JSObject *, jsval, jsval *), jsuint attr)
{
    JSBool found;
    jsval vv;
    if(s)
	vv = STRING_TO_JSVAL(JS_NewStringCopyZ(jcx, s));
    else
	vv = JS_GetEmptyStringValue(jcx);
    JS_HasProperty(jcx, uo, name, &found);
    if(found)
	JS_SetProperty(jcx, uo, name, &vv);
    else
	JS_DefineProperty(jcx, uo, name, vv, NULL, setter, attr);
}				/* loc_def_set */

/* Like the above, but using an integer, this is for port only. */
static void
loc_def_set_n(const char *name, int port,
   JSBool(*setter) (JSContext *, JSObject *, jsval, jsval *), jsuint attr)
{
    JSBool found;
    jsval vv = INT_TO_JSVAL(port);
    JS_HasProperty(jcx, uo, name, &found);
    if(found)
	JS_SetProperty(jcx, uo, name, &vv);
    else
	JS_DefineProperty(jcx, uo, name, vv, NULL, setter, attr);
}				/* loc_def_set_n */

static void
loc_def_set_part(const char *name, const char *s, int n,
   JSBool(*setter) (JSContext *, JSObject *, jsval, jsval *), jsuint attr)
{
    JSBool found;
    jsval vv;
    if(s)
	vv = STRING_TO_JSVAL(JS_NewStringCopyN(jcx, s, n));
    else
	vv = JS_GetEmptyStringValue(jcx);
    JS_HasProperty(jcx, uo, name, &found);
    if(found)
	JS_SetProperty(jcx, uo, name, &vv);
    else
	JS_DefineProperty(jcx, uo, name, vv, NULL, setter, attr);
}				/* loc_def_set_part */

static JSBool
setter_loc(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
    const char *s = stringize(*vp);
    if(!s) {
	JS_ReportError(jcx,
	   "window.location is assigned something that I don't understand");
    } else {
	char *t;
/* I have to copy the string, just so I can run unpercent */
	char *ss = cloneString(s);
	unpercentURL(ss);
	t = resolveURL(cw->fileName, ss);
	nzFree(ss);
/* This call frees t, or takes it over, so you should not free it here. */
	debugPrint(3, "window.location reset to %s", t);
	gotoLocation(t, 0, true);
    }
/* Return false to stop javascript. */
/* After all, we're trying to move to a new web page. */
    return JS_FALSE;
}				/* setter_loc */

static JSBool
setter_loc_href(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
    const char *url = 0;
    if(setter_suspend)
	return JS_TRUE;
    url = stringize(*vp);
    if(!url)
	return JS_TRUE;
    uo = obj;
    url_initialize(url, false, true);
    uo_href = cloneString(url);
    return isWinLoc();
}				/* setter_loc_href */

static JSBool
setter_loc_hash(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
    const char *e;
    if(setter_suspend)
	return JS_TRUE;
    e = stringize(*vp);
    uo = obj;
    build_url(5, e);
    return isWinLoc();
}				/* setter_loc_hash */

static JSBool
setter_loc_search(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
    const char *e;
    if(setter_suspend)
	return JS_TRUE;
    e = stringize(*vp);
    uo = obj;
    build_url(4, e);
    return isWinLoc();
}				/* setter_loc_search */

static JSBool
setter_loc_prot(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
    const char *e;
    if(setter_suspend)
	return JS_TRUE;
    e = stringize(*vp);
    uo = obj;
    build_url(1, e);
    return isWinLoc();
}				/* setter_loc_prot */

static JSBool
setter_loc_pathname(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
    const char *e;
    if(setter_suspend)
	return JS_TRUE;
    e = stringize(*vp);
    uo = obj;
    build_url(3, e);
    return isWinLoc();
}				/* setter_loc_pathname */

static JSBool
setter_loc_hostname(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
    const char *e;
    if(setter_suspend)
	return JS_TRUE;
    e = stringize(*vp);
    uo = obj;
    build_host(1, e, 0);
    build_url(0, 0);
    return isWinLoc();
}				/* setter_loc_hostname */

static JSBool
setter_loc_port(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
    int port;
    if(setter_suspend)
	return JS_TRUE;
    port = JSVAL_TO_INT(*vp);
    uo = obj;
    build_host(2, 0, port);
    build_url(0, 0);
    return isWinLoc();
}				/* setter_loc_port */

static JSBool
setter_loc_host(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
    const char *e, *s;
    int n;
    jsval v;
    if(setter_suspend)
	return JS_TRUE;
    e = stringize(*vp);
    uo = obj;
    build_url(2, e);
/* and we have to update hostname and port */
    setter_suspend = true;
    s = strchr(e, ':');
    if(s)
	n = s - e;
    else
	n = strlen(e);
    v = STRING_TO_JSVAL(JS_NewStringCopyN(jcx, e, n));
    JS_SetProperty(jcx, uo, "hostname", &v);
    if(s) {
	v = INT_TO_JSVAL(atoi(s + 1));
	JS_SetProperty(jcx, uo, "port", &v);
    }
    setter_suspend = false;
    return isWinLoc();
}				/* setter_loc_pathname */

static void
url_initialize(const char *url, bool readonly, bool exclude_href)
{
    int n, port;
    const char *data;
    const char *s;
    const char *pl;
    jsuint attr = JSPROP_ENUMERATE | JSPROP_PERMANENT;
    if(readonly)
	attr |= JSPROP_READONLY;

    setter_suspend = true;

/* Store the url in location.href */
    if(!exclude_href) {
	loc_def_set("href", url, setter_loc_href, attr);
    }

/* Now make a property for each piece of the url. */
    if(s = getProtURL(url)) {
	sprintf(urlbuffer, "%s:", s);
	if(strlen(urlbuffer) >= sizeof (urlbuffer))
	    errorPrint("@protocol: is too long");
	s = urlbuffer;
    }
    loc_def_set("protocol", s, setter_loc_prot, attr);

    data = getDataURL(url);
    s = 0;
    if(data)
	s = strchr(data, '#');
    loc_def_set("hash", s, setter_loc_hash, attr);

    s = getHostURL(url);
    if(s && !*s)
	s = 0;
    loc_def_set("hostname", s, setter_loc_hostname, attr);

    getPortLocURL(url, &pl, &port);
    loc_def_set_n("port", port, setter_loc_port, attr);

    if(s) {			/* this was hostname */
	strcpy(urlbuffer, s);
	if(pl)
	    sprintf(urlbuffer + strlen(urlbuffer), ":%d", port);
	if(strlen(urlbuffer) >= sizeof (urlbuffer))
	    errorPrint("@hostname:port is too long");
	s = urlbuffer;
    }
    loc_def_set("host", s, setter_loc_host, attr);

    s = 0;
    n = 0;
    getDirURL(url, &s, &pl);
    if(s) {
	pl = strpbrk(s, "?\1#");
	n = pl ? pl - s : strlen(s);
	if(!n)
	    s = "/", n = 1;
    }
    loc_def_set_part("pathname", s, n, setter_loc_pathname, attr);

    s = 0;
    if(data && (s = strpbrk(data, "?\1")) &&
       (!(pl = strchr(data, '#')) || pl > s)) {
	if(pl)
	    n = pl - s;
	else
	    n = strlen(s);
    }
    loc_def_set_part("search", s, n, setter_loc_search, attr);

    setter_suspend = false;
}				/* url_initialize */

static JSBool
url_ctor(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
    const char *url = 0;
    const char *s;
    if(argc && JSVAL_IS_STRING(*argv)) {
	s = stringize(argv[0]);
	if(strlen(s))
	    url = s;
    }				/* string argument */
    uo = obj;
    url_initialize(url, false, false);
    return JS_TRUE;
}				/* url_ctor */

static JSFunctionSpec url_methods[] = {
    {"toString", loc_toString, 0, 0, 0},
    {0}
};

void
initLocationClass(void)
{
    JS_InitClass(jcx, jwin, 0, &url_class, url_ctor, 1,
       NULL, url_methods, NULL, NULL);
}				/* initLocationClass */


/*********************************************************************
If js changes the value of an input field in a form,
this fact has to make it back to the text you are reading, in edbrowse,
after js returns.
That requires a special setter function to pass the new value back to the text.
*********************************************************************/

static JSBool
setter_value(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
    const char *val;
    if(setter_suspend)
	return JS_TRUE;
    val = stringize(*vp);
    if(!val) {
	JS_ReportError(jcx,
	   "input.value is assigned something other than a string; this can cause problems when you submit the form.");
    } else {
	javaSetsTagVar(obj, val);
    }
    return JS_TRUE;
}				/* setter_value */

static JSBool
setter_checked(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
    JSBool b;
    if(setter_suspend)
	return JS_TRUE;
    b = JSVAL_TO_BOOLEAN(*vp);
    return JS_TRUE;
}				/* setter_checked */

static JSBool
setter_selected(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
    JSBool b;
    if(setter_suspend)
	return JS_TRUE;
    b = JSVAL_TO_BOOLEAN(*vp);
    return JS_TRUE;
}				/* setter_selected */

static JSBool
setter_selidx(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
    int n;
    if(setter_suspend)
	return JS_TRUE;
    n = JSVAL_TO_INT(*vp);
    return JS_TRUE;
}				/* setter_selidx */

static JSBool
getter_cookie(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
    int cook_l;
    char *cook = initString(&cook_l);
    const char *url = cw->fileName;
    bool secure = false;
    const char *proto;
    char *s;

    if(url) {
	proto = getProtURL(url);
	if(proto && stringEqualCI(proto, "https"))
	    secure = true;
	sendCookies(&cook, &cook_l, url, secure);
	if(memEqualCI(cook, "cookie: ", 8)) {	/* should often happen */
	    strcpy(cook, cook + 8);
	}
	while(s = strstr(cook, "; "))
	    strcpy(s + 1, s + 2);
	if(s = strstr(cook, "\r\n"))
	    *s = 0;
    }

    *vp = STRING_TO_JSVAL(JS_NewStringCopyZ(jcx, cook));
    nzFree(cook);
    return JS_TRUE;
}				/* getter_cookie */

static JSBool
setter_cookie(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
    const char *host = getHostURL(cw->fileName);
    if(!host) {
	JS_ReportError(jcx, "cannot set cookie, ill-defined domain");
    } else {
	const char *s = stringize(*vp);
	if(!receiveCookie(cw->fileName, s))
	    JS_ReportError(jcx, "unable to set cookie %s", s);
    }
    return JS_TRUE;
}				/* setter_cookie */

static JSBool
setter_domain(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
    const char *hostname = getHostURL(cw->fileName);
    const char *dom = 0;
    if(!hostname)
	goto out;		/* local file, don't care */
    dom = stringize(*vp);
    if(dom && strlen(dom) && domainSecurityCheck(hostname, dom))
	goto out;
    if(!dom)
	dom = EMPTYSTRING;
    JS_ReportError(jcx,
       "document.domain is being set to an insecure string <%s>", dom);
  out:
    return JS_TRUE;
}				/* setter_domain */


/*********************************************************************
Convenient set property routines that can be invoked from edbrowse,
requiring no knowledge of smjs.
*********************************************************************/

static JSBool(*my_getter) (JSContext *, JSObject *, jsval, jsval *);
static JSBool(*my_setter) (JSContext *, JSObject *, jsval, jsval *);

void
establish_property_string(void *jv, const char *name, const char *value,
   bool readonly)
{
    jsuint attr = JSPROP_ENUMERATE | JSPROP_PERMANENT;
    if(readonly)
	attr |= JSPROP_READONLY;
    JSObject *obj = jv;
    my_getter = my_setter = 0;
    if(stringEqual(name, "value"))
	my_setter = setter_value;
    if(stringEqual(name, "domain"))
	my_setter = setter_domain;
    if(stringEqual(name, "cookie")) {
	my_getter = getter_cookie;
	my_setter = setter_cookie;
    }
    JS_DefineProperty(jcx, obj, name,
       ((value && *value) ? STRING_TO_JSVAL(JS_NewStringCopyZ(jcx, value))
       : JS_GetEmptyStringValue(jcx)), my_getter, my_setter, attr);
}				/* establish_property_string */

void
establish_property_number(void *jv, const char *name, int value, bool readonly)
{
    jsuint attr = JSPROP_ENUMERATE | JSPROP_PERMANENT;
    if(readonly)
	attr |= JSPROP_READONLY;
    JSObject *obj = jv;
    my_setter = 0;
    if(stringEqual(name, "selectedIndex"))
	my_setter = setter_selidx;
    JS_DefineProperty(jcx, obj, name,
       INT_TO_JSVAL(value), NULL, my_setter, attr);
}				/* establish_property_number */

void
establish_property_bool(void *jv, const char *name, bool value, bool readonly)
{
    jsuint attr = JSPROP_ENUMERATE | JSPROP_PERMANENT;
    if(readonly)
	attr |= JSPROP_READONLY;
    JSObject *obj = jv;
    my_setter = 0;
    if(stringEqual(name, "checked"))
	my_setter = setter_checked;
    if(stringEqual(name, "selected"))
	my_setter = setter_selected;
    JS_DefineProperty(jcx, obj, name,
       (value ? JSVAL_TRUE : JSVAL_FALSE), NULL, my_setter, attr);
}				/* establish_property_bool */

void *
establish_property_array(void *jv, const char *name)
{
    JSObject *obj = jv;
    JSObject *a = JS_NewArrayObject(jcx, 0, NULL);
    establish_property_object(obj, name, a);
    return a;
}				/* establish_property_array */

void
establish_property_object(void *parent, const char *name, void *child)
{
    JS_DefineProperty(jcx, parent, name,
       OBJECT_TO_JSVAL(((JSObject *) child)), 0, 0, PROP_FIXED);
}				/* establish_property_object */

void
establish_property_url(void *jv, const char *name,
   const char *url, bool readonly)
{
    JSObject *obj = jv;
    jsuint attr = JSPROP_ENUMERATE | JSPROP_PERMANENT;
    if(readonly)
	attr |= JSPROP_READONLY;

/* window.location, and document.location, has a special setter */
    my_setter = 0;
    if(stringEqual(name, "location"))
	my_setter = setter_loc;
    uo = JS_NewObject(jcx, &url_class, NULL, obj);
    JS_DefineProperty(jcx, obj, name,
       OBJECT_TO_JSVAL(uo), NULL, my_setter, attr);
    if(!url)
	url = EMPTYSTRING;
    url_initialize(url, readonly, false);
    if(my_setter == setter_loc) {
	if(obj == jwin)
	    jwloc = uo;
	else
	    jdloc = uo;
	JS_DefineFunction(jcx, uo, "reload", loc_reload, 0, PROP_FIXED);
	JS_DefineFunction(jcx, uo, "replace", loc_replace, 1, PROP_FIXED);
    }				/* location object */
}				/* establish_property_url */

void
set_property_string(void *jv, const char *name, const char *value)
{
    JSObject *obj = jv;
    jsval vv;
    setter_suspend = true;
    vv = ((value && *value) ? STRING_TO_JSVAL(JS_NewStringCopyZ(jcx, value))
       : JS_GetEmptyStringValue(jcx));
    JS_SetProperty(jcx, obj, name, &vv);
    setter_suspend = false;
}				/* set_property_string */

void
set_property_number(void *jv, const char *name, int value)
{
    JSObject *obj = jv;
    jsval vv;
    setter_suspend = true;
    vv = INT_TO_JSVAL(value);
    JS_SetProperty(jcx, obj, name, &vv);
    setter_suspend = false;
}				/* set_property_number */

void
set_property_bool(void *jv, const char *name, int value)
{
    JSObject *obj = jv;
    jsval vv;
    setter_suspend = true;
    vv = (value ? JSVAL_TRUE : JSVAL_FALSE);
    JS_SetProperty(jcx, obj, name, &vv);
    setter_suspend = false;
}				/* set_property_bool */

/* These get routines assume the property exists, and of the right type. */
const char *
get_property_url(void *jv, bool doaction)
{
    JSObject *obj = jv;
    JSObject *lo;		/* location object */
    jsval v;
    const char *s;
    JSBool found = false;
    if(!obj)
	return 0;
    if(!doaction) {
	JS_HasProperty(jcx, obj, "href", &found);
	if(found)
	    JS_GetProperty(jcx, obj, "href", &v);
	if(!found) {
	    JS_HasProperty(jcx, obj, "src", &found);
	    if(found)
		JS_GetProperty(jcx, obj, "src", &v);
	}
    } else {
	JS_HasProperty(jcx, obj, "action", &found);
	if(found)
	    JS_GetProperty(jcx, obj, "action", &v);
    }
    if(!found)
	return 0;
    if(!JSVAL_IS_STRING(v)) {
	if(!JSVAL_IS_OBJECT(v)) {
	  badobj:
	    JS_ReportError(jcx,
	       "url object is assigned something that I don't understand; I will not be able to fetch the next web page.");
	    return 0;
	}
	lo = JSVAL_TO_OBJECT(v);
	if(!JS_InstanceOf(jcx, lo, &url_class, emptyArgs))
	    goto badobj;
	JS_GetProperty(jcx, lo, "href", &v);
    }
    s = stringize(v);
    return s;
}				/* get_property_url */

const char *
get_property_string(void *jv, const char *name)
{
    JSObject *obj = jv;
    jsval v;
    if(!obj)
	return 0;
    JS_GetProperty(jcx, obj, name, &v);
    return stringize(v);
}				/* get_property_string */

bool
get_property_bool(void *jv, const char *name)
{
    JSObject *obj = jv;
    jsval v;
    if(!obj)
	return false;
    JS_GetProperty(jcx, obj, name, &v);
    return JSVAL_TO_BOOLEAN(v);
}				/* get_property_bool */


/*********************************************************************
Manage the array of options under an html select.
This will explode into a lot of code, if we ever implement
dynamic option lists under js control.
*********************************************************************/

static JSClass option_class = {
    "Option",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};
void *
establish_js_option(void *ev, int idx)
{
    JSObject *so = ev;		/* select object */
    jsval vv;
    JSObject *oa;		/* option array */
    JSObject *oo;		/* option object */
    JS_GetProperty(jcx, so, "options", &vv);
    oa = JSVAL_TO_OBJECT(vv);
    oo = JS_NewObject(jcx, &option_class, NULL, so);
    vv = OBJECT_TO_JSVAL(oo);
    JS_DefineElement(jcx, oa, idx, vv, NULL, NULL, JSPROP_ENUMERATE);
/* option.form = select.form */
    JS_GetProperty(jcx, so, "form", &vv);
    JS_SetProperty(jcx, oo, "form", &vv);
    return oo;
}				/* establish_js_option */


/*********************************************************************
Compile and call event handlers.
*********************************************************************/

bool
handlerGo(void *obj, const char *name)
{
    jsval v, rval;
    bool rc;
    JSObject *fo;		/* function object */
    JSFunction *f;
    JSBool found;
    JS_HasProperty(jcx, obj, name, &found);
    if(!found)
	return false;
    JS_GetProperty(jcx, obj, name, &v);
    if(!JSVAL_IS_OBJECT(v))
	return false;
    fo = JSVAL_TO_OBJECT(v);
    if(!JS_ObjectIsFunction(jcx, fo))
	return false;
    f = JS_ValueToFunction(jcx, v);
    JS_CallFunction(jcx, obj, f, 0, emptyArgs, &rval);
    rc = true;
    if(JSVAL_IS_BOOLEAN(rval))
	rc = JSVAL_TO_BOOLEAN(rval);
    JS_GC(jcx);
    return rc;
}				/* handlerGo */

void
handlerSet(void *ev, const char *name, const char *code)
{
    JSObject *obj = ev;
    char *newcode;
    JSBool found;
    if(!obj)
	return;
    newcode = allocMem(strlen(code) + 60);
    strcpy(newcode, "with(document) { ");
    JS_HasProperty(jcx, obj, "form", &found);
    if(found)
	strcat(newcode, "with(this.form) { ");
    strcat(newcode, code);
    if(found)
	strcat(newcode, " }");
    strcat(newcode, " }");
    JS_CompileFunction(jcx, obj, name, 0, emptyParms,	/* no named parameters */
       newcode, strlen(newcode), name, 1);
    nzFree(newcode);
}				/* handlerSet */

void
link_onunload_onclick(void *jv)
{
    JSObject *obj = jv;
    jsval v;
    JS_GetProperty(jcx, obj, "onunload", &v);
    JS_DefineProperty(jcx, obj, "onclick", v, 0, 0, PROP_FIXED);
}				/* link_onunload_onclick */

bool
handlerPresent(void *ev, const char *name)
{
    JSObject *obj = ev;
    JSBool found = JS_FALSE;
    if(!obj)
	return false;
    JS_HasProperty(jcx, obj, name, &found);
    return found;
}				/* handlerPresent */
