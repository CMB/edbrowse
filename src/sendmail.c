/* sendmail.c
 * Send mail using the smtp protocol.
 * Send the contents of a file, or the current edbrowse buffer.
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"

#include <time.h>

#define MAXRECAT 100		/* max number of recipients or attachments */
#define MAXMSLINE 1024		/* max mail server line */

static char serverLine[MAXMSLINE];
static bool doSignature;
static char subjectLine[400];
static int mailAccount;

static struct ALIAS {
	char name[16];
	char email[64];
} *addressList;
static int nads;		/* number of addresses */
static time_t adbooktime;

/* read and/or refresh the address book */
bool loadAddressBook(void)
{
	char *buf, *bufend, *v, *last, *s, *t;
	bool cmt = false;
	char state = 0, c;
	int j, buflen, ln = 1;
	time_t mtime;

	if (!addressFile ||
	    (mtime = fileTimeByName(addressFile)) == -1 || mtime <= adbooktime)
		return true;

	debugPrint(3, "loading address book");
	nzFree(addressList);
	addressList = 0;
	nads = 0;
	if (!fileIntoMemory(addressFile, &buf, &buflen))
		return false;
	bufend = buf + buflen;

	for (s = t = last = buf; s < bufend; ++s) {
		c = *s;
		if (cmt) {
			if (c != '\n')
				continue;
			cmt = false;
		}
		if (c == ':') {	/* delimiter */
			if (state == 0) {
				setError(MSG_ABNoAlias, ln);
freefail:
				nzFree(buf);
				nads = 0;
				return false;
			}
			while (t[-1] == ' ' || t[-1] == '\t')
				--t;
			if (state == 1) {
				*t++ = c;
				state = 2;
				continue;
			}
			c = '#';	/* extra fields are ignored */
		}		/* : */
		if (c == '#') {
			cmt = true;
			continue;
		}
		if (c == '\n') {
			++ln;
			if (state == 0)
				continue;
			if (state == 1) {
				setError(MSG_ABNoColon, ln - 1);
				goto freefail;
			}
			if (state == 3) {
				++nads;
				while (isspaceByte(t[-1]))
					--t;
				*t = 0;
				v = strchr(last, ':');
				if (v - last >= 16) {
					setError(MSG_ABAliasLong, ln - 1);
					goto freefail;
				}
				++v;
				if (t - v >= 64) {
					setError(MSG_ABMailLong, ln - 1);
					goto freefail;
				}
				if (!strchr(v, '@')) {
					setError(MSG_ABNoAt, ln - 1);
					goto freefail;
				}
				if (strpbrk(v, " \t")) {
					setError(MSG_ABMailSpaces, ln - 1);
					goto freefail;
				}

				while (last < t) {
					if (!isprintByte(*last)) {
						setError(MSG_AbMailUnprintable,
							 ln - 1);
						goto freefail;
					}
					++last;
				}
				*t++ = c;
			} else
				t = last;	/* back it up */
			last = t;
			state = 0;
			continue;
		}
		/* nl */
		if ((c == ' ' || c == '\t') && (state == 0 || state == 2))
			continue;
		if (state == 0)
			state = 1;
		if (state == 2)
			state = 3;
		*t++ = c;
	}

	*t = 0;
	if (state) {
		setError(MSG_ABUnterminated);
		goto freefail;
	}

	if (nads) {
		addressList = allocMem(nads * sizeof(struct ALIAS));
		j = 0;
		for (s = buf; *s; s = t + 1, ++j) {
			t = strchr(s, ':');
			memcpy(addressList[j].name, s, t - s);
			addressList[j].name[t - s] = 0;
			s = t + 1;
			t = strchr(s, '\n');
			memcpy(addressList[j].email, s, t - s);
			addressList[j].email[t - s] = 0;
		}
	}
	/* aliases are present */
	nzFree(buf);
	adbooktime = mtime;
	return true;
}				/* loadAddressBook */

const char *reverseAlias(const char *reply)
{
	int i;
	for (i = 0; i < nads; ++i)
		if (stringEqual(reply, addressList[i].email)) {
			const char *a = addressList[i].name;
			if (*a == '!')
				break;
			return a;
		}
	return 0;		/* not found */
}				/* reverseAlias */

static char *qpEncode(const char *line)
{
	char *newbuf;
	int l;
	const char *s;
	char c;

	newbuf = initString(&l);
	for (s = line; (c = *s); ++s) {
		if ((c < '\n' && c != '\t') || c == '=') {
			char expand[4];
			sprintf(expand, "=%02X", (uchar) c);
			stringAndString(&newbuf, &l, expand);
		} else {
			stringAndChar(&newbuf, &l, c);
		}
	}

	return newbuf;
}				/* qpEncode */

