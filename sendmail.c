/* sendmail.c
 * Send mail using the smtp protocol.
 * Send the contents of a file, or the current edbrowse buffer.
 * Copyright (c) Karl Dahlke, 2006
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"

#include <time.h>

#define MAXRECAT 100		/* max number of recipients or attachments */

char serverLine[MAXTTYLINE];
static char spareLine[MAXTTYLINE];
int mssock;			/* mail server socket */
static bool doSignature;
static char subjectLine[200];

static struct ALIAS {
    char name[16];
    char email[64];
} *addressList;
static int nads;		/* number of addresses */
static time_t adbooktime;

/* read and/or refresh the address book */
bool
loadAddressBook(void)
{
    char *buf, *bufend, *v, *last, *s, *t;
    bool cmt = false;
    char state = 0, c;
    int j, buflen, ln = 1;
    time_t mtime;

    if(!addressFile ||
       (mtime = fileTimeByName(addressFile)) == -1 || mtime <= adbooktime)
	return true;

    debugPrint(3, "loading address book");
    nzFree(addressList);
    addressList = 0;
    nads = 0;
    if(!fileIntoMemory(addressFile, &buf, &buflen))
	return false;
    bufend = buf + buflen;

    for(s = t = last = buf; s < bufend; ++s) {
	c = *s;
	if(cmt) {
	    if(c != '\n')
		continue;
	    cmt = false;
	}
	if(c == ':') {		/* delimiter */
	    if(state == 0) {
		setError(MSG_ABNoAlias, ln);
	      freefail:
		nzFree(buf);
		return false;
	    }
	    while(t[-1] == ' ' || t[-1] == '\t')
		--t;
	    if(state == 1) {
		*t++ = c;
		state = 2;
		continue;
	    }
	    c = '#';		/* extra fields are ignored */
	}			/* : */
	if(c == '#') {
	    cmt = true;
	    continue;
	}
	if(c == '\n') {
	    ++ln;
	    if(state == 0)
		continue;
	    if(state == 1) {
		setError(MSG_ABNoColon, ln - 1);
		goto freefail;
	    }
	    if(state == 3) {
		++nads;
		while(isspaceByte(t[-1]))
		    --t;
		*t = 0;
		v = strchr(last, ':');
		if(v - last >= 16) {
		    setError(MSG_ABAliasLong, ln - 1);
		    goto freefail;
		}
		++v;
		if(t - v >= 64) {
		    setError(MSG_ABMailLong, ln - 1);
		    goto freefail;
		}
		if(!strchr(v, '@')) {
		    setError(MSG_ABNoAt, ln - 1);
		    goto freefail;
		}
		if(strpbrk(v, " \t")) {
		    setError(MSG_ABMailSpaces, ln - 1);
		    goto freefail;
		}

		while(last < t) {
		    if(!isprintByte(*last)) {
			setError(MSG_AbMailUnprintable, ln - 1);
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
	if((c == ' ' || c == '\t') && (state == 0 || state == 2))
	    continue;
	if(state == 0)
	    state = 1;
	if(state == 2)
	    state = 3;
	*t++ = c;
    }

    *t = 0;
    if(state) {
	setError(MSG_ABUnterminated);
	goto freefail;
    }

    if(nads) {
	addressList = allocMem(nads * sizeof (struct ALIAS));
	j = 0;
	for(s = buf; *s; s = t + 1, ++j) {
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

const char *
reverseAlias(const char *reply)
{
    int i;
    for(i = 0; i < nads; ++i)
	if(stringEqual(reply, addressList[i].email)) {
	    const char *a = addressList[i].name;
	    if(*a == '!')
		break;
	    return a;
	}
    return 0;			/* not found */
}				/* reverseAlias */


/*********************************************************************
Put and get lines from the mail server.
Print the lines if the debug level calls for it.
*********************************************************************/

bool
serverPutLine(const char *buf, bool secure)
{
    int n, len = strlen(buf);
    char c;
    if(debugLevel >= 4) {
	printf("> ");
	for(n = 0; n < len; ++n) {
	    c = buf[n];
	    if(c != '\r')
		putchar(c);
	}
    }
    if(secure)
	n = ssl_write(buf, len);
    else
	n = tcp_write(mssock, buf, len);
    if(n < len) {
	setError(MSG_MailWrite);
	return false;
    }
    return true;
}				/* serverPutLine */

bool
serverGetLine(bool secure)
{
    int n, len, slen;
    char c;
    char *s;
    slen = strlen(spareLine);
    strcpy(serverLine, spareLine);
    s = strchr(serverLine, '\n');
    if(!s) {
	if(secure)
	    len = ssl_read(serverLine + slen, sizeof (serverLine) - 1 - slen);
	else
	    len =
	       tcp_read(mssock, serverLine + slen,
	       sizeof (serverLine) - 1 - slen);
	if(len <= 0) {
	    setError(MSG_MailRead);
	    return false;
	}
	slen += len;
	serverLine[slen] = 0;
    }
    s = strchr(serverLine, '\n');
    if(!s) {
	setError(MSG_MailResponseLong);
	return false;
    }
    strcpy(spareLine, s + 1);
    *s = 0;
    if(s > serverLine && s[-1] == '\r')
	*--s = 0;
    debugPrint(4, "< %s", serverLine);
    return true;
}				/* serverGetLine */

static bool
serverPutGet(const char *line, bool secure)
{
    if(!serverPutLine(line, secure))
	return false;
    if(!serverGetLine(secure))
	return false;
}				/* serverPutGet */

void
serverClose(bool secure)
{
    serverPutLine("quit\r\n", secure);
    endhostent();
    sleep(2);
    if(secure)
	ssl_done();
    close(mssock);
}				/* serverClose */

/* Connect to the mail server */
bool
mailConnect(const char *host, int port, bool secure)
{
    IP32bit ip = tcp_name_ip(host);
    if(ip == NULL_IP) {
	setError((intFlag ? MSG_Interrupted : MSG_MailLocate), host);
	return false;
    }
    debugPrint(4, "%s -> %s", host, tcp_ip_dots(ip));
    mssock = tcp_connect(ip, port, mailTimeout);
    if(mssock < 0) {
	setError(intFlag ? MSG_Interrupted : MSG_MailConnect);
	return false;
    }
    debugPrint(4, "connected to port %d", port);
    spareLine[0] = 0;
    if(secure) {
	int n = ssl_newbind(mssock);
	if(n < 0) {
	    if(n == -999)
		setError(MSG_NoCertify, host);
	    else
		setError(MSG_WebConnectSecure, host, n);
	    return false;
	}
	debugPrint(4, "secure connection established");
    }
    return true;
}				/* mailConnect */

static char base64_chars[] =
   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
char *
base64Encode(const char *inbuf, int inlen, bool lines)
{
    char *out, *outstr;
    uchar *in = (uchar *) inbuf;
    int colno;
    int outlen = ((inlen / 3) + 1) * 4;
    ++outlen;			/* zero on the end */
    if(lines)
	outlen += (inlen / 54) + 1;
    outstr = out = allocMem(outlen);
    colno = 0;
    while(inlen >= 3) {
	*out++ = base64_chars[(int)(*in >> 2)];
	*out++ = base64_chars[(int)((*in << 4 | *(in + 1) >> 4) & 63)];
	*out++ = base64_chars[(int)((*(in + 1) << 2 | *(in + 2) >> 6) & 63)];
	*out++ = base64_chars[(int)(*(in + 2) & 63)];
	inlen -= 3;
	in += 3;
	if(!lines)
	    continue;
	colno += 4;
	if(colno < 72)
	    continue;
	*out++ = '\n';
	colno = 0;
    }
    if(inlen == 1) {
	*out++ = base64_chars[(int)(*in >> 2)];
	*out++ = base64_chars[(int)(*in << 4 & 63)];
	*out++ = '=';
	*out++ = '=';
	colno += 4;
    }
    if(inlen == 2) {
	*out++ = base64_chars[(int)(*in >> 2)];
	*out++ = base64_chars[(int)((*in << 4 | *(in + 1) >> 4) & 63)];
	*out++ = base64_chars[(int)((*(in + 1) << 2) & 63)];
	*out++ = '=';
	colno += 4;
    }
/* finish the last line */
    if(lines && colno)
	*out++ = '\n';
    *out = 0;
    return outstr;
}				/* base64Encode */

char *
qpEncode(const char *line)
{
    char *newbuf;
    int l;
    const char *s;
    char c;

    newbuf = initString(&l);
    for(s = line; (c = *s); ++s) {
	if(c < '\n' && c != '\t' || c == '=') {
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
static char *
isoEncode(char *start, char *end)
{
    int nacount = 0, count = 0, len;
    char *s, *t;
    char c, code;

    for(s = start; s < end; ++s) {
	c = *s;
	if(c == 0)
	    *s = ' ';
	if(isspaceByte(c))
	    *s = ' ';
    }

    for(s = start; s < end; ++s) {
	c = *s;
	++count;
	if(!isprintByte(c) && c != ' ')
	    ++nacount;
    }

    if(!nacount)
	return 0;

    if(nacount * 4 >= count && count > 8) {
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

/* Read a file into memory, mime encode it,
 * and return the type of encoding and the encoded data.
 * Last three parameters are result parameters.
 * If ismail is nonzero, the file is the mail, not an attachment.
 * In fact ismail indicates the line that holds the subject.
 * If ismail is negative, then -ismail indicates the subject line,
 * and the string file is not the filename, but rather, the mail to send. */
bool
encodeAttachment(const char *file, int ismail,
   const char **type_p, const char **enc_p, char **data_p)
{
    char *buf;
    char c;
    bool longline;
    char *s, *t, *v;
    char *ct, *ce;		/* content type, content encoding */
    int buflen, i, cx;
    int nacount, nullcount, nlcount;

    if(ismail < 0) {
	buf = cloneString(file);
	buflen = strlen(buf);
	ismail = -ismail;
	file = EMPTYSTRING;
    } else {

	if(!ismc && (cx = stringIsNum(file)) >= 0) {
	    static char newfilename[16];
	    if(!unfoldBuffer(cx, false, &buf, &buflen))
		return false;
	    if(!buflen) {
		setError(MSG_BufferXEmpty, cx);
		goto freefail;
	    }
	    sprintf(newfilename, "<buffer %d>", cx);
	    file = newfilename;
	    if(sessionList[cx].lw->fileName)
		file = sessionList[cx].lw->fileName;
	} else {
	    if(!fileIntoMemory(file, &buf, &buflen))
		return false;
	    if(!buflen) {
		setError(MSG_FileXEmpty, file);
		goto freefail;
	    }
	}
    }				/* ismail negative or normal */

    if(ismail) {
/* Put newline at the end.  Yes, the buffer is allocated
 * with space for newline and null. */
	if(buf[buflen - 1] != '\n')
	    buf[buflen++] = '\n';
/* check for subject: line */
	s = buf;
	i = ismail;
	while(--i) {
	    while(*s != '\n')
		++s;
	    ++s;
	}
	while(*s == ' ' || *s == '\t')
	    ++s;
	if(!memEqualCI(s, "subject:", 8)) {
	    setError(MSG_SubjectStart);
	    goto freefail;
	}
	s += 8;
	while(*s == ' ' || *s == '\t')
	    ++s;
	t = s;
	while(*s != '\n')
	    ++s;
	v = s;
	while(s > t && isspaceByte(s[-1]))
	    --s;
	if(s == t) {
	    setError(MSG_SubjectEmpty);
	    goto freefail;
	}
	if(s - t >= sizeof (subjectLine)) {
	    setError(MSG_SubjectLong, sizeof (subjectLine) - 1);
	    goto freefail;
	}
	memcpy(subjectLine, t, s - t);
	subjectLine[s - t] = 0;
	t = subjectLine + (s - t);
	s = isoEncode(subjectLine, t);
	if(s) {
	    if(strlen(s) >= sizeof (subjectLine)) {
		setError(MSG_SubjectLong, sizeof (subjectLine) - 1);
		goto freefail;
	    }
	    strcpy(subjectLine, s);
	    nzFree(s);
	}
	debugPrint(6, "subject = %s", subjectLine);
/* Blank lines after subject are optional, and ignored. */
	for(t = buf + buflen; v < t; ++v)
	    if(*v != '\r' && *v != '\n')
		break;
	buflen -= (v - buf);
	if(buflen)
	    memcpy(buf, v, buflen);
	buf[buflen] = 0;

	if(doSignature) {	/* Append .signature file. */
	    c = fileTypeByName(sigFile, false);
	    if(c != 0) {
		int fd, n;
		if(c != 'f') {
		    setError(MSG_SigRegular);
		    goto freefail;
		}
		n = fileSizeByName(sigFile);
		if(n > 0) {
		    buf = reallocMem(buf, buflen + n + 1);
		    fd = open(sigFile, O_RDONLY);
		    if(fd < 0) {
			setError(MSG_SigAccess);
			goto freefail;
		    }
		    read(fd, buf + buflen, n);
		    close(fd);
		    buflen += n;
		    buf[buflen] = 0;
		}
	    }
	}			/* .signature */
    }

    /* primary email message */
    /* Infer content type from the filename */
    ct = 0;
    s = strrchr(file, '.');
    if(s && s[1]) {
	++s;
	if(stringEqualCI(s, "ps"))
	    ct = "application/PostScript";
	if(stringEqualCI(s, "jpeg"))
	    ct = "image/jpeg";
	if(stringEqualCI(s, "gif"))
	    ct = "image/gif";
	if(stringEqualCI(s, "wav"))
	    ct = "audio/basic";
	if(stringEqualCI(s, "mpeg"))
	    ct = "video/mpeg";
	if(stringEqualCI(s, "rtf"))
	    ct = "text/richtext";
	if(stringEqualCI(s, "htm") ||
	   stringEqualCI(s, "html") ||
	   stringEqualCI(s, "shtm") ||
	   stringEqualCI(s, "shtml") || stringEqualCI(s, "asp"))
	    ct = "text/html";
    }

/* Count the nonascii characters */
    nacount = nullcount = nlcount = 0;
    longline = false;
    s = 0;
    for(i = 0; i < buflen; ++i) {
	c = buf[i];
	if(c == '\0')
	    ++nullcount;
	if(c < 0)
	    ++nacount;
	if(c != '\n')
	    continue;
	++nlcount;
	t = buf + i;
	if(s && t - s > 120)
	    longline = true;
	s = t;
    }
    debugPrint(6, "attaching %s length %d nonascii %d nulls %d longline %d",
       file, buflen, nacount, nullcount, longline);
    nacount += nullcount;

    if(buflen > 20 && nacount * 5 > buflen) {	/* binary file */
	if(ismail) {
	    setError(MSG_MailBinary, file);
	    goto freefail;
	}

	s = base64Encode(buf, buflen, true);
	nzFree(buf);
	buf = s;
	if(!ct)
	    ct = "application/octet-stream";	/* default type */
	ce = "base64";
	goto success;
    }
    /* binary file */
    if(!ct)
	ct = "text/plain";

/* Switch to unix newlines - we'll switch back to dos later. */
    v = buf + buflen;
    for(s = t = buf; s < v; ++s) {
	c = *s;
	if(c == '\r' && s < v - 1 && s[1] == '\n')
	    continue;
	*t++ = c;
    }
    buflen = t - buf;

/* Do we need to use quoted-printable? */
/* Perhaps this hshould read (nacount > 0) */
    if(nacount * 20 > buflen || nullcount || longline) {
	char *newbuf;
	int l, colno = 0, space = 0;

	newbuf = initString(&l);
	v = buf + buflen;
	for(s = buf; s < v; ++s) {
	    c = *s;
/* do we have to =expand this character? */
	    if(c < '\n' && c != '\t' ||
	       c == '=' ||
	       c == '\xff' ||
	       (c == ' ' || c == '\t') && s < v - 1 && s[1] == '\n') {
		char expand[4];
		sprintf(expand, "=%02X", (uchar) c);
		stringAndString(&newbuf, &l, expand);
		colno += 3;
	    } else {
		stringAndChar(&newbuf, &l, c);
		++colno;
	    }
	    if(c == '\n') {
		colno = space = 0;
		continue;
	    }
	    if(c == ' ' || c == '\t')
		space = l;
	    if(colno < 72)
		continue;
	    if(s == v - 1)
		continue;
/* If newline's coming up anyways, don't force another one. */
	    if(s[1] == '\n')
		continue;
	    i = l;
	    if(!space || space == i) {
		stringAndString(&newbuf, &l, "=\n");
		colno = space = 0;
		continue;
	    }
	    colno = i - space;
	    stringAndString(&newbuf, &l, "**");	/* make room */
	    while(i > space) {
		newbuf[i + 1] = newbuf[i - 1];
		--i;
	    }
	    newbuf[space] = '=';
	    newbuf[space + 1] = '\n';
	    space = 0;
	}			/* loop over characters */

	nzFree(buf);
	buf = newbuf;
	ce = "quoted-printable";
	goto success;
    }
    /* quoted printable */
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

static char *
mailTimeString(void)
{
    static char buf[48];
    static const char wday[] = "Sun\0Mon\0Tue\0Wed\0Thu\0Fri\0Sat";
    static const char month[] =
       "Jan\0Feb\0Mar\0Apr\0May\0Jun\0Jul\0Aug\0Sep\0Oct\0Nov\0Dec";
    struct tm *cur_tm;
    long now;
    time(&now);
    cur_tm = localtime(&now);
    sprintf(buf, "%s, %02d %s %d %02d:%02d:%02d",
       wday + cur_tm->tm_wday * 4,
       cur_tm->tm_mday,
       month + cur_tm->tm_mon * 4,
       cur_tm->tm_year + 1900, cur_tm->tm_hour, cur_tm->tm_min, cur_tm->tm_sec);
    return buf;
}				/* mailTimeString */

static char *
messageTimeID(void)
{
    static char buf[48];
    struct tm *cur_tm;
    long now;
    time(&now);
    cur_tm = localtime(&now);
    sprintf(buf, "%04d%02d%02d%02d%02d%02d",
       cur_tm->tm_year + 1900, cur_tm->tm_mon, cur_tm->tm_mday,
       cur_tm->tm_hour, cur_tm->tm_min, cur_tm->tm_sec);
    return buf;
}				/* messageTimeID */

static void
appendAttachment(const char *s, char **out, int *l)
{
    const char *t;
    int n;
    while(*s) {			/* another line */
	t = strchr(s, '\n');
	if(!t)
	    t = s + strlen(s);
	n = t - s;
	if(t[-1] == '\r')
	    --n;
	if(n)
	    memcpy(serverLine, s, n);
	serverLine[n] = 0;
	if(n == 1 && serverLine[0] == '.')	/* can't allow this */
	    strcpy(serverLine, " .");
	strcat(serverLine, eol);
	stringAndString(out, l, serverLine);
	if(*t)
	    ++t;
	s = t;
    }
/* Small bug here - an attachment that is not base64 encoded,
 * and had no newline at the end, now has one. */
}				/* appendAttachment */

char *
makeBoundary(void)
{
    static char boundary[60];
    sprintf(boundary, "nextpart-eb-%06d", rand() % 1000000);
    return boundary;
}				/* makeBoundary */

/* Send mail to the smtp server. */
bool
sendMail(int account, const char **recipients, const char *body,
   int subjat, const char **attachments, int nalt, bool dosig)
{
    char *from, *fromiso, *reply, *login, *smlogin, *pass;
    const struct MACCOUNT *a, *ao, *localMail;
    const char *s, *boundary;
    char reccc[MAXRECAT];
    char *t;
    int nat, cx, i, j;
    char *out = 0;
    bool mustmime = false;
    bool firstrec;
    const char *ct, *ce;
    char *encoded = 0;

    if(!validAccount(account))
	return false;
    localMail = accounts + localAccount - 1;

    a = accounts + account - 1;
    from = a->from;
    reply = a->reply;
    ao = a->outssl ? a : localMail;
    login = ao->login;
    smlogin = strchr(login, '\\');
    if(smlogin)
	++smlogin;
    else
	smlogin = login;
    pass = ao->password;
    doSignature = dosig;

    nat = 0;			/* number of attachments */
    if(attachments) {
	while(attachments[nat])
	    ++nat;
    }
    if(nat)
	mustmime = true;
    if(nalt && nalt < nat) {
	setError(MSG_AttAlternate);
	return false;
    }

    if(!loadAddressBook())
	return false;

/* set copy flags */
    for(j = 0; s = recipients[j]; ++j) {
	char cc = 0;
	if(*s == '^' || *s == '?')
	    cc = *s++;
	if(j == MAXRECAT) {
	    setError(MSG_RecipMany, MAXRECAT);
	    return false;
	}
	recipients[j] = s;
	reccc[j] = cc;
    }

/* Look up aliases in the address book */
    for(j = 0; s = recipients[j]; ++j) {
	if(strchr(s, '@'))
	    continue;
	t = 0;
	for(i = 0; i < nads; ++i) {
	    const char *a = addressList[i].name;
	    if(*a == '-' || *a == '!')
		++a;
	    if(!stringEqual(s, a))
		continue;
	    t = addressList[i].email;
	    debugPrint(3, " %s becomes %s", s, t);
	    break;
	}
	if(t) {
	    recipients[j] = t;
	    continue;
	}
	if(!addressFile) {
	    setError(MSG_ABMissing);
	    return false;
	}
	setError(MSG_ABNoAlias2, s);
	return false;
    }				/* recipients */

    if(!j) {
	setError(MSG_RecipNone);
	return false;
    }

/* verify attachments are readable */
    for(j = 0; s = attachments[j]; ++j) {
	if(!ismc && (cx = stringIsNum(s)) >= 0) {
	    if(!cxCompare(cx) || !cxActive(cx))
		return false;
	    if(!sessionList[cx].lw->dol) {
		setError(MSG_AttSessionEmpty, cx);
		return false;
	    }
	} else {
	    char ftype = fileTypeByName(s, false);
	    if(!ftype) {
		setError(MSG_AttAccess, s);
		return false;
	    }
	    if(ftype != 'f') {
		setError(MSG_AttRegular, s);
		return false;
	    }
	    if(!fileSizeByName(s)) {
		setError(MSG_AttEmpty2, s);
		return false;
	    }
	}
    }				/* loop over attachments */

    if(!encodeAttachment(body, subjat, &ct, &ce, &encoded))
	return false;
    if(ce[0] == 'q')
	mustmime = true;

    boundary = makeBoundary();

    if(!mailConnect(ao->outurl, ao->outport, ao->outssl)) {
	nzFree(encoded);
	return false;
    }
    if(!serverGetLine(ao->outssl))
	goto mailfail;
    while(memEqualCI(serverLine, "220-", 4)) {
	if(!serverGetLine(ao->outssl))
	    goto mailfail;
    }
    if(!memEqualCI(serverLine, "220 ", 4)) {
	setError(MSG_MailBadPrompt, serverLine);
	goto mailfail;
    }

    sprintf(serverLine, "%s %s%s", (a->outssl ? "ehlo" : "Helo"), smlogin, eol);
    if(!serverPutLine(serverLine, ao->outssl))
	goto mailfail;
  get250:
    if(!serverGetLine(ao->outssl))
	goto mailfail;
    if(!memEqualCI(serverLine, "250", 3) ||
       serverLine[3] != ' ' && serverLine[3] != '-') {
	setError(MSG_MailWhat, login);
	goto mailfail;
    }
    if(serverLine[3] == '-')
	goto get250;

    if(ao->outssl) {
	char *b;
/* login authentication is the only thing I support right now. */
	if(!serverPutGet("auth login\r\n", true))
	    goto mailfail;
	if(!memEqualCI(serverLine, "334 ", 4)) {
	    setError(MSG_AuthLoginOnly);
	    goto mailfail;
	}
	b = base64Encode(login, strlen(login), false);
	sprintf(serverLine, "%s%s", b, eol);
	nzFree(b);
	if(!serverPutGet(serverLine, true))
	    goto mailfail;
	if(!memEqualCI(serverLine, "334 ", 4)) {
	    setError(MSG_AuthLoginOnly);
	    goto mailfail;
	}
	b = base64Encode(pass, strlen(pass), false);
	sprintf(serverLine, "%s%s", b, eol);
	nzFree(b);
	if(!serverPutGet(serverLine, true))
	    goto mailfail;
	if(!memEqualCI(serverLine, "235 ", 4)) {
	    setError(MSG_SmtpNotComplete, serverLine);
	    goto mailfail;
	}
    }

    sprintf(serverLine, "mail from: <%s>%s", reply, eol);
    if(!serverPutGet(serverLine, ao->outssl))
	goto mailfail;
    if(!memEqualCI(serverLine, "250 ", 4)) {
	setError(MSG_MailReject, reply);
	goto mailfail;
    }

    for(j = 0; s = recipients[j]; ++j) {
	sprintf(serverLine, "rcpt to: <%s>%s", s, eol);
	if(!serverPutGet(serverLine, ao->outssl))
	    goto mailfail;
	if(!memEqualCI(serverLine, "250 ", 4)) {
	    setError(MSG_MailReject, s);
	    goto mailfail;
	}
    }

    if(!serverPutGet("data\r\n", ao->outssl))
	goto mailfail;
    if(!memEqualCI(serverLine, "354 ", 4)) {
	setError(MSG_MailNotReady, serverLine);
	goto mailfail;
    }

/* Build the outgoing mail, and send it in one go, as one string. */
    out = initString(&j);

    firstrec = true;
    for(i = 0; s = recipients[i]; ++i) {
	if(reccc[i])
	    continue;
	stringAndString(&out, &j, firstrec ? "To:" : ",\r\n  ");
	stringAndString(&out, &j, s);
	firstrec = false;
    }
    if(!firstrec)
	stringAndString(&out, &j, eol);

    firstrec = true;
    for(i = 0; s = recipients[i]; ++i) {
	if(reccc[i] != '^')
	    continue;
	stringAndString(&out, &j, firstrec ? "CC:" : ",\r\n  ");
	stringAndString(&out, &j, s);
	firstrec = false;
    }
    if(!firstrec)
	stringAndString(&out, &j, eol);

    firstrec = true;
    for(i = 0; s = recipients[i]; ++i) {
	if(reccc[i] != '?')
	    continue;
	stringAndString(&out, &j, firstrec ? "BCC:" : ",\r\n  ");
	stringAndString(&out, &j, s);
	firstrec = false;
    }
    if(!firstrec)
	stringAndString(&out, &j, eol);

    fromiso = isoEncode(from, from + strlen(from));
    if(!fromiso)
	fromiso = from;
    sprintf(serverLine, "From: %s <%s>%s", fromiso, reply, eol);
    stringAndString(&out, &j, serverLine);
    sprintf(serverLine, "Reply-to: %s <%s>%s", fromiso, reply, eol);
    stringAndString(&out, &j, serverLine);
    if(fromiso != from)
	nzFree(fromiso);
    sprintf(serverLine, "Subject: %s%s", subjectLine, eol);
    stringAndString(&out, &j, serverLine);
    sprintf(serverLine, "Date: %s%sMessage-ID: <%s.%s>%sMime-Version: 1.0%s",
       mailTimeString(), eol, messageTimeID(), reply, eol, eol);
    stringAndString(&out, &j, serverLine);

    if(!mustmime) {
/* no mime components required, we can just send the mail. */
	sprintf(serverLine,
	   "Content-type: %s%sContent-Transfer-Encoding: %s%s%s", ct, eol, ce,
	   eol, eol);
	stringAndString(&out, &j, serverLine);
    } else {
	sprintf(serverLine,
	   "Content-Type: multipart/%s; boundary=%s%sContent-Transfer-Encoding: 7bit%s%s",
	   nalt ? "alternative" : "mixed", boundary, eol, eol, eol);
	stringAndString(&out, &j, serverLine);
	stringAndString(&out, &j,
	   "This message is in MIME format. Since your mail reader does not understand\r\n\
this format, some or all of this message may not be legible.\r\n\r\n--");
	stringAndString(&out, &j, boundary);
	sprintf(serverLine,
	   "%sContent-type: %s%sContent-Transfer-Encoding: %s%s%s", eol, ct,
	   eol, ce, eol, eol);
	stringAndString(&out, &j, serverLine);
    }

/* Now send the body, line by line. */
    appendAttachment(encoded, &out, &j);
    nzFree(encoded);
    encoded = 0;

    if(mustmime) {
	for(i = 0; s = attachments[i]; ++i) {
	    if(!encodeAttachment(s, 0, &ct, &ce, &encoded))
		return false;
	    sprintf(serverLine, "%s--%s%sContent-Type: %s", eol, boundary, eol,
	       ct);
	    stringAndString(&out, &j, serverLine);
/* If the filename has a quote in it, forget it. */
/* Also, suppress filename if this is an alternate presentation. */
	    if(!nalt && !strchr(s, '"') && (ismc || stringIsNum(s) < 0)) {
		sprintf(serverLine, "; name=\"%s\"", s);
		stringAndString(&out, &j, serverLine);
	    }
	    sprintf(serverLine, "%sContent-Transfer-Encoding: %s%s%s", eol, ce,
	       eol, eol);
	    stringAndString(&out, &j, serverLine);
	    appendAttachment(encoded, &out, &j);
	    nzFree(encoded);
	    encoded = 0;
	}			/* loop over attachments */

/* The last boundary */
	sprintf(serverLine, "%s--%s--%s", eol, boundary, eol);
	stringAndString(&out, &j, serverLine);
    }

    /* mime format */
    /* A dot alone ends the transmission */
    stringAndString(&out, &j, ".\r\n");
    if(!serverPutLine(out, ao->outssl))
	goto mailfail;
    nzFree(out);
    out = 0;

    if(!serverGetLine(ao->outssl))
	goto mailfail;
    if(!memEqualCI(serverLine, "250 ", 4) &&
/* do these next two lines make any sense? */
       !strstrCI(serverLine, "message accepted") &&
       !strstrCI(serverLine, "message received")) {
	setError(MSG_MailNotSent, serverLine);
	goto mailfail;
    }

    serverClose(ao->outssl);
    return true;

  mailfail:
    nzFree(encoded);
    nzFree(out);
    close(mssock);
    return false;
}				/* sendMail */

bool
validAccount(int n)
{
    if(!maxAccount) {
	setError(MSG_MailAccountsNone);
	return false;
    }
    if(n <= 0 || n > maxAccount) {
	setError(MSG_MailAccountBad, n, maxAccount);
	return false;
    }
    return true;
}				/* validAccount */

bool
sendMailCurrent(int sm_account, bool dosig)
{
    const char *reclist[MAXRECAT + 1];
    char *recmem;
    const char *atlist[MAXRECAT + 1];
    char *atmem;
    char *s, *t;
    char cxbuf[4];
    int lr, la, ln;
    int nrec = 0, nat = 0, nalt = 0;
    int account = localAccount;
    int j;
    bool rc = false;
    bool subj = false;

    if(cw->browseMode) {
	setError(MSG_MailBrowse);
	return false;
    }
    if(cw->sqlMode) {
	setError(MSG_MailDB);
	return false;
    }
    if(cw->dirMode) {
	setError(MSG_MailDir);
	return false;
    }
    if(cw->binMode) {
	setError(MSG_MailBinary2);
	return false;
    }
    if(!cw->dol) {
	setError(MSG_MailEmpty);
	return false;
    }

    if(!validAccount(account))
	return false;

    recmem = initString(&lr);
    atmem = initString(&la);

/* Gather recipients and attachments, until we reach subject: */
    for(ln = 1; ln <= cw->dol; ++ln) {
	char *line = (char *)fetchLine(ln, -1);
	if(memEqualCI(line, "to:", 3) ||
	   memEqualCI(line, "mailto:", 7) ||
	   memEqualCI(line, "cc:", 3) ||
	   memEqualCI(line, "bcc:", 4) ||
	   memEqualCI(line, "reply to:", 9) ||
	   memEqualCI(line, "reply to ", 9)) {
	    char cc = 0;
	    if(toupper(line[0]) == 'C')
		cc = '^';
	    if(toupper(line[0]) == 'B')
		cc = '?';
	    if(toupper(line[0]) == 'R')
		line += 9;
	    else
		line = strchr(line, ':') + 1;
	    while(*line == ' ' || *line == '\t')
		++line;
	    if(*line == '\n') {
		setError(MSG_RecipNone2, ln);
		goto done;
	    }
	    if(nrec == MAXRECAT) {
		setError(MSG_RecipMany, MAXRECAT);
		goto done;
	    }
	    ++nrec;
	    for(t = line; *t != '\n'; ++t) ;
	    if(cc) {
		if(!lr) {
		    setError(MSG_MailFirstCC);
		    goto done;
		}
		stringAndChar(&recmem, &lr, cc);
	    }
	    stringAndBytes(&recmem, &lr, line, t + 1 - line);
	    continue;
	}
	if(memEqualCI(line, "attach:", 7) || memEqualCI(line, "alt:", 4)) {
	    if(toupper(line[1]) == 'T')
		line += 7;
	    else
		line += 4, ++nalt;
	    while(*line == ' ' || *line == '\t')
		++line;
	    if(*line == '\n') {
		setError(MSG_AttLineX, ln);
		goto done;
	    }
	    if(nat == MAXRECAT) {
		setError(MSG_RecipMany, MAXRECAT);
		goto done;
	    }
	    ++nat;
	    for(t = line; *t != '\n'; ++t) ;
	    stringAndBytes(&atmem, &la, line, t + 1 - line);
	    continue;
	}
	if(memEqualCI(line, "account:", 8)) {
	    line += 8;
	    while(*line == ' ' || *line == '\t')
		++line;
	    if(!isdigitByte(*line) ||
	       (account = strtol(line, &line, 10)) == 0 ||
	       account > maxAccount || *line != '\n') {
		setError(MSG_MailAccountBadLineX, ln);
		goto done;
	    }
	    continue;
	}
	if(memEqualCI(line, "subject:", 8)) {
	    while(*line == ' ' || *line == '\t')
		++line;
	    if(*line == '\n') {
		setError(MSG_SubjectEmpty2);
		goto done;
	    }
	    subj = true;
	}
	break;
    }				/* loop over lines */

    if(sm_account)
	account = sm_account;
    if(!subj) {
	setError(((ln > cw->dol) + MSG_MailFirstLine), ln);
	goto done;
    }

    if(nrec == 0) {
	setError(MSG_RecipNone3);
	goto done;
    }

    for(s = recmem, j = 0; *s; s = t + 1, ++j) {
	t = strchr(s, '\n');
	*t = 0;
	reclist[j] = s;
    }
    reclist[j] = 0;
    for(s = atmem, j = 0; *s; s = t + 1, ++j) {
	t = strchr(s, '\n');
	*t = 0;
	atlist[j] = s;
    }
    atlist[j] = 0;

    sprintf(cxbuf, "%d", context);
    rc = sendMail(account, reclist, cxbuf, ln, atlist, nalt, dosig);

  done:
    nzFree(recmem);
    nzFree(atmem);
    if(!rc && intFlag)
	setError(MSG_Interrupted);
    if(rc)
	i_puts(MSG_OK);
    return rc;
}				/* sendMailCurrent */
