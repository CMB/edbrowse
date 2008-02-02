/* main.c
 * Entry point, arguments and options.
 * Copyright (c) Karl Dahlke, 2006
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"

#include <signal.h>
#include <pcre.h>

/* Define the globals that are declared in eb.h. */
/* See eb.h for descriptive comments. */

const char *version = "3.3.2";
char *userAgents[10], *currentAgent, *currentReferrer;
const char eol[] = "\r\n";
char EMPTYSTRING[] = "";
int debugLevel = 1;
int webTimeout = 20, mailTimeout = 0;
bool ismc, browseLocal, zapMail, unformatMail, passMail, errorExit;
bool isInteractive, intFlag, inInput;
int fileSize, maxFileSize = 50000000;
int localAccount, maxAccount;
struct MACCOUNT accounts[MAXACCOUNT];
int maxMime;
struct MIMETYPE mimetypes[MAXMIME];
static int maxTables;
static struct DBTABLE dbtables[MAXDBT];
char *dbarea, *dblogin, *dbpw;	/* to log into the database */
char *proxy_host;
int proxy_port;
bool caseInsensitive, searchStringsAll;
bool textAreaDosNewlines = true, undoable;
bool allowRedirection = true, allowJS = true, sendReferrer = false;
bool binaryDetect = true;
char ftpMode;
bool showHiddenFiles, helpMessagesOn;
uchar dirWrite, endMarks;
int context = 1;
uchar linePending[MAXTTYLINE];
char *changeFileName, *mailDir;
char *addressFile, *ipbFile;
char *home, *recycleBin, *configFile, *sigFile;
char *cookieFile, *spamCan;
char *edbrowseTempFile, *edbrowseTempPDF, *edbrowseTempHTML;
pst *textLines;
int textLinesMax, textLinesCount;
struct ebWindow *cw;
struct ebSession sessionList[MAXSESSION], *cs;

/* Edbrowse functions, defined in the config file */
#define MAXEBSCRIPT 500
#define MAXNEST 20
static char *ebScript[MAXEBSCRIPT + 1];
static char *ebScriptName[MAXEBSCRIPT + 1];
#define MAXNOJS 500
static const char *javaDis[MAXNOJS];
static int javaDisCount;
#define MAXFILTER 500
struct FILTERDESC {
    const char *match;
    const char *redirect;
    char type;
    long expire;
};
static struct FILTERDESC filters[MAXFILTER];
static int n_filters;
static int subjstart = 0;
static char *cfgcopy;
static int cfglen;
static long nowday;

static void
setNowDay(void)
{
    time_t now;
    time(&now);
    now /= (60 * 60 * 24);	/* convert to days */
    now -= 30 * 365;
    now -= 7;			/* leap years */
    nowday = now;
}				/* setNowDay */

static void
updateConfig(void)
{
    int fh = open(configFile, O_WRONLY | O_TRUNC, 0);
    if(fh < 0) {
	debugPrint(0, "warning: cannot update config file");
	return;
    }
    if(write(fh, cfgcopy, cfglen) < cfglen)
	i_printfExit(MSG_ERBC_NoWrite);
    close(fh);
}				/* updateConfig */

bool
junkSubject(const char *s, char key)
{
    int l, n;
    char *new;
    long exp = nowday;
    if(!s || !*s) {
	i_puts(MSG_NoSubject);
	return false;
    }
    if(!cfgcopy) {
	i_puts(MSG_NoConfig);
	return false;
    }
    if(!subjstart) {
	i_puts(MSG_NoSubjFilter);
	return false;
    }
    if(key == 'j')
	exp += 10;
    if(key == 'J')
	exp += 365;
    l = strlen(s) + 10;
    if(exp > 9999)
	++l;
    n = cfglen + l;
    new = allocMem(n);
    memcpy(new, cfgcopy, subjstart);
    sprintf(new + subjstart, "%d`%s > x\n", exp, s);
    memcpy(new + subjstart + l, cfgcopy + subjstart, cfglen - subjstart);
    nzFree(cfgcopy);
    cfgcopy = new;
    cfglen = n;
    updateConfig();
    if(n_filters < MAXFILTER - 1) {
	filters[n_filters].type = 4;
	filters[n_filters].match = cloneString(s);
	filters[n_filters].redirect = "x";
	++n_filters;
    }
    return true;
}				/* junkSubject */

