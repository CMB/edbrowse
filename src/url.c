/* url.c
 * Separate a url into pieces, turn a relative file into an absolute url.
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"

struct PROTOCOL {
	const char prot[MAXPROTLEN];
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
	"imap", 220, false, true, true}, {
	"imaps", 993, false, true, true}, {
	"smtp", 25, false, true, true}, {
	"submission", 587, false, true, true}, {
	"smtps", 465, false, true, true}, {
	"proxy", 3128, false, true, true}, {
	"ftp", 21, false, true, true}, {
	"sftp", 22, false, true, true}, {
	"scp", 22, false, true, true}, {
	"ftps", 990, false, true, true}, {
	"tftp", 69, false, true, true}, {
	"rtsp", 554, false, true, true}, {
	"pnm", 7070, false, true, true}, {
	"finger", 79, false, true, true}, {
	"smb", 139, false, true, true}, {
	"mailto", 0, false, false, false}, {
	"telnet", 23, false, false, false}, {
	"tn3270", 0, false, false, false}, {
	"data", 0, true, false, false}, {
	"javascript", 0, true, false, false}, {
	"git", 0, false, true, false}, {
	"svn", 0, false, true, false}, {
	"gopher", 70, false, true, true}, {
	"magnet", 0, false, false, false}, {
	"irc", 0, false, true, false}, {
"", 0},};

static int protocolByName(const char *p, int l)
{
	int i;
	for (i = 0; protocols[i].prot[0]; i++)
		if (strlen(protocols[i].prot) == l &&
		    memEqualCI(protocols[i].prot, p, l))
			return i;
	return -1;
}				/* protocolByName */

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
}				/* unpercentURL */

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
}				/* unpercentString */

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
 * The order is important.
 * Google has commas in encoded URLs, and wikipedia has parentheses,
 * so those are (sort of) ok. */
static const char percentable[] = "+,()'\"\\<>!*[]$";
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
}				/* percentURL */

// For debugging only.
bool looksPercented(const char *start, const char *end)
{
	const char *s;
	if (!end)
		end = start + strlen(start);
	for (s = start; s < end; ++s)
		if (*s < ' ' || strchr(percentable + 5, *s))
			return false;
	return true;
}				/* looksPercented */

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
}				/* htmlEscape0 */

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
}				/* httpDefault */

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

/* Find the leading protocol:// */
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
			++q;
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
#if 0
// not sure why I had this code
		if (a < 0 && q == p + 1)
			return false;
#endif
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

/* find the end of the domain */
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

/* again, look for the end of the domain */
	q = p + strcspn(p, ":?#/\1");
// only domain characters allowed
	for (pp = p; pp < q; ++pp)
		if (!isalnumByte(*pp) && *pp != '.' && *pp != '-')
			return false;
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
}				/* parseURL */

bool isURL(const char *url)
{
	return parseURL(url, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}				/* isURL */

bool isSQL(const char *s)
{
	char c = *s;
	const char *c1 = 0;

	if (!sqlPresent)
		goto no;

	if (isURL(s))
		goto no;

// look for word] or word:word]
	if (!isalphaByte(c))
		goto no;

	for (++s; (c = *s); ++s) {
		if (c == '_')
			continue;
		if (isalnumByte(c))
			continue;
		if (c == ':') {
			if (c1)
				goto no;
			c1 = s;
			continue;
		}
		if (c == ']')
			goto yes;
	}

no:
	return false;

yes:
	return true;
}				/* isSQL */

// non-FTP URLs are always browsable.  FTP URLs are browsable if they end with
//a slash. gopher urls are a bit more complicated, not yet implemented.
bool isBrowseableURL(const char *url)
{
	if (isURL(url))
		return (!memEqualCI(url, "ftp://", 6))
		    || (url[strlen(url) - 1] == '/');
	else
		return false;
}				/* isBrowseableURL */

bool isDataURI(const char *u)
{
	return u && memEqualCI(u, "data:", 5);
}				/* isDataURI */

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
}				/* getProtURL */

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
}				/* getProtHostURL */

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
}				/* getDataURL */

// return null for free syntax
static const char *getDataURL1(const char *url)
{
	const char *s;
	bool fs;
	bool rc = parseURL(url, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, &s, 0, 0, &fs);
	if (!rc || fs)
		return 0;
	return s;
}				/* getDataURL1 */

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
}				/* getDirURL */

/* #tag is only meaningfull after the last slash */
char *findHash(const char *s)
{
	const char *t = strrchr(s, '/');
	if (t)
		s = t;
	return (char *)strchr(s, '#');
}				/* findHash */

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
}				/* getFileURL */

bool getPortLocURL(const char *url, const char **portloc, int *port)
{
	bool fs;
	bool rc =
	    parseURL(url, 0, 0, 0, 0, 0, 0, 0, 0, portloc, port, 0, 0, 0, &fs);
	if (!rc || fs)
		return false;
	return true;
}				/* getPortLocURL */

int getPortURL(const char *url)
{
	int port;
	bool fs;
	bool rc = parseURL(url, 0, 0, 0, 0, 0, 0, 0, 0, 0, &port, 0, 0, 0, &fs);
	if (!rc || fs)
		return 0;
	return port;
}				/* getPortURL */

bool isProxyURL(const char *url)
{
	return ((url[0] | 0x20) == 'p');
}

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
	char *s, *end;
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
/* dd could point to : in bogus code such as <A href=crap:foobar> */
/* crap: looks like a slashless protocol, perhaps unknown to us. */
	if (*dd == ':')
		return;
	if (*dd != '/')
		i_printfExit(MSG_BadSlash, url);
	end = dd + strcspn(dd, "?\1");
	rest = cloneString(end);
	inPath = pullString1(dd, end);
	s = inPath;

/* The following algorithm is straight out of RFC 3986, section 5.2.4. */
/* We can ignore several steps because of a loop invariant: */
/* After the test, *s is always a slash. */
	while (*s) {
		if (!strncmp(s, "/./", 3))
			s += 2;	/* Point s at 2nd slash */
		else if (!strcmp(s, "/.")) {
			s[1] = '\0';
			/* We'll copy the segment "/" on the next iteration. */
			/* And that will be the final iteration of the loop. */
		} else if (!strncmp(s, "/../", 4)) {
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

	if (memEqualCI(rel, "data:", 5))
		return cloneString(rel);

	debugPrint(5, "resolve(%s|%s)", base, rel);
	hrefContext = true;
	if (!base)
		base = emptyString;
	if (!rel)
		rel = emptyString;
	n = allocString(strlen(base) + strlen(rel) + 12);

	if (rel[0] == '#') {
/* This is an anchor for the current document, don't resolve. */
/* I assume the base does not have a #fragment on the end; that is not part of the base. */
/* Thus I won't get url#foo#bar */
		strcpy(n, rel);
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
}				/* resolveURL */

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
}				/* sameURL */

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
}				/* altText */

/* get post data ready for a url. */
char *encodePostData(const char *s, const char *keep_chars)
{
	char *post, c;
	int l;
	char buf[4];

	if (!s)
		return 0;
	if (s == emptyString)
		return emptyString;
	if (!keep_chars)
		keep_chars = "-._~()";
	post = initString(&l);
	while ((c = *s++)) {
		if (isalnumByte(c))
			goto putc;
		if (strchr(keep_chars, c))
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
}				/* decodePostData */

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
}				/* decodeMailURL */

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
