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

const char *version = "3.2.1";
char *userAgents[10], *currentAgent, *currentReferrer;
const char eol[] = "\r\n";
char EMPTYSTRING[] = "";
int debugLevel = 1;
int webTimeout = 20, mailTimeout = 0;
bool ismc, browseLocal, zapMail, unformatMail, passMail, errorExit;
bool isInteractive, intFlag, inInput;
const char opint[] = "operation interrupted";
int fileSize, maxFileSize = 50000000;
int localAccount, maxAccount;
struct MACCOUNT accounts[MAXACCOUNT];
int maxMime;
struct MIMETYPE mimetypes[MAXMIME];
static int maxTables;
static struct DBTABLE dbtables[MAXDBT];
char *dbarea, *dblogin, *dbpw;	/* to log into the database */
bool caseInsensitive, searchStringsAll;
bool textAreaDosNewlines = true, undoable;
bool allowRedirection = true, allowJS = true, sendReferrer = false;
bool verifyCertificates = true, binaryDetect = true;
char ftpMode;
bool showHiddenFiles, helpMessagesOn;
uchar dirWrite, endMarks;
int context = 1;
uchar linePending[MAXTTYLINE];
char *changeFileName, *mailDir;
char *addressFile, *ipbFile;
char *home, *recycleBin, *configFile, *sigFile;
char *sslCerts, *cookieFile, *edbrowseTempFile, *spamCan;
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
	errorPrint
	   ("@could not rewrite your config file; your configuration data may be lost!");
    close(fh);
}				/* updateConfig */

