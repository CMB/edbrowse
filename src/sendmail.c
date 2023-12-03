/* sendmail.c
 * Send mail using the smtp protocol.
 * Send the contents of a file, or the current edbrowse buffer.
 */

#include "eb.h"

#include <time.h>

#define MAXALIASLENGTH 16
#define MAXEMAILANDFULLNAMELENGTH 100
#define ABDELIMITER ':'		// character between fields in address book entry
#define FULLNAMEDELIMITER '>'		// character between email address and full name in address book entry
#define REPLACEMENTFULLNAMEDELIMITER '\n' // character temporarily used between email address and full name
#define MAXRECAT 1000		// max number of recipients or attachments
#define MAXMSLINE 1024		// max mail server line
#define LONGLINELIMIT 76
#define LONGWORDLIMIT 998

static char serverLine[MAXMSLINE];
static bool doSignature;
static char subjectLine[400];
static int mailAccount;

static struct ALIAS {
	char brief[MAXALIASLENGTH];
	char email[MAXEMAILANDFULLNAMELENGTH];
	char fullname[MAXEMAILANDFULLNAMELENGTH + 4];
} *addressList;
static int nads;		/* number of addresses */
static time_t adbooktime;

/* read and/or refresh the address book */
bool loadAddressBook(void)
{
	char *buf, *bufend, *v, *record_start, *email_start, *at_sign, *dot, *full_name_start, *s, *t;
	bool cmt = false;
	enum { alias, local_part, quoted_part, backslash_escaped, domain,
		full_name, ignore } state = alias;
	char c;
	int j, buflen, ln = 1;
	time_t mtime;

	if (!addressFile ||
	    (mtime = fileTimeByName(addressFile)) == -1 || mtime <= adbooktime)
		return true;

	debugPrint(3, "loading address book");
	nzFree(addressList);
	addressList = 0;
	nads = 0;
	if (!fileIntoMemory(addressFile, &buf, &buflen, 0))
		return false;
	bufend = buf + buflen;

if (buflen > 0 && bufend[-1] != '\n') {
		setError(MSG_ABUnterminated);
	freefail:
					nzFree(buf);
					nads = 0;
					return false;
	}

/* Remove comments and unimportant spaces and tabs. Make each record into a zero-terminated string. */
	for (record_start = email_start = at_sign = dot = full_name_start = s = t = buf; s < bufend; ++s) {
		c = *s;

// # at start of line is comment
		if(c == '#' && t == record_start) {
			state = ignore;
			continue;
		}

/* Ignore spaces and tabs at the start of aliases, email addresses, and full names.
Ignore all characters other than newline when ignoring until the end of the line. */
		if (((c == ' ' || c == '\t')
		&& ((state == alias && record_start == t)
		|| (state == local_part && email_start == t)
		|| (state == full_name && full_name_start == t)))
		|| (c != '\n' && state == ignore)) {
			continue;
		}

		if (c == ABDELIMITER && state == alias) {
/* Ignore spaces and tabs at the end of an alias. */
			while (t > record_start && (t[-1] == ' ' || t[-1] == '\t')) --t;
			if (record_start == t) {
				setError(MSG_ABNoAlias, ln);
				goto freefail;
			}
			if (t - record_start >= MAXALIASLENGTH) {
				setError(MSG_ABAliasLong, ln, MAXALIASLENGTH - 1);
				goto freefail;
			}
			for (v = record_start; v < t; ++v)
				if (iscntrl((uchar) *v)) {
					setError(MSG_ABMailUnprintable, ln);
					goto freefail;
				}
			*t++ = c;
			email_start = t;
			state = local_part;
			continue;
		}

// allow for fred::phone number: address:etc    -   don't have an email for him
// Even :  : will work cause we ignore spaces after the first colon
		if (c == ABDELIMITER && state == local_part && email_start == t) {
			t = record_start;
			state = ignore;
			continue;
		}

// finish the email address
		if (c == ABDELIMITER && (state == domain || state == full_name)) {
			state = ignore;
			continue;
		}

// switch to full name
		if (c == FULLNAMEDELIMITER && state == domain) {
			while (t[-1] == ' ' || t[-1] == '\t') --t;
			*t++ = REPLACEMENTFULLNAMEDELIMITER;
			full_name_start = t;
			state = full_name;
			continue;
		}

		if (c == '\n') {
			while (t > record_start && (t[-1] == ' ' || t[-1] == '\t')) --t;
			if (record_start < t) {
				char *domain_end = (full_name_start == record_start) ? t - 1 : full_name_start - 2;
				if (state == alias) {
					setError(MSG_ABNoColon, ln);
					goto freefail;
				}
				if (*domain_end == '-' ||
				domain_end == dot) {
malformed:
					setError(MSG_ABMalformed, ln);
					goto freefail;
				}
				if (at_sign <= email_start) {
					setError(MSG_ABNoAt, ln);
					goto freefail;
				}
// I think it's enough to cap email + name at 100
#if 0
				if (domain_end - dot > 63) {
					setError(MSG_ABMailLong, ln, 63);
					goto freefail;
				}
#endif
				if (t - email_start >= MAXEMAILANDFULLNAMELENGTH) {
					setError(MSG_ABMailLong, ln, MAXEMAILANDFULLNAMELENGTH - 1);
					goto freefail;
				}
				for (v = at_sign + 1; v <= domain_end; ++v)
					if ((*v & 0x80) == 0 && !isalnumByte((uchar) *v) && *v != '-' && *v != '.')
						goto malformed;
				if (full_name_start != record_start) {
					for (v = full_name_start; v < t; ++v)
						if (iscntrl((uchar) *v)) {
							setError(MSG_ABMailUnprintable, ln);
							goto freefail;
						}
				}
				++nads;
				*t++ = 0;
				record_start = email_start = at_sign = dot = full_name_start = t;
			}
			++ln;
			state = alias;
			continue;
		}

		if (state == local_part) {
			if (c == '"') {
				if (email_start != t && t[-1] != '.')
					goto malformed;
				state = quoted_part;
			} else if (c == '@') {
				if (email_start == t ||
				t[-1] == '.')
					goto malformed;
#if 0
				if (t - email_start > 64) {
					setError(MSG_ABMailLong, ln, 64);
					goto freefail;
				}
#endif
				at_sign = dot = t;
				state = domain;
			} else if (c == '.') {
				if (email_start == t ||
				t[-1] == '.')
					goto malformed;
			} else if (iscntrl((uchar) c) || strchr(" (),:;<>@[\\]", c))
				goto malformed;
		} else if (c == '\\' && state == quoted_part) {
			state = backslash_escaped;
		} else if (state == backslash_escaped) {
			state = quoted_part;
		} else if (c == '"' && state == quoted_part && s + 1 < bufend && (s[1] == '@' || s[1] == '.')) {
			state = local_part;
		} else if (state == domain) {
			if (c == '.') {
				if (t == at_sign + 1 ||
				t == dot + 1 ||
				t[-1] == '-')
					goto malformed;
#if 0
				if (t - dot > 64) {
					setError(MSG_ABMailLong, ln, 64);
					goto freefail;
				}
#endif
				dot = t;
			} else if (c == '-') {
				if (t == dot + 1)
					goto malformed;
			}
		}
		*t++ = c;
	}
	*t = 0;

	if (nads) {
		addressList = allocMem(nads * sizeof(struct ALIAS));
		j = 0;
		for (s = buf; *s; s = t + 1, ++j) {
			t = strchr(s, ABDELIMITER);
			memcpy(addressList[j].brief, s, t - s);
			addressList[j].brief[t - s] = 0;
			s = t + 1;
			t = strchr(s, 0);
			memcpy(addressList[j].email, s, t - s);
			addressList[j].email[t - s] = 0;
			addressList[j].fullname[0] = 0;
			if ((v = strchr(addressList[j].email, REPLACEMENTFULLNAMEDELIMITER))) {
				*v++ = 0;
				if (strlen(v))
					sprintf(addressList[j].fullname, "%s <%s>", v, addressList[j].email);
					else
					sprintf(addressList[j].fullname, "<%s>", addressList[j].email);
			}
		}
	}

	nzFree(buf);
	adbooktime = mtime;
	return true;
}

