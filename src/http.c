/* http.c
 * HTTP protocol client implementation
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"

#include <wait.h>
#include <time.h>

CURL *http_curl_handle = NULL;
char *serverData;
int serverDataLen;
static int down_fd;		/* downloading file descriptor */
static const char *down_file;	/* downloading filename */
static const char *down_file2;	/* without download directory */
static int down_pid;		/* pid of the downloading child process */
static bool down_permitted;
/* download states.
 * -1 user aborted the download
 * 0 in-memory download
 * 1 download but stop and ask user if it looks binary
* 2 disk download foreground
* 3 disk download background parent
* 4 disk download background child
* 5 disk download background prefork
 * 6 mime type says this should be a stream */
static int down_state;
bool down_bg;			/* download in background */
static int down_length;		/* how much data to disk so far */
static int down_msg;

struct BG_JOB {
	struct BG_JOB *next, *prev;
	int pid, state;
	char file[4];
};
struct listHead down_jobs = {
	&down_jobs, &down_jobs
};

static void background_download(void);
static void setup_download(void);
static char errorText[CURL_ERROR_SIZE + 1];
static char *http_headers;
static int http_headers_len;
static char *httpLanguage;
/* http content type is used in many places, and isn't arbitrarily long
 * or case sensitive, so keep our own sanitized copy. */
static char hct[60];
static char *hct2;		/* extra content info such as charset */
int hcl;			/* http content length */
extern char *newlocation;
extern int newloc_d;

static struct eb_curl_callback_data callback_data = {
	&serverData, &serverDataLen
};

/* string is allocated. Quotes are removed. No other processing is done.
 * You may need to decode %xx bytes or such. */
