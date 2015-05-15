/* main.c
 * Entry point, arguments and options.
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"

#include <sys/stat.h>
#include <pcre.h>

/* Define the globals that are declared in eb.h. */
/* See eb.h for descriptive comments. */

const char *version = "3.5.4+";
char *userAgents[10], *currentAgent, *currentReferrer;
const char eol[] = "\r\n";
int jsPool = 32;
int webTimeout = 20, mailTimeout = 0;
char *sslCerts;
int verifyCertificates = 1;
bool ismc, isimap, passMail;
bool browseLocal;
int browseLine;			/* line number for errors */
bool inInput, listNA;
volatile bool intFlag;
int fileSize;
int localAccount, maxAccount;
struct MACCOUNT accounts[MAXACCOUNT];
int maxMime;
struct MIMETYPE mimetypes[MAXMIME];
int maxproxy;
struct PXENT proxyEntries[MAXPROXY];
static int maxTables;
static struct DBTABLE dbtables[MAXDBT];
char *dbarea, *dblogin, *dbpw;	/* to log into the database */
bool fetchBlobColumns;
bool caseInsensitive, searchStringsAll;
bool allowRedirection = true, allowJS = true, sendReferrer = true;
bool binaryDetect = true, pluginsOn = true;
bool inputReadLine;
uchar dirWrite, endMarks;
int context = 1;
uchar linePending[MAXTTYLINE];
char *changeFileName, *mailDir;
char *mailUnread, *mailStash, *mailReply;
char *addressFile;
char *recycleBin, *configFile, *sigFile, *sigFileEnd;
char *cookieFile;
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

static void setNowDay(void)
{
	time_t now;
	time(&now);
	now /= (60 * 60 * 24);	/* convert to days */
	now -= 30 * 365;
	now -= 7;		/* leap years */
	nowday = now;
}				/* setNowDay */