/* This routine succeeds, or aborts via i_printfExit */
static void
readConfigFile(void)
{
    char *buf, *s, *t, *v, *q;
    char *cfglp, *cfgnlp;
    int buflen, n;
    char c, ftype;
    bool cmt = false;
    bool startline = true;
    bool cfgmodify = false;
    uchar mailblock = 0, mimeblock = 0, tabblock = 0;
    int nest, ln, j;
    int sn = 0;			/* script number */
    char stack[MAXNEST];
    char last[24];
    int lidx = 0;
    struct MACCOUNT *act;
    struct MIMETYPE *mt;
    struct DBTABLE *td;
    static const char *const keywords[] = {
	"inserver", "outserver", "login", "password", "from", "reply",
	"inport", "outport",
	"type", "desc", "suffix", "protocol", "program",
	"tname", "tshort", "cols", "keycol",
	"adbook", "ipblack", "maildir", "agent",
	"jar", "nojs", "spamcan",
	"webtimer", "mailtimer", "certfile", "database", "proxy",
	0
    };

    if(!fileTypeByName(configFile, false))
	return;			/* config file not present */
    if(!fileIntoMemory(configFile, &buf, &buflen))
	showErrorAbort();
/* An extra newline won't hurt. */
    if(buflen && buf[buflen - 1] != '\n')
	buf[buflen++] = '\n';
/* make copy */
    cfgcopy = allocMem(buflen + 1);
    memcpy(cfgcopy, buf, (cfglen = buflen));

/* Undos, uncomment, watch for nulls */
/* Encode mail{ as hex 81 m, and other encodings. */
    ln = 1;
    for(s = t = v = buf; s < buf + buflen; ++s) {
	c = *s;
	if(c == '\0')
	    i_printfExit(MSG_ERBC_Nulls, ln);
	if(c == '\r' && s[1] == '\n')
	    continue;
	if(cmt) {
	    if(c != '\n')
		continue;
	    cmt = false;
	}
	if(c == '#' && startline) {
	    cmt = true;
	    goto putc;
	}
	if(c == '\n') {
	    last[lidx] = 0;
	    lidx = 0;
	    if(stringEqual(last, "}")) {
		*v = '\x82';
		t = v + 1;
	    }
	    if(stringEqual(last, "}else{")) {
		*v = '\x83';
		t = v + 1;
	    }
	    if(stringEqual(last, "mail{")) {
		*v = '\x81';
		v[1] = 'm';
		t = v + 2;
	    }
	    if(stringEqual(last, "mime{")) {
		*v = '\x81';
		v[1] = 'e';
		t = v + 2;
	    }
	    if(stringEqual(last, "table{")) {
		*v = '\x81';
		v[1] = 'b';
		t = v + 2;
	    }
	    if(stringEqual(last, "fromfilter{")) {
		*v = '\x81';
		v[1] = 'r';
		t = v + 2;
	    }
	    if(stringEqual(last, "tofilter{")) {
		*v = '\x81';
		v[1] = 't';
		t = v + 2;
	    }
	    if(stringEqual(last, "subjfilter{")) {
		*v = '\x81';
		v[1] = 's';
		t = v + 2;
		if(!subjstart)
		    subjstart = s + 1 - buf;
	    }
	    if(stringEqual(last, "if(*){")) {
		*v = '\x81';
		v[1] = 'I';
		t = v + 2;
	    }
	    if(stringEqual(last, "if(?){")) {
		*v = '\x81';
		v[1] = 'i';
		t = v + 2;
	    }
	    if(stringEqual(last, "while(*){")) {
		*v = '\x81';
		v[1] = 'W';
		t = v + 2;
	    }
	    if(stringEqual(last, "while(?){")) {
		*v = '\x81';
		v[1] = 'w';
		t = v + 2;
	    }
	    if(stringEqual(last, "until(*){")) {
		*v = '\x81';
		v[1] = 'U';
		t = v + 2;
	    }
	    if(stringEqual(last, "until(?){")) {
		*v = '\x81';
		v[1] = 'u';
		t = v + 2;
	    }
	    if(!strncmp(last, "loop(", 5) && isdigitByte(last[5])) {
		q = last + 6;
		while(isdigitByte(*q))
		    ++q;
		if(stringEqual(q, "){")) {
		    *q = 0;
		    last[4] = 'l';
		    last[3] = '\x81';
		    strcpy(v, last + 3);
		    t = v + strlen(v);
		}
	    }
	    if(!strncmp(last, "function", 8) &&
	       (last[8] == '+' || last[8] == ':')) {
		q = last + 9;
		if(*q == 0 || *q == '{' || *q == '(')
		    i_printfExit(MSG_ERBC_NoFnName, ln);
		if(isdigitByte(*q))
		    i_printfExit(MSG_ERBC_FnDigit, ln);
		while(isalnumByte(*q))
		    ++q;
		if(q - last - 9 > 10)
		    i_printfExit(MSG_ERBC_FnTooLong, ln);
		if(*q != '{' || q[1])
		    i_printfExit(MSG_ERBC_SyntaxErr, ln);
		last[7] = 'f';
		last[6] = '\x81';
		strcpy(v, last + 6);
		t = v + strlen(v);
	    }
	    *t++ = c;
	    v = t;
	    ++ln;
	    startline = true;
	    continue;
	}
	if(c == ' ' || c == '\t') {
	    if(startline)
		continue;
	} else {
	    if(lidx < sizeof (last) - 1)
		last[lidx++] = c;
	    startline = false;
	}
      putc:
	*t++ = c;
    }
    *t = 0;			/* now it's a string */

/* Go line by line */
    ln = 1;
    nest = 0;
    stack[0] = ' ';

    for(s = buf, cfglp = cfgcopy; *s; s = t + 1, cfglp = cfgnlp, ++ln) {
	cfgnlp = strchr(cfglp, '\n') + 1;
	t = strchr(s, '\n');
	if(t == s)
	    continue;		/* empty line */
	if(t == s + 1 && *s == '#')
	    continue;		/* comment */
	*t = 0;			/* I'll put it back later */

/* Gather the filters in a mail filter block */
	if(mailblock > 1 && !strchr("\x81\x82\x83", *s)) {
	    v = strchr(s, '>');
	    if(!v)
		i_printfExit(MSG_ERBC_NoCondFile, ln);
	    while(v > s && (v[-1] == ' ' || v[-1] == '\t'))
		--v;
	    if(v == s)
		i_printfExit(MSG_ERBC_NoMatchStr, ln);
	    c = *v, *v++ = 0;
	    if(c != '>') {
		while(*v != '>')
		    ++v;
		++v;
	    }
	    while(*v == ' ' || *v == '\t')
		++v;
	    if(!*v)
		i_printfExit(MSG_ERBC_MatchNowh, ln, s);
	    if(n_filters == MAXFILTER - 1)
		i_printfExit(MSG_ERBC_Filters, ln);
	    filters[n_filters].redirect = v;
	    if(mailblock >= 2) {
		long exp = strtol(s, &v, 10);
		if(exp > 0 && *v == '`' && v[1]) {
		    s = v + 1;
		    filters[n_filters].expire = exp;
		    if(exp <= nowday) {
			cfgmodify = true;
			memcpy(cfglp, cfgnlp, cfgcopy + cfglen - cfgnlp);
			cfglen -= (cfgnlp - cfglp);
			cfgnlp = cfglp;
			continue;
		    }		/* filter rule out of date */
		}
	    }
	    filters[n_filters].match = s;
	    filters[n_filters].type = mailblock;
	    ++n_filters;
	    continue;
	}

	v = strchr(s, '=');
	if(!v)
	    goto nokeyword;

	while(v > s && (v[-1] == ' ' || v[-1] == '\t'))
	    --v;
	if(v == s)
	    goto nokeyword;
	c = *v, *v = 0;
	for(q = s; q < v; ++q)
	    if(!isalphaByte(*q)) {
		*v = c;
		goto nokeyword;
	    }

	n = stringInList(keywords, s);
	if(n < 0) {
	    if(!nest)
		i_printfExit(MSG_ERBC_BadKeyword, s, ln);
	    *v = c;		/* put it back */
	    goto nokeyword;
	}

	if(n < 8 && mailblock != 1)
	    i_printfExit(MSG_ERBC_MailAttrOut, ln, s);

	if(n >= 8 && n < 13 && mimeblock != 1)
	    i_printfExit(MSG_ERBC_MimeAttrOut, ln, s);

	if(n >= 13 && n < 17 && tabblock != 1)
	    i_printfExit(MSG_ERBC_TableAttrOut, ln, s);

	if(n >= 8 && mailblock)
	    i_printfExit(MSG_ERBC_MailAttrIn, ln, s);

	if((n < 8 || n >= 13) && mimeblock)
	    i_printfExit(MSG_ERBC_MimeAttrIn, ln, s);

	if((n < 13 || n >= 17) && tabblock)
	    i_printfExit(MSG_ERBC_TableAttrIn, ln, s);

/* act upon the keywords */
	++v;
	if(c != '=') {
	    while(*v != '=')
		++v;
	    ++v;
	}
	while(*v == ' ' || *v == '\t')
	    ++v;
	if(!*v)
	    i_printfExit(MSG_ERBC_NoAttr, ln, s);

	switch (n) {
	case 0:
	    act->inurl = v;
	    continue;

	case 1:
	    act->outurl = v;
	    continue;

	case 2:
	    act->login = v;
	    continue;

	case 3:
	    act->password = v;
	    continue;

	case 4:
	    act->from = v;
	    continue;

	case 5:
	    act->reply = v;
	    continue;

	case 6:
	    if(*v == '*')
		act->inssl = true, ++v;
	    act->inport = atoi(v);
	    continue;

	case 7:
	    if(*v == '*')
		act->outssl = true, ++v;
	    act->outport = atoi(v);
	    continue;

	case 8:
	    if(*v == '<')
		mt->stream = true, ++v;
	    mt->type = v;
	    continue;

	case 9:
	    mt->desc = v;
	    continue;

	case 10:
	    mt->suffix = v;
	    continue;

	case 11:
	    mt->prot = v;
	    continue;

	case 12:
	    mt->program = v;
	    continue;

	case 13:
	    td->name = v;
	    continue;

	case 14:
	    td->shortname = v;
	    continue;

	case 15:
	    while(*v) {
		if(td->ncols == MAXTCOLS)
		    i_printfExit(MSG_ERBC_ManyCols, ln, MAXTCOLS);
		td->cols[td->ncols++] = v;
		q = strchr(v, ',');
		if(!q)
		    break;
		*q = 0;
		v = q + 1;
	    }
	    continue;

	case 16:
	    if(!isdigitByte(*v))
		i_printfExit(MSG_ERBC_KeyNotNb, ln);
	    td->key1 = strtol(v, &v, 10);
	    if(*v == ',' && isdigitByte(v[1]))
		td->key2 = strtol(v + 1, &v, 10);
	    if(td->key1 > td->ncols || td->key2 > td->ncols)
		i_printfExit(MSG_ERBC_KeyOutRange, ln, td->ncols);
	    continue;

	case 17:
	    addressFile = v;
	    ftype = fileTypeByName(v, false);
	    if(ftype && ftype != 'f')
		i_printfExit(MSG_ERBC_AbNotFile, v);
	    continue;

	case 18:
	    ipbFile = v;
	    ftype = fileTypeByName(v, false);
	    if(ftype && ftype != 'f')
		i_printfExit(MSG_ERBC_IPNotFile, v);
	    continue;

	case 19:
	    mailDir = v;
	    if(fileTypeByName(v, false) != 'd')
		i_printfExit(MSG_ERBC_NotDir, v);
	    continue;

	case 20:
	    for(j = 0; j < 10; ++j)
		if(!userAgents[j])
		    break;
	    if(j == 10)
		i_printfExit(MSG_ERBC_ManyAgents, ln);
	    userAgents[j] = v;
	    continue;

	case 21:
	    cookieFile = v;
	    ftype = fileTypeByName(v, false);
	    if(ftype && ftype != 'f')
		i_printfExit(MSG_ERBC_JarNotFile, v);
	    j = open(v, O_WRONLY | O_APPEND | O_CREAT, 0600);
	    if(j < 0)
		i_printfExit(MSG_ERBC_JarNoWrite, v);
	    close(j);
	    continue;

	case 22:
	    if(javaDisCount == MAXNOJS)
		i_printfExit(MSG_ERBC_NoJS, MAXNOJS);
	    if(*v == '.')
		++v;
	    q = strchr(v, '.');
	    if(!q || q[1] == 0)
		i_printfExit(MSG_ERBC_DomainDot, ln, v);
	    javaDis[javaDisCount++] = v;
	    continue;

	case 23:
	    spamCan = v;
	    ftype = fileTypeByName(v, false);
	    if(ftype && ftype != 'f')
		i_printfExit(MSG_ERBC_TrashNotFile, v);
	    continue;

	case 24:
	    webTimeout = atoi(v);
	    continue;

	case 25:
	    mailTimeout = atoi(v);
	    continue;

	case 26:
	    sslCerts = v;
	    ftype = fileTypeByName(v, false);
	    if(ftype && ftype != 'f')
		i_printfExit(MSG_ERBC_SSLNoFile, v);
	    j = open(v, O_RDONLY);
	    if(j < 0)
		i_printfExit(MSG_ERBC_SSLNoRead, v);
	    close(j);
	    continue;

	case 27:
	    dbarea = v;
	    v = strchr(v, ',');
	    if(!v)
		continue;
	    *v++ = 0;
	    dblogin = v;
	    v = strchr(v, ',');
	    if(!v)
		continue;
	    *v++ = 0;
	    dbpw = v;
	    continue;

	case 28:		/* proxy */
	    proxy_host = v;
	    proxy_port = 80;
	    v = strchr(v, ':');
	    if(v) {
		*v++ = 0;
		proxy_port = atoi(v);
	    }
	    continue;

	default:
	    i_printfExit(MSG_ERBC_KeywordNYI, ln, s);
	}			/* switch */

      nokeyword:

	if(stringEqual(s, "default") && mailblock == 1) {
	    if(localAccount == maxAccount + 1)
		continue;
	    if(localAccount)
		i_printfExit(MSG_ERBC_SevDefaults);
	    localAccount = maxAccount + 1;
	    continue;
	}

	if(*s == '\x82' && s[1] == 0) {
	    if(mailblock == 1) {
		++maxAccount;
		mailblock = 0;
		if(!act->inurl)
		    i_printfExit(MSG_ERBC_NoInserver, ln);
		if(!act->outurl)
		    i_printfExit(MSG_ERBC_NoOutserver, ln);
		if(!act->login)
		    i_printfExit(MSG_ERBC_NoLogin, ln);
		if(!act->password)
		    i_printfExit(MSG_ERBC_NPasswd, ln);
		if(!act->from)
		    i_printfExit(MSG_ERBC_NoFrom, ln);
		if(!act->reply)
		    i_printfExit(MSG_ERBC_NoReply, ln);
		if(!act->inport)
		    act->inport = 110;
		if(!act->outport)
		    act->outport = 25;
		continue;
	    }

	    if(mailblock) {
		mailblock = 0;
		continue;
	    }

	    if(mimeblock == 1) {
		++maxMime;
		mimeblock = 0;
		if(!mt->type)
		    i_printfExit(MSG_ERBC_NoType, ln);
		if(!mt->desc)
		    i_printfExit(MSG_ERBC_NDesc, ln);
		if(!mt->suffix && !mt->prot)
		    i_printfExit(MSG_ERBC_NoSuffix, ln);
		if(!mt->program)
		    i_printfExit(MSG_ERBC_NoProgram, ln);
		continue;
	    }

	    if(tabblock == 1) {
		++maxTables;
		tabblock = 0;
		if(!td->name)
		    i_printfExit(MSG_ERBC_NoTblName, ln);
		if(!td->shortname)
		    i_printfExit(MSG_ERBC_NoShortName, ln);
		if(!td->ncols)
		    i_printfExit(MSG_ERBC_NColumns, ln);
		continue;
	    }

	    if(--nest < 0)
		i_printfExit(MSG_ERBC_UnexpBrace, ln);
	    if(nest)
		goto putback;
/* This ends the function */
	    *s = 0;		/* null terminate the script */
	    ++sn;
	    continue;
	}

	if(*s == '\x83' && s[1] == 0) {
/* Does else make sense here? */
	    c = toupper(stack[nest]);
	    if(c != 'I')
		i_printfExit(MSG_ERBC_UnexElse, ln);
	    goto putback;
	}

	if(*s != '\x81') {
	    if(!nest)
		i_printfExit(MSG_ERBC_GarblText, ln);
	    goto putback;
	}

/* Starting something */
	c = s[1];
	if((nest || mailblock || mimeblock) && strchr("fmerts", c)) {
	    const char *curblock = "another function";
	    if(mailblock)
		curblock = "a mail descriptor";
	    if(mailblock > 1)
		curblock = "a filter block";
	    if(mimeblock)
		curblock = "a mime descriptor";
	    i_printfExit(MSG_ERBC_FnNotStart, ln, curblock);
	}

	if(!strchr("fmertsb", c) && !nest)
	    i_printfExit(MSG_ERBC_StatNotInFn, ln);

	if(c == 'm') {
	    mailblock = 1;
	    if(maxAccount == MAXACCOUNT)
		i_printfExit(MSG_ERBC_ManyAcc, MAXACCOUNT);
	    act = accounts + maxAccount;
	    continue;
	}

	if(c == 'e') {
	    mimeblock = 1;
	    if(maxMime == MAXMIME)
		i_printfExit(MSG_ERBC_ManyTypes, MAXMIME);
	    mt = mimetypes + maxMime;
	    continue;
	}

	if(c == 'b') {
	    tabblock = 1;
	    if(maxTables == MAXDBT)
		i_printfExit(MSG_ERBC_ManyTables, MAXDBT);
	    td = dbtables + maxTables;
	    continue;
	}

	if(c == 'r') {
	    mailblock = 2;
	    continue;
	}

	if(c == 't') {
	    mailblock = 3;
	    continue;
	}

	if(c == 's') {
	    mailblock = 4;
	    continue;
	}

	if(c == 'f') {
	    stack[++nest] = c;
	    if(sn == MAXEBSCRIPT)
		i_printfExit(MSG_ERBC_ManyFn, sn);
	    ebScriptName[sn] = s + 2;
	    t[-1] = 0;
	    ebScript[sn] = t;
	    goto putback;
	}

	if(++nest >= sizeof (stack))
	    i_printfExit(MSG_ERBC_TooDeeply, ln);
	stack[nest] = c;

      putback:
	*t = '\n';
    }				/* loop over lines */

    if(nest)
	i_printfExit(MSG_ERBC_FnNotClosed, ebScriptName[sn]);

    if(mailblock | mimeblock)
	i_printfExit(MSG_ERBC_MNotClosed);

    if(cfgmodify)
	updateConfig();
}				/* readConfigFile */


