/********************************************************************* This is the back-end process for javascript.
This is the server, and edbrowse is the client.
We receive interprocess messages from edbrowse,
getting and setting properties for various DOM objects.

This is the mozilla version.
If you package this with the mozilla js libraries,
you will need to include the MPL, mozilla public license,
along with the GPL, general public license.

The interface between this process and edbrowse is defined in ebjs.h.
There should be no other local header files common to both.

Exit codes are as follows:
0 terminate normally, as directed by edbrowse
1. bad arguments
2 cannot read or write to edbrowse
3 messages are out of sync
4 cannot create javascript runtime environmet
5 cannot read from stdin or write to stdout
6 unexpected message command from edbrowse
7 unexpected property type from edbrowse
8 unexpected class name from edbrowse
9 only arrays of objects are supported at this time
90 this program was never executed
99 memory allocation error or heap corruption
*********************************************************************/

#include "ebjs.h"

#include <limits>
#include <iostream>
#include <string>

/* work around a bug where the standard UINT32_MAX isn't defined, I really hope this is correct */
#ifndef UINT32_MAX
#define UINT32_MAX std::numeric_limits<uint32_t>::max()
#endif

/* now we can include our jsapi */
#include <jsapi.h>
#include <jsfriendapi.h>

/* And some C header files. This program is a bit of a hybrid, C and C++ */
#include <string.h>
#include <ctype.h>
#include <malloc.h>
#include <unistd.h>
#include <memory.h>
#include <signal.h>

using namespace std;

/* Functions copied from stringfile.c */

#define stringEqual !strcmp

static const char emptyString[] = "";

static bool stringEqualCI(const char *s, const char *t)
{
	char c, d;
	while ((c = *s) && (d = *t)) {
		if (islower(c))
			c = toupper(c);
		if (islower(d))
			d = toupper(d);
		if (c != d)
			return false;
		++s, ++t;
	}
	if (*s)
		return false;
	if (*t)
		return false;
	return true;
}				/* stringEqualCI */

static bool memEqualCI(const char *s, const char *t, int len)
{
	char c, d;
	while (len--) {
		c = *s, d = *t;
		if (islower(c))
			c = toupper(c);
		if (islower(d))
			d = toupper(d);
		if (c != d)
			return false;
		++s, ++t;
	}
	return true;
}				/* memEqualCI */

static int stringInListCI(const char *const *list, const char *s)
{
	int i = 0;
	if (!s)
		return -1;
	while (*list) {
		if (stringEqualCI(s, *list))
			return i;
		++i;
		++list;
	}
	return -1;
}				/* stringInListCI */

static char *allocMem(size_t n)
{
	char *s;
	if (!n)
		return (char *)emptyString;
	if (!(s = (char *)malloc(n))) {
		cerr << "malloc failure in edbrowse-js, " << n << " bytes.\n";
		exit(99);
	}
	return s;
}				/* allocMem */

static void nzFree(const void *s)
{
	if (s && s != emptyString)
		free((void *)s);
}				/* nzFree */

static char *cloneString(const char *s)
{
	char *t;
	unsigned len;

	if (!s)
		return 0;
	if (!*s)
		return (char *)emptyString;
	len = strlen(s) + 1;
	t = allocMem(len);
	strcpy(t, s);
	return t;
}				/* cloneString */

static void skipWhite(const char **s)
{
	const char *t = *s;
	while (isspace(*t))
		++t;
	*s = t;
}				/* skipWhite */

static int stringIsNum(const char *s)
{
	int n;
	if (!isdigit(s[0]))
		return -1;
	n = strtol(s, (char **)&s, 10);
	if (*s)
		return -1;
	return n;
}				/* stringIsNum */

static bool stringIsFloat(const char *s, double *dp)
{
	const char *t;
	*dp = strtod(s, (char **)&t);
	if (*t)
		return false;	/* extra stuff at the end */
	return true;
}				/* stringIsFloat */

/* Functions copied from url.c */

static struct {
	const char *prot;
	int port;
	bool free_syntax;
	bool need_slashes;
	bool need_slash_after_host;
} protocols[] = {
	{
	"file", 0, true, true, false}, {
	"http", 80, false, true, true}, {
	"https", 443, false, true, true}, {
	"pop3", 110, false, true, true}, {
	"pop3s", 995, false, true, true}, {
	"smtp", 25, false, true, true}, {
	"submission", 587, false, true, true}, {
	"smtps", 465, false, true, true}, {
	"proxy", 3128, false, true, true}, {
	"ftp", 21, false, true, true}, {
	"sftp", 22, false, true, true}, {
	"ftps", 990, false, true, true}, {
	"tftp", 69, false, true, true}, {
	"rtsp", 554, false, true, true}, {
	"pnm", 7070, false, true, true}, {
	"finger", 79, false, true, true}, {
	"smb", 139, false, true, true}, {
	"mailto", 0, false, false, false}, {
	"telnet", 23, false, false, false}, {
	"tn3270", 0, false, false, false}, {
	"javascript", 0, true, false, false}, {
	"git", 0, false, false, false}, {
	"svn", 0, false, false, false}, {
	"gopher", 70, false, false, false}, {
	"magnet", 0, false, false, false}, {
	"irc", 0, false, false, false}, {
	NULL, 0}
};

static bool free_syntax;

static int protocolByName(const char *p, int l)
{
	int i;
	for (i = 0; protocols[i].prot; i++)
		if (memEqualCI(protocols[i].prot, p, l))
			return i;
	return -1;
}				/* protocolByName */

/* Decide if it looks like a web url. */
static bool httpDefault(const char *url)
{
	static const char *const domainSuffix[] = {
		"com", "biz", "info", "net", "org", "gov", "edu", "us", "uk",
		"au",
		"ca", "de", "jp", "nz", 0
	};
	int n, len;
	const char *s, *lastdot, *end = url + strcspn(url, "/?#\1");
	if (end - url > 7 && stringEqual(end - 7, ".browse"))
		end -= 7;
	s = strrchr(url, ':');
	if (s && s < end) {
		const char *colon = s;
		++s;
		while (isdigit(*s))
			++s;
		if (s == end)
			end = colon;
	}
/* need at least two embedded dots */
	n = 0;
	for (s = url + 1; s < end - 1; ++s)
		if (*s == '.' && s[-1] != '.' && s[1] != '.')
			++n, lastdot = s;
	if (n < 2)
		return false;
/* All digits, like an ip address, is ok. */
	if (n == 3) {
		for (s = url; s < end; ++s)
			if (!isdigit(*s) && *s != '.')
				break;
		if (s == end)
			return true;
	}
/* Look for standard domain suffix */
	++lastdot;
	len = end - lastdot;
	for (n = 0; domainSuffix[n]; ++n)
		if (memEqualCI(lastdot, domainSuffix[n], len)
		    && !domainSuffix[n][len])
			return true;
/* www.anything.xx is ok */
	if (len == 2 && memEqualCI(url, "www.", 4))
		return true;
	return false;
}				/* httpDefault */

static int parseURL(const char *url, const char **proto, int *prlen, const char **user, int *uslen, const char **pass, int *palen,	/* ftp protocol */
		    const char **host, int *holen,
		    const char **portloc, int *port,
		    const char **data, int *dalen, const char **post)
{
	const char *p, *q;
	int a;

	if (proto)
		*proto = NULL;
	if (prlen)
		*prlen = 0;
	if (user)
		*user = NULL;
	if (uslen)
		*uslen = 0;
	if (pass)
		*pass = NULL;
	if (palen)
		*palen = 0;
	if (host)
		*host = NULL;
	if (holen)
		*holen = 0;
	if (portloc)
		*portloc = 0;
	if (port)
		*port = 0;
	if (data)
		*data = NULL;
	if (dalen)
		*dalen = 0;
	if (post)
		*post = NULL;
	free_syntax = false;

	if (!url)
		return -1;

/* Find the leading protocol:// */
	a = -1;
	p = strchr(url, ':');
	if (p) {
/* You have to have something after the colon */
		q = p + 1;
		if (*q == '/')
			++q;
		if (*q == '/')
			++q;
		while (isspace(*q))
			++q;
		if (!*q)
			return false;
		a = protocolByName(url, p - url);
	}
	if (a >= 0) {
		if (proto)
			*proto = url;
		if (prlen)
			*prlen = p - url;
		if (p[1] != '/' || p[2] != '/') {
			if (protocols[a].need_slashes) {
				if (p[1] != '/') {
/* we should see a slash at this point */
					return -1;
				}
/* We got one out of two slashes, I'm going to call it good */
				++p;
			}
			p -= 2;
		}
		p += 3;
	} else {		/* nothing yet */
		if (p && p - url < 12 && p[1] == '/') {
			for (q = url; q < p; ++q)
				if (!isalpha(*q))
					break;
			if (q == p) {	/* some protocol we don't know */
				char qprot[12];
				memcpy(qprot, url, p - url);
				qprot[p - url] = 0;
				return -1;
			}
		}
		if (httpDefault(url)) {
			static const char http[] = "http://";
			if (proto)
				*proto = http;
			if (prlen)
				*prlen = 4;
			a = 1;
			p = url;
		}
	}

	if (a < 0)
		return false;

	if (free_syntax = protocols[a].free_syntax) {
		if (data)
			*data = p;
		if (dalen)
			*dalen = strlen(p);
		return true;
	}

	q = p + strcspn(p, "@?#/\1");
	if (*q == '@') {	/* user:password@host */
		const char *pp = strchr(p, ':');
		if (!pp || pp > q) {	/* no password */
			if (user)
				*user = p;
			if (uslen)
				*uslen = q - p;
		} else {
			if (user)
				*user = p;
			if (uslen)
				*uslen = pp - p;
			if (pass)
				*pass = pp + 1;
			if (palen)
				*palen = q - pp - 1;
		}
		p = q + 1;
	}

	q = p + strcspn(p, ":?#/\1");
	if (host)
		*host = p;
	if (holen)
		*holen = q - p;
	if (*q == ':') {	/* port specified */
		int n;
		const char *cc, *pp = q + strcspn(q, "/?#\1");
		if (pp > q + 1) {
			n = strtol(q + 1, (char **)&cc, 10);
			if (cc != pp || !isdigit(q[1])) {
/* impropter port number */
				return -1;
			}
			if (port)
				*port = n;
		}
		if (portloc)
			*portloc = q;
		q = pp;		/* up to the slash */
	} else {
		if (port)
			*port = protocols[a].port;
	}			/* colon or not */

/* Skip past /, but not ? or # */
	if (*q == '/')
		q++;
	p = q;

/* post data is handled separately */
	q = p + strcspn(p, "\1");
	if (data)
		*data = p;
	if (dalen)
		*dalen = q - p;
	if (post)
		*post = *q ? q + 1 : NULL;
	return true;
}				/* parseURL */

