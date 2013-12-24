/* jsloc.c
* Javascript support, the URL class and the location object.
 * The cookie string, and other things with get/set side effects.
* Copyright (c) Karl Dahlke, 2008
* This file is part of the edbrowse project, released under GPL.
*/

#include "eb.h"

#include "js.h"

/* jsprf.h is not publically visible on some systems,
so I can't #include it here.
Instead, I'll declare the needed prototype myself, and hope it is consistent
with whatever smjs you are using. */
extern
JS_PUBLIC_API(char *)
JS_smprintf(const char *fmt, ...);

#define PROP_FIXED (JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT)


/*********************************************************************
The URL class, and the associated window.location object, is tricky,
with lots of interacting properties and setter functions.
*********************************************************************/

static const char *emptyParms[] = { 0 };
static jsval emptyArgs[] = { 0 };

static void
  url_initialize(const char *url, eb_bool readonly, eb_bool exclude_href);

const char *
stringize(jsval v)
{
    static char buf[24];
    static char *dynamic;
    int n;
    jsdouble d;
    if(JSVAL_IS_STRING(v)) {
	if(dynamic)
	    JS_free(jcx, dynamic);
	dynamic = our_JSEncodeString(JSVAL_TO_STRING(v));
	return dynamic;
    }
    if(JSVAL_IS_INT(v)) {
	n = JSVAL_TO_INT(v);
	sprintf(buf, "%d", n);
	return buf;
    }
    if(JSVAL_IS_DOUBLE(v)) {
	d = JSVAL_TO_DOUBLE(v);
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
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

/* To builld path names, host names, etc. */
static char urlbuffer[512];
static JSObject *uo;		/* the url object */
static char *uo_href;
static eb_bool setter_suspend;

/* Are we modifying window.location? */
/*Return false if we are, because that will put a stop to javascript. */
static eb_bool
isWinLoc(void)
{
    if(uo != jwloc && uo != jdloc) {
	nzFree(uo_href);
	uo_href = 0;
	return eb_true;
    }
/* This call frees t, or takes it over, so you should not free it here. */
    gotoLocation(uo_href, (allowRedirection ? 0 : 99), eb_false);
    uo_href = 0;
    return eb_false;
}				/* isWinLoc */

/* Converting to a string just pulls out the href property */
static JSBool
loc_toString(JSContext * cx, uintN argc, jsval * vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    jsval rval = JS_RVAL(cx, vp);
    JS_GetProperty(jcx, obj, "href", &rval);
    JS_SET_RVAL(cx, vp, rval);
    return JS_TRUE;
}				/* loc_toString */

static JSBool
loc_reload(JSContext * cx, uintN argc, jsval * vp)
{
    const char *s = cw->firstURL;
    if(s && isURL(s))
	gotoLocation(cloneString(s), (allowRedirection ? 0 : 99), eb_true);
    else
	JS_ReportError(jcx, "location.reload() cannot find a url to refresh");
    return JS_FALSE;
}				/* loc_reload */

static JSBool
loc_replace(JSContext * cx, uintN argc, jsval * vp)
{
    const char *s;
    char *ss, *t;
    jsval *argv = JS_ARGV(cx, vp);
    if(argc > 0 && JSVAL_IS_STRING(argv[0])) {
	s = stringize(argv[0]);
/* I have to copy the string, just so I can run unpercent */
	ss = cloneString(s);
	unpercentURL(ss);
	t = resolveURL(cw->fileName, ss);
	nzFree(ss);
/* This call frees t, or takes it over, so you should not free it here. */
	gotoLocation(t, (allowRedirection ? 0 : 99), eb_true);
	return JS_FALSE;
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
    setter_suspend = eb_true;
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
    v = STRING_TO_JSVAL(our_JS_NewStringCopyZ(jcx, new_url));
    JS_SetProperty(jcx, uo, "href", &v);
/* I want control over this string */
    uo_href = cloneString(new_url);
    JS_smprintf_free(new_url);
    setter_suspend = eb_false;
}				/* build_url */

/* Rebuild host, because hostname or port changed. */
static void
build_host(int exception, const char *hostname, int port)
{
    jsval v;
    const char *oldhost;
    setter_suspend = eb_true;
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
	i_printfExit(MSG_PortTooLong);
    v = STRING_TO_JSVAL(our_JS_NewStringCopyZ(jcx, urlbuffer));
    JS_SetProperty(jcx, uo, "host", &v);
    setter_suspend = eb_false;
}				/* build_host */

/* define or set a local property */
static void
loc_def_set(const char *name, const char *s,
   JSBool(*setter) (JSContext *, JSObject *, jsid, JSBool, jsval *),
   jsuint attr)
{
    JSBool found;
    jsval vv;
    if(s)
	vv = STRING_TO_JSVAL(our_JS_NewStringCopyZ(jcx, s));
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
   JSBool(*setter) (JSContext *, JSObject *, jsid, JSBool, jsval *),
   jsuint attr)
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
   JSBool(*setter) (JSContext *, JSObject *, jsid, JSBool, jsval *),
   jsuint attr)
{
    JSBool found;
    jsval vv;
    if(s)
	vv = STRING_TO_JSVAL(our_JS_NewStringCopyN(jcx, s, n));
    else
	vv = JS_GetEmptyStringValue(jcx);
    JS_HasProperty(jcx, uo, name, &found);
    if(found)
	JS_SetProperty(jcx, uo, name, &vv);
    else
	JS_DefineProperty(jcx, uo, name, vv, NULL, setter, attr);
}				/* loc_def_set_part */

static JSBool
setter_loc(JSContext * cx, JSObject * obj, jsid id, JSBool strict, jsval * vp)
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
	gotoLocation(t, (allowRedirection ? 0 : 99), eb_false);
    }
/* Return false to stop javascript. */
/* After all, we're trying to move to a new web page. */
    return JS_FALSE;
}				/* setter_loc */

