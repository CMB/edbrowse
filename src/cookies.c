/* cookies.c
 * Cookies
 * (c) 2002 Mikulas Patocka
 * This file is part of the Links project, released under GPL
 *
 * Modified by Karl Dahlke for integration with edbrowse.
 * Modified by Chris Brannon to allow cooperation with libcurl.
 */

#include "eb.h"

struct cookie {
	struct cookie *next;
	struct cookie *prev;
/* These are allocated */
	char *name, *value;
	char *server, *path, *domain;
	bool tail;
/* tail is needed for libcurl, to tell it to tail-match. */
/* Why doesn't it just look for the damned dot at the front of the domain? */
	bool secure;
	bool fromjar;
	time_t expires;		/* zero means undefined */
};

static const char *httponly_prefix = "#HttpOnly_";
static const size_t httponly_prefix_len = 10;

static bool isComment(const char *line)
{
	return (line[0] == '#' &&
		strncmp(line, httponly_prefix, httponly_prefix_len) != 0);
}				/* isComment */

static int count_tabs(const char *the_string)
{
	int str_index = 0, num_tabs = 0;
	for (str_index = 0; the_string[str_index] != '\0'; str_index++)
		if (the_string[str_index] == '\t')
			num_tabs++;
	return num_tabs;
}				/* count_tabs */

// Extract an item from the cookie line; result is allocated.
static char *extractHeaderParam(const char *str, const char *item)
{
	int le = strlen(item), lp;
	const char *s = str;
/* ; denotes the next param */
/* Even the first param has to be preceeded by ; */
	while ((s = strchr(s, ';'))) {
		while (*s && (*s == ';' || (uchar) * s <= ' '))
			s++;
		if (!memEqualCI(s, item, le))
			continue;
		s += le;
		while (*s && ((uchar) * s <= ' ' || *s == '='))
			s++;
		if (!*s)
			return emptyString;
		lp = 0;
		while ((uchar) s[lp] >= ' ' && s[lp] != ';')
			lp++;
		return pullString(s, lp);
	}
	return NULL;
}				/* extractHeaderParam */

#define FIELDS_PER_COOKIE_LINE 7

static struct cookie *cookie_from_netscape_line(char *cookie_line)
{
	struct cookie *new_cookie = NULL;
	if (cookie_line && cookie_line[0]) {
/* Only parse the line if it is not a comment and it has the requisite number
 * of tabs.  Comment lines begin with a leading # symbol.
 * Syntax checking is rudimentary, because these lines are
 * machine-generated. */
		if (!isComment(cookie_line) &&
		    count_tabs(cookie_line) == FIELDS_PER_COOKIE_LINE - 1) {
			char *start, *end;
			new_cookie = allocZeroMem(sizeof(struct cookie));
			start = cookie_line;
			end = strchr(cookie_line, '\t');
			new_cookie->domain = pullString1(start, end);
			start = end + 1;
			end = strchr(start, '\t');
			if ((*start == 't') || (*start == 'T'))
				new_cookie->tail = true;
			else
				new_cookie->tail = false;
			start = end + 1;
			end = strchr(start, '\t');
			new_cookie->path = pullString1(start, end);
			start = end + 1;
			if (*start == 'T' || *start == 't')
				new_cookie->secure = true;
			else
				new_cookie->secure = false;
			start = strchr(start, '\t') + 1;
			new_cookie->expires = strtol(start, &end, 10);
/* Now end points to the tab following the expiration time. */
			start = end + 1;
			end = strchr(start, '\t');
			new_cookie->name = pullString1(start, end);
			start = end + 1;
/* strcspn gives count of non-newline characters in string, which is the
 * length of the final field.  Either CR or LF is considered a newline. */
			new_cookie->value =
			    pullString(start, strcspn(start, "\r\n"));
/* Whenever new_cookie->tail is true, there's going to be a dot at the front of the
 * domain name.  Libcurl even puts one there when it parses set-cookie
 * headers.  But let's be sure. */
			if (new_cookie->tail && (new_cookie->domain[0] != '.')
			    && strncmp(new_cookie->domain, httponly_prefix,
				       httponly_prefix_len))
				new_cookie->domain =
				    prependString(new_cookie->domain, ".");
		}
	}