const char *reverseAlias(const char *reply)
{
	int i;
	for (i = 0; i < nads; ++i)
		if (stringEqual(reply, addressList[i].email)) {
			const char *a = addressList[i].brief;
			if (*a == '!')
				break;
			return a;
		}
	return 0;		/* not found */
}

static char *qpEncode(const char *line)
{
	char *newbuf;
	int l;
	const char *s;
	char c;
	newbuf = initString(&l);
	for (s = line; (c = *s); ++s) {
		if ((signed char)c < '\n' || c == '=' || c == '_' || c == '?') {
			char expand[4];
			sprintf(expand, "=%02X", (uchar) c);
			stringAndString(&newbuf, &l, expand);
		} else {
			if(c == ' ') c = '_';
			stringAndChar(&newbuf, &l, c);
		}
	}
	return newbuf;
}

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
	sprintf(t, "=?UTF-8?%c?%s?=", code, s);
	nzFree(s);
	return t;
}

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
}

/* Read a file into memory, mime encode it,
 * and return the type of encoding and the encoded data.
 * Last three parameters are result parameters.
 * If ismail is nonzero, the file is the mail, not an attachment.
 * In fact ismail indicates the line that holds the subject.
 * If ismail is negative, then -ismail indicates the subject line,
 * and the string file is not the filename, but rather, the mail to send. */
