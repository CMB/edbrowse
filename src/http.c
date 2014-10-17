/* http.c
 * HTTP protocol client implementation
 * (c) 2002 Mikulas Patocka
 * This file is part of the Links project, released under GPL.
 *
 * Modified by Karl Dahlke for integration with edbrowse,
 * which is also released under the GPL.
 *
 * Modified by Chris Brannon to allow cooperation with libcurl.
 */

#include "eb.h"

#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>

CURL *curl_handle = NULL;
char *serverData;
int serverDataLen;
static char errorText[CURL_ERROR_SIZE + 1];
static char *httpLanguage;

static struct eb_curl_callback_data callback_data = {
	&serverData, &serverDataLen
};

static eb_bool ftpConnect(const char *url, const char *user, const char *pass);
static eb_bool read_credentials(char *buffer);
static void init_header_parser(void);
static size_t curl_header_callback(char *header_line, size_t size, size_t nmemb,
				   void *unused);
static const char *message_for_response_code(int code);

/* Read from a socket, 100K at a time. */
#define CHUNKSIZE 1000000

/* Callback used by libcurl. Writes data to serverData. */
size_t
eb_curl_callback(char *incoming, size_t size, size_t nitems,
		 struct eb_curl_callback_data *data)
{
	size_t num_bytes = nitems * size;
	int dots1, dots2;
	dots1 = *(data->length) / CHUNKSIZE;
	stringAndBytes(data->buffer, data->length, incoming, num_bytes);
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
		intFlag = eb_false;
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
			return EMPTYSTRING;
		lp = 0;
		while ((uchar) s[lp] >= ' ' && s[lp] != ';')
			lp++;
		return pullString(s, lp);
	}
	return NULL;
}				/* extractHeaderParam */

/* Date format is:    Mon, 03 Jan 2000 21:29:33 GMT */
			/* Or perhaps:     Sun Nov  6 08:49:37 1994 */
time_t parseHeaderDate(const char *date)
{
	static const char *const months[12] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	time_t t = 0;
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

	t = mktime(&tm);
	if (t != (time_t) - 1)
		return t;

fail:
	return 0;
}				/* parseHeaderDate */

eb_bool parseRefresh(char *ref, int *delay_p)
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
		if (delay)
			debugPrint(3, "delay %d %s", delay, ref);
		*delay_p = delay;
		return eb_true;
	}
	i_printf(MSG_GarbledRefresh, ref);
	*delay_p = 0;
	return eb_false;
}				/* parseRefresh */

/* Return true if we waited for the duration, false if interrupted.
 * I don't know how to do this in Windows. */
eb_bool refreshDelay(int sec, const char *u)
{
/* the value 15 seconds is somewhat arbitrary */
	if (sec < 15)
		return eb_true;
	i_printf(MSG_RedirectDelayed, u, sec);
	return eb_false;
}				/* refreshDelay */

static char hexdigits[] = "0123456789abcdef";
#define ESCAPED_CHAR_LENGTH 3

/*
 * Function: copy_and_sanitize
 * Arguments:
 ** start: pointer to start of input string
  ** end: pointer to end of input string.
 * Return value: A new string or NULL if memory allocation failed.
 * This function copies its input to a dynamically-allocated buffer,
 * while performing the following transformation.  Change backslash to
 * slash, and percent-escape any blank, non-printing, or non-ASCII
 * characters.
 * All characters in the area between start and end, not including end,
 * are copied or transformed.

 * Get rid of :/   curl can't handle it.

 * This function is used to sanitize user-supplied URLs.  */

