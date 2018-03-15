/* http.c
 * HTTP protocol client implementation
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"

#ifdef _MSC_VER
#include <fcntl.h>
#else
#include <sys/wait.h>
#endif
#include <time.h>

char *serverData;
int serverDataLen;
CURL *global_http_handle;
CURLSH *global_share_handle;
bool pluginsOn = true;
bool down_bg;			/* download in background */
char showProgress = 'd';	// dots
static char *httpLanguage;	/* outgoing */

struct BG_JOB {
	struct BG_JOB *next, *prev;
	int pid, state;
	size_t fsize;		// file size
	int file2;		// offset into filename
	char file[4];
};
static struct listHead down_jobs = {
	&down_jobs, &down_jobs
};

static void background_download(struct i_get *g);
static void setup_download(struct i_get *g);
static CURL *http_curl_init(struct i_get *g);
static size_t curl_header_callback(char *header_line, size_t size, size_t nmemb,
				   struct i_get *g);
static bool ftpConnect(struct i_get *g, char *creds_buf);
static bool gopherConnect(struct i_get *g);
static bool read_credentials(char *buffer);
static const char *message_for_response_code(int code);
static const char *findProxyForURL(const char *url);

/* string is allocated. Quotes are removed. No other processing is done.
 * You may need to decode %xx bytes or such. */
static char *find_http_header(struct i_get *g, const char *name)
{
	char *s, *t, *u, *v;
	int namelen = strlen(name);
	char *h = g->headers;
	if (!h)
		return NULL;
	for (s = h; *s; s = v) {
/* find start of next line */
		v = strchr(s, '\n');
		if (!v)
			break;
		++v;

/* name: value */
		t = strchr(s, ':');
		if (!t || t >= v)
			continue;
		u = t;
		while (u > s && isspace(u[-1]))
			--u;
		if (u - s != namelen)
			continue;
		if (!memEqualCI(s, name, namelen))
			continue;

/* This is a match */
		++t;
		while (t < v && isspace(*t))
			++t;
		u = v;
		while (u > t && isspace(u[-1]))
			--u;
/* remove quotes */
		if (u - t >= 2 && *t == u[-1] && (*t == '"' || *t == '\''))
			++t, --u;
		if (u == t)
			return NULL;
		return pullString(t, u - t);
	}

	return NULL;
}				/* find_http_header */

static void scan_http_headers(struct i_get *g, bool fromCallback)
{
	char *v;

	if (!g->content[0] && (v = find_http_header(g, "content-type"))) {
		strncpy(g->content, v, sizeof(g->content) - 1);
		caseShift(g->content, 'l');
		nzFree(v);
		debugPrint(3, "content %s", g->content);
		g->charset = strchr(g->content, ';');
		if (g->charset)
			*(g->charset)++ = 0;
		if (g->pg_ok && !cf->mt)
			cf->mt = findMimeByContent(g->content);
	}

	if (!g->cdfn && (v = find_http_header(g, "content-disposition"))) {
		char *s = strstrCI(v, "filename=");
		if (s && !strncmp(v, "attachment", 10)) {
			s += 9;
			if (*s == '"') {
				char *t;
				++s;
				t = strchr(s, '"');
				if (t)
					*t = 0;
			}
			g->cdfn = cloneString(s);
			debugPrint(4, "disposition filename %s", g->cdfn);
// I'm not ready to do this part yet.
#if 0
			if (g->pg_ok && !cf->mt)
				cf->mt = findMimeByFile(g->cdfn);
#endif
		}
		nzFree(v);
	}

	if (!g->hcl && (v = find_http_header(g, "content-length"))) {
		g->hcl = atoi(v);
		nzFree(v);
		if (g->hcl)
			debugPrint(4, "content length %d", g->hcl);
	}

	if (!g->etag && (v = find_http_header(g, "etag"))) {
		g->etag = v;
		debugPrint(4, "etag %s", g->etag);
	}

	if (g->cacheable && (v = find_http_header(g, "cache-control"))) {
		caseShift(v, 'l');
		if (strstr(v, "no-cache")) {
			g->cacheable = false;
			debugPrint(4, "no cache");
		}
		nzFree(v);
	}

	if (g->cacheable && (v = find_http_header(g, "pragma"))) {
		caseShift(v, 'l');
		if (strstr(v, "no-cache")) {
			g->cacheable = false;
			debugPrint(4, "no cache");
		}
		nzFree(v);
	}

	if (!g->modtime && (v = find_http_header(g, "last-modified"))) {
		g->modtime = parseHeaderDate(v);
		if (g->modtime)
			debugPrint(4, "mod date %s", v);
		nzFree(v);
	}
	if (!g->auth_realm[0] && (v = find_http_header(g, "WWW-Authenticate"))) {
		char *realm, *end;
		if ((realm = strstrCI(v, "realm="))) {
			realm += 6;
			if (realm[0] == '"' || realm[0] == '\'') {
				end = strchr(realm + 1, realm[0]);
				realm++;
			} else {
				/* look for space if unquoted */
				end = strchr(realm, ' ');
			}
			if (end) {
				int sz = end - realm;
				if (sz > sizeof(g->auth_realm) - 1)
					sz = sizeof(g->auth_realm) - 1;
				memcpy(g->auth_realm, realm, sz);
				g->auth_realm[sz] = 0;
			} else {
				strncpy(g->auth_realm, realm,
					sizeof(g->auth_realm) - 1);
			}
			debugPrint(4, "auth realm %s", g->auth_realm);
		}
		nzFree(v);
	}

	if (fromCallback)
		return;

	if (!g->newloc && (v = find_http_header(g, "location"))) {
// as though a user had typed it in
		unpercentURL(v);
		g->newloc = v;
	}

	if (!g->newloc && (v = find_http_header(g, "refresh"))) {
		int delay;
		if (parseRefresh(v, &delay)) {
			unpercentURL(v);
			g->newloc = v;
			g->newloc_d = delay;
			v = NULL;
		}
		nzFree(v);
	}
}				/* scan_http_headers */

static void i_get_free(struct i_get *g, bool nodata)
{
	if (nodata) {
		nzFree(g->buffer);
		g->buffer = 0;
		g->length = 0;
	}
	nzFree(g->headers);
	nzFree(g->urlcopy);
	nzFree(g->cdfn);
	nzFree(g->etag);
	nzFree(g->newloc);
	cnzFree(g->down_file);
}

/* actually run the curl request, http or ftp or whatever */
static CURLcode fetch_internet(struct i_get *g)
{
	CURLcode curlret;
	g->buffer = initString(&g->length);
	g->headers = initString(&g->headers_len);
	curlret = curl_easy_perform(g->h);
	if (g->is_http)
		scan_http_headers(g, false);
	return curlret;
}				/* fetch_internet */

/* Callback used by libcurl. Captures data from http, ftp, pop3, gopher.
 * download states:
 * -1 user aborted the download
 * 0 standard in-memory download
 * 1 download but stop and ask user if he wants to download to disk
* 2 disk download foreground
* 3 disk download background parent
* 4 disk download background child
* 5 disk download background prefork
 * 6 mime type says this should be a stream */
size_t
eb_curl_callback(char *incoming, size_t size, size_t nitems, struct i_get * g)
{
	size_t num_bytes = nitems * size;
	int dots1, dots2, rc;

	if (g->down_state == 1 && g->is_http) {
/* don't do a download unless the code is 200. */
		curl_easy_getinfo(g->h, CURLINFO_RESPONSE_CODE, &(g->code));
		if (g->code != 200)
			g->down_state = 0;
	}

	if (g->down_state == 1) {
		if (g->hcl == 0) {
// http should always set http content length, this is just for ftp.
// And ftp downloading a file always has state = 1 on the first data block.
			double d_size = 0.0;	// download size, if we can get it
			curl_easy_getinfo(g->h,
					  CURLINFO_CONTENT_LENGTH_DOWNLOAD,
					  &d_size);
			g->hcl = d_size;
			if (g->hcl < 0)
				g->hcl = 0;
		}

/* state 1, first data block, ask the user */
		setup_download(g);
		if (g->down_state == 0)
			goto showdots;
		if (g->down_state == -1 || g->down_state == 5)
			return -1;
	}

	if (g->down_state == 2 || g->down_state == 4) {	/* to disk */
		rc = write(g->down_fd, incoming, num_bytes);
		if (rc == num_bytes) {
			if (g->down_state == 4) {
#if 0
// Deliberately delay background download, to get several running in parallel
// for testing purposes.
				if (g->down_length == 0)
					sleep(12);
				g->down_length += rc;
#endif
				return rc;
			}
			goto showdots;
		}
		if (g->down_state == 2) {
// has to be the foreground http thread, so ok to call setErro,
// which is not threadsafe.
			setError(MSG_NoWrite2, g->down_file);
			return -1;
		}
		i_printf(MSG_NoWrite2, g->down_file);
		printf(", ");
		i_puts(MSG_DownAbort);
// return -1;
		exit(1);
	}

showdots:
	dots1 = g->length / CHUNKSIZE;
	if (g->down_state == 0)
		stringAndBytes(&g->buffer, &g->length, incoming, num_bytes);
	else
		g->length += num_bytes;
	dots2 = g->length / CHUNKSIZE;
	if (showProgress != 'q' && dots1 < dots2) {
		if (showProgress == 'd') {
			for (; dots1 < dots2; ++dots1)
				putchar('.');
			fflush(stdout);
		}
		if (showProgress == 'c' && g->hcl)
			printf("%d/%d\n", dots2,
			       (g->hcl + CHUNKSIZE - 1) / CHUNKSIZE);
	}
	return num_bytes;
}

/* We want to be able to abort transfers when SIGINT is received. 
 * During data transfers, libcurl ignores EINTR.  So there's no obvious way
 * to abort a transfer on SIGINT.
 * However, libcurl does call a function periodically, to indicate the
 * progress of the transfer.  If the progress function returns a non-zero
 * value, then libcurl aborts the transfer.  The nice thing about libcurl
 * is that it uses timeouts when reading and writing.  It won't block
 * forever in some system call.
 * We can be certain that libcurl will, in fact, call the progress function
 * periodically.
 * Note: libcurl doesn't start calling the progress function until after the
 * connection is made.  So it can block indefinitely during connect().
 * All of the progress arguments to the function are unused. */

static int
curl_progress(void *unused, double dl_total, double dl_now,
	      double ul_total, double ul_now)
{
	int ret = 0;
// ^c will interrupt an http or ftp download.
// Perhaps that is hanging, and blocking edbrowse.
	if (intFlag) {
		i_puts(MSG_Interrupted);
		ret = 1;
	}
	return ret;
}				/* curl_progress */

uchar base64Bits(char c)
{
	if (isupperByte(c))
		return c - 'A';
	if (islowerByte(c))
		return c - ('a' - 26);
	if (isdigitByte(c))
		return c - ('0' - 52);
	if (c == '+')
		return 62;
	if (c == '/')
		return 63;
	return 64;		/* error */
}				/* base64Bits */

/*********************************************************************
Decode some data in base64.
This function operates on the data in-line.  It does not allocate a fresh
string to hold the decoded data.  Since the data will be smaller than
the base64 encoded representation, this cannot overflow.
If you need to preserve the input, copy it first.
start points to the start of the input
*end initially points to the byte just after the end of the input
Returns: GOOD_BASE64_DECODE on success, BAD_BASE64_DECODE or
EXTRA_CHARS_BASE64_DECODE on error.
When the function returns success, *end points to the end of the decoded
data.  On failure, end points to the just past the end of
what was successfully decoded.
*********************************************************************/