bool
encodeAttachment(const char *file, int ismail, bool webform,
		 const char **type_p, const char **enc_p, char **data_p,
bool *long_p)
{
	char *buf;
	char c;
	bool longline, longword, cr, endlinespace, startlinedot, flowed;
	char *s, *t, *v;
	char *ct, *ce;		// content type, content encoding
	int buflen, i, cx;
	int nacount, nullcount, linelength, wordlength;

	debugPrint(5, "subject at line %d", ismail);
	if (ismail < 0) {
		buf = cloneString(file);
		buflen = strlen(buf);
		ismail = -ismail;
		file = emptyString;
	} else {

		if (!ismc && (cx = stringIsNum(file)) >= 0) {
			static char newfilename[21];
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
			sprintf(newfilename, "<session %d>", cx);
			file = newfilename;
			if (sessionList[cx].lw->f0.fileName)
				file = sessionList[cx].lw->f0.fileName;
		} else {
			if (!fileIntoMemory(file, &buf, &buflen, 0))
				return false;
			if (!buflen) {
				if (webform)
					goto empty;
				setError(MSG_FileXEmpty, file);
				goto freefail;
			}
		}
	}			// ismail negative or normal

	if (ismail) {
// Put newline at the end.  Yes, the buffer is allocated
// with space for newline and null.
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
		if (!memEqualCI(s, "subject:", 8)
		&& !memEqualCI(s, "sub:", 4)) {
			setError(MSG_SubjectStart);
			goto freefail;
		}
		s += (s[3] == ':' ? 4 : 8);
		while (*s == ' ' || *s == '\t')
			++s;
		t = s;
		while (*s != '\n')
			++s;
		v = s;
		while (s > t && isspaceByte(s[-1]))
			--s;
		if ((unsigned)(s - t) >= sizeof(subjectLine)) {
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
		debugPrint(5, "subject = %s", subjectLine);
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
			c = fileTypeByName(sigFile, 0);
			if (!c) {
				*sigFileEnd = 0;
				c = fileTypeByName(sigFile, 0);
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
					ignore = read(fd, buf + buflen, n);
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

// alternative from a buffer is usually html; this doesn't fly if wev
// send it over as plain text. This is a crude test.
// Just look for a leading <
	if(!strncmp(file, "<session ", 9)) {
		for (i = 0; i < buflen; ++i) {
			c = buf[i];
			if(!isspaceByte(c)) break;
		}
		if(i < buflen && c == '<')
			ct = "text/html";
	}

/* Count the nonascii characters */
	nacount = nullcount = linelength = wordlength = 0;
	longline = longword = cr = endlinespace = startlinedot = flowed = false;
	for (t = buf, i = 0; i < buflen; ++i, ++t) {
		c = *t;
		if (c == '\0')
			++nullcount;
		if (c & 0x80)
			++nacount;
		if (c == '.' && linelength == 0 && t != buf)
			startlinedot = true;
		if (c == '\n') {
			if(linelength > LONGLINELIMIT) longline = true;
			linelength = 0;
		} else {
// measure length of line by utf8 characters
			if(((uchar)c & 0xc0) != 0x80)
				++linelength;
		}
		if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
			if (wordlength > LONGWORDLIMIT) longword = true;
			wordlength = 0;
			if (c == '\r' && i < buflen - 1 && t[1] != '\n') cr = true;
// If a line not at the end of a paragraph ends with a space,
// consider using format=flowed.
			if (c == ' ' && i + 2 < buflen &&
			((t[1] == '\n' && t[2] != '\n') ||
			(t[1] == '\r' && t[2] != '\r' && (t[2] != '\n' || (i + 3 < buflen && t[3] != '\r')))))
				endlinespace = true;
		} else ++wordlength;
	}
	if(linelength > LONGLINELIMIT) longline = true;
	if (wordlength > LONGWORDLIMIT) longword = true;
	debugPrint(5, "attaching %s length %d nonascii %d nulls %d longline %d",
		   file, buflen, nacount, nullcount, longline);
	nacount += nullcount;

/* Set the type of attachment */
	if (buflen > 20 && nacount * 4 > buflen && !ismail) {
		if (!ct)
			ct = "application/octet-stream";	/* default type for binary */
	}
	if (!ct)
		ct = "text/plain";

// base64 is almost obsolete, with networks and clients able to handle 8bit,
// but I still have to use it if the data contains even a single null,
// because edbrowse treats data as a string after this point.
// I'll still use 64 if it looks binary, perhaps a vital executable,
// rather than simple utf8 text.
// Also use base64 for text attachments that include a carriage return
// when using format=flowed and for all attachments when not using
// format=flowed.

	if ((!ismail && (cr || !flow)) || nullcount || looksBinary((uchar*)buf, buflen)) {
		s = base64Encode(buf, buflen, true);
		nzFree(buf);
		buf = s;
		ce = "base64";
		goto success;
	}

	ce = (nacount ? "8bit" : "7bit");
	if (webform || !(longline || (ismail && flow && endlinespace) || (!ismail && startlinedot))) {
		buf[buflen] = 0;
		goto success;
	}

// Use qp for long lines, it doesn't hurt,
// and when I send I copy lines into a static buffer of a fixed length.

	char *newbuf;
	int l, colno = 0, space = 0;
	if (ismail && flow && !longword) flowed = true;
	else if (longline) ce = "quoted-printable";
	newbuf = initString(&l);
	v = buf + buflen;
	for (s = buf; s < v; ++s) {
		c = *s;
// With format=flowed, skip the space characters at the end of a paragraph
// and double an = at the end of a line.
		if (flowed && c == '=') {
// skip past spaces
			for (t = s + 1; t < v && *t == ' '; ++t)  ;
			if(t == v || *t == '\r' || *t == '\n') {
				stringAndString(&newbuf, &l, "==");
				s = t - 1;
				continue;
			}
		}
		if (flowed && c == ' ') {
// skip past spaces
			for (t = s + 1; t < v && *t == ' '; ++t)  ;
			if(t == v ||
			((t[0] == '\r' || t[0] == '\n') && (t + 1 == v || t[1] == t[0] || (t[0] == '\n' && t[1] == '\r'))) ||
			(t[0] == '\r' && (t + 1 == v || (t[1] == '\n' && (t + 2 == v || t[2] == '\r' || t[2] == '\n'))))) {
				s = t - 1;
				continue;
			}
		}
// do we have to =expand this character?
		if (!flowed && longline &&
		((((uchar)c < '\n' && c != '\t') ||
		    c == '=' ||
		    (uchar)c == 0xff ||
		    ((c == ' ' || c == '\t') &&
		     s < v - 1 && (s[1] == '\n' || s[1] == '\r'))))) {
			char expand[4];
			sprintf(expand, "=%02X", (uchar) c);
			stringAndString(&newbuf, &l, expand);
			colno += 3;
		} else if (!ismail && startlinedot && c == '.' && s != buf && s[-1] == '\n') {
// In an attachment, add an extra dot at the start of a line (other than the first line).
			stringAndString(&newbuf, &l, "..");
			colno += 2;
		} else {
			stringAndChar(&newbuf, &l, c);
// make a utf8 char look like 1
			if((c&0x80) == 0 || (c&0xc0) == 0xc0) ++colno;
			if (flowed && c == ' ' && (s[1] == '\n' || s[1] == '\r')) {
// With format=flowed, when a space ends a line within a paragraph, add = so that we can flow the whole paragraph later.
				stringAndChar(&newbuf, &l, '=');
			}
		}
		if (c == '\n') {
			colno = space = 0;
			continue;
		}
		if (isspaceByte(c))
			space = l;
		if (colno < 72)
			continue;
		if (s == v - 1)
			continue;
// If newline's coming up anyways, don't force another one.
		if (s[1] == '\n')
			continue;
		i = l;
		if (!flowed && (!space || space == i)) {
			stringAndString(&newbuf, &l, "=\n");
			colno = space = 0;
			continue;
		}
// a long line without spaces, like a long url
		if(!space) continue;
		colno = i - space;
		stringAndString(&newbuf, &l, "**");	/* make room */
		while (i > space) {
			newbuf[i + 1] = newbuf[i - 1];
			--i;
		}
		newbuf[space] = '=';
		newbuf[space + 1] = '\n';
		space = 0;
	}	// loop over characters

	nzFree(buf);
	buf = newbuf;

success:
	debugPrint(5, "encoded %s %s length %d", ct, ce, strlen(buf));
	*enc_p = ce;
	*type_p = ct;
	*data_p = buf;
if(long_p) *long_p = flowed;
	debugPrint(6, "%s", buf);
	return true;

freefail:
	nzFree(buf);
	return false;
}

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
}

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
}