bool
junkSubject(const char *s, char key)
{
    int l, n;
    char *new;
    long exp = nowday;
    if(!s || !*s) {
	puts("no subject");
	return false;
    }
    if(!cfgcopy) {
	puts("no config file present");
	return false;
    }
    if(!subjstart) {
	puts(".ebrc config file does not contain a subjfilter{...} block");
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

/* This routine succeeds, or aborts via errorPrint */
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
	"webtimer", "mailtimer", "certfile", "database",
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
	    errorPrint("1.ebrc: null characters at line %d", ln);
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
		    errorPrint("1.ebrc: missing function name at line %d", ln);
		if(isdigitByte(*q))
		    errorPrint
		       ("1.ebrc: function name at line %d begins with a digit",
		       ln);
		while(isalnumByte(*q))
		    ++q;
		if(q - last - 9 > 10)
		    errorPrint
		       ("1.ebrc: function name at line %d is too long, limit ten characters",
		       ln);
		if(*q != '{' || q[1])
		    errorPrint
		       ("1.ebrc: improper function definition syntax at line %d",
		       ln);
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
		errorPrint("1.ebrc: line %d, \"condition > file\" expected",
		   ln);
	    while(v > s && (v[-1] == ' ' || v[-1] == '\t'))
		--v;
	    if(v == s)
		errorPrint(".ebrc: line %d, filter rule has no match string",
		   ln);
	    c = *v, *v++ = 0;
	    if(c != '>') {
		while(*v != '>')
		    ++v;
		++v;
	    }
	    while(*v == ' ' || *v == '\t')
		++v;
	    if(!*v)
		errorPrint("1.ebrc: line %d, match on %s is set nowhere", ln,
		   s);
	    if(n_filters == MAXFILTER - 1)
		errorPrint("1.ebrc: line %d, too many mail filters", ln);
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
		errorPrint("1.ebrc: unrecognized keyword %s at line %d", s, ln);
	    *v = c;		/* put it back */
	    goto nokeyword;
	}

	if(n < 8 && mailblock != 1)
	    errorPrint
	       ("1.ebrc: line %d, attribute %s canot be set outside of a mail descriptor",
	       ln, s);

	if(n >= 8 && n < 13 && mimeblock != 1)
	    errorPrint
	       ("1.ebrc: line %d, attribute %s canot be set outside of a mime descriptor",
	       ln, s);

	if(n >= 13 && n < 17 && tabblock != 1)
	    errorPrint
	       ("1.ebrc: line %d, attribute %s canot be set outside of a table descriptor",
	       ln, s);

	if(n >= 8 && mailblock)
	    errorPrint
	       ("1.ebrc: line %d, attribute %s canot be set inside a mail descriptor or filter block",
	       ln, s);

	if((n < 8 || n >= 13) && mimeblock)
	    errorPrint
	       ("1.ebrc: line %d, attribute %s canot be set inside a mime descriptor",
	       ln, s);

	if((n < 13 || n >= 17) && tabblock)
	    errorPrint
	       ("1.ebrc: line %d, attribute %s canot be set inside a table descriptor",
	       ln, s);

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
	    errorPrint("1.ebrc: line %d, attribute %s is set to nothing", ln,
	       s);

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
	    act->inport = atoi(v);
	    continue;

	case 7:
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
		    errorPrint("1.ebrc: line %d, too many columns, limit %d",
		       ln, MAXTCOLS);
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
		errorPrint
		   ("1.ebrc: line %d, keycol should be number or number,number",
		   ln);
	    td->key1 = strtol(v, &v, 10);
	    if(*v == ',' && isdigitByte(v[1]))
		td->key2 = strtol(v + 1, &v, 10);
	    if(td->key1 > td->ncols || td->key2 > td->ncols)
		errorPrint
		   ("1.ebrc: line %d, keycol is out of range; only %d columns specified",
		   ln, td->ncols);
	    continue;

	case 17:
	    addressFile = v;
	    ftype = fileTypeByName(v, false);
	    if(ftype && ftype != 'f')
		errorPrint("1.ebrc: address book %s is not a regular file", v);
	    continue;

	case 18:
	    ipbFile = v;
	    ftype = fileTypeByName(v, false);
	    if(ftype && ftype != 'f')
		errorPrint("1.ebrc: ip blacklist %s is not a regular file", v);
	    continue;

	case 19:
	    mailDir = v;
	    if(fileTypeByName(v, false) != 'd')
		errorPrint("1.ebrc: %s is not a directory", v);
	    continue;

	case 20:
	    for(j = 0; j < 10; ++j)
		if(!userAgents[j])
		    break;
	    if(j == 10)
		errorPrint("1.ebrc: line %d, too many user agents, limit 9",
		   ln);
	    userAgents[j] = v;
	    continue;

	case 21:
	    cookieFile = v;
	    ftype = fileTypeByName(v, false);
	    if(ftype && ftype != 'f')
		errorPrint("1.ebrc: cookie jar %s is not a regular file", v);
	    j = open(v, O_WRONLY | O_APPEND | O_CREAT, 0600);
	    if(j < 0)
		errorPrint("1.ebrc: cannot %s cookie jar %s",
		   ftype ? "create" : "write to", v);
	    close(j);
	    continue;

	case 22:
	    if(javaDisCount == MAXNOJS)
		errorPrint("1.ebrc: too many no js directives, limit %d",
		   MAXNOJS);
	    if(*v == '.')
		++v;
	    q = strchr(v, '.');
	    if(!q || q[1] == 0)
		errorPrint("1.ebrc: line %d, domain %s does not contain a dot",
		   ln, v);
	    javaDis[javaDisCount++] = v;
	    continue;

	case 23:
	    spamCan = v;
	    ftype = fileTypeByName(v, false);
	    if(ftype && ftype != 'f')
		errorPrint("1.ebrc: mail trash can %s is not a regular file",
		   v);
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
		errorPrint
		   ("1.ebrc: SSL certificate file %s is not a regular file", v);
	    j = open(v, O_RDONLY);
	    if(j < 0)
		errorPrint
		   ("1.ebrc: SSL certificate file %s does not exist or is not readable.",
		   v);
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

	default:
	    errorPrint("1.ebrc: line %d, keyword %s is not yet implemented", ln,
	       s);
	}			/* switch */

      nokeyword:

	if(stringEqual(s, "default") && mailblock == 1) {
	    if(localAccount == maxAccount + 1)
		continue;
	    if(localAccount)
		errorPrint("1.ebrc: sets multiple mail accounts as default");
	    localAccount = maxAccount + 1;
	    continue;
	}

	if(*s == '\x82' && s[1] == 0) {
	    if(mailblock == 1) {
		++maxAccount;
		mailblock = 0;
		if(!act->inurl)
		    errorPrint("1.ebrc: missing inserver at line %d", ln);
		if(!act->outurl)
		    errorPrint("1.ebrc: missing outserver at line %d", ln);
		if(!act->login)
		    errorPrint("1.ebrc: missing login at line %d", ln);
		if(!act->password)
		    errorPrint("1.ebrc: missing password at line %d", ln);
		if(!act->from)
		    errorPrint("1.ebrc: missing from at line %d", ln);
		if(!act->reply)
		    errorPrint("1.ebrc: missing reply at line %d", ln);
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
		    errorPrint("1.ebrc: missing type at line %d", ln);
		if(!mt->desc)
		    errorPrint("1.ebrc: missing description at line %d", ln);
		if(!mt->suffix && !mt->prot)
		    errorPrint
		       ("1.ebrc: missing suffix or protocol list at line %d",
		       ln);
		if(!mt->program)
		    errorPrint("1.ebrc: missing program at line %d", ln);
		continue;
	    }

	    if(tabblock == 1) {
		++maxTables;
		tabblock = 0;
		if(!td->name)
		    errorPrint("1.ebrc: missing table name at line %d", ln);
		if(!td->shortname)
		    errorPrint("1.ebrc: missing short name at line %d", ln);
		if(!td->ncols)
		    errorPrint("1.ebrc: missing columns at line %d", ln);
		continue;
	    }

	    if(--nest < 0)
		errorPrint("1.ebrc: unexpected } at line %d", ln);
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
		errorPrint
		   ("1.ebrc: else at line %d is not part of an if statement",
		   ln);
	    goto putback;
	}

	if(*s != '\x81') {
	    if(!nest)
		errorPrint("1.ebrc: garbled text at line %d", ln);
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
	    errorPrint
	       ("1.ebrc: line %d, cannot start a function, mail/mime descriptor, or filter block inside %s",
	       ln, curblock);
	}

	if(!strchr("fmertsb", c) && !nest)
	    errorPrint
	       ("1.ebrc: statement at line %d must appear inside a function",
	       ln);

	if(c == 'm') {
	    mailblock = 1;
	    if(maxAccount == MAXACCOUNT)
		errorPrint
		   ("1too many email accounts in your config file, limit %d",
		   MAXACCOUNT);
	    act = accounts + maxAccount;
	    continue;
	}

	if(c == 'e') {
	    mimeblock = 1;
	    if(maxMime == MAXMIME)
		errorPrint
		   ("1too many mime types in your config file, limit %d",
		   MAXMIME);
	    mt = mimetypes + maxMime;
	    continue;
	}

	if(c == 'b') {
	    tabblock = 1;
	    if(maxTables == MAXDBT)
		errorPrint
		   ("1too many sql tables in your config file, limit %d",
		   MAXDBT);
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
		errorPrint("1too many functions in your config file, limit %d",
		   sn);
	    ebScriptName[sn] = s + 2;
	    t[-1] = 0;
	    ebScript[sn] = t;
	    goto putback;
	}

	if(++nest >= sizeof (stack))
	    errorPrint
	       ("1.ebrc: line %d, control structures are nested too deeply",
	       ln);
	stack[nest] = c;

      putback:
	*t = '\n';
    }				/* loop over lines */

    if(nest)
	errorPrint("1.ebrc: function %s is not closed at eof",
	   ebScriptName[sn]);

    if(mailblock | mimeblock)
	errorPrint("1.ebrc: mail or mime block is not closed at EOF");

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
	printf("interrupt, type qt to quit completely\n");
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
	errorPrint("1home directory not defined by $HOME.");
    if(fileTypeByName(home, false) != 'd')
	errorPrint("1%s is not a directory", home);