/*********************************************************************
Redirect the incoming mail into a file, based on the subject or the sender.
Along with the filters in .ebrc, this routine dips into your addressbook,
to see if the sender (by email) is one of your established aliases.
If it is, we save it in a file of the same name.
This is saved formatted, unless you put a minus sign
at the start of the alias in your address book.
This is the same convention as the from filters in .ebrc.
If you don't want an alias to act as a redirect filter,
put a ! at the beginning of the alias name.
*********************************************************************/

const char *
mailRedirect(const char *to, const char *from,
   const char *reply, const char *subj)
{
    int rlen = strlen(reply);
    int slen = strlen(subj);
    int tlen = strlen(to);
    struct FILTERDESC *f;
    const char *r;

    for(f = filters; f->match; ++f) {
	const char *m = f->match;
	int mlen = strlen(m);
	r = f->redirect;
	int j, k;

	switch (f->type) {
	case 2:
	    if(stringEqualCI(m, from))
		return r;
	    if(stringEqualCI(m, reply))
		return r;
	    if(*m == '@' && mlen < rlen &&
	       stringEqualCI(m, reply + rlen - mlen))
		return r;
	    break;

	case 3:
	    if(stringEqualCI(m, to))
		return r;
	    if(*m == '@' && mlen < tlen && stringEqualCI(m, to + tlen - mlen))
		return r;
	    break;

	case 4:
	    if(stringEqualCI(m, subj))
		return r;
/* a prefix match is ok */
	    if(slen < 16 || mlen < 16)
		break;		/* too short */
	    j = k = 0;
	    while(true) {
		char c = subj[j];
		char d = m[k];
		if(isupperByte(c))
		    c = tolower(c);
		if(isupperByte(d))
		    d = tolower(d);
		if(!c || !d)
		    break;
		if(c != d)
		    break;
		for(++j; c == subj[j]; ++j) ;
		for(++k; d == m[k]; ++k) ;
	    }
/* must match at least 2/3 of either string */
	    if(k > j)
		j = k;
	    if(j >= 2 * mlen / 3 || j >= 2 * slen / 3) {
		return r;
	    }
	    break;
	}			/* switch */
    }				/* loop */

    r = reverseAlias(reply);
    return r;
}				/* mailRedirect */