int base64Decode(char *start, char **end)
{
	char *b64_end = *end;
	uchar val, leftover, mod;
	bool equals;
	int ret = GOOD_BASE64_DECODE;
	char c, *q, *r;
	mod = 0;
	equals = false;
	for (q = r = start; q < b64_end; ++q) {
		c = *q;
		if (isspaceByte(c))
			continue;
		if (equals) {
			if (c == '=')
				continue;
			ret = EXTRA_CHARS_BASE64_DECODE;
			break;
		}
		if (c == '=') {
			equals = true;
			continue;
		}
		val = base64Bits(c);
		if (val & 64) {
			ret = BAD_BASE64_DECODE;
			break;
		}
		if (mod == 0) {
			leftover = val << 2;
		} else if (mod == 1) {
			*r++ = (leftover | (val >> 4));
			leftover = val << 4;
		} else if (mod == 2) {
			*r++ = (leftover | (val >> 2));
			leftover = val << 6;
		} else {
			*r++ = (leftover | val);
		}
		++mod;
		mod &= 3;
	}
	*end = r;
	return ret;
}				/* base64Decode */

static void
unpackUploadedFile(const char *post, const char *boundary,
		   char **postb, int *postb_l)
{
	static const char message64[] = "Content-Transfer-Encoding: base64";
	const int boundlen = strlen(boundary);
	const int m64len = strlen(message64);
	char *post2;
	char *b1, *b2, *b3, *b4;	/* boundary points */
	int unpack_ret;

	*postb = 0;
	*postb_l = 0;
	if (!strstr(post, message64))
		return;

	post2 = cloneString(post);
	b2 = strstr(post2, boundary);
	while (true) {
		b1 = b2 + boundlen;
		if (*b1 != '\r')
			break;
		b1 += 2;
		b1 = strstr(b1, "Content-Transfer");
		b2 = strstr(b1, boundary);
		if (memcmp(b1, message64, m64len))
			continue;
		b1 += m64len - 6;
		strcpy(b1, "8bit\r\n\r\n");
		b1 += 8;
		b1[0] = b1[1] = ' ';
		b3 = b2 - 4;

		b4 = b3;
		unpack_ret = base64Decode(b1, &b4);
		if (unpack_ret != GOOD_BASE64_DECODE)
			mail64Error(unpack_ret);
		/* Should we *really* keep going at this point? */
		strmove(b4, b3);
		b2 = b4 + 4;
	}

	b1 += strlen(b1);
	*postb = post2;
	*postb_l = b1 - post2;
}				/* unpackUploadedFile */

/* Date format is:    Mon, 03 Jan 2000 21:29:33 GMT|[+-]nnnn */
			/* Or perhaps:     Sun Nov  6 08:49:37 1994 */
/* or perhaps 1994-11-06 08:49:37.nnnnZ */
time_t parseHeaderDate(const char *date)
{
	static const char *const months[12] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	time_t t = 0;
	int zone = 0;
	time_t now = 0;
	int y;			/* remember the type of format */
	struct tm *temptm = NULL;
	struct tm tm;
	long utcoffset = 0;
	memset(&tm, 0, sizeof(struct tm));
	tm.tm_isdst = -1;
	const char *date0 = date;	// remember for debugging

	now = time(NULL);
	temptm = localtime(&now);
	if (temptm == NULL)
		goto fail;
#ifndef _MSC_VER
	utcoffset = temptm->tm_gmtoff;
#endif

	if (isdigitByte(date[0]) && isdigitByte(date[1]) &&
	    isdigitByte(date[2]) && isdigitByte(date[3]) && date[4] == '-') {
		y = 2;
		tm.tm_year = atoi(date + 0) - 1900;
		tm.tm_mon = atoi(date + 5) - 1;
		tm.tm_mday = atoi(date + 8);
		date += 11;
		goto f3;
	}

/* skip past day of the week */
	date = strchr(date, ' ');
	if (!date)
		goto fail;
	date++;

	if (isdigitByte(*date)) {	/* first format */
		y = 0;
		if (isdigitByte(date[1])) {
			tm.tm_mday = (date[0] - '0') * 10 + date[1] - '0';
			date += 2;
		} else {
			tm.tm_mday = *date - '0';
			++date;
		}
		if (*date != ' ' && *date != '-')
			goto fail;
		++date;
		for (tm.tm_mon = 0; tm.tm_mon < 12; tm.tm_mon++)
			if (memEqualCI(date, months[tm.tm_mon], 3))
				goto f1;
		goto fail;
f1:
		date += 3;
		if (*date == ' ') {
			date++;
			if (!isdigitByte(date[0]) || !isdigitByte(date[1]) ||
			    !isdigitByte(date[2]) || !isdigitByte(date[3]))
				goto fail;
			tm.tm_year =
			    (date[0] - '0') * 1000 + (date[1] - '0') * 100 +
			    (date[2] - '0') * 10 + date[3] - '0' - 1900;
			date += 4;
		} else if (*date == '-') {
			/* Sunday, 06-Nov-94 08:49:37 GMT */
			date++;
			if (!isdigitByte(date[0]) || !isdigitByte(date[1]))
				goto fail;
			if (!isdigitByte(date[2])) {
				tm.tm_year =
				    (date[0] >=
				     '7' ? 1900 : 2000) + (date[0] - '0') * 10 +
				    date[1] - '0' - 1900;
				date += 2;
			} else {
				tm.tm_year = atoi(date) - 1900;
				date += 4;
			}
		} else
			goto fail;
		if (*date != ' ')
			goto fail;
		date++;
	} else {
/* second format */
		y = 1;
		for (tm.tm_mon = 0; tm.tm_mon < 12; tm.tm_mon++)
			if (memEqualCI(date, months[tm.tm_mon], 3))
				goto f2;
		goto fail;
f2:
		date += 3;
		while (*date == ' ')
			date++;
		if (!isdigitByte(date[0]))
			goto fail;
		tm.tm_mday = date[0] - '0';
		date++;
		if (*date != ' ') {
			if (!isdigitByte(date[0]))
				goto fail;
			tm.tm_mday = tm.tm_mday * 10 + date[0] - '0';
			date++;
		}
		if (*date != ' ')
			goto fail;
		date++;
	}

f3:
/* ready to crack time */
	if (!isdigitByte(date[0]) || !isdigitByte(date[1]))
		goto fail;
	tm.tm_hour = (date[0] - '0') * 10 + date[1] - '0';
	date += 2;
	if (*date != ':')
		goto fail;
	date++;
	if (!isdigitByte(date[0]) || !isdigitByte(date[1]))
		goto fail;
	tm.tm_min = (date[0] - '0') * 10 + date[1] - '0';
	date += 2;
	if (*date != ':')
		goto fail;
	date++;
	if (!isdigitByte(date[0]) || !isdigitByte(date[1]))
		goto fail;
	tm.tm_sec = (date[0] - '0') * 10 + date[1] - '0';
	date += 2;
	if (y == 2)
		goto f4;

	if (y == 1) {
/* year is at the end */
		if (*date != ' ')
			goto fail;
		date++;
		if (!isdigitByte(date[0]) || !isdigitByte(date[1]) ||
		    !isdigitByte(date[2]) || !isdigitByte(date[3]))
			goto fail;
		tm.tm_year =
		    (date[0] - '0') * 1000 + (date[1] - '0') * 100 + (date[2] -
								      '0') *
		    10 + date[3] - '0' - 1900;
		date += 4;
	}

	if (*date != ' ' && *date)
		goto fail;

	while (*date == ' ')
		++date;
	if ((*date == '+' || *date == '-') &&
	    isdigit(date[1]) && isdigit(date[2]) &&
	    isdigit(date[3]) && isdigit(date[4])) {
		zone = 10 * (date[1] - '0') + date[2] - '0';
		zone *= 60;
		zone += 10 * (date[3] - '0') + date[4] - '0';
		zone *= 60;
/* adjust to gmt */
		if (*date == '+')
			zone = -zone;
	}

f4:
	t = mktime(&tm);
	if (t != (time_t) - 1)
		return t + zone + utcoffset;

fail:
	debugPrint(3, "parseHeaderDate fails on %s", date0);
	return 0;
}				/* parseHeaderDate */

bool parseRefresh(char *ref, int *delay_p)
{
	int delay = 0;
	char *u = ref;
	if (isdigitByte(*u))
		delay = atoi(u);
	while (isdigitByte(*u) || *u == '.')
		++u;
	if (*u == ';')
		++u;
	while (*u == ' ')
		++u;
	if (memEqualCI(u, "url=", 4)) {
		char qc;
		u += 4;
		qc = *u;
		if (qc == '"' || qc == '\'')
			++u;
		else
			qc = 0;
		strmove(ref, u);
		u = ref + strlen(ref);
		if (u > ref && u[-1] == qc)
			u[-1] = 0;
		debugPrint(3, "delay %d %s", delay, ref);
/* avoid the obvious infinite loop */
		if (sameURL(ref, cf->fileName)) {
			*delay_p = 0;
			return false;
		}
		*delay_p = delay;
		return true;
	}
	i_printf(MSG_GarbledRefresh, ref);
	*delay_p = 0;
	return false;
}				/* parseRefresh */

bool shortRefreshDelay(const char *r, int d)
{
/* the value 10 seconds is somewhat arbitrary */
	if (d < 10)
		return true;
	i_printf(MSG_RedirectDelayed, r, d);
	return false;
}				/* shortRefreshDelay */

// encode the url, if it was supplied by the user.
// Otherwise just make a copy.
// Either way there is room for one more char at the end.
static void urlSanitize(struct i_get *g, const char *post)
{
	const char *portloc;
	const char *url = g->url;

	if (g->uriEncoded && !looksPercented(url, post)) {
		debugPrint(2, "Warning, url %s doesn't look encoded", url);
		g->uriEncoded = false;
	}

	if (!g->uriEncoded) {
		g->urlcopy = percentURL(url, post);
		g->urlcopy_l = strlen(g->urlcopy);
	} else {
		if (post)
			g->urlcopy_l = post - url;
		else
			g->urlcopy_l = strlen(url);
		g->urlcopy = allocMem(g->urlcopy_l + 2);
		strncpy(g->urlcopy, url, g->urlcopy_l);
		g->urlcopy[g->urlcopy_l] = 0;
	}

// get rid of : in http://this.that.com:/path, curl can't handle it.
	getPortLocURL(g->urlcopy, &portloc, 0);
	if (portloc && !isdigit(portloc[1])) {
		const char *s = portloc + strcspn(portloc, "/?#\1");
		strmove((char *)portloc, s);
		g->urlcopy_l = strlen(g->urlcopy);
	}
}				/* urlSanitize */