/* Return 0 if there was no need to encode */
static char *isoEncode(char *start, char *end)
{
	int nacount = 0, count = 0, len;
	char *s, *t;
	char c, code;

	for (s = start; s < end; ++s) {
		c = *s;
		if (c == 0)
			*s = ' ';
		if (isspaceByte(c))
			*s = ' ';
	}

	for (s = start; s < end; ++s) {
		c = *s;
		++count;
		if (!isprintByte(c) && c != ' ')
			++nacount;
	}

	if (!nacount)
		return 0;

	if (nacount * 4 >= count && count > 8) {
		code = 'B';
		s = base64Encode(start, end - start, false);
		goto package;
	}

	code = 'Q';
	s = qpEncode(start);

package:
	len = strlen(s);
	t = allocMem(len + 20);
	sprintf(t, "=?ISO-8859-1?%c?%s?=", code, s);
	nzFree(s);
	return t;
}				/* isoEncode */

/*********************************************************************
Return a string that defines the charset of the outgoing mail.
This just looks at your language setting - reaaly dumb.
It should interrogate each file/attachment.
Well, this will get us started.
*********************************************************************/

static char *charsetString(const char *ct, const char *ce)
{
	static char buf[24];
	buf[0] = 0;
	if (!stringEqual(ce, "7bit") &&
	    (stringEqual(ct, "text/plain") || stringEqual(ct, "text/html"))) {
		if (cons_utf8)
			strcpy(buf, "; charset=utf-8");
		else
			sprintf(buf, "; charset=iso-8859-%d", type8859);
	}
	return buf;
}				/* charsetString */

/* Read a file into memory, mime encode it,
 * and return the type of encoding and the encoded data.
 * Last three parameters are result parameters.
 * If ismail is nonzero, the file is the mail, not an attachment.
 * In fact ismail indicates the line that holds the subject.
 * If ismail is negative, then -ismail indicates the subject line,
 * and the string file is not the filename, but rather, the mail to send. */