/*********************************************************************
Are we ok to parse and execute javascript?
*********************************************************************/

bool
javaOK(const char *url)
{
    int j, hl, dl;
    const char *h, *d, *q, *path;
    if(!allowJS)
	return false;
    if(!url)
	return true;
    h = getHostURL(url);
    if(!h)
	return true;
    hl = strlen(h);
    path = getDataURL(url);
    for(j = 0; j < javaDisCount; ++j) {
	d = javaDis[j];
	q = strchr(d, '/');
	if(!q)
	    q = d + strlen(d);
	dl = q - d;
	if(dl > hl)
	    continue;
	if(!memEqualCI(d, h + hl - dl, dl))
	    continue;
	if(*q == '/') {
	    ++q;
	    if(hl != dl)
		continue;
	    if(!path)
		continue;
	    if(strncmp(q, path, strlen(q)))
		continue;
	    return false;
	}			/* domain/path was specified */
	if(hl == dl)
	    return false;
	if(h[hl - dl - 1] == '.')
	    return false;
    }
    return true;
}				/* javaOK */

/* Catch interrupt and react appropriately. */
static void
catchSig(int n)
{
    intFlag = true;
    if(inInput)
	i_puts(MSG_EnterInterrupt);
/* If we were reading from a file, or socket, this signal should
 * cause the read to fail.  Check for intFlag, so we know it was
 * interrupted, and not an io failure.
 * Then clean up appropriately. */
    signal(SIGINT, catchSig);
}				/* catchSig */