static char *find_http_header(const char *name)
{
	char *s, *t, *u, *v;
	int namelen = strlen(name);

	if (!http_headers)
		return NULL;

	for (s = http_headers; *s; s = v) {
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

static void scan_http_headers(bool fromCallback)
{
	char *v;

	if (!hct[0] && (v = find_http_header("content-type"))) {
		strncpy(hct, v, sizeof(hct) - 1);
		caseShift(hct, 'l');
		nzFree(v);
		debugPrint(3, "content %s", hct);
		hct2 = strchr(hct, ';');
		if (hct2)
			*hct2++ = 0;
/* The protocol, such as rtsp, could have already set the mime type. */
		if (!cw->mt)
			cw->mt = findMimeByContent(hct);
	}

	if (!hcl && (v = find_http_header("content-length"))) {
		hcl = atoi(v);
		nzFree(v);
		debugPrint(3, "content length %d", hcl);
	}

	if (fromCallback)
		return;

	if ((newlocation == NULL) && (v = find_http_header("location"))) {
		unpercentURL(v);
		newlocation = v;
		newloc_d = -1;
	}

	if (v = find_http_header("refresh")) {
		int delay;
		if (parseRefresh(v, &delay)) {
			unpercentURL(v);
			gotoLocation(v, delay, true);
/* string is passed to somewhere else, set v to null so it is not freed */
			v = NULL;
		}
		nzFree(v);
	}
}				/* scan_http_headers */

/* actually run the curl request, http or ftp or whatever */
static CURLcode fetch_internet(bool is_http)
{
	CURLcode curlret;
/* this should already be 0 */
	nzFree(newlocation);
	newlocation = NULL;
	nzFree(http_headers);
	http_headers = initString(&http_headers_len);
	hct[0] = 0;
	hct2 = NULL;
	hcl = 0;
	curlret = curl_easy_perform(http_curl_handle);
	if (is_http)
		scan_http_headers(false);
	nzFree(http_headers);
	http_headers = 0;
	return curlret;
}				/* fetch_internet */

static bool ftpConnect(const char *url, const char *user, const char *pass);
static bool read_credentials(char *buffer);
static size_t curl_header_callback(char *header_line, size_t size, size_t nmemb,
				   void *unused);
static const char *message_for_response_code(int code);

/* Callback used by libcurl. Appends data to serverData. */
size_t
eb_curl_callback(char *incoming, size_t size, size_t nitems,
		 struct eb_curl_callback_data *data)
{
	size_t num_bytes = nitems * size;
	int dots1, dots2, rc;

	if (down_state == 1) {
/* state 1, first data block, ask the user */
		setup_download();
		if (down_state == 0)
			goto showdots;
		if (down_state == -1 || down_state == 5)
			return -1;
	}

	if (down_state == 2 || down_state == 4) {	/* to disk */
		rc = write(down_fd, incoming, num_bytes);
		if (rc == num_bytes) {
			if (down_state == 4)
				return rc;
			goto showdots;
		}
		if (down_state == 2) {
			setError(MSG_NoWrite2, down_file);
			return -1;
		}
		i_printf(MSG_NoWrite2, down_file);
		printf(", ");
		i_puts(MSG_DownAbort);
		exit(1);
	}

showdots:
	dots1 = *(data->length) / CHUNKSIZE;
	if (down_state == 0)
		stringAndBytes(data->buffer, data->length, incoming, num_bytes);
	else
		*(data->length) += num_bytes;
	dots2 = *(data->length) / CHUNKSIZE;
	if (dots1 < dots2) {
		for (; dots1 < dots2; ++dots1)
			putchar('.');
		fflush(stdout);
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
	if (intFlag) {
		intFlag = false;
		ret = 1;
	}
	return ret;
}				/* curl_progress */

/* Pull a keyword: attribute out of an internet header. */
static char *extractHeaderItem(const char *head, const char *end,
			       const char *item, const char **ptr)
{
	int ilen = strlen(item);
	const char *f, *g;
	char *h = 0;
	for (f = head; f < end - ilen - 1; f++) {
		if (*f != '\n')
			continue;
		if (!memEqualCI(f + 1, item, ilen))
			continue;
		f += ilen;
		if (f[1] != ':')
			continue;
		f += 2;
		while (*f == ' ')
			++f;
		for (g = f; g < end && *g >= ' '; g++) ;
		while (g > f && g[-1] == ' ')
			--g;
		h = pullString1(f, g);
		if (ptr)
			*ptr = f;
		break;
	}
	return h;
}				/* extractHeaderItem */

/* This is a global function; it is called from cookies.c */
char *extractHeaderParam(const char *str, const char *item)
{
	int le = strlen(item), lp;
	const char *s = str;
/* ; denotes the next param */
/* Even the first param has to be preceeded by ; */
	while (s = strchr(s, ';')) {
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

/* Date format is:    Mon, 03 Jan 2000 21:29:33 GMT|[+-]nnnn */
			/* Or perhaps:     Sun Nov  6 08:49:37 1994 */
time_t parseHeaderDate(const char *date)
{
	static const char *const months[12] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	time_t t = 0;
	int zone = 0;
	int y;			/* remember the type of format */
	struct tm tm;
	memset(&tm, 0, sizeof(struct tm));

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
			if (!isdigitByte(date[0]))
				goto fail;
			if (!isdigitByte(date[1]))
				goto fail;
			if (!isdigitByte(date[2]))
				goto fail;
			if (!isdigitByte(date[3]))
				goto fail;
			tm.tm_year =
			    (date[0] - '0') * 1000 + (date[1] - '0') * 100 +
			    (date[2] - '0') * 10 + date[3] - '0' - 1900;
			date += 4;
		} else if (*date == '-') {
			/* Sunday, 06-Nov-94 08:49:37 GMT */
			date++;
			if (!isdigitByte(date[0]))
				goto fail;
			if (!isdigitByte(date[1]))
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

/* ready to crack time */
	if (!isdigitByte(date[0]))
		goto fail;
	if (!isdigitByte(date[1]))
		goto fail;
	tm.tm_hour = (date[0] - '0') * 10 + date[1] - '0';
	date += 2;
	if (*date != ':')
		goto fail;
	date++;
	if (!isdigitByte(date[0]))
		goto fail;
	if (!isdigitByte(date[1]))
		goto fail;
	tm.tm_min = (date[0] - '0') * 10 + date[1] - '0';
	date += 2;
	if (*date != ':')
		goto fail;
	date++;
	if (!isdigitByte(date[0]))
		goto fail;
	if (!isdigitByte(date[1]))
		goto fail;
	tm.tm_sec = (date[0] - '0') * 10 + date[1] - '0';
	date += 2;

	if (y) {
/* year is at the end */
		if (*date != ' ')
			goto fail;
		date++;
		if (!isdigitByte(date[0]))
			goto fail;
		if (!isdigitByte(date[1]))
			goto fail;
		if (!isdigitByte(date[2]))
			goto fail;
		if (!isdigitByte(date[3]))
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

	t = mktime(&tm);
	if (t != (time_t) - 1)
		return t + zone;

fail:
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
		if (sameURL(ref, cw->fileName)) {
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

/* Return true if we waited for the duration, false if interrupted.
 * I don't know how to do this in Windows. */
bool refreshDelay(int sec, const char *u)
{
/* the value 15 seconds is somewhat arbitrary */
	if (sec < 15)
		return true;
	i_printf(MSG_RedirectDelayed, u, sec);
	return false;
}				/* refreshDelay */

long hcode;			/* example, 404 */
static char herror[32];		/* example, file not found */
static char *urlcopy;

bool httpConnect(const char *url, bool down_ok, bool webpage)
{
	char *referrer = NULL;
	CURLcode curlret = CURLE_OK;
	struct curl_slist *custom_headers = NULL;
	struct curl_slist *tmp_headers = NULL;
	char user[MAXUSERPASS], pass[MAXUSERPASS];
	char creds_buf[MAXUSERPASS * 2 + 1];	/* creds abr. for credentials */
	int creds_len = 0;
	bool still_fetching = true;
	const char *host;
	const char *prot;
	char *cmd;
	const char *post, *s;
	char *postb = NULL;
	int postb_l = 0;
	bool transfer_status = false;
	bool proceed_unauthenticated = false;
	int redirect_count = 0;
	bool name_changed = false;

	urlcopy = NULL;
	serverData = NULL;
	serverDataLen = 0;
	strcpy(creds_buf, ":");	/* Flush stale username and password. */
	cw->mt = NULL;		/* should already be null */

/* Pull user password out of the url */
	user[0] = pass[0] = 0;
	s = getUserURL(url);
	if (s) {
		if (strlen(s) >= sizeof(user) - 2) {
			setError(MSG_UserNameLong, sizeof(user));
			return false;
		}
		strcpy(user, s);
	}
	s = getPassURL(url);
	if (s) {
		if (strlen(s) >= sizeof(pass) - 2) {
			setError(MSG_PasswordLong, sizeof(pass));
			return false;
		}
		strcpy(pass, s);
	}

	prot = getProtURL(url);
	if (!prot) {
		setError(MSG_WebProtBad, "(?)");
		return false;
	}

	if (stringEqualCI(prot, "http") || stringEqualCI(prot, "https")) {
		;		/* ok for now */
	} else if (stringEqualCI(prot, "ftp") ||
		   stringEqualCI(prot, "ftps") ||
		   stringEqualCI(prot, "scp") ||
		   stringEqualCI(prot, "tftp") || stringEqualCI(prot, "sftp")) {
		return ftpConnect(url, user, pass);
	} else if ((cw->mt = findMimeByProtocol(prot)) && pluginsOn
		   && cw->mt->stream) {
mimestream:
		cmd = pluginCommand(cw->mt, url, NULL, NULL);
		if (!cmd)
			return false;
/* Stop ignoring SIGPIPE for the duration of system(): */
		signal(SIGPIPE, SIG_DFL);
		system(cmd);
		signal(SIGPIPE, SIG_IGN);
		nzFree(cmd);
		i_puts(MSG_OK);
		return true;
	} else {
		setError(MSG_WebProtBad, prot);
		return false;
	}

/* Ok, it's http, but the suffix could force a plugin */
	if ((cw->mt = findMimeByURL(url)) && pluginsOn && cw->mt->stream)
		goto mimestream;

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
	if (post) {
		urlcopy = percentURL(url, post);
		post++;

		if (strncmp(post, "`mfd~", 5)) ;	/* No need to do anything, not multipart. */

		else {
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
		curlret = curl_easy_setopt(http_curl_handle, CURLOPT_POSTFIELDS,
					   (postb_l ? postb : post));
		if (curlret != CURLE_OK)
			goto curl_fail;
		curlret =
		    curl_easy_setopt(http_curl_handle, CURLOPT_POSTFIELDSIZE,
				     postb_l ? postb_l : strlen(post));
		if (curlret != CURLE_OK)
			goto curl_fail;
	} else {
		urlcopy = percentURL(url, NULL);
		curlret =
		    curl_easy_setopt(http_curl_handle, CURLOPT_HTTPGET, 1);
		if (curlret != CURLE_OK)
			goto curl_fail;
	}

	if (sendReferrer && currentReferrer) {
		const char *post2 = strchr(currentReferrer, '\1');
		if (!post2)
			post2 = currentReferrer + strlen(currentReferrer);
		if (post2 - currentReferrer >= 7
		    && !memcmp(post2 - 7, ".browse", 7))
			post2 -= 7;
		nzFree(cw->referrer);
		cw->referrer = cloneString(currentReferrer);
		cw->referrer[post2 - currentReferrer] = 0;
		referrer = cw->referrer;
	}

	curlret = curl_easy_setopt(http_curl_handle, CURLOPT_REFERER, referrer);
	if (curlret != CURLE_OK)
		goto curl_fail;
	curlret =
	    curl_easy_setopt(http_curl_handle, CURLOPT_HTTPHEADER,
			     custom_headers);
	if (curlret != CURLE_OK)
		goto curl_fail;
	curlret = setCurlURL(http_curl_handle, urlcopy);
	if (curlret != CURLE_OK)
		goto curl_fail;

	/* If we have a username and password, then tell libcurl about it.
	 * libcurl won't send it to the server unless server gave a 401 response.
	 * Libcurl selects the most secure form of auth provided by server. */

	if (user[0] && pass[0]) {
		strcpy(creds_buf, user);
		creds_len = strlen(creds_buf);
		creds_buf[creds_len] = ':';
		strcpy(creds_buf + creds_len + 1, pass);
	} else
		getUserPass(urlcopy, creds_buf, false);

/*
 * If the URL didn't have user and password, and getUserPass failed,
 * then creds_buf == "".
 */
	curlret =
	    curl_easy_setopt(http_curl_handle, CURLOPT_USERPWD, creds_buf);
	if (curlret != CURLE_OK)
		goto curl_fail;

/* We are ready to make a transfer.  Here is where it gets complicated.
 * At the top of the loop, we perform the HTTP request.  It may fail entirely
 * (I.E., libcurl returns an indicator other than CURLE_OK).
 * We may be redirected.  Edbrowse needs finer control over the redirection
 * process than libcurl gives us.
 * Decide whether to accept the redirection, using the following criteria.
 * Does user permit redirects?  Will we exceed maximum allowable redirects?
 * Is the destination in the fetch history?
 * We may be asked for authentication.  In that case, grab username and
 * password from the user.  If the server accepts the username and password,
 * then add it to the list of authentication records.  */

	still_fetching = true;
	serverData = initString(&serverDataLen);

	while (still_fetching == true) {
		char *redir = NULL;
		down_state = 0;
		down_file = NULL;
		down_permitted = down_ok;
		callback_data.length = &serverDataLen;

perform:
		curlret = fetch_internet(true);

		if (down_state == 6)
			goto mimestream;

		if (down_state == 5) {
/* user has directed a download of this file in the background. */
			background_download();
			if (down_state == 4)
				goto perform;
		}

		if (down_state == 3 || down_state == -1) {
/* set this to null so we don't push a new buffer */
			serverData = NULL;
			return false;
		}

		if (down_state == 4) {
			if (curlret != CURLE_OK) {
				ebcurl_setError(curlret, urlcopy);
				showError();
				exit(2);
			}
			i_printf(MSG_DownSuccess);
			printf(": %s\n", down_file2);
			exit(0);
		}

		if (*(callback_data.length) >= CHUNKSIZE)
			nl();	/* We printed dots, so we terminate them with newline */

		if (down_state == 2) {
			close(down_fd);
			setError(MSG_DownSuccess);
			serverData = NULL;
			return false;
		}

		if (curlret != CURLE_OK)
			goto curl_fail;
		curl_easy_getinfo(http_curl_handle, CURLINFO_RESPONSE_CODE,
				  &hcode);
		if (curlret != CURLE_OK)
			goto curl_fail;

		debugPrint(3, "http code %ld %s", hcode, herror);

/* refresh header is an alternate form of redirection */
		if (newlocation && newloc_d >= 0) {
			if (!refreshDelay(newloc_d, newlocation)) {
				nzFree(newlocation);
				newlocation = 0;
			} else {
				hcode = 302;
			}
		}

		if (allowRedirection &&
		    (hcode >= 301 && hcode <= 303 ||
		     hcode >= 307 && hcode <= 308)) {
			redir = newlocation;
			if (redir)
				redir = resolveURL(urlcopy, redir);
			still_fetching = false;
			if (redir == NULL) {
				/* Redirected, but we don't know where to go. */
				i_printf(MSG_RedirectNoURL, hcode);
				transfer_status = true;
			} else if (redirect_count >= 10) {
				i_puts(MSG_RedirectMany);
				transfer_status = true;
				nzFree(redir);
			} else {	/* redirection looks good. */
				strcpy(creds_buf, ":");	/* Flush stale data. */
				nzFree(urlcopy);
				urlcopy = redir;
				unpercentURL(urlcopy);

/* Convert POST request to GET request after redirection. */
/* This should only be done for 301 through 303 */
				if (hcode < 307)
					curl_easy_setopt(http_curl_handle,
							 CURLOPT_HTTPGET, 1);
/* I think there is more work to do for 307 308,
 * pasting the prior post string onto the new URL. Not sure about this. */

				getUserPass(urlcopy, creds_buf, false);

				curlret =
				    curl_easy_setopt(http_curl_handle,
						     CURLOPT_USERPWD,
						     creds_buf);
				if (curlret != CURLE_OK)
					goto curl_fail;

				curlret = setCurlURL(http_curl_handle, urlcopy);
				if (curlret != CURLE_OK)
					goto curl_fail;

				nzFree(serverData);
				serverData = emptyString;
				serverDataLen = 0;
				redirect_count += 1;
				still_fetching = true;
				name_changed = true;
				debugPrint(2, "redirect %s", urlcopy);
			}
		}

		else if (hcode == 401 && !proceed_unauthenticated) {
			i_printf(MSG_AuthRequired, urlcopy);
			nl();
			bool got_creds = read_credentials(creds_buf);
			if (got_creds) {
				addWebAuthorization(urlcopy, creds_buf, false);
				curl_easy_setopt(http_curl_handle,
						 CURLOPT_USERPWD, creds_buf);
				nzFree(serverData);
				serverData = emptyString;
				serverDataLen = 0;
			} else {
/* User aborted the login process, try and at least get something. */
				proceed_unauthenticated = true;
			}
		}
		/* authenticate? */
		else {		/* not redirect, not 401 */
			still_fetching = false;
			transfer_status = true;
		}
	}

curl_fail:
	if (custom_headers)
		curl_slist_free_all(custom_headers);
	if (curlret != CURLE_OK)
		ebcurl_setError(curlret, urlcopy);

	nzFree(newlocation);
	newlocation = 0;

	if (transfer_status == false) {
		nzFree(serverData);
		serverData = NULL;
		serverDataLen = 0;
		nzFree(urlcopy);	/* Free it on transfer failure. */
	} else {
		if (hcode != 200 && hcode != 201 &&
		    (webpage || debugLevel >= 2) ||
		    hcode == 201 && debugLevel >= 3)
			i_printf(MSG_HTTPError, hcode,
				 message_for_response_code(hcode));
		if (name_changed)
			changeFileName = urlcopy;
		else
			nzFree(urlcopy);	/* Don't need it anymore. */
	}

	nzFree(postb);

/* Check for plugin to run here */
	if (transfer_status && hcode == 200 && cw->mt && pluginsOn &&
	    !cw->mt->stream && !cw->mt->outtype && cw->mt->program) {
		bool rc = playServerData();
		nzFree(serverData);
		serverData = NULL;
		serverDataLen = 0;
		return rc;
	}

	return transfer_status;
}				/* httpConnect */

/* Format a line from an ftp ls. */
static void ftpls(char *line)
{
	int l = strlen(line);
	int j;
	if (l && line[l - 1] == '\r')
		line[--l] = 0;

/* blank line becomes paragraph break */
	if (!l || memEqualCI(line, "total ", 6) && stringIsNum(line + 6)) {
		stringAndString(&serverData, &serverDataLen, "<P>\n");
		return;
	}
	stringAndString(&serverData, &serverDataLen, "<br>");

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
			stringAndString(&serverData, &serverDataLen,
					"<A HREF=x");
			serverData[serverDataLen - 1] = qc;
			t = strstr(q, " -> ");
			if (t)
				stringAndBytes(&serverData, &serverDataLen, q,
					       t - q);
			else
				stringAndString(&serverData, &serverDataLen, q);
			stringAndChar(&serverData, &serverDataLen, qc);
			stringAndChar(&serverData, &serverDataLen, '>');
			stringAndString(&serverData, &serverDataLen, q);
			stringAndString(&serverData, &serverDataLen, "</A>");
			if (line[0] == 'd')
				stringAndChar(&serverData, &serverDataLen, '/');
			stringAndString(&serverData, &serverDataLen, ": ");
			stringAndNum(&serverData, &serverDataLen, fsize);
			stringAndChar(&serverData, &serverDataLen, '\n');
			return;
		}
	}

	if (!strpbrk(line, "<>&")) {
		stringAndString(&serverData, &serverDataLen, line);
	} else {
		char c, *q;
		for (q = line; c = *q; ++q) {
			char *meta = 0;
			if (c == '<')
				meta = "&lt;";
			if (c == '>')
				meta = "&gt;";
			if (c == '&')
				meta = "&amp;";
			if (meta)
				stringAndString(&serverData, &serverDataLen,
						meta);
			else
				stringAndChar(&serverData, &serverDataLen, c);
		}
	}

	stringAndChar(&serverData, &serverDataLen, '\n');
}				/* ftpls */

/* parse_directory_listing: convert an FTP-style listing to html. */
/* Repeatedly calls ftpls to parse each line of the data. */
static void parse_directory_listing(void)
{
	char *incomingData = serverData;
	int incomingLen = serverDataLen;
	serverData = initString(&serverDataLen);
	stringAndString(&serverData, &serverDataLen, "<html>\n<body>\n");
	char *s, *t;

	if (!incomingLen) {
		i_stringAndMessage(&serverData, &serverDataLen,
				   MSG_FTPEmptyDir);
	} else {

		s = incomingData;
		while (s < incomingData + incomingLen) {
			t = strchr(s, '\n');
			if (!t || t >= incomingData + incomingLen)
				break;	/* should never happen */
			*t = 0;
			ftpls(s);
			s = t + 1;
		}
	}

	stringAndString(&serverData, &serverDataLen, "</body></html>\n");
	nzFree(incomingData);
}				/* parse_directory_listing */

void ebcurl_setError(CURLcode curlret, const char *url)
{
	const char *host = NULL, *protocol = NULL;
	protocol = getProtURL(url);
	host = getHostURL(url);

/* this should never happen */
	if (!host)
		host = emptyString;

	switch (curlret) {

	case CURLE_UNSUPPORTED_PROTOCOL:
		setError(MSG_WebProtBad, protocol);
		break;
	case CURLE_URL_MALFORMAT:
		setError(MSG_BadURL, url);
		break;
	case CURLE_COULDNT_RESOLVE_HOST:
		setError(MSG_IdentifyHost, host);
		break;
	case CURLE_REMOTE_ACCESS_DENIED:
		setError(MSG_RemoteAccessDenied);
		break;
	case CURLE_TOO_MANY_REDIRECTS:
		setError(MSG_RedirectMany);
		break;

	case CURLE_OPERATION_TIMEDOUT:
		setError(MSG_Timeout);
		break;
	case CURLE_PEER_FAILED_VERIFICATION:
	case CURLE_SSL_CACERT:
		setError(MSG_NoCertify, host);
		break;

	case CURLE_GOT_NOTHING:
	case CURLE_RECV_ERROR:
		setError(MSG_WebRead);
		break;
	case CURLE_SEND_ERROR:
		setError(MSG_CurlSendData);
		break;
	case CURLE_COULDNT_CONNECT:
		setError(MSG_WebConnect, host);
		break;
	case CURLE_FTP_CANT_GET_HOST:
		setError(MSG_FTPConnect);
		break;

	case CURLE_ABORTED_BY_CALLBACK:
		setError(MSG_Interrupted);
		break;
/* These all look like session initiation failures. */
	case CURLE_FTP_WEIRD_SERVER_REPLY:
	case CURLE_FTP_WEIRD_PASS_REPLY:
	case CURLE_FTP_WEIRD_PASV_REPLY:
	case CURLE_FTP_WEIRD_227_FORMAT:
	case CURLE_FTP_COULDNT_SET_ASCII:
	case CURLE_FTP_COULDNT_SET_BINARY:
	case CURLE_FTP_PORT_FAILED:
		setError(MSG_FTPSession);
		break;

	case CURLE_FTP_USER_PASSWORD_INCORRECT:
		setError(MSG_LogPass);
		break;

	case CURLE_FTP_COULDNT_RETR_FILE:
		setError(MSG_FTPTransfer);
		break;

	case CURLE_SSL_CONNECT_ERROR:
		setError(MSG_SSLConnectError, errorText);
		break;

	case CURLE_LOGIN_DENIED:
		setError(MSG_LogPass);
		break;

	default:
		setError(MSG_CurlCatchAll, curl_easy_strerror(curlret));
		break;
	}
}				/* ebcurl_setError */

/* Like httpConnect, but for ftp */
static bool ftpConnect(const char *url, const char *user, const char *pass)
{
	int protLength;		/* length of "ftp://" */
	int urlcopy_l = 0;
	bool transfer_success = false;
	bool has_slash, is_scp;
	CURLcode curlret = CURLE_OK;
	char creds_buf[MAXUSERPASS * 2 + 1];
	size_t creds_len = 0;

	urlcopy = NULL;
	protLength = strchr(url, ':') - url + 3;
/* scp is somewhat unique among the protocols handled here */
	is_scp = memEqualCI(url, "scp", 3);

	if (user[0] && pass[0]) {
		strcpy(creds_buf, user);
		creds_len = strlen(creds_buf);
		creds_buf[creds_len] = ':';
		strcpy(creds_buf + creds_len + 1, pass);
	} else if (memEqualCI(url, "ftp", 3)) {
		strcpy(creds_buf, "anonymous:ftp@example.com");
	}

	curlret =
	    curl_easy_setopt(http_curl_handle, CURLOPT_USERPWD, creds_buf);
	if (curlret != CURLE_OK)
		goto ftp_transfer_fail;

	serverData = initString(&serverDataLen);
	urlcopy = initString(&urlcopy_l);
	stringAndString(&urlcopy, &urlcopy_l, url);

/* libcurl appends an implicit slash to URLs like "ftp://foo.com".
* Be explicit, so that edbrowse knows that we have a directory. */

	if (!strchr(urlcopy + protLength, '/'))
		stringAndChar(&urlcopy, &urlcopy_l, '/');

	curlret = setCurlURL(http_curl_handle, urlcopy);
	if (curlret != CURLE_OK)
		goto ftp_transfer_fail;

	has_slash = urlcopy[urlcopy_l - 1] == '/';
/* don't download a directory listing, we want to see that */
/* Fetching a directory will fail in the special case of scp. */
	down_state = (has_slash ? 0 : 1);
	down_file = NULL;	/* should already be null */
	down_msg = MSG_FTPDownload;
	if (is_scp)
		down_msg = MSG_SCPDownload;
	callback_data.length = &serverDataLen;

perform:
	curlret = fetch_internet(false);

	if (down_state == 5) {
/* user has directed a download of this file in the background. */
		background_download();
		if (down_state == 4)
			goto perform;
	}

	if (down_state == 3 || down_state == -1) {
/* set this to null so we don't push a new buffer */
		serverData = NULL;
		return false;
	}

	if (down_state == 4) {
		if (curlret != CURLE_OK) {
			ebcurl_setError(curlret, urlcopy);
			showError();
			exit(2);
		}
		i_printf(MSG_DownSuccess);
		printf(": %s\n", down_file2);
		exit(0);
	}

	if (*(callback_data.length) >= CHUNKSIZE)
		nl();		/* We printed dots, so we terminate them with newline */

	if (down_state == 2) {
		close(down_fd);
		setError(MSG_DownSuccess);
		serverData = NULL;
		return false;
	}

/* Should we run this code on any error condition? */
/* The SSH error pops up under sftp. */
	if (curlret == CURLE_FTP_COULDNT_RETR_FILE ||
	    curlret == CURLE_REMOTE_FILE_NOT_FOUND || curlret == CURLE_SSH) {
		if (has_slash | is_scp)
			transfer_success = false;
		else {		/* try appending a slash. */
			stringAndChar(&urlcopy, &urlcopy_l, '/');
			down_state = 0;
			curlret = setCurlURL(http_curl_handle, urlcopy);
			if (curlret != CURLE_OK)
				goto ftp_transfer_fail;

			curlret = fetch_internet(false);
			if (curlret != CURLE_OK)
				transfer_success = false;
			else {
				parse_directory_listing();
				transfer_success = true;
			}
		}
	} else if (curlret == CURLE_OK) {
		if (has_slash == true)
			parse_directory_listing();
		transfer_success = true;
	} else
		transfer_success = false;

ftp_transfer_fail:
	if (transfer_success == false) {
		if (curlret != CURLE_OK)
			ebcurl_setError(curlret, urlcopy);
		nzFree(serverData);
		serverData = 0;
		serverDataLen = 0;
	}
	if (transfer_success == true && !stringEqual(url, urlcopy))
		changeFileName = urlcopy;
	else
		nzFree(urlcopy);

	return transfer_success;
}				/* ftpConnect */

/* If the user has asked for locale-specific responses, then build an
 * appropriate Accept-Language: header. */
void setHTTPLanguage(const char *lang)
{
	int httpLanguage_l;

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
	int success = fcntl(socketfd, F_SETFD, FD_CLOEXEC);
	if (success == -1)
		success = 1;
	else
		success = 0;
	return success;
}

/* Clean up libcurl's state. */
static void my_curl_cleanup(void)
{
	curl_easy_cleanup(http_curl_handle);
	curl_global_cleanup();
}				/* my_curl_cleanup */

void http_curl_init(void)
{
	CURLcode curl_init_status = CURLE_OK;
	http_curl_handle = curl_easy_init();
	if (http_curl_handle == NULL)
		i_printfExit(MSG_LibcurlNoInit);

/* Lots of these setopt calls shouldn't fail.  They just diddle a struct. */
	curl_easy_setopt(http_curl_handle, CURLOPT_SOCKOPTFUNCTION,
			 my_curl_safeSocket);
	curl_easy_setopt(http_curl_handle, CURLOPT_WRITEFUNCTION,
			 eb_curl_callback);
	curl_easy_setopt(http_curl_handle, CURLOPT_WRITEDATA, &callback_data);
	curl_easy_setopt(http_curl_handle, CURLOPT_HEADERFUNCTION,
			 curl_header_callback);
	if (debugLevel >= 4)
		curl_easy_setopt(http_curl_handle, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(http_curl_handle, CURLOPT_DEBUGFUNCTION,
			 ebcurl_debug_handler);
	curl_easy_setopt(http_curl_handle, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt(http_curl_handle, CURLOPT_PROGRESSFUNCTION,
			 curl_progress);
	curl_easy_setopt(http_curl_handle, CURLOPT_CONNECTTIMEOUT, webTimeout);
	curl_easy_setopt(http_curl_handle, CURLOPT_USERAGENT, currentAgent);
	curl_easy_setopt(http_curl_handle, CURLOPT_SSLVERSION,
			 CURL_SSLVERSION_DEFAULT);

/*
* tell libcurl to pick the strongest method from basic, digest and ntlm authentication
* don't use any auth method as it will prefer Negotiate to NTLM,
* and it looks like in most cases microsoft IIS says it supports both and libcurl
* doesn't fall back to NTLM when it discovers that Negotiate isn't set up on a system
*/
	curl_easy_setopt(http_curl_handle, CURLOPT_HTTPAUTH,
			 CURLAUTH_BASIC | CURLAUTH_DIGEST | CURLAUTH_NTLM);

/* The next few setopt calls could allocate or perform file I/O. */
	errorText[0] = '\0';
	curl_init_status =
	    curl_easy_setopt(http_curl_handle, CURLOPT_ERRORBUFFER, errorText);
	if (curl_init_status != CURLE_OK)
		goto libcurl_init_fail;
	curl_init_status =
	    curl_easy_setopt(http_curl_handle, CURLOPT_CAINFO, sslCerts);
	if (curl_init_status != CURLE_OK)
		goto libcurl_init_fail;
	curl_init_status =
	    curl_easy_setopt(http_curl_handle, CURLOPT_COOKIEFILE, cookieFile);
	if (curl_init_status != CURLE_OK)
		goto libcurl_init_fail;
	curl_init_status =
	    curl_easy_setopt(http_curl_handle, CURLOPT_COOKIEJAR, cookieFile);
	if (curl_init_status != CURLE_OK)
		goto libcurl_init_fail;
	curl_init_status =
	    curl_easy_setopt(http_curl_handle, CURLOPT_ENCODING, "");
	if (curl_init_status != CURLE_OK)
		goto libcurl_init_fail;
	atexit(my_curl_cleanup);

libcurl_init_fail:
	if (curl_init_status != CURLE_OK)
		i_printfExit(MSG_LibcurlNoInit);
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
 * Note: prompt and error_message should be message constants from messages.h.
 * Return value: none.  buffer contains input on return. */

/* We need to read two things from the user while authenticating: a username
 * and a password.  Here, the task of prompting and reading is encapsulated
 * in a function, and we call that function twice.
 * After the call, the buffer contains the user's input, without a newline.
 * The return value is the length of the string in buffer. */
static int
prompt_and_read(int prompt, char *buffer, int buffer_length, int error_message)
{
	bool reading = true;
	int n = 0;
	while (reading) {
		i_printf(prompt);
		fflush(stdout);
		if (!fgets(buffer, buffer_length, stdin))
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
				    MSG_UserNameLong);
		if (!stringEqual(buffer, "x")) {
			char *password_ptr = buffer + input_length + 1;
			prompt_and_read(MSG_Password, password_ptr, MAXUSERPASS,
					MSG_PasswordLong);
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
curl_header_callback(char *header_line, size_t size, size_t nmemb, void *unused)
{
	size_t bytes_in_line = size * nmemb;
	stringAndBytes(&http_headers, &http_headers_len,
		       header_line, bytes_in_line);

	if (down_permitted && down_state == 0 && !hct[0]) {
		scan_http_headers(true);
		if (cw->mt && cw->mt->stream && pluginsOn) {
/* I don't think this ever happens, since streams are indicated by the protocol,
 * and we wouldn't even get here, but just in case -
 * stop the download and set the flag so we can pass this url
 * to the program that handles this kind of stream. */
			down_state = 6;
			return -1;
		}
		if (hct[0] && !memEqualCI(hct, "text/", 5) &&
		    (!pluginsOn || !cw->mt || cw->mt->download)) {
			down_state = 1;
			down_msg = MSG_Down;
		}
	}

	return bytes_in_line;
}				/* curl_header_callback */

/* Print text, discarding the unnecessary carriage return character. */
static void
prettify_network_text(const char *text, size_t size, FILE * destination)
{
	int i;
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
		     size_t size, void *unused)
{
	static bool last_curlin = false;

	if (info_desc == CURLINFO_HEADER_OUT) {
		printf("curl>\n");
		prettify_network_text(data, size, stdout);
	} else if (info_desc == CURLINFO_HEADER_IN) {
		if (!last_curlin)
			printf("curl<\n");
		prettify_network_text(data, size, stdout);
	} else;			/* Do nothing.  We don't care about this piece of data. */

	if (info_desc == CURLINFO_HEADER_IN)
		last_curlin = true;
	else if (info_desc)
		last_curlin = false;

	return 0;
}				/* ebcurl_debug_handler */

/* At this point, down_state = 1 */
static void setup_download(void)
{
	const char *filepart;
	const char *answer;
	int msg;

/* if not run from a terminal then just return. */
	if (!isatty(0)) {
		down_state = 0;
		return;
	}

	filepart = getFileURL(urlcopy, true);
top:
	answer = getFileName(down_msg, filepart, false, true);
/* space for a filename means read into memory */
	if (stringEqual(answer, " ")) {
		down_state = 0;	/* in memory download */
		return;
	}

	if (stringEqual(answer, "x") || stringEqual(answer, "X")) {
		down_state = -1;
		setError(MSG_DownAbort);
		return;
	}

	if (!envFileDown(answer, &answer)) {
		showError();
		goto top;
	}

	down_fd = creat(answer, 0666);
	if (down_fd < 0) {
		i_printf(MSG_NoCreate2, answer);
		nl();
		goto top;
	}

	down_file = down_file2 = answer;
	if (downDir) {
		int l = strlen(downDir);
		if (!strncmp(down_file2, downDir, l)) {
			down_file2 += l;
			if (down_file2[0] == '/')
				++down_file2;
		}
	}
	down_state = (down_bg ? 5 : 2);
	callback_data.length = &down_length;
}				/* setup_download */

/* At this point, down_state = 5 */
static void background_download(void)
{
	down_pid = fork();
	if (down_pid < 0) {	/* should never happen */
		down_state = -1;
/* perhaps a better error message here */
		setError(MSG_DownAbort);
		return;
	}

	if (down_pid) {		/* parent */
		struct BG_JOB *job;
		close(down_fd);
/* the error message here isn't really an error, but a progress message */
		setError(MSG_DownProgress);
		down_state = 3;

/* push job onto the list, for tracking and display */
		job = allocMem(sizeof(struct BG_JOB) + strlen(down_file2));
		job->pid = down_pid;
		job->state = 4;
		strcpy(job->file, down_file2);
		addToListBack(&down_jobs, job);

		return;
	}

/* child doesn't need javascript */
	js_disconnect();
/* ignore interrupt, not sure about quit and hangup */
	signal(SIGINT, SIG_IGN);
	down_state = 4;
}				/* background_download */

/* show background jobs and return the number of jobs pending */
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
		puts(j->file);
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
		puts(j->file);
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
		puts(j->file);
	}
	if (part)
		puts("}");

	if (!present)
		i_puts(MSG_Empty);

	return numback;
}				/* bg_jobs */

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

CURLcode setCurlURL(CURL * h, const char *url)
{
	const char *proxy = findProxyForURL(url);
	if (!proxy)
		proxy = "";
	else
		debugPrint(3, "proxy %s", proxy);
	const char *host = getHostURL(url);
	unsigned long verify = mustVerifyHost(host);
	curl_easy_setopt(h, CURLOPT_PROXY, proxy);
	curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, verify);
	curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, verify);
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

const char *findProxyForURL(const char *url)
{
	return findProxyInternal(getProtURL(url), getHostURL(url));
}				/* findProxyForURL */
