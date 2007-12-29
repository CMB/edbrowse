/* http.c
 * HTTP protocol client implementation
 * (c) 2002 Mikulas Patocka
 * This file is part of the Links project, released under GPL.
 *
 * Modified by Karl Dahlke for integration with edbrowse,
 * which is also released under the GPL.
 * 
 * OpenSSL exception:
 * As a special exception, I hereby grant permission to link
 * the code of this program with the OpenSSL library
 * (or with modified versions of OpenSSL that use the same license as OpenSSL),
 * and distribute linked combinations including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.
 * If you modify this file, you may extend this exception to your version of the
 * file, but you are not obligated to do so.
 * If you do not wish to do so, delete this exception statement from your version.
 */

#include "eb.h"

/* You need the open ssl library for secure connections. */
/* Hence the openSSL exception above. */
#include <openssl/ssl.h>
#include <openssl/err.h>	/* for error-retrieval */
#include <openssl/rand.h>

static SSL_CTX *sslcx;		/* the overall ssl context */
char *serverData;
int serverDataLen;

/* Called from main() */
void
ssl_init(bool doConfig)
{
/* I don't understand any of this. */
    char f_randfile[ABSPATH];
    if(RAND_egd(RAND_file_name(f_randfile, sizeof (f_randfile))) < 0) {
	/* Not an EGD, so read and write to it */
	if(RAND_load_file(f_randfile, -1))
	    RAND_write_file(f_randfile);
    }
    SSLeay_add_ssl_algorithms();
    sslcx = SSL_CTX_new(SSLv23_client_method());
    SSL_CTX_set_options(sslcx, SSL_OP_ALL);

    /*
       Load certificates from the file whose pathname is
       sslCerts.
       The third argument to this function is the name of a directory in which
       certificates are stored, one per file.
       Both the filename and the directory name may be NULL.
       Right now, we only support one large file containing all the
       certificates.  This could be changed easily.
     */

    SSL_CTX_load_verify_locations(sslcx, sslCerts, NULL);
    if(!sslCerts) {
	verifyCertificates = false;
	if(doConfig)
	    if(debugLevel >= 1)
		i_puts(MSG_NoCertFile);
    }

    SSL_CTX_set_default_verify_paths(sslcx);
    SSL_CTX_set_mode(sslcx, SSL_MODE_AUTO_RETRY);
    ssl_must_verify(verifyCertificates);
}				/* ssl_init */

void
ssl_must_verify(bool verify_flag)
{
    if(verify_flag == true)
	SSL_CTX_set_verify(sslcx, SSL_VERIFY_PEER, NULL);
    else
	SSL_CTX_set_verify(sslcx, SSL_VERIFY_NONE, NULL);
}				/* ssl_must_verify */

/* This is similar to our tcp_readFully in tcp.c */
static int
ssl_readFully(SSL * ssl, char *buf, int len)
{
    int pos = 0;
    int n;
    while(len) {
	n = SSL_read(ssl, buf + pos, len);
	if(n < 0)
	    return n;
	if(!n)
	    return pos;
	len -= n, pos += n;
    }
    return pos;			/* filled the whole buffer */
}				/* ssl_readFully */

/* Read from a socket, 100K at a time. */
#define CHUNKSIZE 100000
static bool
readSocket(SSL * ssl, int fh)
{
    struct CHUNK {
	struct CHUNK *next;
	char data[CHUNKSIZE];
    };
    struct CHUNK *chunklist = 0, *lastchunk = 0, *p, *q;
    int n, len = 0;
    char *data;
    bool isprintByte = false;

    while(true) {
	p = allocMem(sizeof (struct CHUNK));
	if(ssl)
	    n = ssl_readFully(ssl, p->data, CHUNKSIZE);
	else
	    n = tcp_readFully(fh, p->data, CHUNKSIZE);
	if(n < 0) {
	    setError(intFlag ? MSG_Interrupted : MSG_WebRead);
	    free(p);
	    for(p = chunklist; p; p = q) {
		q = p->next;
		free(p);
	    }
	    return false;
	}			/* error */
	len += n;
	if(n) {
	    if(lastchunk)
		lastchunk->next = p;
	    else
		chunklist = p;
	    lastchunk = p;
	} else
	    free(p);
	if(n < CHUNKSIZE)
	    break;
	printf(".");
	fflush(stdout);
	isprintByte = true;
    }				/* loop reading data */
    if(isprintByte)
	nl();

/* Put it all together */
    serverData = data = allocMem(len + 1);
    serverDataLen = len;
    debugPrint(4, "%d bytes read from the socket", len);
    p = chunklist;
    while(len) {
	int piece = (len < CHUNKSIZE ? len : CHUNKSIZE);
	memcpy(data, p->data, piece);
	data += piece;
	len -= piece;
	q = p->next;
	free(p);
	p = q;
    }

    return true;
}				/* readSocket */