void
ebClose(int n)
{
    dbClose();
    exit(n);
}				/* ebClose */

void
eeCheck(void)
{
    if(errorExit)
	ebClose(1);
}

/* I'm not going to expand wild card arguments here.
 * I don't need to on Unix, and on Windows there is a
 * setargv.obj, or something like that, that performs the expansion.
 * I'll assume you have folded that object into libc.lib.
 * So now you can edit *.c, on any operating system,
 * and it will do the right thing, with no work on my part. */

int
main(int argc, char **argv)
{
    int cx, account;
    bool rc, doConfig = true;

    selectLanguage();

    ttySaveSettings();

/* Let's everybody use my malloc and free routines */
    pcre_malloc = allocMem;
    pcre_free = nzFree;

/* Establish the home directory, and standard edbrowse files thereunder. */
    home = getenv("HOME");
/* Empty is the same as missing. */
    if(home && !*home)
	home = 0;
/* I require this, though I'm not sure what this means for non-Unix OS's */
    if(!home)
	i_printfExit(MSG_NotHome);
    if(fileTypeByName(home, false) != 'd')
	i_printfExit(MSG_NotDir, home);

/* See sample.ebrc in this directory for a sample config file. */
    configFile = allocMem(strlen(home) + 7);
    sprintf(configFile, "%s/.ebrc", home);

    recycleBin = allocMem(strlen(home) + 10);
    sprintf(recycleBin, "%s/.recycle", home);
    edbrowseTempFile = allocMem(strlen(recycleBin) + 8 + 6);
/* The extra 6 is for the suffix */
    sprintf(edbrowseTempFile, "%s/eb_tmp", recycleBin);
    edbrowseTempPDF = allocMem(strlen(recycleBin) + 8);
    sprintf(edbrowseTempPDF, "%s/eb_pdf", recycleBin);
    edbrowseTempHTML = allocMem(strlen(recycleBin) + 9);
    sprintf(edbrowseTempHTML, "%s/eb_html", recycleBin);
    if(fileTypeByName(recycleBin, false) != 'd') {
	if(mkdir(recycleBin, 0700)) {
	    free(recycleBin);
	    recycleBin = 0;
	}
    }

    sigFile = allocMem(strlen(home) + 12);
    sprintf(sigFile, "%s/.signature", home);

    {
	static char agent0[32] = "edbrowse/";
	strcat(agent0, version);
	userAgents[0] = currentAgent = agent0;
    }

    setNowDay();

    ++argv, --argc;
    if(argc && stringEqual(argv[0], "-c")) {
	if(argc == 1)
	    *argv = configFile;
	else
	    ++argv, --argc;
	doConfig = false;
    } else {
	readConfigFile();
	if(maxAccount && !localAccount)
	    localAccount = 1;
    }
    account = localAccount;

    for(; argc && argv[0][0] == '-'; ++argv, --argc) {
	char *s = *argv;
	++s;
	if(stringEqual(s, "v")) {
	    puts(version);
	    exit(0);
	}
	if(stringEqual(s, "d")) {
	    debugLevel = 4;
	    continue;
	}
	if(*s == 'd' && isdigitByte(s[1]) && !s[2]) {
	    debugLevel = s[1] - '0';
	    continue;
	}
	if(stringEqual(s, "e")) {
	    errorExit = true;
	    continue;
	}
	if(*s == 'u')
	    ++s, unformatMail = true;
	if(*s == 'p')
	    ++s, passMail = true;
	if(*s == 'm' && isdigitByte(s[1])) {
	    if(!maxAccount)
		i_printfExit(MSG_NoMailAcc);
	    account = strtol(s + 1, &s, 10);
	    if(account == 0 || account > maxAccount)
		i_printfExit(MSG_BadAccNb, maxAccount);
	    if(!*s) {
		ismc = true;	/* running as a mail client */
		allowJS = false;	/* no javascript in mail client */
		++argv, --argc;	/* we're going to break out */
		if(argc && stringEqual(argv[0], "-Zap"))
		    ++argv, --argc, zapMail = true;
		break;
	    }
	}
	i_printfExit(MSG_Usage);
    }				/* options */

    if(tcp_init() < 0)
	debugPrint(4, "tcp failure, could not identify this machine");
    else
	debugPrint(4, "host info established for %s, %s",
	   tcp_thisMachineName, tcp_thisMachineDots);

    if(!sslCerts) {
	verifyCertificates = 0;
	if(doConfig)
	    if(debugLevel >= 1)
		i_puts(MSG_NoCertFile);
    }
    ssl_init();

    srand(time(0));

    loadBlacklist();

    if(ismc) {
	char **reclist, **atlist;
	char *s, *body;
	int nat, nalt, nrec;
	if(!argc)
	    fetchMail(account);
	if(argc == 1)
	    i_printfExit(MSG_MinOneRec);
/* I don't know that argv[argc] is 0, or that I can set it to 0,
 * so I back everything up by 1. */
	reclist = argv - 1;
	for(nat = nalt = 0; nat < argc; ++nat) {
	    s = argv[argc - 1 - nat];
	    if(*s != '+' && *s != '-')
		break;
	    if(*s == '-')
		++nalt;
	    strcpy(s, s + 1);
	}
	atlist = argv + argc - nat - 1;
	if(atlist <= argv)
	    i_printfExit(MSG_MinOneRecBefAtt);
	body = *atlist;
	if(nat)
	    memcpy(atlist, atlist + 1, sizeof (char *) * nat);
	atlist[nat] = 0;
	nrec = atlist - argv;
	memcpy(reclist, reclist + 1, sizeof (char *) * nrec);
	atlist[-1] = 0;
	if(sendMail(account, (const char **)reclist, body, 1,
	   (const char **)atlist, nalt, true))
	    exit(0);
	showError();
	exit(1);
    }

    cookiesFromJar();

    signal(SIGINT, catchSig);
    siginterrupt(SIGINT, 1);
    signal(SIGPIPE, SIG_IGN);


    cx = 0;
    while(argc) {
	char *file = *argv;
	++cx;
	if(cx == MAXSESSION)
	    i_printfExit(MSG_ManyOpen, MAXSESSION);
	cxSwitch(cx, false);
	if(cx == 1)
	    runEbFunction("init");
	changeFileName = 0;
	fetchHistory(0, 0);	/* reset history */
	cw->fileName = cloneString(file);
	if(isSQL(file))
	    cw->sqlMode = true;
	rc = readFile(file, "");
	if(fileSize >= 0)
	    debugPrint(1, "%d", fileSize);
	fileSize = -1;
	if(!rc) {
	    showError();
	} else if(changeFileName) {
	    nzFree(cw->fileName);
	    cw->fileName = changeFileName;
	    changeFileName = 0;
	}
	if(cw->fileName && memEqualCI(cw->fileName, "ftp://", 6)) {
	    nzFree(cw->fileName);
	    cw->fileName = 0;
	}
	cw->firstOpMode = cw->changeMode = false;
/* Browse the text if it's a url */
	if(rc && !(cw->binMode | cw->dirMode) && cw->dol && isURL(cw->fileName)) {
	    if(runCommand("b"))
		debugPrint(1, "%d", fileSize);
	    else
		showError();
	}
	++argv, --argc;
    }				/* loop over files */
    if(!cx) {			/* no files */
	++cx;
	cxSwitch(cx, false);
	runEbFunction("init");
	i_puts(MSG_Ready);
    }
    if(cx > 1)
	cxSwitch(1, false);
    undoable = false;

    while(true) {
	uchar saveline[MAXTTYLINE];
	pst p = inputLine();
	copyPstring(saveline, p);
	if(perl2c((char *)p))
	    i_puts(MSG_EnterNull);
	else
	    edbrowseCommand((char *)p, false);
	copyPstring(linePending, saveline);
    }				/* infinite loop */
}				/* main */