static JSBool
setter_loc_href(JSContext * cx, JSObject * obj, jsid id, JSBool strict,
   jsval * vp)
{
    const char *url = 0;
    if(setter_suspend)
	return JS_TRUE;
    url = stringize(*vp);
    if(!url)
	return JS_TRUE;
    uo = obj;
    url_initialize(url, eb_false, eb_true);
    uo_href = cloneString(url);
    if(uo == jwloc || uo == jdloc) {
	char *t;
	unpercentURL(uo_href);
	t = resolveURL(cw->fileName, uo_href);
	nzFree(uo_href);
	uo_href = t;
    }
    return isWinLoc();
}				/* setter_loc_href */

static JSBool
setter_loc_hash(JSContext * cx, JSObject * obj, jsid id, JSBool strict,
   jsval * vp)
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
setter_loc_search(JSContext * cx, JSObject * obj, jsid id, JSBool strict,
   jsval * vp)
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
setter_loc_prot(JSContext * cx, JSObject * obj, jsid id, JSBool strict,
   jsval * vp)
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
setter_loc_pathname(JSContext * cx, JSObject * obj, jsid id, JSBool strict,
   jsval * vp)
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
setter_loc_hostname(JSContext * cx, JSObject * obj, jsid id, JSBool strict,
   jsval * vp)
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
setter_loc_port(JSContext * cx, JSObject * obj, jsid id, JSBool strict,
   jsval * vp)
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
setter_loc_host(JSContext * cx, JSObject * obj, jsid id, JSBool strict,
   jsval * vp)
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
    setter_suspend = eb_true;
    s = strchr(e, ':');
    if(s)
	n = s - e;
    else
	n = strlen(e);
    v = STRING_TO_JSVAL(our_JS_NewStringCopyN(jcx, e, n));
    JS_SetProperty(jcx, uo, "hostname", &v);
    if(s) {
	v = INT_TO_JSVAL(atoi(s + 1));
	JS_SetProperty(jcx, uo, "port", &v);
    }
    setter_suspend = eb_false;
    return isWinLoc();
}				/* setter_loc_pathname */

static void
url_initialize(const char *url, eb_bool readonly, eb_bool exclude_href)
{
    int n, port;
    const char *data;
    const char *s;
    const char *pl;
    jsuint attr = JSPROP_ENUMERATE | JSPROP_PERMANENT;
    if(readonly)
	attr |= JSPROP_READONLY;

    setter_suspend = eb_true;

/* Store the url in location.href */
    if(!exclude_href) {
	loc_def_set("href", url, setter_loc_href, attr);
    }

/* Now make a property for each piece of the url. */
    if(s = getProtURL(url)) {
	sprintf(urlbuffer, "%s:", s);
	if(strlen(urlbuffer) >= sizeof (urlbuffer))
	    i_printfExit(MSG_ProtTooLong);
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
	    i_printfExit(MSG_PortTooLong);
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
    } else {
/* If we have foo.html#?bla, then ?bla is not the query.
 * We need to set s to NULL and n to 0, lest we feed invalid data to
 * spidermonkey. */
	s = NULL;
	n = 0;
    }

    loc_def_set_part("search", s, n, setter_loc_search, attr);

    setter_suspend = eb_false;
}				/* url_initialize */