static bool isURL(const char *url)
{
	int j = parseURL(url, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	if (j < 0)
		return false;
	return j;
}				/* isURL */

/* Helper functions to return pieces of the URL.
 * Makes a copy, so you can have your 0 on the end.
 * Return 0 for an error, and "" if that piece is missing. */

static const char *getProtURL(const char *url)
{
	static char buf[12];
	int l;
	const char *s;
	int rc = parseURL(url, &s, &l, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	if (rc <= 0)
		return 0;
	memcpy(buf, s, l);
	buf[l] = 0;
	return buf;
}				/* getProtURL */

static char hostbuf[400];
static const char *getHostURL(const char *url)
{
	int l;
	const char *s;
	char *t;
	char c, d;
	int rc = parseURL(url, 0, 0, 0, 0, 0, 0, &s, &l, 0, 0, 0, 0, 0);
	if (rc <= 0)
		return 0;
	if (free_syntax)
		return 0;
	if (!s)
		return emptyString;
	if (l >= sizeof(hostbuf)) {
/* domain is too long, just give up */
/* This is old C code; could easily be handled with string in C++ */
		return 0;
	}
	memcpy(hostbuf, s, l);
	if (l && hostbuf[l - 1] == '.')
		--l;
	hostbuf[l] = 0;
/* domain names must be ascii, with no spaces */
	d = 0;
	for (s = t = hostbuf; (c = *s); ++s) {
		c &= 0x7f;
		if (c == ' ')
			continue;
		if (c == '.' && d == '.')
			continue;
		*t++ = d = c;
	}
	*t = 0;
	return hostbuf;
}				/* getHostURL */

static const char *getDataURL(const char *url)
{
	const char *s;
	int rc = parseURL(url, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, &s, 0, 0);
	if (rc <= 0)
		return 0;
	return s;
}				/* getDataURL */

static void getDirURL(const char *url, const char **start_p, const char **end_p)
{
	const char *dir = getDataURL(url);
	const char *end;
	static const char myslash[] = "/";
	if (!dir || dir == url)
		goto slash;
	if (free_syntax)
		goto slash;
	if (!strchr("#?\1", *dir)) {
		if (*--dir != '/') {
/* this should never happen */
			goto slash;
		}
	}
	end = strpbrk(dir, "#?\1");
	if (!end)
		end = dir + strlen(dir);
	while (end > dir && end[-1] != '/')
		--end;
	if (end > dir) {
		*start_p = dir;
		*end_p = end;
		return;
	}
slash:
	*start_p = myslash;
	*end_p = myslash + 1;
}				/* getDirURL */

static bool getPortLocURL(const char *url, const char **portloc, int *port)
{
	int rc = parseURL(url, 0, 0, 0, 0, 0, 0, 0, 0, portloc, port, 0, 0, 0);
	if (rc <= 0)
		return false;
	if (free_syntax)
		return false;
	return true;
}				/* getPortLocURL */

/* The software for this process begins now. */

static void usage(void)
{
	cerr << "Usage:  edbrowse-js pipe_in pipe_out jsHeapSize\n";
	exit(1);
}				/* usage */

/* arguments, as indicated by the above */
static int pipe_in, pipe_out, jsPool;

static void js_start(void);
static void readMessage(void);
static void processMessage(void);
static void createJavaContext(void);
static void writeHeader(void);

static JSContext *jcx;
static JSObject *winobj;	/* window object */
static JSObject *docobj;	/* document object */

static struct EJ_MSG head;
static string errorMessage;
static string effects;
static void endeffect(void)
{
	effects += "`~@}\n";
}

static char *membername;
static char *propval;
static enum ej_proptype proptype;
static char *runscript;

int main(int argc, char **argv)
{
	if (argc != 4)
		usage();

	pipe_in = stringIsNum(argv[1]);
	pipe_out = stringIsNum(argv[2]);
	jsPool = stringIsNum(argv[3]);
	if (pipe_in < 0 || pipe_out < 0 || jsPool < 0)
		usage();

	if (jsPool < 2)
		jsPool = 2;

	js_start();

/* edbrowse catches interrupt, this process ignores it. */
/* Use quit to terminate, or kill from another console. */
	signal(SIGINT, SIG_IGN);

	while (true) {
		readMessage();
		head.highstat = EJ_HIGH_OK;
		head.lowstat = EJ_LOW_OK;

		if (head.cmd == EJ_CMD_EXIT)
			exit(0);

		if (head.cmd == EJ_CMD_CREATE) {
/* this one is special */
			createJavaContext();
			if (!head.highstat) {
				head.jcx = jcx;
				head.winobj = winobj;
				head.docobj = docobj;
			}
			writeHeader();
			continue;
		}

		jcx = (JSContext *) head.jcx;
		winobj = (JSObject *) head.winobj;
		docobj = (JSObject *) head.docobj;

		if (head.cmd == EJ_CMD_DESTROY) {
/* don't enter the compartment of a context you want to destroy */
			JS_DestroyContext(jcx);
			writeHeader();
			continue;
		}

/* this function will enter the compartment */
		processMessage();
	}
}				/* main */

/* read from and write to edbrowse */
static void readFromEb(void *data_p, int n)
{
	int rc;
	if (n == 0)
		return;
	rc = read(pipe_in, data_p, n);
	if (rc == n)
		return;
/* Oops - can't read from the process any more */
	exit(2);
}				/* readFromEb */

static void writeToEb(const void *data_p, int n)
{
	int rc;
	if (n == 0)
		return;
	rc = write(pipe_out, data_p, n);
	if (rc == n)
		return;
/* Oops - can't write to the process any more */
	cerr << "js cannot communicate with edbrowse\n";
	exit(2);
}				/* writeToEb */

static void writeHeader(void)
{
	head.magic = EJ_MAGIC;
	head.side = effects.length();
	head.msglen = errorMessage.length();

	writeToEb(&head, sizeof(head));

/* send out the error message and side effects, if present. */
/* Edbrowse will expect these before any returned values. */
	if (head.side) {
		writeToEb(effects.c_str(), head.side);
		effects.clear();
	}

	if (head.msglen) {
		writeToEb(errorMessage.c_str(), head.msglen);
		errorMessage.clear();
	}

/* That's the header, you may still need to send a returned value */
}				/* writeHeader */

static char *readString(int n)
{
	char *s;
	if (!n)
		return 0;
	s = allocMem(n + 1);
	readFromEb(s, n);
	s[n] = 0;
	return s;
}				/* readString */

/* Read the entire message, so we can process it and move on,
 * without any sync errors. This means we must read the property or run script
 * or anything else passed along. */
static void readMessage(void)
{
	enum ej_cmd cmd;
	enum ej_proptype pt;

	readFromEb(&head, sizeof(head));

	if (head.magic != EJ_MAGIC) {
		cerr << "Messages between js and edbrowse are out of sync\n";
		exit(3);
	}

	cmd = head.cmd;
	pt = head.proptype;

	if (cmd == EJ_CMD_SCRIPT) {
		if (head.proplength)
			runscript = readString(head.proplength);
	}

	if (cmd == EJ_CMD_HASPROP ||
	    cmd == EJ_CMD_GETPROP ||
	    cmd == EJ_CMD_CALL ||
	    cmd == EJ_CMD_SETPROP || cmd == EJ_CMD_DELPROP) {
		if (head.n)
			membername = readString(head.n);
	}

	if (cmd == EJ_CMD_SETPROP || cmd == EJ_CMD_SETAREL) {
		proptype = head.proptype;
		if (head.proplength)
			propval = readString(head.proplength);
	}

/* and that's the whole message */
}				/* readMessage */

static JSRuntime *jrt;		/* our js runtime environment */
static const size_t gStackChunkSize = 8192;

static void js_start(void)
{
	jrt = JS_NewRuntime(jsPool * 1024L * 1024L, JS_NO_HELPER_THREADS);
	if (jrt)
		return;		/* ok */

	cerr << "Cannot create javascript runtime environment\n";
/* send a message to edbrowse, so it can disable javascript,
 * so we don't get this same error on every browse. */
	head.highstat = EJ_HIGH_PROC_FAIL;
	head.lowstat = EJ_LOW_RUNTIME;
	writeHeader();
	exit(4);
}				/* js_start */

static void misconfigure(void)
{
/* there may already be a larger error */
	if (head.highstat > EJ_HIGH_CX_FAIL)
		return;
	head.highstat = EJ_HIGH_CX_FAIL;
	head.lowstat = EJ_LOW_VARS;
}				/* misconfigure */

static void
my_ErrorReporter(JSContext * cx, const char *message, JSErrorReport * report)
{
	if (report && report->errorNumber == JSMSG_OUT_OF_MEMORY ||
	    message && strstr(message, "out of memory")) {
		head.highstat = EJ_HIGH_HEAP_FAIL;
		head.lowstat = EJ_LOW_MEMORY;
	} else if (errorMessage.empty() && head.highstat == EJ_HIGH_OK &&
		   message && *message) {
		if (report)
			head.lineno = report->lineno;
		errorMessage = message;
		head.highstat = EJ_HIGH_STMT_FAIL;
		head.lowstat = EJ_LOW_SYNTAX;
	}

	if (report)
		report->flags = 0;
}				/* my_ErrorReporter */

/* see if a rooted js object is window or document.
 * Note what we have to do to get back to the actual object pointer. */
static bool iswindoc(JS::HandleObject obj)
{
	JSObject *p = *obj.address();
	return (p == winobj || p == docobj);
}				/* iswindoc */

/* is a rooted object window.location or document.location? */
static bool iswindocloc(JS::HandleObject obj)
{
	JS::RootedObject w(jcx), p(jcx);
	js::RootedValue v(jcx);
/* try window.location first */
	w = winobj;
	if (JS_GetProperty(jcx, w, "location", v.address()) == JS_TRUE &&
	    v.isObject()) {
		p = JSVAL_TO_OBJECT(v);
		if (p == obj)
			return true;
	}
	w = docobj;
	if (JS_GetProperty(jcx, w, "location", v.address()) == JS_TRUE &&
	    v.isObject()) {
		p = JSVAL_TO_OBJECT(v);
		if (p == obj)
			return true;
	}
	return false;
}				/* iswindocloc */

/*********************************************************************
Convert a JS string to a C string.
This function is named JS_c_str to remind you of the C++ equivalent.
However, this function allocates the result; you must free it.
The converse is handled by JS_NewStringcopyZ, as provided by the library.
*********************************************************************/

static char *JS_c_str(js::HandleString str)
{
	size_t encodedLength = JS_GetStringEncodingLength(jcx, str);
	char *buffer = allocMem(encodedLength + 1);
	buffer[encodedLength] = '\0';
	size_t result =
	    JS_EncodeStringToBuffer(jcx, str, buffer, encodedLength);
	if (result == (size_t) - 1) {
		misconfigure();
		buffer[0] = 0;
	}
	return buffer;
}				/* JS_c_str */

/* represent an object pointer in ascii */
static const char *pointerString(const JSObject * obj)
{
	static char pbuf[32];
	sprintf(pbuf, "%p", obj);
	return pbuf;
}				/* pointerString */

/* like the function in ebjs.c, but a different name */
static const char *fakePropName(void)
{
	static char fakebuf[24];
	static int idx = 0;
	++idx;
	sprintf(fakebuf, "cg$$%d", idx);
	return fakebuf;
}				/*fakePropName */

/*********************************************************************
This returns the string equivalent of the js value, but use with care.
It's only good til the next call to stringize, then it will be trashed.
If you want the result longer than that, you better copy it.
*********************************************************************/

static const char *stringize(js::HandleValue v)
{
	static char buf[32];
	static const char *dynamic;
	int n;
	double d;
	if (JSVAL_IS_STRING(v)) {
		if (dynamic)
			nzFree(dynamic);
		js::RootedString str(jcx, JSVAL_TO_STRING(v));
		dynamic = JS_c_str(str);
		return dynamic;
	}

	if (JSVAL_IS_INT(v)) {
		n = JSVAL_TO_INT(v);
		sprintf(buf, "%d", n);
		return buf;
	}

	if (JSVAL_IS_BOOLEAN(v)) {
		n = JSVAL_TO_BOOLEAN(v);
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

/* don't know what it is */
	return 0;
}				/* stringize */

/* The generic class and constructor */

#define generic_class(c, name) \
static JSClass c##_class = { \
	#name, JSCLASS_HAS_PRIVATE, \
	JS_PropertyStub, JS_DeletePropertyStub, \
	JS_PropertyStub, JS_StrictPropertyStub, \
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, \
	NULL, JSCLASS_NO_OPTIONAL_MEMBERS \
};

#define generic_ctor(c) \
static JSBool c##_ctor(JSContext * cx, unsigned int argc, jsval * vp) \
{ \
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp); \
	JSObject & callee = args.callee(); \
	jsval callee_val = JS::ObjectValue(callee); \
	JS::RootedObject newobj(cx, \
		JS_NewObjectForConstructor(cx, &c##_class, &callee_val)); \
	if (newobj == NULL) { \
		misconfigure(); \
		return JS_FALSE; \
	} \
	args.rval().set(OBJECT_TO_JSVAL(newobj)); \
	return JS_TRUE; \
}

#define generic_class_ctor(c, name) \
generic_class(c, name) \
generic_ctor(c)

/* window class is diffferent, because the flags must be global */
static JSClass window_class = {
	"Window",
	JSCLASS_HAS_PRIVATE | JSCLASS_GLOBAL_FLAGS,
	JS_PropertyStub, JS_DeletePropertyStub,
	JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
	NULL, JSCLASS_NO_OPTIONAL_MEMBERS
};

/* the window constructor can open a new window, a new edbrowse session. */
/* This is done by setting n{url} in the effects string */
static JSBool window_ctor(JSContext * cx, unsigned int argc, jsval * vp)
{
	const char *newloc = 0;
	const char *winname = 0;
	JS::RootedString str(cx);
	js::RootedValue v(cx);
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JSObject & callee = args.callee();
	jsval callee_val = JS::ObjectValue(callee);
	JS::RootedObject newwin(cx,
				JS_NewObjectForConstructor(cx, &window_class,
							   &callee_val));
	if (newwin == NULL) {
		misconfigure();
		return JS_FALSE;
	}
	if (args.length() > 0 && (str = JS_ValueToString(cx, args[0])))
		newloc = JS_c_str(str);
	if (args.length() > 1 && (str = JS_ValueToString(cx, args[1])))
		winname = JS_c_str(str);
/* third argument is attributes, like window size and location,
 * that we don't care about.
 * I only do something if opening a new web page.
 * If it's just a blank window, I don't know what to do with that. */
	if (newloc && *newloc) {
		effects += "n{";	// }
		effects += newloc;
		effects += '\n';
		effects += winname;
		endeffect();
	}
	nzFree(newloc);
	nzFree(winname);
	v = OBJECT_TO_JSVAL(winobj);
	JS_SetProperty(cx, newwin, "opener", v.address());
	args.rval().set(OBJECT_TO_JSVAL(newwin));
	return JS_TRUE;
}				/* window_ctor */

/* All the other dom classes and constructors.
 * If a constructor is not in this list, it is coming later,
 * because it does something special. */
generic_class_ctor(document, Document)
    generic_class_ctor(html, Html)
    generic_class_ctor(head, Head)
    generic_class_ctor(meta, Meta)
    generic_class_ctor(link, Link)
    generic_class_ctor(body, Body)
    generic_class_ctor(base, Base)
    generic_class_ctor(form, Form)
    generic_class_ctor(element, Element)
    generic_class_ctor(image, Image)
    generic_class_ctor(frame, Frame)
    generic_class_ctor(anchor, Anchor)
    generic_class_ctor(table, Table)
    generic_class_ctor(div, Div)
    generic_class_ctor(area, Area)
    generic_class_ctor(span, Span)
    generic_class_ctor(trow, Trow)
    generic_class_ctor(cell, Cell)
    generic_class(option, Option)
/* constructor below */
    generic_class_ctor(script, Script)
    generic_class(url, URL)
/* constructor below */
    generic_class(timer, Timer)
/* instantiated through window.setTimout() */
#define PROP_STD (JSPROP_ENUMERATE | JSPROP_PERMANENT)
#define PROP_READONLY (JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY)
/* text and value can be passed as args to the constructor */
static JSBool option_ctor(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::RootedString str(cx);
	js::RootedValue v(cx);
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JSObject & callee = args.callee();
	jsval callee_val = JS::ObjectValue(callee);
	JS::RootedObject newopt(cx,
				JS_NewObjectForConstructor(cx, &option_class,
							   &callee_val));
	if (newopt == NULL) {
		misconfigure();
		return JS_FALSE;
	}
	if (args.length() > 0 && (str = JS_ValueToString(cx, args[0]))) {
		v = STRING_TO_JSVAL(str);
		JS_DefineProperty(cx, newopt, "text", v, NULL, NULL, PROP_STD);
	}
	if (args.length() > 1 && (str = JS_ValueToString(cx, args[1]))) {
		v = STRING_TO_JSVAL(str);
		JS_DefineProperty(cx, newopt, "value", v, NULL, NULL, PROP_STD);
	}
	str = JS_NewStringCopyZ(cx, "OPTION");
	v = STRING_TO_JSVAL(str);
	JS_DefineProperty(cx, newopt, "nodeName", v, NULL, NULL, PROP_READONLY);
	v = JSVAL_FALSE;
	JS_DefineProperty(cx, newopt, "selected", v, NULL, NULL, PROP_STD);
	JS_DefineProperty(cx, newopt, "defaultSelected", v, NULL, NULL,
			  PROP_STD);
	args.rval().set(OBJECT_TO_JSVAL(newopt));
	return JS_TRUE;
}				/* option_ctor */

static void url_initialize(JS::HandleObject uo, const char *url,
			   bool exclude_href);

static JSBool url_ctor(JSContext * cx, unsigned int argc, jsval * vp)
{
	const char *url = emptyString;
	const char *s;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JSObject & callee = args.callee();
	jsval callee_val = JS::ObjectValue(callee);
	JS::RootedObject uo(cx,
			    JS_NewObjectForConstructor(cx, &url_class,
						       &callee_val));
	if (uo == NULL) {
		misconfigure();
		return JS_FALSE;
	}
	if (args.length() > 0 && JSVAL_IS_STRING(args[0])) {
		js::RootedValue v(cx, args[0]);
		s = stringize(v);
		if (s[0])
			url = s;
	}			/* string argument */
	url_initialize(uo, url, false);
	args.rval().set(OBJECT_TO_JSVAL(uo));
	return JS_TRUE;
}				/* url_ctor */

/*********************************************************************
Setters perform special actions when values are assigned to js properties
or even objects. These must be implemented as native C functions.
Setters are effectively suspended by setting the following variable to true.
This is usually done when the dom manipulates the javascript objects directly.
When website javascript is running however,
the setters should run as well.
*********************************************************************/

static bool setter_suspend;

/* Lots of little routines for the pieces of the URL object */

/* Put a url together from its pieces, after something has changed. */
static void build_url(JS::HandleObject uo, int component, const char *e)
{
	js::RootedValue v(jcx);
	const char *new_url;
	string url_str, pathname;
	static const char *const noslashes[] = {
		"mailto", "telnet", "javascript", 0
	};

	setter_suspend = true;
/* e was built using stringize(), best to copy it */
	e = cloneString(e);

	if (component == 1)	// protocol
		url_str = e;
	else {
		if (JS_GetProperty(jcx, uo, "protocol", v.address()) ==
		    JS_FALSE) {
abort:
			setter_suspend = false;
			nzFree(e);
			misconfigure();
			return;
		}
		url_str = stringize(v);
	}
	if (url_str.length() && stringInListCI(noslashes, url_str.c_str()) < 0)
		url_str += "//";

	if (component == 2)	// host
		url_str += e;
	else {
		if (JS_GetProperty(jcx, uo, "host", v.address()) == JS_FALSE)
			goto abort;
		url_str += stringize(v);
	}

	if (component == 3)	// path
		pathname = e;
	else {
		if (JS_GetProperty(jcx, uo, "pathname", v.address()) ==
		    JS_FALSE)
			goto abort;
		pathname = stringize(v);
	}
	if (pathname[0] != '/')
		url_str += '/';
	url_str += pathname;

	if (component == 4)	// search
		url_str += e;
	else {
		if (JS_GetProperty(jcx, uo, "search", v.address()) == JS_FALSE)
			goto abort;
		url_str += stringize(v);
	}

	if (component == 5)	// hash
		url_str += e;
	else {
		if (JS_GetProperty(jcx, uo, "hash", v.address()) == JS_FALSE)
			goto abort;
		url_str += stringize(v);
	}

	new_url = url_str.c_str();
	v = STRING_TO_JSVAL(JS_NewStringCopyZ(jcx, new_url));
	if (JS_SetProperty(jcx, uo, "href", v.address()) == JS_FALSE)
		goto abort;

	setter_suspend = false;
	nzFree(e);
}				/* build_url */

/* Rebuild host, because hostname or port changed. */
static void build_host(JS::HandleObject uo, int component,
		       const char *hostname, int port)
{
	js::RootedValue v(jcx);
	const char *oldhost;
	bool hadcolon = false;
	string q;
	const char *newhost;

	setter_suspend = true;

	if (JS_GetProperty(jcx, uo, "host", v.address()) == JS_FALSE)
		goto abort;
	oldhost = stringize(v);
	if (strchr(oldhost, ':'))
		hadcolon = true;

	if (component == 1) {
		if (JS_GetProperty(jcx, uo, "port", v.address()) == JS_FALSE) {
abort:
			setter_suspend = false;
			misconfigure();
			return;
		}
		port = JSVAL_TO_INT(v);
	} else {
		if (JS_GetProperty(jcx, uo, "hostname", v.address()) ==
		    JS_FALSE)
			goto abort;
		hostname = stringize(v);
	}

	q = hostname;
	if (component == 2 || hadcolon) {
		q += ':';
		q += port;
	}
	newhost = q.c_str();
	v = STRING_TO_JSVAL(JS_NewStringCopyZ(jcx, newhost));
	if (JS_SetProperty(jcx, uo, "host", v.address()) == JS_FALSE)
		goto abort;

	setter_suspend = false;
}				/* build_host */

static void
loc_def_set(JS::HandleObject uo, const char *name, const char *s,
	    JSStrictPropertyOp setter)
{
	JSBool found;
	js::RootedValue v(jcx);

	if (s)
		v = STRING_TO_JSVAL(JS_NewStringCopyZ(jcx, s));
	else
		v = JS_GetEmptyStringValue(jcx);
	JS_HasProperty(jcx, uo, name, &found);
	if (found) {
		if (JS_SetProperty(jcx, uo, name, v.address()) == JS_FALSE) {
abort:
			misconfigure();
			return;
		}
	} else {
		if (JS_DefineProperty(jcx, uo, name, v, NULL, setter, PROP_STD)
		    == JS_FALSE)
			goto abort;
	}
}				/* loc_def_set */

/* Like the above, but using an integer, this is for port only. */
static void
loc_def_set_n(JS::HandleObject uo, const char *name, int port,
	      JSStrictPropertyOp setter)
{
	JSBool found;
	js::RootedValue v(jcx, INT_TO_JSVAL(port));

	JS_HasProperty(jcx, uo, name, &found);
	if (found) {
		if (JS_SetProperty(jcx, uo, name, v.address()) == JS_FALSE) {
abort:
			misconfigure();
			return;
		}
	} else {
		if (JS_DefineProperty(jcx, uo, name, v, NULL, setter, PROP_STD)
		    == JS_FALSE)
			goto abort;
	}
}				/* loc_def_set_n */

/* string s of length n */
static void
loc_def_set_part(JS::HandleObject uo, const char *name, const char *s,
		 int n, JSStrictPropertyOp setter)
{
	JSBool found;
	js::RootedValue v(jcx);

	if (s)
		v = STRING_TO_JSVAL(JS_NewStringCopyN(jcx, s, n));
	else
		v = JS_GetEmptyStringValue(jcx);

	JS_HasProperty(jcx, uo, name, &found);
	if (found) {
		if (JS_SetProperty(jcx, uo, name, v.address()) == JS_FALSE) {
abort:
			misconfigure();
			return;
		}
	} else {
		if (JS_DefineProperty(jcx, uo, name, v, NULL, setter, PROP_STD)
		    == JS_FALSE)
			goto abort;
	}
}				/* loc_def_set_part */

/* this setter is only for window.location or document.location */
static JSBool
setter_loc(JSContext * cx, JS::HandleObject uo, JS::Handle < jsid > id,
	   JSBool strict, JS::MutableHandle < JS::Value > vp)
{
	const char *s = stringize(vp);
	if (!s) {
		JS_ReportError(jcx,
			       "window.location is assigned something that I don't understand");
	} else {
		effects += "n{";	// }
		effects += s;
		effects += '\n';
		endeffect();
	}
	return JS_TRUE;
}				/* setter_loc */

/* this setter can also open a new window, if the parent object
 * is window.location or document.location.
 * Otherwise it has the usual side effects for the URL class,
 * distributing the pieces of the url to the members. */
static JSBool
setter_loc_href(JSContext * cx, JS::HandleObject uo,
		JS::Handle < jsid > id, JSBool strict,
		JS::MutableHandle < JS::Value > vp)
{
	const char *url = 0;
	const char *urlcopy;
	if (setter_suspend)
		return JS_TRUE;
	url = stringize(vp);
	if (!url)
		return JS_TRUE;
	urlcopy = cloneString(url);
	url_initialize(uo, urlcopy, true);
	if (iswindocloc(uo)) {
		effects += "n{";	// }
		effects += urlcopy;
		effects += '\n';
		endeffect();
	}
	nzFree(urlcopy);
	return JS_TRUE;
}				/* setter_loc_href */

static JSBool
setter_loc_hash(JSContext * cx, JS::HandleObject uo,
		JS::Handle < jsid > id, JSBool strict,
		JS::MutableHandle < JS::Value > vp)
{
	const char *e;
	if (setter_suspend)
		return JS_TRUE;
	e = stringize(vp);
	build_url(uo, 5, e);
	return JS_TRUE;
}				/* setter_loc_hash */

static JSBool
setter_loc_search(JSContext * cx, JS::HandleObject uo,
		  JS::Handle < jsid > id, JSBool strict,
		  JS::MutableHandle < JS::Value > vp)
{
	const char *e;
	if (setter_suspend)
		return JS_TRUE;
	e = stringize(vp);
	build_url(uo, 4, e);
	return JS_TRUE;
}				/* setter_loc_search */

static JSBool
setter_loc_prot(JSContext * cx, JS::HandleObject uo,
		JS::Handle < jsid > id, JSBool strict,
		JS::MutableHandle < JS::Value > vp)
{
	const char *e;
	if (setter_suspend)
		return JS_TRUE;
	e = stringize(vp);
	build_url(uo, 1, e);
	return JS_TRUE;
}				/* setter_loc_prot */

static JSBool
setter_loc_pathname(JSContext * cx, JS::HandleObject uo,
		    JS::Handle < jsid > id, JSBool strict,
		    JS::MutableHandle < JS::Value > vp)
{
	const char *e;
	if (setter_suspend)
		return JS_TRUE;
	e = stringize(vp);
	build_url(uo, 3, e);
	return JS_TRUE;
}				/* setter_loc_pathname */

static JSBool
setter_loc_hostname(JSContext * cx, JS::HandleObject uo,
		    JS::Handle < jsid > id, JSBool strict,
		    JS::MutableHandle < JS::Value > vp)
{
	const char *e;
	if (setter_suspend)
		return JS_TRUE;
	e = stringize(vp);
	build_host(uo, 1, e, 0);
	build_url(uo, 0, 0);
	return JS_TRUE;
}				/* setter_loc_hostname */

static JSBool
setter_loc_port(JSContext * cx, JS::HandleObject uo,
		JS::Handle < jsid > id, JSBool strict,
		JS::MutableHandle < JS::Value > vp)
{
	int port;
	if (setter_suspend)
		return JS_TRUE;
	port = JSVAL_TO_INT(vp);
	build_host(uo, 2, 0, port);
	build_url(uo, 0, 0);
	return JS_TRUE;
}				/* setter_loc_port */

static JSBool
setter_loc_host(JSContext * cx, JS::HandleObject uo,
		JS::Handle < jsid > id, JSBool strict,
		JS::MutableHandle < JS::Value > vp)
{
	const char *e, *s;
	int n;
	js::RootedValue v(jcx);
	if (setter_suspend)
		return JS_TRUE;
	e = stringize(vp);
	e = cloneString(e);
	build_url(uo, 2, e);
/* and we have to update hostname and port */
	setter_suspend = true;
	s = strchr(e, ':');
	if (s)
		n = s - e;
	else
		n = strlen(e);
	v = STRING_TO_JSVAL(JS_NewStringCopyN(jcx, e, n));
	if (JS_SetProperty(jcx, uo, "hostname", v.address()) == JS_FALSE) {
abort:
		setter_suspend = false;
		nzFree(e);
		return JS_FALSE;
	}
	if (s) {
		v = INT_TO_JSVAL(atoi(s + 1));
		if (JS_SetProperty(jcx, uo, "port", v.address()) == JS_FALSE)
			goto abort;
	}
	setter_suspend = false;
	nzFree(e);
	return JS_TRUE;
}				/* setter_loc_host */

/*********************************************************************
As the name url_initialize might indicate, this function is called
by the constructor, to create the pieces of the url:
protocol, port, domain, path, hash, etc.
But it is also called when a url is assigned to URL.href,
again to cut the url into pieces and set the corresponding members.
This is how the URL class works, and it is a royal pain to program.
*********************************************************************/

static void
url_initialize(JS::HandleObject uo, const char *url, bool exclude_href)
{
	int n, port;
	const char *data;
	const char *s;
	const char *pl;
	string q;

	setter_suspend = true;

/* Store the url in location.href */
	if (!exclude_href)
		loc_def_set(uo, "href", url, setter_loc_href);

/* Now make a property for each piece of the url. */
	if (s = getProtURL(url)) {
		q = s;
		q += ':';
		s = q.c_str();
	}
	loc_def_set(uo, "protocol", s, setter_loc_prot);

	data = getDataURL(url);
	s = 0;
	if (data)
		s = strchr(data, '#');
	loc_def_set(uo, "hash", s, setter_loc_hash);

	s = getHostURL(url);
	if (s && !*s)
		s = 0;
	loc_def_set(uo, "hostname", s, setter_loc_hostname);

	getPortLocURL(url, &pl, &port);
	loc_def_set_n(uo, "port", port, setter_loc_port);

	if (s) {		/* this was hostname */
		q = s;
		if (pl) {
			q += ':';
			q += port;
		}
		s = q.c_str();
	}
	loc_def_set(uo, "host", s, setter_loc_host);

	s = 0;
	n = 0;
	getDirURL(url, &s, &pl);
	if (s) {
		pl = strpbrk(s, "?\1#");
		n = pl ? pl - s : strlen(s);
		if (!n)
			s = "/", n = 1;
	}
	loc_def_set_part(uo, "pathname", s, n, setter_loc_pathname);

	s = 0;
	if (data && (s = strpbrk(data, "?\1")) &&
	    (!(pl = strchr(data, '#')) || pl > s)) {
		if (pl)
			n = pl - s;
		else
			n = strlen(s);
	} else {
/* If we have foo.html#?bla, then ?bla is not the query. */
		s = NULL;
		n = 0;
	}
	loc_def_set_part(uo, "search", s, n, setter_loc_search);

	setter_suspend = false;
}				/* url_initialize */

static const char *emptyParms[] = { 0 };
static jsval emptyArgs[] = { jsval() };

/* Use stringize() to return a property as a string, if it is
 * string compatible. The string is allocated, free it when done. */
static char *get_property_string(JS::HandleObject parent, const char *name)
{
	js::RootedValue v(jcx);
	const char *s;
	if (JS_GetProperty(jcx, parent, name, v.address()) == JS_FALSE)
		return NULL;

	if (v.isObject()) {
/* special code here to return the object pointer */
/* That's what edbrowse is going to want. */
		s = pointerString(JSVAL_TO_OBJECT(v));
	} else
		s = stringize(v);
	return cloneString(s);
}				/* get_property_string */

/* if js changes the value of an input field, this must be reflected
 * in the <foobar> text in edbrowse. */
static JSBool
setter_value(JSContext * cx, JS::HandleObject obj,
	     JS::Handle < jsid > id, JSBool strict,
	     JS::MutableHandle < JS::Value > vp)
{
	const char *val;
	if (setter_suspend)
		return JS_TRUE;
	val = stringize(vp);
	if (!val) {
		JS_ReportError(jcx,
			       "input.value is assigned something other than a string; this can cause problems when you submit the form.");
	} else {
		effects += "v{";	// }
		effects += pointerString(*obj.address());
		effects += '=';
		effects += val;
		endeffect();
	}
	return JS_TRUE;
}				/* setter_value */

static JSBool
setter_innerHTML(JSContext * cx, JS::HandleObject obj,
		 JS::Handle < jsid > id, JSBool strict,
		 JS::MutableHandle < jsval > vp)
{
	const char *s = stringize(vp);
	if (s && strlen(s)) {
		effects += "i{h<!-- inner html -->\n";	// }
		effects += s;
		if (s[strlen(s) - 1] != '\n')
			effects += '\n';
		endeffect();
	}
	return JS_TRUE;
}				/* setter_innerHTML */

static JSBool
setter_innerText(JSContext * cx, JS::HandleObject obj,
		 JS::Handle < jsid > id, JSBool strict,
		 JS::MutableHandle < jsval > vp)
{
	const char *s = stringize(vp);
	if (s && strlen(s)) {
		effects += "i{t";	// }
		effects += s;
		if (s[strlen(s) - 1] != '\n')
			effects += '\n';
		endeffect();
	}
	return JS_TRUE;
}				/* setter_innerText */

/*********************************************************************
Maintain a copy of the cookie string that is relevant for this web page.
Include a leading semicolon, thus the form
; foo=73838; bar=j_k_qqr; bas=21998999
This is the same as the string in document.cookie.
But the setter folds a new cookie into this string,
and also passes the cookie back to edbrowse to put in the cookie jar.
*********************************************************************/

static string cookieCopy;

static bool foldinCookie(const char *newcook)
{
	string nc = newcook;
	int j, k;

/* cut off the extra attributes */
	j = nc.find_first_of(" \t;");
	if (j != string::npos)
		nc.resize(j);

/* cookie has to look like keyword=value */
	j = nc.find("=");
	if (j == string::npos || j == 0)
		return false;

/* pass back to edbrowse */
	effects += "c{";	// }
	effects += newcook;
	endeffect();

/* put ; in front */
	string nc1 = "; " + nc;
	j = j + 2;
	string search = nc1.substr(0, j + 1);
	k = cookieCopy.find(search);
	if (k == string::npos) {
/* not there, just tack the new cookie on the end */
		cookieCopy += nc1;
		return true;
	}

	string rest = cookieCopy.substr(k + 2);
	cookieCopy.resize(k);
	cookieCopy += nc1;
	k = rest.find(";");
	if (k != string::npos)
		cookieCopy += rest.substr(k);
	return true;
}				/* foldinCookie */

static JSBool
setter_cookie(JSContext * cx, JS::HandleObject obj,
	      JS::Handle < jsid > id, JSBool strict,
	      JS::MutableHandle < JS::Value > vp)
{
	const char *val;
	if (setter_suspend)
		return JS_TRUE;

	val = stringize(vp);
	if (val && foldinCookie(val)) {
		setter_suspend = true;
		js::RootedValue v(jcx);
		v = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, val));
		JS_SetProperty(jcx, obj, "cookie", v.address());
		setter_suspend = false;
	}

	return JS_TRUE;
}				/* setter_cookie */

/* this is a placeholder */
static JSBool
setter_domain(JSContext * cx, JS::HandleObject obj,
	      JS::Handle < jsid > id, JSBool strict,
	      JS::MutableHandle < JS::Value > vp)
{
	const char *val;
	if (setter_suspend)
		return JS_TRUE;
	val = stringize(vp);
	return JS_TRUE;
}				/* setter_domain */

static void
set_property_string(js::HandleObject parent, const char *name,
		    const char *value)
{
	js::RootedValue v(jcx);
	JSStrictPropertyOp my_setter = NULL;
	JSBool found;

	if (value && *value)
		v = STRING_TO_JSVAL(JS_NewStringCopyZ(jcx, value));
	else
		v = JS_GetEmptyStringValue(jcx);

	JS_HasProperty(jcx, parent, name, &found);
	if (found) {
		setter_suspend = true;
		if (JS_SetProperty(jcx, parent, name, v.address()) == JS_FALSE)
			misconfigure();
		setter_suspend = false;
		return;
	}

	if (stringEqual(name, "value"))
		my_setter = setter_value;
	if (stringEqual(name, "innerHTML"))
		my_setter = setter_innerHTML;
	if (stringEqual(name, "innerText"))
		my_setter = setter_innerText;
	if (stringEqual(name, "domain") && *parent.address() == docobj)
		my_setter = setter_domain;
	if (stringEqual(name, "cookie") && *parent.address() == docobj)
		my_setter = setter_cookie;

	if (JS_DefineProperty(jcx, parent, name, v, NULL, my_setter, PROP_STD)
	    == JS_FALSE)
		misconfigure();
}				/* set_property_string */

static void set_property_bool(js::HandleObject parent, const char *name, bool n)
{
	js::RootedValue v(jcx);
	JSBool found;

	v = (n ? JSVAL_TRUE : JSVAL_FALSE);

	JS_HasProperty(jcx, parent, name, &found);
	if (found) {
		setter_suspend = true;
		if (JS_SetProperty(jcx, parent, name, v.address()) == JS_FALSE)
			misconfigure();
		setter_suspend = false;
		return;
	}

	if (JS_DefineProperty(jcx, parent, name, v, NULL, NULL, PROP_STD) ==
	    JS_FALSE)
		misconfigure();
}				/* set_property_bool */

static void
set_property_number(js::HandleObject parent, const char *name, int n)
{
	js::RootedValue v(jcx);
	JSBool found;

	v = INT_TO_JSVAL(n);

	JS_HasProperty(jcx, parent, name, &found);
	if (found) {
		setter_suspend = true;
		if (JS_SetProperty(jcx, parent, name, v.address()) == JS_FALSE)
			misconfigure();
		setter_suspend = false;
		return;
	}

	if (JS_DefineProperty(jcx, parent, name, v, NULL, NULL, PROP_STD) ==
	    JS_FALSE)
		misconfigure();
}				/* set_property_number */

static void
set_property_float(js::HandleObject parent, const char *name, double n)
{
	js::RootedValue v(jcx);
	JSBool found;

	v = DOUBLE_TO_JSVAL(n);

	JS_HasProperty(jcx, parent, name, &found);
	if (found) {
		setter_suspend = true;
		if (JS_SetProperty(jcx, parent, name, v.address()) == JS_FALSE)
			misconfigure();
		setter_suspend = false;
		return;
	}

	if (JS_DefineProperty(jcx, parent, name, v, NULL, NULL, PROP_STD) ==
	    JS_FALSE)
		misconfigure();
}				/* set_property_float */

static void
set_property_object(js::HandleObject parent, const char *name,
		    JS::HandleObject child)
{
	js::RootedValue v(jcx);
	JSBool found;
	JSStrictPropertyOp my_setter = NULL;

	v = OBJECT_TO_JSVAL(child);

	JS_HasProperty(jcx, parent, name, &found);
	if (found) {
		setter_suspend = true;
		if (JS_SetProperty(jcx, parent, name, v.address()) == JS_FALSE)
			misconfigure();
		setter_suspend = false;
		return;
	}

	if (stringEqual(name, "location") && iswindoc(parent))
		my_setter = setter_loc;

	if (JS_DefineProperty(jcx, parent, name, v, NULL, my_setter, PROP_STD)
	    == JS_FALSE)
		misconfigure();
}				/* set_property_object */

/* for window.focus etc */
static JSBool nullFunction(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* nullFunction */

static JSBool falseFunction(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	args.rval().set(JSVAL_FALSE);
	return JS_TRUE;
}				/* falseFunction */

static JSBool trueFunction(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	args.rval().set(JSVAL_TRUE);
	return JS_TRUE;
}				/* trueFunction */

static JSBool setAttribute(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	js::RootedValue v1(cx), v2(cx);
	if (args.length() != 2 || !JSVAL_IS_STRING(args[0])) {
		JS_ReportError(cx, "unexpected arguments to setAttribute()");
	} else {
		v1 = args[0];
		v2 = args[1];
		const char *prop = stringize(v1);
		if (JS_DefineProperty(cx, obj, prop, v2, NULL, NULL, PROP_STD)
		    == JS_FALSE) {
			misconfigure();
			return JS_FALSE;
		}
	}
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* setAttribute */

static JSBool appendChild(JSContext * cx, unsigned int argc, jsval * vp)
{
	unsigned length;
	js::RootedValue v(cx);
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
	if (JS_GetProperty(cx, obj, "elements", v.address()) == JS_FALSE)
		return JS_TRUE;	/* no such array */
	JS::RootedObject elar(cx, JSVAL_TO_OBJECT(v));
	if (elar == NULL) {
		misconfigure();
		return JS_FALSE;
	}
	if (JS_GetArrayLength(cx, elar, &length) == JS_FALSE) {
		misconfigure();
		return JS_FALSE;
	}
	if (JS_DefineElement(cx, elar, length,
			     (args.length() > 0 ? args[0] : JSVAL_NULL),
			     NULL, NULL, JSPROP_ENUMERATE) == JS_FALSE) {
		misconfigure();
		return JS_FALSE;
	}
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* appendChild */

static JSFunctionSpec body_methods[] = {
	JS_FS("setAttribute", setAttribute, 2, 0),
	JS_FS("appendChild", appendChild, 1, 0),
	JS_FS_END
};

static JSFunctionSpec head_methods[] = {
	JS_FS("setAttribute", setAttribute, 2, 0),
	JS_FS("appendChild", appendChild, 1, 0),
	JS_FS_END
};

static JSFunctionSpec link_methods[] = {
	JS_FS("setAttribute", setAttribute, 2, 0),
	JS_FS_END
};

static void dwrite1(unsigned int argc, jsval * argv, bool newline)
{
	int i;
	const char *msg;
	JS::RootedString str(jcx);
	effects += "w{";	// }
	for (i = 0; i < argc; ++i) {
		if ((str = JS_ValueToString(jcx, argv[i])) &&
		    (msg = JS_c_str(str))) {
			effects += msg;
			nzFree(msg);
		}
	}
	if (newline)
		effects += '\n';
	endeffect();
}				/* dwrite1 */

static JSBool doc_write(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	dwrite1(args.length(), args.array(), false);
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* doc_write */

static JSBool doc_writeln(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	dwrite1(args.length(), args.array(), true);
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* doc_writeln */

static JSFunctionSpec document_methods[] = {
	JS_FS("focus", nullFunction, 0, 0),
	JS_FS("blur", nullFunction, 0, 0),
	JS_FS("open", nullFunction, 0, 0),
	JS_FS("close", nullFunction, 0, 0),
	JS_FS("write", doc_write, 0, 0),
	JS_FS("writeln", doc_writeln, 0, 0),
	JS_FS_END
};

static JSFunctionSpec element_methods[] = {
	JS_FS("focus", nullFunction, 0, 0),
	JS_FS("blur", nullFunction, 0, 0),
	JS_FS_END
};

static JSBool form_submit(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
	effects += "f{s";	// }
	effects += pointerString(obj);
	endeffect();
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* form_submit */

static JSBool form_reset(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
	effects += "f{r";	// }
	effects += pointerString(obj);
	endeffect();
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* form_reset */

static JSFunctionSpec form_methods[] = {
	JS_FS("submit", form_submit, 0, 0),
	JS_FS("reset", form_reset, 0, 0),
	JS_FS_END
};

static JSBool win_close(JSContext * cx, unsigned int argc, jsval * vp)
{
	if (head.highstat <= EJ_HIGH_CX_FAIL) {
		head.highstat = EJ_HIGH_CX_FAIL;
		head.lowstat = EJ_LOW_CLOSE;
	}
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* win_close */

static JSBool win_alert(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	const char *msg = NULL;
	JS::RootedString str(cx);
	if (args.length() > 0 && (str = JS_ValueToString(cx, args[0]))) {
		msg = JS_c_str(str);
	}
	if (msg && *msg)
		puts(msg);
	nzFree(msg);
	args.rval().set(JSVAL_VOID);
	return JS_TRUE;
}				/* win_alert */

static JSBool win_prompt(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	const char *msg = emptyString;
	const char *answer = emptyString;
	JS::RootedString str(cx);
	char inbuf[80];
	char *s;
	char c;

	if (args.length() > 0 && (str = JS_ValueToString(cx, args[0])))
		msg = JS_c_str(str);
	if (!msg)
		return JS_TRUE;
	if (!*msg) {
		nzFree(msg);
		return JS_TRUE;
	}
	if (args.length() > 1 && (str = JS_ValueToString(cx, args[1])))
		answer = JS_c_str(str);

	printf("%s", msg);
/* If it doesn't end in space or question mark, print a colon */
	c = msg[strlen(msg) - 1];
	if (!isspace(c)) {
		if (!ispunct(c))
			printf(":");
		printf(" ");
	}
	if (answer)
		printf("[%s] ", answer);
	fflush(stdout);
	if (!fgets(inbuf, sizeof(inbuf), stdin))
		exit(5);
	s = inbuf + strlen(inbuf);
	if (s > inbuf && s[-1] == '\n')
		*--s = 0;
	if (inbuf[0]) {
		nzFree(answer);	/* Don't need the default answer anymore. */
		answer = inbuf;
	}
	args.rval().set(STRING_TO_JSVAL(JS_NewStringCopyZ(cx, answer)));
	if (answer != inbuf)
		nzFree(answer);
	return JS_TRUE;
}				/* win_prompt */

static JSBool win_confirm(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	const char *msg = emptyString;
	JS::RootedString str(cx);
	char inbuf[80];
	char c;
	bool first = true;

	if (args.length() > 0 && (str = JS_ValueToString(cx, args[0])))
		msg = JS_c_str(str);
	if (!msg)
		return JS_TRUE;
	if (!*msg) {
		nzFree(msg);
		return JS_TRUE;
	}

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

	c = tolower(c);
	if (c == 'y')
		args.rval().set(JSVAL_TRUE);
	else
		args.rval().set(JSVAL_FALSE);
	nzFree(msg);
	return JS_TRUE;
}				/* win_confirm */

/* Set a timer or an interval */
static JSObject *setTimeout(unsigned int argc, jsval * argv, bool isInterval)
{
	JS::RootedValue v0(jcx), v1(jcx);
	JS::RootedObject fo(jcx), to(jcx);
	int n;			/* number of milliseconds */
	char fname[48];		/* function name */
	const char *fstr;	/* function string */
	const char *methname = (isInterval ? "setInterval" : "setTimeout");
	const char *allocatedName = NULL;
	const char *s = NULL;

	if (argc != 2 || !JSVAL_IS_INT(argv[1]))
		goto badarg;

	v0 = argv[0];
	v1 = argv[1];
	n = JSVAL_TO_INT(v1);
	if (JSVAL_IS_STRING(v0) ||
	    v0.isObject() &&
	    JS_ValueToObject(jcx, v0, fo.address()) &&
	    JS_ObjectIsFunction(jcx, fo)) {

/* build the tag object and link it to window */
		to = JS_NewObject(jcx, &timer_class, NULL, winobj);
		if (to == NULL) {
abort:
			misconfigure();
			return NULL;
		}
		v1 = OBJECT_TO_JSVAL(to);
		if (JS_DefineProperty
		    (jcx, winobj, fakePropName(), v1, NULL, NULL,
		     PROP_READONLY) == JS_FALSE)
			goto abort;
		if (fo) {
/* Extract the function name, which requires several steps */
			js::RootedFunction f(jcx,
					     JS_ValueToFunction(jcx,
								OBJECT_TO_JSVAL
								(fo)));
			JS::RootedString jss(jcx, JS_GetFunctionId(f));
			if (jss)
				allocatedName = JS_c_str(jss);
			s = allocatedName;
/* Remember that unnamed functions are named anonymous. */
			if (!s || !*s || stringEqual(s, "anonymous"))
				s = "javascript";
			int len = strlen(s);
			if (len > sizeof(fname) - 4)
				len = sizeof(fname) - 4;
			strncpy(fname, s, len);
			nzFree(allocatedName);
			fname[len] = 0;
			strcat(fname, "()");
			fstr = fname;
			set_property_object(to, "onclick", fo);

		} else {

/* compile the function from the string */
			fstr = stringize(v0);
			if (JS_CompileFunction
			    (jcx, to, "onclick", 0, emptyParms, fstr,
			     strlen(fstr), "onclick", 1) == NULL) {
				JS_ReportError(jcx,
					       "error compiling function in %s()",
					       methname);
				return NULL;
			}
			strcpy(fname, "?");
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
			fstr = fname;
		}

		effects += "t{";	// }
		effects += n;
		effects += '|';
		effects += fstr;
		effects += '|';
		effects += pointerString(to);
		effects += '|';
		effects += isInterval;
		endeffect();

		return to;
	}

badarg:
	JS_ReportError(jcx, "invalid arguments to %s()", methname);
	return NULL;
}				/* setTimeout */

/* set timer and set interval */
static JSBool win_sto(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject obj(jcx,
			     setTimeout(args.length(), args.array(), false));
	if (obj == NULL)
		return JS_FALSE;
	args.rval().set(OBJECT_TO_JSVAL(obj));
	return JS_TRUE;
}				/* win_sto */

static JSBool win_intv(JSContext * cx, unsigned int argc, jsval * vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject obj(jcx,
			     setTimeout(args.length(), args.array(), true));
	if (obj == NULL)
		return JS_FALSE;
	args.rval().set(OBJECT_TO_JSVAL(obj));
	return JS_TRUE;
}				/* win_intv */

static JSFunctionSpec window_methods[] = {
	JS_FS("alert", win_alert, 1, 0),
	JS_FS("prompt", win_prompt, 2, 0),
	JS_FS("confirm", win_confirm, 1, 0),
	JS_FS("setTimeout", win_sto, 2, 0),
	JS_FS("setInterval", win_intv, 2, 0),
	JS_FS("close", win_close, 0, 0),
	JS_FS("focus", nullFunction, 0, 0),
	JS_FS("blur", nullFunction, 0, 0),
	JS_FS("scroll", nullFunction, 0, 0),
	JS_FS_END
};

static struct {
	JSClass *obj_class;
	JSNative constructor;
	JSFunctionSpec *methods;
	int nargs;
} const domClasses[] = {
	{&document_class, document_ctor, document_methods},
	{&html_class, html_ctor},
	{&head_class, head_ctor, head_methods},
	{&meta_class, meta_ctor},
	{&link_class, link_ctor, link_methods, 0},
	{&body_class, body_ctor, body_methods},
	{&base_class, base_ctor},
	{&form_class, form_ctor, form_methods},
	{&element_class, element_ctor, element_methods, 0},
	{&image_class, image_ctor, NULL, 1},
	{&frame_class, frame_ctor},
	{&anchor_class, anchor_ctor, NULL, 1},
	{&table_class, table_ctor},
	{&trow_class, trow_ctor},
	{&cell_class, cell_ctor},
	{&div_class, div_ctor},
	{&area_class, area_ctor},
	{&span_class, span_ctor},
	{&option_class, option_ctor, NULL, 2},
	{&script_class, script_ctor},
	{&url_class, url_ctor},
	{0}
};

static void createJavaContext(void)
{
	int i;

	jcx = JS_NewContext(jrt, gStackChunkSize);
	if (!jcx) {
		head.highstat = EJ_HIGH_HEAP_FAIL;
		head.lowstat = EJ_LOW_CX;
		return;
	}

	JSAutoRequest autoreq(jcx);
	JS_SetErrorReporter(jcx, my_ErrorReporter);
	JS_SetOptions(jcx, JSOPTION_VAROBJFIX);

/* Create the Window object, which is the global object in DOM. */
	winobj = JS_NewGlobalObject(jcx, &window_class, NULL);
	if (!winobj) {
no_win:
		head.highstat = EJ_HIGH_HEAP_FAIL;
		head.lowstat = EJ_LOW_WIN;
		JS_DestroyContext(jcx);
		return;
	}

/* enter the compartment for this object for the duration of this function */
	JSAutoCompartment ac(jcx, winobj);

/* now set the window object as global */
	JS_SetGlobalObject(jcx, winobj);
/* Math, Date, Number, String, etc */
	if (!JS_InitStandardClasses(jcx, winobj)) {
no_std:
		head.highstat = EJ_HIGH_HEAP_FAIL;
		head.lowstat = EJ_LOW_STD;
		JS_DestroyContext(jcx);
		return;
	}

/* initialise the window class */
	if (JS_InitClass
	    (jcx, winobj, NULL, &window_class, window_ctor, 3, NULL,
	     window_methods, NULL, NULL) == NULL)
		goto no_win;
/* Ok, but the global object was created before the class,
* so it doesn't have its methods yet. */
	if (JS_DefineFunctions(jcx, winobj, window_methods) == JS_FALSE)
		goto no_win;

/* Other classes that we'll need. */
	for (i = 0; domClasses[i].obj_class; ++i) {
		if (JS_InitClass
		    (jcx, winobj, 0, domClasses[i].obj_class,
		     domClasses[i].constructor, domClasses[i].nargs, NULL,
		     domClasses[i].methods, NULL, NULL) == NULL)
			goto no_std;
	}

/* document under window */
	JS::RootedObject d(jcx,
			   JS_NewObject(jcx, &document_class, NULL, winobj));
	if (!d) {
no_doc:
		head.highstat = EJ_HIGH_HEAP_FAIL;
		head.lowstat = EJ_LOW_DOC;
		JS_DestroyContext(jcx);
		return;
	}
	docobj = *d.address();
	js::RootedValue v(jcx, OBJECT_TO_JSVAL(d));
	if (JS_SetProperty(jcx, winobj, "document", v.address()) == JS_FALSE)
		goto no_doc;

}				/* createJavaContext */

/* based on propval and proptype */
static void set_property_generic(js::HandleObject parent, const char *name)
{
	int i, n;
	double d;
	JSObject *child;
	JS::RootedObject childroot(jcx);
	JSClass *cp;

	switch (proptype) {
	case EJ_PROP_STRING:
		set_property_string(parent, name, propval);
		break;

	case EJ_PROP_INT:
		n = atoi(propval);
		set_property_number(parent, name, n);
		break;

	case EJ_PROP_BOOL:
		n = atoi(propval);
		set_property_bool(parent, name, n);
		break;

	case EJ_PROP_FLOAT:
		d = atof(propval);
		set_property_float(parent, name, d);
		break;

	case EJ_PROP_OBJECT:
		sscanf(propval, "%p", &child);
		childroot = child;
		set_property_object(parent, name, childroot);
		break;

	case EJ_PROP_INSTANCE:
		cp = 0;
		if (propval) {
/* find the class */
			for (i = 0; cp = domClasses[i].obj_class; ++i)
				if (stringEqual(cp->name, propval))
					break;
			if (!cp) {
				cerr << "Unexpected class name " << propval <<
				    " from edbrowse\n";
				exit(8);
			}
			nzFree(propval);
			propval = 0;
		}
		childroot = JS_NewObject(jcx, cp, NULL, parent);
		if (!childroot) {
			misconfigure();
			break;
		}
childreturn:
		set_property_object(parent, name, childroot);
		propval = cloneString(pointerString(*childroot.address()));
		break;

	case EJ_PROP_ARRAY:
		childroot = JS_NewArrayObject(jcx, 0, NULL);
		goto childreturn;

	case EJ_PROP_FUNCTION:
		if (!propval || !*propval) {
/* null or empty function, link to native null function */
			JS_NewFunction(jcx, nullFunction, 0, 0, parent,
				       membername);
		} else {
			JS_CompileFunction(jcx, parent, name, 0, emptyParms,
					   propval, strlen(propval), name, 1);
		}
		break;

	default:
		cerr << "Unexpected property type " << proptype <<
		    " from edbrowse\n";
		exit(7);
	}

}				/* set_property_generic */

/*********************************************************************
ebjs.c allows for geting and setting array elements of all types,
however the DOM only uses objects. Being lazy, I will simply
implement objects. You can add other types later.
*********************************************************************/

static JSObject *get_array_element_object(JS::HandleObject parent, int idx)
{
	js::RootedValue v(jcx);
	JS::RootedObject child(jcx);
	if (JS_GetElement(jcx, parent, idx, v.address()) == JS_FALSE)
		return 0;	/* perhaps out of range */
	if (!v.isObject()) {
		cerr << "JS DOM arrays should contain only objects\n";
		exit(9);
	}
	child = JSVAL_TO_OBJECT(v);
	return child;
}				/* get_array_element_object */

static void
set_array_element_object(JS::HandleObject parent, int idx,
			 JS::HandleObject child)
{
	js::RootedValue v(jcx);
	JSBool found;
	v = OBJECT_TO_JSVAL(child);
	JS_HasElement(jcx, parent, idx, &found);
	if (found) {
		if (JS_SetElement(jcx, parent, idx, v.address()) == JS_FALSE)
			misconfigure();
	} else {
		if (JS_DefineElement(jcx, parent, idx, v, NULL, NULL, PROP_STD)
		    == JS_FALSE)
			misconfigure();
	}
}				/* set_array_element_object */

static ej_proptype val_proptype(JS::HandleValue v)
{
	JS::RootedObject child(jcx);
	unsigned length;

	if (JSVAL_IS_STRING(v))
		return EJ_PROP_STRING;
	if (JSVAL_IS_INT(v))
		return EJ_PROP_INT;
	if (JSVAL_IS_DOUBLE(v))
		return EJ_PROP_FLOAT;
	if (JSVAL_IS_BOOLEAN(v))
		return EJ_PROP_BOOL;

	if (v.isObject()) {
		child = JSVAL_TO_OBJECT(v);
		if (JS_ObjectIsFunction(jcx, child))
			return EJ_PROP_FUNCTION;
/* is there a better way to test for array? */
		if (JS_GetArrayLength(jcx, child, &length) == JS_TRUE)
			return EJ_PROP_ARRAY;
		return EJ_PROP_OBJECT;
	}

	return EJ_PROP_NONE;	/* don't know */
}				/* val_proptype */

static ej_proptype find_proptype(JS::HandleObject parent, const char *name)
{
	js::RootedValue v(jcx);
	if (JS_GetProperty(jcx, parent, name, v.address()) == JS_FALSE)
		return EJ_PROP_NONE;
	return val_proptype(v);
}				/* find_proptype */

/* run a javascript function and return the result.
 * If the result is an object then the pointer, as a string, is returned.
 * The string is always allocated, you must free it. */
static char *run_function(JS::HandleObject parent, const char *name)
{
	js::RootedValue v(jcx);
	JSBool found;
	bool rc;
	const char *s;

	proptype = EJ_PROP_NONE;
	JS_HasProperty(jcx, parent, name, &found);
	if (!found)
		return NULL;

	rc = JS_CallFunctionName(jcx, parent, name, 0, emptyArgs, v.address());
	if (!rc)
		return NULL;

	proptype = val_proptype(v);
	if (v.isObject())
		s = pointerString(JSVAL_TO_OBJECT(v));
	else
		s = stringize(v);
	return cloneString(s);
}				/* run_function */

/* process each message from edbrowse and respond appropriately */
static void processMessage(void)
{
	JSAutoRequest autoreq(jcx);
	JSAutoCompartment ac(jcx, winobj);
/* head.obj should be a valid object or 0 */
	JS::RootedObject parent(jcx, (JSObject *) head.obj);
	JS::RootedObject child(jcx);
	JSObject *chp;
	js::RootedValue v(jcx);
	const char *s;
	bool rc, setret;

	switch (head.cmd) {
	case EJ_CMD_SCRIPT:
		rc = false;
		s = runscript;
/* Sometimes Mac puts these three chars at the start of a text file. */
		if (!strncmp(s, "\xef\xbb\xbf", 3))
			s += 3;
		if (JS_EvaluateScript(jcx, parent, s, strlen(s),
				      "foo", head.lineno, v.address())) {
			rc = true;
			if (JSVAL_IS_BOOLEAN(v))
				rc = JSVAL_TO_BOOLEAN(v);
		}
		head.n = rc;
		nzFree(runscript);
		runscript = 0;
		head.proplength = 0;
		writeHeader();
		break;

	case EJ_CMD_HASPROP:
		head.proptype = find_proptype(parent, membername);
		nzFree(membername);
		membername = 0;
		head.n = head.proplength = 0;
		writeHeader();
		break;

	case EJ_CMD_DELPROP:
		JS_DeleteProperty(jcx, parent, membername);
		nzFree(membername);
		membername = 0;
		head.n = head.proplength = 0;
		writeHeader();
		break;

	case EJ_CMD_GETPROP:
		propval = get_property_string(parent, membername);
		nzFree(membername);
		membername = 0;
		head.n = head.proplength = 0;
		if (propval)
			head.proplength = strlen(propval);
		writeHeader();
		if (propval)
			writeToEb(propval, head.proplength);
		nzFree(propval);
		propval = 0;
		break;

	case EJ_CMD_SETPROP:
/* does this set_property return something? */
		setret = false;
		if (head.proptype == EJ_PROP_ARRAY
		    || head.proptype == EJ_PROP_INSTANCE)
			setret = true;
		set_property_generic(parent, membername);
		nzFree(membername);
		membername = 0;
		head.n = head.proplength = 0;
		if (setret) {
			if (propval)
				head.proplength = strlen(propval);
		} else {
			nzFree(propval);
			propval = 0;
		}
		writeHeader();
		if (setret && propval) {
			writeToEb(propval, head.proplength);
			nzFree(propval);
			propval = 0;
		}
		break;

	case EJ_CMD_GETAREL:
		child = get_array_element_object(parent, head.n);
		propval = 0;	/* should already be 0 */
		head.proplength = 0;
		if (child) {
			propval = cloneString(pointerString(*child.address()));
			head.proplength = strlen(propval);
		}
		writeHeader();
		if (propval)
			writeToEb(propval, head.proplength);
		nzFree(propval);
		propval = 0;
		break;

	case EJ_CMD_SETAREL:
		if (propval) {
			sscanf(propval, "%p", &chp);
			child = chp;
			set_array_element_object(parent, head.n, child);
			nzFree(propval);
			propval = 0;
		}
		head.proplength = 0;
		writeHeader();
		break;

	case EJ_CMD_CALL:
		propval = run_function(parent, membername);
		nzFree(membername);
		membername = 0;
		head.proplength = head.n = 0;
		if (propval)
			head.proplength = strlen(propval);
		head.proptype = proptype;
		writeHeader();
		if (propval)
			writeToEb(propval, head.proplength);
		nzFree(propval);
		propval = 0;
		break;

	default:
		cerr << "Unexpected message command " << head.cmd <<
		    " from edbrowse\n";
		exit(6);
	}
}				/* processMessage */