bool
encodeAttachment(const char *file, int ismail, bool webform,
		 const char **type_p, const char **enc_p, char **data_p)
{
	char *buf;
	char c;
	bool longline;
	char *s, *t, *v;
	char *ct, *ce;		/* content type, content encoding */
	int buflen, i, cx;
	int nacount, nullcount, nlcount;

	if (ismail < 0) {
		buf = cloneString(file);
		buflen = strlen(buf);
		ismail = -ismail;
		file = emptyString;
	} else {

		if (!ismc && (cx = stringIsNum(file)) >= 0) {
			static char newfilename[16];
			if (!unfoldBuffer(cx, false, &buf, &buflen))
				return false;
			if (!buflen) {
				if (webform) {
empty:
					buf = emptyString;
					ct = "text/plain";
					ce = "7bit";
					goto success;
				}
				setError(MSG_BufferXEmpty, cx);
				goto freefail;
			}
			sprintf(newfilename, "<buffer %d>", cx);
			file = newfilename;
			if (sessionList[cx].lw->f0.fileName)
				file = sessionList[cx].lw->f0.fileName;
		} else {
			if (!fileIntoMemory(file, &buf, &buflen))
				return false;
			if (!buflen) {
				if (webform)
					goto empty;
				setError(MSG_FileXEmpty, file);
				goto freefail;
			}
		}
	}			/* ismail negative or normal */

	if (ismail) {
/* Put newline at the end.  Yes, the buffer is allocated
 * with space for newline and null. */
		if (buf[buflen - 1] != '\n')
			buf[buflen++] = '\n';
/* check for subject: line */
		s = buf;
		i = ismail;
		while (--i) {
			while (*s != '\n')
				++s;
			++s;
		}
		while (*s == ' ' || *s == '\t')
			++s;
		if (!memEqualCI(s, "subject:", 8)) {
			setError(MSG_SubjectStart);
			goto freefail;
		}
		s += 8;
		while (*s == ' ' || *s == '\t')
			++s;
		t = s;
		while (*s != '\n')
			++s;
		v = s;
		while (s > t && isspaceByte(s[-1]))
			--s;
		if (s - t >= sizeof(subjectLine)) {
			setError(MSG_SubjectLong, sizeof(subjectLine) - 1);
			goto freefail;
		}
		if (s > t)
			memcpy(subjectLine, t, s - t);
		subjectLine[s - t] = 0;
		if (subjectLine[0]) {
			char *subjiso = isoEncode(subjectLine,
						  subjectLine +
						  strlen(subjectLine));
			if (subjiso) {
				if (strlen(subjiso) >= sizeof(subjectLine)) {
					nzFree(subjiso);
					setError(MSG_SubjectLong,
						 sizeof(subjectLine) - 1);
					goto freefail;
				}
				strcpy(subjectLine, subjiso);
				nzFree(subjiso);
			}
		}
		debugPrint(6, "subject = %s", subjectLine);
/* Blank lines after subject are optional, and ignored. */
		for (t = buf + buflen; v < t; ++v)
			if (*v != '\r' && *v != '\n')
				break;
		buflen -= (v - buf);
		if (buflen)
			memmove(buf, v, buflen);
		buf[buflen] = 0;

		if (doSignature) {	/* Append .signature file. */
/* Try account specific .signature file, then fall back to .signature */
			sprintf(sigFileEnd, "%d", mailAccount);
			c = fileTypeByName(sigFile, false);
			if (!c) {
				*sigFileEnd = 0;
				c = fileTypeByName(sigFile, false);
			}
			if (c != 0) {
				int fd, n;
				if (c != 'f') {
					setError(MSG_SigRegular);
					goto freefail;
				}
				n = fileSizeByName(sigFile);
				if (n > 0) {
					buf = reallocMem(buf, buflen + n + 1);
					fd = open(sigFile, O_RDONLY);
					if (fd < 0) {
						setError(MSG_SigAccess);
						goto freefail;
					}
					read(fd, buf + buflen, n);
					close(fd);
					buflen += n;
					buf[buflen] = 0;
				}
			}
		}		/* .signature */
	}

	/* Infer content type from the filename */
	ct = 0;
	s = strrchr(file, '.');
	if (s && s[1]) {
		++s;
		if (stringEqualCI(s, "ps"))
			ct = "application/PostScript";
		if (stringEqualCI(s, "jpeg"))
			ct = "image/jpeg";
		if (stringEqualCI(s, "gif"))
			ct = "image/gif";
		if (stringEqualCI(s, "wav"))
			ct = "audio/basic";
		if (stringEqualCI(s, "mpeg"))
			ct = "video/mpeg";
		if (stringEqualCI(s, "rtf"))
			ct = "text/richtext";
		if (stringEqualCI(s, "htm") ||
		    stringEqualCI(s, "html") ||
		    stringEqualCI(s, "shtm") ||
		    stringEqualCI(s, "shtml") || stringEqualCI(s, "asp"))
			ct = "text/html";
	}

/* Count the nonascii characters */
	nacount = nullcount = nlcount = 0;
	longline = false;
	s = 0;
	for (i = 0; i < buflen; ++i) {
		c = buf[i];
		if (c == '\0')
			++nullcount;
		if (c < 0)
			++nacount;
		if (c != '\n')
			continue;
		++nlcount;
		t = buf + i;
		if (s && t - s > 120)
			longline = true;
		if (!s && i > 120)
			longline = true;
		s = t;
	}
	t = buf + i;
	if (s && t - s > 120)
		longline = true;
	if (!s && i > 120)
		longline = true;
	debugPrint(6, "attaching %s length %d nonascii %d nulls %d longline %d",
		   file, buflen, nacount, nullcount, longline);
	nacount += nullcount;

/* Set the type of attachment */
	if (buflen > 20 && nacount * 5 > buflen) {
		if (!ct)
			ct = "application/octet-stream";	/* default type for binary */
	}
	if (!ct)
		ct = "text/plain";

/* Criteria for base64 encode.
 * files uploaded from a web form need not be encoded, unless they contain
 * nulls, which is a quirk of my slapped together software. */

	if ((!webform && buflen > 20 && nacount * 5 > buflen) ||
	    (webform && nullcount)) {
		if (ismail) {
			setError(MSG_MailBinary, file);
			goto freefail;
		}
		s = base64Encode(buf, buflen, true);
		nzFree(buf);
		buf = s;
		ce = "base64";
		goto success;
	}

	if (!webform) {
/* Switch to unix newlines - we'll switch back to dos later. */
		v = buf + buflen;
		for (s = t = buf; s < v; ++s) {
			c = *s;
			if (c == '\r' && s < v - 1 && s[1] == '\n')
				continue;
			*t++ = c;
		}
		buflen = t - buf;

/* Do we need to use quoted-printable? */
/* Perhaps this hshould read (nacount > 0) */
		if (nacount * 20 > buflen || nullcount || longline) {
			char *newbuf;
			int l, colno = 0, space = 0;

			newbuf = initString(&l);
			v = buf + buflen;
			for (s = buf; s < v; ++s) {
				c = *s;
/* do we have to =expand this character? */
				if ((c < '\n' && c != '\t') ||
				    c == '=' ||
				    c == '\xff' ||
				    ((c == ' ' || c == '\t') &&
				     s < v - 1 && s[1] == '\n')) {
					char expand[4];
					sprintf(expand, "=%02X", (uchar) c);
					stringAndString(&newbuf, &l, expand);
					colno += 3;
				} else {
					stringAndChar(&newbuf, &l, c);
					++colno;
				}
				if (c == '\n') {
					colno = space = 0;
					continue;
				}
				if (c == ' ' || c == '\t')
					space = l;
				if (colno < 72)
					continue;
				if (s == v - 1)
					continue;
/* If newline's coming up anyways, don't force another one. */
				if (s[1] == '\n')
					continue;
				i = l;
				if (!space || space == i) {
					stringAndString(&newbuf, &l, "=\n");
					colno = space = 0;
					continue;
				}
				colno = i - space;
				stringAndString(&newbuf, &l, "**");	/* make room */
				while (i > space) {
					newbuf[i + 1] = newbuf[i - 1];
					--i;
				}
				newbuf[space] = '=';
				newbuf[space + 1] = '\n';
				space = 0;
			}	/* loop over characters */

			nzFree(buf);
			buf = newbuf;
			ce = "quoted-printable";
			goto success;
		}
	}

	buf[buflen] = 0;
	ce = (nacount ? "8bit" : "7bit");

success:
	debugPrint(6, "encoded %s %s length %d", ct, ce, strlen(buf));
	*enc_p = ce;
	*type_p = ct;
	*data_p = buf;
	return true;

freefail:
	nzFree(buf);
	return false;
}				/* encodeAttachment */

