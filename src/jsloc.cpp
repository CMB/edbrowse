/* jsloc.cpp
* Javascript support, the URL class and the location object.
 * The cookie string, and other things with get/set side effects.
* Copyright (c) Karl Dahlke, 2008
* This file is part of the edbrowse project, released under GPL.
*/

#include "eb.h"

#include "js.h"

/*********************************************************************
The URL class, and the associated window.location object, is tricky,
with lots of interacting properties and setter functions.
*********************************************************************/

static const char *emptyParms[] = { 0 };

// can we just have a generic empty value class
static jsval emptyArgs[] = { jsval() };

static void
url_initialize(const char *url, eb_bool readonly, eb_bool exclude_href);

const char *stringize(js::HandleValue v)
{
	static char buf[24];
	static char *dynamic;
	int n;
	double d;
	if (JSVAL_IS_STRING(v)) {
		if (dynamic)
			JS_free(cw->jss->jcx, dynamic);
		js::RootedString jstr(cw->jss->jcx, JSVAL_TO_STRING(v));
		dynamic = our_JSEncodeString(jstr);
		return dynamic;
	}
	if (JSVAL_IS_INT(v)) {
		n = JSVAL_TO_INT(v);
		sprintf(buf, "%d", n);
		return buf;
	}
	if (JSVAL_IS_DOUBLE(v)) {
		d = JSVAL_TO_DOUBLE(v);
		n = d;
		if (n == d)
			sprintf(buf, "%d", n);
		else
			sprintf(buf, "%lf", d);
		return buf;
	}
/* Sorry, I don't look for object.toString() */
	return 0;		/* failed */
}				/* stringize */

static JSClass url_class = {
	"URL",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub,
	JS_DeletePropertyStub,
	JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub,
	JS_ResolveStub,
	JS_ConvertStub,
};

/* To builld path names, host names, etc. */
static char urlbuffer[512];
static char *uo_href;
static eb_bool setter_suspend;