/* Pull a keyword: attribute out of an internet header. */
char *
extractHeaderItem(const char *head, const char *end,
   const char *item, const char **ptr)
{
    int ilen = strlen(item);
    const char *f, *g;
    char *h = 0;
    for(f = head; f < end - ilen - 1; f++) {
	if(*f != '\n')
	    continue;
	if(!memEqualCI(f + 1, item, ilen))
	    continue;
	f += ilen;
	if(f[1] != ':')
	    continue;
	f += 2;
	while(*f == ' ')
	    ++f;
	for(g = f; g < end && *g >= ' '; g++) ;
	while(g > f && g[-1] == ' ')
	    --g;
	h = pullString1(f, g);
	if(ptr)
	    *ptr = f;
	break;
    }
    return h;
}				/* extractHeaderItem */

char *
extractHeaderParam(const char *str, const char *item)
{
    int le = strlen(item), lp;
    const char *s = str;
/* ; denotes the next param */
/* Even the first param has to be preceeded by ; */
    while(s = strchr(s, ';')) {
	while(*s && (*s == ';' || (uchar) * s <= ' '))
	    s++;
	if(!memEqualCI(s, item, le))
	    continue;
	s += le;
	while(*s && ((uchar) * s <= ' ' || *s == '='))
	    s++;
	if(!*s)
	    return EMPTYSTRING;
	lp = 0;
	while((uchar) s[lp] >= ' ' && s[lp] != ';')
	    lp++;
	return pullString(s, lp);
    }
    return NULL;
}				/* extractHeaderParam */

/* Date format is:    Mon, 03 Jan 2000 21:29:33 GMT */
			/* Or perhaps:     Sun Nov  6 08:49:37 1994 */
time_t
parseHeaderDate(const char *date)
{
    static const char *const months[12] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    time_t t = 0;
    int y;			/* remember the type of format */
    struct tm tm;
    memset(&tm, 0, sizeof (struct tm));

/* skip past day of the week */
    date = strchr(date, ' ');
    if(!date)
	goto fail;
    date++;

    if(isdigitByte(*date)) {	/* first format */
	y = 0;
	if(isdigitByte(date[1])) {
	    tm.tm_mday = (date[0] - '0') * 10 + date[1] - '0';
	    date += 2;
	} else {
	    tm.tm_mday = *date - '0';
	    ++date;
	}
	if(*date != ' ' && *date != '-')
	    goto fail;
	++date;
	for(tm.tm_mon = 0; tm.tm_mon < 12; tm.tm_mon++)
	    if(memEqualCI(date, months[tm.tm_mon], 3))
		goto f1;
	goto fail;
      f1:
	date += 3;
	if(*date == ' ') {
	    date++;
	    if(!isdigitByte(date[0]))
		goto fail;
	    if(!isdigitByte(date[1]))
		goto fail;
	    if(!isdigitByte(date[2]))
		goto fail;
	    if(!isdigitByte(date[3]))
		goto fail;
	    tm.tm_year =
	       (date[0] - '0') * 1000 + (date[1] - '0') * 100 + (date[2] -
	       '0') * 10 + date[3] - '0' - 1900;
	    date += 4;
	} else if(*date == '-') {
	    /* Sunday, 06-Nov-94 08:49:37 GMT */
	    date++;
	    if(!isdigitByte(date[0]))
		goto fail;
	    if(!isdigitByte(date[1]))
		goto fail;
	    if(!isdigitByte(date[2])) {
		tm.tm_year =
		   (date[0] >=
		   '7' ? 1900 : 2000) + (date[0] - '0') * 10 + date[1] - '0' -
		   1900;
		date += 2;
	    } else {
		tm.tm_year = atoi(date) - 1900;
		date += 4;
	    }
	} else
	    goto fail;
	if(*date != ' ')
	    goto fail;
	date++;
    } else {
/* second format */
	y = 1;
	for(tm.tm_mon = 0; tm.tm_mon < 12; tm.tm_mon++)
	    if(memEqualCI(date, months[tm.tm_mon], 3))
		goto f2;
	goto fail;
      f2:
	date += 3;
	while(*date == ' ')
	    date++;
	if(!isdigitByte(date[0]))
	    goto fail;
	tm.tm_mday = date[0] - '0';
	date++;
	if(*date != ' ') {
	    if(!isdigitByte(date[0]))
		goto fail;
	    tm.tm_mday = tm.tm_mday * 10 + date[0] - '0';
	    date++;
	}
	if(*date != ' ')
	    goto fail;
	date++;
    }

/* ready to crack time */
    if(!isdigitByte(date[0]))
	goto fail;
    if(!isdigitByte(date[1]))
	goto fail;
    tm.tm_hour = (date[0] - '0') * 10 + date[1] - '0';
    date += 2;
    if(*date != ':')
	goto fail;
    date++;
    if(!isdigitByte(date[0]))
	goto fail;
    if(!isdigitByte(date[1]))
	goto fail;
    tm.tm_min = (date[0] - '0') * 10 + date[1] - '0';
    date += 2;
    if(*date != ':')
	goto fail;
    date++;
    if(!isdigitByte(date[0]))
	goto fail;
    if(!isdigitByte(date[1]))
	goto fail;
    tm.tm_sec = (date[0] - '0') * 10 + date[1] - '0';
    date += 2;

    if(y) {
/* year is at the end */
	if(*date != ' ')
	    goto fail;
	date++;
	if(!isdigitByte(date[0]))
	    goto fail;
	if(!isdigitByte(date[1]))
	    goto fail;
	if(!isdigitByte(date[2]))
	    goto fail;
	if(!isdigitByte(date[3]))
	    goto fail;
	tm.tm_year =
	   (date[0] - '0') * 1000 + (date[1] - '0') * 100 + (date[2] -
	   '0') * 10 + date[3] - '0' - 1900;
	date += 4;
    }

    if(*date != ' ' && *date)
	goto fail;

    t = mktime(&tm);
    if(t != (time_t) - 1)
	return t;

  fail:
    return 0;
}				/* parseHeaderDate */