static char *mailTimeString(void)
{
	static char buf[48];
	struct tm *cur_tm;
	time_t now;
	static const char months[] =
	    "Jan\0Feb\0Mar\0Apr\0May\0Jun\0Jul\0Aug\0Sep\0Oct\0Nov\0Dec";
	static const char wdays[] = "Sun\0Mon\0Tue\0Wed\0Thu\0Fri\0Sat";

	time(&now);
	cur_tm = localtime(&now);

/*********************************************************************
	strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %z", cur_tm);
That's what I use to do, but a user in France always got a date
with French month and weekday, even though I setlocale(LC_TIME, "C");
I changed C to en_US, still the French shines through.
I don't understand it, so I just work around it.
This is an internet standard, it's suppose to be English.
*********************************************************************/

	sprintf(buf, "%s, %02d %s ",
		wdays + cur_tm->tm_wday * 4,
		cur_tm->tm_mday, months + cur_tm->tm_mon * 4);
// and strftime can do the rest.
	strftime(buf + 12, sizeof(buf) - 12, "%Y %H:%M:%S %z", cur_tm);

	return buf;
}				/* mailTimeString */

static char *messageTimeID(void)
{
	static char buf[48];
	struct tm *cur_tm;
	time_t now;
	time(&now);
	cur_tm = localtime(&now);
	sprintf(buf, "%04d%02d%02d%02d%02d%02d",
		cur_tm->tm_year + 1900, cur_tm->tm_mon, cur_tm->tm_mday,
		cur_tm->tm_hour, cur_tm->tm_min, cur_tm->tm_sec);
	return buf;
}				/* messageTimeID */

static void appendAttachment(const char *s, char **out, int *l)
{
	const char *t;
	int n;
	while (*s) {		/* another line */
		t = strchr(s, '\n');
		if (!t)
			t = s + strlen(s);
		n = t - s;
		if (t[-1] == '\r')
			--n;
		if (n)
			memcpy(serverLine, s, n);
		serverLine[n] = 0;
		strcat(serverLine, eol);
		stringAndString(out, l, serverLine);
		if (*t)
			++t;
		s = t;
	}
/* Small bug here - an attachment that is not base64 encoded,
 * and had no newline at the end, now has one. */
}				/* appendAttachment */

char *makeBoundary(void)
{
	static char boundary[24];
	sprintf(boundary, "nextpart-eb-%06d", rand() % 1000000);
	return boundary;
}				/* makeBoundary */

struct smtp_upload {
	const char *data;
/* These really need to be size_t, not int!
 * fixme when edbrowse uses size_t consistently */
	int length;
	int pos;
};