char *copy_and_sanitize(const char *start, const char *end)
{
	int bytes_to_alloc = end - start + 1;
	char *new_copy = NULL;
	const char *in_pointer = NULL;
	char *out_pointer = NULL;
	const char *portloc = NULL;

	for (in_pointer = start; in_pointer < end; in_pointer++)
		if (*in_pointer <= 32)
			bytes_to_alloc += (ESCAPED_CHAR_LENGTH - 1);
	new_copy = allocMem(bytes_to_alloc);
	if (new_copy) {
		char *frag, *params;
		out_pointer = new_copy;
		for (in_pointer = start; in_pointer < end; in_pointer++) {
			if (*in_pointer == '\\')
				*out_pointer++ = '/';
			else if (*in_pointer <= 32) {
				*out_pointer++ = '%';
				*out_pointer++ =
				    hexdigits[(uchar) (*in_pointer & 0xf0) >>
					      4];
				*out_pointer++ =
				    hexdigits[(*in_pointer & 0x0f)];
			} else
				*out_pointer++ = *in_pointer;
		}
		*out_pointer = '\0';
/* excise #hash, required by some web servers */
		frag = strchr(new_copy, '#');
		if (frag) {
			params = strchr(new_copy, '?');
			if (params && params > frag)
				strmove(frag, params);
			else
				*frag = 0;
		}

		getPortLocURL(new_copy, &portloc, 0);
		if (portloc && !isdigit(portloc[1])) {
			const char *s = portloc + strcspn(portloc, "/?#\1");
			strmove((char *)portloc, s);
		}
	}

	return new_copy;
}				/* copy_and_sanitize */

long hcode;			/* example, 404 */
static char herror[32];		/* example, file not found */
extern char *newlocation;
extern int newloc_d;

eb_bool httpConnect(const char *from, const char *url)
{
	char *referrer = NULL;
	CURLcode curlret = CURLE_OK;
	struct curl_slist *custom_headers = NULL;
	struct curl_slist *tmp_headers = NULL;
	char user[MAXUSERPASS], pass[MAXUSERPASS];
	char creds_buf[MAXUSERPASS * 2 + 1];	/* creds abr. for credentials */
	int creds_len = 0;
	eb_bool still_fetching = eb_true;
	int ssl_version;
	const char *host;
	struct MIMETYPE *mt;
	const char *prot;
	char *cmd;
	char suffix[12];
	const char *post, *s;
	char *postb = NULL;
	char *urlcopy = NULL;
	int postb_l = 0;
	eb_bool transfer_status = eb_false;
	int redirect_count = 0;
	eb_bool name_changed = eb_false;

	serverData = NULL;
	serverDataLen = 0;
	strcpy(creds_buf, ":");	/* Flush stale username and password. */

/* Pull user password out of the url */
	user[0] = pass[0] = 0;
	s = getUserURL(url);
	if (s) {
		if (strlen(s) >= sizeof(user) - 2) {
			setError(MSG_UserNameLong, sizeof(user));
			return eb_false;
		}
		strcpy(user, s);
	}
	s = getPassURL(url);
	if (s) {
		if (strlen(s) >= sizeof(pass) - 2) {
			setError(MSG_PasswordLong, sizeof(pass));
			return eb_false;
		}
		strcpy(pass, s);
	}

	prot = getProtURL(url);

/* See if the protocol is a recognized stream */
	if (!prot) {
		setError(MSG_WebProtBad, "(?)");
		return eb_false;
	}

	if (stringEqualCI(prot, "http") || stringEqualCI(prot, "https")) {
		;		/* ok for now */
	} else if (stringEqualCI(prot, "ftp") ||
		   stringEqualCI(prot, "ftps") ||
		   stringEqualCI(prot, "tftp") || stringEqualCI(prot, "sftp")) {
		return ftpConnect(url, user, pass);
	} else if (mt = findMimeByProtocol(prot)) {
mimeProcess:
		cmd = pluginCommand(mt, url, 0);
/* Stop ignoring SIGPIPE for the duration of system(): */
		signal(SIGPIPE, SIG_DFL);
		system(cmd);
		signal(SIGPIPE, SIG_IGN);
		nzFree(cmd);
		return eb_true;
	} else {
		setError(MSG_WebProtBad, prot);
		return eb_false;
	}

/* Ok, it's http, but the suffix could force a plugin */
	post = url + strcspn(url, "?#\1");
	for (s = post - 1; s >= url && *s != '.' && *s != '/'; --s) ;
	if (*s == '.') {
		++s;
		if (post >= s + sizeof(suffix))
			post = s + sizeof(suffix) - 1;
		strncpy(suffix, s, post - s);
		suffix[post - s] = 0;
		if ((mt = findMimeBySuffix(suffix)) && mt->stream)
			goto mimeProcess;
	}

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
		urlcopy = copy_and_sanitize(url, post);
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
		curlret = curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS,
					   (postb_l ? postb : post));
		if (curlret != CURLE_OK)
			goto curl_fail;
		curlret = curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE,
					   postb_l ? postb_l : strlen(post));
		if (curlret != CURLE_OK)
			goto curl_fail;
	} else {
		urlcopy = copy_and_sanitize(url, url + strlen(url));
		curlret = curl_easy_setopt(curl_handle, CURLOPT_HTTPGET, 1);
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

	curlret = curl_easy_setopt(curl_handle, CURLOPT_REFERER, referrer);
	if (curlret != CURLE_OK)
		goto curl_fail;
	curlret =
	    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, custom_headers);
	if (curlret != CURLE_OK)
		goto curl_fail;
	curlret = setCurlURL(curl_handle, urlcopy);
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
		getUserPass(urlcopy, creds_buf, eb_false);

