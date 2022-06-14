/*********************************************************************
This file contains support routines for internet access, primarily http
and https. The first batch of functions involves url parsing,
which seems simple enough, but is actually complicated and tedious.
All the various url encoding and decoding, and separating the url into parts.
This is used by any internet fetch, ftp, gopher, you name it.
After that is cookie support, for http and https.
Most websites require cookies to run properly.
Set jar = path in your .ebrc for the cookie jar.
Then there are caching functions for web pages.
If a web page is fetched often, or recently, it will be in your cache.
This is suppose to improve performance.
If you want it, you have to set cachedir and cachesize in your .ebrc.
Finally, there are functions for http authentication.
This is a login password mechanism that is specified in the http protocol.
It isn't fields in a form.
I don't think it is used very often, but sometimes it is,
and we should support it, at least at a basic level.
*********************************************************************/

#include "eb.h"

struct PROTOCOL {
	const char prot[MAXPROTLEN];
	int port;
	bool free_syntax;
	bool need_slashes;
	bool need_slash_after_host;
} protocols[] = {
	{"file", 0, true, false, false},
	{"http", 80, false, true, true},
	{"https", 443, false, true, true},
	{"pop3", 110, false, true, true},
	{"pop3s", 995, false, true, true},
	{"imap", 220, false, true, true},
	{"imaps", 993, false, true, true},
	{"smtp", 25, false, true, true},
	{"submission", 587, false, true, true},
	{"smtps", 465, false, true, true},
	{"proxy", 3128, false, true, true},
	{"ftp", 21, false, true, true},
	{"sftp", 22, false, true, true},
	{"scp", 22, false, true, true},
	{"ftps", 990, false, true, true},
	{"tftp", 69, false, true, true},
	{"rtsp", 554, false, true, true},
	{"pnm", 7070, false, true, true},
	{"finger", 79, false, true, true},
	{"smb", 139, false, true, true},
	{"mailto", 0, false, false, false},
	{"telnet", 23, false, false, false},
	{"tn3270", 0, false, false, false},
	{"data", 0, true, false, false},
	{"javascript", 0, true, false, false},
	{"git", 0, false, true, false},
	{"svn", 0, false, true, false},
	{"gopher", 70, false, true, true},
	{"magnet", 0, false, false, false},
	{"irc", 0, false, true, false},
	{"", 0, false, false, false},
};

static int protocolByName(const char *p, int l)
{
	int i;
	for (i = 0; protocols[i].prot[0]; i++)
		if ((int)strlen(protocols[i].prot) == l &&
		    memEqualCI(protocols[i].prot, p, l))
			return i;
	return -1;
}

/* Unpercent the host component of a url, not the data component. */
void unpercentURL(char *url)
{
	char c, *u, *w;
	int n;
	u = w = url;
	while ((c = *u)) {
		++u;
		if (c == '+')
			c = ' ';
		if (c == '%' && isxdigit(u[0]) && isxdigit(u[1])) {
			c = fromHex(u[0], u[1]);
			u += 2;
		}
		if (!c)
			c = ' ';	/* should never happen */
		*w++ = c;
		if (strchr("?#\1", c))
			break;
		if (c != '/')
			continue;
		n = w - url;
		if (n == 1 || n > 16)
			break;
		if (w[-2] != ':' && w[-2] != '/')
			break;
	}
	strmove(w, u);
}

/* Unpercent an entire string. */
void unpercentString(char *s)
{
	char c, *u, *w;
	u = w = s;
	while ((c = *u)) {
		++u;
		if (c == '+')
			c = ' ';
		if (c == '%' && isxdigit(u[0]) && isxdigit(u[1])) {
			c = fromHex(u[0], u[1]);
			u += 2;
		}
		if (!c)
			c = ' ';	/* should never happen */
		*w++ = c;
	}
	*w = 0;
}

/*
 * Function: percentURL
 * Arguments:
 ** start: pointer to start of input string
  ** end: pointer to end of input string.
 * Return value: A new string with the url encoded.
 * There is an extra byte, room for / at the end.
 * This function copies its input to a dynamically-allocated buffer,
 * while performing the following transformation.  Change backslash to slash,
 * and percent-escape some of the reserved characters as per RFC3986.
 * Some of the chars retain their reserved semantics and should not be changed.
 * This is a friggin guess!
 * All characters in the area between start and end, not including end,
 * are copied or transformed, except the hash, which is removed.
 * This function is used to sanitize user-supplied URLs.  */

/* these punctuations are percentable, anywhere in a url.
 * Google has commas in encoded URLs, and wikipedia has parentheses,
 * so those are (sort of) ok. */
static const char percentable[] = "'\"<>";
static const char hexdigits[] = "0123456789abcdef";
#define ESCAPED_CHAR_LENGTH 3

char *percentURL(const char *start, const char *end)
{
	int bytes_to_alloc;
	char *new_copy;
	const char *in_pointer;
	char *out_pointer;
	char *frag;

	if (!end)
		end = start + strlen(start);
	bytes_to_alloc = end - start + 2;
	new_copy = NULL;
	in_pointer = NULL;
	out_pointer = NULL;

	for (in_pointer = start; in_pointer < end; in_pointer++)
		if (*in_pointer <= ' ' || strchr(percentable, *in_pointer))
			bytes_to_alloc += (ESCAPED_CHAR_LENGTH - 1);

	new_copy = allocMem(bytes_to_alloc);
	out_pointer = new_copy;
	for (in_pointer = start; in_pointer < end; in_pointer++) {
		if (*in_pointer == '\\')
			*out_pointer++ = '/';
		else if (*in_pointer <= ' ' || strchr(percentable, *in_pointer)) {
			*out_pointer++ = '%';
			*out_pointer++ =
			    hexdigits[(uchar) (*in_pointer & 0xf0) >> 4];
			*out_pointer++ = hexdigits[(*in_pointer & 0x0f)];
		} else
			*out_pointer++ = *in_pointer;
	}
	*out_pointer = '\0';
/* excise #hash, required by some web servers */
	frag = findHash(new_copy);
	if (frag)
		*frag = 0;

	return new_copy;
}

// For debugging only.
bool looksPercented(const char *start, const char *end)
{
	const char *s;
	if (!end)
		end = start + strlen(start);
	for (s = start; s < end; ++s)
		if (*s < ' ' || strchr(percentable, *s))
			return false;
	return true;
}

/* escape & < > for display on a web page */
char *htmlEscape0(const char *s, bool do_and)
{
	char *t;
	int l;
	if (!s)
		return 0;
	if (!*s)
		return emptyString;
	t = initString(&l);
	for (; *s; ++s) {
		if (*s == '&' && do_and) {
			stringAndString(&t, &l, "&amp;");
			continue;
		}
		if (*s == '\'' && do_and) {
			stringAndString(&t, &l, "&apos;");
			continue;
		}
		if (*s == '<') {
			stringAndString(&t, &l, "&lt;");
			continue;
		}
		if (*s == '>') {
			stringAndString(&t, &l, "&gt;");
			continue;
		}
		stringAndChar(&t, &l, *s);
	}
	return t;
}