static size_t smtp_upload_callback(char *buffer_for_curl, size_t size,
				   size_t nmem, struct smtp_upload *upload)
{
	size_t out_buffer_size = size * nmem;
	size_t remaining = upload->length - upload->pos;
	size_t to_send, cur_pos;

	if (upload->pos >= upload->length)
		return 0;

	if (out_buffer_size < remaining)
		to_send = out_buffer_size;
	else
		to_send = remaining;

	memcpy(buffer_for_curl, upload->data + upload->pos, to_send);
	cur_pos = upload->pos + to_send;
	upload->pos = cur_pos;
	return to_send;
}				/* smtp_upload_callback */

static char *buildSMTPURL(const struct MACCOUNT *account)
{
	char *url = NULL;
	const char *scheme;
	const char *smlogin = strchr(account->login, '\\');

	if (smlogin)
		++smlogin;
	else
		smlogin = "unknown";

	if (account->outssl & 1)
		scheme = "smtps";
	else
		scheme = "smtp";

	if (asprintf
	    (&url, "%s://%s:%d/%s", scheme, account->outurl, account->outport,
	     smlogin) == -1)
		i_printfExit(MSG_NoMem);

	return url;
}				/* buildSMTPURL */

static struct curl_slist *buildRecipientSList(const char **recipients)
{
	struct curl_slist *recipient_slist = NULL;
	const char **r;

	for (r = recipients; *r; r++) {
		recipient_slist = curl_slist_append(recipient_slist, *r);
		if (recipient_slist == NULL)
			i_printfExit(MSG_NoMem);
	}

	return recipient_slist;
}				/* buildRecipientSList */

static CURL *newSendmailHandle(const struct MACCOUNT *account,
			       const char *outurl, const char *reply,
			       struct curl_slist *recipients)
{
	static struct i_get g;
	CURLcode res = CURLE_OK;
	CURL *handle = curl_easy_init();
	if (!handle) {
		setError(MSG_LibcurlNoInit);
		return NULL;
	}

	curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, mailTimeout);
	res = setCurlURL(handle, outurl);
	if (res != CURLE_OK) {
		goto new_handle_cleanup;
	}

	if (debugLevel >= 4)
		curl_easy_setopt(handle, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(handle, CURLOPT_DEBUGFUNCTION, ebcurl_debug_handler);
	curl_easy_setopt(handle, CURLOPT_DEBUGDATA, &g);

	if (account->outssl == 2)
		curl_easy_setopt(handle, CURLOPT_USE_SSL, CURLUSESSL_ALL);

	if (account->outssl) {
		res =
		    curl_easy_setopt(handle, CURLOPT_USERNAME, account->login);
		if (res != CURLE_OK) {
			goto new_handle_cleanup;
		}

		res =
		    curl_easy_setopt(handle, CURLOPT_PASSWORD,
				     account->password);
		if (res != CURLE_OK) {
			goto new_handle_cleanup;
		}
	}

	res = curl_easy_setopt(handle, CURLOPT_MAIL_FROM, reply);
	if (res != CURLE_OK) {
		goto new_handle_cleanup;
	}

	res = curl_easy_setopt(handle, CURLOPT_MAIL_RCPT, recipients);
	if (res != CURLE_OK) {
		goto new_handle_cleanup;
	}

new_handle_cleanup:
	if (res != CURLE_OK) {
		ebcurl_setError(res, outurl, 0, emptyString);
		curl_easy_cleanup(handle);
		handle = NULL;
	}

	return handle;
}				/* newSendmailHandle */

typedef struct tagsmtp_upload {
	const char *data;
	size_t length;
	size_t pos;
} smtp_upload;

static bool
sendMailSMTP(const struct MACCOUNT *account, const char *reply,
	     const char **recipients, const char *message)
{
	CURL *handle = 0;
	CURLcode res = CURLE_OK;
	bool smtp_success = false;
	char *smtp_url = buildSMTPURL(account);
	struct curl_slist *recipient_slist = buildRecipientSList(recipients);
	smtp_upload upload;
	upload.data = message;
	upload.length = strlen(message);
	upload.pos = 0;
	handle = newSendmailHandle(account, smtp_url, reply, recipient_slist);

	if (!handle)
		goto smtp_cleanup;

	curl_easy_setopt(handle, CURLOPT_READFUNCTION, smtp_upload_callback);
	curl_easy_setopt(handle, CURLOPT_READDATA, &upload);
	curl_easy_setopt(handle, CURLOPT_UPLOAD, 1L);

	res = curl_easy_perform(handle);
	if (res == CURLE_OK)
		smtp_success = true;

smtp_cleanup:
	if (res != CURLE_OK)
		ebcurl_setError(res, smtp_url, 0, emptyString);
	if (handle)
		curl_easy_cleanup(handle);
	curl_slist_free_all(recipient_slist);
	nzFree(smtp_url);
	return smtp_success;
}				/* sendMailSMTP */