static void appendAttachment(const char *s, char **out, int *l, bool flowed,
const char *ct)
{
	const char *t;
	int n;
	int paraplus = -1;
	bool ta; // text attachment
	bool cr; // cr was at the end
// check to see it's not a base64 attachment
	ta = (ct && *ct != 'b');
	while (*s) {		/* another line */
		t = strchr(s, '\n');
		if (!t)
			t = s + strlen(s);
		n = t - s;
		cr = false;
		if (n && t[-1] == '\r')
			--n, cr = true;
		if(!n) {
			paraplus = -1;
		} else {
			if(flowed && paraplus < 0) {
				paraplus = 0;
// does this paragraph have any long lines; should we append +
				const char *u, *v, *x;
				v = strstr(s, "\n\n"); // end of paragraph
				x = strstr(s, "\n\r\n");
				if(!v || (x && x < v)) v = x;
// a long line will be cut by = by qp encode.
				u = strstr(s, "=\n");
				x = strstr(s, "=\r\n");
				if(!u || (x && x < u)) u = x;
				if(u && (!v || v > u)) paraplus = 1;
			}
			memcpy(serverLine, s, n);
// With format=flowed, put spaces on the end of lines in paragraphs containing
// a long line and remove = at the end of lines. */
			if(flowed && paraplus > 0) {
				if (serverLine[n-1] == '=')
					--n;
				if ((!n || serverLine[n-1] != ' ') &&
				t[0] && t[1] &&
				t[1] != '\n' && t[1] != '\r')
					serverLine[n++] = ' ';
			}
		}
		serverLine[n] = 0;
		strcat(serverLine, ((ta && !cr) ? "\n" : eol));
		stringAndString(out, l, serverLine);
		if (*t)
			++t;
		s = t;
	}
/* Small bug here - an attachment that is not base64 encoded,
 * and had no newline at the end, now has one. */
}