bool httpConnect(struct i_get *g)
{
	const char *url = g->url;
	char *cacheData = NULL;
	int cacheDataLen = 0;
	CURL *h;		// the curl http handle
	char *referrer = NULL;
	CURLcode curlret = CURLE_OK;
	struct curl_slist *custom_headers = NULL;
	struct curl_slist *tmp_headers = NULL;
	const struct MIMETYPE *mt;
	char creds_buf[MAXUSERPASS * 2 + 2];	/* creds abr. for credentials */
	bool still_fetching = true;
	char prot[MAXPROTLEN], host[MAXHOSTLEN];
	const char *post, *s;
	char *postb = NULL;
	int postb_l = 0;
	bool transfer_status = false;
	bool proceed_unauthenticated = false;
	int redirect_count = 0;
	bool post_request = false;
	bool head_request = false;
	uchar sxfirst = 0;
	int n;

	if (!getProtHostURL(url, prot, host)) {
// only the foreground http thread uses setError,
// the traditional /bin/ed error system.
		if (g->foreground)
			setError(MSG_DomainEmpty);
		return false;
	}
// plugins can only be ok from one thread, the interactive thread
// that calls up web pages at the user's behest.
// None of this machinery need be threadsafe.
	if (g->pg_ok && (cf->mt = mt = findMimeByURL(url, &sxfirst)) &&
	    !(mt->from_file | mt->down_url) && !(mt->outtype && g->playonly)) {
		char *f;
		urlSanitize(g, 0);
mimestream:
// don't have to fetch the data, the program can handle it.
		nzFree(g->buffer);
		g->buffer = 0;
		g->code = 200;
		f = g->urlcopy;
		if (mt->outtype) {
			runPluginCommand(mt, f, 0, 0, 0, &g->buffer,
					 &g->length);
			cf->render1 = true;
			if (sxfirst)
				cf->render2 = true;
			i_get_free(g, false);
		} else {
			runPluginCommand(mt, f, 0, 0, 0, 0, 0);
			i_get_free(g, true);
		}
		return true;
	}

/* Pull user password out of the url */
	n = getCredsURL(url, creds_buf);
	if (n == 1) {
		if (g->foreground)
			setError(MSG_UserNameLong, MAXUSERPASS);
		return false;
	}
	if (n == 2) {
		if (g->foreground)
			setError(MSG_PasswordLong, MAXUSERPASS);
		return false;
	}

	if (!curlActive) {
		eb_curl_global_init();
		cookiesFromJar();
		setupEdbrowseCache();
	}

	if (stringEqualCI(prot, "http") || stringEqualCI(prot, "https")) {
		;		/* ok for now */
	} else if (stringEqualCI(prot, "gopher")) {
		return gopherConnect(g);
	} else if (stringEqualCI(prot, "ftp") ||
		   stringEqualCI(prot, "ftps") ||
		   stringEqualCI(prot, "scp") ||
		   stringEqualCI(prot, "tftp") || stringEqualCI(prot, "sftp")) {
		return ftpConnect(g, creds_buf);
	} else {
		if (g->foreground)
			setError(MSG_WebProtBad, prot);
		else if (debugLevel >= 3) {
			i_printf(MSG_WebProtBad, prot);
			nl();
		}
		return false;
	}

	h = http_curl_init(g);
	if (!h)
		return false;

/* "Expect:" header causes some servers to lose.  Disable it. */
	tmp_headers = curl_slist_append(custom_headers, "Expect:");
	if (tmp_headers == NULL)
		i_printfExit(MSG_NoMem);
	custom_headers = tmp_headers;
	if (httpLanguage) {
		custom_headers =
		    curl_slist_append(custom_headers, httpLanguage);
		if (custom_headers == NULL)
			i_printfExit(MSG_NoMem);
	}

	post = strchr(url, '\1');
	postb = 0;
	urlSanitize(g, post);

	if (post) {
		post_request = true;
		post++;

		if (strncmp(post, "`mfd~", 5) == 0) {
			int multipart_header_len = 0;
			char *multipart_header =
			    initString(&multipart_header_len);
			char thisbound[24];
			post += 5;
			stringAndString(&multipart_header,
					&multipart_header_len,
					"Content-Type: multipart/form-data; boundary=");
			s = strchr(post, '\r');
			stringAndBytes(&multipart_header, &multipart_header_len,
				       post, s - post);
			tmp_headers =
			    curl_slist_append(custom_headers, multipart_header);
			if (tmp_headers == NULL)
				i_printfExit(MSG_NoMem);
			custom_headers = tmp_headers;
			/* curl_slist_append made a copy of multipart_header. */
			nzFree(multipart_header);
			memcpy(thisbound, post, s - post);
			thisbound[s - post] = 0;
			post = s + 2;
			unpackUploadedFile(post, thisbound, &postb, &postb_l);
		}
		curlret = curl_easy_setopt(h, CURLOPT_POSTFIELDS,
					   (postb_l ? postb : post));
		if (curlret != CURLE_OK)
			goto curl_fail;
		curlret =
		    curl_easy_setopt(h, CURLOPT_POSTFIELDSIZE,
				     postb_l ? postb_l : strlen(post));
		if (curlret != CURLE_OK)
			goto curl_fail;
	} else {
		curlret = curl_easy_setopt(h, CURLOPT_HTTPGET, 1);
		if (curlret != CURLE_OK)
			goto curl_fail;
	}

	if (sendReferrer && isURL(g->thisfile) &&
	    (memEqualCI(g->thisfile, "http:", 5)
	     || memEqualCI(g->thisfile, "https:", 6))) {
		char *p, *p2, *p3;
		referrer = cloneString(g->thisfile);
// lop off post data
		p = strchr(referrer, '\1');
		if (p)
			*p = 0;
// lop off .browse
		p = referrer + strlen(referrer);
		if (p - referrer > 7 && !memcmp(p - 7, ".browse", 7))
			p[-7] = 0;
// excise login:password
		p = strchr(referrer, ':');
		++p;
		if (*p == '/')
			++p;
		if (*p == '/')
			++p;
		p2 = strchr(p, '@');
		p3 = strchr(p, '/');
		if (p2 && (!p3 || p2 < p3))
			strmove(p, p2 + 1);
// The current protocol should be http or https, we cleared out everything else.
// But https to http is not allowed.   RFC 2616, section 15.1.3
		p = strchr(referrer, ':');
		if (strlen(prot) == 4 && p - referrer == 5) {
			nzFree(referrer);
			referrer = NULL;
		}
	}
// We keep the same referrer even after redirections, which I think is right.
// That's why it's here instead of inside the loop.
	curlret = curl_easy_setopt(h, CURLOPT_REFERER, referrer);
	if (curlret != CURLE_OK)
		goto curl_fail;
	curlret = curl_easy_setopt(h, CURLOPT_HTTPHEADER, custom_headers);
	if (curlret != CURLE_OK)
		goto curl_fail;
	curlret = setCurlURL(h, g->urlcopy);
	if (curlret != CURLE_OK)
		goto curl_fail;

	/* If we have a username and password, then tell libcurl about it.
	 * libcurl won't send it to the server unless server gave a 401 response.
	 * Libcurl selects the most secure form of auth provided by server. */

	if (stringEqual(creds_buf, ":"))
		getUserPass(g->urlcopy, creds_buf, false);
// If the URL didn't have user and password, and getUserPass failed,
// then creds_buf = ":".
	curlret = curl_easy_setopt(h, CURLOPT_USERPWD, creds_buf);
	if (curlret != CURLE_OK)
		goto curl_fail;

/* We are ready to make a transfer.  Here is where it gets complicated.
 * At the top of the loop, we perform the HTTP request.  It may fail entirely
 * (I.E., libcurl returns an indicator other than CURLE_OK).
 * We may be redirected.  Edbrowse needs finer control over the redirection
 * process than libcurl gives us.
 * Decide whether to accept the redirection, using the following criteria.
 * Does user permit redirects?  Will we exceed maximum allowable redirects?
 * We may be asked for authentication.  In that case, grab username and
 * password from the user.  If the server accepts the username and password,
 * then add it to the list of authentication records.  */

	still_fetching = true;

	if (!post_request && presentInCache(g->urlcopy)) {
		head_request = true;
		curl_easy_setopt(h, CURLOPT_NOBODY, 1l);
	}

	while (still_fetching == true) {
		char *redir = NULL;

// recheck the url after a redirect
		if (redirect_count && g->pg_ok &&
		    (cf->mt = mt = findMimeByURL(g->urlcopy, &sxfirst)) &&
		    !(mt->from_file | mt->down_url) &&
		    !(mt->outtype && g->playonly)) {
			curl_easy_cleanup(h);
			goto mimestream;
		}

perform:
		g->is_http = g->cacheable = true;
		curlret = fetch_internet(g);

/*********************************************************************
This is a one line workaround for an apparent bug in curl.
The return CURLE_WRITE_ERROR means the data fetched from the internet
could not be written to disk. And how does curl know?
Because the callback function returns a lesser number of bytes.
This is like write(), if it returns a lesser number
of bytes then it was unable to write the entire block to disk.
Ok, but I never return fewer bytes than was passed to me.
I return the expected number of bytes, or -1 in the rare case
that I want to abort the download.
So you see, curl should never return this WRITE error.
Yet it does, in version 7.58.0-2, on debian.
And only on one page we have found so far:
https://www.literotica.com/stories/new_submissions.php
The entire page is downloaded, down to the very last byte,
then the WRITE error is passed back.
Well if it happens once it will happen elsewhere.
Users will not be able to fetch pages from the internet, and not know why.
The error message, can't write to disk, is not helpful at all.
So this is a simple workaround.
*********************************************************************/

		if (curlret == CURLE_WRITE_ERROR)
			curlret = CURLE_OK;

		if (g->down_state == 6) {
// Header has indicated a plugin by content type or protocol or suffix.
			curl_easy_cleanup(h);
			goto mimestream;
		}

/*********************************************************************
If the desired file is in cache for some reason, and we issued the head request,
and it is application, or some such that triggers a download, then state = 1,
but no data is forthcoming, and the user was never asked if he wants
to download, so state is still 1.
So ask, and then look at state.
If state is nonzero, sorry, I'm not taking the file from cache,
not yet, just because it's a whole bunch of new code.
We don't generally store our downloaded files in cache anyways,
they go where they go, so this doesn't come up very often.
*********************************************************************/

		if (head_request) {
			if (g->down_state == 1) {
				setup_download(g);
/* now we have our answer */
			}

			if (g->down_state != 0) {
				curl_easy_setopt(h, CURLOPT_NOBODY, 0l);
				head_request = false;
				debugPrint(3, "switch to get for download %d",
					   g->down_state);
			}

			if (g->down_state == 2) {
				curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE,
						  &g->code);
				if (g->code == 200)
					goto perform;
				g->down_state = 0;
			}
		}

		if (g->down_state == 5) {
/* user has directed a download of this file in the background. */
			background_download(g);
			if (g->down_state == 4)
				goto perform;
		}

		if (g->down_state == 3 || g->down_state == -1) {
			i_get_free(g, true);
			curl_easy_cleanup(h);
			nzFree(referrer);
			return false;
		}

		if (g->down_state == 4) {
			if (curlret != CURLE_OK) {
				ebcurl_setError(curlret, g->urlcopy, 1,
						g->error);
			} else {
				i_printf(MSG_DownSuccess);
				printf(": %s\n", g->down_file2);
			}
// We're going to exit, so don't need to do this stuff,
// but some day this might be the end of a thread, not the end of a process.
			i_get_free(g, true);
			curl_easy_cleanup(h);
			nzFree(referrer);
			exit(0);
		}

		if (g->length >= CHUNKSIZE && showProgress == 'd')
			nl();	/* We printed dots, so terminate them with newline */

		if (g->down_state == 2) {
			close(g->down_fd);
			i_get_free(g, true);
			setError(MSG_DownSuccess);
			curl_easy_cleanup(h);
			nzFree(referrer);
			return false;
		}

		if (curlret != CURLE_OK)
			goto curl_fail;
		curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &g->code);
		if (curlret != CURLE_OK)
			goto curl_fail;

		debugPrint(3, "http code %ld", g->code);

