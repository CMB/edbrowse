/* url.c
 * (c) 2002 Mikulas Patocka
 * This file is part of the Links project, released under GPL.
 *
 * Modified by Karl Dahlke for integration with edbrowse.
 */

#include "eb.h"

struct {
    char *prot;
    int port;
    bool free_syntax;
    bool need_slashes;
    bool need_slash_after_host;
} protocols[] = {
    {
    "file", 0, true, true, false}, {
    "http", 80, false, true, true}, {
    "https", 443, false, true, true}, {
    "proxy", 3128, false, true, true}, {
    "ftp", 21, false, true, true}, {
    "rtsp", 554, false, true, true}, {
    "pnm", 7070, false, true, true}, {
    "finger", 79, false, true, true}, {
    "smb", 139, false, true, true}, {
    "mailto", 0, false, false, false}, {
    "telnet", 23, false, false, false}, {
    "tn3270", 0, false, false, false}, {
    "javascript", 0, true, false, false}, {
    NULL, 0}
};

static bool free_syntax;

static int
protocolByName(const char *p, int l)
{
    int i;
    for(i = 0; protocols[i].prot; i++)
	if(memEqualCI(protocols[i].prot, p, l))
	    return i;
    return -1;
}				/* protocolByName */

/* Unpercent the host component of a url.  Christ what a pain! */
void
unpercentURL(char *url)
{
    char c, *u, *w;
    int n;
    u = w = url;
    while(c = *u) {
	++u;
	if(c == '%' && isxdigit(u[0]) && isxdigit(u[1])) {
	    c = fromHex(u[0], u[1]);
	    u += 2;
	}
	if(!c)
	    c = ' ';		/* should never happen */
	*w++ = c;
	if(strchr("?#\1", c))
	    break;
	if(c != '/')
	    continue;
	n = w - url;
	if(n == 1 || n > 16)
	    break;
	if(w[-2] != ':' && w[-2] != '/')
	    break;
    }
    strcpy(w, u);
}				/* unpercentURL */

/* Decide if it looks like a web url. */
static bool
httpDefault(const char *url)
{
    static const char *const domainSuffix[] = {
	"com", "biz", "info", "net", "org", "gov", "edu", "us", "uk", "au",
	"ca", "de", "jp", "nz", 0
    };
    int n, len;
    const char *s, *lastdot, *end = url + strcspn(url, "/?#\1");
    if(end - url > 7 && stringEqual(end - 7, ".browse"))
	end -= 7;
    s = strrchr(url, ':');
    if(s && s < end) {
	const char *colon = s;
	++s;
	while(isdigitByte(*s))
	    ++s;
	if(s == end)
	    end = colon;
    }
/* need at least two embedded dots */
    n = 0;
    for(s = url + 1; s < end - 1; ++s)
	if(*s == '.' && s[-1] != '.' && s[1] != '.')
	    ++n, lastdot = s;
    if(n < 2)
	return false;
/* All digits, like an ip address, is ok. */
    if(n == 3) {
	for(s = url; s < end; ++s)
	    if(!isdigitByte(*s) && *s != '.')
		break;
	if(s == end)
	    return true;
    }
/* Look for standard domain suffix */
    ++lastdot;
    len = end - lastdot;
    for(n = 0; domainSuffix[n]; ++n)
	if(memEqualCI(lastdot, domainSuffix[n], len) && !domainSuffix[n][len])
	    return true;
/* www.anything.xx is ok */
    if(len == 2 && memEqualCI(url, "www.", 4))
	return true;
    return false;
}				/* httpDefault */