char *makeBoundary(void)
{
	static char boundary[24];
	sprintf(boundary, "nextpart-eb-%06d", rand() % 1000000);
	return boundary;
}

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
}

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
}

static struct curl_slist *buildRecipientSList(const char **recipients)
{
	struct curl_slist *recipient_slist = NULL;
	const char **r;
	const char *r1, *r2, *r3;

	for (r = recipients; *r; r++) {
// do not include names here
		r1 = *r;
		r2 = strchr(r1, '<');
		r3 = strchr(r1, '>');
		if(r2 && r3) {
// Yeah, this is const, and I'm casting it so I can change it,
// but only for a second then I put it back.
			*(char*)r3 = 0;
			recipient_slist = curl_slist_append(recipient_slist, r2 + 1);
			*(char*)r3 = '>';
		} else
			recipient_slist = curl_slist_append(recipient_slist, r1);
		if (recipient_slist == NULL)
			i_printfExit(MSG_NoMem);
	}

	return recipient_slist;
}

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
}

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

	if(debugLevel >= 6) {
		debugPrint(6, "outgoing message");
		debugPrint(6, "%s", message);
		puts("debug don't send");
	} else {
		res = curl_easy_perform(handle);
	}
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
}

/* Send mail to the smtp server. */
bool
sendMail(int account, const char **recipients, const char *body,
	 int subjat, const char **attachments, const char *refline,
	 int nalt, bool dosig)
{
	char *from, *fromiso, *reply;
	const struct MACCOUNT *a, *ao, *localMail;
	const char *s, *s2, *boundary;
	char reccc[MAXRECAT + MAXCC];
	char *t;
	int nat, nrec, cx, i, j;
	char *out = 0;
	bool sendmail_success = false;
	bool mustmime = false;
	bool firstrec, flowed;
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
	while (attachments[nat])
		++nat;

// attachments from the mail descriptor
	for (j=0; a->cclist[j]; ++j)
		if(a->cctype[j])
			attachments[nat++] = a->cclist[j];
	attachments[nat] = 0;

	if (nat)
		mustmime = true;

	if (nalt && nalt < nat) {
		setError(MSG_AttAlternate);
		return false;
	}

	if (!loadAddressBook())
		return false;

	nrec = 0;		/* number of recipients */
	while (recipients[nrec])
		++nrec;

// recipients from the mail descriptor
	for (j=0; a->cclist[j]; ++j)
		if(!a->cctype[j])
			recipients[nrec++] = a->cclist[j];
	recipients[nrec] = 0;

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
			const char *a = addressList[i].brief;
			if (*a == '-' || *a == '!')
				++a;
			if (!stringEqual(s, a))
				continue;
			t = addressList[i].email;
			if(addressList[i].fullname[0])
				t = addressList[i].fullname;
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
	}			// recipients

	if (!j) {
		setError(MSG_RecipNone);
		return false;
	}