/* Are we modifying window.location? */
/*Return false if we are, because that will put a stop to javascript. */
static eb_bool isWinLoc(void)
{
SWITCH_COMPARTMENT(eb_false);
	if (cw->jss->uo != cw->jss->jwloc && cw->jss->uo != cw->jss->jdloc) {
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
static JSBool loc_toString(JSContext * cx, unsigned int argc, jsval * vp)
{
SWITCH_COMPARTMENT(JS_FALSE);
	JS::RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	js::RootedValue rval(cw->jss->jcx);
	JS_GetProperty(cw->jss->jcx, obj, "href", rval.address());
	args.rval().set(rval);
	return JS_TRUE;
}				/* loc_toString */

static JSBool loc_reload(JSContext * cx, unsigned int argc, jsval * vp)
{
SWITCH_COMPARTMENT(JS_FALSE);
	const char *s = cw->firstURL;
	if (s && isURL(s))
		gotoLocation(cloneString(s), (allowRedirection ? 0 : 99),
			     eb_true);
	else
		JS_ReportError(cw->jss->jcx,
			       "location.reload() cannot find a url to refresh");
	return JS_FALSE;
}				/* loc_reload */

static JSBool loc_replace(JSContext * cx, unsigned int argc, jsval * vp)
{
SWITCH_COMPARTMENT(JS_FALSE);
	const char *s;
	char *ss, *t;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	if (args.length() > 0 && JSVAL_IS_STRING(args[0])) {
		js::RootedValue url_v(cw->jss->jcx, args[0]);
		s = stringize(url_v);
/* I have to copy the string, just so I can run unpercent */
		ss = cloneString(s);
		unpercentURL(ss);
		t = resolveURL(cw->fileName, ss);
		nzFree(ss);
/* This call frees t, or takes it over, so you should not free it here. */
		gotoLocation(t, (allowRedirection ? 0 : 99), eb_true);
		return JS_FALSE;
	}
	JS_ReportError(cw->jss->jcx,
		       "argument to location.replace() does not look like a url");
	return JS_FALSE;
}				/* loc_replace */

/* Put a url together from its pieces, after something has changed. */
static void build_url(int exception, const char *e)
{
SWITCH_COMPARTMENT();
	JS::RootedObject uo(cw->jss->jcx, cw->jss->uo);
	js::RootedValue v(cw->jss->jcx);
	char *new_url;
	string tmp, url_str;
	static const char *const noslashes[] = {
		"mailto", "telnet", "javascript", 0
	};
	setter_suspend = eb_true;
/* I'm a little worried about the first one being freed while I'm
 * getting the next one.
 * I just don't know that much about the js heap. */
	if (exception == 1)
		url_str.assign(e);
	else {
		JS_GetProperty(cw->jss->jcx, uo, "protocol", v.address());
		url_str.assign(stringize(v));
	}
	if (stringInListCI(noslashes, url_str.c_str()) < 0)
		url_str += "//";
	if (exception == 2)
		url_str += string(e);
	else {
		JS_GetProperty(cw->jss->jcx, uo, "host", v.address());
		url_str += string(stringize(v));
	}
	string pathname;
	if (exception == 3) {
		pathname = string(e);
		url_str += pathname;
	} else {
		JS_GetProperty(cw->jss->jcx, uo, "pathname", v.address());
		pathname = string(stringize(v));
		url_str += pathname;
	}
	if (pathname[0] != '/')
		url_str += "/";
	if (exception == 4)
		url_str += string(e);
	else {
		JS_GetProperty(cw->jss->jcx, uo, "search", v.address());
		url_str += string(stringize(v));
	}
	if (exception == 5)
		url_str += string(e);
	else {
		JS_GetProperty(cw->jss->jcx, uo, "hash", v.address());
		url_str += string(stringize(v));
	}
	new_url = (char *)url_str.c_str();
	v = STRING_TO_JSVAL(our_JS_NewStringCopyZ(cw->jss->jcx, new_url));
	JS_SetProperty(cw->jss->jcx, uo, "href", v.address());
/* I want control over this string */
	uo_href = cloneString(new_url);
	setter_suspend = eb_false;
}				/* build_url */

/* Rebuild host, because hostname or port changed. */
static void build_host(int exception, const char *hostname, int port)
{
SWITCH_COMPARTMENT();
	js::RootedValue v(cw->jss->jcx);
	const char *oldhost;
	setter_suspend = eb_true;
	if (exception == 1) {
		JS_GetProperty(cw->jss->jcx, cw->jss->uo, "port", v.address());
		port = JSVAL_TO_INT(v);
	} else {
		JS_GetProperty(cw->jss->jcx, cw->jss->uo, "hostname",
			       v.address());
		hostname = stringize(v);
	}
	JS_GetProperty(cw->jss->jcx, cw->jss->uo, "host", v.address());
	oldhost = stringize(v);
	if (exception == 2 || strchr(oldhost, ':'))
		sprintf(urlbuffer, "%s:%d", hostname, port);
	else
		strcpy(urlbuffer, hostname);
	if (strlen(urlbuffer) >= sizeof(urlbuffer))
		i_printfExit(MSG_PortTooLong);
	v = STRING_TO_JSVAL(our_JS_NewStringCopyZ(cw->jss->jcx, urlbuffer));
	JS_SetProperty(cw->jss->jcx, cw->jss->uo, "host", v.address());
	setter_suspend = eb_false;
}				/* build_host */

/* define or set a local property */
static void
loc_def_set(const char *name, const char *s,
	    JSStrictPropertyOp setter, unsigned attr)
{
SWITCH_COMPARTMENT();
	JSBool found;
	js::RootedValue vv(cw->jss->jcx);
	if (s)
		vv = STRING_TO_JSVAL(our_JS_NewStringCopyZ(cw->jss->jcx, s));
	else
		vv = JS_GetEmptyStringValue(cw->jss->jcx);
	JS_HasProperty(cw->jss->jcx, cw->jss->uo, name, &found);
	if (found)
		JS_SetProperty(cw->jss->jcx, cw->jss->uo, name, vv.address());
	else
		JS_DefineProperty(cw->jss->jcx, cw->jss->uo, name, vv, NULL,
				  setter, attr);
}				/* loc_def_set */

/* Like the above, but using an integer, this is for port only. */
static void
loc_def_set_n(const char *name, int port,
	      JSStrictPropertyOp setter, unsigned attr)
{
SWITCH_COMPARTMENT();

	JSBool found;
	js::RootedValue vv(cw->jss->jcx, INT_TO_JSVAL(port));
	JS_HasProperty(cw->jss->jcx, cw->jss->uo, name, &found);
	if (found)
		JS_SetProperty(cw->jss->jcx, cw->jss->uo, name, vv.address());
	else
		JS_DefineProperty(cw->jss->jcx, cw->jss->uo, name, vv, NULL,
				  setter, attr);
}				/* loc_def_set_n */

static void
loc_def_set_part(const char *name, const char *s, int n,
		 JSStrictPropertyOp setter, unsigned attr)
{
SWITCH_COMPARTMENT();
	JSBool found;
	js::RootedValue vv(cw->jss->jcx);
	if (s)
		vv = STRING_TO_JSVAL(our_JS_NewStringCopyN(cw->jss->jcx, s, n));
	else
		vv = JS_GetEmptyStringValue(cw->jss->jcx);
	JS_HasProperty(cw->jss->jcx, cw->jss->uo, name, &found);
	if (found)
		JS_SetProperty(cw->jss->jcx, cw->jss->uo, name, vv.address());
	else
		JS_DefineProperty(cw->jss->jcx, cw->jss->uo, name, vv, NULL,
				  setter, attr);
}				/* loc_def_set_part */

static JSBool
setter_loc(JSContext * cx, JS::Handle < JSObject * >obj, JS::Handle < jsid > id,
	   JSBool strict, JS::MutableHandle < JS::Value > vp)
{
	const char *s = stringize(vp);
	if (!s) {
		JS_ReportError(cw->jss->jcx,
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
setter_loc_href(JSContext * cx, JS::Handle < JSObject * >obj,
		JS::Handle < jsid > id, JSBool strict,
		JS::MutableHandle < JS::Value > vp)
{
	const char *url = 0;
	if (setter_suspend)
		return JS_TRUE;
	url = stringize(vp);
	if (!url)
		return JS_TRUE;
	cw->jss->uo = obj;
	url_initialize(url, eb_false, eb_true);
	uo_href = cloneString(url);
	if (cw->jss->uo == cw->jss->jwloc || cw->jss->uo == cw->jss->jdloc) {
		char *t;
		unpercentURL(uo_href);
		t = resolveURL(cw->fileName, uo_href);
		nzFree(uo_href);
		uo_href = t;
	}
	return isWinLoc();
}				/* setter_loc_href */

static JSBool
setter_loc_hash(JSContext * cx, JS::Handle < JSObject * >obj,
		JS::Handle < jsid > id, JSBool strict,
		JS::MutableHandle < JS::Value > vp)
{
	const char *e;
	if (setter_suspend)
		return JS_TRUE;
	e = stringize(vp);
	cw->jss->uo = obj;
	build_url(5, e);
	return isWinLoc();
}				/* setter_loc_hash */

static JSBool
setter_loc_search(JSContext * cx, JS::Handle < JSObject * >obj,
		  JS::Handle < jsid > id, JSBool strict,
		  JS::MutableHandle < JS::Value > vp)
{
	const char *e;
	if (setter_suspend)
		return JS_TRUE;
	e = stringize(vp);
	cw->jss->uo = obj;
	build_url(4, e);
	return isWinLoc();
}				/* setter_loc_search */

static JSBool
setter_loc_prot(JSContext * cx, JS::Handle < JSObject * >obj,
		JS::Handle < jsid > id, JSBool strict,
		JS::MutableHandle < JS::Value > vp)
{
	const char *e;
	if (setter_suspend)
		return JS_TRUE;
	e = stringize(vp);
	cw->jss->uo = obj;
	build_url(1, e);
	return isWinLoc();
}				/* setter_loc_prot */

static JSBool
setter_loc_pathname(JSContext * cx, JS::Handle < JSObject * >obj,
		    JS::Handle < jsid > id, JSBool strict,
		    JS::MutableHandle < JS::Value > vp)
{
	const char *e;
	if (setter_suspend)
		return JS_TRUE;
	e = stringize(vp);
	cw->jss->uo = obj;
	build_url(3, e);
	return isWinLoc();
}				/* setter_loc_pathname */

static JSBool
setter_loc_hostname(JSContext * cx, JS::Handle < JSObject * >obj,
		    JS::Handle < jsid > id, JSBool strict,
		    JS::MutableHandle < JS::Value > vp)
{
	const char *e;
	if (setter_suspend)
		return JS_TRUE;
	e = stringize(vp);
	cw->jss->uo = obj;
	build_host(1, e, 0);
	build_url(0, 0);
	return isWinLoc();
}				/* setter_loc_hostname */

static JSBool
setter_loc_port(JSContext * cx, JS::Handle < JSObject * >obj,
		JS::Handle < jsid > id, JSBool strict,
		JS::MutableHandle < JS::Value > vp)
{
	int port;
	if (setter_suspend)
		return JS_TRUE;
	port = JSVAL_TO_INT(vp);
	cw->jss->uo = obj;
	build_host(2, 0, port);
	build_url(0, 0);
	return isWinLoc();
}				/* setter_loc_port */

static JSBool
setter_loc_host(JSContext * cx, JS::Handle < JSObject * >obj,
		JS::Handle < jsid > id, JSBool strict,
		JS::MutableHandle < JS::Value > vp)
{
	const char *e, *s;
	int n;
	js::RootedValue v(cw->jss->jcx);
	if (setter_suspend)
		return JS_TRUE;
	e = stringize(vp);
	cw->jss->uo = obj;
	build_url(2, e);
/* and we have to update hostname and port */
	setter_suspend = eb_true;
	s = strchr(e, ':');
	if (s)
		n = s - e;
	else
		n = strlen(e);
	v = STRING_TO_JSVAL(our_JS_NewStringCopyN(cw->jss->jcx, e, n));
	JS_SetProperty(cw->jss->jcx, cw->jss->uo, "hostname", v.address());
	if (s) {
		v = INT_TO_JSVAL(atoi(s + 1));
		JS_SetProperty(cw->jss->jcx, cw->jss->uo, "port", v.address());
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
	unsigned attr = JSPROP_ENUMERATE | JSPROP_PERMANENT;
	if (readonly)
		attr |= JSPROP_READONLY;

	setter_suspend = eb_true;

/* Store the url in location.href */
	if (!exclude_href) {
		loc_def_set("href", url, setter_loc_href, attr);
	}

/* Now make a property for each piece of the url. */
	if (s = getProtURL(url)) {
		sprintf(urlbuffer, "%s:", s);
		if (strlen(urlbuffer) >= sizeof(urlbuffer))
			i_printfExit(MSG_ProtTooLong);
		s = urlbuffer;
	}
	loc_def_set("protocol", s, setter_loc_prot, attr);

	data = getDataURL(url);
	s = 0;
	if (data)
		s = strchr(data, '#');
	loc_def_set("hash", s, setter_loc_hash, attr);

	s = getHostURL(url);
	if (s && !*s)
		s = 0;
	loc_def_set("hostname", s, setter_loc_hostname, attr);

	getPortLocURL(url, &pl, &port);
	loc_def_set_n("port", port, setter_loc_port, attr);

	if (s) {		/* this was hostname */
		strcpy(urlbuffer, s);
		if (pl)
			sprintf(urlbuffer + strlen(urlbuffer), ":%d", port);
		if (strlen(urlbuffer) >= sizeof(urlbuffer))
			i_printfExit(MSG_PortTooLong);
		s = urlbuffer;
	}
	loc_def_set("host", s, setter_loc_host, attr);

	s = 0;
	n = 0;
	getDirURL(url, &s, &pl);
	if (s) {
		pl = strpbrk(s, "?\1#");
		n = pl ? pl - s : strlen(s);
		if (!n)
			s = "/", n = 1;
	}
	loc_def_set_part("pathname", s, n, setter_loc_pathname, attr);

	s = 0;
	if (data && (s = strpbrk(data, "?\1")) &&
	    (!(pl = strchr(data, '#')) || pl > s)) {
		if (pl)
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

static JSBool url_ctor(JSContext * cx, unsigned int argc, jsval * vp)
{
	const char *url = NULL;
	const char *s;
	JS::RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	if (args.length() > 0 && JSVAL_IS_STRING(args[0])) {
		js::RootedValue url_v(cw->jss->jcx, args[0]);
		s = stringize(url_v);
		if (strlen(s))
			url = s;
	}			/* string argument */
	cw->jss->uo = obj;
	url_initialize(url, eb_false, eb_false);
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* url_ctor */

static JSFunctionSpec url_methods[] = {
	{"toString", loc_toString, 0, 0},
	{0}
};

eb_bool initLocationClass(void)
{
SWITCH_COMPARTMENT(eb_false);
	if (JS_InitClass(cw->jss->jcx, cw->jss->jwin, NULL, &url_class, url_ctor, 1,
		     NULL, url_methods, NULL, NULL) == NULL)
{
javaSessionFail();
return eb_false;
}
return eb_true;
}				/* initLocationClass */

/*********************************************************************
If js changes the value of an input field in a form,
this fact has to make it back to the text you are reading, in edbrowse,
after js returns.
That requires a special setter function to pass the new value back to the text.
*********************************************************************/

static JSBool
setter_value(JSContext * cx, JS::Handle < JSObject * >obj,
	     JS::Handle < jsid > id, JSBool strict,
	     JS::MutableHandle < JS::Value > vp)
{
	const char *val;
	if (setter_suspend)
		return JS_TRUE;
	val = stringize(vp);
	if (!val) {
		JS_ReportError(cw->jss->jcx,
			       "input.value is assigned something other than a string; this can cause problems when you submit the form.");
	} else {
		javaSetsTagVar(obj, val);
	}
	return JS_TRUE;
}				/* setter_value */

static JSBool
setter_checked(JSContext * cx, JS::Handle < JSObject * >obj,
	       JS::Handle < jsid > id, JSBool strict,
	       JS::MutableHandle < JS::Value > vp)
{
	JSBool b;
	if (setter_suspend)
		return JS_TRUE;
	b = JSVAL_TO_BOOLEAN(vp);
	return JS_TRUE;
}				/* setter_checked */

static JSBool
setter_selected(JSContext * cx, JS::Handle < JSObject * >obj,
		JS::Handle < jsid > id, JSBool strict,
		JS::MutableHandle < JS::Value > vp)
{
	JSBool b;
	if (setter_suspend)
		return JS_TRUE;
	b = JSVAL_TO_BOOLEAN(vp);
	return JS_TRUE;
}				/* setter_selected */

static JSBool
setter_selidx(JSContext * cx, JS::Handle < JSObject * >obj,
	      JS::Handle < jsid > id, JSBool strict,
	      JS::MutableHandle < JS::Value > vp)
{
	int n;
	if (setter_suspend)
		return JS_TRUE;
	n = JSVAL_TO_INT(vp);
	return JS_TRUE;
}				/* setter_selidx */

static JSBool
getter_cookie(JSContext * cx, JS::Handle < JSObject * >obj,
	      JS::Handle < jsid > id, JS::MutableHandle < jsval > vp)
{
	int cook_l;
	char *cook = initString(&cook_l);
	const char *url = cw->fileName;
	eb_bool secure = eb_false;
	const char *proto;
	char *s;

	if (url) {
		proto = getProtURL(url);
		if (proto && stringEqualCI(proto, "https"))
			secure = eb_true;
		sendCookies(&cook, &cook_l, url, secure);
		if (memEqualCI(cook, "cookie: ", 8)) {	/* should often happen */
			strmove(cook, cook + 8);
		}
		if (s = strstr(cook, "\r\n"))
			*s = 0;
	}

	vp.set(STRING_TO_JSVAL(our_JS_NewStringCopyZ(cw->jss->jcx, cook)));
	nzFree(cook);
	return JS_TRUE;
}				/* getter_cookie */

static JSBool
setter_cookie(JSContext * cx, JS::Handle < JSObject * >obj,
	      JS::Handle < jsid > id, JSBool strict,
	      JS::MutableHandle < JS::Value > vp)
{
	const char *host = getHostURL(cw->fileName);
	if (!host) {
		JS_ReportError(cw->jss->jcx,
			       "cannot set cookie, ill-defined domain");
	} else {
		const char *s = stringize(vp);
		if (!receiveCookie(cw->fileName, s))
			JS_ReportError(cw->jss->jcx, "unable to set cookie %s",
				       s);
	}
	return JS_TRUE;
}				/* setter_cookie */

static JSBool
setter_domain(JSContext * cx, JS::Handle < JSObject * >obj,
	      JS::Handle < jsid > id, JSBool strict,
	      JS::MutableHandle < JS::Value > vp)
{
	const char *hostname = getHostURL(cw->fileName);
	const char *dom = 0;
	if (!hostname)
		goto out;	/* local file, don't care */
	dom = stringize(vp);
	if (dom && strlen(dom) && domainSecurityCheck(hostname, dom))
		goto out;
	if (!dom)
		dom = EMPTYSTRING;
	JS_ReportError(cw->jss->jcx,
		       "document.domain is being set to an insecure string <%s>",
		       dom);
out:
	return JS_TRUE;
}				/* setter_domain */

/*********************************************************************
Convenient set property routines that can be invoked from edbrowse,
requiring no knowledge of smjs.
*********************************************************************/

static JSPropertyOp my_getter;
static JSStrictPropertyOp my_setter;

void
establish_property_string(JS::HandleObject jv, const char *name,
			  const char *value, eb_bool readonly)
{
	SWITCH_COMPARTMENT();
	JS::RootedObject obj(cw->jss->jcx, jv);
	unsigned attr = JSPROP_ENUMERATE | JSPROP_PERMANENT;
	JS::RootedValue v(cw->jss->jcx);
	if (readonly)
		attr |= JSPROP_READONLY;
	my_getter = NULL;
	my_setter = NULL;
	if (stringEqual(name, "value"))
		my_setter = setter_value;
	if (stringEqual(name, "domain"))
		my_setter = setter_domain;
	if (stringEqual(name, "cookie")) {
		my_getter = getter_cookie;
		my_setter = setter_cookie;
	}
	if (value && *value)
		v = STRING_TO_JSVAL(our_JS_NewStringCopyZ(cw->jss->jcx, value));
	else {
		v = JS_GetEmptyStringValue(cw->jss->jcx);
	}
	JS_DefineProperty(cw->jss->jcx, obj, name, v, my_getter, my_setter,
			  attr);
}				/* establish_property_string */

void
establish_property_number(JS::HandleObject jv, const char *name, int value,
			  eb_bool readonly)
{
	SWITCH_COMPARTMENT();
	JS::RootedObject obj(cw->jss->jcx, jv);
	unsigned attr = JSPROP_ENUMERATE | JSPROP_PERMANENT;
	if (readonly)
		attr |= JSPROP_READONLY;
	my_setter = NULL;
	if (stringEqual(name, "selectedIndex"))
		my_setter = setter_selidx;
	js::RootedValue value_jv(cw->jss->jcx, INT_TO_JSVAL(value));
	JS_DefineProperty(cw->jss->jcx, obj, name,
			  value_jv, NULL, my_setter, attr);
}				/* establish_property_number */

void
establish_property_bool(JS::HandleObject jv, const char *name, eb_bool value,
			eb_bool readonly)
{
	SWITCH_COMPARTMENT();
	unsigned attr = JSPROP_ENUMERATE | JSPROP_PERMANENT;
	if (readonly)
		attr |= JSPROP_READONLY;
	JS::RootedObject obj(cw->jss->jcx, jv);
	my_setter = 0;
	if (stringEqual(name, "checked"))
		my_setter = setter_checked;
	if (stringEqual(name, "selected"))
		my_setter = setter_selected;
	JS_DefineProperty(cw->jss->jcx, obj, name,
			  (value ? JSVAL_TRUE : JSVAL_FALSE), NULL, my_setter,
			  attr);
}				/* establish_property_bool */

JSObject *establish_property_array(JS::HandleObject jv, const char *name)
{
	SWITCH_COMPARTMENT(NULL);
	JS::RootedObject obj(cw->jss->jcx, jv);
	JS::RootedObject a(cw->jss->jcx,
			   JS_NewArrayObject(cw->jss->jcx, 0, NULL));
	establish_property_object(obj, name, a);
	return (JSObject *) a;
}				/* establish_property_array */

void
establish_property_object(JS::HandleObject parent, const char *name,
			  JS::HandleObject child)
{
	SWITCH_COMPARTMENT();
	js::RootedValue child_v(cw->jss->jcx, OBJECT_TO_JSVAL(child));
	JS_DefineProperty(cw->jss->jcx, parent, name,
			  child_v, 0, 0, PROP_FIXED);
}				/* establish_property_object */

void
establish_property_url(JS::HandleObject jv, const char *name,
		       const char *url, eb_bool readonly)
{
	SWITCH_COMPARTMENT();
	JS::RootedObject obj(cw->jss->jcx, jv);
	unsigned attr = JSPROP_ENUMERATE | JSPROP_PERMANENT;
	if (readonly)
		attr |= JSPROP_READONLY;

/* window.location, and document.location, has a special setter */
	my_setter = 0;
	if (stringEqual(name, "location"))
		my_setter = setter_loc;
	cw->jss->uo = JS_NewObject(cw->jss->jcx, &url_class, NULL, obj);
	js::RootedValue uo_v(cw->jss->jcx, OBJECT_TO_JSVAL(cw->jss->uo));
	JS_DefineProperty(cw->jss->jcx, obj, name, uo_v, NULL, my_setter, attr);
	if (!url)
		url = EMPTYSTRING;
	url_initialize(url, readonly, eb_false);
	if (my_setter == setter_loc) {
		if (obj == cw->jss->jwin)
			cw->jss->jwloc = cw->jss->uo;
		else
			cw->jss->jdloc = cw->jss->uo;
		JS_DefineFunction(cw->jss->jcx, cw->jss->uo, "reload",
				  loc_reload, 0, PROP_FIXED);
		JS_DefineFunction(cw->jss->jcx, cw->jss->uo, "replace",
				  loc_replace, 1, PROP_FIXED);
	}			/* location object */
}				/* establish_property_url */

void set_property_string(js::HandleObject jv, const char *name,
			 const char *value)
{
	SWITCH_COMPARTMENT();
	JS::RootedObject obj(cw->jss->jcx, jv);
	js::RootedValue vv(cw->jss->jcx);
	setter_suspend = eb_true;
	vv = ((value &&
	       *value) ? STRING_TO_JSVAL(our_JS_NewStringCopyZ(cw->jss->jcx,
							       value))
	      : JS_GetEmptyStringValue(cw->jss->jcx));
	JS_SetProperty(cw->jss->jcx, obj, name, vv.address());
	setter_suspend = eb_false;
}				/* set_property_string */

void set_global_property_string(const char *name, const char *value)
{
	set_property_string(cw->jss->jwin, name, value);
}				/* set_global_property_string */

void set_property_number(JS::HandleObject jv, const char *name, int value)
{
	SWITCH_COMPARTMENT();
	js::RootedObject obj(cw->jss->jcx, jv);
	js::RootedValue vv(cw->jss->jcx);
	setter_suspend = eb_true;
	vv = INT_TO_JSVAL(value);
	JS_SetProperty(cw->jss->jcx, obj, name, vv.address());
	setter_suspend = eb_false;
}				/* set_property_number */

void set_property_bool(JS::HandleObject jv, const char *name, int value)
{
	SWITCH_COMPARTMENT();
	js::RootedObject obj(cw->jss->jcx, jv);
	js::RootedValue vv(cw->jss->jcx);
	setter_suspend = eb_true;
	vv = (value ? JSVAL_TRUE : JSVAL_FALSE);
	JS_SetProperty(cw->jss->jcx, obj, name, vv.address());
	setter_suspend = eb_false;
}				/* set_property_bool */

/* These get routines assume the property exists, and of the right type. */
char *get_property_url(JS::HandleObject jv, eb_bool doaction)
{
	SWITCH_COMPARTMENT(NULL);
	js::RootedObject obj(cw->jss->jcx, jv);
	js::RootedObject lo(cw->jss->jcx);	/* location object */
	js::RootedValue v(cw->jss->jcx);
	const char *s;
	char *out_str = NULL;
	int out_str_l;
	JSBool found = eb_false;
	if (!obj)
		return 0;
	if (!doaction) {
		JS_HasProperty(cw->jss->jcx, obj, "href", &found);
		if (found)
			JS_GetProperty(cw->jss->jcx, obj, "href", v.address());
		if (!found) {
			JS_HasProperty(cw->jss->jcx, obj, "src", &found);
			if (found)
				JS_GetProperty(cw->jss->jcx, obj, "src",
					       v.address());
		}
	} else {
		JS_HasProperty(cw->jss->jcx, obj, "action", &found);
		if (found)
			JS_GetProperty(cw->jss->jcx, obj, "action",
				       v.address());
	}
	if (!found)
		return 0;
	if (!JSVAL_IS_STRING(v)) {
		if (!v.isObject()) {
badobj:
			JS_ReportError(cw->jss->jcx,
				       "url object is assigned something that I don't understand; I may not be able to fetch the next web page.");
			return 0;
		}
		lo = JSVAL_TO_OBJECT(v);
		JS_HasProperty(cw->jss->jcx, lo, "actioncrash", &found);
		if (found)
			return 0;
		if (!JS_InstanceOf(cw->jss->jcx, lo, &url_class, emptyArgs))
			goto badobj;
		JS_GetProperty(cw->jss->jcx, lo, "href", v.address());
	}
	s = stringize(v);
/* we assume that the console can handle whatever is in the string,
so no UTF8 check */
	out_str = cloneString(s);
	return out_str;
}				/* get_property_url */

char *get_property_string(JS::HandleObject jv, const char *name)
{
	SWITCH_COMPARTMENT(NULL);
	js::RootedObject obj(cw->jss->jcx, jv);
	js::RootedValue v(cw->jss->jcx);
	const char *s = NULL;
	char *out_str = NULL;
	if (!obj)
		return 0;
	JS_GetProperty(cw->jss->jcx, obj, name, v.address());
	s = stringize(v);
/* assume either the string is ascii or the console is UTF8 */
	out_str = cloneString(s);
	return out_str;
}				/* get_property_string */

eb_bool get_property_bool(JS::HandleObject jv, const char *name)
{
	SWITCH_COMPARTMENT(eb_false);
	js::RootedObject obj(cw->jss->jcx, jv);
	js::RootedValue v(cw->jss->jcx);
	if (!obj)
		return eb_false;
	JS_GetProperty(cw->jss->jcx, obj, name, v.address());
	return JSVAL_TO_BOOLEAN(v);
}				/* get_property_bool */

char *get_property_option(JS::HandleObject jv)
{
	SWITCH_COMPARTMENT(NULL);
	js::RootedObject obj(cw->jss->jcx, jv);
	JS::RootedValue v(cw->jss->jcx);
	JS::RootedObject oa(cw->jss->jcx, NULL);	/* option array */
	JS::RootedObject oo(cw->jss->jcx, NULL);	/* option object */
	int n;

	if (!obj)
		return 0;
	JS_GetProperty(cw->jss->jcx, obj, "selectedIndex", v.address());
	n = JSVAL_TO_INT(v);
	if (n < 0)
		return 0;
	JS_GetProperty(cw->jss->jcx, obj, "options", v.address());
	oa = JSVAL_TO_OBJECT(v);
	JS_GetElement(cw->jss->jcx, oa, n, v.address());
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
	JS_PropertyStub,
	JS_DeletePropertyStub,
	JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub,
	JS_ResolveStub,
	JS_ConvertStub,
};

JSObject *establish_js_option(JS::HandleObject ev, int idx)
{
	SWITCH_COMPARTMENT(NULL);
	js::RootedValue vv(cw->jss->jcx);
	js::RootedObject oa(cw->jss->jcx);	/* option array */
	js::RootedObject oo(cw->jss->jcx);	/* option object */
	JS_GetProperty(cw->jss->jcx, ev, "options", vv.address());
	oa = JSVAL_TO_OBJECT(vv);
	oo = JS_NewObject(cw->jss->jcx, &option_class, NULL, ev);
	vv = OBJECT_TO_JSVAL(oo);
	JS_DefineElement(cw->jss->jcx, oa, idx, vv, NULL, NULL,
			 JSPROP_ENUMERATE);
/* option.form = select.form */
	JS_GetProperty(cw->jss->jcx, ev, "form", vv.address());
	JS_SetProperty(cw->jss->jcx, oo, "form", vv.address());
	return oo;
}				/* establish_js_option */

/*********************************************************************
Compile and call event handlers.
*********************************************************************/

eb_bool handlerGo(JS::HandleObject obj, const char *name)
{
	SWITCH_COMPARTMENT(eb_false);
	js::RootedValue rval(cw->jss->jcx);
	eb_bool rc;
	JSBool found;
	JS_HasProperty(cw->jss->jcx, obj, name, &found);
	if (!found)
		return eb_false;
	rc = JS_CallFunctionName(cw->jss->jcx, obj, name, 0, emptyArgs,
				 rval.address());
	if (rc && JSVAL_IS_BOOLEAN(rval))
		rc = JSVAL_TO_BOOLEAN(rval);
	return rc;
}				/* handlerGo */

void handlerSet(JS::HandleObject ev, const char *name, const char *code)
{
	SWITCH_COMPARTMENT();
	char *newcode;
	JSBool found;
	newcode = (char *)allocMem(strlen(code) + 60);
	strcpy(newcode, "with(document) { ");
	JS_HasProperty(cw->jss->jcx, ev, "form", &found);
	if (found)
		strcat(newcode, "with(this.form) { ");
	strcat(newcode, code);
	if (found)
		strcat(newcode, " }");
	strcat(newcode, " }");
	JS_CompileFunction(cw->jss->jcx, ev, name, 0, emptyParms,	/* no named parameters */
			   newcode, strlen(newcode), name, 1);
	nzFree(newcode);
}				/* handlerSet */

void link_onunload_onclick(JS::HandleObject jv)
{
	SWITCH_COMPARTMENT();
	JS::RootedValue v(cw->jss->jcx);
	JS_GetProperty(cw->jss->jcx, jv, "onunload", v.address());
	JS_DefineProperty(cw->jss->jcx, jv, "onclick", v, 0, 0, PROP_FIXED);
}				/* link_onunload_onclick */

eb_bool handlerPresent(JS::HandleObject ev, const char *name)
{
	SWITCH_COMPARTMENT(eb_false);
	JSBool found = JS_FALSE;
	JS_HasProperty(cw->jss->jcx, ev, name, &found);
	return found;
}				/* handlerPresent */