	return new_cookie;
}				/* cookie_from_netscape_line */

static void freeCookie(struct cookie *c)
{
	nzFree(c->name);
	nzFree(c->value);
	nzFree(c->server);
	nzFree(c->path);
	nzFree(c->domain);
}				/* freeCookie */

static struct listHead cookies = { &cookies, &cookies };

/*
 * Construct a cookie line of the form used by Netscape's file format,
 * from a cookie c.  Returns dynamically-allocated memory, which the
 * caller must free. */
static char *netscapeCookieLine(const struct cookie *c)
{
/* Netscape format */
/*
 * The second field of a cookie file entry is not documented anywhere.
 * It's been a great mystery since 2001.  But if you look at the libcurl
 * source, you'll see that they use it.  A value of TRUE indicates that
 * the domain field should be tail-matched, and a value of FALSE indicates
 * that it should not. */
	const char *tailstr;
	char *cookLine =
	    allocMem(strlen(c->path) + strlen(c->domain) + strlen(c->name) +
		     strlen(c->value) + 128);
	if (c->tail)
		tailstr = "TRUE";
	else
		tailstr = "FALSE";
	sprintf(cookLine, "%s\t%s\t%s\t%s\t%u\t%s\t%s\n",
		c->domain, tailstr,
		c->path, c->secure ? "TRUE" : "FALSE", (unsigned)c->expires,
		c->name, c->value);
	return cookLine;
}

/* Tell libcurl about a new cookie.  Called when setting cookies from
 * JavaScript.
 * The function is pretty simple.  Construct a line of the form used by
 * the Netscape cookie file format, and pass that to libcurl.
 * Also called by mergeCookies() to bring other jar cookies into curl space,
 * but should we be doing debugPrints in that case? */
static CURLcode cookieForLibcurl(const struct cookie *c)
{
	CURLcode ret;
	char *cookLine = netscapeCookieLine(c);
	ret =
	    curl_easy_setopt(global_http_handle, CURLOPT_COOKIELIST, cookLine);
	nzFree(cookLine);
	return ret;
}				/* cookieForLibcurl */

/* Should this server really specify this domain in a cookie? */
/* Domain must be the trailing substring of server. */
bool domainSecurityCheck(const char *server, const char *domain)
{
	int i, dl, nd;
	dl = strlen(domain);
/* x.com or x.y.z */
	if (dl < 5)
		return false;
	if (dl > strlen(server))
		return false;
	i = strlen(server) - dl;
	if (!stringEqualCI(server + i, domain))
		return false;
	if (i && server[i - 1] != '.')
		return false;
	nd = 2;			/* number of dots */
	if (dl > 4 && domain[dl - 4] == '.') {
		static const char *const tld[] = {
			"com", "edu", "net", "org", "gov", "mil", "int", "biz",
			NULL
		};
		if (stringInListCI(tld, domain + dl - 3) >= 0)
			nd = 1;
	}
	for (i = 0; domain[i]; i++)
		if (domain[i] == '.')
			if (!--nd)
				return true;
	return false;
}				/* domainSecurityCheck */