/* See sample.ebrc in this directory for a sample config file. */
    configFile = allocMem(strlen(home) + 7);
    sprintf(configFile, "%s/.ebrc", home);

    recycleBin = allocMem(strlen(home) + 10);
    sprintf(recycleBin, "%s/.recycle", home);
    edbrowseTempFile = allocMem(strlen(recycleBin) + 8 + 6);
/* The extra 6 is for the suffix */
    sprintf(edbrowseTempFile, "%s/eb_tmp", recycleBin);
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
		errorPrint
		   ("1no mail accounts specified, please check your .ebrc config file");
	    account = strtol(s + 1, &s, 10);
	    if(account == 0 || account > maxAccount)
		errorPrint("1invalid account number, please use 1 through %d",
		   maxAccount);
	    if(!*s) {
		ismc = true;	/* running as a mail client */
		allowJS = false;	/* no javascript in mail client */
		++argv, --argc;	/* we're going to break out */
		if(argc && stringEqual(argv[0], "-Zap"))
		    ++argv, --argc, zapMail = true;
		break;
	    }
	}
	errorPrint("1edbrowse  -v    (show version)\n\
edbrowse -h (this message)\n\
edbrowse -c (edit config file)\n\
edbrowse  [-e] [-d?] -[u|p]m?    (read your mail) \n\
edbrowse  [-e] [-d?] -m? address1 address2 ... file [+attachments]\n\
edbrowse  [-e] [-d?] file1 file2 ...");
    }				/* options */

    if(tcp_init() < 0)
	debugPrint(4, "tcp failure, could not identify this machine");
    else
	debugPrint(4, "host info established for %s, %s",
	   tcp_thisMachineName, tcp_thisMachineDots);

    ssl_init(doConfig);

    srand(time(0));

    loadBlacklist();

    if(ismc) {
	char **reclist, **atlist;
	char *s, *body;
	int nat, nalt, nrec;
	if(!argc)
	    fetchMail(account);
	if(argc == 1)
	    errorPrint
	       ("1please specify at least one recipient and the file to send");
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
	    errorPrint
	       ("1please specify at least one recipient and the file to send, before your attachments");
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
	    errorPrint("1too many files open simultaneously, limit %d",
	       MAXSESSION);
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
	printf("edbrowse ready\n");
    }
    if(cx > 1)
	cxSwitch(1, false);
    undoable = false;

    while(true) {
	uchar saveline[MAXTTYLINE];
	pst p = inputLine();
	copyPstring(saveline, p);
	if(perl2c((char *)p))
	    printf
	       ("entered command line contains nulls; you can use \\0 in a search/replace string to indicate null\n");
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
	setError("no function specified");
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
	    setError("function name should only contain letters and numbers");
	    goto fail;
	}
    for(j = 0; ebScript[j]; ++j)
	if(stringEqual(linecopy, ebScriptName[j] + 1))
	    break;
    if(!ebScript[j]) {
	setError("no such function %s", linecopy);
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
	    setError("too many arguments");
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
	    setError(opint);
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
	    setError("could not spawn subcommand %s, errno %d", cmd, errno);
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
		setError("could not create temp file %s, errno %d",
		   edbrowseTempFile, errno);
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
	setError("too many sql tables in cache, limit %d", MAXDBT);
	return 0;
    }
    td = dbtables + maxTables++;
    td->name = td->shortname = cloneString(name);
    td->ncols = 0; /* it's already 0 */
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