bool
parseRefresh(char *ref, int *delay_p)
{
    int delay = 0;
    char *u = ref;
    if(isdigitByte(*u))
	delay = atoi(u);
    while(isdigitByte(*u) || *u == '.')
	++u;
    if(*u == ';')
	++u;
    while(*u == ' ')
	++u;
    if(memEqualCI(u, "url=", 4)) {
	char qc;
	u += 4;
	qc = *u;
	if(qc == '"' || qc == '\'')
	    ++u;
	else
	    qc = 0;
	strcpy(ref, u);
	u = ref + strlen(ref);
	if(u > ref && u[-1] == qc)
	    u[-1] = 0;
	if(delay)
	    debugPrint(2, "delay %d", delay);
	*delay_p = delay;
	return true;
    }
    i_printf(MSG_GarbledRefresh, ref);
    *delay_p = 0;
    return false;
}				/* parseRefresh */

/* Return true if we waited for the duration, false if interrupted.
 * I don't know how to do this in Windows. */
bool
refreshDelay(int sec, const char *u)
{
/* the value 15 seconds is somewhat arbitrary */
    if(sec < 15)
	return true;
    i_printf(MSG_RedirectDelayed, u, sec);
    return false;
}				/* refreshDelay */

int hcode;			/* example, 404 */
char herror[32];		/* example, file not found */