/* verify attachments are readable */
	for (j = 0; (s = attachments[j]); ++j) {
		if (!ismc && (cx = stringIsNum(s)) >= 0) {
			if (!cxCompare(cx) || !cxActive(cx, true))
				return false;
			if (!sessionList[cx].lw->dol) {
				setError(MSG_AttSessionEmpty, cx);
				return false;
			}
		} else {
			char ftype = fileTypeByName(s, 0);
			if (!ftype) {
				setError(MSG_NoAccess, s);
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
	}			// loop over attachments

	if (!encodeAttachment(body, subjat, false, &ct, &ce, &encoded, &flowed))
		return false;
	if (ce[0] == 'q')
		mustmime = true;

	boundary = makeBoundary();

// Build the outgoing mail as one string.
	out = initString(&j);

	firstrec = true;
	for (i = 0; (s = recipients[i]); ++i) {
		if (reccc[i])
			continue;
		stringAndString(&out, &j, firstrec ? "To: " : ",\r\n  ");
		stringAndString(&out, &j, s);
		firstrec = false;
	}
	if (!firstrec)
		stringAndString(&out, &j, eol);

	firstrec = true;
	for (i = 0; (s = recipients[i]); ++i) {
		if (reccc[i] != '^')
			continue;
		stringAndString(&out, &j, firstrec ? "CC: " : ",\r\n  ");
		stringAndString(&out, &j, s);
		firstrec = false;
	}
	if (!firstrec)
		stringAndString(&out, &j, eol);

	firstrec = true;
	for (i = 0; (s = recipients[i]); ++i) {
		if (reccc[i] != '?')
			continue;
		stringAndString(&out, &j, firstrec ? "BCC: " : ",\r\n  ");
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
	sprintf(serverLine, "Reply-To: %s <%s>%s", fromiso, reply, eol);
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
		"Date: %s%sMessage-ID: <%s.%s>%sMIME-Version: 1.0%s",
		mailTimeString(), eol, messageTimeID(), reply, eol, eol);
	stringAndString(&out, &j, serverLine);

	if (!mustmime) {
/* no mime components required, we can just send the mail. */
		sprintf(serverLine,
			"Content-Type: %s%s%s%sContent-Transfer-Encoding: %s%s%s",
			ct, charsetString(ct, ce),
			(flowed ? "; format=flowed; delsp=no" : ""), eol,
			ce, eol, eol);
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
			"%sContent-Type: %s%s%s%sContent-Transfer-Encoding: %s%s%s",
			eol, ct, charsetString(ct, ce),
			(flowed ? "; format=flowed; delsp=no" : ""), eol,
			ce, eol, eol);
		stringAndString(&out, &j, serverLine);
	}

/* Now send the body, line by line. */
	appendAttachment(encoded, &out, &j, flowed, NULL);
	nzFree(encoded);
	encoded = 0;

	if (mustmime) {
		for (i = 0; (s = attachments[i]); ++i) {
			if (!encodeAttachment(s, 0, false, &ct, &ce, &encoded, 0))
				return false;
			sprintf(serverLine, "%s--%s%sContent-Type: %s%s", eol,
				boundary, eol, ct, charsetString(ct, ce));
			stringAndString(&out, &j, serverLine);
// If the filename has a quote in it, forget it.
// Also, suppress filename if this is an alternate presentation.
			s2 = 0;
			if (!nalt && !strchr(s, '"')
			    && (ismc || stringIsNum(s) < 0)) {
// for security reasons, and convenience, don't present the absolute path.
				s2 = strrchr(s, '/');
				if(s2) {
					if(!*++s2)
// attaching a directory?  This just shouldn't happen.
						s2 = 0;
				} else s2 = s;
			}
			if(s2) {
				sprintf(serverLine, "; name=\"%s\"", s2);
				stringAndString(&out, &j, serverLine);
			}
			if(!nalt) {
				sprintf(serverLine, "%sContent-Disposition: attachment", eol);
				stringAndString(&out, &j, serverLine);
				if(s2) {
					sprintf(serverLine, "; filename=\"%s\"", s2);
					stringAndString(&out, &j, serverLine);
				}
			}
			sprintf(serverLine,
				"%sContent-Transfer-Encoding: %s%s%s", eol, ce,
				eol, eol);
			stringAndString(&out, &j, serverLine);
			appendAttachment(encoded, &out, &j, false, ct);
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
}

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
}

bool sendMailCurrent(int sm_account, bool dosig)
{
	const char *reclist[MAXRECAT + MAXCC + 1];
	char *recmem;
	const char *atlist[MAXRECAT + MAXCC + 1];
	char *atmem;
	char *s, *t;
	char cxbuf[4];
	int lr, la, ln;
	char *refline = 0;
	int nrec, nat, nalt;
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
	if (cw->irciMode | cw->ircoMode) {
		setError(MSG_MailIrc);
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

	nrec = nat = nalt = 0;

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

		if (memEqualCI(line, "subject:", 8) || memEqualCI(line, "sub:", 4)) {
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
	if (rc && debugLevel >= 1)
		i_puts(MSG_OK);
	return rc;
}