/* refresh header is an alternate form of redirection */
		if (g->newloc && g->newloc_d >= 0) {
			if (shortRefreshDelay(g->newloc, g->newloc_d)) {
				g->code = 302;
			} else {
				nzFree(g->newloc);
				g->newloc = 0;
			}
		}

		redir = g->newloc;
		g->newloc = 0;

		if (allowRedirection &&
		    ((g->code >= 301 && g->code <= 303) ||
		     (g->code >= 307 && g->code <= 308))) {
			if (redir)
				redir = resolveURL(g->urlcopy, redir);
			still_fetching = false;
			if (redir == NULL) {
				/* Redirected, but we don't know where to go. */
				i_printf(MSG_RedirectNoURL, g->code);
				transfer_status = true;
			} else if (redirect_count >= 10) {
				i_puts(MSG_RedirectMany);
				transfer_status = true;
				nzFree(redir);
			} else {	/* redirection looks good. */
				strcpy(creds_buf, ":");	/* Flush stale data. */
				nzFree(g->urlcopy);
				g->urlcopy = redir;
				g->urlcopy_l = strlen(g->urlcopy);
				redir = NULL;

/* Convert POST request to GET request after redirection. */
/* This should only be done for 301 through 303 */
				if (g->code < 307) {
					curl_easy_setopt(h, CURLOPT_HTTPGET, 1);
					post_request = false;
				}
/* I think there is more work to do for 307 308,
 * pasting the prior post string onto the new URL. Not sure about this. */

				getUserPass(g->urlcopy, creds_buf, false);
				curlret =
				    curl_easy_setopt(h, CURLOPT_USERPWD,
						     creds_buf);
				if (curlret != CURLE_OK)
					goto curl_fail;

				curlret = setCurlURL(h, g->urlcopy);
				if (curlret != CURLE_OK)
					goto curl_fail;

				if (!post_request && presentInCache(g->urlcopy)) {
					head_request = true;
					curl_easy_setopt(h, CURLOPT_NOBODY, 1l);
				}
// This is unusual in that we're using the i_get structure again,
// so we need to reset some parts of it and not others.
				nzFree(g->buffer);
				g->buffer = 0;
// This 302 redirection could set content type = application/binary,
// which in turn sets state = 1, which is ignored since 302 takes precedence.
// So state might still be 1, set it back to 0.
				g->down_state = 0;
				g->code = 0;
				nzFree(g->headers);
				g->headers = 0;
				g->headers_len = 0;
				g->content[0] = 0;
				g->charset = 0;
				g->hcl = 0;
				nzFree(g->cdfn);
				g->cdfn = 0;
				g->modtime = 0;
				nzFree(g->etag);
				g->etag = 0;
				++redirect_count;
				still_fetching = true;
				debugPrint(2, "redirect %s", g->urlcopy);
			}
		}

		else if (g->code == 401 && !proceed_unauthenticated) {
			bool got_creds = false;

			/* only try realm on first try - prevents loop */
			if (stringEqual(creds_buf, ":"))
				got_creds =
				    getUserPassRealm(g->urlcopy, creds_buf,
						     g->auth_realm);
			if (!got_creds && g->foreground) {
				i_printf(MSG_AuthRequired, g->urlcopy,
					 g->auth_realm);
				nl();
				got_creds = read_credentials(creds_buf);
			}
			if (got_creds && g->foreground)
				addWebAuthorization(g->urlcopy, creds_buf,
						    false, g->auth_realm);
			if (got_creds) {
				curl_easy_setopt(h, CURLOPT_USERPWD, creds_buf);
				nzFree(g->buffer);
				g->buffer = 0;
				g->length = 0;
			} else {
/* User aborted the login process, try and at least get something. */
				proceed_unauthenticated = true;
			}
		} else {	/* not redirect, not 401 */
			if (head_request) {
				if (fetchCache
				    (g->urlcopy, g->etag, g->modtime,
				     &cacheData, &cacheDataLen)) {
					nzFree(g->buffer);
					g->buffer = cacheData;
					g->length = cacheDataLen;
					still_fetching = false;
					transfer_status = true;
				} else {
/* Back through the loop,
 * now doing GET rather than HEAD. */
					curl_easy_setopt(h, CURLOPT_NOBODY, 0l);
					head_request = false;
					--redirect_count;
				}
			} else {
				if (g->code == 200 && g->cacheable &&
				    (g->modtime || g->etag) &&
				    g->down_state == 0)
					storeCache(g->urlcopy, g->etag,
						   g->modtime, g->buffer,
						   g->length);
				still_fetching = false;
				transfer_status = true;
			}
		}
	}

curl_fail:
	if (custom_headers)
		curl_slist_free_all(custom_headers);
	curl_easy_cleanup(h);
	nzFree(postb);

	if (curlret != CURLE_OK) {
		ebcurl_setError(curlret, g->urlcopy, (g->foreground ? 0 : 1),
				g->error);
		nzFree(referrer);
		i_get_free(g, true);
		return false;
	}

	if (!transfer_status) {
		nzFree(referrer);
		i_get_free(g, true);
		return false;
	}

	if ((g->code != 200 && g->code != 201 &&
	     (g->foreground || debugLevel >= 2)) ||
	    (g->code == 201 && debugLevel >= 3))
		i_printf(MSG_HTTPError,
			 g->code, message_for_response_code(g->code));

// with lopping off post data, or encoding the url,
// it's easier to just assume the name has changed,
// even if there is no redirection.
	g->cfn = g->urlcopy;
	g->urlcopy = 0;

/* see if http header has set the filename */
	if (g->cdfn) {
		nzFree(g->cfn);
		g->cfn = g->cdfn;
		g->cdfn = NULL;
	}

	if (g->headers_p) {
		*g->headers_p = g->headers;
// The string is your responsibility now.
		g->headers = 0;
	}

	i_get_free(g, false);
	g->referrer = referrer;
	return transfer_status;
}				/* httpConnect */

// copy text over to the buffer but change < to &lt; etc,
// since this data will be browsed as if it were html.
static void prepHtmlString(struct i_get *g, const char *q)
{
	char c;
	if (!strpbrk(q, "<>&")) {	// no bad characters
		stringAndString(&g->buffer, &g->length, q);
		return;
	}
	for (; (c = *q); ++q) {
		char *meta = 0;
		if (c == '<')
			meta = "&lt;";
		if (c == '>')
			meta = "&gt;";
		if (c == '&')
			meta = "&amp;";
		if (meta)
			stringAndString(&g->buffer, &g->length, meta);
		else
			stringAndChar(&g->buffer, &g->length, c);
	}
}

/* Format a line from an ftp directory. */
static void ftp_ls_line(struct i_get *g, char *line)
{
	int l = strlen(line);
	int j;
	if (l && line[l - 1] == '\r')
		line[--l] = 0;

/* blank line becomes paragraph break */
	if (!l || (memEqualCI(line, "total ", 6) && stringIsNum(line + 6))) {
		stringAndString(&g->buffer, &g->length, "<P>\n");
		return;
	}
	stringAndString(&g->buffer, &g->length, "<br>");

	for (j = 0; line[j]; ++j)
		if (!strchr("-rwxdls", line[j]))
			break;

	if (j == 10 && line[j] == ' ') {	/* long list */
		int fsize, nlinks;
		char user[42], group[42];
		char month[8];
		int day;
		char *q, *t;
		sscanf(line + j, " %d %40s %40s %d %3s %d",
		       &nlinks, user, group, &fsize, month + 1, &day);
		q = strchr(line, ':');
		if (q) {
			for (++q; isdigitByte(*q) || *q == ':'; ++q) ;
			while (*q == ' ')
				++q;
		} else {
/* old files won't have the time, but instead, they have the year. */
/* bad news for us; no good/easy way to glom onto this one. */
			month[0] = month[4] = ' ';
			month[5] = 0;
			q = strstr(line, month);
			if (q) {
				q += 8;
				while (*q == ' ')
					++q;
				while (isdigitByte(*q))
					++q;
				while (*q == ' ')
					++q;
			}
		}

		if (q && *q) {
			char qc = '"';
			if (strchr(q, qc))
				qc = '\'';
			stringAndString(&g->buffer, &g->length, "<A HREF=x");
			g->buffer[g->length - 1] = qc;
			t = strstr(q, " -> ");
			if (t)
				stringAndBytes(&g->buffer, &g->length, q,
					       t - q);
			else
				stringAndString(&g->buffer, &g->length, q);
			stringAndChar(&g->buffer, &g->length, qc);
			stringAndChar(&g->buffer, &g->length, '>');
			stringAndString(&g->buffer, &g->length, q);
			stringAndString(&g->buffer, &g->length, "</A>");
			if (line[0] == 'd')
				stringAndChar(&g->buffer, &g->length, '/');
			stringAndString(&g->buffer, &g->length, ": ");
			stringAndNum(&g->buffer, &g->length, fsize);
			stringAndChar(&g->buffer, &g->length, '\n');
			return;
		}
	}

	prepHtmlString(g, line);
	stringAndChar(&g->buffer, &g->length, '\n');
}				/* ftp_ls_line */

/* ftp_listing: convert an FTP-style listing to html. */
/* Repeatedly calls ftp_ls_line to parse each line of the data. */
static void ftp_listing(struct i_get *g)
{
	char *s, *t;
	char *incomingData = g->buffer;
	int incomingLen = g->length;
	g->buffer = initString(&g->length);
	stringAndString(&g->buffer, &g->length, "<html>\n<body>\n");

	if (!incomingLen) {
		i_stringAndMessage(&g->buffer, &g->length, MSG_FTPEmptyDir);
	} else {

		s = incomingData;
		while (s < incomingData + incomingLen) {
			t = strchr(s, '\n');
			if (!t || t >= incomingData + incomingLen)
				break;	/* should never happen */
			*t = 0;
			ftp_ls_line(g, s);
			s = t + 1;
		}
	}

	stringAndString(&g->buffer, &g->length, "</body></html>\n");
	nzFree(incomingData);
}				/* ftp_listing */

/* Format a line from a gopher directory. */
static void gopher_ls_line(struct i_get *g, char *line)
{
	int port;
	char first, *text, *pathname, *host, *s, *plus;
	int l = strlen(line);
	if (l && line[l - 1] == '\r')
		line[--l] = 0;

// first character is the type of line
	first = 'i';
	if (line[0])
		first = *line++;
// . alone ends the listing
	if (first == '.')
		return;

// cut into pieces by tabs.
	pathname = host = 0;
	text = line;
	s = strchr(line, '\t');
	if (s) {
		*s++ = 0;
		pathname = s;
		s = strchr(pathname, '\t');
		if (s) {
			*s++ = 0;
			host = s;
			s = strchr(host, '\t');
			if (s) {
				*s++ = 0;
				if (*s) {
					// Gopher+ servers add an extra \t+,
					// which we need to truncate
					plus = strchr(s, '\t');
					if (plus)
						*plus = 0;
					port = atoi(s);
				}
			}
		}
	}

	while (*text == ' ')
		++text;

// gopher is very much line oriented.
	stringAndString(&g->buffer, &g->length, "<br>\n");

// i or 3 is informational, 3 being an error.
	if (first == 'i' || first == '3') {
		prepHtmlString(g, text);
		stringAndChar(&g->buffer, &g->length, '\n');
		return;
	}
// everything else becomes hyperlink
	if (host) {
		char qc = '"';
// I just assume host and path can be quoted with either " or '
		if (strchr(host, qc)	// should never happen
		    || strchr(pathname, qc))
			qc = '\'';
		stringAndString(&g->buffer, &g->length, "<a href=x");
		g->buffer[g->length - 1] = qc;

		pathname = encodePostData(pathname, "./-_$");
		if (!strncmp(pathname, "URL:", 4)) {
			stringAndString(&g->buffer, &g->length, pathname + 4);
		} else {
			stringAndString(&g->buffer, &g->length, "gopher://");
			stringAndString(&g->buffer, &g->length, host);
			if (port && port != 70) {
				stringAndChar(&g->buffer, &g->length, ':');
				stringAndNum(&g->buffer, &g->length, port);
			}
// gopher requires us to inject the  "first" directive into the path. Wow.
			stringAndChar(&g->buffer, &g->length, '/');
			stringAndChar(&g->buffer, &g->length, first);
			stringAndString(&g->buffer, &g->length, pathname);
		}
		nzFree(pathname);
		stringAndChar(&g->buffer, &g->length, qc);
		stringAndChar(&g->buffer, &g->length, '>');
	}

	s = strchr(text, '(');
	if (s && s == text)
		s = 0;
	if (s)
		*s = 0;
	prepHtmlString(g, text);
	if (host)
		stringAndString(&g->buffer, &g->length, "</a>");
	if (s) {
		*s = '(';
		prepHtmlString(g, s);
	}
	stringAndChar(&g->buffer, &g->length, '\n');
}				/* gopher_ls_line */