bool
httpConnect(const char *from, const char *url)
{
    int port;			/* usually 80 */
    const char *portloc;
    IP32bit hip;		/* host IP */
    const char *host, *post, *s;
    char *hdr;			/* http header */
    char *u;
    int l, n, err;
    bool isprox, rc, secure, newurl;
    int hsock;			/* http socket */
    SSL *hssl;			/* for secure connections */
    int hct;			/* content type */
    int hce;			/* content encoding */
    char *ref, *loc;		/* refresh= location= */
    int delay;			/* before we refresh the page */
    int recount = 0;		/* count redirections */
    char user[MAXUSERPASS], pass[MAXUSERPASS];
    struct MIMETYPE *mt;
    const char *prot;
    char *cmd;
    char suffix[12];

    if(!isURL(url)) {
	setError(MSG_BadURL, url);
	return false;
    }

    n = fetchHistory(from, url);
    if(n < 0)
	return false;		/* infinite loop */
    if(n == false) {
      already:
	serverData = EMPTYSTRING;
	serverDataLen = 0;
	return true;		/* success, because it's already fetched */
    }
    newurl = true;

  restart:
    hct = CT_HTML;		/* the default */
    hce = ENC_PLAIN;
    isprox = isProxyURL(url);
    hssl = 0;
    prot = getProtURL(url);
    secure = stringEqualCI(prot, "https");
    host = getHostURL(url);
    if(!host)
	i_printfExit(MSG_EmptyHost);
    if(proxy_host) {
	if(secure) {
	    setError(MSG_SSLProxyNYI);
	    return false;
	}
	hip = tcp_name_ip(proxy_host);
    } else {
	hip = tcp_name_ip(host);
    }
    if(hip == -1) {
	setError((intFlag ? MSG_Interrupted : MSG_IdentifyHost), host);
	return false;
    }
    debugPrint(4, "%s -> %s",
       (proxy_host ? proxy_host : host), tcp_ip_dots(hip));

/* See if the protocol is a recognized stream */
    if(stringEqualCI(prot, "http") || stringEqualCI(prot, "https")) {
	;			/* ok for now */
    } else if(stringEqualCI(prot, "ftp")) {
	return ftpConnect(url);
    } else if(mt = findMimeByProtocol(prot)) {
      mimeProcess:
	cmd = pluginCommand(mt, url, 0);
	system(cmd);
	nzFree(cmd);
	return true;
    } else {
	setError(MSG_WebProtBad, prot);
	return false;
    }

/* Ok, it's http, but the suffix could force a plugin */
    post = url + strcspn(url, "?#\1");
    for(s = post - 1; s >= url && *s != '.' && *s != '/'; --s) ;
    if(*s == '.') {
	++s;
	if(post >= s + sizeof (suffix))
	    post = s + sizeof (suffix) - 1;
	strncpy(suffix, s, post - s);
	suffix[post - s] = 0;
	if((mt = findMimeBySuffix(suffix)) && mt->stream)
	    goto mimeProcess;
    }

/* Pull user password out of the url */
    user[0] = pass[0] = 0;
    if(newurl) {
	s = getUserURL(url);
	if(s) {
	    if(strlen(s) >= sizeof (user) - 2) {
		setError(MSG_UserNameLong, sizeof (user));
		return false;
	    }
	    strcpy(user, s);
	}
	s = getPassURL(url);
	if(s) {
	    if(strlen(s) >= sizeof (pass) - 2) {
		setError(MSG_PasswordLong, sizeof (pass));
		return false;
	    }
	    strcpy(pass, s);
	}
/* Don't add the authentication record yet;
 * because I don't know what the method is.
 * If I assume it's basic, and it's not, I'm sending the password
 * (essentialy) in the clear, and the user is assuming it's always
 * encrypted.  So just keep it around, and deploy it if necessary. */
    }
    newurl = false;

    getPortLocURL(url, &portloc, &port);
    hsock = tcp_connect(hip, (proxy_host ? proxy_port : port), webTimeout);
    if(hsock < 0) {
	setError((intFlag ? MSG_Interrupted : MSG_WebConnect), host);
	return false;
    }
    if(proxy_host)
	debugPrint(4, "connected to port %d/%d", proxy_port, port);
    else
	debugPrint(4, "connected to port %d", port);
    if(secure) {
	hssl = SSL_new(sslcx);
	SSL_set_fd(hssl, hsock);
/* Do we need this?
hssl->options |= SSL_OP_NO_TLSv1;
*/
	err = SSL_connect(hssl);
	if(err != 1) {
	    err = ERR_peek_last_error();
	    ERR_clear_error();
	    if(ERR_GET_REASON(err) != SSL_R_CERTIFICATE_VERIFY_FAILED)
		setError(MSG_WebConnectSecure, host, err);
	    else
		setError(MSG_NoCertify, host);
	    SSL_free(hssl);
	    close(hsock);
	    return false;
	}
	debugPrint(4, "secure connection established");
    }

    post = strchr(url, '\1');
    if(post)
	post++;

    hdr = initString(&l);
    stringAndString(&hdr, &l, post ? "POST " : "GET ");
    if(proxy_host) {
	stringAndString(&hdr, &l, prot);
	stringAndString(&hdr, &l, "://");
	stringAndString(&hdr, &l, host);
/* Some proxy servers won't accept :80 after the hostname.  Dunno why. */
	if(secure || port != 80) {
	    stringAndChar(&hdr, &l, ':');
	    stringAndNum(&hdr, &l, port);
	}
    }
    stringAndChar(&hdr, &l, '/');

    s = getDataURL(url);
    while(true) {
	char c, buf[4];
	if(post && s == post - 1)
	    break;
	c = *s;
	if(!c)
	    break;
	if(c == '#')
	    break;
	if(c == '\\')
	    c = '/';
	if(c <= ' ') {
	    sprintf(buf, "%%%02X", (unsigned char)c);
	    stringAndString(&hdr, &l, buf);
	} else
	    stringAndChar(&hdr, &l, c);
	++s;
    }
    stringAndString(&hdr, &l, " HTTP/1.0\r\n");

    stringAndString(&hdr, &l, "Host: ");
    stringAndString(&hdr, &l, host);
    if(portloc) {		/* port specified */
	stringAndChar(&hdr, &l, ':');
	stringAndNum(&hdr, &l, port);
    }
    stringAndString(&hdr, &l, eol);

    stringAndString(&hdr, &l, "User-Agent: ");
    stringAndString(&hdr, &l, currentAgent);
    stringAndString(&hdr, &l, eol);

    if(sendReferrer && currentReferrer) {
	const char *post2 = strchr(currentReferrer, '\1');
	const char *q = strchr(currentReferrer, '"');
/* I just can't handle quote in the referring url */
	if(!q || post2 && q > post2) {
/* I always thought referrer had 4 r's, but not here. */
	    stringAndString(&hdr, &l, "Referer: \"");
	    if(!post2)
		post2 = currentReferrer + strlen(currentReferrer);
	    if(post2 - currentReferrer >= 7 && !memcmp(post2 - 7, ".browse", 7))
		post2 -= 7;
	    stringAndBytes(&hdr, &l, currentReferrer, post2 - currentReferrer);
	    stringAndChar(&hdr, &l, '"');
	    stringAndString(&hdr, &l, eol);
	    cw->referrer = cloneString(currentReferrer);
	    cw->referrer[post2 - currentReferrer] = 0;
	}
    }
    stringAndString(&hdr, &l, "Accept: */*\r\n");
    stringAndString(&hdr, &l, (isprox ? "Proxy-Connection: " : "Connection: "));
/* Keep-Alive feature not yet implemented */
    stringAndString(&hdr, &l, "close\r\n");

/* Web caching not yet implemented. */
    stringAndString(&hdr, &l,
       "Pragma: no-cache\r\nCache-Control: no-cache\r\n");
    stringAndString(&hdr, &l, "Accept-Encoding: gzip, compress\r\n");
    stringAndString(&hdr, &l, "Accept-Language: en\r\n");
    if(u = getAuthString(url)) {
	stringAndString(&hdr, &l, u);
	free(u);
    }

    if(post) {
	if(strncmp(post, "`mfd~", 5)) {
	    stringAndString(&hdr, &l,
	       "Content-Type: application/x-www-form-urlencoded\r\n");
	} else {
	    post += 5;
	    stringAndString(&hdr, &l,
	       "Content-Type: multipart/form-data; boundary=");
	    s = strchr(post, '\r');
	    stringAndBytes(&hdr, &l, post, s - post);
	    post = s + 2;
	    stringAndString(&hdr, &l, eol);
	}
	stringAndString(&hdr, &l, "Content-Length: ");
	stringAndNum(&hdr, &l, strlen(post));
	stringAndString(&hdr, &l, eol);
    }
    /* post */
    sendCookies(&hdr, &l, url, secure);

/* Here's the blank line that ends the header. */
    stringAndString(&hdr, &l, eol);

    if(debugLevel >= 4) {
/* print the header to be sent. */
	for(u = hdr; *u; ++u) {
	    char c = *u;
	    if(c != '\r')
		putchar(c);
	}
    }

    if(post)
	stringAndString(&hdr, &l, post);

    if(secure)
	n = SSL_write(hssl, hdr, l);
    else
	n = tcp_write(hsock, hdr, l);
    debugPrint(4, "http header sent, %d/%d bytes", n, l);
    free(hdr);
    if(n < l) {
	setError(intFlag ? MSG_Interrupted : MSG_WebSend);
	if(secure)
	    SSL_free(hssl);
	close(hsock);
	return false;
    }

    rc = readSocket(hssl, hsock);
    if(secure)
	SSL_free(hssl);
    close(hsock);
    if(!rc)
	goto abort;

/* Parse the http header, at the start of the data stream. */
    if(serverDataLen < 16)
	goto nohead;		/* too short */
    u = serverData;
    if((*u++ | 0x20) != 'h')
	goto nohead;
    if((*u++ | 0x20) != 't')
	goto nohead;
    if((*u++ | 0x20) != 't')
	goto nohead;
    if((*u++ | 0x20) != 'p')
	goto nohead;
    if(*u++ != '/')
	goto nohead;
    while(isdigitByte(*u) || *u == '.')
	++u;
    if(*u != ' ')
	goto nohead;
    while(*u == ' ')
	++u;
    if(!isdigitByte(*u))
	goto nohead;
    hcode = strtol(u, &u, 10);
    while(*u == ' ')
	++u;
    hdr = u;
    while(*u != '\r' && *u != '\n')
	++u;
    n = u - hdr;
    if(n >= sizeof (herror))
	n = sizeof (herror) - 1;
    strncpy(herror, hdr, n);
    herror[n] = 0;
    debugPrint(3, "http code %d %s", hcode, herror);
    hdr = 0;
/* set hdr if we find our empty line */
    while(u < serverData + serverDataLen) {
	char c = *u;
	char d = 0;
	if(u < serverData + serverDataLen - 1)
	    d = u[1];
	if(!c)
	    break;
	if(c == '\n' && d == '\n') {
	    hdr = u + 2;
	    break;
	}
	if(c == '\r' && d == '\n' &&
	   u <= serverData + serverDataLen - 4 &&
	   u[2] == '\r' && u[3] == '\n') {
	    hdr = u + 4;
	    break;
	}
	++u;
    }
    if(!hdr)
	goto nohead;

    if(debugLevel >= 4) {
/* print the header just received. */
	for(u = serverData; u < hdr; ++u) {
	    char c = *u;
	    if(c != '\r')
		putchar(c);
	}
    }

/* We need to gather the cookies before we redirect. */
    s = serverData;
    while(u = extractHeaderItem(s, hdr, "Set-Cookie", &s)) {
	rc = receiveCookie(url, u);
	nzFree(u);
	debugPrint(3, rc ? "accepted" : "rejected");
    }

    if(hcode == 401) {		/* authorization requested */
	int authmeth = 1;	/* basic method by default */
	if(u = extractHeaderItem(serverData, hdr, "WWW-Authenticate", 0)) {
	    if(!memEqualCI(u, "basic", 5) || isalnumByte(u[5])) {
		setError(MSG_BadAuthMethod, u);
		nzFree(u);
		goto abort;
	    }
	    if(!(user[0] | pass[0])) {
		s = strstr(u, "realm=");
		if(s) {
		    char q = 0;
		    char *v = 0;
		    s += 6;
		    if(isquote(*s))
			q = *s++;
		    if(q)
			v = strchr(s, q);
		    if(v)
			*v = 0;
		    puts(s);
		}
	    }
	    nzFree(u);
	}
	if(!(user[0] | pass[0])) {
	    if(!isInteractive) {
		setError(MSG_Authorize2);
		goto abort;
	    }
	    i_puts(MSG_WebAuthorize);
	  getlogin:
	    i_printf(MSG_UserName);
	    fflush(stdout);
	    fflush(stdin);
	    if(!fgets(user, sizeof (user), stdin))
		ebClose(0);
	    n = strlen(user);
	    if(n >= sizeof (user) - 1) {
		i_printf(MSG_UserNameLong, sizeof (user) - 2);
		goto getlogin;
	    }
	    if(n && user[n - 1] == '\n')
		user[--n] = 0;
	    if(stringEqual(user, "x")) {
		setError(MSG_LoginAbort);
		goto abort;
	    }
	    i_printf(MSG_Password);
	    fflush(stdout);
	    fflush(stdin);
	    if(!fgets(pass, sizeof (pass), stdin))
		ebClose(0);
	    n = strlen(pass);
	    if(n >= sizeof (pass) - 1) {
		i_printf(MSG_PasswordLong, sizeof (pass) - 2);
		goto getlogin;
	    }
	    if(n && pass[n - 1] == '\n')
		pass[--n] = 0;
	    if(stringEqual(pass, "x")) {
		setError(MSG_LoginAbort);
		goto abort;
	    }
	}
	intFlag = false;
	if(!addWebAuthorization(url, 1, user, pass, false))
	    goto abort;
	nzFree(serverData);
	serverData = 0;
	serverDataLen = 0;
	goto restart;
    }
    /* 401 authentication */
    if(u = extractHeaderItem(serverData, hdr, "Content-Encoding", 0)) {
	hce = -1;
	if(stringEqualCI(u, "compress"))
	    hce = ENC_COMPRESS;
	if(stringEqualCI(u, "gzip"))
	    hce = ENC_GZIP;
	if(stringEqualCI(u, "7bit"))
	    hce = 0;
	if(stringEqualCI(u, "8bit"))
	    hce = 0;
	if(hce < 0) {
	    i_printf(MSG_CompressUnrecognized, u);
	    hce = 0;
	}
	nzFree(u);
	if(hce && (u = extractHeaderItem(serverData, hdr, "Content-type", 0))) {
/* If this isn't text, to be rendered, then don't uncompress it */
	    if(!memEqualCI(u, "text", 4) &&
	       !memEqualCI(u, "application/x-javascript", 24))
		hce = 0;
	    free(u);
	}
    }

    /* compressed */
    /* Look for http redirection.  Note,
     * meta http-equiv redirection does not occur unless and until
     * the page is browsed, i.e. its html interpreted. */
    delay = 0;
    ref = loc = u = 0;
    if((hcode >= 301 && hcode <= 303 || hcode == 200) && allowRedirection) {
	if(hcode == 200) {
	    ref = extractHeaderItem(serverData, hdr, "refresh", 0);
	    if(ref) {
		if(parseRefresh(ref, &delay)) {
		    u = ref;
		} else {
		    free(ref);
		    ref = 0;
		}
	    }
	} else {
	    loc = extractHeaderItem(serverData, hdr, "location", 0);
	    if(!loc[0])
		loc = 0;
	    if(!loc) {
		i_printf(MSG_RedirectNoURL, hcode);
		if(hcode >= 301 && hcode <= 303)
		    hcode = 200;
	    } else
		u = loc;
	}
	if(u) {
	    unpercentURL(u);
	    if(refreshDelay(delay, u)) {
		n = fetchHistory(url, u);
		if(n < 0) {
		    free(u);
		    goto abort;
		}		/* infinite loop */
		if(n == false) {
/* success, because it's already fetched */
		    free(u);
		    goto already;
		}
		if(recount >= 10) {
		    free(u);
		    i_puts(MSG_RedirectMany);
		    goto gotFile;
		}
/* Redirection looks valid, let's run with it. */
		++recount;
		free(serverData);
		serverData = 0;
		serverDataLen = 0;
/* trying to plug a subtle memory leak */
		if(recount > 2)
		    free((void *)from);
		from = url;
		url = resolveURL(from, u);
		nzFree(u);
		changeFileName = (char *)url;
		debugPrint(2, "redirect %s", url);
		newurl = true;
		goto restart;
	    } else {
		n = hdr - serverData;
		if(n >= strlen(u) + 36) {
		    char rbuf[36];
		    sprintf(rbuf, "<A HREF=%s>Refresh[%d]</A><br>\n", u, delay);
		    l = strlen(rbuf);
		    hdr -= l;
		    memcpy(hdr, rbuf, l);
		}		/* enough room */
		free(u);
	    }
	}			/* redirection present */
    }
    /* looking for redirection */
    if(hcode != 200)
	i_printf(MSG_HTTPError, hcode, herror);

  gotFile:
/* Don't need the header any more */
    n = hdr - serverData;
    serverDataLen -= n;
    memcpy(serverData, serverData + n, serverDataLen);

    if(!serverDataLen)
	return true;

    if(hce) {			/* data is compressed */
	int fh;
	char *scmd;
	char suffix[4];
	suffix[0] = 0;
	u = edbrowseTempFile + strlen(edbrowseTempFile);
	if(hce == ENC_COMPRESS)
	    strcpy(suffix, ".Z");
	if(hce == ENC_GZIP)
	    strcpy(suffix, ".gz");
	strcpy(u, suffix);
	debugPrint(3, "uncompressing the web page with method %s", suffix);
	fh =
	   open(edbrowseTempFile, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC,
	   0666);
	if(fh < 0) {
	    setError(MSG_TempNoCreate, edbrowseTempFile);
	    *u = 0;
	    goto abort;
	}
	if(write(fh, serverData, serverDataLen) < serverDataLen) {
	    setError(MSG_TempNoWrite, edbrowseTempFile);
	    close(fh);
	    *u = 0;
	    goto abort;
	}
	close(fh);
	*u = 0;
	nzFree(serverData);
	serverData = 0;
	serverDataLen = 0;
	unlink(edbrowseTempFile);
	scmd = allocMem(2 * strlen(edbrowseTempFile) + 20);
	sprintf(scmd, "zcat %s%s > %s",
	   edbrowseTempFile, suffix, edbrowseTempFile);
	system(scmd);
	free(scmd);
	n = fileSizeByName(edbrowseTempFile);
	if(n <= 0) {
	    setError(MSG_TempNoUncompress);
	    return false;
	}
	serverData = allocMem(n + 2);
	fh = open(edbrowseTempFile, O_RDONLY | O_BINARY);
	if(fh < 0) {
	    setError(MSG_TempNoAccess, edbrowseTempFile);
	    goto abort;
	}
	if(read(fh, serverData, n) < n) {
	    setError(MSG_TempNoRead, edbrowseTempFile);
	    close(fh);
	    goto abort;
	}
	close(fh);
	serverDataLen = n;
	serverData[n] = 0;
/* doesn't hurt to clean house, now that everything worked. */
	strcpy(u, suffix);
	unlink(edbrowseTempFile);
	*u = 0;
	unlink(edbrowseTempFile);
    }

    return true;

  nohead:
    i_puts(MSG_HTTPHeader);
    return true;

  abort:
    nzFree(serverData);
    serverData = 0;
    serverDataLen = 0;
    return false;
}				/* httpConnect */