/* Let's jump right into it - parse a cookie, as received from a website. */
bool receiveCookie(const char *url, const char *str)
{
	struct cookie *c;
	const char *p, *q, *server;
	char *date, *s;

	if (!curlActive)
		return false;
	debugPrint(3, "cookie %s", str);

	server = getHostURL(url);
	if (server == 0 || !*server)
		return false;

/* Cookie starts with name=value.  If we can't get that, go home. */
	for (p = str; *p != ';' && *p; p++) ;
	for (q = str; *q != '='; q++)
		if (!*q || q >= p)
			return false;
	if (str == q)
		return false;

	c = allocZeroMem(sizeof(struct cookie));
	c->tail = false;
	c->name = pullString1(str, q);
	++q;
	if (p - q > 0)
		c->value = pullString1(q, p);
	else
		c->value = emptyString;

	c->server = cloneString(server);

	if ((date = extractHeaderParam(str, "expires"))) {
		c->expires = parseHeaderDate(date);
		nzFree(date);
	} else if ((date = extractHeaderParam(str, "max-age"))) {
		int n = stringIsNum(date);
		if (n >= 0) {
			time_t now = time(0);
			c->expires = now + n;
		}
		nzFree(date);
	}

	c->path = extractHeaderParam(str, "path");
	if (!c->path) {
/* The url indicates the path for this cookie, if a path is not explicitly given */
		const char *dir, *dirend;
		getDirURL(url, &dir, &dirend);
		c->path = pullString1(dir, dirend);
	} else {
		if (!c->path[0] || c->path[strlen(c->path) - 1] != '/')
			c->path = appendString(c->path, "/");
		if (c->path[0] != '/')
			c->path = prependString(c->path, "/");
	}

	if (!(c->domain = extractHeaderParam(str, "domain"))) {
		c->domain = cloneString(server);
	} else {
/* Is this safe for tail-matching? */
		const char *domtemp = c->domain;
		if (domtemp[0] == '.')
			domtemp++;
		if (!domainSecurityCheck(server, domtemp)) {
			nzFree(c->domain);
			c->domain = cloneString(server);
		} else {
/* It's safe to do tail-matching with this domain. */
			c->tail = true;
/* Guarantee that it does in fact start with dot, prepending if necessary.. */
			if (c->domain[0] != '.')
				c->domain = prependString(c->domain, ".");
		}
	}

	if ((s = extractHeaderParam(str, "secure"))) {
		c->secure = true;
		nzFree(s);
	}

	cookieForLibcurl(c);
	freeCookie(c);
	nzFree(c);
	return true;
}				/* receiveCookie */

/*********************************************************************
This function is called at edbrowse startup.
It reads the cookies in the cookie jar and hands them to curl.
You'd think curl would just read and parse the file itself, but it doesn't.
It does however write the updated file on exit, including all the new cookies
it has acquired during the edbrowse session.
Why it writes at exit, but doesn't read at startup, I have no idea,
so we have to spoonfeed it the cookies at the start.
There's something else curl doesn't do, it doesn't delete
expired cookies, they just accumulate forever.
So we cull the old cookies from the cookie jar as we're passing them up to curl.
When it writes the jar at exit, the old cookies will be gone.
If spoonfeed = false, then we're just bringing the cookies from jar into memory,
and not handing them to curl.
This is done by mergeCookies().
*********************************************************************/

static bool spoonfeed = true;

void cookiesFromJar(void)
{
	char *cbuf, *s, *t;
	int n, cnt, expired;
	char *cbuf_end;
	time_t now;
	struct cookie *c;

	if (!cookieFile)
		return;
	if (!fileIntoMemory(cookieFile, &cbuf, &n))
		showErrorAbort();
	cbuf[n] = 0;
	cbuf_end = cbuf + n;
	time(&now);

	cnt = expired = 0;
	s = cbuf;

	while (s < cbuf_end) {
		t = s + strcspn(s, "\r\n");
/* t points to the first newline past s.  If there is no newline in s,
 * then it points to the NUL byte at end of s. */
		*t = '\0';
		c = cookie_from_netscape_line(s);

		if (c) {	/* Got a valid cookie line. */
			if (c->expires < now) {
				freeCookie(c);
				nzFree(c);
				++expired;
			} else {
				cnt++;
				c->fromjar = true;
				addToListBack(&cookies, c);
			}
		}

		s = t + 1;	/* Get ready to read more lines. */
/* Skip over blank lines, if necessary.  */
		while (s < cbuf_end && (*s == '\r' || *s == '\n'))
			s++;
	}

	debugPrint(3, "%d persistent cookies, %d expired", cnt, expired);
	nzFree(cbuf);

	if (!spoonfeed) {
// cookies are in memory; that's all we have to do.
		return;
	}

	foreach(c, cookies)
	    cookieForLibcurl(c);

#if 0
// We use to write the file out again with the old cookies deleted,
// I don't think we need to do this.
	if (expired && whichproc == 'e') {
/* Pour the cookies back into the jar */
		f = fopen(cookieFile, "w");
		if (!f)
			i_printfExit(MSG_NoRebCookie, cookieFile);
		foreach(c, cookies) {
			char *cookLine = netscapeCookieLine(c);
			fputs(cookLine, f);
			nzFree(cookLine);
		}
		fclose(f);
	}
#endif

// Free the resources allocated by this routine.
	foreach(c, cookies)
	    freeCookie(c);
	freeList(&cookies);
}				/* cookiesFromJar */