static int
parseURL(const char *url, const char **proto, int *prlen, const char **user, int *uslen, const char **pass, int *palen,	/* ftp protocol */
   const char **host, int *holen,
   const char **portloc, int *port,
   const char **data, int *dalen, const char **post)
{
    const char *p, *q;
    int a;

    if(proto)
	*proto = NULL;
    if(prlen)
	*prlen = 0;
    if(user)
	*user = NULL;
    if(uslen)
	*uslen = 0;
    if(pass)
	*pass = NULL;
    if(palen)
	*palen = 0;
    if(host)
	*host = NULL;
    if(holen)
	*holen = 0;
    if(portloc)
	*portloc = 0;
    if(port)
	*port = 0;
    if(data)
	*data = NULL;
    if(dalen)
	*dalen = 0;
    if(post)
	*post = NULL;
    free_syntax = false;

    if(!url)
	return -1;

/* Find the leading protocol:// */
    a = -1;
    p = strchr(url, ':');
    if(p) {
/* You have to have something after the colon */
	q = p + 1;
	if(*q == '/')
	    ++q;
	if(*q == '/')
	    ++q;
	while(isspaceByte(*q))
	    ++q;
	if(!*q)
	    return false;
	a = protocolByName(url, p - url);
    }
    if(a >= 0) {
	if(proto)
	    *proto = url;
	if(prlen)
	    *prlen = p - url;
	if(p[1] != '/' || p[2] != '/') {
	    if(protocols[a].need_slashes) {
		if(p[1] != '/') {
		    setError("%s:// expected", protocols[a].prot);
		    return -1;
		}
/* We got one out of two slashes, I'm going to call it good */
		++p;
	    }
	    p -= 2;
	}
	p += 3;
    } else {			/* nothing yet */
	if(p && p - url < 12 && p[1] == '/') {
	    for(q = url; q < p; ++q)
		if(!isalphaByte(*q))
		    break;
	    if(q == p) {	/* some protocol we don't know */
		char qprot[12];
		memcpy(qprot, url, p - url);
		qprot[p - url] = 0;
		setError("unrecognized protocol %s", qprot);
		return -1;
	    }
	}
	if(httpDefault(url)) {
	    static const char http[] = "http://";
	    if(proto)
		*proto = http;
	    if(prlen)
		*prlen = 4;
	    a = 1;
	    p = url;
	}
    }

    if(a < 0)
	return false;

    if(free_syntax = protocols[a].free_syntax) {
	if(data)
	    *data = p;
	if(dalen)
	    *dalen = strlen(p);
	return true;
    }

    q = p + strcspn(p, "@?#/\1");
    if(*q == '@') {		/* user:password@host */
	const char *pp = strchr(p, ':');
	if(!pp || pp > q) {	/* no password */
	    if(user)
		*user = p;
	    if(uslen)
		*uslen = q - p;
	} else {
	    if(user)
		*user = p;
	    if(uslen)
		*uslen = pp - p;
	    if(pass)
		*pass = pp + 1;
	    if(palen)
		*palen = q - pp - 1;
	}
	p = q + 1;
    }
    /* @ */
    q = p + strcspn(p, ":?#/\1");
    if(host)
	*host = p;
    if(holen)
	*holen = q - p;
    if(*q == ':') {		/* port specified */
	int n;
	const char *cc, *pp = q + strcspn(q, "/?#\1");
	n = strtol(q + 1, (char **)&cc, 10);
	if(cc != pp || !isdigitByte(q[1])) {
	    setError("invalid :port specifier at the end of the domain");
	    return -1;
	}
	if(port)
	    *port = n;
	if(portloc)
	    *portloc = q;
	q = pp;			/* up to the slash */
    } else {
	if(port)
	    *port = protocols[a].port;
    }				/* colon or not */

/* Skip past /, but not ? or # */
    if(*q == '/')
	q++;
    p = q;

/* post data is handled separately */
    q = p + strcspn(p, "\1");
    if(data)
	*data = p;
    if(dalen)
	*dalen = q - p;
    if(post)
	*post = *q ? q + 1 : NULL;
    return true;
}				/* parseURL */