/* Format a line from an ftp ls. */
static void
ftpls(char *line)
{
    int l = strlen(line);
    int j;
/* blank line becomes paragraph break */
    if(!l || !memcmp(line, "total ", 6) && stringIsNum(line + 6) > 0) {
	stringAndString(&serverData, &serverDataLen, "<P>\n");
	return;
    }
    for(j = 0; line[j]; ++j)
	if(!strchr("-rwxdls", line[j]))
	    break;
    if(j == 10 && line[j] == ' ') {	/* long list */
	int fsize, nlinks;
	char user[42], group[42];
	char *q;
	sscanf(line + j, " %d %40s %40s %d", &nlinks, user, group, &fsize);
	q = strchr(line, ':');
	if(q) {
	    for(++q; isdigitByte(*q) || *q == ':'; ++q) ;
	    while(*q == ' ')
		++q;
	    if(*q) {		/* file name */
		char qc = '"';
		if(strchr(q, qc))
		    qc = '\'';
		stringAndString(&serverData, &serverDataLen, "<br><A HREF=x");
		serverData[serverDataLen - 1] = qc;
		stringAndString(&serverData, &serverDataLen, q);
		stringAndChar(&serverData, &serverDataLen, qc);
		stringAndChar(&serverData, &serverDataLen, '>');
		stringAndString(&serverData, &serverDataLen, q);
		stringAndString(&serverData, &serverDataLen, "</A>");
		if(line[0] == 'd')
		    stringAndChar(&serverData, &serverDataLen, '/');
		stringAndString(&serverData, &serverDataLen, ": ");
		stringAndNum(&serverData, &serverDataLen, fsize);
		stringAndChar(&serverData, &serverDataLen, '\n');
		return;
	    }
	}
    }
    if(!strpbrk(line, "<>&")) {
	stringAndString(&serverData, &serverDataLen, line);
    } else {
	char c, *q;
	for(q = line; c = *q; ++q) {
	    char *meta = 0;
	    if(c == '<')
		meta = "&lt;";
	    if(c == '>')
		meta = "&gt;";
	    if(c == '&')
		meta = "&amp;";
	    if(meta)
		stringAndString(&serverData, &serverDataLen, meta);
	    else
		stringAndChar(&serverData, &serverDataLen, c);
	}
    }
    stringAndChar(&serverData, &serverDataLen, '\n');
}				/* ftpls */