/* Decide if it looks like a web url. */
/* Don't do this in a href context  <a href=www.google.com> */
static bool hrefContext;
static bool httpDefault(const char *url)
{
	static const char *const domainSuffix[] = {
		"com", "biz", "info", "net", "org", "gov", "edu", "us", "uk",
		"au",
		"ca", "de", "jp", "nz", 0
	};
	int n, len;
	const char *s, *lastdot, *end;
	if (hrefContext)
		return false;
	end = url + strcspn(url, "/?#\1");
	if (end - url > 7 && stringEqual(end - 7, ".browse"))
		end -= 7;
	s = strrchr(url, ':');
	if (s && s < end) {
		const char *colon = s;
		++s;
		while (isdigitByte(*s))
			++s;
		if (s == end)
			end = colon;
	}
// check for the ipv6 format
	if(end - url >= 7 && url[0] == '[' && end[-1]== ']') {
		n = 0;
		for (s = url + 1; s < end - 1; ++s) {
			if(*s == ':') ++n;
			else if(!isxdigit(*s)) return false;
		}
		return (n >= 5); // at least 5 colons
	}
// only domain characters allowed
	for (s = url; s < end; ++s)
		if (!isalnumByte(*s) && *s != '.' && *s != '-')
			return false;
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
			if (!isdigitByte(*s) && *s != '.')
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
	if (len >= 2 && memEqualCI(url, "www.", 4))
		return true;
	return false;
}

/*********************************************************************
From wikipedia url
scheme://domain:port/path?query_string#fragment_id
but I allow, at the end of this, control a followed by post data, with the
understanding that there should not be query_string and post data simultaneously.
*********************************************************************/