static bool isInDomain(const char *d, const char *s)
{
	int dl = strlen(d);
	int sl = strlen(s);
	int j = sl - dl;
	if (j < 0)
		return false;
	if (!memEqualCI(d, s + j, dl))
		return false;
	if (j && s[j - 1] != '.')
		return false;
	return true;
}				/* isInDomain */

static bool isPathPrefix(const char *d, const char *s)
{
	int dl = strlen(d);
	int sl = strlen(s);
	if (dl > sl)
		return false;
	return !memcmp(d, s, dl);
}				/* isPathPrefix */

/*********************************************************************
Given a URL, find the cookies that belong to that URL.
These are the cookies that are part of the headers when you fetch a web page.
Curl does all these calculations for us when it fetches the page,
thus building the proper headers, but dog gone it I can't get my hands on
that logic so I have to rewrite it all here. Ugh!
I need it for javascript  document.cookie, which returns all the cookies
that belong to this web page, including any new cookies that were
added during this edbrowse session, e.g. document.cookie = newCookie;
*********************************************************************/

void sendCookies(char **s, int *l, const char *url, bool issecure)
{
	const char *server = getHostURL(url);
	const char *data = getDataURL(url);
	int nc = 0;		/* new cookie */
	struct cookie *c = NULL;
	time_t now;
	struct curl_slist *known_cookies = NULL;
	struct curl_slist *cursor = NULL;

	if (!curlActive)
		return;
	if (!url || !server || !data)
		return;

	curl_easy_getinfo(global_http_handle, CURLINFO_COOKIELIST,
			  &known_cookies);

	if (data > url && data[-1] == '/')
		data--;
	if (!*data)
		data = "/";
	time(&now);

/* Can't use foreach here, since known_cookies is just a pointer. */
	cursor = known_cookies;

/* The code at the top of the loop guards against a memory leak.
 * Otherwise, structs could become inaccessible after continue statements. */
	while (cursor != NULL) {
		if (c != NULL) {	/* discard un-freed cookie structs */
			freeCookie(c);
			nzFree(c);
		}
		c = cookie_from_netscape_line(cursor->data);
		cursor = cursor->next;
		if (c == NULL)	/* didn't read a cookie line. */
			continue;
/* This next test is technically redundant, but let's be clear that
 * HttpOnly cookies *never ever ever* get passed to JavaScript...
 */
		if (!strncmp(c->domain, httponly_prefix, httponly_prefix_len))
			continue;
		if (!isInDomain(c->domain, server))
			continue;
		if (!isPathPrefix(c->path, data))
			continue;
		if (c->expires && c->expires < now)
			continue;
		if (c->secure && !issecure)
			continue;
/* We're good to go. */
		if (!nc)
			stringAndString(s, l, "Cookie: "), nc = 1;
		else
			stringAndString(s, l, "; ");
		stringAndString(s, l, c->name);
		stringAndChar(s, l, '=');
		stringAndString(s, l, c->value);
		debugPrint(3, "send cookie %s=%s", c->name, c->value);
	}

	if (c != NULL) {
		freeCookie(c);
		nzFree(c);
	}

	if (known_cookies != NULL)
		curl_slist_free_all(known_cookies);
	if (nc)
		stringAndString(s, l, eol);
}				/* sendCookies */