/* Like httpConnect, but for ftp */
/* Basically, a system call to ncftpget */
bool
ftpConnect(const char *url)
{
    FILE *f;
    char *cmd, *out;
    int cmd_l, out_l;
    int rc;
    bool dirmode;
    int c;
    static const char npf[] = "not a plain file.";
    const int npfsize = strlen(npf);

    serverData = 0;
    serverDataLen = 0;
    fileSize = -1;
    if(debugLevel >= 1)
	i_puts(MSG_FTPDownload);
    dirmode = false;

  top:cmd = initString(&cmd_l);
    if(dirmode) {
	stringAndString(&cmd, &cmd_l, "ncftpls -l ");
    } else {
	stringAndString(&cmd, &cmd_l, "ncftpget -r 1 -v -z ");
	if(debugLevel >= 4)
	    stringAndString(&cmd, &cmd_l, "-d stdout ");
    }
    if(ftpMode) {
	char mode[4];
	sprintf(mode, "-%c ", ftpMode);
	stringAndString(&cmd, &cmd_l, mode);
    }
/* Quote the url, in case there are spaces in the name. */
    stringAndChar(&cmd, &cmd_l, '"');
    stringAndString(&cmd, &cmd_l, url);
    if(dirmode)
	stringAndChar(&cmd, &cmd_l, '/');
    stringAndChar(&cmd, &cmd_l, '"');
    stringAndString(&cmd, &cmd_l, " 2>&1");
    debugPrint(3, "%s", cmd);

    f = popen(cmd, "r");
    if(!f) {
	setError(MSG_TempNoSystem, cmd, errno);
	nzFree(cmd);
	return false;
    }
    out = initString(&out_l);
    if(dirmode) {
	serverData = initString(&serverDataLen);
	stringAndString(&serverData, &serverDataLen, "<html>\n");
    }

    while((c = getc(f)) != EOF) {
	if(c == '\r')
	    c = '\n';
	if(c == '\n') {
	    if(dirmode)
		ftpls(out);
	    else {
		if(!out_l)
		    continue;
		if(out_l > npfsize && stringEqual(out + out_l - npfsize, npf)) {
		    pclose(f);
		    nzFree(cmd);
		    nzFree(out);
		    dirmode = true;
		    goto top;
		}
/* Don't print the ETA messages */
		if(!strstr(out, " ETA: "))
		    puts(out);
	    }
	    nzFree(out);
	    out = initString(&out_l);
	} else
	    stringAndChar(&out, &out_l, c);
    }

    rc = pclose(f);
    nzFree(cmd);

    if(out_l) {			/* should never happen */
	puts(out);
	nzFree(out);
    }

    if(rc) {
	if(!(rc & 0xff))
	    rc >>= 8;
	if(rc > 0 && rc <= 11)
	    setError(MSG_FTPConnect - 1 + rc);
	else
	    setError(MSG_FTPUnexpected, rc);
	return false;
    }
    i_puts(dirmode + MSG_Success);
    if(dirmode) {		/* need a final slash */
	int l = strlen(url);
	changeFileName = allocMem(l + 2);
	strcpy(changeFileName, url);
	strcat(changeFileName, "/");
    }
    return true;
}				/* ftpConnect */


