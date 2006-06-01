/* cookies.c
 * Cookies
 * (c) 2002 Mikulas Patocka
 * This file is part of the Links project, released under GPL
 *
 * Modified by Karl Dahlke for integration with edbrowse.
 */

#include "eb.h"

struct cookie {
    struct cookie *next;
    struct cookie *prev;
/* These are allocated */
    char *name, *value;
    char *server, *path, *domain;
    time_t expires;		/* zero means undefined */
    bool secure;
};

static void
freeCookie(struct cookie *c)
{
    nzFree(c->name);
    nzFree(c->value);
    nzFree(c->server);
    nzFree(c->path);
    nzFree(c->domain);
}				/* freeCookie */

static struct listHead cookies = { &cookies, &cookies };

static bool displacedCookie;
static void
acceptCookie(struct cookie *c)
{
    struct cookie *d;
    displacedCookie = false;
    foreach(d, cookies) {
	if(stringEqualCI(d->name, c->name) &&
	   stringEqualCI(d->domain, c->domain)) {
	    displacedCookie = true;
	    delFromList(d);
	    freeCookie(d);
	    nzFree(d);
	    break;
	}
    }
    addToListBack(&cookies, c);
}				/* acceptCookie */

static void
cookieIntoJar(const struct cookie *c)
{
    FILE *f;
    if(!cookieFile) {
	static bool warn = false;
	if(warn)
	    return;
	puts("your config file does not specify a cooky jar");
	warn = true;
	return;
    }
    if(c->expires <= time(0))
	return;			/* not persistent, or out of date */
    f = fopen(cookieFile, "a");
    if(!f)
	return;
/* Netscape format */
/* I have no clue what the second argument is suppose to be, */
/* I'm always calling it false. */
    fprintf(f, "%s\tFALSE\t%s\t%s\t%u\t%s\t%s\n",
       c->domain, c->path,
       c->secure ? "TRUE" : "FALSE", (unsigned)c->expires, c->name, c->value);
    fclose(f);
    debugPrint(3, "into jar");
}				/* cookieIntoJar */

/* Should this server really specify this domain in a cookie? */
/* Domain must be the trailing substring of server. */
bool
domainSecurityCheck(const char *server, const char *domain)
{
    int i, dl, nd;
    dl = strlen(domain);
/* x.com or x.y.z */
    if(dl < 5)
	return false;
    if(dl > strlen(server))
	return false;
    i = strlen(server) - dl;
    if(!stringEqualCI(server + i, domain))
	return false;
    if(i && server[i - 1] != '.')
	return false;
    nd = 2;			/* number of dots */
    if(dl > 4 && domain[dl - 4] == '.') {
	static const char *const tld[] = {
	    "com", "edu", "net", "org", "gov", "mil", "int", "biz", NULL
	};
	if(stringInListCI(tld, domain + dl - 3) >= 0)
	    nd = 1;
    }
    for(i = 0; domain[i]; i++)
	if(domain[i] == '.')
	    if(!--nd)
		return true;
    return false;
}				/* domainSecurityCheck */

/* Let's jump right into it - parse a cookie, as received from a website. */
bool
receiveCookie(const char *url, const char *str)
{
    struct cookie *c;
    const char *p, *q, *server;
    char *date, *s;

    debugPrint(3, "%s", str);

    server = getHostURL(url);
    if(server == 0 || !*server)
	return false;

/* Cookie starts with name=value.  If we can't get that, go home. */
    for(p = str; *p != ';' && *p; p++) ;
    for(q = str; *q != '='; q++)
	if(!*q || q >= p)
	    return false;
    if(str == q || q + 1 == p)
	return false;

    c = allocZeroMem(sizeof (struct cookie));
    c->name = pullString1(str, q);
    ++q;
    c->value = pullString1(q, p);

    c->server = cloneString(server);

    if(date = extractHeaderParam(str, "expires")) {
	c->expires = parseHeaderDate(date);
	nzFree(date);
    } else if(date = extractHeaderParam(str, "max-age")) {
	int n = stringIsNum(date);
	if(n >= 0) {
	    time_t now = time(0);
	    c->expires = now + n;
	}
	nzFree(date);
    }

    c->path = extractHeaderParam(str, "path");
    if(!c->path) {
/* The url indicates the path for this cookie, if a path is not explicitly given */
	const char *dir, *dirend;
	getDirURL(url, &dir, &dirend);
	c->path = pullString1(dir, dirend);
    } else {
	if(!c->path[0] || c->path[strlen(c->path) - 1] != '/')
	    c->path = appendString(c->path, "/");
	if(c->path[0] != '/')
	    c->path = prependString(c->path, "/");
    }

    if(!(c->domain = extractHeaderParam(str, "domain")))
	c->domain = cloneString(server);
    if(c->domain[0] == '.')
	strcpy(c->domain, c->domain + 1);
    if(!domainSecurityCheck(server, c->domain)) {
	nzFree(c->domain);
	c->domain = cloneString(server);
    }

    if(s = extractHeaderParam(str, "secure")) {
	c->secure = true;
	nzFree(s);
    }

    acceptCookie(c);
    cookieIntoJar(c);
    return true;
}				/* receiveCookie */