bool
isURL(const char *url)
{
    int j = parseURL(url, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    if(j < 0)
	return false;
    return j;
}				/* isURL */

/* Helper functions to return pieces of the URL.
 * Makes a copy, so you can have your 0 on the end.
 * Return 0 for an error, and "" if that piece is missing. */

const char *
getProtURL(const char *url)
{
    static char buf[12];
    int l;
    const char *s;
    int rc = parseURL(url, &s, &l, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    if(rc <= 0)
	return 0;
    memcpy(buf, s, l);
    buf[l] = 0;
    return buf;
}				/* getProtURL */

static char hostbuf[400];
const char *
getHostURL(const char *url)
{
    int l;
    const char *s;
    char *t;
    char c, d;
    int rc = parseURL(url, 0, 0, 0, 0, 0, 0, &s, &l, 0, 0, 0, 0, 0);
    if(rc <= 0)
	return 0;
    if(free_syntax)
	return 0;
    if(!s)
	return EMPTYSTRING;
    if(l >= sizeof (hostbuf)) {
	setError("domain name too long");
	return 0;
    }
    memcpy(hostbuf, s, l);
    if(l && hostbuf[l - 1] == '.')
	--l;
    hostbuf[l] = 0;
/* domain names must be ascii, with no spaces */
    d = 0;
    for(s = t = hostbuf; (c = *s); ++s) {
	c &= 0x7f;
	if(c == ' ')
	    continue;
	if(c == '.' && d == '.')
	    continue;
	*t++ = d = c;
    }
    *t = 0;
    return hostbuf;
}				/* getHostURL */

const char *
getHostPassURL(const char *url)
{
    int hl;
    const char *h, *z, *u;
    char *t;
    int rc = parseURL(url, 0, 0, &u, 0, 0, 0, &h, &hl, 0, 0, 0, 0, 0);
    if(rc <= 0 || !h)
	return 0;
    if(free_syntax)
	return 0;
    z = h;
    t = hostbuf;
    if(u)
	z = u, hl += h - u, t += h - u;
    if(hl >= sizeof (hostbuf)) {
	setError("domain name too long");
	return 0;
    }
    memcpy(hostbuf, z, hl);
    hostbuf[hl] = 0;
/* domain names must be ascii */
    for(; *t; ++t)
	*t &= 0x7f;
    return hostbuf;
}				/* getHostPassURL */

const char *
getUserURL(const char *url)
{
    static char buf[MAXUSERPASS];
    int l;
    const char *s;
    int rc = parseURL(url, 0, 0, &s, &l, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    if(rc <= 0)
	return 0;
    if(free_syntax)
	return EMPTYSTRING;
    if(!s)
	return EMPTYSTRING;
    if(l >= sizeof (buf)) {
	setError("user name too long");
	return 0;
    }
    memcpy(buf, s, l);
    buf[l] = 0;
    return buf;
}				/* getUserURL */

const char *
getPassURL(const char *url)
{
    static char buf[MAXUSERPASS];
    int l;
    const char *s;
    int rc = parseURL(url, 0, 0, 0, 0, &s, &l, 0, 0, 0, 0, 0, 0, 0);
    if(rc <= 0)
	return 0;
    if(free_syntax)
	return EMPTYSTRING;
    if(!s)
	return EMPTYSTRING;
    if(l >= sizeof (buf)) {
	setError("password too long");
	return 0;
    }
    memcpy(buf, s, l);
    buf[l] = 0;
    return buf;
}				/* getPassURL */

const char *
getDataURL(const char *url)
{
    const char *s;
    int rc = parseURL(url, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, &s, 0, 0);
    if(rc <= 0)
	return 0;
    return s;
}				/* getDataURL */

void
getDirURL(const char *url, const char **start_p, const char **end_p)
{
    const char *dir = getDataURL(url);
    const char *end;
    static const char myslash[] = "/";
    if(!dir || dir == url)
	goto slash;
    if(free_syntax)
	goto slash;
    if(!strchr("#?\1", *dir)) {
	if(*--dir != '/')
	    errorPrint("1misplaced / in getDirURL(%s)", url);
    }
    end = strpbrk(dir, "#?\1");
    if(!end)
	end = dir + strlen(dir);
    while(end > dir && end[-1] != '/')
	--end;
    if(end > dir) {
	*start_p = dir;
	*end_p = end;
	return;
    }
  slash:
    *start_p = myslash;
    *end_p = myslash + 1;
}				/* getDirURL */

bool
getPortLocURL(const char *url, const char **portloc, int *port)
{
    int rc = parseURL(url, 0, 0, 0, 0, 0, 0, 0, 0, portloc, port, 0, 0, 0);
    if(rc <= 0)
	return false;
    if(free_syntax)
	return false;
    return true;
}				/* getPortLocURL */

int
getPortURL(const char *url)
{
    int port;
    int rc = parseURL(url, 0, 0, 0, 0, 0, 0, 0, 0, 0, &port, 0, 0, 0);
    if(rc <= 0)
	return 0;
    if(free_syntax)
	return 0;
    return port;
}				/* getPortURL */

bool
isProxyURL(const char *url)
{
    return ((url[0] | 0x20) == 'p');
}

/* Don't let a web page fetch itself. */
static char *histURL[MAXFETCH];
static int histFrom[MAXFETCH];
static int n_fetch;
int
fetchHistory(const char *prev, const char *next)
{
    int i;
    int from = -1;
/* zero is a reset */
    debugPrint(4, "fetch hist %s : %s", prev, next);
    if(prev == 0) {
	for(i = 0; i < n_fetch; ++i)
	    free(histURL[i]);
	n_fetch = 0;
	if(!next)
	    return false;
    } else {
	if(memEqualCI(prev, "http://", 7))
	    prev += 7;
	for(i = 0; i < n_fetch; ++i)
	    if(stringEqual(prev, histURL[i])) {
		from = i;
		break;
	    }
    }
    if(n_fetch >= MAXFETCH) {
	setError
	   ("too many fetches from the internet, you may want to disable `redirect html'");
	return -1;
    }
/* Have we seen this one before? */
    if(memEqualCI(next, "http://", 7))
	next += 7;
    for(i = 0; i < n_fetch; ++i)
	if(stringEqual(next, histURL[i]))
	    break;
    if(i == n_fetch) {		/* new */
	histURL[i] = cloneString(next);
	histFrom[i] = from;
	++n_fetch;
	return true;
    }
/* Oops, we've already fetched this page. */
    while(from >= 0 && from != i)
	from = histFrom[from];
    if(from < 0)
	return false;
    setError("web page indirectly fetches itself, an infinite loop");
    return -1;
}				/* FetchHistory */

const char *
firstURL(void)
{
    if(!n_fetch)
	return 0;
    return histURL[0];
}				/* firstURL */

static void
squashDirectories(char *url)
{
    char *dd = (char *)getDataURL(url);
    char *s, *t, *end;
    if(memEqualCI(url, "javascript:", 11))
	return;
    if(!dd || dd == url)
	return;
    if(!*dd)
	return;
    if(strchr("#?\1", *dd))
	return;
    --dd;
    if(*dd != '/')
	errorPrint("@misplaced / in %s", url);
    end = dd + strcspn(dd, "?#\1");
    while(true) {
	s = strstr(dd, "/./");
	if(s && s < end) {
	    strcpy(s, s + 2);
	    continue;
	}
	s = strstr(dd, "/../");
	if(!s)
	    break;
	if(s > end)
	    break;
	if(s == dd) {
	    strcpy(s, s + 3);
	    continue;
	}
	for(t = s - 1; *t != '/'; --t) ;
	s += 3;
	strcpy(t, s);
    }
}				/* squashDirectories */

char *
resolveURL(const char *base, const char *rel)
{
    char *n;			/* new url */
    const char *s, *p;
    char *q;
    int l;
    if(!base)
	base = EMPTYSTRING;
    n = allocMem(strlen(base) + strlen(rel) + 12);
    debugPrint(5, "resolve(%s|%s)", base, rel);
    if(rel[0] == '#') {
/* # alone means do nothing. */
	if(!rel[1]) {
	    strcpy(n, rel);
	  out_n:
	    debugPrint(5, "= %s", n);
	    return n;
	}
/* We could have changed the base url via the <base> tag,
 * so this #ref could actually refer to some other web page.
 * Best to run through standard procedure. */
	strcpy(n, base);
	for(q = n; *q && *q != '\1' && *q != '#'; q++) ;
	strcpy(q, rel);
	goto out_n;
    }
    if(rel[0] == '?' || rel[0] == '\1') {
	strcpy(n, base);
	for(q = n; *q && *q != '\1' && *q != '#' && *q != '?'; q++) ;
	strcpy(q, rel);
	goto out_n;
    }
    if(rel[0] == '/' && rel[1] == '/') {
	if(s = strstr(base, "//")) {
	    strncpy(n, base, s - base);
	    n[s - base] = 0;
	} else
	    strcpy(n, "http:");
	strcat(n, rel);
	goto squash;
    }
    if(parseURL(rel, &s, &l, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) > 0) {
	n[0] = 0;
	if(s != rel) {
/* It didn't have http in front of it before, put it on now. */
	    strncpy(n, s, l);
	    strcpy(n + l, "://");
	}
	strcat(n, rel);
	goto squash;
    }				/* relative is already a url */
    s = base;
    if(rel[0] == '/') {
	s = getDataURL(base);
	if(!s) {
	    strcpy(n, rel);
	    goto squash;
	}
	if(!*s) {
	    if(s - base >= 7 && stringEqual(s - 7, ".browse"))
		s -= 7;
	    if(s > base && s[-1] == '/')
		--s;
	} else if(!strchr("#?\1", *s))
	    --s;
	l = s - base;
	strncpy(n, base, l);
	strcpy(n + l, rel);
	goto squash;
    }
/* This is a relative change, paste it on after the last slash */
    s = base;
    if(parseURL(base, 0, 0, 0, 0, 0, 0, &p, 0, 0, 0, 0, 0, 0) > 0 && p)
	s = p;
    for(p = 0; *s; ++s) {
	if(*s == '/')
	    p = s;
	if(strchr("#?\1", *s))
	    break;
    }
    if(!p) {
	if(isURL(base))
	    p = s;
	else
	    p = base;
    }
    l = p - base;
    if(l) {
	strncpy(n, base, l);
	n[l++] = '/';
    }
    strcpy(n + l, rel);
  squash:
    squashDirectories(n);
    goto out_n;
}				/* resolveURL */

/* This routine could be, should be, more sophisticated */
bool
sameURL(const char *s, const char *t)
{
    const char *u, *v;
    int l;
/* It's ok if one says http and the other implies it. */
    if(memEqualCI(s, "http://", 7))
	s += 7;
    if(memEqualCI(t, "http://", 7))
	t += 7;
    u = s + strcspn(s, "#");
    v = t + strcspn(t, "#?\1");
    if(u - s >= 7 && stringEqual(u - 7, ".browse"))
	u -= 7;
    if(v - t >= 7 && stringEqual(v - 7, ".browse"))
	v -= 7;
    l = u - s;
    if(l != v - t)
	return false;
    return !memcmp(s, t, l);
}				/* sameURL */

/* Find some helpful text to print, in place of an image. */
/* Text longer than 80 chars isn't helpful, so we return a static buffer. */
char *
altText(const char *base)
{
    static char buf[80];
    int len, n;
    int recount = 0;
    char *s;
    debugPrint(5, "altText(%s)", base);
    if(!base)
	return 0;
    if(stringEqual(base, "#"))
	return 0;
    if(memEqualCI(base, "javascript", 10))
	return 0;
  retry:
    if(recount >= 2)
	return 0;
    strncpy(buf, base, sizeof (buf) - 1);
    spaceCrunch(buf, true, true);
    len = strlen(buf);
    if(len && !isalnumByte(buf[len - 1]))
	buf[--len] = 0;
    while(len && !isalnumByte(buf[0]))
	strcpy(buf, buf + 1), --len;
    if(len > 10) {
/* see if it's a phrase/sentence or a pathname/url */
/* Do this by counting spaces */
	for(n = 0, s = buf; *s; ++s)
	    if(*s == ' ')
		++n;
	if(n * 8 >= len)
	    return buf;		/* looks like words */
/* Ok, now we believe it's a pathname or url */
/* Get rid of everything after ? or # */
	s = strpbrk(buf, "#?\1");
	if(s)
	    *s = 0;
/* get rid of common suffix */
	s = strrchr(buf, '.');
	if(s) {
/* get rid of trailing .html */
	    static const char *const suffix[] = {
		"html", "htm", "shtml", "shtm", "php", "asp", "cgi", "rm",
		"ram",
		"gif", "jpg", "bmp",
		0
	    };
	    n = stringInListCI(suffix, s + 1);
	    if(n >= 0 || s[1] == 0)
		*s = 0;
	}
/* Get rid of everything up to the last slash, leaving the file name */
	s = strrchr(buf, '/');
	if(s && recount) {
	    char *ss;
	    *s = 0;
	    ss = strrchr(buf, '/');
	    if(!ss)
		return 0;
	    if(ss > buf && ss[-1] == '/')
		return 0;
	    *s = '/';
	    s = ss;
	}
	if(s)
	    strcpy(buf, s + 1);
    }				/* more than ten characters */
    ++recount;
/* If we don't have enough letters, forget it */
    len = strlen(buf);
    if(len < 3)
	goto retry;
    for(n = 0, s = buf; *s; ++s)
	if(isalphaByte(*s))
	    ++n;
    if(n * 2 <= len)
	return 0;		/* not enough letters */
    return buf;
}				/* altText */

/* get post data ready for a url. */
char *
encodePostData(const char *s)
{
    char *post, c;
    int l;
    char buf[4];

    if(!s)
	return 0;
    if(s == EMPTYSTRING)
	return EMPTYSTRING;
    post = initString(&l);
    while(c = *s++) {
	if(isalnumByte(c))
	    goto putc;
	if(c == ' ') {
	    c = '+';
	    goto putc;
	}
	if(strchr("-._~*()!", c))
	    goto putc;
	sprintf(buf, "%%%02X", (uchar) c);
	stringAndString(&post, &l, buf);
	continue;
      putc:
	stringAndChar(&post, &l, c);
    }
    return post;
}				/* encodePostData */

static char
dohex(char c, const char **sp)
{
    const char *s = *sp;
    char d, e;
    if(c == '+')
	return ' ';
    if(c != '%')
	return c;
    d = *s++;
    e = *s++;
    if(!isxdigit(d) || !isxdigit(e))
	return c;		/* should never happen */
    d = fromHex(d, e);
    if(!d)
	d = ' ';		/* don't allow nulls */
    *sp = s;
    return d;
}				/* dohex */

char *
decodePostData(const char *data, const char *name, int seqno)
{
    const char *s, *n, *t;
    char *new = 0, *w = 0;
    int j = 0;
    char c;

    if(!seqno && !name)
	errorPrint("@decodePostData(0,0)");

    for(s = data; *s; s = (*t ? t + 1 : t)) {
	n = 0;
	t = strchr(s, '&');
	if(!t)
	    t = s + strlen(s);
/* select attribute by number */
	++j;
	if(j == seqno)
	    w = new = allocMem(t - s + 1);
	if(seqno && !w)
	    continue;
	if(name)
	    n = name;
	while(s < t && (c = *s) != '=') {
	    ++s;
	    c = dohex(c, &s);
	    if(n) {
/* I don't know if this is suppose to be case insensitive all the time,
 * though there are situations when it must be, as in
 * mailto:address?Subject=blah-blah */
		if(isalphaByte(c)) {
		    if(!((c ^ *n) & 0xdf))
			++n;
		    else
			n = 0;
		} else if(c == *n)
		    ++n;
		else
		    n = 0;
	    }
	    if(w)
		*w++ = c;
	}

	if(s == t) {		/* no equals, just a string */
	    if(name)
		continue;
	    *w = 0;
	    return new;
	}
	if(w)
	    *w++ = c;
	++s;			/* skip past equals */
	if(name) {
	    if(!n)
		continue;
	    if(*n)
		continue;
	    w = new = allocMem(t - s + 1);
	}

/* At this point we have a match */
	while(s < t) {
	    c = *s++;
	    c = dohex(c, &s);
	    *w++ = c;
	}
	*w = 0;
	return new;
    }

    return 0;
}				/* decodePostData */

void
decodeMailURL(const char *url, char **addr_p, char **subj_p, char **body_p)
{
    const char *s;
    char *new;
    if(memEqualCI(url, "mailto:", 7))
	url += 7;
    s = url + strcspn(url, "/?");
    if(addr_p)
	*addr_p = pullString1(url, s);
    if(subj_p)
	*subj_p = 0;
    if(body_p)
	*body_p = 0;
    s = strchr(url, '?');
    if(!s)
	return;
    url = s + 1;
    if(subj_p)
	*subj_p = decodePostData(url, "subject", 0);
    if(body_p)
	*body_p = decodePostData(url, "body", 0);
}				/* decodeMailURL */