/*********************************************************************
Gather up the ip addresses of any domains referenced by this file.
These can be added to firewalls or filters.
Eventually we'll compare these against a blacklist to screen for spam.
This only works on the current buffer.
*********************************************************************/

void
allIPs(void)
{
    static IP32bit iplist[5 + 1];
    char *domlist[8];
    int iptotal = 0, domtotal = 0;
    IP32bit ip;
    int ntags, tagno;
    char *href;
    const char *dom;
    int ln;			/* line number */
    int ftype, j, k, nf;
    char *p;

    for(ln = 1; ln <= cw->dol; ++ln) {
	p = (char *)fetchLine(ln, -1);
	ftype = 0;		/* input stuff doesn't work */
	findField(p, ftype, 0, &nf, 0, 0, 0, 0);
	for(j = 1; j <= nf; ++j) {
	    findField(p, ftype, j, &nf, 0, &tagno, &href, 0);
	    if(!href)
		continue;
	    if(dom = getHostURL(href)) {
/* Ok, many times the same domain will be referenced over and over again.
 * A waste of time to look it up over and over again.
 * So I check for it here.  But what am I missing?
 * The second time I might have gotten a different ip address,
 * something I want to screen for,
 * so I should really gather all possible ip addresses on the first go,
 * but I don't have a function in tcp.c to do that,
 * and I don't have time to write one  at this moment. */
		for(k = 0; k < domtotal; ++k)
		    if(stringEqualCI(domlist[k], dom))
			break;
		if(k == domtotal) {
		    domlist[domtotal++] = cloneString(dom);
		    debugPrint(3, "hip %s", dom);
		    if(tcp_isDots(dom))
			ip = tcp_dots_ip(dom);
		    else
			ip = tcp_name_ip(dom);
		    if(ip != NULL_IP) {
/* could be a repeat */
			for(k = 0; k < iptotal; ++k)
			    if(iplist[k] == ip)
				break;
			if(k == iptotal) {
			    iplist[iptotal++] = ip;
			    if(ismc && onBlacklist1(ip)) {
				nzFree(href);
				goto done;
			    }
			}
		    }		/* valid ip */
		}
		/* different domain */
	    }			/* valid domain */
	    nzFree(href);
	    if(iptotal == 5)
		goto done;
	    if(domtotal == 8)
		goto done;
	}			/* loop over references on this line */
    }				/* loop over lines */

  done:
    iplist[iptotal] = NULL_IP;
    cw->iplist = iplist;
    for(k = 0; k < domtotal; ++k)
	nzFree(domlist[k]);
}				/* allIPs */