/* gopher_listing: convert a gopher-style listing to html. */
/* Repeatedly calls gopher_ls_line to parse each line of the data. */
static void gopher_listing(struct i_get *g)
{
	char *s, *t;
	char *incomingData = g->buffer;
	int incomingLen = g->length;
	g->buffer = initString(&g->length);
	stringAndString(&g->buffer, &g->length, "<html>\n<body>\n");

	if (!incomingLen) {
		i_stringAndMessage(&g->buffer, &g->length, MSG_GopherEmptyDir);
	} else {

		s = incomingData;
		while (s < incomingData + incomingLen) {
			t = strchr(s, '\n');
			if (!t || t >= incomingData + incomingLen)
				break;	/* should never happen */
			*t = 0;
			gopher_ls_line(g, s);
			s = t + 1;
		}
	}

	stringAndString(&g->buffer, &g->length, "</body></html>\n");
	nzFree(incomingData);
}				/* gopher_listing */

// action: 0 traditional set, 1 print, 2 print and exit
void ebcurl_setError(CURLcode curlret, const char *url, int action,
		     const char *curl_error)
{
	char prot[MAXPROTLEN], host[MAXHOSTLEN];
	void (*fn) (int, ...);

	if (!getProtHostURL(url, prot, host)) {
/* this should never happen */
		prot[0] = host[0] = 0;
	}

	fn = (action ? i_printf : setError);

	switch (curlret) {
	case CURLE_UNSUPPORTED_PROTOCOL:
		(*fn) (MSG_WebProtBad, prot);
		break;

	case CURLE_URL_MALFORMAT:
		(*fn) (MSG_BadURL, url);
		break;

	case CURLE_COULDNT_RESOLVE_HOST:
		(*fn) (MSG_IdentifyHost, host);
		break;

	case CURLE_REMOTE_ACCESS_DENIED:
		(*fn) (MSG_RemoteAccessDenied);
		break;

	case CURLE_TOO_MANY_REDIRECTS:
		(*fn) (MSG_RedirectMany);
		break;

	case CURLE_OPERATION_TIMEDOUT:
		(*fn) (MSG_Timeout);
		break;

	case CURLE_PEER_FAILED_VERIFICATION:
	case CURLE_SSL_CACERT:
		(*fn) (MSG_NoCertify, host);
		break;

	case CURLE_GOT_NOTHING:
	case CURLE_RECV_ERROR:
		(*fn) (MSG_WebRead);
		break;

	case CURLE_SEND_ERROR:
		(*fn) (MSG_CurlSendData);
		break;

	case CURLE_COULDNT_CONNECT:
		(*fn) (MSG_WebConnect, host);
		break;

	case CURLE_FTP_CANT_GET_HOST:
		(*fn) (MSG_FTPConnect);
		break;

	case CURLE_ABORTED_BY_CALLBACK:
#if 0
// this is printed by the callback function
		(*fn) (MSG_Interrupted);
#endif
		break;

/* These all look like session initiation failures. */
	case CURLE_FTP_WEIRD_SERVER_REPLY:
	case CURLE_FTP_WEIRD_PASS_REPLY:
	case CURLE_FTP_WEIRD_PASV_REPLY:
	case CURLE_FTP_WEIRD_227_FORMAT:
	case CURLE_FTP_COULDNT_SET_ASCII:
	case CURLE_FTP_COULDNT_SET_BINARY:
	case CURLE_FTP_PORT_FAILED:
		(*fn) (MSG_FTPSession);
		break;

	case CURLE_FTP_USER_PASSWORD_INCORRECT:
		(*fn) (MSG_LogPass);
		break;

	case CURLE_FTP_COULDNT_RETR_FILE:
		(*fn) (MSG_FTPTransfer);
		break;

	case CURLE_SSL_CONNECT_ERROR:
		(*fn) (MSG_SSLConnectError, curl_error);
		break;

	case CURLE_LOGIN_DENIED:
		(*fn) (MSG_LogPass);
		break;

	default:
		(*fn) (MSG_CurlCatchAll, curl_easy_strerror(curlret));
		break;
	}

	if (action)
		nl();
	if (action == 2)
		exit(2);
}				/* ebcurl_setError */

/* Like httpConnect, but for ftp */
static bool ftpConnect(struct i_get *g, char *creds_buf)
{
	CURL *h;		// the curl handle for ftp
	int protLength;		/* length of "ftp://" */
	bool transfer_success = false;
	bool has_slash, is_scp;
	CURLcode curlret = CURLE_OK;
	const char *url = g->url;

	protLength = strchr(url, ':') - url + 3;
/* scp is somewhat unique among the protocols handled here */
	is_scp = memEqualCI(url, "scp", 3);

	if (stringEqual(creds_buf, ":") && memEqualCI(url, "ftp", 3))
		strcpy(creds_buf, "anonymous:ftp@example.com");

	g->down_state = 0;
	g->down_file = g->down_file2 = NULL;
	g->down_length = 0;
	h = http_curl_init(g);
	if (!h)
		goto ftp_transfer_fail;
	curlret = curl_easy_setopt(h, CURLOPT_USERPWD, creds_buf);
	if (curlret != CURLE_OK)
		goto ftp_transfer_fail;

	urlSanitize(g, 0);

/* libcurl appends an implicit slash to URLs like "ftp://foo.com".
* Be explicit, so that edbrowse knows that we have a directory. */
	if (!strchr(g->urlcopy + protLength, '/'))
		strcpy(g->urlcopy + g->urlcopy_l, "/");

	curlret = setCurlURL(h, g->urlcopy);
	if (curlret != CURLE_OK)
		goto ftp_transfer_fail;

	has_slash = g->urlcopy[g->urlcopy_l] == '/';
/* don't download a directory listing, we want to see that */
/* Fetching a directory will fail in the special case of scp. */
	g->down_state = (has_slash ? 0 : 1);
	g->down_msg = MSG_FTPDownload;
	if (is_scp)
		g->down_msg = MSG_SCPDownload;

perform:
	curlret = fetch_internet(g);

	if (g->down_state == 5) {
/* user has directed a download of this file in the background. */
		background_download(g);
		if (g->down_state == 4)
			goto perform;
	}

	if (g->down_state == 3 || g->down_state == -1) {
		i_get_free(g, true);
		curl_easy_cleanup(h);
		return false;
	}

	if (g->down_state == 4) {
		if (curlret != CURLE_OK)
			ebcurl_setError(curlret, g->urlcopy, 2, g->error);
		i_printf(MSG_DownSuccess);
		printf(": %s\n", g->down_file2);
		exit(0);
	}

	if (g->length >= CHUNKSIZE && showProgress == 'd')
		nl();		/* We printed dots, so terminate them with newline */

	if (g->down_state == 2) {
		close(g->down_fd);
		setError(MSG_DownSuccess);
		i_get_free(g, true);
		curl_easy_cleanup(h);
		return false;
	}

/* Should we run this code on any error condition? */
/* The SSH error pops up under sftp. */
	if (curlret == CURLE_FTP_COULDNT_RETR_FILE ||
	    curlret == CURLE_REMOTE_FILE_NOT_FOUND || curlret == CURLE_SSH) {
		if (has_slash | is_scp)
			transfer_success = false;
		else {		/* try appending a slash. */
			strcpy(g->urlcopy + g->urlcopy_l, "/");
			g->down_state = 0;
			cnzFree(g->down_file);
			g->down_file = 0;
			curlret = setCurlURL(h, g->urlcopy);
			if (curlret != CURLE_OK)
				goto ftp_transfer_fail;

			curlret = fetch_internet(g);
			if (curlret != CURLE_OK)
				transfer_success = false;
			else {
				ftp_listing(g);
				transfer_success = true;
			}
		}
	} else if (curlret == CURLE_OK) {
		if (has_slash)
			ftp_listing(g);
		transfer_success = true;
	} else
		transfer_success = false;

ftp_transfer_fail:
	if (h)
		curl_easy_cleanup(h);
	if (transfer_success == false) {
		if (curlret != CURLE_OK)
			ebcurl_setError(curlret, g->urlcopy,
					(g->foreground ? 0 : 1), g->error);
		i_get_free(g, true);
	}
	if (transfer_success == true && !stringEqual(url, g->urlcopy))
		g->cfn = g->urlcopy;
	else
		nzFree(g->urlcopy);
	g->urlcopy = 0;

	i_get_free(g, false);

	return transfer_success;
}				/* ftpConnect */

/* Like httpConnect, but for gopher */
static bool gopherConnect(struct i_get *g)
{
	CURL *h;		// the curl handle for gopher
	int protLength;		/* length of "gopher://" */
	bool transfer_success = false;
	bool has_slash;
	char first = 0;
	char *s;
	CURLcode curlret = CURLE_OK;
	const char *url = g->url;

	protLength = strchr(url, ':') - url + 3;
	h = http_curl_init(g);
	if (!h)
		goto gopher_transfer_fail;
	urlSanitize(g, 0);

/* libcurl appends an implicit slash to URLs like "gopher://foo.com".
* Be explicit, so that edbrowse knows if we have a directory. */
	if (!strchr(g->urlcopy + protLength, '/'))
		strcpy(g->urlcopy + g->urlcopy_l, "/");
	curlret = setCurlURL(h, g->urlcopy);
	if (curlret != CURLE_OK)
		goto gopher_transfer_fail;

	has_slash = g->urlcopy[strlen(g->urlcopy) - 1] == '/';
/* don't download a directory listing, we want to see that */
	g->down_state = (has_slash ? 0 : 1);
	g->down_msg = MSG_GopherDownload;
// That's the default, let the leading character override
	s = strchr(g->urlcopy + protLength, '/');
	if (s && (first = s[1])) {
// almost every file type downloads.
		g->down_state = 1;
// 0 is tricky because "05" and "09" can mean binary
// in doubt, treat as integer and skip leading 0s
		while (first == '0' && isdigit(s[2])) {
			s++;
			first = s[1];
		}
		if (strchr("017h", first))
			g->down_state = 0;
		if (first == '1' || first == '7')
			has_slash = true;
	}

perform:
	curlret = fetch_internet(g);

	if (g->down_state == 5) {
/* user has directed a download of this file in the background. */
		background_download(g);
		if (g->down_state == 4)
			goto perform;
	}

	if (g->down_state == 3 || g->down_state == -1) {
		i_get_free(g, true);
		curl_easy_cleanup(h);
		return false;
	}

	if (g->down_state == 4) {
		if (curlret != CURLE_OK)
			ebcurl_setError(curlret, g->urlcopy, 2, g->error);
		i_printf(MSG_DownSuccess);
		printf(": %s\n", g->down_file2);
		exit(0);
	}

	if (g->length >= CHUNKSIZE && showProgress == 'd')
		nl();		/* We printed dots, so terminate them with newline */

	if (g->down_state == 2) {
		close(g->down_fd);
		setError(MSG_DownSuccess);
		i_get_free(g, true);
		curl_easy_cleanup(h);
		return false;
	}

	if (curlret == CURLE_OK) {
		if (has_slash)
			gopher_listing(g);
		transfer_success = true;
	} else
		transfer_success = false;

gopher_transfer_fail:
	if (h)
		curl_easy_cleanup(h);
	if (!transfer_success) {
		if (curlret != CURLE_OK)
			ebcurl_setError(curlret, g->urlcopy,
					(g->foreground ? 0 : 1), g->error);
		i_get_free(g, true);
		return false;
	}

	if (!stringEqual(url, g->urlcopy))
		g->cfn = g->urlcopy;
	g->urlcopy = 0;

	if (first == '0') {
// it's a text file, neeed to undos.
// The curl callback function always makes sure there is an extra byte at the end.
		g->buffer[g->length] = 0;
		int i, j;
		for (i = j = 0; i < g->length; ++i) {
			if (g->buffer[i] == '\r' && g->buffer[i + 1] == '\n')
				continue;
			g->buffer[j++] = g->buffer[i];
		}
		g->buffer[j] = 0;
		g->length = j;
	}

	return true;
}				/* gopherConnect */