// Compare two cookies; this is for qsort.
static int compareCookies(const void *s, const void *t)
{
	const struct cookie **a0 = (const struct cookie **)s;
	const struct cookie **b0 = (const struct cookie **)t;
	const struct cookie *a = *a0;
	const struct cookie *b = *b0;
	int d;
// domains are case insensitive, should we shift these to lower case?
	d = strcmp(a->domain, b->domain);
	if (d)
		return d;
	d = strcmp(a->name, b->name);
	if (d)
		return d;
// It's the same cookie, check expiration dates.
// Reverse order, put the latest expiration date first.
// The transients have expires = 0 and will be last.
// We shouldn't be considdering those anyways.
	d = b->expires - a->expires;
	if (d)
		return d;
// Same cookie same expires, probably from the jar originally
// and now in curl space. Don't waste time putting it back into curl.
// Put the curl entry first so we can skip that step.
	return ((int)a->fromjar - (int)b->fromjar);
}

/*********************************************************************
You're working on a long term project, and you have programmer's documentation
up in console 6 for 3 weeks. The project ends, and you close down
edbrowse in console 6. What happens?
On exit, curl writes the cookie jar with all the cookies it knows about.
These are basically the same cookies you had 3 weeks earlier.
Other instances of edbrowse have added cookies to the jar since then
but that doesn't matter. Those cookies are gone. Poof!
To get around this, I merge the cookies in the jar with the cookies in curl
space just before exit, then curl will right all the cookies to the jar
and we won't lose any.
When there is a duplicate I keep the cookie with the latest expiration date.
There could be thousands of cookies, so don't do a bubble sort, use qsort().
Start by reading from the jar. I didn't want
to copy all that code, so call cookiesFromJar with spoonfeed = false.
Then get the cookies from curl space,
but of course we want all of them, not just for a given URL.
Then sort, merge, and put the new ones back into curl,
which will write to the jar just before it exits.
*********************************************************************/

void mergeCookies(void)
{
	time_t now;
	struct cookie *c, *c2, **a;
	struct curl_slist *known_cookies;
	struct curl_slist *cursor;
	int nc;			// number of cookies
	int i;

	if (!cookieFile)
		return;
	if (whichproc != 'e')
		return;
	if (ismc)
		return;

	spoonfeed = false;
	cookiesFromJar();
	spoonfeed = true;

	time(&now);
	curl_easy_getinfo(global_http_handle, CURLINFO_COOKIELIST,
			  &known_cookies);
	cursor = known_cookies;
	while (cursor != NULL) {
		c = cookie_from_netscape_line(cursor->data);
		cursor = cursor->next;
		if (c == NULL)	/* didn't read a cookie line. */
			continue;
		if (c->expires < now) {	// transient or old
			freeCookie(c);
			nzFree(c);
			continue;
		}
		addToListBack(&cookies, c);
	}

	if (known_cookies != NULL)
		curl_slist_free_all(known_cookies);

// qsort needs an array, not a linked list.
	nc = 0;
	foreach(c, cookies)
	    ++ nc;
	if (!nc)
		return;

	a = allocMem(nc * sizeof(void *));
	i = 0;
	foreach(c, cookies)
	    a[i++] = c;
	qsort(a, nc, sizeof(void *), compareCookies);

// Step through and tell curl about any cookies in the jar that are new,
// or newer than what curl already knows about.
	for (i = 0; i < nc; ++i) {
		c = a[i];
		if (c->fromjar)
			cookieForLibcurl(c);
// skip past duplicates of this cookie.
		while (true) {
			if (i == nc - 1)
				break;
			c2 = a[i + 1];	// next one
			if (!stringEqual(c->domain, c2->domain))
				break;
			if (!stringEqual(c->name, c2->name))
				break;
			++i;
		}
	}

// free resources
	nzFree(a);
	foreach(c, cookies)
	    freeCookie(c);
	freeList(&cookies);
}
