/* main.c
 * Entry point, arguments and options.
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"

#include <pthread.h>
#include <pcre.h>
#include <signal.h>

/* Define the globals that are declared in eb.h. */
/* See eb.h for descriptive comments. */

const char *progname;
const char eol[] = "\r\n";
const char *version = "3.7.3";
char *changeFileName;
char *configFile, *addressFile, *cookieFile;
char *mailDir, *mailUnread, *mailStash, *mailReply;
char *recycleBin, *sigFile, *sigFileEnd;
char *cacheDir;
int cacheSize = 1000, cacheCount = 10000;
char *ebTempDir, *ebUserDir;
char *userAgents[MAXAGENT + 1];
char *currentAgent;
bool allowRedirection = true, allowJS = true, sendReferrer = true;
bool allowXHR = true;
bool ftpActive;
int webTimeout = 20, mailTimeout = 0;
int displayLength = 500;
int verifyCertificates = 1;
char *sslCerts;
int localAccount, maxAccount;
struct MACCOUNT accounts[MAXACCOUNT];
int maxMime;
struct MIMETYPE mimetypes[MAXMIME];
/* filters to save emails in various files */
#define MAXFILTER 500
struct FILTERDESC {
	const char *match;
	const char *redirect;
	char type;
	long expire;
};
static struct FILTERDESC mailFilters[MAXFILTER];
static int n_filters;
int maxproxy;
struct PXENT proxyEntries[MAXPROXY];
/* for edbrowse functions defined in the config file */
#define MAXEBSCRIPT 500		// this many scripts
#define MAXNEST 20		// nested blocks
static char *ebScript[MAXEBSCRIPT + 1];
static char *ebScriptName[MAXEBSCRIPT + 1];
#define MAXNOJS 500
static const char *javaDis[MAXNOJS];
static int javaDisCount;
static struct DBTABLE dbtables[MAXDBT];
static int numTables;
volatile bool intFlag;
bool curlActive;
bool ismc, isimap, passMail;
char whichproc = 'e';		// edbrowse
bool inInput, listNA;
int fileSize;
char *dbarea, *dblogin, *dbpw;	/* to log into the database */
bool fetchBlobColumns;
bool caseInsensitive, searchStringsAll, searchWrap = true;
bool binaryDetect = true;
bool inputReadLine;
bool curlAuthNegotiate = false;
int context = 1;
pst linePending;
struct ebSession sessionList[MAXSESSION], *cs;
int maxSession;
static pthread_mutex_t share_mutex = PTHREAD_MUTEX_INITIALIZER;

#define MAXNOJS 500
static const char *javaDis[MAXNOJS];
static int javaDisCount;

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
		int k, mlen = strlen(m);

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
			if (mlen > slen)
				break;
			if (mlen == slen) {
				if (stringEqualCI(m, subj))
					return r;
				break;
			}
/* a prefix or suffix match is ok */
/* have to be at least half the subject line */
			if (slen > mlen + mlen)
				break;
			if (memEqualCI(m, subj, mlen))
				return r;
			k = slen - mlen;
			if (memEqualCI(m, subj + k, mlen))
				return r;
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

