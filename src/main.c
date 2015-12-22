/* main.c
 * Entry point, arguments and options.
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"

#include <sys/stat.h>
#include <pcre.h>

/* Define the globals that are declared in eb.h. */
/* See eb.h for descriptive comments. */

const char *progname;
const char eol[] = "\r\n";
bool ismc, isimap, passMail;
bool inInput, listNA;
int fileSize;
char *dbarea, *dblogin, *dbpw;	/* to log into the database */
bool fetchBlobColumns;
bool caseInsensitive, searchStringsAll;
bool binaryDetect = true;
bool inputReadLine;
int context = 1;
pst linePending;
struct ebSession sessionList[MAXSESSION], *cs;

#define MAXNOJS 500
static const char *javaDis[MAXNOJS];
static int javaDisCount;
static int subjstart = 0;
static char *cfgcopy;
static int cfglen;
static long nowday;

#ifdef _MSC_VER
#endif // _MSC_VER

static void setNowDay(void)
{
	time_t now;
	time(&now);
	now /= (60 * 60 * 24);	/* convert to days */
	now -= 30 * 365;
	now -= 7;		/* leap years */
	nowday = now;
}				/* setNowDay */

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

	for (f = mailFilters; f->match; ++f) {
		const char *m = f->match;
		int mlen = strlen(m);
		int j, k;

		r = f->redirect;

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

/*\ MSVC Debug: May need to provide path to 3rdParty DLLs, like
 *  set PATH=F:\Projects\software\bin;%PATH% ...
\*/

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

#ifndef _MSC_VER		// port setlinebuf(stdout);, if required...
/* In case this is being piped over to a synthesizer, or whatever. */
	if (fileTypeByHandle(fileno(stdout)) != 'f')
		setlinebuf(stdout);
#endif // !_MSC_VER

	selectLanguage();

	eb_curl_global_init();
	ttySaveSettings();
	initializeReadline();
	progname = argv[0];

/* Let's everybody use my malloc and free routines */
	pcre_malloc = allocMem;
	pcre_free = nzFree;

/* Establish the home directory, and standard edbrowse files thereunder. */
	home = getenv("HOME");
#ifdef _MSC_VER
	if (!home) {
		home = getenv("APPDATA");
		if (home) {
			char *ebdata = (char *)allocMem(ABSPATH);
			sprintf(ebdata, "%s\\edbrowse", home);
			if (fileTypeByName(ebdata, false) != 'd') {
				FILE *fp;
				char *cfgfil;
				if (mkdir(ebdata, 0700)) {
					i_printfExit(MSG_NotHome);	// TODO: more appropriate exit message...
				}
				cfgfil = (char *)allocMem(ABSPATH);
				sprintf(cfgfil, "%s\\.ebrc", ebdata);
				fp = fopen(cfgfil, "w");
				if (fp) {
					fwrite(ebrc_string, 1,
					       strlen(ebrc_string), fp);
					fclose(fp);
				}
				i_printfExit(MSG_Personalize, cfgfil);

			}
			home = ebdata;
		}
	}
#endif // !_MSC_VER

/* Empty is the same as missing. */
	if (home && !*home)
		home = 0;
/* I require this, though I'm not sure what this means for non-Unix OS's */
	if (!home)
		i_printfExit(MSG_NotHome);
	if (fileTypeByName(home, false) != 'd')
		i_printfExit(MSG_NotDir, home);

	configFile = allocMem(strlen(home) + 7);
	sprintf(configFile, "%s/.ebrc", home);
/* if not present then create it, as was done above */
	if (fileTypeByName(configFile, false) == 0) {
		int fh = creat(configFile, 0600);
		if (fh >= 0) {
			write(fh, ebrc_string, strlen(ebrc_string));
			close(fh);
			i_printfExit(MSG_Personalize, configFile);
		}
	}

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
		static char agent0[64] = "edbrowse/";
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

	if (!sslCerts && doConfig && debugLevel >= 1)
		i_puts(MSG_NoCertFile);

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
#ifndef _MSC_VER		// port siginterrupt(SIGINT, 1); signal(SIGPIPE, SIG_IGN);, if required
	siginterrupt(SIGINT, 1);
	signal(SIGPIPE, SIG_IGN);
#endif // !_MSC_VER

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
		pst p = inputLine();
		pst save_p = clonePstring(p);
		if (perl2c((char *)p)) {
			i_puts(MSG_EnterNull);
			nzFree(save_p);
		} else {
			edbrowseCommand((char *)p, false);
			nzFree(linePending);
			linePending = save_p;
		}
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
	for (i = 0; i < numTables; ++i, ++td)
		if (stringEqual(td->shortname, sn))
			return td;
	return 0;
}				/* findTableDescriptor */

struct DBTABLE *newTableDescriptor(const char *name)
{
	struct DBTABLE *td;
	if (numTables == MAXDBT) {
		setError(MSG_ManyTables, MAXDBT);
		return 0;
	}
	td = dbtables + numTables++;
	td->name = td->shortname = cloneString(name);
	td->ncols = 0;		/* it's already 0 */
	return td;
}				/* newTableDescriptor */