/* Send mail to the smtp server. */
bool
sendMail(int account, const char **recipients, const char *body,
	 int subjat, const char **attachments, const char *refline,
	 int nalt, bool dosig)
{
	char *from, *fromiso, *reply;
	const struct MACCOUNT *a, *ao, *localMail;
	const char *s, *boundary;
	char reccc[MAXRECAT];
	char *t;
	int nat, cx, i, j;
	char *out = 0;
	bool sendmail_success = false;
	bool mustmime = false;
	bool firstrec;
	const char *ct, *ce;
	char *encoded = 0;

	if (!validAccount(account))
		return false;
	mailAccount = account;
	localMail = accounts + localAccount - 1;

	a = accounts + account - 1;
	from = a->from;
	reply = a->reply;
	ao = a->outssl ? a : localMail;
	doSignature = dosig;

	nat = 0;		/* number of attachments */
	if (attachments) {
		while (attachments[nat])
			++nat;
	}
	if (nat)
		mustmime = true;
	if (nalt && nalt < nat) {
		setError(MSG_AttAlternate);
		return false;
	}

	if (!loadAddressBook())
		return false;

/* set copy flags */
	for (j = 0; (s = recipients[j]); ++j) {
		char cc = 0;
		if (*s == '^' || *s == '?')
			cc = *s++;
		if (j == MAXRECAT) {
			setError(MSG_RecipMany, MAXRECAT);
			return false;
		}
		recipients[j] = s;
		reccc[j] = cc;
	}

/* Look up aliases in the address book */
	for (j = 0; (s = recipients[j]); ++j) {
		if (strchr(s, '@'))
			continue;
		t = 0;
		for (i = 0; i < nads; ++i) {
			const char *a = addressList[i].name;
			if (*a == '-' || *a == '!')
				++a;
			if (!stringEqual(s, a))
				continue;
			t = addressList[i].email;
			debugPrint(3, " %s becomes %s", s, t);
			break;
		}
		if (t) {
			recipients[j] = t;
			continue;
		}
		if (!addressFile) {
			setError(MSG_ABMissing);
			return false;
		}
		setError(MSG_ABNoAlias2, s);
		return false;
	}			/* recipients */

	if (!j) {
		setError(MSG_RecipNone);
		return false;
	}

/* verify attachments are readable */
	for (j = 0; (s = attachments[j]); ++j) {
		if (!ismc && (cx = stringIsNum(s)) >= 0) {
			if (!cxCompare(cx) || !cxActive(cx))
				return false;
			if (!sessionList[cx].lw->dol) {
				setError(MSG_AttSessionEmpty, cx);
				return false;
			}
		} else {
			char ftype = fileTypeByName(s, false);
			if (!ftype) {
				setError(MSG_AttAccess, s);
				return false;
			}
			if (ftype != 'f') {
				setError(MSG_AttRegular, s);
				return false;
			}
			if (!fileSizeByName(s)) {
				setError(MSG_AttEmpty2, s);
				return false;
			}
		}
	}			/* loop over attachments */

	if (!encodeAttachment(body, subjat, false, &ct, &ce, &encoded))
		return false;
	if (ce[0] == 'q')
		mustmime = true;

	boundary = makeBoundary();

/* Build the outgoing mail, as one string. */
	out = initString(&j);

	firstrec = true;
	for (i = 0; (s = recipients[i]); ++i) {
		if (reccc[i])
			continue;
		stringAndString(&out, &j, firstrec ? "To:" : ",\r\n  ");
		stringAndString(&out, &j, s);
		firstrec = false;
	}
	if (!firstrec)
		stringAndString(&out, &j, eol);

	firstrec = true;
	for (i = 0; (s = recipients[i]); ++i) {
		if (reccc[i] != '^')
			continue;
		stringAndString(&out, &j, firstrec ? "CC:" : ",\r\n  ");
		stringAndString(&out, &j, s);
		firstrec = false;
	}
	if (!firstrec)
		stringAndString(&out, &j, eol);

	firstrec = true;
	for (i = 0; (s = recipients[i]); ++i) {
		if (reccc[i] != '?')
			continue;
		stringAndString(&out, &j, firstrec ? "BCC:" : ",\r\n  ");
		stringAndString(&out, &j, s);
		firstrec = false;
	}
	if (!firstrec)
		stringAndString(&out, &j, eol);

	fromiso = isoEncode(from, from + strlen(from));
	if (!fromiso)
		fromiso = from;
	sprintf(serverLine, "From: %s <%s>%s", fromiso, reply, eol);
	stringAndString(&out, &j, serverLine);
	sprintf(serverLine, "Reply-to: %s <%s>%s", fromiso, reply, eol);
	stringAndString(&out, &j, serverLine);
	if (fromiso != from)
		nzFree(fromiso);
	if (refline) {
		s = strchr(refline, '\n');
		if (!s)		/* should never happen */
			s = refline + strlen(refline);
		stringAndBytes(&out, &j, refline, s - refline);
		stringAndString(&out, &j, eol);
	}
	sprintf(serverLine, "User-Agent: %s%s", currentAgent, eol);
	stringAndString(&out, &j, serverLine);
	if (subjectLine[0]) {
		sprintf(serverLine, "Subject: %s%s", subjectLine, eol);
		stringAndString(&out, &j, serverLine);
	}
	sprintf(serverLine,
		"Date: %s%sMessage-ID: <%s.%s>%sMime-Version: 1.0%s",
		mailTimeString(), eol, messageTimeID(), reply, eol, eol);
	stringAndString(&out, &j, serverLine);

	if (!mustmime) {
/* no mime components required, we can just send the mail. */
		sprintf(serverLine,
			"Content-Type: %s%s%sContent-Transfer-Encoding: %s%s%s",
			ct, charsetString(ct, ce), eol, ce, eol, eol);
		stringAndString(&out, &j, serverLine);
	} else {
		sprintf(serverLine,
			"Content-Type: multipart/%s; boundary=%s%sContent-Transfer-Encoding: 7bit%s%s",
			nalt ? "alternative" : "mixed", boundary, eol, eol,
			eol);
		stringAndString(&out, &j, serverLine);
		stringAndString(&out, &j,
				"This message is in MIME format. Since your mail reader does not understand\r\n\
this format, some or all of this message may not be legible.\r\n\r\n--");
		stringAndString(&out, &j, boundary);
		sprintf(serverLine,
			"%sContent-Type: %s%s%sContent-Transfer-Encoding: %s%s%s",
			eol, ct, charsetString(ct, ce), eol, ce, eol, eol);
		stringAndString(&out, &j, serverLine);
	}

/* Now send the body, line by line. */
	appendAttachment(encoded, &out, &j);
	nzFree(encoded);
	encoded = 0;

	if (mustmime) {
		for (i = 0; (s = attachments[i]); ++i) {
			if (!encodeAttachment(s, 0, false, &ct, &ce, &encoded))
				return false;
			sprintf(serverLine, "%s--%s%sContent-Type: %s%s", eol,
				boundary, eol, ct, charsetString(ct, ce));
			stringAndString(&out, &j, serverLine);
/* If the filename has a quote in it, forget it. */
/* Also, suppress filename if this is an alternate presentation. */
			if (!nalt && !strchr(s, '"')
			    && (ismc || stringIsNum(s) < 0)) {
				sprintf(serverLine, "; name=\"%s\"", s);
				stringAndString(&out, &j, serverLine);
			}
			sprintf(serverLine,
				"%sContent-Transfer-Encoding: %s%s%s", eol, ce,
				eol, eol);
			stringAndString(&out, &j, serverLine);
			appendAttachment(encoded, &out, &j);
			nzFree(encoded);
			encoded = 0;
		}		/* loop over attachments */

/* The last boundary */
		sprintf(serverLine, "%s--%s--%s", eol, boundary, eol);
		stringAndString(&out, &j, serverLine);
	}

	/* mime format */

	sendmail_success = sendMailSMTP(ao, reply, recipients, out);
	nzFree(out);
	return sendmail_success;
}				/* sendMail */