/* This routine succeeds, or aborts via i_printfExit */
static void readConfigFile(void)
{
	char *buf, *s, *t, *v, *q;
	int buflen, n;
	char c, ftype;
	bool cmt = false;
	bool startline = true;
	uchar mailblock = 0;
	bool mimeblock = false, tabblock = false;
	int nest, ln, j;
	int sn = 0;		/* script number */
	char stack[MAXNEST];
	char last[24];
	int lidx = 0;
	struct MACCOUNT *act;
	struct PXENT *px;
	struct MIMETYPE *mt;
	struct DBTABLE *td;

/* Order is important here: mail{}, mime{}, table{}, then global keywords */
#define MAILWORDS 0
#define MIMEWORDS 8
#define TABLEWORDS 15
#define GLOBALWORDS 19
	static const char *const keywords[] = {
		"inserver", "outserver", "login", "password", "from", "reply",
		"inport", "outport",
		"type", "desc", "suffix", "protocol", "program",
		"content", "outtype",
		"tname", "tshort", "cols", "keycol",
		"adbook", "downdir", "maildir", "agent",
		"jar", "nojs", "xyz@xyz",
		"webtimer", "mailtimer", "certfile", "datasource", "proxy",
		"linelength", "localizeweb", "jspool", "novs",
		0
	};

	if (!fileTypeByName(configFile, false))
		return;		/* config file not present */
	if (!fileIntoMemory(configFile, &buf, &buflen))
		showErrorAbort();
/* An extra newline won't hurt. */
	if (buflen && buf[buflen - 1] != '\n')
		buf[buflen++] = '\n';

/* Undos, uncomment, watch for nulls */
/* Encode mail{ as hex 81 m, and other encodings. */
	ln = 1;
	for (s = t = v = buf; s < buf + buflen; ++s) {
		c = *s;
		if (c == '\0')
			i_printfExit(MSG_EBRC_Nulls, ln);
		if (c == '\r' && s[1] == '\n')
			continue;

		if (cmt) {
			if (c != '\n')
				continue;
			cmt = false;
		}

		if (c == '#' && startline) {
			cmt = true;
			goto putc;
		}

		if (c == '\n') {
			last[lidx] = 0;
			lidx = 0;
			if (stringEqual(last, "}")) {
				*v = '\x82';
				t = v + 1;
			}
			if (stringEqual(last, "}else{")) {
				*v = '\x83';
				t = v + 1;
			}
			if (stringEqual(last, "mail{")) {
				*v = '\x81';
				v[1] = 'm';
				t = v + 2;
			}
			if (stringEqual(last, "plugin{") ||
			    stringEqual(last, "mime{")) {
				*v = '\x81';
				v[1] = 'e';
				t = v + 2;
			}
			if (stringEqual(last, "table{")) {
				*v = '\x81';
				v[1] = 'b';
				t = v + 2;
			}
			if (stringEqual(last, "fromfilter{")) {
				*v = '\x81';
				v[1] = 'r';
				t = v + 2;
			}
			if (stringEqual(last, "tofilter{")) {
				*v = '\x81';
				v[1] = 't';
				t = v + 2;
			}
			if (stringEqual(last, "subjfilter{")) {
				*v = '\x81';
				v[1] = 's';
				t = v + 2;
			}
			if (stringEqual(last, "if(*){")) {
				*v = '\x81';
				v[1] = 'I';
				t = v + 2;
			}
			if (stringEqual(last, "if(?){")) {
				*v = '\x81';
				v[1] = 'i';
				t = v + 2;
			}
			if (stringEqual(last, "while(*){")) {
				*v = '\x81';
				v[1] = 'W';
				t = v + 2;
			}
			if (stringEqual(last, "while(?){")) {
				*v = '\x81';
				v[1] = 'w';
				t = v + 2;
			}
			if (stringEqual(last, "until(*){")) {
				*v = '\x81';
				v[1] = 'U';
				t = v + 2;
			}
			if (stringEqual(last, "until(?){")) {
				*v = '\x81';
				v[1] = 'u';
				t = v + 2;
			}
			if (!strncmp(last, "loop(", 5) && isdigitByte(last[5])) {
				q = last + 6;
				while (isdigitByte(*q))
					++q;
				if (stringEqual(q, "){")) {
					*q = 0;
					last[4] = 'l';
					last[3] = '\x81';
					strcpy(v, last + 3);
					t = v + strlen(v);
				}
			}
			if (!strncmp(last, "function", 8) &&
			    (last[8] == '+' || last[8] == ':')) {
				q = last + 9;
				if (*q == 0 || *q == '{' || *q == '(')
					i_printfExit(MSG_EBRC_NoFnName, ln);
				if (isdigitByte(*q))
					i_printfExit(MSG_EBRC_FnDigit, ln);
				while (isalnumByte(*q))
					++q;
				if (q - last - 9 > 10)
					i_printfExit(MSG_EBRC_FnTooLong, ln);
				if (*q != '{' || q[1])
					i_printfExit(MSG_EBRC_SyntaxErr, ln);
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

		if (c == ' ' || c == '\t') {
			if (startline)
				continue;
		} else {
			if (lidx < sizeof(last) - 1)
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

	for (s = buf; *s; s = t + 1, ++ln) {
		t = strchr(s, '\n');
		if (t == s)
			continue;	/* empty line */
		if (t == s + 1 && *s == '#')
			continue;	/* comment */
		*t = 0;		/* I'll put it back later */

/* Gather the filters in a mail filter block */
		if (mailblock > 1 && !strchr("\x81\x82\x83", *s)) {
			v = strchr(s, '>');
			if (!v)
				i_printfExit(MSG_EBRC_NoCondFile, ln);
			while (v > s && (v[-1] == ' ' || v[-1] == '\t'))
				--v;
			if (v == s)
				i_printfExit(MSG_EBRC_NoMatchStr, ln);
			c = *v, *v++ = 0;
			if (c != '>') {
				while (*v != '>')
					++v;
				++v;
			}
			while (*v == ' ' || *v == '\t')
				++v;
			if (!*v)
				i_printfExit(MSG_EBRC_MatchNowh, ln, s);
			if (n_filters == MAXFILTER - 1)
				i_printfExit(MSG_EBRC_Filters, ln);
			filters[n_filters].match = s;
			filters[n_filters].redirect = v;
			filters[n_filters].type = mailblock;
			++n_filters;
			continue;
		}

		v = strchr(s, '=');
		if (!v)
			goto nokeyword;

		while (v > s && (v[-1] == ' ' || v[-1] == '\t'))
			--v;
		if (v == s)
			goto nokeyword;
		c = *v, *v = 0;
		for (q = s; q < v; ++q)
			if (!isalphaByte(*q)) {
				*v = c;
				goto nokeyword;
			}

		n = stringInList(keywords, s);
		if (n < 0) {
			if (!nest)
				i_printfExit(MSG_EBRC_BadKeyword, s, ln);
			*v = c;	/* put it back */
			goto nokeyword;
		}

		if (nest)
			i_printfExit(MSG_EBRC_KeyInFunc, ln);

		if (n < MIMEWORDS && mailblock != 1)
			i_printfExit(MSG_EBRC_MailAttrOut, ln, s);

		if (n >= MIMEWORDS && n < TABLEWORDS && !mimeblock)
			i_printfExit(MSG_EBRC_MimeAttrOut, ln, s);

		if (n >= TABLEWORDS && n < GLOBALWORDS && !tabblock)
			i_printfExit(MSG_EBRC_TableAttrOut, ln, s);

		if (n >= MIMEWORDS && mailblock)
			i_printfExit(MSG_EBRC_MailAttrIn, ln, s);

		if ((n < MIMEWORDS || n >= TABLEWORDS) && mimeblock)
			i_printfExit(MSG_EBRC_MimeAttrIn, ln, s);

		if ((n < TABLEWORDS || n >= GLOBALWORDS) && tabblock)
			i_printfExit(MSG_EBRC_TableAttrIn, ln, s);

/* act upon the keywords */
		++v;
		if (c != '=') {
			while (*v != '=')
				++v;
			++v;
		}
		while (*v == ' ' || *v == '\t')
			++v;
		if (!*v)
			i_printfExit(MSG_EBRC_NoAttr, ln, s);

		switch (n) {
		case 0:	/* inserver */
			act->inurl = v;
			continue;

		case 1:	/* outserver */
			act->outurl = v;
			continue;

		case 2:	/* login */
			act->login = v;
			continue;

		case 3:	/* password */
			act->password = v;
			continue;

		case 4:	/* from */
			act->from = v;
			continue;

		case 5:	/* reply */
			act->reply = v;
			continue;

		case 6:	/* inport */
			if (*v == '*')
				act->inssl = 1, ++v;
			act->inport = atoi(v);
			continue;

		case 7:	/* outport */
			if (*v == '+')
				act->outssl = 4, ++v;
			if (*v == '^')
				act->outssl = 2, ++v;
			if (*v == '*')
				act->outssl = 1, ++v;
			act->outport = atoi(v);
			continue;

		case 8:	/* type */
			mt->type = v;
			continue;

		case 9:	/* desc */
			mt->desc = v;
			continue;

		case 10:	/* suffix */
			mt->suffix = v;
			continue;

		case 11:	/* protocol */
			mt->prot = v;
			mt->stream = true;
			continue;

		case 12:	/* program */
			mt->program = v;
			continue;

		case 13:	/* content */
			mt->content = v;
			continue;

		case 14:	/* outtype */
			c = tolower(*v);
			if (c != 'h' && c != 't')
				i_printfExit(MSG_EBRC_Outtype, ln);
			mt->outtype = c;
			continue;

		case 15:	/* tname */
			td->name = v;
			continue;

		case 16:	/* tshort */
			td->shortname = v;
			continue;

		case 17:	/* cols */
			while (*v) {
				if (td->ncols == MAXTCOLS)
					i_printfExit(MSG_EBRC_ManyCols, ln,
						     MAXTCOLS);
				td->cols[td->ncols++] = v;
				q = strchr(v, ',');
				if (!q)
					break;
				*q = 0;
				v = q + 1;
			}
			continue;

		case 18:	/* keycol */
			if (!isdigitByte(*v))
				i_printfExit(MSG_EBRC_KeyNotNb, ln);
			td->key1 = strtol(v, &v, 10);
			if (*v == ',' && isdigitByte(v[1]))
				td->key2 = strtol(v + 1, &v, 10);
			if (td->key1 > td->ncols || td->key2 > td->ncols)
				i_printfExit(MSG_EBRC_KeyOutRange, ln,
					     td->ncols);
			continue;

		case 19:	/* adbook */
			addressFile = v;
			ftype = fileTypeByName(v, false);
			if (ftype && ftype != 'f')
				i_printfExit(MSG_EBRC_AbNotFile, v);
			continue;

		case 20:	/* downdir */
			downDir = v;
			if (fileTypeByName(v, false) != 'd')
				i_printfExit(MSG_EBRC_NotDir, v);
			continue;

		case 21:	/* maildir */
			mailDir = v;
			if (fileTypeByName(v, false) != 'd')
				i_printfExit(MSG_EBRC_NotDir, v);
			mailUnread = allocMem(strlen(v) + 20);
			sprintf(mailUnread, "%s/unread", v);
/* We need the unread directory, else we can't fetch mail. */
/* Create it if it isn't there. */
			if (fileTypeByName(mailUnread, false) != 'd') {
				if (mkdir(mailUnread, 0700))
					i_printfExit(MSG_EBRC_NotDir,
						     mailUnread);
			}
			mailReply = allocMem(strlen(v) + 20);
			sprintf(mailReply, "%s/.reply", v);
			continue;

		case 22:	/* agent */
			for (j = 0; j < 10; ++j)
				if (!userAgents[j])
					break;
			if (j == 10)
				i_printfExit(MSG_EBRC_ManyAgents, ln);
			userAgents[j] = v;
			continue;

		case 23:	/* jar */
			cookieFile = v;
			ftype = fileTypeByName(v, false);
			if (ftype && ftype != 'f')
				i_printfExit(MSG_EBRC_JarNotFile, v);
			j = open(v, O_WRONLY | O_APPEND | O_CREAT, 0600);
			if (j < 0)
				i_printfExit(MSG_EBRC_JarNoWrite, v);
			close(j);
			continue;

		case 24:	/* nojs */
			if (javaDisCount == MAXNOJS)
				i_printfExit(MSG_EBRC_NoJS, MAXNOJS);
			if (*v == '.')
				++v;
			q = strchr(v, '.');
			if (!q || q[1] == 0)
				i_printfExit(MSG_EBRC_DomainDot, ln, v);
			javaDis[javaDisCount++] = v;
			continue;

		case 26:	/* webtimer */
			webTimeout = atoi(v);
			continue;

		case 27:	/* mailtimer */
			mailTimeout = atoi(v);
			continue;

		case 28:	/* certfile */
			sslCerts = v;
			ftype = fileTypeByName(v, false);
			if (ftype && ftype != 'f')
				i_printfExit(MSG_EBRC_SSLNoFile, v);
			j = open(v, O_RDONLY);
			if (j < 0)
				i_printfExit(MSG_EBRC_SSLNoRead, v);
			close(j);
			continue;

		case 29:	/* datasource */
			setDataSource(v);
			continue;

		case 30:	/* proxy */
			if (maxproxy == MAXPROXY)
				i_printfExit(MSG_EBRC_NoPROXY, MAXPROXY);
			px = proxyEntries + maxproxy;
			maxproxy++;
			spaceCrunch(v, true, true);
			q = strchr(v, ' ');
			if (q) {
				*q = 0;
				if (!stringEqual(v, "*"))
					px->prot = v;
				v = q + 1;
				q = strchr(v, ' ');
				if (q) {
					*q = 0;
					if (!stringEqual(v, "*"))
						px->domain = v;
					v = q + 1;
				}
			}
			if (!stringEqualCI(v, "direct"))
				px->proxy = v;
			continue;

		case 31:	/* linelength */
			displayLength = atoi(v);
			if (displayLength < 80)
				displayLength = 80;
			continue;

		case 32:	/* localizeweb */
/* We should probably allow autodetection of language. */
/* E.G., the keyword auto indicates that you want autodetection. */
			setHTTPLanguage(v);
			continue;

		case 33:	/* jspool */
			jsPool = atoi(v);
			if (jsPool < 2)
				jsPool = 2;
			if (jsPool > 1000)
				jsPool = 1000;
			continue;

		case 34:	/* novs */
			if (*v == '.')
				++v;
			q = strchr(v, '.');
			if (!q || q[1] == 0)
				i_printfExit(MSG_EBRC_DomainDot, ln, v);
			addNovsHost(v);
			continue;

		default:
			i_printfExit(MSG_EBRC_KeywordNYI, ln, s);
		}		/* switch */

nokeyword:

		if (stringEqual(s, "default") && mailblock == 1) {
			if (localAccount == maxAccount + 1)
				continue;
			if (localAccount)
				i_printfExit(MSG_EBRC_SevDefaults);
			localAccount = maxAccount + 1;
			continue;
		}

		if (stringEqual(s, "nofetch") && mailblock == 1) {
			act->nofetch = true;
			continue;
		}

		if (stringEqual(s, "secure") && mailblock == 1) {
			act->secure = true;
			continue;
		}

		if (stringEqual(s, "imap") && mailblock == 1) {
			act->imap = act->nofetch = true;
			continue;
		}

		if (stringEqual(s, "download") && mimeblock == 1) {
			mt->download = true;
			continue;
		}
		if (stringEqual(s, "stream") && mimeblock == 1) {
			mt->stream = true;
			continue;
		}

		if (*s == '\x82' && s[1] == 0) {
			if (mailblock == 1) {
				++maxAccount;
				mailblock = 0;
				if (!act->inurl)
					i_printfExit(MSG_EBRC_NoInserver, ln);
				if (!act->outurl)
					i_printfExit(MSG_EBRC_NoOutserver, ln);
				if (!act->login)
					i_printfExit(MSG_EBRC_NoLogin, ln);
				if (!act->password)
					i_printfExit(MSG_EBRC_NPasswd, ln);
				if (!act->from)
					i_printfExit(MSG_EBRC_NoFrom, ln);
				if (!act->reply)
					i_printfExit(MSG_EBRC_NoReply, ln);
				if (act->secure)
					act->inssl = act->outssl = 1;
				if (!act->inport)
					if (act->secure)
						act->inport =
						    (act->imap ? 993 : 995);
					else
						act->inport =
						    (act->imap ? 220 : 110);
				if (!act->outport)
					act->outport = (act->secure ? 465 : 25);
				continue;
			}

			if (mailblock) {
				mailblock = 0;
				continue;
			}

			if (mimeblock) {
				++maxMime;
				mimeblock = false;
				if (!mt->type)
					i_printfExit(MSG_EBRC_NoType, ln);
				if (!mt->desc)
					i_printfExit(MSG_EBRC_NDesc, ln);
				if (!mt->suffix && !mt->prot)
					i_printfExit(MSG_EBRC_NoSuffix, ln);
				if (!mt->program)
					i_printfExit(MSG_EBRC_NoProgram, ln);
				continue;
			}

			if (tabblock) {
				++maxTables;
				tabblock = false;
				if (!td->name)
					i_printfExit(MSG_EBRC_NoTblName, ln);
				if (!td->shortname)
					i_printfExit(MSG_EBRC_NoShortName, ln);
				if (!td->ncols)
					i_printfExit(MSG_EBRC_NColumns, ln);
				continue;
			}

			if (--nest < 0)
				i_printfExit(MSG_EBRC_UnexpBrace, ln);
			if (nest)
				goto putback;
/* This ends the function */
			*s = 0;	/* null terminate the script */
			++sn;
			continue;
		}

		if (*s == '\x83' && s[1] == 0) {
/* Does else make sense here? */
			c = toupper(stack[nest]);
			if (c != 'I')
				i_printfExit(MSG_EBRC_UnexElse, ln);
			goto putback;
		}

		if (*s != '\x81') {
			if (!nest)
				i_printfExit(MSG_EBRC_GarblText, ln);
			goto putback;
		}

/* Starting something */
		c = s[1];
		if ((nest || mailblock || mimeblock) && strchr("fmerts", c)) {
			const char *curblock = "another function";
			if (mailblock)
				curblock = "a mail descriptor";
			if (mailblock > 1)
				curblock = "a filter block";
			if (mimeblock)
				curblock = "a plugin descriptor";
			i_printfExit(MSG_EBRC_FnNotStart, ln, curblock);
		}

		if (!strchr("fmertsb", c) && !nest)
			i_printfExit(MSG_EBRC_StatNotInFn, ln);

		if (c == 'm') {
			mailblock = 1;
			if (maxAccount == MAXACCOUNT)
				i_printfExit(MSG_EBRC_ManyAcc, MAXACCOUNT);
			act = accounts + maxAccount;
			continue;
		}

		if (c == 'e') {
			mimeblock = true;
			if (maxMime == MAXMIME)
				i_printfExit(MSG_EBRC_ManyTypes, MAXMIME);
			mt = mimetypes + maxMime;
			continue;
		}

		if (c == 'b') {
			tabblock = true;
			if (maxTables == MAXDBT)
				i_printfExit(MSG_EBRC_ManyTables, MAXDBT);
			td = dbtables + maxTables;
			continue;
		}

		if (c == 'r') {
			mailblock = 2;
			continue;
		}

		if (c == 't') {
			mailblock = 3;
			continue;
		}

		if (c == 's') {
			mailblock = 4;
			continue;
		}

		if (c == 'f') {
			stack[++nest] = c;
			if (sn == MAXEBSCRIPT)
				i_printfExit(MSG_EBRC_ManyFn, sn);
			ebScriptName[sn] = s + 2;
			t[-1] = 0;
			ebScript[sn] = t;
			goto putback;
		}

		if (++nest >= sizeof(stack))
			i_printfExit(MSG_EBRC_TooDeeply, ln);
		stack[nest] = c;

putback:
		*t = '\n';
	}			/* loop over lines */

	if (nest)
		i_printfExit(MSG_EBRC_FnNotClosed, ebScriptName[sn]);

	if (mailblock | mimeblock)
		i_printfExit(MSG_EBRC_MNotClosed);
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

const char *mailRedirect(const char *to, const char *from,
			 const char *reply, const char *subj)
{
	int rlen = strlen(reply);
	int slen = strlen(subj);
	int tlen = strlen(to);
	struct FILTERDESC *f;
	const char *r;

	for (f = filters; f->match; ++f) {
		const char *m = f->match;
		int mlen = strlen(m);
		r = f->redirect;
		int j, k;

		switch (f->type) {
		case 2:
			if (stringEqualCI(m, from))
				return r;
			if (stringEqualCI(m, reply))
				return r;
			if (*m == '@' && mlen < rlen &&
			    stringEqualCI(m, reply + rlen - mlen))
				return r;
			break;

		case 3:
			if (stringEqualCI(m, to))
				return r;
			if (*m == '@' && mlen < tlen
			    && stringEqualCI(m, to + tlen - mlen))
				return r;
			break;

		case 4:
			if (stringEqualCI(m, subj))
				return r;
/* a prefix match is ok */
			if (slen < 16 || mlen < 16)
				break;	/* too short */
			j = k = 0;
			while (true) {
				char c = subj[j];
				char d = m[k];
				if (isupperByte(c))
					c = tolower(c);
				if (isupperByte(d))
					d = tolower(d);
				if (!c || !d)
					break;
				if (c != d)
					break;
				for (++j; c == subj[j]; ++j) ;
				for (++k; d == m[k]; ++k) ;
			}
/* must match at least 2/3 of either string */
			if (k > j)
				j = k;
			if (j >= 2 * mlen / 3 || j >= 2 * slen / 3) {
				return r;
			}
			break;
		}		/* switch */
	}			/* loop */

	r = reverseAlias(reply);
	return r;
}				/* mailRedirect */

/*********************************************************************
Are we ok to parse and execute javascript?
*********************************************************************/

bool javaOK(const char *url)
{
	int j, hl, dl;
	const char *h, *d, *q, *path;
	if (!allowJS)
		return false;
	if (!url)
		return true;
	if (isDataURI(url))
		return true;
	h = getHostURL(url);
	if (!h)
		return true;
	hl = strlen(h);
	path = getDataURL(url);
	for (j = 0; j < javaDisCount; ++j) {
		d = javaDis[j];
		q = strchr(d, '/');
		if (!q)
			q = d + strlen(d);
		dl = q - d;
		if (dl > hl)
			continue;
		if (!memEqualCI(d, h + hl - dl, dl))
			continue;
		if (*q == '/') {
			++q;
			if (hl != dl)
				continue;
			if (!path)
				continue;
			if (strncmp(q, path, strlen(q)))
				continue;
			return false;
		}		/* domain/path was specified */
		if (hl == dl)
			return false;
		if (h[hl - dl - 1] == '.')
			return false;
	}
	return true;
}				/* javaOK */

/* Catch interrupt and react appropriately. */
static void catchSig(int n)
{
	intFlag = true;
	if (inInput)
		i_puts(MSG_EnterInterrupt);
/* If we were reading from a file, or socket, this signal should
 * cause the read to fail.  Check for intFlag, so we know it was
 * interrupted, and not an io failure.
 * Then clean up appropriately. */
	signal(SIGINT, catchSig);
}				/* catchSig */

void ebClose(int n)
{
	bg_jobs(true);
	dbClose();
	js_shutdown();
/* We should call curl_global_cleanup() here, for clarity and completeness,
 * but it can cause a seg fault when combined with older versions of open ssl,
 * and the process is going to exit anyways, so don't worry about it. */
	exit(n);
}				/* ebClose */

bool isSQL(const char *s)
{
	char c;
	const char *c1 = 0, *c2 = 0;
	c = *s;

	if (!sqlPresent)
		goto no;

	if (isURL(s))
		goto no;

	if (!isalphaByte(c))
		goto no;

	for (++s; c = *s; ++s) {
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
		if (c == ']') {
			c2 = s;
			goto yes;
		}
	}

no:
	return false;

yes:
	return true;
}				/* isSQL */

void setDataSource(char *v)
{
	dbarea = dblogin = dbpw = 0;
	if (!v)
		return;
	if (!*v)
		return;
	dbarea = v;
	v = strchr(v, ',');
	if (!v)
		return;
	*v++ = 0;
	dblogin = v;
	v = strchr(v, ',');
	if (!v)
		return;
	*v++ = 0;
	dbpw = v;
}				/* setDataSource */

static void eb_curl_global_init(void)
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
	version_data = curl_version_info(CURLVERSION_NOW);
	if (version_data->version_num < least_acceptable_version)
		i_printfExit(MSG_CurlVersion, major, minor, patch);
}				/* eb_curl_global_init */

/* I'm not going to expand wild card arguments here.
 * I don't need to on Unix, and on Windows there is a
 * setargv.obj, or something like that, that performs the expansion.
 * I'll assume you have folded that object into libc.lib.
 * So now you can edit *.c, on any operating system,
 * and it will do the right thing, with no work on my part. */

int main(int argc, char **argv)
{
	int cx, account;
	bool rc, doConfig = true;
	bool dofetch = false, domail = false;

/* In case this is being piped over to a synthesizer, or whatever. */
	if (fileTypeByHandle(fileno(stdout)) != 'f')
		setlinebuf(stdout);

	selectLanguage();

	eb_curl_global_init();
	ttySaveSettings();
	initializeReadline();

/* Let's everybody use my malloc and free routines */
	pcre_malloc = allocMem;
	pcre_free = nzFree;

/* Establish the home directory, and standard edbrowse files thereunder. */
	home = getenv("HOME");
/* Empty is the same as missing. */
	if (home && !*home)
		home = 0;
/* I require this, though I'm not sure what this means for non-Unix OS's */
	if (!home)
		i_printfExit(MSG_NotHome);
	if (fileTypeByName(home, false) != 'd')
		i_printfExit(MSG_NotDir, home);

/* See sample.ebrc in this directory for a sample config file. */
	configFile = allocMem(strlen(home) + 7);
	sprintf(configFile, "%s/.ebrc", home);

	recycleBin = allocMem(strlen(home) + 8);
	sprintf(recycleBin, "%s/.Trash", home);
	if (fileTypeByName(recycleBin, false) != 'd') {
		if (mkdir(recycleBin, 0700)) {
/* Don't want to abort here; we might be on a readonly filesystem.
 * Don't have a Trash directory and can't creat one; yet we should move on. */
			free(recycleBin);
			recycleBin = 0;
		}
	}

	if (recycleBin) {
		mailStash = allocMem(strlen(recycleBin) + 12);
		sprintf(mailStash, "%s/rawmail", recycleBin);
		if (fileTypeByName(mailStash, false) != 'd') {
			if (mkdir(mailStash, 0700)) {
				free(mailStash);
				mailStash = 0;
			}
		}
	}

	sigFile = allocMem(strlen(home) + 20);
	sprintf(sigFile, "%s/.signature", home);
	sigFileEnd = sigFile + strlen(sigFile);

	{
		static char agent0[32] = "edbrowse/";
		strcat(agent0, version);
		userAgents[0] = currentAgent = agent0;
	}

	setNowDay();

	++argv, --argc;
	if (argc && stringEqual(argv[0], "-c")) {
		if (argc == 1)
			*argv = configFile;
		else
			++argv, --argc;
		doConfig = false;
	} else {
		readConfigFile();
		if (maxAccount && !localAccount)
			localAccount = 1;
	}
	account = localAccount;

	for (; argc && argv[0][0] == '-'; ++argv, --argc) {
		char *s = *argv;
		++s;

		if (stringEqual(s, "v")) {
			puts(version);
			exit(0);
		}

		if (stringEqual(s, "d")) {
			debugLevel = 4;
			continue;
		}

		if (*s == 'd' && isdigitByte(s[1]) && !s[2]) {
			debugLevel = s[1] - '0';
			continue;
		}

		if (stringEqual(s, "e")) {
			errorExit = true;
			continue;
		}

		if (*s == 'p')
			++s, passMail = true;

		if (*s == 'm' || *s == 'f') {
			if (!maxAccount)
				i_printfExit(MSG_NoMailAcc);
			if (*s == 'f') {
				account = 0;
				dofetch = true;
				++s;
				if (*s == 'm')
					domail = true, ++s;
			} else {
				domail = true;
				++s;
			}
			if (isdigitByte(*s)) {
				account = strtol(s, &s, 10);
				if (account == 0 || account > maxAccount)
					i_printfExit(MSG_BadAccNb, maxAccount);
			}
			if (!*s) {
				ismc = true;	/* running as a mail client */
				allowJS = false;	/* no javascript in mail client */
				++argv, --argc;
				if (!argc || !dofetch)
					break;
			}
		}

		i_printfExit(MSG_Usage);
	}			/* options */

	if (!sslCerts) {
		verifyCertificates = 0;
		if (doConfig)
			if (debugLevel >= 1)
				i_puts(MSG_NoCertFile);
	}

	srand(time(0));

	if (ismc) {
		char **reclist, **atlist;
		char *s, *body;
		int nat, nalt, nrec;

		if (!argc) {
/* This is fetch / read mode */
			if (dofetch) {
				int nfetch = 0;
				if (account) {
					isimap = accounts[account - 1].imap;
					if (isimap)
						domail = false;
					nfetch = fetchMail(account);
				} else {
					nfetch = fetchAllMail();
				}
				if (!domail) {
					if (nfetch)
						i_printf(MSG_MessagesX, nfetch);
					else
						i_puts(MSG_NoMail);
				}
			}

			if (domail) {
				scanMail();
			}

			exit(0);
		}

/* now in sendmail mode */
		if (argc == 1)
			i_printfExit(MSG_MinOneRec);
/* I don't know that argv[argc] is 0, or that I can set it to 0,
 * so I back everything up by 1. */
		reclist = argv - 1;
		for (nat = nalt = 0; nat < argc; ++nat) {
			s = argv[argc - 1 - nat];
			if (*s != '+' && *s != '-')
				break;
			if (*s == '-')
				++nalt;
			strmove(s, s + 1);
		}
		atlist = argv + argc - nat - 1;
		if (atlist <= argv)
			i_printfExit(MSG_MinOneRecBefAtt);
		body = *atlist;
		if (nat)
			memmove(atlist, atlist + 1, sizeof(char *) * nat);
		atlist[nat] = 0;
		nrec = atlist - argv;
		memmove(reclist, reclist + 1, sizeof(char *) * nrec);
		atlist[-1] = 0;
		if (sendMail(account, (const char **)reclist, body, 1,
			     (const char **)atlist, 0, nalt, true))
			exit(0);
		showError();
		exit(1);
	}

	cookiesFromJar();
	http_curl_init();

	signal(SIGINT, catchSig);
	siginterrupt(SIGINT, 1);
	signal(SIGPIPE, SIG_IGN);

	cx = 0;
	while (argc) {
		char *file = *argv;
		++cx;
		if (cx == MAXSESSION)
			i_printfExit(MSG_ManyOpen, MAXSESSION);
		cxSwitch(cx, false);
		if (cx == 1)
			runEbFunction("init");
		changeFileName = 0;
		cw->fileName = cloneString(file);
		cw->firstURL = cloneString(file);
		if (isSQL(file))
			cw->sqlMode = true;
		rc = readFileArgv(file);
		if (fileSize >= 0)
			debugPrint(1, "%d", fileSize);
		fileSize = -1;
		if (!rc) {
			showError();
		} else if (changeFileName) {
			nzFree(cw->fileName);
			cw->fileName = changeFileName;
			changeFileName = 0;
		}

		cw->undoable = cw->changeMode = false;
/* Browse the text if it's a url */
		if (rc && isURL(cw->fileName) &&
		    (cw->mt && cw->mt->outtype
		     || isBrowseableURL(cw->fileName))) {
			if (runCommand("b"))
				debugPrint(1, "%d", fileSize);
			else
				showError();
		}
		++argv, --argc;
	}			/* loop over files */
	if (!cx) {		/* no files */
		++cx;
		cxSwitch(cx, false);
		runEbFunction("init");
		i_puts(MSG_Ready);
	}
	if (cx > 1)
		cxSwitch(1, false);

	while (true) {
		uchar saveline[MAXTTYLINE];
		pst p = inputLine();
		copyPstring(saveline, p);
		if (perl2c((char *)p))
			i_puts(MSG_EnterNull);
		else
			edbrowseCommand((char *)p, false);
		copyPstring(linePending, saveline);
	}			/* infinite loop */
}				/* main */

/* Find the balancing brace in an edbrowse function */
static const char *balance(const char *ip, int direction)
{
	int nest = 0;
	uchar code;

	while (true) {
		if (direction > 0) {
			ip = strchr(ip, '\n') + 1;
		} else {
			for (ip -= 2; *ip != '\n'; --ip) ;
			++ip;
		}
		code = *ip;
		if (code == 0x83) {
			if (nest)
				continue;
			break;
		}
		if (code == 0x81)
			nest += direction;
		if (code == 0x82)
			nest -= direction;
		if (nest < 0)
			break;
	}

	return ip;
}				/* balance */

/* Run an edbrowse function, as defined in the config file. */
bool runEbFunction(const char *line)
{
	char *linecopy = cloneString(line);
	const char *args[10];
	int argl[10];		/* lengths of args */
	int argtl;		/* total length */
	const char *s;
	char *t, *new;
	int j, l, nest;
	const char *ip;		/* think instruction pointer */
	const char *endl;	/* end of line to be processed */
	bool nofail, ok;
	uchar code;
	char stack[MAXNEST];
	int loopcnt[MAXNEST];

/* Separate function name and arguments */
	spaceCrunch(linecopy, true, false);
	if (linecopy[0] == 0) {
		setError(MSG_NoFunction);
		goto fail;
	}
	memset(args, 0, sizeof(args));
	memset(argl, 0, sizeof(argl));
	argtl = 0;
	t = strchr(linecopy, ' ');
	if (t)
		*t = 0;
	for (s = linecopy; *s; ++s)
		if (!isalnumByte(*s)) {
			setError(MSG_BadFunctionName);
			goto fail;
		}
	for (j = 0; ebScript[j]; ++j)
		if (stringEqualCI(linecopy, ebScriptName[j] + 1))
			break;
	if (!ebScript[j]) {
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
	for (s = t; s; s = t) {
		if (++j >= 10) {
			setError(MSG_ManyArgs);
			goto fail;
		}
		args[j] = ++s;
		t = strchr(s, ' ');
		if (t)
			*t = 0;
		if (argtl)
			++argtl;
		argtl += (argl[j] = strlen(s));
	}
	argl[0] = argtl;

	while (code = *ip) {
		if (intFlag) {
			setError(MSG_Interrupted);
			goto fail;
		}
		endl = strchr(ip, '\n');

		if (code == 0x83) {
			ip = balance(ip, 1) + 2;
			--nest;
			continue;
		}

		if (code == 0x82) {
			char control = stack[nest];
			char ucontrol = toupper(control);
			const char *start = balance(ip, -1);
			start = strchr(start, '\n') + 1;
			if (ucontrol == 'L') {	/* loop */
				if (--loopcnt[nest])
					ip = start;
				else
					ip = endl + 1, --nest;
				continue;
			}
			if (ucontrol == 'W' || ucontrol == 'U') {
				bool jump = ok;
				if (islowerByte(control))
					jump ^= true;
				if (ucontrol == 'U')
					jump ^= true;
				ok = true;
				if (jump)
					ip = start;
				else
					ip = endl + 1, --nest;
				continue;
			}
/* Apparently it's the close of an if or an else, just fall through */
			goto nextline;
		}

		if (code == 0x81) {
			const char *skip = balance(ip, 1);
			bool jump;
			char control = ip[1];
			char ucontrol = toupper(control);
			stack[++nest] = control;
			if (ucontrol == 'L') {
				loopcnt[nest] = j = atoi(ip + 2);
				if (j)
					goto nextline;
ahead:
				if (*skip == (char)0x82)
					--nest;
				ip = skip + 2;
				continue;
			}
			if (ucontrol == 'U')
				goto nextline;
/* if or while, test on ok */
			jump = ok;
			if (isupperByte(control))
				jump ^= true;
			ok = true;
			if (jump)
				goto ahead;
			goto nextline;
		}

		if (!ok && nofail)
			goto fail;

/* compute length of line, then build the line */
		l = endl - ip;
		for (s = ip; s < endl; ++s)
			if (*s == '~' && isdigitByte(s[1]))
				l += argl[s[1] - '0'];
		t = new = allocMem(l + 1);
		for (s = ip; s < endl; ++s) {
			if (*s == '~' && isdigitByte(s[1])) {
				j = *++s - '0';
				if (j) {
					if (!args[j]) {
						setError(MSG_NoArgument, j);
						nzFree(new);
						goto fail;
					}
					strcpy(t, args[j]);
					t += argl[j];
					continue;
				}
/* ~0 is all args together */
				for (j = 1; j <= 9 && args[j]; ++j) {
					if (j > 1)
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

	if (!ok && nofail)
		goto fail;

	nzFree(linecopy);
	return true;

fail:
	nzFree(linecopy);
	return false;
}				/* runEbFunction */

struct DBTABLE *findTableDescriptor(const char *sn)
{
	int i;
	struct DBTABLE *td = dbtables;
	for (i = 0; i < maxTables; ++i, ++td)
		if (stringEqual(td->shortname, sn))
			return td;
	return 0;
}				/* findTableDescriptor */

struct DBTABLE *newTableDescriptor(const char *name)
{
	struct DBTABLE *td;
	if (maxTables == MAXDBT) {
		setError(MSG_ManyTables, MAXDBT);
		return 0;
	}
	td = dbtables + maxTables++;
	td->name = td->shortname = cloneString(name);
	td->ncols = 0;		/* it's already 0 */
	return td;
}				/* newTableDescriptor */
