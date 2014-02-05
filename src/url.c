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
	eb_bool free_syntax;
	eb_bool need_slashes;
	eb_bool need_slash_after_host;
} protocols[] = {
	{
	"file", 0, eb_true, eb_true, eb_false}, {
	"http", 80, eb_false, eb_true, eb_true}, {
	"https", 443, eb_false, eb_true, eb_true}, {
	"proxy", 3128, eb_false, eb_true, eb_true}, {
	"ftp", 21, eb_false, eb_true, eb_true}, {
	"rtsp", 554, eb_false, eb_true, eb_true}, {
	"pnm", 7070, eb_false, eb_true, eb_true}, {
	"finger", 79, eb_false, eb_true, eb_true}, {
	"smb", 139, eb_false, eb_true, eb_true}, {
	"mailto", 0, eb_false, eb_false, eb_false}, {
	"telnet", 23, eb_false, eb_false, eb_false}, {
	"tn3270", 0, eb_false, eb_false, eb_false}, {
	"javascript", 0, eb_true, eb_false, eb_false}, {
	"git", 0, eb_false, eb_false, eb_false}, {
	"svn", 0, eb_false, eb_false, eb_false}, {
	"gopher", 70, eb_false, eb_false, eb_false}, {
	"magnet", 0, eb_false, eb_false, eb_false}, {
	"irc", 0, eb_false, eb_false, eb_false}, {
	NULL, 0}
};

static eb_bool free_syntax;

static int protocolByName(const char *p, int l)
{
	int i;
	for (i = 0; protocols[i].prot; i++)
		if (memEqualCI(protocols[i].prot, p, l))
			return i;
	return -1;
}				/* protocolByName */