bool validAccount(int n)
{
	if (!maxAccount) {
		setError(MSG_MailAccountsNone);
		return false;
	}
	if (n <= 0 || n > maxAccount) {
		setError(MSG_MailAccountBad, n, maxAccount);
		return false;
	}
	return true;
}				/* validAccount */

bool sendMailCurrent(int sm_account, bool dosig)
{
	const char *reclist[MAXRECAT + 1];
	char *recmem;
	const char *atlist[MAXRECAT + 1];
	char *atmem;
	char *s, *t;
	char cxbuf[4];
	int lr, la, ln;
	char *refline = 0;
	int nrec = 0, nat = 0, nalt = 0;
	int account = localAccount;
	int j;
	bool rc = false;
	bool subj = false;

	if (cw->browseMode) {
		setError(MSG_MailBrowse);
		return false;
	}
	if (cw->sqlMode) {
		setError(MSG_MailDB);
		return false;
	}
	if (cw->dirMode) {
		setError(MSG_MailDir);
		return false;
	}
	if (cw->binMode) {
		setError(MSG_MailBinary2);
		return false;
	}
	if (!cw->dol) {
		setError(MSG_MailEmpty);
		return false;
	}

	if (!validAccount(account))
		return false;

	recmem = initString(&lr);
	atmem = initString(&la);

/* Gather recipients and attachments, until we reach subject: */
	for (ln = 1; ln <= cw->dol; ++ln) {
		char *line = (char *)fetchLine(ln, -1);

		if (memEqualCI(line, "to:", 3) ||
		    memEqualCI(line, "mailto:", 7) ||
		    memEqualCI(line, "cc:", 3) ||
		    memEqualCI(line, "bcc:", 4) ||
		    memEqualCI(line, "reply to:", 9) ||
		    memEqualCI(line, "reply to ", 9)) {
			char cc = 0;
			if (toupper(line[0]) == 'C')
				cc = '^';
			if (toupper(line[0]) == 'B')
				cc = '?';
			if (toupper(line[0]) == 'R')
				line += 9;
			else
				line = strchr(line, ':') + 1;
			while (*line == ' ' || *line == '\t')
				++line;
			if (*line == '\n') {
				setError(MSG_RecipNone2, ln);
				goto done;
			}
			if (nrec == MAXRECAT) {
				setError(MSG_RecipMany, MAXRECAT);
				goto done;
			}
			++nrec;
			for (t = line; *t != '\n'; ++t) ;
			if (cc) {
				if (!lr) {
					setError(MSG_MailFirstCC);
					goto done;
				}
				stringAndChar(&recmem, &lr, cc);
			}
			stringAndBytes(&recmem, &lr, line, t + 1 - line);
			continue;
		}

		if (memEqualCI(line, "attach:", 7)
		    || memEqualCI(line, "alt:", 4)) {
			if (toupper(line[1]) == 'T')
				line += 7;
			else
				line += 4, ++nalt;
			while (*line == ' ' || *line == '\t')
				++line;
			if (*line == '\n') {
				setError(MSG_AttLineX, ln);
				goto done;
			}
			if (nat == MAXRECAT) {
				setError(MSG_RecipMany, MAXRECAT);
				goto done;
			}
			++nat;
			for (t = line; *t != '\n'; ++t) ;
			stringAndBytes(&atmem, &la, line, t + 1 - line);
			continue;
		}

		if (memEqualCI(line, "account:", 8)) {
			line += 8;
			while (*line == ' ' || *line == '\t')
				++line;
			if (!isdigitByte(*line) ||
			    (account = strtol(line, &line, 10)) == 0 ||
			    account > maxAccount || *line != '\n') {
				setError(MSG_MailAccountBadLineX, ln);
				goto done;
			}
			continue;
		}

		if (memEqualCI(line, "references:", 11)) {
			if (!refline)
				refline = line;
			continue;
		}

		if (memEqualCI(line, "subject:", 8)) {
			while (*line == ' ' || *line == '\t')
				++line;
			subj = true;
		}

		break;
	}			/* loop over lines */

	if (sm_account)
		account = sm_account;
	if (!subj) {
		setError(((ln > cw->dol) + MSG_MailFirstLine), ln);
		goto done;
	}

	if (nrec == 0) {
		setError(MSG_RecipNone3);
		goto done;
	}

	for (s = recmem, j = 0; *s; s = t + 1, ++j) {
		t = strchr(s, '\n');
		*t = 0;
		reclist[j] = s;
	}
	reclist[j] = 0;
	for (s = atmem, j = 0; *s; s = t + 1, ++j) {
		t = strchr(s, '\n');
		*t = 0;
		atlist[j] = s;
	}
	atlist[j] = 0;

	sprintf(cxbuf, "%d", context);
	rc = sendMail(account, reclist, cxbuf, ln, atlist, refline, nalt,
		      dosig);

done:
	nzFree(recmem);
	nzFree(atmem);
	if (!rc && intFlag)
		setError(MSG_Interrupted);
	if (rc)
		i_puts(MSG_OK);
	return rc;
}				/* sendMailCurrent */