/* If the user has asked for locale-specific responses, then build an
 * appropriate Accept-Language: header. */
void setHTTPLanguage(const char *lang)
{
	int httpLanguage_l;

	nzFree(httpLanguage);
	httpLanguage = NULL;
	if (!lang)
		return;

	httpLanguage = initString(&httpLanguage_l);
	stringAndString(&httpLanguage, &httpLanguage_l, "Accept-Language: ");
	stringAndString(&httpLanguage, &httpLanguage_l, lang);
}				/* setHTTPLanguage */

/* Set the FD_CLOEXEC flag on a socket newly-created by libcurl.
 * Let's not leak libcurl's sockets to child processes created by the
 * ! (escape-to-shell) command.
 * This is a callback.  It returns 0 on success, 1 on failure, per the
 * libcurl docs.
 */
static int
my_curl_safeSocket(void *clientp, curl_socket_t socketfd, curlsocktype purpose)
{
#ifdef _MSC_VER
	return 0;
#else // !_MSC_VER for success = fcntl(socketfd, F_SETFD, FD_CLOEXEC);
	int success = fcntl(socketfd, F_SETFD, FD_CLOEXEC);
	if (success == -1)
		success = 1;
	else
		success = 0;
	return success;
#endif // _MSC_VER y/n
}

static CURL *http_curl_init(struct i_get *g)
{
	CURLcode curl_init_status = CURLE_OK;
	int curl_auth;
	CURL *h = curl_easy_init();
	if (h == NULL)
		goto libcurl_init_fail;
	g->h = h;
	curl_init_status =
	    curl_easy_setopt(h, CURLOPT_SHARE, global_share_handle);
	if (curl_init_status != CURLE_OK)
		goto libcurl_init_fail;
/* Lots of these setopt calls shouldn't fail.  They just diddle a struct. */
	curl_easy_setopt(h, CURLOPT_SOCKOPTFUNCTION, my_curl_safeSocket);
	curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, eb_curl_callback);
	curl_easy_setopt(h, CURLOPT_WRITEDATA, g);
	curl_easy_setopt(h, CURLOPT_HEADERFUNCTION, curl_header_callback);
	curl_easy_setopt(h, CURLOPT_HEADERDATA, g);
	if (debugLevel >= 4)
		curl_easy_setopt(h, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(h, CURLOPT_DEBUGFUNCTION, ebcurl_debug_handler);
	curl_easy_setopt(h, CURLOPT_DEBUGDATA, g);
	curl_easy_setopt(h, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt(h, CURLOPT_PROGRESSFUNCTION, curl_progress);
	curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT, webTimeout);
	curl_easy_setopt(h, CURLOPT_USERAGENT, currentAgent);
	curl_easy_setopt(h, CURLOPT_SSLVERSION, CURL_SSLVERSION_DEFAULT);
	curl_easy_setopt(h, CURLOPT_USERAGENT, currentAgent);
/* We're doing this manually for now.
	curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, allowRedirection);
*/
	curl_easy_setopt(h, CURLOPT_AUTOREFERER, sendReferrer);
	if (ftpActive)
		curl_easy_setopt(h, CURLOPT_FTPPORT, "-");
	else
		curl_easy_setopt(h, CURLOPT_FTPPORT, NULL);
/* See "man curl_easy_setopt.3" for info on CURLOPT_FTPPORT.  Supplying
* "-" makes libcurl select the best IP address for active ftp. */

/*
* tell libcurl to pick the strongest method from basic, digest and ntlm authentication
* don't use any auth method by default as it will prefer Negotiate to NTLM,
* and it looks like in most cases microsoft IIS says it supports both and libcurl
* doesn't fall back to NTLM when it discovers that Negotiate isn't set up on a system
*/
	curl_auth = CURLAUTH_BASIC | CURLAUTH_DIGEST | CURLAUTH_NTLM;
	if (curlAuthNegotiate)
#ifdef CURLAUTH_NEGOTIATE
		curl_auth |= CURLAUTH_NEGOTIATE;
#else
		curl_auth |= CURLAUTH_GSSNEGOTIATE;	/* libcurl < 7.38 */
#endif
	curl_easy_setopt(h, CURLOPT_HTTPAUTH, curl_auth);

/* The next few setopt calls could allocate or perform file I/O. */
	g->error[0] = '\0';
	curl_init_status = curl_easy_setopt(h, CURLOPT_ERRORBUFFER, g->error);
	if (curl_init_status != CURLE_OK)
		goto libcurl_init_fail;
	curl_init_status = curl_easy_setopt(h, CURLOPT_ENCODING, "");
	if (curl_init_status != CURLE_OK)
		goto libcurl_init_fail;
	return h;

libcurl_init_fail:
	i_printf(MSG_LibcurlNoInit);
	if (h)
		curl_easy_cleanup(h);
	return 0;
}				/* http_curl_init */

/*
 * There's no easy way to get at the server's response message from libcurl.
 * So here are some tables and a function for translating response codes to
 * messages.
*/

static const char *response_codes_1xx[] = {
	"Continue",
	"Switching Protocols"
};

static const char *response_codes_2xx[] = {
	"OK",
	"Created" "Accepted",
	"Non-Authoritative Information",
	"No Content",
	"Reset Content",
	"Partial Content"
};

static const char *response_codes_3xx[] = {
	"Multiple Choices",
	"Moved Permanently",
	"Found",
	"See Other",
	"Not Modified",
	"Use Proxy",
	"(Unused)",
	"Temporary Redirect"
};

static const char *response_codes_4xx[] = {
	"Bad Request",
	"Unauthorized",
	"Payment Required",
	"Forbidden",
	"Not Found",
	"Method Not Allowed",
	"Not Acceptable",
	"Proxy Authentication Required",
	"Request Timeout",
	"Conflict",
	"Gone",
	"Length Required",
	"Precondition Failed",
	"Request Entity Too Large",
	"Request-URI Too Long",
	"Unsupported Media Type",
	"Requested Range Not Satisfiable",
	"Expectation Failed"
};

static const char *response_codes_5xx[] = {
	"Internal Server Error",
	"Not Implemented",
	"Bad Gateway",
	"Service Unavailable",
	"Gateway Timeout",
	"HTTP Version Not Supported"
};

static const char *unknown_http_response =
    "Unknown response when accessing webpage.";

static int max_codes[] = {
	0,
	sizeof(response_codes_1xx) / sizeof(char *),
	sizeof(response_codes_2xx) / sizeof(char *),
	sizeof(response_codes_3xx) / sizeof(char *),
	sizeof(response_codes_4xx) / sizeof(char *),
	sizeof(response_codes_5xx) / sizeof(char *)
};

static const char **responses[] = {
	NULL, response_codes_1xx, response_codes_2xx, response_codes_3xx,
	response_codes_4xx, response_codes_5xx
};

static const char *message_for_response_code(int code)
{
	const char *message = NULL;
	if (code < 100 || code > 599)
		message = unknown_http_response;
	else {
		int primary = code / 100;	/* Yields int in interval [1,6] */
		int subcode = code % 100;
		if (subcode >= max_codes[primary])
			message = unknown_http_response;
		else
			message = responses[primary][subcode];
	}
	return message;
}				/* message_for_response_code */

/*
 * Function: prompt_and_read
 * Arguments:
  ** prompt: prompt that user should see.
  ** buffer: buffer into which the data should be stored.
  ** max_length: maximum allowable length of input.
  ** error_msg: message to display if input exceeds maximum length.
  ** hide_echo: whether to disable terminal echo (sensitive input)
 * Note: prompt and error_message should be message constants from messages.h.
 * Return value: none.  buffer contains input on return. */

/* We need to read two things from the user while authenticating: a username
 * and a password.  Here, the task of prompting and reading is encapsulated
 * in a function, and we call that function twice.
 * After the call, the buffer contains the user's input, without a newline.
 * The return value is the length of the string in buffer. */
int
prompt_and_read(int prompt, char *buffer, int buffer_length, int error_message,
		bool hide_echo)
{
	bool reading = true;
	int n = 0;

	while (reading) {
		char *s;
		if (hide_echo)
			ttySetEcho(false);
		i_printf(prompt);
		fflush(stdout);
		s = fgets(buffer, buffer_length, stdin);
		if (hide_echo)
			ttySetEcho(true);
		if (!s)
			ebClose(0);
		n = strlen(buffer);
		if (n && buffer[n - 1] == '\n')
			buffer[--n] = '\0';	/* replace newline with NUL */
		if (n >= (MAXUSERPASS - 1)) {
			i_printf(error_message, MAXUSERPASS - 2);
			nl();
		} else
			reading = false;
	}
	return n;
}				/* prompt_and_read */

/*
 * Function: read_credentials
 * Arguments:
 ** buffer: buffer in which to place username and password.
 * Return value: true if credentials were read, false otherwise.

* Behavior: read a username and password from the user.  Store them in
 * the buffer, separated by a colon.
 * This function returns false in two situations.
 * 1. The program is not being run interactively.  The error message is
 * set to indicate this.
 * 2. The user aborted the login process by typing x"x".
 * Again, the error message reflects this condition.
*/

static bool read_credentials(char *buffer)
{
	int input_length = 0;
	bool got_creds = false;

	if (!isInteractive)
		setError(MSG_Authorize2);
	else {
		i_puts(MSG_WebAuthorize);
		input_length =
		    prompt_and_read(MSG_UserName, buffer, MAXUSERPASS,
				    MSG_UserNameLong, false);
		if (!stringEqual(buffer, "x")) {
			char *password_ptr = buffer + input_length + 1;
			prompt_and_read(MSG_Password, password_ptr, MAXUSERPASS,
					MSG_PasswordLong, true);
			if (!stringEqual(password_ptr, "x")) {
				got_creds = true;
				*(password_ptr - 1) = ':';	/* separate user and password with colon. */
			}
		}

		if (!got_creds)
			setError(MSG_LoginAbort);
	}

	return got_creds;
}				/* read_credentials */

/* Callback used by libcurl.
 * Gather all the http headers into one long string. */
static size_t
curl_header_callback(char *header_line, size_t size, size_t nmemb,
		     struct i_get *g)
{
	const struct MIMETYPE *mt;
	size_t bytes_in_line = size * nmemb;
	stringAndBytes(&g->headers, &g->headers_len,
		       header_line, bytes_in_line);