/* Find the balancing brace in an edbrowse function */
static const char *
balance(const char *ip, int direction)
{
    int nest = 0;
    uchar code;

    while(true) {
	if(direction > 0) {
	    ip = strchr(ip, '\n') + 1;
	} else {
	    for(ip -= 2; *ip != '\n'; --ip) ;
	    ++ip;
	}
	code = *ip;
	if(code == 0x83) {
	    if(nest)
		continue;
	    break;
	}
	if(code == 0x81)
	    nest += direction;
	if(code == 0x82)
	    nest -= direction;
	if(nest < 0)
	    break;
    }

    return ip;
}				/* balance */

/* Run an edbrowse function, as defined in the config file. */
bool
runEbFunction(const char *line)
{
    char *linecopy = cloneString(line);
    const char *args[10];
    int argl[10];		/* lengths of args */
    int argtl;			/* total length */
    const char *s;
    char *t, *new;
    int j, l, nest;
    const char *ip;		/* think instruction pointer */
    const char *endl;		/* end of line to be processed */
    bool nofail, ok;
    uchar code;
    char stack[MAXNEST];
    int loopcnt[MAXNEST];

/* Separate function name and arguments */
    spaceCrunch(linecopy, true, false);
    if(linecopy[0] == 0) {
	setError(MSG_NoFunction);
	goto fail;
    }
    memset(args, 0, sizeof (args));
    memset(argl, 0, sizeof (argl));
    argtl = 0;
    t = strchr(linecopy, ' ');
    if(t)
	*t = 0;
    for(s = linecopy; *s; ++s)
	if(!isalnumByte(*s)) {
	    setError(MSG_BadFunctionName);
	    goto fail;
	}
    for(j = 0; ebScript[j]; ++j)
	if(stringEqual(linecopy, ebScriptName[j] + 1))
	    break;
    if(!ebScript[j]) {
	setError(MSG_NoSuchFunction, linecopy);
	goto fail;
    }

/* skip past the leading \n */
    ip = ebScript[j] + 1;
    nofail = (ebScriptName[j][0] == '+');
    nest = 0;
    ok = true;

/* collect arguments */
    j = 0;
    for(s = t; s; s = t) {
	if(++j >= 10) {
	    setError(MSG_ManyArgs);
	    goto fail;
	}
	args[j] = ++s;
	t = strchr(s, ' ');
	if(t)
	    *t = 0;
	if(argtl)
	    ++argtl;
	argtl += (argl[j] = strlen(s));
    }
    argl[0] = argtl;

    while(code = *ip) {
	if(intFlag) {
	    setError(MSG_Interrupted);
	    goto fail;
	}
	endl = strchr(ip, '\n');

	if(code == 0x83) {
	    ip = balance(ip, 1) + 2;
	    --nest;
	    continue;
	}

	if(code == 0x82) {
	    char control = stack[nest];
	    char ucontrol = toupper(control);
	    const char *start = balance(ip, -1);
	    start = strchr(start, '\n') + 1;
	    if(ucontrol == 'L') {	/* loop */
		if(--loopcnt[nest])
		    ip = start;
		else
		    ip = endl + 1, --nest;
		continue;
	    }
	    if(ucontrol == 'W' || ucontrol == 'U') {
		bool jump = ok;
		if(islowerByte(control))
		    jump ^= true;
		if(ucontrol == 'U')
		    jump ^= true;
		ok = true;
		if(jump)
		    ip = start;
		else
		    ip = endl + 1, --nest;
		continue;
	    }
/* Apparently it's the close of an if or an else, just fall through */
	    goto nextline;
	}

	if(code == 0x81) {
	    const char *skip = balance(ip, 1);
	    bool jump;
	    char control = ip[1];
	    char ucontrol = toupper(control);
	    stack[++nest] = control;
	    if(ucontrol == 'L') {
		loopcnt[nest] = j = atoi(ip + 2);
		if(j)
		    goto nextline;
	      ahead:
		if(*skip == (char)0x82)
		    --nest;
		ip = skip + 2;
		continue;
	    }
	    if(ucontrol == 'U')
		goto nextline;
/* if or while, test on ok */
	    jump = ok;
	    if(isupperByte(control))
		jump ^= true;
	    ok = true;
	    if(jump)
		goto ahead;
	    goto nextline;
	}

	if(!ok && nofail)
	    goto fail;

/* compute length of line, then build the line */
	l = endl - ip;
	for(s = ip; s < endl; ++s)
	    if(*s == '~' && isdigitByte(s[1]))
		l += argl[s[1] - '0'];
	t = new = allocMem(l + 1);
	for(s = ip; s < endl; ++s) {
	    if(*s == '~' && isdigitByte(s[1])) {
		j = *++s - '0';
		if(j) {
		    if(!args[j]) {
			setError(MSG_NoArgument, j);
			nzFree(new);
			goto fail;
		    }
		    strcpy(t, args[j]);
		    t += argl[j];
		    continue;
		}
/* ~0 is all args together */
		for(j = 1; j <= 9 && args[j]; ++j) {
		    if(j > 1)
			*t++ = ' ';
		    strcpy(t, args[j]);
		    t += argl[j];
		}
		continue;
	    }
	    *t++ = *s;
	}
	*t = 0;

/* Here we go! */
	debugPrint(3, "< %s", new);
	ok = edbrowseCommand(new, true);
	free(new);

      nextline:
	ip = endl + 1;
    }

    if(!ok && nofail)
	goto fail;

    nzFree(linecopy);
    return true;

  fail:
    nzFree(linecopy);
    return false;
}				/* runEbFunction */