/*
 * If the URL didn't have user and password, and getUserPass failed,
 * then creds_buf == "".
 */
	curlret = curl_easy_setopt(curl_handle, CURLOPT_USERPWD, creds_buf);
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

	still_fetching = eb_true;
	ssl_version = CURL_SSLVERSION_DEFAULT;
	serverData = initString(&serverDataLen);

	while (still_fetching == eb_true) {
		char *redir = NULL;
		curl_easy_setopt(curl_handle, CURLOPT_SSLVERSION, ssl_version);
		init_header_parser();
		curlret = curl_easy_perform(curl_handle);
		if (serverDataLen >= CHUNKSIZE)
			nl();	/* We printed dots, so we terminate them with newline */
		if (curlret == CURLE_SSL_CONNECT_ERROR) {
/* all this would be unnecessary if curl sent the proper hello message */
/* try the next version */
			if (ssl_version == CURL_SSLVERSION_DEFAULT) {
				ssl_version = CURL_SSLVERSION_SSLv3;
				debugPrint(3, "stepping back to sslv3");
				continue;
			}
			if (ssl_version == CURL_SSLVERSION_SSLv3) {
				ssl_version = CURL_SSLVERSION_TLSv1;
				debugPrint(3, "stepping back to tlsv1");
				continue;
			}
/* probably shouldn't step down to SSLv2; it is considered to be insecure */
		}
		if (curlret != CURLE_OK)
			goto curl_fail;
		curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &hcode);
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

		if (hcode >= 301 && hcode <= 303 && allowRedirection) {
			redir = newlocation;
			if (redir)
				redir = resolveURL(urlcopy, redir);
			still_fetching = eb_false;
			if (redir == NULL) {
				/* Redirected, but we don't know where to go. */
				i_printf(MSG_RedirectNoURL, hcode);
				transfer_status = eb_true;
			} else if (redirect_count >= 10) {
				i_puts(MSG_RedirectMany);
				transfer_status = eb_true;
				nzFree(redir);
			} else {	/* redirection looks good. */
				strcpy(creds_buf, ":");	/* Flush stale data. */
				nzFree(urlcopy);
				urlcopy = redir;
				unpercentURL(urlcopy);

/* Convert POST request to GET request after redirection. */
				curl_easy_setopt(curl_handle, CURLOPT_HTTPGET,
						 1);

				getUserPass(urlcopy, creds_buf, eb_false);

				curlret =
				    curl_easy_setopt(curl_handle,
						     CURLOPT_USERPWD,
						     creds_buf);
				if (curlret != CURLE_OK)
					goto curl_fail;

				curlret = setCurlURL(curl_handle, urlcopy);
				if (curlret != CURLE_OK)
					goto curl_fail;

				nzFree(serverData);
				serverData = EMPTYSTRING;
				serverDataLen = 0;
				redirect_count += 1;
				still_fetching = eb_true;
				name_changed = eb_true;
				debugPrint(2, "redirect %s", urlcopy);

/* after redirection, go back to default ssl version. */
/* It might be a completely different server. */
/* Some day we might want to cache which domains require which ssl versions */
				ssl_version = CURL_SSLVERSION_DEFAULT;
			}
		}

		else if (hcode == 401) {
			i_printf(MSG_AuthRequired, urlcopy);
			nl();
			eb_bool got_creds = read_credentials(creds_buf);
			if (got_creds) {
				addWebAuthorization(urlcopy, creds_buf,
						    eb_false);
				curl_easy_setopt(curl_handle, CURLOPT_USERPWD,
						 creds_buf);
				nzFree(serverData);
				serverData = EMPTYSTRING;
				serverDataLen = 0;
			} else {	/* User aborted the login process. */
				still_fetching = eb_false;
				transfer_status = eb_false;
			}
		}
		/* authenticate? */
		else {		/* not redirect, not 401 */
			still_fetching = eb_false;
			transfer_status = eb_true;
		}
	}