	scan_http_headers(g, true);
	mt = cf->mt;

// a from-the-web mime type causes a download interrupt
	if (g->pg_ok && mt && !(mt->down_url | mt->from_file) &&
	    !(mt->outtype && g->playonly)) {
		g->down_state = 6;
		return -1;
	}

	if (g->down_ok && g->down_state == 0 &&
	    !(mt && g->pg_ok && mt->down_url && !mt->from_file) &&
	    g->content[0] && !memEqualCI(g->content, "text/", 5) &&
	    !memEqualCI(g->content, "application/xhtml+xml", 21)) {
		g->down_state = 1;
		g->down_msg = MSG_Down;
		debugPrint(3, "potential download based on type %s",
			   g->content);
	}

	return bytes_in_line;
}				/* curl_header_callback */

/* Print text, discarding the unnecessary carriage return character. */
static void
prettify_network_text(const char *text, size_t size, FILE * destination)
{
	size_t i;
	for (i = 0; i < size; i++) {
		if (text[i] != '\r')
			fputc(text[i], destination);
	}
}				/* prettify_network_text */

/* Print incoming and outgoing headers.
 * Incoming headers are prefixed with curl<, and outgoing headers are
 * prefixed with curl> 
 * We may support more of the curl_infotype values soon. */

int
ebcurl_debug_handler(CURL * handle, curl_infotype info_desc, char *data,
		     size_t size, struct i_get *g)
{
	FILE *f = debugFile ? debugFile : stdout;
	if (info_desc == CURLINFO_HEADER_OUT) {
		fprintf(f, "curl>\n");
		prettify_network_text(data, size, f);
	} else if (info_desc == CURLINFO_HEADER_IN) {
		if (!g->last_curlin)
			fprintf(f, "curl<\n");
		prettify_network_text(data, size, f);
	} else;			/* Do nothing.  We don't care about this piece of data. */

	if (info_desc == CURLINFO_HEADER_IN)
		g->last_curlin = true;
	else if (info_desc)
		g->last_curlin = false;

	return 0;
}				/* ebcurl_debug_handler */

// At this point, down_state = 1
// Only runs from the foreground thread, does not have to be threadsafe.
static void setup_download(struct i_get *g)
{
	const char *filepart;
	const char *answer;

/* if not run from a terminal then just return. */
	if (!isInteractive) {
		g->down_state = 0;
		return;
	}

	filepart = getFileURL(g->urlcopy, true);
top:
	answer = getFileName(g->down_msg, filepart, false, true);
/* space for a filename means read into memory */
	if (stringEqual(answer, " ")) {
		g->down_state = 0;	/* in memory download */
		return;
	}

	if (stringEqual(answer, "x") || stringEqual(answer, "X")) {
		g->down_state = -1;
		setError(MSG_DownAbort);
		return;
	}

	if (!envFileDown(answer, &answer)) {
		showError();
		goto top;
	}

	g->down_fd = creat(answer, 0666);
	if (g->down_fd < 0) {
		i_printf(MSG_NoCreate2, answer);
		nl();
		goto top;
	}
// we will free down_file, but not down_file2
	g->down_file = g->down_file2 = cloneString(answer);
	if (downDir) {
		int l = strlen(downDir);
		if (!strncmp(g->down_file2, downDir, l)) {
			g->down_file2 += l;
			if (g->down_file2[0] == '/')
				++g->down_file2;
		}
	}
	g->down_state = (down_bg ? 5 : 2);
}				/* setup_download */

#ifdef _MSC_VER			// need fork()
/* At this point, down_state = 5 */
static void background_download(struct i_get *g)
{
	g->down_state = -1;
/* perhaps a better error message here */
	setError(MSG_DownAbort);
	return;
}

int bg_jobs(bool iponly)
{
	return 0;
}

#else // !_MSC_VER

/* At this point, down_state = 5 */
static void background_download(struct i_get *g)
{
	int down_pid = fork();
	if (down_pid < 0) {	/* should never happen */
		g->down_state = -1;
/* perhaps a better error message here */
		setError(MSG_DownAbort);
		return;
	}

	if (down_pid) {		/* parent */
		struct BG_JOB *job;
		close(g->down_fd);
/* the error message here isn't really an error, but a progress message */
		setError(MSG_DownProgress);
		g->down_state = 3;

/* push job onto the list for tracking and display */
		job = allocMem(sizeof(struct BG_JOB) + strlen(g->down_file));
		job->pid = down_pid;
		job->state = 4;
		strcpy(job->file, g->down_file);
		job->file2 = g->down_file2 - g->down_file;
// round file size up to the nearest chunk.
// This will come out 0 only if the true size is 0.
		job->fsize = ((g->hcl + (CHUNKSIZE - 1)) / CHUNKSIZE);
		addToListBack(&down_jobs, job);

		return;
	}

/* ignore interrupt, not sure about quit and hangup */
	signal(SIGINT, SIG_IGN);
	g->down_state = 4;
}				/* background_download */

/* show background jobs and return the number of jobs pending */
/* if iponly is true then just show in progress */
int bg_jobs(bool iponly)
{
	bool present = false, part;
	int numback = 0;
	struct BG_JOB *j;
	int pid, status;

/* gather exit status of background jobs */
	foreach(j, down_jobs) {
		if (j->state != 4)
			continue;
		pid = waitpid(j->pid, &status, WNOHANG);
		if (!pid)
			continue;
		j->state = -1;
		if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
			j->state = 0;
	}

/* three passes */
/* in progress */
	part = false;
	foreach(j, down_jobs) {
		if (j->state != 4)
			continue;
		++numback;
		if (!part) {
			i_printf(MSG_InProgress);
			puts(" {");
			part = present = true;
		}
		printf("%s", j->file + j->file2);
		if (j->fsize)
			printf(" %d/%zu",
			       (fileSizeByName(j->file) / CHUNKSIZE), j->fsize);
		nl();
	}
	if (part)
		puts("}");

	if (iponly)
		return numback;

/* complete */
	part = false;
	foreach(j, down_jobs) {
		if (j->state != 0)
			continue;
		if (!part) {
			i_printf(MSG_Complete);
			puts(" {");
			part = present = true;
		}
		puts(j->file + j->file2);
	}
	if (part)
		puts("}");

/* failed */
	part = false;
	foreach(j, down_jobs) {
		if (j->state != -1)
			continue;
		if (!part) {
			i_printf(MSG_Failed);
			puts(" {");
			part = present = true;
		}
		puts(j->file + j->file2);
	}
	if (part)
		puts("}");

	if (!present)
		i_puts(MSG_Empty);

	return numback;
}				/* bg_jobs */
#endif // #ifndef _MSC_VER // need fork()

static char **novs_hosts;
size_t novs_hosts_avail;
size_t novs_hosts_max;

void addNovsHost(char *host)
{
	if (novs_hosts_max == 0) {
		novs_hosts_max = 32;
		novs_hosts = allocZeroMem(novs_hosts_max * sizeof(char *));
	} else if (novs_hosts_avail >= novs_hosts_max) {
		novs_hosts_max *= 2;
		novs_hosts =
		    reallocMem(novs_hosts, novs_hosts_max * sizeof(char *));
	}
	novs_hosts[novs_hosts_avail++] = host;
}				/* addNovsHost */

/* Return true if the cert for this host should be verified. */
static bool mustVerifyHost(const char *host)
{
	size_t this_host_len = strlen(host);
	size_t i;

	if (!verifyCertificates)
		return false;

	for (i = 0; i < novs_hosts_avail; i++) {
		size_t l1 = strlen(novs_hosts[i]);
		size_t l2 = this_host_len;
		if (l1 > l2)
			continue;
		l2 -= l1;
		if (!stringEqualCI(novs_hosts[i], host + l2))
			continue;
		if (l2 && host[l2 - 1] != '.')
			continue;
		return false;
	}
	return true;
}				/* mustVerifyHost */

void deleteNovsHosts(void)
{
	nzFree(novs_hosts);
	novs_hosts = NULL;
	novs_hosts_avail = novs_hosts_max = 0;
}				/* deleteNovsHosts */

CURLcode setCurlURL(CURL * h, const char *url)
{
	char host[MAXHOSTLEN];
	unsigned long verify;
	const char *proxy = findProxyForURL(url);
	if (!proxy)
		proxy = "";
	else
		debugPrint(3, "proxy %s", proxy);
	if (!getProtHostURL(url, NULL, host))
		return CURLE_URL_MALFORMAT;
	verify = mustVerifyHost(host);
	curl_easy_setopt(h, CURLOPT_PROXY, proxy);
	curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, verify);
	curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, (verify ? 2 : 0));
	return curl_easy_setopt(h, CURLOPT_URL, url);
}				/* setCurlURL */

/*********************************************************************
Given a protocol and a domain, find the proxy server
to mediate your request.
This is the C version, using entries in .ebrc.
There is a javascript version of the same name, that we will support later.
This is a beginning, and it can be used even when javascript is disabled.
A return of null means DIRECT, and this is the default
if we don't match any of the proxy entries.
*********************************************************************/

static const char *findProxyInternal(const char *prot, const char *domain)
{
	struct PXENT *px = proxyEntries;
	int i;

/* first match wins */
	for (i = 0; i < maxproxy; ++i, ++px) {

		if (px->prot) {
			char *s = px->prot;
			char *t;
			int rc;
			while (*s) {
				t = strchr(s, '|');
				if (t)
					*t = 0;
				rc = stringEqualCI(s, prot);
				if (t)
					*t = '|';
				if (rc)
					goto domain;
				if (!t)
					break;
				s = t + 1;
			}
			continue;
		}

domain:
		if (px->domain) {
			int l1 = strlen(px->domain);
			int l2 = strlen(domain);
			if (l1 > l2)
				continue;
			l2 -= l1;
			if (!stringEqualCI(px->domain, domain + l2))
				continue;
			if (l2 && domain[l2 - 1] != '.')
				continue;
		}

		return px->proxy;
	}

	return 0;
}				/* findProxyInternal */

static const char *findProxyForURL(const char *url)
{
	char prot[MAXPROTLEN], host[MAXHOSTLEN];
	if (!getProtHostURL(url, prot, host)) {
/* this should never happen */
		return 0;
	}
	return findProxyInternal(prot, host);
}				/* findProxyForURL */

/* expand a frame inline.
 * Pass a range of lines; you can expand all the frames in one go.
 * Return false if there is a problem fetching a web page,
 * or if none of the lines are frames. */
static int frameContractLine(int lineNumber);
static const char *stringInBufLine(const char *s, const char *t);
bool frameExpand(bool expand, int ln1, int ln2)
{
	int ln;			/* line number */
	int problem = 0, p;
	bool something_worked = false;

	for (ln = ln1; ln <= ln2; ++ln) {
		if (expand)
			p = frameExpandLine(ln, NULL);
		else
			p = frameContractLine(ln);
		if (p > problem)
			problem = p;
		if (p == 0)
			something_worked = true;
	}

	if (something_worked && problem < 3)
		problem = 0;
	if (problem == 1)
		setError(expand ? MSG_NoFrame1 : MSG_NoFrame2);
	if (problem == 2)
		setError(MSG_FrameNoURL);
	return (problem == 0);
}				/* frameExpand */

/* Problems: 0, frame expanded successfully.
 1 line is not a frame.
 2 frame doesn't have a valid url.
 3 Problem fetching the rul or rendering the page.  */