/* Send the contents of the current buffer to a running program */
bool
bufferToProgram(const char *cmd, const char *suffix, bool trailPercent)
{
    FILE *f;
    char *buf = 0;
    int buflen, n;
    int size1, size2;
    char *u = edbrowseTempFile + strlen(edbrowseTempFile);

    if(!trailPercent) {
/* pipe the buffer into the program */
	f = popen(cmd, "w");
	if(!f) {
	    setError(MSG_NoSpawn, cmd, errno);
	    return false;
	}
	if(!unfoldBuffer(context, false, &buf, &buflen)) {
	    pclose(f);
	    return false;	/* should never happen */
	}
	n = fwrite(buf, buflen, 1, f);
	pclose(f);
    } else {
	sprintf(u, ".%s", suffix);
	size1 = currentBufferSize();
	size2 = fileSizeByName(edbrowseTempFile);
	if(size1 == size2) {
/* assume it's the same data */
	    *u = 0;
	} else {
	    f = fopen(edbrowseTempFile, "w");
	    if(!f) {
		setError(MSG_TempNoCreate2, edbrowseTempFile, errno);
		*u = 0;
		return false;
	    }
	    *u = 0;
	    if(!unfoldBuffer(context, false, &buf, &buflen)) {
		fclose(f);
		return false;	/* should never happen */
	    }
	    n = fwrite(buf, buflen, 1, f);
	    fclose(f);
	}
	system(cmd);
    }

    nzFree(buf);
    return true;
}				/* bufferToProgram */