static bool parseURL(const char *url, const char **proto, int *prlen, const char **user, int *uslen, const char **pass, int *palen,	/* ftp protocol */
		     const char **host, int *holen,
		     const char **portloc, int *port,
		     const char **data, int *dalen, const char **post,
		     bool * freep)
{
	const char *p, *q, *pp;
	int a;
	bool has_slashes = false;

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
	if (freep)
		*freep = false;

	if (!url)
		return false;

// Find the leading protocol://
	a = -1;
	p = strchr(url, ':');
	if (p) {
		for (q = url; q < p; ++q)
			if (!isalnumByte(*q))
				break;
		if (q < p)
			p = 0;
		if (isdigit(url[0]))
			p = 0;
	}

	if (p) {
		q = p + 1;
		if (*q == '/')
			++q, has_slashes = true;
		if (*q == '/')
			++q;
		skipWhite(&q);

		if (!*q) {
// You have to have something after the colon
// but javascript: is technically a url, I guess
			if (strncmp(url, "javascript:", 11))
				return false;
		}

		if (proto)
			*proto = url;
		if (prlen)
			*prlen = p - url;
		a = protocolByName(url, p - url);
		if (a >= 0 && !protocols[a].need_slashes)
			++p;
		else
			p = q;
	} else if (httpDefault(url)) {
		static const char http[] = "http://";
		if (proto)
			*proto = http;
		if (prlen)
			*prlen = 4;
		a = 1;
		p = url;
	} else
		return false;

/*********************************************************************
I wanted to add the following, so we can edit the file u:v, that is,
u:v is not a url.
	if(a < 0 && !has_slashes)
		return false;
However, this breaks nasa.gov. They actually have in their js:
                            if (f.hasDOM && (t = ot.call(e, 'foobar:baz')), 'foobar:' === t)
So we have to treat foobar:baz as a url with the foobar protocol.
That means you can't edit the file u:v directly.
Use ./u:v or some other mechanism. I know, it's annoying, what can I do?
*********************************************************************/

	if (a < 0 || protocols[a].free_syntax) {
		if (data)
			*data = p;
		if (dalen)
			*dalen = strlen(p);
		if (freep)
			*freep = true;
		return true;
	}

	if (a < 0)
		return true;	// don't know anything else

// find the end of the domain
	q = p + strcspn(p, "@?#/\1");
	if (*q == '@') {	/* user:password@host */
		pp = strchr(p, ':');
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

// again, look for the end of the domain, this time watching for :port,
// which does indeed end the domain. But wait! ipv6 has : in the middle
// of the domain. I need some special code here for that possibility.
	if(p[0] == '[' && (q = strchr(p, ']'))) {
// only ipv6 characters allowed
		for (pp = p + 1; pp < q; ++pp)
			if (!isxdigit(*pp) && *pp != ':')
				return false;
// looks good
		++q;
// yeah it's possible to have .browse on the end, a real corner case.
		if(!strncmp(q, ".browse", 7))
			q += 7;
// now has to be a domain ending character
		if(*q && !strchr(":?#/\1", *q))
			return false;
	} else {
		q = p + strcspn(p, ":?#/\1");
// only domain characters allowed
		for (pp = p; pp < q; ++pp)
			if (!isalnumByte(*pp) && *pp != '.' && *pp != '-')
				return false;
	}

	if (host)
		*host = p;
	if (holen) {
		*holen = q - p;
// Watch out. Accessing document.cookie from javascript calls this function,
// and we might have .browse on the end of the domain, which causes trouble.
		if(*holen > 7 && stringEqual(q - 7, ".browse"))
			*holen -= 7;
	}

	if (*q == ':') {	/* port specified */
		int n;
		const char *cc, *pp = q + strcspn(q, "/?#\1");
		if (pp > q + 1) {
			n = strtol(q + 1, (char **)&cc, 10);
			if (cc != pp || !isdigitByte(q[1])) {
//                              setError(MSG_BadPort);
				return false;
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
}

bool isURL(const char *url)
{
	return parseURL(url, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

bool isSQL(const char *s)
{
	char c = *s;
	const char *colon = 0;

	if (!sqlPresent || c != ']')
		goto no;

	c = *++s;
// look for word] or word:word]
	if (!isalphaByte(c))
		goto no;
	for (++s; (c = *s); ++s) {
		if (c == '_')
			continue;
		if (isalnumByte(c))
			continue;
		if (c == ':') {
			if (colon)
				goto no;
			colon = s;
			continue;
		}
		if (c == ']')
			goto yes;
		goto no;
	}

no:
	return false;

yes:
	return true;
}

// non-FTP URLs are always browsable.  FTP URLs are browsable if they end with
//a slash. gopher urls are a bit more complicated, not yet implemented.
bool isBrowseableURL(const char *url)
{
	if (isURL(url))
		return (!memEqualCI(url, "ftp://", 6))
		    || (url[strlen(url) - 1] == '/');
	else
		return false;
}

bool isDataURI(const char *u)
{
	return u && memEqualCI(u, "data:", 5);
}

/* Helper functions to return pieces of the URL.
 * Makes a copy, so you can have your 0 on the end.
 * Return 0 for an error, and "" if that piece is missing. */

const char *getProtURL(const char *url)
{
	static char buf[MAXPROTLEN];
	int l;
	const char *s;
	if (!parseURL(url, &s, &l, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
		return 0;
	if (l >= MAXPROTLEN)
		l = MAXPROTLEN - 1;
	memcpy(buf, s, l);
	buf[l] = 0;
	return buf;
}

// Is this a url without http:// in front?
bool missingProtURL(const char *url)
{
	const char *s;
	if (!parseURL(url, &s, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
		return false;	// not a url
// protocol is always the start of url, unless url is a recognized
// format like www.foo.bar.com, then s points to the static string "http://".
	return (s != url);
}

static char hostbuf[MAXHOSTLEN];
const char *getHostURL(const char *url)
{
	int l;
	const char *s;
	char *t;
	char c, d;
	bool fs;
	bool rc = parseURL(url, 0, 0, 0, 0, 0, 0, &s, &l, 0, 0, 0, 0, 0, &fs);
	if (!rc || fs)
		return 0;
	if (!s)
		return emptyString;
	if ((unsigned)l >= sizeof(hostbuf)) {
		setError(MSG_DomainLong);
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
}

bool getProtHostURL(const char *url, char *pp, char *hp)
{
	int l1, l2;
	const char *s1, *s2;
	bool fs;
	if (!parseURL(url, &s1, &l1, 0, 0, 0, 0, &s2, &l2, 0, 0, 0, 0, 0, &fs))
		return false;
	if (pp) {
		*pp = 0;
		if (s1) {
			if (l1 >= MAXPROTLEN)
				l1 = MAXPROTLEN - 1;
			memcpy(pp, s1, l1);
			pp[l1] = 0;
		}
	}
	if (hp) {
		*hp = 0;
		if (s2) {
			if (l2 >= MAXHOSTLEN)
				l2 = MAXHOSTLEN - 1;
			memcpy(hp, s2, l2);
			hp[l2] = 0;
		}
	}
	return true;
}

// return user:password. Fails only if user or password too long.
int getCredsURL(const char *url, char *buf)
{
	int l1, l2;
	const char *s1, *s2;
	bool fs;
	bool rc =
	    parseURL(url, 0, 0, &s1, &l1, &s2, &l2, 0, 0, 0, 0, 0, 0, 0, &fs);
	strcpy(buf, ":");
	if (!rc || fs)
		return 0;
	if (s1 && l1 > MAXUSERPASS)
		return 1;
	if (s2 && l2 > MAXUSERPASS)
		return 2;
	if (s1)
		strncpy(buf, s1, l1);
	else
		l1 = 0;
	buf[l1++] = ':';
	if (s2)
		strncpy(buf + l1, s2, l2);
	else
		l2 = 0;
	buf[l1 + l2] = 0;
	return 0;
}

const char *getDataURL(const char *url)
{
	const char *s;
	bool rc = parseURL(url, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, &s, 0, 0, 0);
	if (!rc)
		return 0;
	return s;
}

// return null for free syntax
static const char *getDataURL1(const char *url)
{
	const char *s;
	bool fs;
	bool rc = parseURL(url, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, &s, 0, 0, &fs);
	if (!rc || fs)
		return 0;
	return s;
}

void getDirURL(const char *url, const char **start_p, const char **end_p)
{
	const char *dir = getDataURL1(url);
	const char *end;
	static const char myslash[] = "/";
	if (!dir || dir == url)
		goto slash;
	if (!strchr("#?\1", *dir)) {
		if (*--dir != '/')
			i_printfExit(MSG_BadDirSlash, url);
	}
	if (*dir == '#')	/* special case */
		end = dir;
	else
		end = strpbrk(dir, "?\1");
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
}

// I assume this is a properly formatted URL, thus # indicates hash
char *findHash(const char *s)
{
	return strchr(s, '#');
}

/* extract the file piece of a pathname or url */
/* This is for debugPrint or w/, so could be chopped for convenience */
char *getFileURL(const char *url, bool chophash)
{
	const char *s;
	const char *e;
	s = strrchr(url, '/');
	if (s)
		++s;
	else
		s = url;
	e = 0;
	if (isURL(url)) {
		chophash = true;
		e = strpbrk(s, "?\1");
	}
	if (!e)
		e = s + strlen(s);
	if (chophash) {
		const char *h = findHash(s);
		if (h)
			e = h;
	}
// if slash at the end then back up to the prior slash
// /.browse is like / at the end
	if (s > url && (e == s || (e - s == 7 && !strncmp(s, ".browse", 7)))) {
		while (s > url && s[-1] == '/')
			--s;
		e = s;
		while (s > url && s[-1] != '/')
			--s;
	}
/* don't retain the .browse suffix on a url */
	if (e - s > 7 && stringEqual(e - 7, ".browse"))
		e -= 7;
	if (e - s > 64)
		e = s + 64;
	if (e == s)
		strcpy(hostbuf, "/");
	else {
		strncpy(hostbuf, s, e - s);
		hostbuf[e - s] = 0;
	}
	return hostbuf;
}

bool getPortLocURL(const char *url, const char **portloc, int *port)
{
	bool fs;
	bool rc =
	    parseURL(url, 0, 0, 0, 0, 0, 0, 0, 0, portloc, port, 0, 0, 0, &fs);
	if (!rc || fs)
		return false;
	return true;
}

int getPortURL(const char *url)
{
	int port;
	bool fs;
	bool rc = parseURL(url, 0, 0, 0, 0, 0, 0, 0, 0, 0, &port, 0, 0, 0, &fs);
	if (!rc || fs)
		return 0;
	return port;
}

bool isProxyURL(const char *url)
{
	return ((url[0] | 0x20) == 'p');
}

static void squashDirectories(char *url)
{
	char *dd, *s, *u, *end, *rest;

	if (memEqualCI(url, "javascript:", 11))
		return;

// if you're browsing local files, it may not be a url at all.
	if(!isURL(url)) {
		dd = url;
	} else {
		dd = (char *)getDataURL(url);
		if (!dd || dd == url)
			return;
		if (!*dd)
			return;
		if (strchr("#?\1", *dd))
			return;
		--dd;
/* dd could point to : in bogus code such as <A href=crap:foobar> */
/* crap: looks like a slashless protocol, perhaps unknown to us. */
		if (*dd == ':')
			return;
		if (*dd != '/') {
			i_printf(MSG_BadSlash, url);
			return;
		}
	}

	end = dd + strcspn(dd, "?\1");
	rest = cloneString(end);
	*end = 0;

/* The following algorithm is straight out of RFC 3986, section 5.2.4. */
	s = dd;
	while (*s) {
		if (!strncmp(s, "/./", 3)) {
			strmove(s, s+2);
			continue;
		}
		if (s == dd && !strncmp(s, "./", 2)) {
			strmove(s, s+2);
			continue;
		}
		if (stringEqual(s, "/.")) {
			s[1] = '\0';
			break;
		}
		if (!strncmp(s, "/../", 4)) {
			if(s == dd) {
				strmove(s, s+3);
				continue;
			}
			if((s == dd + 2 && !strncmp(dd, "..", 2)) ||
			(s > dd + 2 && !strncmp(s - 3, "/..", 3))) {
				s += 3;
				continue;
			}
			for(u = s-1; u >= dd && *u != '/'; --u)  ;
			strmove(u + 1, s + 4);
			s = (u >= dd ? u : u + 1);
			continue;
		}
		if (stringEqual(s, "/..")) {
			if(s == dd) {
				s[1] = 0;
				break;
			}
			if((s == dd + 2 && !strncmp(dd, "..", 2)) ||
			(s > dd + 2 && !strncmp(s - 3, "/..", 3))) {
				break;
			}
			for(u = s-1; u >= dd && *u != '/'; --u)  ;
			u[1] = 0;
			break;
		}
			++s;
	}

	strcat(url, rest);
	nzFree(rest);
}

char *resolveURL(const char *base, const char *rel)
{
	char *n;		/* new url */
	const char *s, *p;
	char *q;
	int l;

	if (memEqualCI(rel, "data:", 5))
		return cloneString(rel);

	debugPrint(5, "resolve(%s|%s)", base, rel);
	hrefContext = true;
	if (!base)
		base = emptyString;
	if (!rel)
		rel = emptyString;
	n = allocString(strlen(base) + strlen(rel) + 12);
	*n = 0;

	if (rel[0] == '#') {
// This is an anchor for the current document
		if(isURL(base))
			strcpy(n, base);
// We don't want url#foo#bar
		q = strchr(n, '#');
		if(q)
			*q = 0;
		if(rel[1])
			strcat(n, rel);
out_n:
		debugPrint(5, "= %s", n);
		hrefContext = false;
		return n;
	}

	if (rel[0] == '?' || rel[0] == '\1') {
/* setting or changing get or post data */
		strcpy(n, base);
		for (q = n; *q && *q != '\1' && *q != '?'; q++) ;
		strcpy(q, rel);
		goto out_n;
	}

	if (rel[0] == '/' && rel[1] == '/') {
		if ((s = strstr(base, "//"))) {
			strncpy(n, base, s - base);
			n[s - base] = 0;
		} else
			strcpy(n, "http:");
		strcat(n, rel);
		goto squash;
	}

	if (parseURL(rel, &s, &l, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) > 0) {
/* has a protocol */
		n[0] = 0;
		if (s != rel) {
/* It didn't have http in front of it before, put it on now. */
/* This is old; it shouldn't happen any more. */
			strncpy(n, s, l);
			strcpy(n + l, "://");
		}
		strcat(n, rel);
		goto squash;
	}
// at this point rel is not a url.
	s = base;
	if (rel[0] == '/') {
		s = getDataURL(base);
		if (!s) {
			strcpy(n, rel);
			goto squash;
		}
		if (!*s) {
			if (s - base >= 7 && stringEqual(s - 7, ".browse"))
				s -= 7;
			if (s > base && s[-1] == '/')
				--s;
		} else if (!strchr("#?\1", *s)) {
			--s;
		} else if (s[-1] == '/')
			--s;
		l = s - base;
		strncpy(n, base, l);
		strcpy(n + l, rel);
		goto squash;
	}
/* This is a relative change, paste it on after the last slash */
	s = base;
	if (parseURL(base, 0, 0, 0, 0, 0, 0, &p, 0, 0, 0, 0, 0, 0, 0) > 0 && p)
		s = p;
	for (p = 0; *s; ++s) {
		if (*s == '/')
			p = s;
		if (strchr("#?\1", *s))
			break;
	}
	if (!p) {
		if (isURL(base))
			p = s;
		else
			p = base;
	}
	l = p - base;
	if (l) {
		strncpy(n, base, l);
		n[l++] = '/';
	}
	strcpy(n + l, rel);

squash:
	squashDirectories(n);
	goto out_n;
}

/* This routine could be, should be, more sophisticated */
bool sameURL(const char *s, const char *t)
{
	const char *u, *p, *q;
	int l;

	if (!s || !t)
		return false;

/* check for post data at the end */
	p = strchr(s, '\1');
	if (!p)
		p = s + strlen(s);
	q = strchr(t, '\1');
	if (!q)
		q = t + strlen(t);
	if (!stringEqual(p, q))
		return false;

/* lop off hash */
	if ((u = findHash(s)))
		p = u;
	if ((u = findHash(t)))
		q = u;

/* It's ok if one says http and the other implies it. */
	if (memEqualCI(s, "http://", 7))
		s += 7;
	if (memEqualCI(t, "http://", 7))
		t += 7;

	if (p - s >= 7 && stringEqual(p - 7, ".browse"))
		p -= 7;
	if (q - t >= 7 && stringEqual(q - 7, ".browse"))
		q -= 7;
	l = p - s;
	if (l != q - t)
		return false;
	return !memcmp(s, t, l);
}

/* Find some helpful text to print in place of an image.
 * Not sure why we would need more than 1000 chars for this,
 * so return a static buffer. */
char *altText(const char *base)
{
	static char buf[1000];
	int len, n;
	char *s;
	debugPrint(6, "altText(%s)", base);
	if (!base)
		return 0;
	if (stringEqual(base, "#"))
		return 0;
	if (memEqualCI(base, "javascript", 10))
		return 0;
	strncpy(buf, base, sizeof(buf) - 1);
	spaceCrunch(buf, true, false);
	len = strlen(buf);
/* remove punctuation mark from the end of a sentence or phrase */
	if (len >= 2 && !isalnumByte(buf[len - 1]) && isalnumByte(buf[len - 2]))
		buf[--len] = 0;
/* strip leading whitespace */
	while (len && isspaceByte(buf[0]))
		strmove(buf, buf + 1), --len;
	if (len > 10) {
/* see whether it's a phrase/sentence or a pathname/url */
		if (!isURL(buf))
			return buf;	/* looks like words */
/* Ok, now we believe it's a pathname or url */
/* get rid of post or get data */
		s = strpbrk(buf, "?\1");
		if (s)
			*s = 0;
/* get rid of common suffix */
		s = strrchr(buf, '.');
		if (s) {
/* get rid of trailing .html */
			static const char *const suffix[] = {
				"html", "htm", "shtml", "shtm", "php", "asp",
				"cgi", "rm",
				"ram",
				"gif", "jpg", "bmp",
				0
			};
			n = stringInListCI(suffix, s + 1);
			if (n >= 0 || s[1] == 0)
				*s = 0;
		}
/* Get rid of everything up to the last slash, leaving the file name */
retry:
		s = strrchr(buf, '/');
		if (s && s - buf >= 12) {
			if (!s[1]) {
				*s = 0;
				goto retry;
			}
			strmove(buf, s + 1);
		}
	}			/* more than ten characters */
	return buf;
}

/* get post data ready for a url. */
char *encodePostData(const char *s, const char *keep_chars)
{
	char *post, c;
	int l;
	char buf[4];
	bool is_http = false;

	if (!s)
		return 0;
	if (s == emptyString)
		return emptyString;
	if (!keep_chars)
		keep_chars = "-._~()", is_http = true;
	post = initString(&l);
	while ((c = *s++)) {
		if (isalnumByte(c))
			goto putc;
		if (strchr(keep_chars, c))
			goto putc;

/*********************************************************************
Ok, space to + is a real pain in the ass!
Who was smoking what when he came up with that one?
I can turn space into %20 and that always works everywhere, so why not do that?
Well, when encoding input fields, *all* the other browsers turn space to plus.
If we want to hold debug output side by side, or even compare it via computer,
which we sometimes do, it helps if the output is byte for byte the same.
So I, rather reluctantly, turn space to + for post data.
However, this routine is called by gopher to build a url.
Not input fields but the pathname of a url, and in that context,
space has to become %20.
I can tell it's gopher because keep_chars is not null.
So that's 20 lines of comments just to explain
the second half of this if statement.
*********************************************************************/
		if(c == ' ' && is_http) {
			c = '+';
			goto putc;
		}

		sprintf(buf, "%%%02X", (uchar) c);
		stringAndString(&post, &l, buf);
		continue;
putc:
		stringAndChar(&post, &l, c);
	}
	return post;
}

static char dohex(char c, const char **sp)
{
	const char *s = *sp;
	char d, e;
	if (c == '+')
		return ' ';
	if (c != '%')
		return c;
	d = *s++;
	e = *s++;
	if (!isxdigit(d) || !isxdigit(e))
		return c;	/* should never happen */
	d = fromHex(d, e);
	if (!d)
		d = ' ';	/* don't allow nulls */
	*sp = s;
	return d;
}

char *decodePostData(const char *data, const char *name, int seqno)
{
	const char *s, *n, *t;
	char *ns = 0, *w = 0;
	int j = 0;
	char c;

	if (!seqno && !name)
		i_printfExit(MSG_DecodePost);

	for (s = data; *s; s = (*t ? t + 1 : t)) {
		n = 0;
		t = strchr(s, '&');
		if (!t)
			t = s + strlen(s);
/* select attribute by number */
		++j;
		if (j == seqno)
			w = ns = allocString(t - s + 1);
		if (seqno && !w)
			continue;
		if (name)
			n = name;
		while (s < t && (c = *s) != '=') {
			++s;
			c = dohex(c, &s);
			if (n) {
/* I don't know if this is suppose to be case insensitive all the time,
 * though there are situations when it must be, as in
 * mailto:address?Subject=blah-blah */
				if (isalphaByte(c)) {
					if (!((c ^ *n) & 0xdf))
						++n;
					else
						n = 0;
				} else if (c == *n)
					++n;
				else
					n = 0;
			}
			if (w)
				*w++ = c;
		}

		if (s == t) {	/* no equals, just a string */
			if (name)
				continue;
			*w = 0;
			return ns;
		}
		if (w)
			*w++ = c;
		++s;		/* skip past equals */
		if (name) {
			if (!n)
				continue;
			if (*n)
				continue;
			w = ns = allocString(t - s + 1);
		}

/* At this point we have a match */
		while (s < t) {
			c = *s++;
			c = dohex(c, &s);
			*w++ = c;
		}
		*w = 0;
		return ns;
	}

	return 0;
}

void decodeMailURL(const char *url, char **addr_p, char **subj_p, char **body_p)
{
	const char *s;
	if (memEqualCI(url, "mailto:", 7))
		url += 7;
	s = url + strcspn(url, "/?");
	if (addr_p)
		*addr_p = pullString1(url, s);
	if (subj_p)
		*subj_p = 0;
	if (body_p)
		*body_p = 0;
	s = strchr(url, '?');
	if (!s)
		return;
	url = s + 1;
	if (subj_p)
		*subj_p = decodePostData(url, "subject", 0);
	if (body_p)
		*body_p = decodePostData(url, "body", 0);
}

// Does a url match a pattern, from an entry in .ebrc
// edbrowse.org matches edbrowse.org and foo.edbrowse.org
// edbrowse.org/foo matches edbrowse.org/foo/bar
bool patternMatchURL(const char *url, const char *pattern)
{
	char prot[MAXPROTLEN], host[MAXHOSTLEN];
	const char *path, *q;
	int hl, dl, ql;
	if (!url || !pattern)
		return false;
	if (!url[0] || !pattern[0])
		return false;
// This function has to be threadsafe, so I call getProtHostURL,
// which is also threadsafe.
	if (!getProtHostURL(url, prot, host))
		return false;
	hl = strlen(host);
	path = getDataURL(url);
	q = strchr(pattern, '/');
	if (!q)
		q = pattern + strlen(pattern);
	dl = q - pattern;
	if (dl > hl)
		return false;
	if (!memEqualCI(pattern, host + hl - dl, dl))
		return false;
	if (*q == '/') {
		++q;
		if (hl != dl || !path)
			return false;
		ql = strlen(q);
		return !strncmp(q, path, ql) &&
		    (path[ql] == 0 || path[ql] == '/');
	}			/* domain/path was specified */
	return hl == dl || host[hl - dl - 1] == '.';
}

// Make sure a web page is not trying to read a local file through a frame
bool frameSecurityFile(const char *thisfile)
{
	Frame *f = &cf->owner->f0;
	for (; f != cf; f = f->next) {
		if (!isURL(f->fileName))
			continue;
		setError(MSG_NoAccessSecure, thisfile);
		return false;
	}
	return true;
}

/* cookie functions.
 * (c) 2002 Mikulas Patocka
 * This file is part of the Links project, released under GPL
 * Modified by Karl Dahlke for integration with edbrowse.
 * Modified by Chris Brannon to allow cooperation with libcurl.
 */

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
}

static int count_tabs(const char *the_string)
{
	int str_index = 0, num_tabs = 0;
	for (str_index = 0; the_string[str_index] != '\0'; str_index++)
		if (the_string[str_index] == '\t')
			num_tabs++;
	return num_tabs;
}

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
}

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
}

static void freeCookie(struct cookie *c)
{
	nzFree(c->name);
	nzFree(c->value);
	nzFree(c->server);
	nzFree(c->path);
	nzFree(c->domain);
}

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
}

/* Should this server really specify this domain in a cookie? */
/* Domain must be the trailing substring of server. */
static bool domainSecurityCheck(const char *server, const char *domain)
{
	int i, dl, nd;
	dl = strlen(domain);
/* x.com or x.y.z */
	if (dl < 5)
		return false;
	if (dl > (int)strlen(server))
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
}

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
}

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

// Free the resources allocated by this routine.
	foreach(c, cookies)
	    freeCookie(c);
	freeList(&cookies);
}

bool isInDomain(const char *d, const char *s)
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
}

static bool isPathPrefix(const char *d, const char *s)
{
	int dl = strlen(d);
	int sl = strlen(s);
	if (dl > sl)
		return false;
	return !memcmp(d, s, dl);
}

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
}

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

/*********************************************************************
Maintain a cache of the http files.
The url is the key.
The result is a string that holds a 5 digit filename, the etag,
last modified time, last access time, and file size.
nnnnn tab etag tab last-mod tab access tab size
The access time helps us clean house; delete the oldest files.
If you change the format of this file in any way, increment the version number.
Previous cache files will be left hanging around, but oh well.
Not expecting to change this file format very often.
cacheDir is the directory holding the cached files,
and cacheControl is the file that houses the database.
I open a lock file with O_EXCL when accessing the cache,
and if busy then I wait a few milliseconds and try again.
If the stored etag and header etag are both present, and don't match,
then the file is stale.
If one or the other etag is missing, and mod time website > mod time cached,
then the file is stale.
We don't even query the cache if we don't have at least one of etag or mod time.
*********************************************************************/

#define CACHECONTROLVERSION 1

#ifdef DOSLIKE
#define USLEEP(a) Sleep(a / 1000)	// sleep millisecs
#else
#define USLEEP(a) usleep(a)	// sleep microsecs
#endif

static int control_fh = -1;	/* file handle for cacheControl */
static char *cache_data;
static time_t now_t;
static char *cacheFile, *cacheLock, *cacheControl;

/* a cache entry */
struct CENTRY {
	off_t offset;
	size_t textlength;
	const char *url;
	int filenumber;
	const char *etag;
	int modtime;
	int accesstime;
	int pages;		/* in 4K pages */
};

static struct CENTRY *entries;
static int numentries;

void setupEdbrowseCache(void)
{
	int fh;

	if (control_fh >= 0) {
		close(control_fh);
		control_fh = -1;
	}
#ifdef DOSLIKE
	if (!cacheDir) {
		if (!ebUserDir)
			return;
		cacheDir = allocMem(strlen(ebUserDir) + 7);
		sprintf(cacheDir, "%s/cache", ebUserDir);
	}
	if (fileTypeByName(cacheDir, false) != 'd') {
		if (mkdir(cacheDir, 0700)) {
/* Don't want to abort here; we might be on a readonly filesystem.
 * Don't have a cache directory and can't creat one; yet we should move on. */
			free(cacheDir);
			cacheDir = 0;
			return;
		}
	}
#else
	if (!cacheDir) {
		cacheDir = allocMem(strlen(home) + 10);
		sprintf(cacheDir, "%s/.ebcache", home);
	}
	if (fileTypeByName(cacheDir, false) != 'd') {
		if (mkdir(cacheDir, 0700)) {
/* Don't want to abort here; we might be on a readonly filesystem.
 * Don't have a cache directory and can't creat one; yet we should move on. */
			free(cacheDir);
			cacheDir = 0;
			return;
		}
	}
#endif

/* the cache control file, which urls go to which files, and when fetched? */
	nzFree(cacheControl);
	cacheControl = allocMem(strlen(cacheDir) + 11);
	sprintf(cacheControl, "%s/control%02d", cacheDir, CACHECONTROLVERSION);
/* make sure the control file exists, just for grins */
	fh = open(cacheControl, O_WRONLY | O_APPEND | O_CREAT, MODE_private);
	if (fh >= 0)
		close(fh);

	nzFree(cacheLock);
	cacheLock = allocMem(strlen(cacheDir) + 6);
	sprintf(cacheLock, "%s/lock", cacheDir);

	nzFree(cacheFile);
	cacheFile = allocMem(strlen(cacheDir) + 7);

	nzFree(entries);
	entries = allocMem(cacheCount * sizeof(struct CENTRY));
}

/*********************************************************************
Read the control file into memory and parse it into entry structures.
Sadly, I do this every time you access the cache.
It would be better to hold all this data in memory, with the time stamp of the
control file, and if the control file has not been updated
then just use what we have;
and if it has been updated then read it and parse it.
Well maybe we'll implement this later.
For now, the control file isn't too big, it's not prohibitive
to do this every time.
Note that control is a nice ascii readable file, helps with debugging.
*********************************************************************/

static bool readControl(void)
{
	char *s, *t, *endfile;
	char *data;
	int datalen;
	struct CENTRY *e;
	int ln = 1;

	lseek(control_fh, 0L, 0);
	if (!fdIntoMemory(control_fh, &data, &datalen))
		return false;

	numentries = 0;
	e = entries;
	endfile = data + datalen;
	for (s = data; s != endfile; s = t, ++ln) {
		t = strchr(s, '\n');
		if (!t) {
/* file does not end in newline; this should never happen! */
/* Not sure what to do, but at least it's not a seg fault. */
			break;
		}
		++t;
		e->offset = s - data;
		e->textlength = t - s;
		e->url = s;
		s = strchr(s, '\t');
		if (!s || s >= t) {
			debugPrint(3, "cache control file line %d is bogus",
				   ln);
			continue;
		}
		*s++ = 0;
		e->filenumber = strtol(s, &s, 10);
		++s;
		e->etag = s;
		s = strchr(s, '\t');
		if (!s || s >= t) {
			debugPrint(3, "cache control file line %d is bogus",
				   ln);
			continue;
		}
		*s++ = 0;
		sscanf(s, "%d %d %d", &e->modtime, &e->accesstime, &e->pages);
		++e, ++numentries;
	}

	cache_data = data;	/* remember to free this later */
	return true;
}

/* create an ascii equivalent for a record, this is allocated */
static char *record2string(const struct CENTRY *e)
{
	char *t;
	asprintf(&t, "%s\t%05d\t%s\t%d\t%d\t%d\n",
		 e->url, e->filenumber, e->etag, e->modtime, e->accesstime,
		 e->pages);
	return t;
}

/* ON a rare occasion we will have to rewrite the entire control file.
 * If this fails, and it shouldn't, then our only recourse is to clear the cache.
 * If successful, then the file is closed. */
static bool writeControl(void)
{
	struct CENTRY *e;
	int i;
	FILE *f;

	lseek(control_fh, 0L, 0);
	truncate0(cacheControl, control_fh);
/* buffered IO is more efficient */
	f = fdopen(control_fh, "w");

	e = entries;
	for (i = 0; i < numentries; ++i, ++e) {
		int rc;
		char *newrec = record2string(e);
		e->textlength = strlen(newrec);
		rc = fprintf(f, "%s", newrec);
		free(newrec);
		if (rc <= 0) {
			fclose(f);
			control_fh = -1;
			truncate0(cacheControl, -1);
			return false;
		}
	}

	fclose(f);
	control_fh = -1;
	return true;
}

/* create a file number to fold into the file name.
 * This is chosen at random. At worst we should get
 * an unused number in 2 or 3 tries. */
static int generateFileNumber(void)
{
	struct CENTRY *e;
	int i, n;

	while (true) {
		n = rand() % 100000;
		e = entries;
		for (i = 0; i < numentries; ++i, ++e)
			if (e->filenumber == n)
				break;
		if (i == numentries)
			return n;
	}
}

/* get exclusive access to the cache */
static bool setLock(void)
{
	int i;
	int lock_fh;
	time_t lock_t;

	if (!cacheDir)
		return false;
	if (!cacheSize)
		return false;

top:
	time(&now_t);

/* try every 10 ms, 100 times, for a total of 1 second */
	for (i = 0; i < 100; ++i) {
		lock_fh =
		    open(cacheLock, O_WRONLY | O_EXCL | O_CREAT, MODE_private);
		if (lock_fh >= 0) {	/* got it */
			close(lock_fh);
			if (control_fh < 0) {
				control_fh =
				    open(cacheControl, O_RDWR | O_BINARY, 0);
				if (control_fh < 0) {
/* got the lock but couldn't open the database */
					unlink(cacheLock);
					return false;
				}
			}
			if (!readControl()) {
				unlink(cacheLock);
				return false;
			}
			return true;
		}
		if (errno != EEXIST)
			return false;
		USLEEP(10000);
	}

/* if lock file is more than 5 minutes old then something bad has happened,
 * just remove it. */
	lock_t = fileTimeByName(cacheLock);
	if (now_t - lock_t > 5 * 60) {
		if (unlink(cacheLock) == 0)
			goto top;
	}

	return false;
}

static void clearLock(void)
{
	unlink(cacheLock);
}

/* Remove any cached files and initialize the database */
static void clearCacheInternal(void)
{
	struct CENTRY *e;
	int i;

	debugPrint(3, "clear cache");

/* loop through and remove the files */
	e = entries;
	for (i = 0; i < numentries; ++i, ++e) {
		sprintf(cacheFile, "%s/%05d", cacheDir, e->filenumber);
		unlink(cacheFile);
	}

	truncate0(cacheControl, -1);
}

// This function is not used and has not been tested.
// Maybe some day it will be invoked from an edbrowse command.
void clearCache(void)
{
	if (!setLock())
		return;
	close(control_fh);
	control_fh = -1;
	clearCacheInternal();
	free(cache_data);
	clearLock();
}

/* Fetch a file from cache. return true if fetched successfully,
false if the file has not been cached or is stale.
If true then the last access time is set to now.
The data is returned by the pointer provided; if there is no pointer
for the length of the data, then the name of the cache file is returned instead,
wherein the calling routine can access the file directly.
You might think there is a race condition here; some other edbrowse
process fills the cache and removes 100 files, but this file was just accessed,
so is at the top of the list, and won't be removed.
In other words, a destructive race condition is almost impossible. Some goofy
characters are prepended to the filename to help us identify it as such. */

bool fetchCache(const char *url, const char *etag, time_t modtime,
		char **data, int *data_len)
{
	struct CENTRY *e;
	int i;
	char *newrec;
	size_t newlen = 0;

/* you have to give me enough information */
	if (!modtime && (!etag || !*etag))
		return false;

	if (!setLock())
		return false;

/* find the url */
	e = entries;
	for (i = 0; i < numentries; ++i, ++e) {
		if (!sameURL(url, e->url))
			continue;
/* look for match on etag */
		if (e->etag[0] && etag && etag[0]) {
/* both etags are present */
			if (stringEqual(etag, e->etag))
				goto match;
			goto nomatch;
		}
		if (!modtime)
			goto nomatch;
		if (modtime / 8 > e->modtime)
			goto nomatch;
		goto match;
	}
/* url not found */

nomatch:
	free(cache_data);
	clearLock();
	return false;

match:
	sprintf(cacheFile, "%s/%05d", cacheDir, e->filenumber);
	if (data_len) {
		if (!fileIntoMemory(cacheFile, data, data_len))
			goto nomatch;
	} else {
		char *a = allocMem(strlen(cacheFile) + 5 + 1);
		sprintf(a, "`cfn~%s", cacheFile);
		*data = a;
	}

/* file has been pulled from cache */
/* have to update the access time */
	e->accesstime = now_t / 8;
	newrec = record2string(e);
	newlen = strlen(newrec);
	if (newlen == e->textlength) {
		lseek(control_fh, e->offset, 0);
		write(control_fh, newrec, newlen);
	} else {
		if (!writeControl())
			clearCacheInternal();
	}

	debugPrint(3, "from cache");
	free(newrec);
	free(cache_data);
	clearLock();
	return true;
}

/* for quicksort */
/* records sorted by access time in reverse order */
static int entry_cmp(const void *s, const void *t)
{
	return ((struct CENTRY *)t)->accesstime -
	    ((struct CENTRY *)s)->accesstime;
}

/*
 * Is a URL present in the cache?  This can save on HEAD requests,
 * since we can just do a straight GET if the item is not there.
 */
bool presentInCache(const char *url)
{
	bool ret = false;
	struct CENTRY *e;
	int i;

	if (!setLock())
		return false;

	e = entries;

	for (i = 0; (!ret && i < numentries); ++i, ++e) {
		if (!sameURL(url, e->url))
			continue;
		ret = true;
	}

	free(cache_data);
	clearLock();
	return ret;
}

/* Put a file into the cache.
 * Sets the modified time and last access time to now.
 * Time is in 8 second chunks, so even a 32 bit int will hold us for centuries. */

void storeCache(const char *url, const char *etag, time_t modtime,
		const char *data, int datalen)
{
	struct CENTRY *e;
	int i;
	int filenum;
	bool append = false;

	if (!setLock())
		return;

/* leading http:// is the default, and not needed in the control file.
 * sameURL() takes care of all that. */
	if (memEqualCI(url, "http://", 7))
		url += 7;

/* find the url */
	e = entries;
	for (i = 0; i < numentries; ++i, ++e) {
		if (sameURL(url, e->url))
			break;
	}

	if (i < numentries)
		filenum = e->filenumber;
	else
		filenum = generateFileNumber();
	sprintf(cacheFile, "%s/%05d", cacheDir, filenum);
	if (!memoryOutToFile(cacheFile, data, datalen,
			     MSG_TempNoCreate2, MSG_NoWrite2)) {
/* oops, can't write the file */
		unlink(cacheFile);
		debugPrint(3, "cannot write web page into cache");
		free(cache_data);
		clearLock();
		return;
	}

	if (i < numentries) {
		char *newrec;
		size_t newlen;
/* we're just updating a preexisting record */
		e->accesstime = now_t / 8;
		e->modtime = modtime / 8;
		e->etag = (etag ? etag : emptyString);
		e->pages = (datalen + 4095) / 4096;
		newrec = record2string(e);
		newlen = strlen(newrec);
		if (newlen == e->textlength) {
/* record is the same length, just update it */
			lseek(control_fh, e->offset, 0);
			write(control_fh, newrec, newlen);
			debugPrint(3, "into cache");
			free(cache_data);
			free(newrec);
			clearLock();
			return;
		}

/* Record has changed length, have to rewrite the whole control file */
		e->textlength = newlen;
		if (!writeControl())
			clearCacheInternal();
		else
			debugPrint(3, "into cache");
		free(cache_data);
		clearLock();
		return;
	}

/* this file is new. See if the database is full. */
	append = true;
	if (numentries >= 140) {
		int npages = 0;
		e = entries;
		for (i = 0; i < numentries; ++i, ++e)
			npages += e->pages;

		if (numentries == cacheCount || npages / 256 >= cacheSize) {
/* sort to find the 100 oldest files */
			qsort(entries, numentries, sizeof(struct CENTRY),
			      entry_cmp);
			debugPrint(3,
				   "cache is full; removing the 100 oldest files");
			e = entries + numentries - 100;
			for (i = 0; i < 100; ++i, ++e) {
				sprintf(cacheFile, "%s/%05d", cacheDir,
					e->filenumber);
				unlink(cacheFile);
			}
			numentries -= 100;
			append = false;
		}
	}

	e = entries + numentries;
	++numentries;
	e->url = url;
	e->filenumber = filenum;
	e->etag = (etag ? etag : emptyString);
	e->accesstime = now_t / 8;
	e->modtime = modtime / 8;
	e->pages = (datalen + 4095) / 4096;

	if (append) {
/* didn't have to prune; just append this record */
		char *newrec = record2string(e);
		e->textlength = strlen(newrec);
		lseek(control_fh, 0L, 2);
		write(control_fh, newrec, e->textlength);
		debugPrint(3, "into cache");
		free(cache_data);
		free(newrec);
		clearLock();
		return;
	}

/* have to rewrite the whole control file */
	if (!writeControl())
		clearCacheInternal();
	else
		debugPrint(3, "into cache");
	free(cache_data);
	clearLock();
}

/* user password authorization for web access
 * (c) 2002 Mikulas Patocka
 * This file is part of the Links project, released under GPL.
 * Modified by Karl Dahlke for integration with edbrowse.
 */

struct httpAuth {
	struct httpAuth *next;
	struct httpAuth *prev;
/* These strings are allocated. */
	char *host;
	char *directory;
	char *realm;
	char *user_password;
	int port;
	bool proxy;
};

static struct listHead authlist = { &authlist, &authlist };

bool getUserPass(const char *url, char *creds, bool find_proxy)
{
	int port = getPortURL(url);
	char host[MAXHOSTLEN];
	const char *dir, *dirend;
	struct httpAuth *a;
	struct httpAuth *found = NULL;
	int d1len, d2len;

	if (!getProtHostURL(url, NULL, host))
		return false;

	getDirURL(url, &dir, &dirend);
	d2len = dirend - dir;

	foreach(a, authlist) {
		if (found == NULL && a->proxy == find_proxy &&
		    stringEqualCI(a->host, host) && a->port == port) {
			if (!a->proxy) {
/* Directory match not done for proxy records. */
				d1len = strlen(a->directory);
				if (d1len > d2len)
					continue;
				if (memcmp(a->directory, dir, d1len))
					continue;
				found = a;
			} else	/* not proxy */
				found = a;
		}
	}

	if (found)
		strcpy(creds, found->user_password);

	return (found != NULL);
}

bool getUserPassRealm(const char *url, char *creds, const char *realm)
{
	char host[MAXHOSTLEN];
	int port = getPortURL(url);
	struct httpAuth *a;
	struct httpAuth *found = NULL;

	if (!getProtHostURL(url, NULL, host))
		return false;

	foreach(a, authlist) {
		if (found == NULL && stringEqualCI(a->host, host) &&
		    a->port == port) {
			if (!a->realm)
				continue;
			if (strcmp(a->realm, realm))
				continue;
			found = a;
		}
	}

	if (found)
		strcpy(creds, found->user_password);

	return (found != NULL);
}

bool
addWebAuthorization(const char *url,
		    const char *credentials, bool proxy, const char *realm)
{
	char host[MAXHOSTLEN];
	struct httpAuth *a;
	const char *dir = 0, *dirend;
	int port, dl;
	bool urlProx = isProxyURL(url);
	bool updated = false;

	if (proxy) {
		if (!urlProx) {
			setError(MSG_ProxyAuth);
			return false;
		}
	} else if (urlProx)
		url = getDataURL(url);

	if (!getProtHostURL(url, NULL, host))
		return false;
	port = getPortURL(url);
	if (!proxy) {
		getDirURL(url, &dir, &dirend);
		dl = dirend - dir;
	}

/* See if we've done this one before. */
	foreach(a, authlist) {
		if (a->proxy == proxy &&
		    a->port == port &&
		    stringEqualCI(a->host, host) &&
		    (proxy || (dl == (int)strlen(a->directory)
			       && !memcmp(a->directory, dir, dl)))) {
			char *s = cloneString(credentials);
			char *t = a->user_password;
			a->user_password = s;
			nzFree(t);
			updated = true;
			break;
		}
	}

	if (!updated) {
		a = allocZeroMem(sizeof(struct httpAuth));
		a->proxy = proxy;
		a->port = port;
		if (!a->host)
			a->host = cloneString(host);
		if (dir && !a->directory)
			a->directory = pullString1(dir, dirend);
		if (realm && !a->realm)
			a->realm = cloneString(realm);
		a->user_password = cloneString(credentials);
		addToListFront(&authlist, a);
	}

	debugPrint(3, "%s authorization for %s%s",
		   updated ? "updated" : "new", a->host, a->directory);
	return true;
}