static JSBool
url_ctor(JSContext * cx, uintN argc, jsval * vp)
{
    const char *url = NULL;
    const char *s;
    jsval *argv;
    JSObject *obj;
    obj = JS_THIS_OBJECT(cx, vp);
    argv = JS_ARGV(cx, vp);
    if(argc && JSVAL_IS_STRING(*argv)) {
	s = stringize(argv[0]);
	if(strlen(s))
	    url = s;
    }				/* string argument */
    uo = obj;
    url_initialize(url, eb_false, eb_false);
    return JS_TRUE;
}				/* url_ctor */

static JSFunctionSpec url_methods[] = {
    {"toString", loc_toString, 0, 0},
    {0}
};

void
initLocationClass(void)
{
    JS_InitClass(jcx, jwin, NULL, &url_class, url_ctor, 1,
       NULL, url_methods, NULL, NULL);
}				/* initLocationClass */


/*********************************************************************
If js changes the value of an input field in a form,
this fact has to make it back to the text you are reading, in edbrowse,
after js returns.
That requires a special setter function to pass the new value back to the text.
*********************************************************************/

static JSBool
setter_value(JSContext * cx, JSObject * obj, jsid id, JSBool strict, jsval * vp)
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
setter_checked(JSContext * cx, JSObject * obj, jsid id, JSBool strict,
   jsval * vp)
{
    JSBool b;
    if(setter_suspend)
	return JS_TRUE;
    b = JSVAL_TO_BOOLEAN(*vp);
    return JS_TRUE;
}				/* setter_checked */

static JSBool
setter_selected(JSContext * cx, JSObject * obj, jsid id, JSBool strict,
   jsval * vp)
{
    JSBool b;
    if(setter_suspend)
	return JS_TRUE;
    b = JSVAL_TO_BOOLEAN(*vp);
    return JS_TRUE;
}				/* setter_selected */

static JSBool
setter_selidx(JSContext * cx, JSObject * obj, jsid id, JSBool strict,
   jsval * vp)
{
    int n;
    if(setter_suspend)
	return JS_TRUE;
    n = JSVAL_TO_INT(*vp);
    return JS_TRUE;
}				/* setter_selidx */

static JSBool
getter_cookie(JSContext * cx, JSObject * obj, jsid id, jsval * vp)
{
    int cook_l;
    char *cook = initString(&cook_l);
    const char *url = cw->fileName;
    eb_bool secure = eb_false;
    const char *proto;
    char *s;

    if(url) {
	proto = getProtURL(url);
	if(proto && stringEqualCI(proto, "https"))
	    secure = eb_true;
	sendCookies(&cook, &cook_l, url, secure);
	if(memEqualCI(cook, "cookie: ", 8)) {	/* should often happen */
	    strmove(cook, cook + 8);
	}
	if(s = strstr(cook, "\r\n"))
	    *s = 0;
    }

    *vp = STRING_TO_JSVAL(our_JS_NewStringCopyZ(jcx, cook));
    nzFree(cook);
    return JS_TRUE;
}				/* getter_cookie */

static JSBool
setter_cookie(JSContext * cx, JSObject * obj, jsid id, JSBool strict,
   jsval * vp)
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
setter_domain(JSContext * cx, JSObject * obj, jsid id, JSBool strict,
   jsval * vp)
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

static JSBool(*my_getter) (JSContext *, JSObject *, jsid, jsval *);
static JSBool(*my_setter) (JSContext *, JSObject *, jsid, JSBool, jsval *);

void
establish_property_string(void *jv, const char *name, const char *value,
   eb_bool readonly)
{
    JSObject *obj = jv;
    jsuint attr = JSPROP_ENUMERATE | JSPROP_PERMANENT;
    jsval v;
    if(readonly)
	attr |= JSPROP_READONLY;
    my_getter = NULL;
    my_setter = NULL;
    if(stringEqual(name, "value"))
	my_setter = setter_value;
    if(stringEqual(name, "domain"))
	my_setter = setter_domain;
    if(stringEqual(name, "cookie")) {
	my_getter = getter_cookie;
	my_setter = setter_cookie;
    }
    if(value && *value)
	v = STRING_TO_JSVAL(our_JS_NewStringCopyZ(jcx, value));
    else {
	v = JS_GetEmptyStringValue(jcx);
    }
    JS_DefineProperty(jcx, obj, name, v, my_getter, my_setter, attr);
}				/* establish_property_string */

void
establish_property_number(void *jv, const char *name, int value,
   eb_bool readonly)
{
    JSObject *obj = jv;
    jsuint attr = JSPROP_ENUMERATE | JSPROP_PERMANENT;
    if(readonly)
	attr |= JSPROP_READONLY;
    my_setter = NULL;
    if(stringEqual(name, "selectedIndex"))
	my_setter = setter_selidx;
    JS_DefineProperty(jcx, obj, name,
       INT_TO_JSVAL(value), NULL, my_setter, attr);
}				/* establish_property_number */