curl_fail:
	if (custom_headers)
		curl_slist_free_all(custom_headers);
	if (curlret != CURLE_OK)
		ebcurl_setError(curlret, urlcopy);

	nzFree(newlocation);
	newlocation = 0;

	if (transfer_status == eb_false) {
		nzFree(serverData);
		serverData = NULL;
		serverDataLen = 0;
		nzFree(urlcopy);	/* Free it on transfer failure. */
	} else {
		if (hcode != 200)
			i_printf(MSG_HTTPError, hcode,
				 message_for_response_code(hcode));
		if (name_changed)
			changeFileName = urlcopy;
		else
			nzFree(urlcopy);	/* Don't need it anymore. */
	}

	nzFree(postb);
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
		host = EMPTYSTRING;

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
static eb_bool ftpConnect(const char *url, const char *user, const char *pass)
{
	int protLength;		/* length of "ftp://" */
	char *urlcopy = NULL;
	int urlcopy_l = 0;
	eb_bool transfer_success = eb_false;
	eb_bool has_slash;
	CURLcode curlret = CURLE_OK;
	char creds_buf[MAXUSERPASS * 2 + 1];
	size_t creds_len = 0;

	protLength = strchr(url, ':') - url + 3;

	if (user[0] && pass[0]) {
		strcpy(creds_buf, user);
		creds_len = strlen(creds_buf);
		creds_buf[creds_len] = ':';
		strcpy(creds_buf + creds_len + 1, pass);
	} else if (memEqualCI(url, "ftp", 3)) {
		strcpy(creds_buf, "anonymous:ftp@example.com");
	}

	curlret = curl_easy_setopt(curl_handle, CURLOPT_USERPWD, creds_buf);
	if (curlret != CURLE_OK)
		goto ftp_transfer_fail;

	serverData = initString(&serverDataLen);
	urlcopy = initString(&urlcopy_l);
	stringAndString(&urlcopy, &urlcopy_l, url);

/* libcurl appends an implicit slash to URLs like "ftp://foo.com".
* Be explicit, so that edbrowse knows that we have a directory. */

	if (!strchr(urlcopy + protLength, '/'))
		stringAndChar(&urlcopy, &urlcopy_l, '/');

	has_slash = urlcopy[urlcopy_l - 1] == '/';
	if (debugLevel >= 2)
		i_puts(MSG_FTPDownload);

	curlret = setCurlURL(curl_handle, urlcopy);
	if (curlret != CURLE_OK)
		goto ftp_transfer_fail;
	curlret = curl_easy_perform(curl_handle);

/* Should we run this code on any error condition? */
/* The SSH error pops up under sftp. */
	if (curlret == CURLE_FTP_COULDNT_RETR_FILE ||
	    curlret == CURLE_REMOTE_FILE_NOT_FOUND || curlret == CURLE_SSH) {
		if (has_slash == eb_true)	/* Was a directory. */
			transfer_success = eb_false;
		else {		/* try appending a slash. */
			stringAndChar(&urlcopy, &urlcopy_l, '/');
			curlret = setCurlURL(curl_handle, urlcopy);
			if (curlret != CURLE_OK)
				goto ftp_transfer_fail;

			curlret = curl_easy_perform(curl_handle);
			if (curlret != CURLE_OK)
				transfer_success = eb_false;
			else {
				parse_directory_listing();
				transfer_success = eb_true;
			}
		}
	} else if (curlret == CURLE_OK) {
		if (has_slash == eb_true)
			parse_directory_listing();
		transfer_success = eb_true;
	} else
		transfer_success = eb_false;

	if (serverDataLen >= CHUNKSIZE)
		nl();		/* We printed dots, so we terminate them with newline */

ftp_transfer_fail:
	if (transfer_success == eb_false) {
		if (curlret != CURLE_OK)
			ebcurl_setError(curlret, urlcopy);
		nzFree(serverData);
		serverData = 0;
		serverDataLen = 0;
	}
	if (transfer_success == eb_true && !stringEqual(url, urlcopy))
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
	curl_easy_cleanup(curl_handle);
	curl_global_cleanup();
}				/* my_curl_cleanup */