int frameExpandLine(int ln, jsobjtype fo)
{
	pst line;
	int tagno, start;
	const char *s;
	struct htmlTag *t;
	struct ebFrame *save_cf, *new_cf, *last_f;
	uchar save_local;
	struct htmlTag *cdt;	// contentDocument tag

	if (fo) {
		t = tagFromJavaVar(fo);
		if (!t)
			return 1;
	} else {
		line = fetchLine(ln, -1);
		s = stringInBufLine((char *)line, "Frame ");
		if (!s)
			return 1;
		if ((s = strchr(s, InternalCodeChar)) == NULL)
			return 2;
		tagno = strtol(s + 1, (char **)&s, 10);
		if (tagno < 0 || tagno >= cw->numTags || *s != '{')
			return 2;
		t = tagList[tagno];
	}
	if (t->action != TAGACT_FRAME)
		return 1;

/* the easy case is if it's already been expanded before, we just unhide it. */
	if (t->f1) {
		if (!fo)
			t->contracted = false;
		return 0;
	}

	s = t->href;
	if (!s) {
// No source. If this is your request then return an error.
// But if we're dipping into the objects then it needs to expand
// into a separate window, a separate js space, with an empty body.
		if (!fo)
			return 2;
// After expansion we need to be able to expand it,
// because there's something there, well maybe.
		t->href = cloneString("#");
	}

	save_cf = cf = t->f0;
/* have to push a new frame before we read the web page */
	for (last_f = &(cw->f0); last_f->next; last_f = last_f->next) ;
	last_f->next = cf = allocZeroMem(sizeof(struct ebFrame));
	cf->owner = cw;
	cf->frametag = t;
	debugPrint(2, "fetch frame %s", (s ? s : "empty"));
	if (s) {
		bool rc = readFileArgv(s, (fo ? 2 : 1));
		if (!rc) {
/* serverData was never set, or was freed do to some other error. */
/* We just need to pop the frame and return. */
			fileSize = -1;	/* don't print 0 */
			nzFree(cf->fileName);
			free(cf);
			last_f->next = 0;
			cf = save_cf;
			return 3;
		}

/*********************************************************************
readFile could return success and yet serverData is null.
This happens if httpConnect did something other than fetching data,
like playing a stream. Does that happen, even in a frame?
It can, if the frame is a youtube video, which is not unusual at all.
So check for serverData null here. Once again we pop the frame.
*********************************************************************/

		if (serverData == NULL) {
			nzFree(cf->fileName);
			free(cf);
			last_f->next = 0;
			cf = save_cf;
			fileSize = -1;
			return 0;
		}
	} else {
		serverData = cloneString("<body></body>");
		serverDataLen = strlen(serverData);
	}

	new_cf = cf;
	if (changeFileName) {
		nzFree(cf->fileName);
		cf->fileName = changeFileName;
		cf->uriEncoded = true;
		changeFileName = 0;
	} else {
		cf->fileName = cloneString(s);
	}

/* don't print the size of what we just fetched */
	fileSize = -1;

/* If we got some data it has to be html.
 * I should check for that, something like htmlTest in html.c,
 * but I'm too lazy to do that right now, so I'll just assume it's good. */

	cf->hbase = cloneString(cf->fileName);
	save_local = browseLocal;
	browseLocal = !isURL(cf->fileName);
	prepareForBrowse(serverData, serverDataLen);
	if (javaOK(cf->fileName))
		createJavaContext();
	nzFree(newlocation);	/* should already be 0 */
	newlocation = 0;

	start = cw->numTags;
/* call the tidy parser to build the html nodes */
	html2nodes(serverData, true);
	nzFree(serverData);	/* don't need it any more */
	serverData = 0;
	htmlGenerated = false;
// in the edbrowse world, the only child of the frame tag
// is the contentDocument tag.
	cdt = t->firstchild;
// the placeholder document node will soon be orphaned.
	delete_property(cdt->jv, "parentNode");
	htmlNodesIntoTree(start, cdt);
	cdt->step = 0;
	prerender(0);

/*********************************************************************
At this point cdt->step is 1; the html tree is built, but not decorated.
Well I put the object on cdt manually. Besides, we don't want to set up
the fake cdt object and the getter that auto-expands the frame,
we did that before and now it's being expanded. So bump step up to 2.
*********************************************************************/
	cdt->step = 2;

	if (cf->docobj) {
		jsobjtype topobj;
		decorate(0);
		set_basehref(cf->hbase);
// parent points to the containing frame.
		set_property_object(cf->winobj, "parent", save_cf->winobj);
// And top points to the top.
		cf = save_cf;
		topobj = get_property_object(cf->winobj, "top");
		cf = new_cf;
		set_property_object(cf->winobj, "top", topobj);
		set_property_object(cf->winobj, "frameElement", t->jv);
		run_function_bool(cf->winobj, "eb$qs$start");
		runScriptsPending();
		runOnload();
		runScriptsPending();
		run_event_bool(cf->docobj, "document", "onDOMContentLoaded");
		rebuildSelectors();
		set_property_string(cf->docobj, "readyState", "complete");
	}

	if (cf->fileName) {
		int j = strlen(cf->fileName);
		cf->fileName = reallocMem(cf->fileName, j + 8);
		strcat(cf->fileName, ".browse");
	}

	t->f1 = cf;
	cf = save_cf;
	browseLocal = save_local;
	if (fo)
		t->contracted = true;
	if (new_cf->docobj) {
		jsobjtype cdo;	// contentDocument object
		jsobjtype cwo;	// contentWindow object
		jsobjtype cna;	// childNodes array
		cdo = new_cf->docobj;
		disconnectTagObject(cdt);
		connectTagObject(cdt, cdo);
// Should I switch this tag into the new frame? I don't really know.
		cdt->f0 = new_cf;
		set_property_object(t->jv, "content$Document", cdo);
		cna = get_property_object(t->jv, "childNodes");
		set_array_element_object(cna, 0, cdo);
// Should we do this? For consistency I guess yes.
		set_property_object(cdo, "parentNode", t->jv);
		cwo = new_cf->winobj;
		set_property_object(t->jv, "content$Window", cwo);
// run the frame onload function if it is there.
// I assume it should run in the higher context.
		run_event_bool(t->jv, t->info->name, "onload");
	}

	return 0;
}				/* frameExpandLine */

static int frameContractLine(int ln)
{
	struct htmlTag *t = line2frame(ln);
	if (!t)
		return 1;
	t->contracted = true;
	return 0;
}				/* frameContractLine */

struct htmlTag *line2frame(int ln)
{
	const char *line;
	int n, opentag = 0, ln1 = ln;
	const char *s;

	for (; ln; --ln) {
		line = (char *)fetchLine(ln, -1);
		if (!opentag && ln < ln1
		    && (s = stringInBufLine(line, "*--`\n"))) {
			for (--s; s > line && *s != InternalCodeChar; --s) ;
			if (*s == InternalCodeChar)
				opentag = atoi(s + 1);
			continue;
		}
		s = stringInBufLine(line, "*`--\n");
		if (!s)
			continue;
		for (--s; s > line && *s != InternalCodeChar; --s) ;
		if (*s != InternalCodeChar)
			continue;
		n = atoi(s + 1);
		if (!opentag)
			return tagList[n];
		if (n == opentag)
			opentag = 0;
	}

	return 0;
}				/* line2frame */

/* a text line in the buffer isn't a string; you can't use strstr */
static const char *stringInBufLine(const char *s, const char *t)
{
	int n = strlen(t);
	for (; *s != '\n'; ++s) {
		if (!strncmp(s, t, n))
			return s;
	}
	return 0;
}				/* stringInBufLine */

bool reexpandFrame(void)
{
	int j, start;
	struct htmlTag *frametag;
	struct htmlTag *cdt;	// contentDocument tag
	uchar save_local;
	bool rc;
	jsobjtype save_top, save_parent, save_fe;

	cf = newloc_f;
	frametag = cf->frametag;
	cdt = frametag->firstchild;
	save_top = get_property_object(cf->winobj, "top");
	save_parent = get_property_object(cf->winobj, "parent");
	save_fe = get_property_object(cf->winobj, "frameElement");

// Cut away our tree nodes from the previous document, which are now inaccessible.
	underKill(cdt);

// the previous document node will soon be orphaned.
	delete_property(cdt->jv, "parentNode");

	delTimers(cf);
	freeJavaContext(cf);
	nzFree(cf->dw);
	cf->dw = 0;
	nzFree(cf->hbase);
	cf->hbase = 0;
	nzFree(cf->fileName);
	cf->fileName = newlocation;
	newlocation = 0;
	cf->uriEncoded = false;
	nzFree(cf->firstURL);
	cf->firstURL = 0;
	rc = readFileArgv(cf->fileName, 2);
	if (!rc) {
/* serverData was never set, or was freed do to some other error. */
		fileSize = -1;	/* don't print 0 */
		return false;
	}

	if (serverData == NULL) {
/* frame replaced itself with a playable stream, what to do? */
		fileSize = -1;
		return true;
	}

	if (changeFileName) {
		nzFree(cf->fileName);
		cf->fileName = changeFileName;
		cf->uriEncoded = true;
		changeFileName = 0;
	}

/* don't print the size of what we just fetched */
	fileSize = -1;

	cf->hbase = cloneString(cf->fileName);
	save_local = browseLocal;
	browseLocal = !isURL(cf->fileName);
	prepareForBrowse(serverData, serverDataLen);
	if (javaOK(cf->fileName))
		createJavaContext();

	start = cw->numTags;
/* call the tidy parser to build the html nodes */
	html2nodes(serverData, true);
	nzFree(serverData);	/* don't need it any more */
	serverData = 0;
	htmlGenerated = false;
	htmlNodesIntoTree(start, cdt);
	cdt->step = 0;
	prerender(0);
	cdt->step = 2;
	if (cf->docobj) {
		decorate(0);
		set_basehref(cf->hbase);
		set_property_object(cf->winobj, "top", save_top);
		set_property_object(cf->winobj, "parent", save_parent);
		set_property_object(cf->winobj, "frameElement", save_fe);
		run_function_bool(cf->winobj, "eb$qs$start");
		runScriptsPending();
		runOnload();
		runScriptsPending();
		run_event_bool(cf->docobj, "document", "onDOMContentLoaded");
		rebuildSelectors();
		set_property_string(cf->docobj, "readyState", "complete");
	}

	j = strlen(cf->fileName);
	cf->fileName = reallocMem(cf->fileName, j + 8);
	strcat(cf->fileName, ".browse");
	browseLocal = save_local;

	if (cf->docobj) {
		struct ebFrame *save_cf;
		jsobjtype cdo;	// contentDocument object
		jsobjtype cwo;	// contentWindow object
		jsobjtype cna;	// childNodes array
		cdo = cf->docobj;
		cwo = cf->winobj;
		disconnectTagObject(cdt);
		connectTagObject(cdt, cdo);
// Should I switch this tag into the new frame? I don't really know.
		cdt->f0 = cf;
// have to point contentDocument to the new document object,
// but that requires a change of context.
		save_cf = cf;
		cf = frametag->f0;
		set_property_object(frametag->jv, "content$Document", cdo);
		cna = get_property_object(frametag->jv, "childNodes");
		set_array_element_object(cna, 0, cdo);
// Should we do this? For consistency I guess yes.
		set_property_object(cdo, "parentNode", frametag->jv);
		set_property_object(frametag->jv, "content$Window", cwo);
		cf = save_cf;
	}

	return true;
}				/* reexpandFrame */

// Make sure a web page is not trying to read a local file.
bool frameSecurityFile(const char *thisfile)
{
	struct ebFrame *f = &cf->owner->f0;
	for (; f != cf; f = f->next) {
		if (!isURL(f->fileName))
			continue;
		setError(MSG_NoAccessSecure, thisfile);
		return false;
	}
	return true;
}