struct DBTABLE *
findTableDescriptor(const char *sn)
{
    int i;
    struct DBTABLE *td = dbtables;
    for(i = 0; i < maxTables; ++i, ++td)
	if(stringEqual(td->shortname, sn))
	    return td;
    return 0;
}				/* findTableDescriptor */

struct DBTABLE *
newTableDescriptor(const char *name)
{
    struct DBTABLE *td;
    if(maxTables == MAXDBT) {
	setError(MSG_ManyTables, MAXDBT);
	return 0;
    }
    td = dbtables + maxTables++;
    td->name = td->shortname = cloneString(name);
    td->ncols = 0;		/* it's already 0 */
    return td;
}				/* newTableDescriptor */

struct MIMETYPE *
findMimeBySuffix(const char *suffix)
{
    int i;
    int len = strlen(suffix);
    struct MIMETYPE *m = mimetypes;

    for(i = 0; i < maxMime; ++i, ++m) {
	const char *s = m->suffix, *t;
	if(!s)
	    continue;
	while(*s) {
	    t = strchr(s, ',');
	    if(!t)
		t = s + strlen(s);
	    if(t - s == len && memEqualCI(s, suffix, len))
		return m;
	    if(*t)
		++t;
	    s = t;
	}
    }

    return 0;
}				/* findMimeBySuffix */

struct MIMETYPE *
findMimeByProtocol(const char *prot)
{
    int i;
    int len = strlen(prot);
    struct MIMETYPE *m = mimetypes;

    for(i = 0; i < maxMime; ++i, ++m) {
	const char *s = m->prot, *t;
	if(!s)
	    continue;
	while(*s) {
	    t = strchr(s, ',');
	    if(!t)
		t = s + strlen(s);
	    if(t - s == len && memEqualCI(s, prot, len))
		return m;
	    if(*t)
		++t;
	    s = t;
	}
    }

    return 0;
}				/* findMimeByProtocol */

/* The result is allocated */
char *
pluginCommand(const struct MIMETYPE *m, const char *file, const char *suffix)
{
    int len, suflen;
    const char *s;
    char *cmd, *t;
    bool trailPercent = false;

/* leave rooom for space quote quote null */
    len = strlen(m->program) + 4;
    if(file) {
	len += strlen(file);
    } else if(m->program[strlen(m->program) - 1] == '%') {
	trailPercent = true;
	len += strlen(edbrowseTempFile) + 6;
    }

    suflen = 0;
    if(suffix) {
	suflen = strlen(suffix);
	for(s = m->program; *s; ++s)
	    if(*s == '*')
		len += suflen - 1;
    }

    cmd = allocMem(len);
    t = cmd;
    for(s = m->program; *s; ++s) {
	if(suffix && *s == '*') {
	    strcpy(t, suffix);
	    t += suflen;
	} else {
	    *t++ = *s;
	}
    }
    *t = 0;

    if(file) {
	sprintf(t, " \"%s\"", file);
    } else if(trailPercent) {
	sprintf(t - 1, " \"%s.%s\"", edbrowseTempFile, suffix);
    }

    debugPrint(3, "%s", cmd);
    return cmd;
}				/* pluginCommand */