void my_curl_init(void)
{
	const unsigned int major = 7;
	const unsigned int minor = 29;
	const unsigned int patch = 0;
	const unsigned int least_acceptable_version =
	    (major << 16) | (minor << 8) | patch;
	curl_version_info_data *version_data = NULL;
	CURLcode curl_init_status = curl_global_init(CURL_GLOBAL_ALL);
	if (curl_init_status != 0)
		i_printfExit(MSG_LibcurlNoInit);
	curl_handle = curl_easy_init();
	if (curl_handle == NULL)
		i_printfExit(MSG_LibcurlNoInit);
	version_data = curl_version_info(CURLVERSION_NOW);
	if (version_data->version_num < least_acceptable_version)
		i_printfExit(MSG_CurlVersion, major, minor, patch);

/* Lots of these setopt calls shouldn't fail.  They just diddle a struct. */
	curl_easy_setopt(curl_handle, CURLOPT_SOCKOPTFUNCTION,
			 my_curl_safeSocket);

	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, eb_curl_callback);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &callback_data);
	curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION,
			 curl_header_callback);
	if (debugLevel >= 4)
		curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(curl_handle, CURLOPT_DEBUGFUNCTION,
			 ebcurl_debug_handler);
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt(curl_handle, CURLOPT_PROGRESSFUNCTION, curl_progress);
	curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, webTimeout);
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, currentAgent);
	curl_easy_setopt(curl_handle, CURLOPT_SSLVERSION,
			 CURL_SSLVERSION_DEFAULT);

/*
* tell libcurl to pick the strongest method from basic, digest and ntlm authentication
* don't use any auth method as it will prefer Negotiate to NTLM,
* and it looks like in most cases microsoft IIS says it supports both and libcurl
* doesn't fall back to NTLM when it discovers that Negotiate isn't set up on a system
*/
	curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH,
			 CURLAUTH_BASIC | CURLAUTH_DIGEST | CURLAUTH_NTLM);

/* The next few setopt calls could allocate or perform file I/O. */
	errorText[0] = '\0';
	curl_init_status =
	    curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, errorText);
	if (curl_init_status != CURLE_OK)
		goto libcurl_init_fail;
	curl_init_status =
	    curl_easy_setopt(curl_handle, CURLOPT_CAINFO, sslCerts);
	if (curl_init_status != CURLE_OK)
		goto libcurl_init_fail;
	curl_init_status =
	    curl_easy_setopt(curl_handle, CURLOPT_COOKIEFILE, cookieFile);
	if (curl_init_status != CURLE_OK)
		goto libcurl_init_fail;
	curl_init_status =
	    curl_easy_setopt(curl_handle, CURLOPT_COOKIEJAR, cookieFile);
	if (curl_init_status != CURLE_OK)
		goto libcurl_init_fail;
	curl_init_status = curl_easy_setopt(curl_handle, CURLOPT_ENCODING, "");
	if (curl_init_status != CURLE_OK)
		goto libcurl_init_fail;
	atexit(my_curl_cleanup);