/* Unpercent the host component of a url.  Christ what a pain! */
void unpercentURL(char *url)
{
	char c, *u, *w;
	int n;
	u = w = url;
	while (c = *u) {
		++u;
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
}				/* unpercentURL */

/* Decide if it looks like a web url. */
static eb_bool httpDefault(const char *url)
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
		while (isdigitByte(*s))
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
		return eb_false;
/* All digits, like an ip address, is ok. */
	if (n == 3) {
		for (s = url; s < end; ++s)
			if (!isdigitByte(*s) && *s != '.')
				break;
		if (s == end)
			return eb_true;
	}
/* Look for standard domain suffix */
	++lastdot;
	len = end - lastdot;
	for (n = 0; domainSuffix[n]; ++n)
		if (memEqualCI(lastdot, domainSuffix[n], len)
		    && !domainSuffix[n][len])
			return eb_true;
/* www.anything.xx is ok */
	if (len == 2 && memEqualCI(url, "www.", 4))
		return eb_true;
	return eb_false;
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
	free_syntax = eb_false;

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
		while (isspaceByte(*q))
			++q;
		if (!*q)
			return eb_false;
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
					setError(MSG_ProtExpected,
						 protocols[a].prot);
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
				if (!isalphaByte(*q))
					break;
			if (q == p) {	/* some protocol we don't know */
				char qprot[12];
				memcpy(qprot, url, p - url);
				qprot[p - url] = 0;
				setError(MSG_BadProt, qprot);
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
		return eb_false;

	if (free_syntax = protocols[a].free_syntax) {
		if (data)
			*data = p;
		if (dalen)
			*dalen = strlen(p);
		return eb_true;
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
			if (cc != pp || !isdigitByte(q[1])) {
				setError(MSG_BadPort);
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
	return eb_true;
}				/* parseURL */

eb_bool isURL(const char *url)
{
	int j = parseURL(url, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	if (j < 0)
		return eb_false;
	return j;
}				/* isURL */

/* non-FTP URLs are always browsable.  FTP URLs are browsable if they end with
* a slash. */
eb_bool isBrowseableURL(const char *url)
{
	if (isURL(url))
		return (!memEqualCI(url, "ftp://", 6))
		    || (url[strlen(url) - 1] == '/');
	else
		return eb_false;
}				/* isBrowseableURL */

/* Helper functions to return pieces of the URL.
 * Makes a copy, so you can have your 0 on the end.
 * Return 0 for an error, and "" if that piece is missing. */

const char *getProtURL(const char *url)
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
const char *getHostURL(const char *url)
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
		return EMPTYSTRING;
	if (l >= sizeof(hostbuf)) {
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
}				/* getHostURL */

const char *getHostPassURL(const char *url)
{
	int hl;
	const char *h, *z, *u;
	char *t;
	int rc = parseURL(url, 0, 0, &u, 0, 0, 0, &h, &hl, 0, 0, 0, 0, 0);
	if (rc <= 0 || !h)
		return 0;
	if (free_syntax)
		return 0;
	z = h;
	t = hostbuf;
	if (u)
		z = u, hl += h - u, t += h - u;
	if (hl >= sizeof(hostbuf)) {
		setError(MSG_DomainLong);
		return 0;
	}
	memcpy(hostbuf, z, hl);
	hostbuf[hl] = 0;
/* domain names must be ascii */
	for (; *t; ++t)
		*t &= 0x7f;
	return hostbuf;
}				/* getHostPassURL */

const char *getUserURL(const char *url)
{
	static char buf[MAXUSERPASS];
	int l;
	const char *s;
	int rc = parseURL(url, 0, 0, &s, &l, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	if (rc <= 0)
		return 0;
	if (free_syntax)
		return EMPTYSTRING;
	if (!s)
		return EMPTYSTRING;
	if (l >= sizeof(buf)) {
		setError(MSG_UserNameLong2);
		return 0;
	}
	memcpy(buf, s, l);
	buf[l] = 0;
	return buf;
}				/* getUserURL */

const char *getPassURL(const char *url)
{
	static char buf[MAXUSERPASS];
	int l;
	const char *s;
	int rc = parseURL(url, 0, 0, 0, 0, &s, &l, 0, 0, 0, 0, 0, 0, 0);
	if (rc <= 0)
		return 0;
	if (free_syntax)
		return EMPTYSTRING;
	if (!s)
		return EMPTYSTRING;
	if (l >= sizeof(buf)) {
		setError(MSG_PasswordLong2);
		return 0;
	}
	memcpy(buf, s, l);
	buf[l] = 0;
	return buf;
}				/* getPassURL */

const char *getDataURL(const char *url)
{
	const char *s;
	int rc = parseURL(url, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, &s, 0, 0);
	if (rc <= 0)
		return 0;
	return s;
}				/* getDataURL */

void getDirURL(const char *url, const char **start_p, const char **end_p)
{
	const char *dir = getDataURL(url);
	const char *end;
	static const char myslash[] = "/";
	if (!dir || dir == url)
		goto slash;
	if (free_syntax)
		goto slash;
	if (!strchr("#?\1", *dir)) {
		if (*--dir != '/')
			i_printfExit(MSG_BadDirSlash, url);
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

eb_bool getPortLocURL(const char *url, const char **portloc, int *port)
{
	int rc = parseURL(url, 0, 0, 0, 0, 0, 0, 0, 0, portloc, port, 0, 0, 0);
	if (rc <= 0)
		return eb_false;
	if (free_syntax)
		return eb_false;
	return eb_true;
}				/* getPortLocURL */

int getPortURL(const char *url)
{
	int port;
	int rc = parseURL(url, 0, 0, 0, 0, 0, 0, 0, 0, 0, &port, 0, 0, 0);
	if (rc <= 0)
		return 0;
	if (free_syntax)
		return 0;
	return port;
}				/* getPortURL */

eb_bool isProxyURL(const char *url)
{
	return ((url[0] | 0x20) == 'p');
}

/*
 * hasPrefix: return true if s has a prefix of p, false otherwise.
 */
static eb_bool hasPrefix(char *s, char *p)
{
	eb_bool ret = eb_false;
	if (!p[0])
		ret = eb_true;	/* Empty string is a prefix of all strings. */
	else {
		size_t slen = strlen(s);
		size_t plen = strlen(p);
		ret = (plen <= slen) && !strncmp(p, s, plen);
	}
	return ret;
}				/* hasPrefix */

/*
 * copyPathSegment: copy everything from *src, starting with the leftmost
 * character (a slash), and ending with either the next slash (not included)
 * or the end of the string.
 * Advance *src to point to the character succeeding the copied text.
 */
static void copyPathSegment(char **src, char **dest, int *destlen)
{
	int spanlen = strcspn(*src + 1, "/") + 1;
	stringAndBytes(dest, destlen, *src, spanlen);
	*src = *src + spanlen;
}				/* copyPathSegment */

/*
 * Remove the rightmost component of a path,
 * including the preceding slash, if any.
 */
static void snipLastSegment(char **path, int *pathLen)
{
	char *rightmostSlash = strrchr(*path, '/');
	if (rightmostSlash == NULL)
		rightmostSlash = *path;
	*rightmostSlash = '\0';
	*pathLen = rightmostSlash - *path;
}				/* snipLastSegment */

static void squashDirectories(char *url)
{
	char *dd = (char *)getDataURL(url);
	char *s, *t, *end;
	char *inPath = NULL;
	char *outPath;
	int outPathLen = 0;
	char *rest = NULL;

	outPath = initString(&outPathLen);
	if (memEqualCI(url, "javascript:", 11))
		return;
	if (!dd || dd == url)
		return;
	if (!*dd)
		return;
	if (strchr("#?\1", *dd))
		return;
	--dd;
	if (*dd != '/')
		i_printfExit(MSG_BadSlash, url);
	end = dd + strcspn(dd, "?#\1");
	rest = cloneString(end);
	inPath = pullString1(dd, end);
	s = inPath;

/* The following algorithm is straight out of RFC 3986, section 5.2.4. */
/* We can ignore several steps because of a loop invariant: */
/* After the test, *s is always a slash. */
	while (*s) {
		if (hasPrefix(s, "/./"))
			s += 2;	/* Point s at 2nd slash */
		else if (!strcmp(s, "/.")) {
			s[1] = '\0';
			/* We'll copy the segment "/" on the next iteration. */
			/* And that will be the final iteration of the loop. */
		} else if (hasPrefix(s, "/../")) {
			s += 3;	/* Point s at 2nd slash */
			snipLastSegment(&outPath, &outPathLen);
		} else if (!strcmp(s, "/..")) {
			s[1] = '\0';
			snipLastSegment(&outPath, &outPathLen);
			/* As above, copy "/" on the next and final iteration. */
		} else
			copyPathSegment(&s, &outPath, &outPathLen);
	}
	*dd = '\0';
	strcat(url, outPath);
	strcat(url, rest);
	nzFree(inPath);
	nzFree(outPath);
	nzFree(rest);
}				/* squashDirectories */

char *resolveURL(const char *base, const char *rel)
{
	char *n;		/* new url */
	const char *s, *p;
	char *q;
	int l;
	if (!base)
		base = EMPTYSTRING;
	n = allocMem(strlen(base) + strlen(rel) + 12);
	debugPrint(5, "resolve(%s|%s)", base, rel);
	if (rel[0] == '#') {
/* # alone means do nothing. */
		if (!rel[1]) {
			strcpy(n, rel);
out_n:
			debugPrint(5, "= %s", n);
			return n;
		}
/* We could have changed the base url via the <base> tag,
 * so this #ref could actually refer to some other web page.
 * Best to run through standard procedure. */
		strcpy(n, base);
		for (q = n; *q && *q != '\1' && *q != '#'; q++) ;
		strcpy(q, rel);
		goto out_n;
	}
	if (rel[0] == '?' || rel[0] == '\1') {
		strcpy(n, base);
		for (q = n; *q && *q != '\1' && *q != '#' && *q != '?'; q++) ;
		strcpy(q, rel);
		goto out_n;
	}
	if (rel[0] == '/' && rel[1] == '/') {
		if (s = strstr(base, "//")) {
			strncpy(n, base, s - base);
			n[s - base] = 0;
		} else
			strcpy(n, "http:");
		strcat(n, rel);
		goto squash;
	}
	if (parseURL(rel, &s, &l, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) > 0) {
		n[0] = 0;
		if (s != rel) {
/* It didn't have http in front of it before, put it on now. */
			strncpy(n, s, l);
			strcpy(n + l, "://");
		}
		strcat(n, rel);
		goto squash;
	}			/* relative is already a url */
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
	if (parseURL(base, 0, 0, 0, 0, 0, 0, &p, 0, 0, 0, 0, 0, 0) > 0 && p)
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
}				/* resolveURL */

/* This routine could be, should be, more sophisticated */
eb_bool sameURL(const char *s, const char *t)
{
	const char *u, *v;
	int l;
/* It's ok if one says http and the other implies it. */
	if (memEqualCI(s, "http://", 7))
		s += 7;
	if (memEqualCI(t, "http://", 7))
		t += 7;
	u = s + strcspn(s, "#");
	v = t + strcspn(t, "#?\1");
	if (u - s >= 7 && stringEqual(u - 7, ".browse"))
		u -= 7;
	if (v - t >= 7 && stringEqual(v - 7, ".browse"))
		v -= 7;
	l = u - s;
	if (l != v - t)
		return eb_false;
	return !memcmp(s, t, l);
}				/* sameURL */

/* Find some helpful text to print, in place of an image. */
/* Text longer than 80 chars isn't helpful, so we return a static buffer. */
char *altText(const char *base)
{
	static char buf[80];
	int len, n;
	int recount = 0;
	char *s;
	debugPrint(5, "altText(%s)", base);
	if (!base)
		return 0;
	if (stringEqual(base, "#"))
		return 0;
	if (memEqualCI(base, "javascript", 10))
		return 0;
retry:
	if (recount >= 2)
		return 0;
	strncpy(buf, base, sizeof(buf) - 1);
	spaceCrunch(buf, eb_true, eb_false);
	len = strlen(buf);
	if (len && !isalnumByte(buf[len - 1]))
		buf[--len] = 0;
	while (len && !isalnumByte(buf[0]))
		strmove(buf, buf + 1), --len;
	if (len > 10) {
/* see if it's a phrase/sentence or a pathname/url */
/* Do this by counting spaces */
		for (n = 0, s = buf; *s; ++s)
			if (*s == ' ')
				++n;
		if (n * 8 >= len)
			return buf;	/* looks like words */
/* Ok, now we believe it's a pathname or url */
/* Get rid of everything after ? or # */
		s = strpbrk(buf, "#?\1");
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
		s = strrchr(buf, '/');
		if (s && recount) {
			char *ss;
			*s = 0;
			ss = strrchr(buf, '/');
			if (!ss)
				return 0;
			if (ss > buf && ss[-1] == '/')
				return 0;
			*s = '/';
			s = ss;
		}
		if (s)
			strmove(buf, s + 1);
	}			/* more than ten characters */
	++recount;
/* If we don't have enough letters, forget it */
	len = strlen(buf);
	if (len < 3)
		goto retry;
	for (n = 0, s = buf; *s; ++s)
		if (isalphaByte(*s))
			++n;
	if (n * 2 <= len)
		return 0;	/* not enough letters */
	return buf;
}				/* altText */

/* get post data ready for a url. */
char *encodePostData(const char *s)
{
	char *post, c;
	int l;
	char buf[4];

	if (!s)
		return 0;
	if (s == EMPTYSTRING)
		return EMPTYSTRING;
	post = initString(&l);
	while (c = *s++) {
		if (isalnumByte(c))
			goto putc;
		if (c == ' ') {
			c = '+';
			goto putc;
		}
		if (strchr("-._~*()!", c))
			goto putc;
		sprintf(buf, "%%%02X", (uchar) c);
		stringAndString(&post, &l, buf);
		continue;
putc:
		stringAndChar(&post, &l, c);
	}
	return post;
}				/* encodePostData */

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
}				/* dohex */

char *decodePostData(const char *data, const char *name, int seqno)
{
	const char *s, *n, *t;
	char *new = 0, *w = 0;
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
			w = new = allocMem(t - s + 1);
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
			return new;
		}
		if (w)
			*w++ = c;
		++s;		/* skip past equals */
		if (name) {
			if (!n)
				continue;
			if (*n)
				continue;
			w = new = allocMem(t - s + 1);
		}

/* At this point we have a match */
		while (s < t) {
			c = *s++;
			c = dohex(c, &s);
			*w++ = c;
		}
		*w = 0;
		return new;
	}

	return 0;
}				/* decodePostData */

void decodeMailURL(const char *url, char **addr_p, char **subj_p, char **body_p)
{
	const char *s;
	char *new;
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
}				/* decodeMailURL */