/* I'm assuming I can read the cookie file, process it,
 * and if necessary, write it out again, with the expired cookies deleted,
 * all before another edbrowse process interferes.
 * I've given it some thought, and I think I can ignore the race conditions. */
void
cookiesFromJar(void)
{
    char *cbuf, *s, *t;
    FILE *f;
    int n, cnt, expired, displaced;
    time_t now;
    struct cookie *c;

    if(!cookieFile)
	return;
    if(!fileIntoMemory(cookieFile, &cbuf, &n))
	showErrorAbort();
    cbuf[n] = 0;
    time(&now);

    cnt = expired = displaced = 0;
    s = cbuf;
    while(*s) {
	++cnt;
	c = allocZeroMem(sizeof (struct cookie));
	t = strchr(s, '\t');
	*t = 0;
	c->domain = cloneString(s);
	s = t + 1;
	t = strchr(s, '\t');
	s = t + 1;
	t = strchr(s, '\t');
	*t = 0;
	c->path = cloneString(s);
	s = t + 1;
	t = strchr(s, '\t');
	c->secure = (*s == 'T');
	s = t + 1;
	t = strchr(s, '\t');
	*t = 0;
	c->expires = (time_t) atol(s);
	s = t + 1;
	t = strchr(s, '\t');
	*t = 0;
	c->name = cloneString(s);
	s = t + 1;
	t = strchr(s, '\n');
	*t = 0;
	c->value = cloneString(s);
	s = t + 1;

	if(c->expires < now) {
	    freeCookie(c);
	    nzFree(c);
	    ++expired;
	} else {
	    acceptCookie(c);
	    displaced += displacedCookie;
	}
    }

    debugPrint(3, "%d persistent cookies, %d expired, %d displaced",
       cnt, expired, displaced);
    nzFree(cbuf);
    if(!(expired + displaced))
	return;

/* Pour the cookies back into the jar */
    f = fopen(cookieFile, "w");
    if(!f)
	errorPrint("1cannot rebuild your cookie jar %s", cookieFile);
    foreach(c, cookies)
       fprintf(f, "%s\tFALSE\t%s\t%s\t%u\t%s\t%s\n",
       c->domain, c->path,
       c->secure ? "TRUE" : "FALSE", (unsigned)c->expires, c->name, c->value);
    fclose(f);
}				/* cookiesFromJar */

static bool
isInDomain(const char *d, const char *s)
{
    int dl = strlen(d);
    int sl = strlen(s);
    int j = sl - dl;
    if(j < 0)
	return false;
    if(!memEqualCI(d, s + j, dl))
	return false;
    if(j && s[j - 1] != '.')
	return false;
    return true;
}				/* isInDomain */

static bool
isPathPrefix(const char *d, const char *s)
{
    int dl = strlen(d);
    int sl = strlen(s);
    if(dl > sl)
	return false;
    return !memcmp(d, s, dl);
}				/* isPathPrefix */

void
sendCookies(char **s, int *l, const char *url, bool issecure)
{
    const char *server = getHostURL(url);
    const char *data = getDataURL(url);
    int nc = 0;			/* new cookie */
    struct cookie *c, *d;
    time_t now;

    if(!url || !server || !data)
	return;

    if(data > url && data[-1] == '/')
	data--;
    if(!*data)
	data = "/";
    time(&now);

    foreach(c, cookies) {
	if(!isInDomain(c->domain, server))
	    continue;
	if(!isPathPrefix(c->path, data))
	    continue;
	if(c->expires && c->expires < now) {
	    d = c;
	    c = c->prev;
	    delFromList(d);
	    freeCookie(d);
	    nzFree(d);
	    continue;
	}
	if(c->secure && !issecure)
	    continue;
/* We're good to go. */
	if(!nc)
	    stringAndString(s, l, "Cookie: "), nc = 1;
	else
	    stringAndString(s, l, "; ");
	stringAndString(s, l, c->name);
	stringAndChar(s, l, '=');
	stringAndString(s, l, c->value);
	debugPrint(3, "send cookie %s=%s", c->name, c->value);
    }

    if(nc)
	stringAndString(s, l, eol);
}				/* sendCookies */