libcurl_init_fail:
	if (curl_init_status != CURLE_OK)
		i_printfExit(MSG_LibcurlNoInit);
}				/* my_curl_init */

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
	eb_bool reading = eb_true;
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
			reading = eb_false;
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

static eb_bool read_credentials(char *buffer)
{
	int input_length = 0;
	eb_bool got_creds = eb_false;

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
				got_creds = eb_true;
				*(password_ptr - 1) = ':';	/* separate user and password with colon. */
			}
		}

		if (!got_creds)
			setError(MSG_LoginAbort);
	}

	return got_creds;
}				/* read_credentials */

/*
 * This code reads a stream of header lines from libcurl.
 * It can go away one of these days, when we're sure that everyone is using
 * version 7.18.2 or greater of libcurl.
*/
/* Call this before every HTTP request. */
static void init_header_parser(void)
{
/* this should already be 0 */
	nzFree(newlocation);
	newlocation = NULL;
}				/* init_header_parser */

static const char *loc_field = "Location:";
static size_t loc_field_length = 9;	/* length of string "Location:" */
static const char *refresh_field = "Refresh:";
static size_t refresh_field_length = 8;	/* length of string "Refresh:" */

/* Callback used by libcurl.
 * Right now, it just extracts Location: headers. */
static size_t
curl_header_callback(char *header_line, size_t size, size_t nmemb, void *unused)
{
	size_t bytes_in_line = size * nmemb;
	char *last_pos = header_line + bytes_in_line;

	/* If we're still looking for a location: header, and this line is long
	 * enough to be one, and the line starts with "Location: ", then proceed.
	 */

	if ((newlocation == NULL) && (bytes_in_line > loc_field_length) &&
	    memEqualCI(header_line, loc_field, loc_field_length)) {
		const char *start = header_line + loc_field_length;
		const char *end = last_pos - 1;

/* Make start point to first non-whitespace char after Location: or to
 * last_pos if no such char exists. */
		while (isspaceByte(*start) && (start < last_pos))
			start++;

/* end points to start of trailing whitespace if it exists.  Otherwise,
 * it is last_pos. */
		while ((end >= start) && isspaceByte(*end))
			end--;
		end++;

		if (start < end)
			newlocation = copy_and_sanitize(start, end);
		newloc_d = -1;
	}

	if ((bytes_in_line > refresh_field_length) &&
	    memEqualCI(header_line, refresh_field, refresh_field_length)) {
/* want to use parseRefresh, but that has to end in null */
		char *start = header_line + loc_field_length;
		char *end = last_pos - 1;
		int delay;

/* Make start point to first non-whitespace char after Refresh: or to
 * last_pos if no such char exists. */
		while (isspaceByte(*start) && (start < last_pos))
			start++;

/* end points to start of trailing whitespace if it exists.  Otherwise,
 * it is last_pos. */
		while ((end >= start) && isspaceByte(*end))
			end--;
		end++;

		if (start < end) {
			memmove(header_line, start, end - start);
			header_line[end - start] = 0;	/* now null terminated */
			if (parseRefresh(header_line, &delay)) {
				unpercentURL(header_line);
				gotoLocation(cloneString(header_line), delay,
					     eb_true);
			}
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
	static eb_bool last_curlin = eb_false;

	if (info_desc == CURLINFO_HEADER_OUT) {
		printf("curl>\n");
		prettify_network_text(data, size, stdout);
	} else if (info_desc == CURLINFO_HEADER_IN) {
		if (!last_curlin)
			printf("curl<\n");
		prettify_network_text(data, size, stdout);
	} else;			/* Do nothing.  We don't care about this piece of data. */

	if (info_desc == CURLINFO_HEADER_IN)
		last_curlin = eb_true;
	else if (info_desc)
		last_curlin = eb_false;

	return 0;
}				/* ebcurl_debug_handler */