void
establish_property_bool(void *jv, const char *name, eb_bool value, eb_bool readonly)
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
   const char *url, eb_bool readonly)
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
    url_initialize(url, readonly, eb_false);
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
    setter_suspend = eb_true;
    vv = ((value && *value) ? STRING_TO_JSVAL(our_JS_NewStringCopyZ(jcx, value))
       : JS_GetEmptyStringValue(jcx));
    JS_SetProperty(jcx, obj, name, &vv);
    setter_suspend = eb_false;
}				/* set_property_string */

void
set_property_number(void *jv, const char *name, int value)
{
    JSObject *obj = jv;
    jsval vv;
    setter_suspend = eb_true;
    vv = INT_TO_JSVAL(value);
    JS_SetProperty(jcx, obj, name, &vv);
    setter_suspend = eb_false;
}				/* set_property_number */

void
set_property_bool(void *jv, const char *name, int value)
{
    JSObject *obj = jv;
    jsval vv;
    setter_suspend = eb_true;
    vv = (value ? JSVAL_TRUE : JSVAL_FALSE);
    JS_SetProperty(jcx, obj, name, &vv);
    setter_suspend = eb_false;
}				/* set_property_bool */

/* These get routines assume the property exists, and of the right type. */
char *
get_property_url(void *jv, eb_bool doaction)
{
    JSObject *obj = jv;
    JSObject *lo;		/* location object */
    jsval v;
    const char *s;
    char *out_str = NULL;
    int out_str_l;
    JSBool found = eb_false;
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
	       "url object is assigned something that I don't understand; I may not be able to fetch the next web page.");
	    return 0;
	}
	lo = JSVAL_TO_OBJECT(v);
	JS_HasProperty(jcx, lo, "actioncrash", &found);
	if(found)
	    return 0;
	if(!JS_InstanceOf(jcx, lo, &url_class, emptyArgs))
	    goto badobj;
	JS_GetProperty(jcx, lo, "href", &v);
    }
    s = stringize(v);
    if(!JS_CStringsAreUTF8()) {
	out_str = cloneString(s);
    } else {
	if(cons_utf8) {
	    out_str = cloneString(s);
	} else {
	    utf2iso(s, strlen(s), &out_str, &out_str_l);
	}
    }
    return out_str;
}				/* get_property_url */

char *
get_property_string(void *jv, const char *name)
{
    JSObject *obj = jv;
    jsval v;
    const char *s = NULL;
    char *out_str = NULL;
    char *converted;
    int converted_l;
    if(!obj)
	return 0;
    JS_GetProperty(jcx, obj, name, &v);
    s = stringize(v);
    if(!JS_CStringsAreUTF8()) {
	out_str = cloneString(s);
    } else {
	if(cons_utf8) {
	    out_str = cloneString(s);
	} else {
	    utf2iso(s, strlen(s), &converted, &converted_l);
	    out_str = converted;
	}
    }
    return out_str;
}				/* get_property_string */

eb_bool
get_property_bool(void *jv, const char *name)
{
    JSObject *obj = jv;
    jsval v;
    if(!obj)
	return eb_false;
    JS_GetProperty(jcx, obj, name, &v);
    return JSVAL_TO_BOOLEAN(v);
}				/* get_property_bool */

char *
get_property_option(void *jv)
{
    JSObject *obj = jv;
    jsval v;
    JSObject *oa;		/* option array */
    JSObject *oo;		/* option object */
    int n;

    if(!obj)
	return 0;
    JS_GetProperty(jcx, obj, "selectedIndex", &v);
    n = JSVAL_TO_INT(v);
    if(n < 0)
	return 0;
    JS_GetProperty(jcx, obj, "options", &v);
    oa = JSVAL_TO_OBJECT(v);
    JS_GetElement(jcx, oa, n, &v);
    oo = JSVAL_TO_OBJECT(v);
    return get_property_string(oo, "value");
}				/* get_property_option */


/*********************************************************************
Manage the array of options under an html select.
This will explode into a lot of code, if we ever implement
dynamic option lists under js control.
*********************************************************************/

static JSClass option_class = {
    "Option",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
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

eb_bool
handlerGo(void *obj, const char *name)
{
    jsval rval;
    eb_bool rc;
    JSBool found;
    JS_HasProperty(jcx, obj, name, &found);
    if(!found)
	return eb_false;
    rc = JS_CallFunctionName(jcx, obj, name, 0, emptyArgs, &rval);
    if(rc && JSVAL_IS_BOOLEAN(rval))
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

eb_bool
handlerPresent(void *ev, const char *name)
{
    JSObject *obj = ev;
    JSBool found = JS_FALSE;
    if(!obj)
	return eb_false;
    JS_HasProperty(jcx, obj, name, &found);
    return found;
}				/* handlerPresent */