bool isSQL(const char *s)
{
	char c = *s;
	const char *c1 = 0;

	if (!sqlPresent)
		goto no;

	if (isURL(s))
		goto no;

// look for word] or word:word]
	if (!isalphaByte(c))
		goto no;

	for (++s; (c = *s); ++s) {
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
		if (c == ']')
			goto yes;
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

/*
 * Libcurl allows some really fine-grained access to data.  We could
 * have multiple mutexes if we want, and that might lead to less
 * blocking.  For now, we just use one mutex.
 */

static void lock_share(CURL * handle, curl_lock_data data,
		       curl_lock_access access, void *userptr)
{
/* TODO error handling. */
	pthread_mutex_lock(&share_mutex);
}				/* lock_share */

static void unlock_share(CURL * handle, curl_lock_data data, void *userptr)
{
	pthread_mutex_unlock(&share_mutex);
}				/* unlock_share */

void eb_curl_global_init(void)
{
	const unsigned int major = 7;
	const unsigned int minor = 29;
	const unsigned int patch = 0;
	const unsigned int least_acceptable_version =
	    (major << 16) | (minor << 8) | patch;
	curl_version_info_data *version_data = NULL;
	CURLcode curl_init_status = curl_global_init(CURL_GLOBAL_ALL);
	if (curl_init_status != 0)
		goto libcurl_init_fail;
	version_data = curl_version_info(CURLVERSION_NOW);
	if (version_data->version_num < least_acceptable_version)
		i_printfExit(MSG_CurlVersion, major, minor, patch);

// Initialize the global handle, to manage the cookie space.
	global_share_handle = curl_share_init();
	if (global_share_handle == NULL)
		goto libcurl_init_fail;

	curl_share_setopt(global_share_handle, CURLSHOPT_LOCKFUNC, lock_share);
	curl_share_setopt(global_share_handle, CURLSHOPT_UNLOCKFUNC,
			  unlock_share);
	curl_share_setopt(global_share_handle, CURLSHOPT_SHARE,
			  CURL_LOCK_DATA_COOKIE);
	curl_share_setopt(global_share_handle, CURLSHOPT_SHARE,
			  CURL_LOCK_DATA_DNS);
	curl_share_setopt(global_share_handle, CURLSHOPT_SHARE,
			  CURL_LOCK_DATA_SSL_SESSION);

	global_http_handle = curl_easy_init();
	if (global_http_handle == NULL)
		goto libcurl_init_fail;
	if (sslCerts) {
		curl_init_status =
		    curl_easy_setopt(global_http_handle, CURLOPT_CAINFO,
				     sslCerts);
		if (curl_init_status != CURLE_OK)
			goto libcurl_init_fail;
	}
	if (cookieFile && !ismc) {
		curl_init_status =
		    curl_easy_setopt(global_http_handle, CURLOPT_COOKIEJAR,
				     cookieFile);
		if (curl_init_status != CURLE_OK)
			goto libcurl_init_fail;
	}
	curl_init_status =
	    curl_easy_setopt(global_http_handle, CURLOPT_ENCODING, "");
	if (curl_init_status != CURLE_OK)
		goto libcurl_init_fail;

	curl_init_status =
	    curl_easy_setopt(global_http_handle, CURLOPT_SHARE,
			     global_share_handle);
	if (curl_init_status != CURLE_OK)
		goto libcurl_init_fail;
	curlActive = true;
	return;

libcurl_init_fail:
	i_printfExit(MSG_LibcurlNoInit);
}				/* eb_curl_global_init */

static void eb_curl_global_cleanup(void)
{
	curl_easy_cleanup(global_http_handle);
	curl_global_cleanup();
}				/* eb_curl_global_cleanup */

void ebClose(int n)
{
	bg_jobs(true);
	dbClose();
	if (curlActive) {
		mergeCookies();
		eb_curl_global_cleanup();
	}
	exit(n);
}				/* ebClose */

static void setupEdbrowseTempDirectory(void)
{
	int userid;
#ifdef DOSLIKE
	int l;
	char *a;
	ebTempDir = getenv("TEMP");
	if (!ebTempDir) {
		i_printf(MSG_NoEnvVar, "TEMP");
		nl();
		exit(1);
	}
// put /edbrowse on the end
	l = strlen(ebTempDir);
	a = allocString(l + 10);
	sprintf(a, "%s/edbrowse", ebTempDir);
	ebTempDir = a;
	userid = 0;
#else
	ebTempDir = "/tmp/.edbrowse";
	userid = geteuid();
#endif

// On a multiuser system, mkdir /tmp/.edbrowse at startup,
// by root, and then chmod 1777

	if (fileTypeByName(ebTempDir, false) != 'd') {
/* no such directory, try to make it */
/* this temp edbrowse directory is used by everyone system wide */
		if (mkdir(ebTempDir, 0777)) {
			i_printf(MSG_TempDir, ebTempDir);
			ebTempDir = 0;
			return;
		}
// yes, we called mkdir with 777 above, but that was cut by umask.
		chmod(ebTempDir, 0777);
	}
// make room for user ID on the end
	ebUserDir = allocMem(strlen(ebTempDir) + 20);
	sprintf(ebUserDir, "%s/%d", ebTempDir, userid);
	if (fileTypeByName(ebUserDir, false) != 'd') {
/* no such directory, try to make it */
		if (mkdir(ebUserDir, 0700)) {
			i_printf(MSG_TempDir, ebUserDir);
			ebUserDir = 0;
			return;
		}
		chmod(ebUserDir, 0700);
	}
}				/* setupEdbrowseTempDirectory */

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
	bool rc, doConfig = true, autobrowse = false;
	bool dofetch = false, domail = false;
	static char agent0[64] = "edbrowse/";

#ifndef _MSC_VER		// port setlinebuf(stdout);, if required...
/* In case this is being piped over to a synthesizer, or whatever. */
	if (fileTypeByHandle(fileno(stdout)) != 'f')
		setlinebuf(stdout);
#endif // !_MSC_VER

	selectLanguage();
	setHTTPLanguage(eb_language);

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

/* recycle bin and .signature files are unix-like, and not adjusted for windows. */
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

	strcat(agent0, version);
	userAgents[0] = currentAgent = agent0;

	setupEdbrowseTempDirectory();

	progname = argv[0];
	++argv, --argc;

	ttySaveSettings();
	initializeReadline();

/* Let's everybody use my malloc and free routines */
	pcre_malloc = allocMem;
	pcre_free = nzFree;

	if (argc && stringEqual(argv[0], "-c")) {
		if (argc == 1) {
			*argv = configFile;
			doConfig = false;
		} else {
			configFile = argv[1];
			argv += 2, argc -= 2;
		}
	}
	if (doConfig)
		readConfigFile();
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

		if (stringEqual(s, "b")) {
			autobrowse = true;
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
				eb_curl_global_init();
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

	signal(SIGINT, catchSig);

	cx = 0;
	while (argc) {
		char *file = *argv;
		char *file2 = NULL;	// will be allocated
		++cx;
		if (cx == MAXSESSION)
			i_printfExit(MSG_ManyOpen, MAXSESSION);
		cxSwitch(cx, false);
		if (cx == 1)
			runEbFunction("init");

// function on the command line
		if (file[0] == '<') {
			runEbFunction(file + 1);
			++argv, --argc;
			continue;
		}

		changeFileName = 0;
		file2 = allocMem(strlen(file) + 10);
// Every URL needs a protocol.
		if (missingProtURL(file))
			sprintf(file2 + 2, "http://%s", file);
		else
			strcpy(file2 + 2, file);
		file = file2 + 2;

		if (autobrowse) {
			const struct MIMETYPE *mt;
			uchar sxfirst = 0;
			if (isURL(file))
				mt = findMimeByURL(file, &sxfirst);
			else
				mt = findMimeByFile(file);
			if (mt && !mt->outtype)
				playBuffer("pb", file);
			else {
				file2[0] = 'b';
				file2[1] = ' ';
				if (runCommand(file2))
					debugPrint(1, "%d", fileSize);
				else
					showError();
			}

		} else {

			cf->fileName = cloneString(file);
			cf->firstURL = cloneString(file);
			if (isSQL(file))
				cw->sqlMode = true;
			rc = readFileArgv(file, 0);
			if (fileSize >= 0)
				debugPrint(1, "%d", fileSize);
			fileSize = -1;
			if (!rc) {
				showError();
			} else if (changeFileName) {
				nzFree(cf->fileName);
				cf->fileName = changeFileName;
				changeFileName = 0;
			}
			cw->undoable = cw->changeMode = false;
/* Browse the text if it's a url */
			if (rc && isURL(cf->fileName)
			    && ((cf->mt && cf->mt->outtype)
				|| isBrowseableURL(cf->fileName))) {
				if (runCommand("b"))
					debugPrint(1, "%d", fileSize);
				else
					showError();
			}
		}

		nzFree(file2);
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
/* This function must be reentrant. */
bool runEbFunction(const char *line)
{
	char *linecopy = cloneString(line);
	char *fncopy = 0;
	char *allargs = 0;
	const char *args[10];
	int argl[10];		/* lengths of args */
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

/* collect arguments, ~0 first */
	if (t) {
		args[0] = allargs = cloneString(t + 1);
		argl[0] = strlen(allargs);
	} else {
		args[0] = allargs = emptyString;
		argl[0] = 0;
	}

	j = 0;
	for (s = t; s; s = t) {
		if (++j >= 10) {
//                      setError(MSG_ManyArgs);
//                      goto fail;
			break;
		}
		args[j] = ++s;
		t = strchr(s, ' ');
		if (t)
			*t = 0;
		argl[j] = strlen(s);
	}

// This or a downstream function could invoke config.
// Don't know why anybody would do that!
	fncopy = cloneString(ip);
	ip = fncopy;

	while ((code = *ip)) {
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
				if (!args[j]) {
					setError(MSG_NoArgument, j);
					nzFree(new);
					goto fail;
				}
				strcpy(t, args[j]);
				t += argl[j];
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
	nzFree(fncopy);
	nzFree(allargs);
	return true;

fail:
	nzFree(linecopy);
	nzFree(fncopy);
	nzFree(allargs);
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

static char *configMemory;

// unread the config file, so we can read it again
void unreadConfigFile(void)
{
	if (!configMemory)
		return;
	nzFree(configMemory);
	configMemory = 0;

	memset(mailFilters, 0, sizeof(mailFilters));
	n_filters = 0;
	memset(accounts, 0, sizeof(accounts));
	maxAccount = localAccount = 0;
	memset(mimetypes, 0, sizeof(mimetypes));
	maxMime = 0;
	memset(dbtables, 0, sizeof(dbtables));
	numTables = 0;
	memset(proxyEntries, 0, sizeof(proxyEntries));
	maxproxy = 0;
	memset(ebScript, 0, sizeof(ebScript));
	memset(ebScriptName, 0, sizeof(ebScriptName));
	memset(userAgents + 1, 0, sizeof(userAgents) - sizeof(userAgents[0]));
	javaDisCount = 0;

	addressFile = NULL;
	cookieFile = NULL;
	sslCerts = NULL;
	downDir = NULL;
	mailDir = NULL;
	nzFree(cacheDir);
	cacheDir = NULL;
	nzFree(mailUnread);
	mailUnread = NULL;
	nzFree(mailReply);
	mailReply = NULL;

	webTimeout = mailTimeout = 0;
	displayLength = 500;

	setDataSource(NULL);
	setHTTPLanguage(eb_language);
	deleteNovsHosts();
}				/* unreadConfigFile */

/* Order is important here: mail{}, mime{}, table{}, then global keywords */
#define MAILWORDS 0
#define MIMEWORDS 8
#define TABLEWORDS 16
#define GLOBALWORDS 20

static const char *const keywords[] = {
	"inserver", "outserver", "login", "password", "from", "reply",
	"inport", "outport",
	"type", "desc", "suffix", "protocol", "program",
	"content", "outtype", "urlmatch",
	"tname", "tshort", "cols", "keycol",
	"downdir", "maildir", "agent",
	"jar", "nojs", "cachedir",
	"webtimer", "mailtimer", "certfile", "datasource", "proxy",
	"linelength", "localizeweb", "notused33", "novs", "cachesize",
	"adbook", 0
};

/* Read the config file and populate the corresponding data structures. */
/* This routine succeeds, or aborts via one of these macros. */
#define cfgAbort0(m) { i_printf(m); nl(); return; }
#define cfgAbort1(m, arg) { i_printf(m, arg); nl(); return; }
#define cfgLine0(m) { i_printf(m, ln); nl(); return; }
#define cfgLine1(m, arg) { i_printf(m, ln, arg); nl(); return; }
#define cfgLine1a(m, arg) { i_printf(m, arg, ln); nl(); return; }

void readConfigFile(void)
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

	unreadConfigFile();

	if (!fileIntoMemory(configFile, &buf, &buflen)) {
		i_printf(MSG_NoConfig, configFile);
		return;
	}

/* An extra newline won't hurt. */
	if (buflen && buf[buflen - 1] != '\n')
		buf[buflen++] = '\n';

// remember this allocated pointer in case we want to reset everything.
	configMemory = buf;

/* Undos, uncomment, watch for nulls */
/* Encode mail{ as hex 81 m, and other encodings. */
	ln = 1;
	for (s = t = v = buf; s < buf + buflen; ++s) {
		c = *s;
		if (c == '\0')
			cfgLine0(MSG_EBRC_Nulls);
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
					cfgLine0(MSG_EBRC_NoFnName);
#if 0
				if (isdigitByte(*q))
					cfgLine0(MSG_EBRC_FnDigit);
#endif
				while (isalnumByte(*q))
					++q;
				if (q - last - 9 > 10)
					cfgLine0(MSG_EBRC_FnTooLong);
				if (*q != '{' || q[1])
					cfgLine0(MSG_EBRC_SyntaxErr);
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
				cfgLine0(MSG_EBRC_NoCondFile);
			while (v > s && (v[-1] == ' ' || v[-1] == '\t'))
				--v;
			if (v == s)
				cfgLine0(MSG_EBRC_NoMatchStr);
			c = *v, *v++ = 0;
			if (c != '>') {
				while (*v != '>')
					++v;
				++v;
			}
			while (*v == ' ' || *v == '\t')
				++v;
			if (!*v)
				cfgLine1(MSG_EBRC_MatchNowh, s);
			if (n_filters == MAXFILTER - 1)
				cfgLine0(MSG_EBRC_Filters);
			mailFilters[n_filters].match = s;
			mailFilters[n_filters].redirect = v;
			mailFilters[n_filters].type = mailblock;
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
				cfgLine1a(MSG_EBRC_BadKeyword, s);
			*v = c;	/* put it back */
			goto nokeyword;
		}

		if (nest)
			cfgLine0(MSG_EBRC_KeyInFunc);

		if (n < MIMEWORDS && mailblock != 1)
			cfgLine1(MSG_EBRC_MailAttrOut, s);

		if (n >= MIMEWORDS && n < TABLEWORDS && !mimeblock)
			cfgLine1(MSG_EBRC_MimeAttrOut, s);

		if (n >= TABLEWORDS && n < GLOBALWORDS && !tabblock)
			cfgLine1(MSG_EBRC_TableAttrOut, s);

		if (n >= MIMEWORDS && mailblock)
			cfgLine1(MSG_EBRC_MailAttrIn, s);

		if ((n < MIMEWORDS || n >= TABLEWORDS) && mimeblock)
			cfgLine1(MSG_EBRC_MimeAttrIn, s);

		if ((n < TABLEWORDS || n >= GLOBALWORDS) && tabblock)
			cfgLine1(MSG_EBRC_TableAttrIn, s);

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
			cfgLine1(MSG_EBRC_NoAttr, s);

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
				cfgLine0(MSG_EBRC_Outtype);
			mt->outtype = c;
			continue;

		case 15:	/* urlmatch */
			mt->urlmatch = v;
			continue;

		case 16:	/* tname */
			td->name = v;
			continue;

		case 17:	/* tshort */
			td->shortname = v;
			continue;

		case 18:	/* cols */
			while (*v) {
				if (td->ncols == MAXTCOLS)
					cfgLine1(MSG_EBRC_ManyCols, MAXTCOLS);
				td->cols[td->ncols++] = v;
				q = strchr(v, ',');
				if (!q)
					break;
				*q = 0;
				v = q + 1;
			}
			continue;

		case 19:	/* keycol */
			if (!isdigitByte(*v))
				cfgLine0(MSG_EBRC_KeyNotNb);
			td->key1 = (uchar) strtol(v, &v, 10);
			if (*v == ',' && isdigitByte(v[1]))
				td->key2 = (uchar) strtol(v + 1, &v, 10);
			if (td->key1 > td->ncols || td->key2 > td->ncols)
				cfgLine1(MSG_EBRC_KeyOutRange, td->ncols);
			continue;

		case 20:	/* downdir */
			downDir = v;
			if (fileTypeByName(v, false) != 'd')
				cfgAbort1(MSG_EBRC_NotDir, v);
			continue;

		case 21:	/* maildir */
			mailDir = v;
			if (fileTypeByName(v, false) != 'd')
				cfgAbort1(MSG_EBRC_NotDir, v);
			mailUnread = allocMem(strlen(v) + 20);
			sprintf(mailUnread, "%s/unread", v);
/* We need the unread directory, else we can't fetch mail. */
/* Create it if it isn't there. */
			if (fileTypeByName(mailUnread, false) != 'd') {
				if (mkdir(mailUnread, 0700))
					cfgAbort1(MSG_EBRC_NotDir, mailUnread);
			}
			mailReply = allocMem(strlen(v) + 20);
			sprintf(mailReply, "%s/.reply", v);
			continue;

		case 22:	/* agent */
			for (j = 0; j < MAXAGENT; ++j)
				if (!userAgents[j])
					break;
			if (j == MAXAGENT)
				cfgLine1(MSG_EBRC_ManyAgents, MAXAGENT);
			userAgents[j] = v;
			continue;

		case 23:	/* jar */
			cookieFile = v;
			ftype = fileTypeByName(v, false);
			if (ftype && ftype != 'f')
				cfgAbort1(MSG_EBRC_JarNotFile, v);
			j = open(v, O_WRONLY | O_APPEND | O_CREAT, 0600);
			if (j < 0)
				cfgAbort1(MSG_EBRC_JarNoWrite, v);
			close(j);
			continue;

		case 24:	/* nojs */
			if (javaDisCount == MAXNOJS)
				cfgAbort1(MSG_EBRC_NoJS, MAXNOJS);
			if (*v == '.')
				++v;
			q = strchr(v, '.');
			if (!q || q[1] == 0)
				cfgLine1(MSG_EBRC_DomainDot, v);
			javaDis[javaDisCount++] = v;
			continue;

		case 25:	/* cachedir */
			nzFree(cacheDir);
			cacheDir = cloneString(v);
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
				cfgAbort1(MSG_EBRC_SSLNoFile, v);
			j = open(v, O_RDONLY);
			if (j < 0)
				cfgAbort1(MSG_EBRC_SSLNoRead, v);
			close(j);
			continue;

		case 29:	/* datasource */
			setDataSource(v);
			continue;

		case 30:	/* proxy */
			if (maxproxy == MAXPROXY)
				cfgAbort1(MSG_EBRC_NoPROXY, MAXPROXY);
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

		case 34:	/* novs */
			if (*v == '.')
				++v;
			q = strchr(v, '.');
			if (!q || q[1] == 0)
				cfgLine1(MSG_EBRC_DomainDot, v);
			addNovsHost(v);
			continue;

		case 35:	/* cachesize */
			cacheSize = atoi(v);
			if (cacheSize <= 0)
				cacheSize = 0;
			if (cacheSize >= 10000)
				cacheSize = 10000;
			continue;

		case 36:	/* adbook */
			addressFile = v;
			ftype = fileTypeByName(v, false);
			if (ftype && ftype != 'f')
				cfgAbort1(MSG_EBRC_AbNotFile, v);
			continue;

		default:
			cfgLine1(MSG_EBRC_KeywordNYI, s);
		}		/* switch */

nokeyword:

		if (stringEqual(s, "default") && mailblock == 1) {
			if (localAccount == maxAccount + 1)
				continue;
			if (localAccount)
				cfgAbort0(MSG_EBRC_SevDefaults);
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

		if (stringEqual(s, "from_file") && mimeblock == 1) {
			mt->from_file = true;
			continue;
		}
		if (stringEqual(s, "down_url") && mimeblock == 1) {
			mt->down_url = true;
			continue;
		}

		if (*s == '\x82' && s[1] == 0) {
			if (mailblock == 1) {
				++maxAccount;
				mailblock = 0;
				if (!act->inurl)
					cfgLine0(MSG_EBRC_NoInserver);
				if (!act->outurl)
					cfgLine0(MSG_EBRC_NoOutserver);
				if (!act->login)
					cfgLine0(MSG_EBRC_NoLogin);
				if (!act->password)
					cfgLine0(MSG_EBRC_NPasswd);
				if (!act->from)
					cfgLine0(MSG_EBRC_NoFrom);
				if (!act->reply)
					cfgLine0(MSG_EBRC_NoReply);
				if (act->secure)
					act->inssl = act->outssl = 1;
				if (!act->inport) {
					if (act->secure) {
						act->inport =
						    (act->imap ? 993 : 995);
					} else {
						act->inport =
						    (act->imap ? 143 : 110);
					}
				}
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
					cfgLine0(MSG_EBRC_NoType);
				if (!mt->desc)
					cfgLine0(MSG_EBRC_NDesc);
				if (!mt->suffix && !mt->prot)
					cfgLine0(MSG_EBRC_NoSuffix);
				if (!mt->program)
					cfgLine0(MSG_EBRC_NoProgram);
				continue;
			}

			if (tabblock) {
				++numTables;
				tabblock = false;
				if (!td->name)
					cfgLine0(MSG_EBRC_NoTblName);
				if (!td->shortname)
					cfgLine0(MSG_EBRC_NoShortName);
				if (!td->ncols)
					cfgLine0(MSG_EBRC_NColumns);
				continue;
			}

			if (--nest < 0)
				cfgLine0(MSG_EBRC_UnexpBrace);
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
				cfgLine0(MSG_EBRC_UnexElse);
			goto putback;
		}

		if (*s != '\x81') {
			if (!nest)
				cfgLine0(MSG_EBRC_GarblText);
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
			cfgLine1(MSG_EBRC_FnNotStart, curblock);
		}

		if (!strchr("fmertsb", c) && !nest)
			cfgLine0(MSG_EBRC_StatNotInFn);

		if (c == 'm') {
			mailblock = 1;
			if (maxAccount == MAXACCOUNT)
				cfgAbort1(MSG_EBRC_ManyAcc, MAXACCOUNT);
			act = accounts + maxAccount;
			continue;
		}

		if (c == 'e') {
			mimeblock = true;
			if (maxMime == MAXMIME)
				cfgAbort1(MSG_EBRC_ManyTypes, MAXMIME);
			mt = mimetypes + maxMime;
			continue;
		}

		if (c == 'b') {
			tabblock = true;
			if (numTables == MAXDBT)
				cfgAbort1(MSG_EBRC_ManyTables, MAXDBT);
			td = dbtables + numTables;
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
				cfgAbort1(MSG_EBRC_ManyFn, sn);
			ebScriptName[sn] = s + 2;
			t[-1] = 0;
			ebScript[sn] = t;
			goto putback;
		}

		if (++nest >= sizeof(stack))
			cfgLine0(MSG_EBRC_TooDeeply);
		stack[nest] = c;

putback:
		*t = '\n';
	}			/* loop over lines */

	if (nest)
		cfgAbort1(MSG_EBRC_FnNotClosed, ebScriptName[sn]);

	if (mailblock | mimeblock)
		cfgAbort0(MSG_EBRC_MNotClosed);

	if (!sslCerts)
		verifyCertificates = 0;

	if (maxAccount && !localAccount)
		localAccount = 1;
}				/* readConfigFile */
