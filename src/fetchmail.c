/* fetchmail.c
 * Get mail using the pop3 protocol.
 * Format the mail in ascii, or in html.
 * Unpack attachments.
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"

#define MHLINE 200		/* length of a mail header line */
struct MHINFO {
	struct MHINFO *next, *prev;
	struct listHead components;
	char *start, *end;
	char subject[MHLINE + 4];
	char to[MHLINE];
	char from[MHLINE];
	char reply[MHLINE];
	char date[MHLINE];
	char boundary[MHLINE];
	int boundlen;
	char *tolist, *cclist;
	int tolen, cclen;
	char mid[MHLINE];	/* message id */
	char ref[MHLINE];	/* references */
	char cfn[MHLINE];	/* content file name */
	uchar ct, ce;		/* content type, content encoding */
	bool andOthers;
	bool doAttach;
	bool atimage;
	bool pgp;
	uchar error64;
};

static int nattach;		/* number of attachments */
static int nimages;		/* number of attached images */
static char *firstAttach;	/* name of first file */
static bool mailIsHtml;
static char *fm;		/* formatted mail string */
static int fm_l;
static struct MHINFO *lastMailInfo;
static char *lastMailText;

static void freeMailInfo(struct MHINFO *w)
{
	struct MHINFO *v;
	while (!listIsEmpty(&w->components)) {
		v = w->components.next;
		delFromList(v);
		freeMailInfo(v);
	}
	nzFree(w->tolist);
	nzFree(w->cclist);
	nzFree(w);
}				/* freeMailInfo */

static bool ignoreImages;

static void writeAttachment(struct MHINFO *w)
{
	const char *atname;
	if ((ismc | ignoreImages) && w->atimage)
		return;		/* image ignored */
	if (w->pgp)
		return;		/* Ignore PGP signatures. */
	if (w->error64 == BAD_BASE64_DECODE)
		i_printf(MSG_Abbreviated);
	if (w->start == w->end) {
		i_printf(MSG_AttEmpty);
		if (w->cfn[0])
			printf(" %s", w->cfn);
		nl();
		atname = "x";
	} else {
		i_printf(MSG_Att);
		atname = getFileName(MSG_FileName, (w->cfn[0] ? w->cfn : 0),
				     true, false);
/* X is like x, but deletes all future images */
		if (stringEqual(atname, "X")) {
			atname = "x";
			ignoreImages = true;
		}
	}
	if (!ismc && stringEqual(atname, "e")) {
		int cx, svcx = context;
		for (cx = 1; cx < MAXSESSION; ++cx)
			if (!sessionList[cx].lw)
				break;
		if (cx == MAXSESSION) {
			i_printf(MSG_AttNoBuffer);
		} else {
			cxSwitch(cx, false);
			i_printf(MSG_SessionX, cx);
			if (!addTextToBuffer
			    ((pst) w->start, w->end - w->start, 0, false))
				i_printf(MSG_AttNoCopy, cx);
			else if (w->cfn[0])
				cw->fileName = cloneString(w->cfn);
			cxSwitch(svcx, false);	/* back to where we were */
		}
	} else if (!stringEqual(atname, "x")) {
		int fh =
		    open(atname, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, 0666);
		if (fh < 0) {
			i_printf(MSG_AttNoSave, atname);
			if (ismc)
				exit(1);
		} else {
			int nb = w->end - w->start;
			if (write(fh, w->start, nb) < nb) {
				i_printf(MSG_AttNoWrite, atname);
				if (ismc)
					exit(1);
			}
			close(fh);
		}
	}
}				/* writeAttachment */

static void writeAttachments(struct MHINFO *w)
{
	struct MHINFO *v;
	if (w->doAttach) {
		writeAttachment(w);
	} else {
		foreach(v, w->components)
		    writeAttachments(v);
	}
}				/* writeAttachments */

/* find the last mail in the unread directory */
static int unreadMax, unreadMin, unreadCount;
static int unreadBase;		/* find min larger than base */

static void unreadStats(void)
{
	const char *f;
	int n;

	unreadMax = 0;
	unreadMin = 0;
	unreadCount = 0;

	while (f = nextScanFile(mailUnread)) {
		if (!stringIsNum(f))
			continue;
		n = atoi(f);
		if (n > unreadMax)
			unreadMax = n;
		if (n > unreadBase) {
			if (!unreadMin || n < unreadMin)
				unreadMin = n;
			++unreadCount;
		}
	}
}				/* unreadStats */

static char *umf;		/* unread mail file */
static char *umf_end;
static int umfd;		/* file descriptor for the above */
/* string to hold the mail message, and then its length */
static char *mailstring;
static int mailstring_l;
/* convert mail message to/from utf8 if need be. */
/* This isn't really right, cause it should be done per mime component. */
static char *mailu8;
static int mailu8_l;

static struct eb_curl_callback_data callback_data = {
	&mailstring, &mailstring_l
};

static CURL *newFetchmailHandle(const char *mailbox, const char *username,
				const char *password)
{
	CURLcode res;
	CURL *handle = curl_easy_init();
	if (!handle)
		i_printfExit(MSG_LibcurlNoInit);

	curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, mailTimeout);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, eb_curl_callback);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, &callback_data);
	if (debugLevel >= 4)
		curl_easy_setopt(handle, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(handle, CURLOPT_DEBUGFUNCTION, ebcurl_debug_handler);
	res = curl_easy_setopt(handle, CURLOPT_CAINFO, sslCerts);
	if (res != CURLE_OK)
		i_printfExit(MSG_LibcurlNoInit);

	res = curl_easy_setopt(handle, CURLOPT_USERNAME, username);
	if (res != CURLE_OK) {
		ebcurl_setError(res, mailbox);
		showErrorAbort();
	}

	res = curl_easy_setopt(handle, CURLOPT_PASSWORD, password);
	if (res != CURLE_OK) {
		ebcurl_setError(res, mailbox);
		showErrorAbort();
	}

	return handle;
}				/* newFetchmailHandle */

static char *get_mailbox_url(const struct MACCOUNT *account)
{
	const char *scheme = "pop3";
	char *url = NULL;

	if (account->inssl)
		scheme = "pop3s";

	if (account->imap)
		scheme = (account->inssl ? "imaps" : "imap");

	if (asprintf(&url,
		     "%s://%s:%d/", scheme, account->inurl,
		     account->inport) == -1) {
/* The byte count is a little white lie / guess, we don't know
 * how much asprintf *really* requested. */
		i_printfExit(MSG_MemAllocError,
			     strlen(scheme) + strlen(account->inurl) + 8);
	}
	return url;
}				/* get_mailbox_url */

static CURLcode fetchOneMessage(CURL * handle, const char *message_url,
				int message_number)
{
	CURLcode res = CURLE_OK;

	res = setCurlURL(handle, message_url);
	if (res != CURLE_OK)
		return res;
	res = curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, NULL);
	if (res != CURLE_OK)
		return res;
	res = curl_easy_setopt(handle, CURLOPT_NOBODY, 0L);
	if (res != CURLE_OK)
		return res;

	res = curl_easy_perform(handle);
	if (mailstring_l >= CHUNKSIZE)
		nl();		/* We printed dots, so we terminate them with newline */
	if (res != CURLE_OK)
		return res;

/* Remove DOS newlines. */
	int j, k;
	for (j = k = 0; j < mailstring_l; j++) {
		if (mailstring[j] == '\r' && j < mailstring_l - 1
		    && mailstring[j + 1] == '\n')
			continue;
		mailstring[k++] = mailstring[j];
	}
	mailstring_l = k;

/* got the file, save it in unread */
	sprintf(umf_end, "%d", unreadMax + message_number);
	umfd = open(umf, O_WRONLY | O_TEXT | O_CREAT, 0666);
	if (umfd < 0)
		i_printfExit(MSG_NoCreate, umf);
	if (write(umfd, mailstring, mailstring_l) < mailstring_l)
		i_printfExit(MSG_NoWrite, umf);
	close(umfd);

	return res;
}				/* fetchOneMessage */

static CURLcode deleteOneMessage(CURL * handle, const char *message_url)
{
	CURLcode res = curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "DELE");

	if (res != CURLE_OK)
		return res;
	res = curl_easy_setopt(handle, CURLOPT_NOBODY, 1L);
	if (res != CURLE_OK)
		return res;
	res = curl_easy_setopt(handle, CURLOPT_NOBODY, 1L);
	if (res != CURLE_OK)
		return res;
	res = curl_easy_perform(handle);

	return res;
}				/* deleteOneMessage */

static CURLcode count_messages(CURL * handle, const char *mailbox,
			       int *message_count)
{
	CURLcode res = setCurlURL(handle, mailbox);
	int i, num_messages = 0;
	bool last_nl = true;

	if (res != CURLE_OK)
		return res;

	res = curl_easy_perform(handle);
	if (res != CURLE_OK)
		return res;

	for (i = 0; i < mailstring_l; i++) {
		if (mailstring[i] == '\n' || mailstring[i] == '\r') {
			last_nl = true;
			continue;
		}
		if (last_nl && isdigit(mailstring[i]))
			num_messages++;
		last_nl = false;
	}

	*message_count = num_messages;
	return CURLE_OK;
}				/* count_messages */

/* Returns number of messages fetched */
int fetchMail(int account)
{
	const struct MACCOUNT *a = accounts + account - 1;
	const char *login = a->login;
	const char *pass = a->password;
	int nfetch = 0;		/* number of messages actually fetched */
	CURLcode res_curl = CURLE_OK;
	char *mailbox_url = get_mailbox_url(a);
	const char *url_for_error = mailbox_url;
	char *message_url = NULL;
	int message_count = 0, message_number;

	if (!mailDir)
		i_printfExit(MSG_NoMailDir);
	if (chdir(mailDir))
		i_printfExit(MSG_NoDirChange, mailDir);

	if (!umf) {
		umf = allocMem(strlen(mailUnread) + 12);
		sprintf(umf, "%s/", mailUnread);
		umf_end = umf + strlen(umf);
	}
	unreadBase = 0;
	unreadStats();

	mailstring = initString(&mailstring_l);
	CURL *mail_handle = newFetchmailHandle(mailbox_url, login, pass);
	res_curl = count_messages(mail_handle, mailbox_url, &message_count);
	if (res_curl != CURLE_OK)
		goto fetchmail_cleanup;

	for (message_number = 1; message_number <= message_count;
	     message_number++) {
		if (asprintf(&message_url, "%s%u", mailbox_url, message_number)
		    == -1) {
/* Again, the byte count in the error message is a bit of a fib. */
			i_printfExit(MSG_MemAllocError,
				     strlen(mailbox_url) + 11);
		}
		nzFree(mailstring);
		mailstring = initString(&mailstring_l);
		res_curl =
		    fetchOneMessage(mail_handle, message_url, message_number);
		if (res_curl != CURLE_OK)
			goto fetchmail_cleanup;
		nfetch++;
		res_curl = deleteOneMessage(mail_handle, message_url);
		if (res_curl != CURLE_OK)
			goto fetchmail_cleanup;
		nzFree(message_url);
		message_url = NULL;
	}

fetchmail_cleanup:
	if (message_url)
		url_for_error = message_url;
	if (res_curl != CURLE_OK) {
		ebcurl_setError(res_curl, url_for_error);
		showError();
	}
	curl_easy_cleanup(mail_handle);
	nzFree(message_url);
	nzFree(mailbox_url);
	nzFree(mailstring);
	mailstring = initString(&mailstring_l);
	return nfetch;
}				/* fetchMail */

/* fetch from all accounts except those with nofetch or imap set */
int fetchAllMail(void)
{
	int i, j;
	const struct MACCOUNT *a, *b;
	int nfetch = 0;

	for (i = 1; i <= maxAccount; ++i) {
		a = accounts + i - 1;
		if (a->nofetch | a->imap)
			continue;

/* don't fetch from an earlier account that has the same host an dlogin */
		for (j = 1; j < i; ++j) {
			b = accounts + j - 1;
			if (!b->nofetch &&
			    stringEqual(a->inurl, b->inurl) &&
			    stringEqual(a->login, b->login))
				break;
		}
		if (j < i)
			continue;

		debugPrint(3, "fetch from %d %s", i, a->inurl);
		nfetch += fetchMail(i);
	}

	return nfetch;
}				/* fetchAllMail */

static int presentMail(void);
static void readReplyInfo(void);
static void writeReplyInfo(const char *addstring);

void scanMail(void)
{
	int nmsgs, m;

	if (!isInteractive)
		i_printfExit(MSG_FetchNotBackgnd);
	if (!mailDir)
		i_printfExit(MSG_NoMailDir);
	if (chdir(mailDir))
		i_printfExit(MSG_NoDirChange, mailDir);

	if (!umf) {
		umf = allocMem(strlen(mailUnread) + 12);
		sprintf(umf, "%s/", mailUnread);
		umf_end = umf + strlen(umf);
	}

/* How many mail messages? */
	unreadBase = 0;
	unreadStats();
	nmsgs = unreadCount;
	if (!nmsgs) {
		i_puts(MSG_NoMail);
		exit(0);
	}
	i_printf(MSG_MessagesX, nmsgs);

	loadAddressBook();

	for (m = 1; m <= nmsgs; ++m) {
		nzFree(lastMailText);
		lastMailText = 0;
/* Now grab the entire message */
		unreadStats();
		sprintf(umf_end, "%d", unreadMin);
		if (!fileIntoMemory(umf, &mailstring, &mailstring_l))
			showErrorAbort();
		unreadBase = unreadMin;

		if (presentMail() == 1)
			unlink(umf);
	}			/* loop over mail messages */

	exit(0);
}				/* scanMail */

/* a mail message is in mailstring, present it to the user */
/* Return 0 for ok, 1 to delete the mail, -1 to stop.
 * stop is only meaningful for imap. */
static int presentMail(void)
{
	int j, k;
	const char *redirect = NULL;	/* send mail elsewhere */
	char key = 0;
	const char *atname = NULL;	/* name of file or attachment */
	bool delflag = false;	/* delete this mail */
	bool scanat = false;	/* scan for attachments */
	int displine;
	int stashNumber = -1;

/* clear things out from the last message */
	if (lastMailInfo)
		freeMailInfo(lastMailInfo);
	lastMailInfo = 0;

	if (sessionList[1].lw)
		cxQuit(1, 2);
	cs = 0;
	cxSwitch(1, false);

	iuReformat(mailstring, mailstring_l, &mailu8, &mailu8_l);
	if (mailu8) {
		if (!addTextToBuffer((pst) mailu8, mailu8_l, 0, false))
			showErrorAbort();
	} else {
		if (!addTextToBuffer((pst) mailstring, mailstring_l, 0, false))
			showErrorAbort();
	}

	browseCurrentBuffer();

	if (!passMail) {
		redirect = mailRedirect(lastMailInfo->to,
					lastMailInfo->from,
					lastMailInfo->reply,
					lastMailInfo->subject);
	}

	if (redirect) {
		if (!isimap) {
			delflag = true;
			key = 'w';
			if (*redirect == '-')
				++redirect, key = 'u';
			if (stringEqual(redirect, "x"))
				i_puts(MSG_Junk);
			else
				printf("> %s\n", redirect);
		} else {
			if (*redirect == '-')
				++redirect;
			if (stringEqual(redirect, "x"))
				redirect = NULL;
		}
	}

/* display the next page of mail and get a command from the keyboard */
	displine = 1;
paging:
	if (!delflag) {		/* show next page */
		if (displine <= cw->dol) {
			for (j = 0; j < 20 && displine <= cw->dol;
			     ++j, ++displine) {
				char *showline = (char *)fetchLine(displine, 1);
				k = pstLength((pst) showline);
				showline[--k] = 0;
				printf("%s\n", showline);
				nzFree(showline);
			}
		}
	}

/* get key command from user */
key_command:
	if (delflag)
		goto writeMail;

/* interactive prompt depends on whether there is more text or not */
	printf("%c ", displine > cw->dol ? '?' : '*');
	fflush(stdout);
	key = getLetter((isimap ? "q? nwWuUasd" : "q? nwud"));
	printf("\b\b\b");
	fflush(stdout);

	switch (key) {
	case 'q':
		i_puts(MSG_Quit);
		exit(0);

	case 'n':
		i_puts(MSG_Next);
		goto afterinput;

	case 's':
		i_puts(MSG_Stop);
		goto afterinput;

	case 'd':
		i_puts(MSG_Delete);
		delflag = true;
		goto afterinput;

	case ' ':
		if (displine > cw->dol)
			i_puts(MSG_EndMessage);
		goto paging;

	case '?':
		i_puts(isimap ? MSG_ImapReadHelp : MSG_MailHelp);
		goto key_command;

	case 'a':
		key = 'w';	/* this will scan attachments */
		scanat = true;

	case 'w':
	case 'W':
	case 'u':
	case 'U':
		break;

	default:
		i_puts(MSG_NYI);
		goto key_command;
	}			/* switch */

/* At this point we're saving the mail somewhere. */
writeMail:
	if (!isimap || isupper(key))
		delflag = true;
	atname = 0;
	if (!isimap)
		atname = redirect;

	if (scanat)
		goto attachOnly;

saveMail:
	if (!atname)
		atname = getFileName(MSG_FileName, redirect, false, false);
	if (stringEqual(atname, "x"))
		goto afterinput;

	char exists = fileTypeByName(atname, false);
	int fsize;		/* file size */
	int fh = open(atname, O_WRONLY | O_TEXT | O_CREAT | O_APPEND, 0666);
	if (fh < 0) {
		i_printf(MSG_NoCreate, atname);
		goto saveMail;
	}
	if (exists)
		write(fh,
		      "======================================================================\n",
		      71);
	if (key == 'u') {
		if (write(fh, mailstring, mailstring_l) < mailstring_l) {
badsave:
			i_printf(MSG_NoWrite, atname);
			close(fh);
			goto saveMail;
		}
		close(fh);
		fsize = mailstring_l;
	} else {

/* key = w, write the file - if pop then save the original unformatted */
		if (!isimap && mailStash) {
			char *rmf;	/* raw mail file */
			int rmfh;	/* file handle to same */
/* I want a fairly easy filename, in case I want to go look at the original.
* Not a 30 character message ID that I am forced to cut&paste.
* 4 or 5 digits would be nice.
* So the filename looks like /home/foo/.Trash/rawmail/36921
* I pick the digits randomly.
* Please don't accumulate 100,000 emails before you empty your trash.
* It's good to have a cron job empty the trash early Sunday morning.
*/

			k = strlen(mailStash);
			rmf = allocMem(k + 12);
/* Try 20 times, then give up. */
			for (j = 0; j < 20; ++j) {
				int rn = rand() % 100000;	/* random number */
				sprintf(rmf, "%s/%05d", mailStash, rn);
				if (fileTypeByName(rmf, false))
					continue;
/* dump the original mail into the file */
				rmfh =
				    open(rmf,
					 O_WRONLY | O_TEXT | O_CREAT | O_APPEND,
					 0666);
				if (rmfh < 0)
					break;
				if (write(rmfh, mailstring, mailstring_l) <
				    mailstring_l) {
					close(rmfh);
					unlink(rmf);
					break;
				}
				close(rmfh);
/* written successfully, remember the stash number */
				stashNumber = rn;
				break;
			}
		}

		fsize = 0;
		for (j = 1; j <= cw->dol; ++j) {
			char *showline = (char *)fetchLine(j,
							   1);
			int len = pstLength((pst)
					    showline);
			if (write(fh, showline, len) < len)
				goto badsave;
			nzFree(showline);
			fsize += len;
		}		/* loop over lines */

		if (stashNumber >= 0) {
			char addstash[60];
			int minor = rand() % 100000;
			sprintf(addstash, "\nUnformatted %05d.%05d\n",
				stashNumber, minor);
			k = strlen(addstash);
			if (write(fh, addstash, k) < k)
				goto badsave;
			fsize += k;
/* write the mailInfo data to the mail reply file */
			addstash[k - 1] = ':';
			writeReplyInfo(addstash + k - 12);
		}

		close(fh);

attachOnly:

		if (nattach)
			writeAttachments(lastMailInfo);
		else if (scanat)
			i_puts(MSG_NoAttachments);
	}			/* unformat or format */

	if (scanat)
		goto afterinput;
/* print "mail saved" message */
	i_printf(MSG_MailSaved, fsize);
	if (exists)
		i_printf(MSG_Appended);
	nl();

afterinput:
	nzFree(mailstring);
	mailstring = 0;
	nzFree(mailu8);
	mailu8 = 0;

	if (delflag)
		return 1;
	if (key == 's')
		return -1;
	return 0;
}				/* presentMail */

/* Here are the common keywords for mail header lines.
 * These are in alphabetical order, so you can stick more in as you find them.
 * The more words we have, the more accurate the test. */
static const char *const mhwords[] = {
	"action:",
	"arrival-date:",
	"bcc:",
	"cc:",
	"content-transfer-encoding:",
	"content-type:",
	"date:",
	"delivered-to:",
	"errors-to:",
	"final-recipient:",
	"from:",
	"importance:",
	"last-attempt-date:",
	"list-id:",
	"mailing-list:",
	"message-id:",
	"mime-version:",
	"precedence:",
	"received:",
	"remote-mta:",
	"reply-to:",
	"reporting-mta:",
	"return-path:",
	"sender:",
	"sent:",
	"status:",
	"subject:",
	"to:",
	"user-agent:",
	"x-beenthere:",
	"x-comment:",
	"x-loop:",
	"x-mailer:",
	"x-mailman-version:",
	"x-mdaemon-deliver-to:",
	"x-mdremoteip:",
	"x-mimeole:",
	"x-ms-tnef-correlator:",
	"x-msmail-priority:",
	"x-originating-ip:",
	"x-priority:",
	"x-return-path:",
	"X-Spam-Checker-Version:",
	"x-spam-level:",
	"x-spam-msg-id:",
	"X-SPAM-Msg-Sniffer-Result:",
	"x-spam-processed:",
	"x-spam-status:",
	"x-uidl:",
	0
};

/* Before we render a mail message, let's make sure it looks like email.
 * This is similar to htmlTest() in html.c. */
bool emailTest(void)
{
	int i, j, k, n;

/* This is a very simple test - hopefully not too simple.
 * The first 20 non-indented lines have to look like mail header lines,
 * with at least half the keywords recognized. */
	for (i = 1, j = k = 0; i <= cw->dol && j < 20; ++i) {
		char *q;
		char *p = (char *)fetchLine(i, -1);
		char first = *p;
		if (first == '\n' || first == '\r' && p[1] == '\n')
			break;
		if (first == ' ' || first == '\t')
			continue;
		++j;		/* nonindented line */
		for (q = p; isalnumByte(*q) || *q == '_' || *q == '-'; ++q) ;
		if (q == p)
			continue;
		if (*q++ != ':')
			continue;
/* X-Whatever is a mail header word */
		if (q - p >= 8 && p[1] == '-' && toupper(p[0]) == 'X') {
			++k;
		} else {
			for (n = 0; mhwords[n]; ++n)
				if (memEqualCI(mhwords[n], p, q - p))
					break;
			if (mhwords[n])
				++k;
		}
		if (k >= 4 && k * 2 >= j)
			return true;
	}			/* loop over lines */

	return false;
}				/* emailTest */

static void mail64Error(int err)
{
	switch (err) {
	case BAD_BASE64_DECODE:
		runningError(MSG_AttBad64);
		break;
	case EXTRA_CHARS_BASE64_DECODE:
		runningError(MSG_AttAfterChars);
		break;
	}			/* switch on error code */
}				/* mail64Error */

static void unpackQP(struct MHINFO *w)
{
	uchar val;
	char c, d, *q, *r;
	for (q = r = w->start; q < w->end; ++q) {
		c = *q;
		if (c != '=') {
			*r++ = c;
			continue;
		}
		c = *++q;
		if (c == '\n')
			continue;
		d = q[1];
		if (isxdigit(c) && isxdigit(d)) {
			d = fromHex(c, d);
			if (d == 0)
				d = ' ';
			*r++ = d;
			++q;
			continue;
		}
		--q;
		*r++ = '=';
	}
	w->end = r;
	*r = 0;
}				/* unpackQP */

void
unpackUploadedFile(const char *post, const char *boundary,
		   char **postb, int *postb_l)
{
	static const char message64[] = "Content-Transfer-Encoding: base64";
	const int boundlen = strlen(boundary);
	const int m64len = strlen(message64);
	char *post2;
	char *b1, *b2, *b3, *b4;	/* boundary points */

	*postb = 0;
	*postb_l = 0;
	if (!strstr(post, message64))
		return;

	post2 = cloneString(post);
	b2 = strstr(post2, boundary);
	while (true) {
		b1 = b2 + boundlen;
		if (*b1 != '\r')
			break;
		b1 += 2;
		b1 = strstr(b1, "Content-Transfer");
		b2 = strstr(b1, boundary);
		if (memcmp(b1, message64, m64len))
			continue;
		b1 += m64len - 6;
		strcpy(b1, "8bit\r\n\r\n");
		b1 += 8;
		b1[0] = b1[1] = ' ';
		b3 = b2 - 4;

		b4 = b3;
		int unpack_ret = base64Decode(b1, &b4);
		if (unpack_ret != GOOD_BASE64_DECODE)
			mail64Error(unpack_ret);
		/* Should we *really* keep going at this point? */
		strmove(b4, b3);
		b2 = b4 + 4;
	}

	b1 += strlen(b1);
	*postb = post2;
	*postb_l = b1 - post2;
}				/* unpackUploadedFile */

/* Look for the name of the attachment and boundary */
static void ctExtras(struct MHINFO *w, const char *s, const char *t)
{
	char quote;
	const char *q, *al, *ar;

	if (w->ct < CT_MULTI) {
		quote = 0;
		for (q = s + 1; q < t; ++q) {
			if (isalnumByte(q[-1]))
				continue;
/* could be name= or filename= */
			if (memEqualCI(q, "file", 4))
				q += 4;
			if (!memEqualCI(q, "name=", 5))
				continue;
			q += 5;
			if (*q == '"') {
				quote = *q;
				++q;
			}
			for (al = q; q < t; ++q) {
				if (*q == '"')
					break;
				if (quote)
					continue;
				if (strchr(",; \t", *q))
					break;
			}
			ar = q;
			if (ar - al >= MHLINE)
				ar = al + MHLINE - 1;
			strncpy(w->cfn, al, ar - al);
			break;
		}
	}
	/* regular file */
	if (w->ct >= CT_MULTI) {
		quote = 0;
		for (q = s + 1; q < t; ++q) {
			if (isalnumByte(q[-1]))
				continue;
			if (!memEqualCI(q, "boundary=", 9))
				continue;
			q += 9;
			if (*q == '"') {
				quote = *q;
				++q;
			}
			for (al = q; q < t; ++q) {
				if (*q == '"')
					break;
				if (quote)
					continue;
				if (strchr(",; \t", *q))
					break;
			}
			ar = q;
			w->boundlen = ar - al;
			strncpy(w->boundary, al, ar - al);
			break;
		}
	}			/* multi or alt */
}				/* ctExtras */

static void isoDecode(char *vl, char **vrp)
{
	char *vr = *vrp;
	char *start, *end;	/* section being decoded */
	char *s, *t, c, d, code;
	int len;
	uchar val, leftover, mod;

	start = vl;
restart:
	start = strstr(start, "=?");
	if (!start || start >= vr)
		goto finish;
	start += 2;
	if (!memEqualCI(start, "iso-", 4) &&
	    !memEqualCI(start, "us-ascii", 8) &&
	    !memEqualCI(start, "utf-", 4) &&
	    !memEqualCI(start, "cp1252", 6) &&
	    !memEqualCI(start, "gb", 2) && !memEqualCI(start, "windows-", 8))
		goto restart;
	s = strchr(start, '?');
	if (!s || s > vr - 5 || s[2] != '?')
		goto restart;
	code = s[1];
	code = toupper(code);
	if (code != 'Q' && code != 'B')
		goto restart;
	s += 3;
	end = strstr(s, "?=");
	if (!end || end > vr - 2)
		goto restart;

	t = start - 2;

	if (code == 'Q') {
		while (s < end) {
			c = *s++;
			if (c == '=') {
				c = *s;
				d = s[1];
				if (isxdigit(c) && isxdigit(d)) {
					d = fromHex(c, d);
					*t++ = d;
					s += 2;
					continue;
				}
				c = '=';
			}
			*t++ = c;
		}
		goto copy;
	}

/* base64 */
	mod = 0;
	for (; s < end; ++s) {
		c = *s;
		if (isspaceByte(c))
			continue;
		if (c == '=')
			continue;
		val = base64Bits(c);
		if (val & 64)
			val = 0;	/* ignore errors here */
		if (mod == 0) {
			leftover = val << 2;
		} else if (mod == 1) {
			*t++ = (leftover | (val >> 4));
			leftover = val << 4;
		} else if (mod == 2) {
			*t++ = (leftover | (val >> 2));
			leftover = val << 6;
		} else {
			*t++ = (leftover | val);
		}
		++mod;
		mod &= 3;
	}

copy:
	s += 2;
	start = t;
	len = vr - s;
	if (len)
		memmove(t, s, len);
	vr = t + len;
	goto restart;

finish:
	for (s = vl; s < vr; ++s) {
		c = *s;
		if (c == 0 || c == '\t')
			*s = ' ';
	}

	*vrp = vr;
}				/* isoDecode */

/* mail header reformat, to/from utf8 */
static void mhReformat(char *line)
{
	char *tbuf;
	int tlen = strlen(line);
	iuReformat(line, tlen, &tbuf, &tlen);
	if (!tbuf)
		return;
	if (tlen >= MHLINE)
		tbuf[MHLINE - 1] = 0;
	strcpy(line, tbuf);
	nzFree(tbuf);
}				/* mhReformat */

static void extractLessGreater(char *s)
{
	char *vl, *vr;
	vl = strchr(s, '<');
	vr = strchr(s, '>');
	if (vl && vr && vl < vr) {
		*vr = 0;
		strmove(s, vl + 1);
	}
}				/* extractLessGreater */

/* Now that we know it's mail, see what information we can
 * glean from the headers.
 * Returns a pointer to an allocated MHINFO structure.
 * This routine is recursive. */
static struct MHINFO *headerGlean(char *start, char *end)
{
	char *s, *t, *q;
	char *vl, *vr;		/* value left and value right */
	struct MHINFO *w;
	int j, k, n;
	char linetype = 0;

/* defaults */
	w = allocZeroMem(sizeof(struct MHINFO));
	initList(&w->components);
	w->ct = CT_OTHER;
	w->ce = CE_8BIT;
	w->andOthers = false;
	w->tolist = initString(&w->tolen);
	w->cclist = initString(&w->cclen);
	w->start = start, w->end = end;

	for (s = start; s < end; s = t + 1) {
		char quote;
		char first = *s;
		t = strchr(s, '\n');
		if (!t)
			t = end - 1;	/* should never happen */
		if (t == s)
			break;	/* empty line */

		if (first == ' ' || first == '\t') {
			if (linetype == 'c')
				ctExtras(w, s, t);
			if (linetype == 't')
				stringAndBytes(&w->tolist, &w->tolen, s, t - s);
			if (linetype == 'y')
				stringAndBytes(&w->cclist, &w->cclen, s, t - s);
			continue;
		}

/* find the lead word */
		for (q = s; isalnumByte(*q) || *q == '_' || *q == '-'; ++q) ;
		if (q == s)
			continue;	/* should never happen */
		if (*q++ != ':')
			continue;	/* should never happen */
		for (vl = q; *vl == ' ' || *vl == '\t'; ++vl) ;
		for (vr = t; vr > vl && (vr[-1] == ' ' || vr[-1] == '\t');
		     --vr) ;
		if (vr == vl)
			continue;	/* empty */

/* too long? */
		if (vr - vl > MHLINE - 1)
			vr = vl + MHLINE - 1;

/* This is sort of a switch statement on the word */
		if (memEqualCI(s, "subject:", q - s)) {
			linetype = 's';
			if (w->subject[0])
				continue;
/* get rid of forward/reply prefixes */
			for (q = vl; q < vr; ++q) {
				static const char *const prefix[] = {
					"re", "sv", "fwd", 0
				};
				if (!isalphaByte(*q))
					continue;
				if (q > vl && isalnumByte(q[-1]))
					continue;
				for (j = 0; prefix[j]; ++j)
					if (memEqualCI
					    (q, prefix[j], strlen(prefix[j])))
						break;
				if (!prefix[j])
					continue;
				j = strlen(prefix[j]);
				if (!strchr(":-,;", q[j]))
					continue;
				++j;
				while (q + j < vr && q[j] == ' ')
					++j;
				memmove(q, q + j, vr - q - j);
				vr -= j;
				--q;	/* try again */
			}
			isoDecode(vl, &vr);
			strncpy(w->subject, vl, vr - vl);
/* If the subject is really long, spreads onto the next line,
 * I'll just use ... */
			if (t < end - 1 && (t[1] == ' ' || t[1] == '\t'))
				strcat(w->subject, "...");
			mhReformat(w->subject);
			continue;
		}

		if (memEqualCI(s, "reply-to:", q - s)) {
			linetype = 'r';
			if (!w->reply[0])
				strncpy(w->reply, vl, vr - vl);
			continue;
		}

		if (memEqualCI(s, "message-id:", q - s)) {
			linetype = 'm';
			if (!w->mid[0])
				strncpy(w->mid, vl, vr - vl);
			continue;
		}

		if (memEqualCI(s, "references:", q - s)) {
			linetype = 'e';
			if (!w->ref[0])
				strncpy(w->ref, vl, vr - vl);
			continue;
		}

		if (memEqualCI(s, "from:", q - s)) {
			linetype = 'f';
			if (w->from[0])
				continue;
			isoDecode(vl, &vr);
			strncpy(w->from, vl, vr - vl);
			mhReformat(w->from);
			continue;
		}

		if (memEqualCI(s, "date:", q - s)
		    || memEqualCI(s, "sent:", q - s)) {
			linetype = 'd';
			if (w->date[0])
				continue;
/* don't need the weekday, seconds, or timezone */
			if (vr - vl > 5 &&
			    isalphaByte(vl[0]) && isalphaByte(vl[1])
			    && isalphaByte(vl[2]) && vl[3] == ','
			    && vl[4] == ' ')
				vl += 5;
			strncpy(w->date, vl, vr - vl);
			q = strrchr(w->date, ':');
			if (q)
				*q = 0;
			continue;
		}

		if (memEqualCI(s, "to:", q - s)) {
			linetype = 't';
			if (w->tolen)
				stringAndChar(&w->tolist, &w->tolen, ',');
			stringAndBytes(&w->tolist, &w->tolen, q, vr - q);
			if (w->to[0])
				continue;
			strncpy(w->to, vl, vr - vl);
/* Only retain the first recipient */
			quote = 0;
			for (q = w->to; *q; ++q) {
				if (*q == ',' && !quote) {
					w->andOthers = true;
					break;
				}
				if (*q == '"') {
					if (!quote)
						quote = *q;
					else if (quote == *q)
						quote = 0;
					continue;
				}
				if (*q == '<') {
					if (!quote)
						quote = *q;
					continue;
				}
				if (*q == '>') {
					if (quote == '<')
						quote = 0;
					continue;
				}
			}
			*q = 0;	/* cut it off at the comma */
			continue;
		}

		if (memEqualCI(s, "cc:", q - s)) {
			linetype = 'y';
			if (w->cclen)
				stringAndChar(&w->cclist, &w->cclen, ',');
			stringAndBytes(&w->cclist, &w->cclen, q, vr - q);
			w->andOthers = true;
			continue;
		}

		if (memEqualCI(s, "content-type:", q - s)) {
			linetype = 'c';
			if (memEqualCI(vl, "application/pgp-signature", 25))
				w->pgp = true;
			if (memEqualCI(vl, "text", 4))
				w->ct = CT_RICH;
			if (memEqualCI(vl, "text/html", 9))
				w->ct = CT_HTML;
			if (memEqualCI(vl, "text/plain", 10))
				w->ct = CT_TEXT;
			if (memEqualCI(vl, "application", 11))
				w->ct = CT_APPLIC;
			if (memEqualCI(vl, "multipart", 9))
				w->ct = CT_MULTI;
			if (memEqualCI(vl, "multipart/alternative", 21))
				w->ct = CT_ALT;

			ctExtras(w, s, t);
			continue;
		}

		if (memEqualCI(s, "content-transfer-encoding:", q - s)) {
			linetype = 'e';
			if (memEqualCI(vl, "quoted-printable", 16))
				w->ce = CE_QP;
			if (memEqualCI(vl, "7bit", 4))
				w->ce = CE_7BIT;
			if (memEqualCI(vl, "8bit", 4))
				w->ce = CE_8BIT;
			if (memEqualCI(vl, "base64", 6))
				w->ce = CE_64;
			continue;
		}

		linetype = 0;
	}			/* loop over lines */

/* make sure there's room for a final nl */
	stringAndChar(&w->tolist, &w->tolen, ' ');
	stringAndChar(&w->cclist, &w->cclen, ' ');
	extractEmailAddresses(w->tolist);
	extractEmailAddresses(w->cclist);

	w->start = start = s + 1;

/* Fix up reply and from lines.
 * From should be the name, reply the address. */
	if (!w->from[0])
		strcpy(w->from, w->reply);
	if (!w->reply[0])
		strcpy(w->reply, w->from);
	if (w->from[0] == '"') {
		strmove(w->from, w->from + 1);
		q = strchr(w->from, '"');
		if (q)
			*q = 0;
	}
	vl = strchr(w->from, '<');
	vr = strchr(w->from, '>');
	if (vl && vr && vl < vr) {
		while (vl > w->from && vl[-1] == ' ')
			--vl;
		*vl = 0;
	}
	extractLessGreater(w->reply);
/* get rid of (name) comment */
	vl = strchr(w->reply, '(');
	vr = strchr(w->reply, ')');
	if (vl && vr && vl < vr) {
		while (vl > w->reply && vl[-1] == ' ')
			--vl;
		*vl = 0;
	}
/* no @ means it's not an email address */
	if (!strchr(w->reply, '@'))
		w->reply[0] = 0;
	if (stringEqual(w->reply, w->from))
		w->from[0] = 0;
	extractLessGreater(w->to);
	extractLessGreater(w->mid);
	extractLessGreater(w->ref);

	cutDuplicateEmails(w->tolist, w->cclist, w->reply);
	if (debugLevel >= 5) {
		puts("mail header analyzed");
		printf("subject: %s\n", w->subject);
		printf("from: %s\n", w->from);
		printf("date: %s\n", w->date);
		printf("reply: %s\n", w->reply);
		printf("tolist: %s\n", w->tolist);
		printf("cclist: %s\n", w->cclist);
		printf("reference: %s\n", w->ref);
		printf("message: %s\n", w->mid);
		printf("boundary: %d|%s\n", w->boundlen, w->boundary);
		printf("filename: %s\n", w->cfn);
		printf("content %d/%d\n", w->ct, w->ce);
	}

	if (w->ce == CE_QP)
		unpackQP(w);
	if (w->ce == CE_64) {
		w->error64 = base64Decode(w->start, &w->end);
		if (w->error64 != GOOD_BASE64_DECODE)
			mail64Error(w->error64);
	}
	if (w->ce == CE_64 && w->ct == CT_OTHER || w->ct == CT_APPLIC
	    || w->cfn[0]) {
		w->doAttach = true;
		++nattach;
		q = w->cfn;
		if (*q) {	/* name present */
			if (stringEqual(q, "winmail.dat")) {
				w->atimage = true;
				++nimages;
			} else if ((q = strrchr(q, '.'))) {
				static const char *const imagelist[] = {
					"gif", "jpg", "tif", "bmp", "asc",
					"png", 0
				};
/* the asc isn't an image, it's a signature card. */
/* Similarly for the winmail.dat */
				if (stringInListCI(imagelist, q + 1) >= 0) {
					w->atimage = true;
					++nimages;
				}
			}
			if (!w->atimage && nattach == nimages + 1)
				firstAttach = w->cfn;
		}
		return w;
	}

/* loop over the mime components */
	if (w->ct == CT_MULTI || w->ct == CT_ALT) {
		char *lastbound = 0;
		bool endmode = false;
		struct MHINFO *child;
/* We really need the -1 here, because sometimes the boundary will
 * be the very first thing in the message body. */
		s = w->start - 1;
		while (!endmode && (t = strstr(s, "\n--")) && t < end) {
			if (memcmp(t + 3, w->boundary, w->boundlen)) {
				s = t + 3;
				continue;
			}
			q = t + 3 + w->boundlen;
			while (*q == '-')
				endmode = true, ++q;
			if (*q == '\n')
				++q;
			debugPrint(5, "boundary found at offset %d",
				   t - w->start);
			if (lastbound) {
				child = headerGlean(lastbound, t);
				addToListBack(&w->components, child);
			}
			s = lastbound = q;
		}
		w->start = w->end = 0;
		return w;
	}

	/* mime or alt */
	/* Scan through, we might have a mail message included inline */
	vl = 0;			/* first mail header keyword line */
	for (s = start; s < end; s = t + 1) {
		char first = *s;
		t = strchr(s, '\n');
		if (!t)
			t = end - 1;	/* should never happen */
		if (t == s) {	/* empty line */
			if (!vl)
				continue;
/* Do we have enough for a mail header? */
			if (k >= 4 && k * 2 >= j) {
				struct MHINFO *child = headerGlean(vl, end);
				addToListBack(&w->components, child);
				w->end = end = vl;
				goto textonly;
			}	/* found mail message inside */
			vl = 0;
		}		/* empty line */
		if (first == ' ' || first == '\t')
			continue;	/* indented */
		for (q = s; isalnumByte(*q) || *q == '_' || *q == '-'; ++q) ;
		if (q == s || *q != ':') {
			vl = 0;
			continue;
		}
/* looks like header: stuff */
		if (!vl) {
			vl = s;
			j = k = 0;
		}
		++j;
		for (n = 0; mhwords[n]; ++n)
			if (memEqualCI(mhwords[n], s, q - s))
				break;
		if (mhwords[n])
			++k;
	}			/* loop over lines */

/* Header could be at the very end */
	if (vl && k >= 4 && k * 2 >= j) {
		struct MHINFO *child = headerGlean(vl, end);
		addToListBack(&w->components, child);
		w->end = end = vl;
	}

textonly:
/* Any additional processing of the text, from start to end, can go here. */
/* Remove leading blank lines or lines with useless words */
	for (s = start; s < end; s = t + 1) {
		t = strchr(s, '\n');
		if (!t)
			t = end - 1;	/* should never happen */
		vl = s, vr = t;
		if (vr - vl >= 4 && memEqualCI(vr - 4, "<br>", 4))
			vr -= 4;
		while (vl < vr) {
			if (isalnumByte(*vl))
				break;
			++vl;
		}
		while (vl < vr) {
			if (isalnumByte(vr[-1]))
				break;
			--vr;
		}
		if (vl == vr)
			continue;	/* empty */
		if (memEqualCI(vl, "forwarded message", vr - vl))
			continue;
		if (memEqualCI(vl, "original message", vr - vl))
			continue;
		break;		/* something real */
	}
	w->start = start = s;

	return w;
}				/* headerGlean */

static char *headerShow(struct MHINFO *w, bool top)
{
	static char buf[(MHLINE + 30) * 4];
	static char lastsubject[MHLINE];
	char *s;
	bool lines = false;
	buf[0] = 0;

	if (!(w->subject[0] | w->from[0] | w->reply[0]))
		return buf;

	if (!top) {
		strcpy(buf, "Message");
		lines = true;
		if (w->from[0]) {
			strcat(buf, " from ");
			strcat(buf, w->from);
		}
		if (w->subject[0]) {
			if (stringEqual(w->subject, lastsubject)) {
				strcat(buf, " with the same subject");
			} else {
				strcat(buf, " with subject: ");
				strcat(buf, w->subject);
			}
		} else
			strcat(buf, " with no subject");
		if (mailIsHtml) {	/* trash & < > */
			for (s = buf; *s; ++s) {
/* This is quick and stupid */
				if (*s == '<')
					*s = '(';
				if (*s == '>')
					*s = ')';
				if (*s == '&')
					*s = '*';
			}
		}
/* need a dot at the end? */
		s = buf + strlen(buf);
		if (isalnumByte(s[-1]))
			*s++ = '.';
		strcpy(s, mailIsHtml ? "\n<br>" : "\n");
		if (w->date[0]) {
			strcat(buf, "Sent ");
			strcat(buf, w->date);
		}
		if (w->reply) {
			if (!w->date[0])
				strcat(buf, "From ");
			else
				strcat(buf, " from ");
			strcat(buf, w->reply);
		}
		if (w->date[0] | w->reply[0]) {	/* second line */
			strcat(buf, "\n");
		}
	} else {

/* This is at the top of the file */
		if (w->subject[0]) {
			sprintf(buf, "Subject: %s\n", w->subject);
			lines = true;
		}
		if (nattach && ismc) {
			char atbuf[20];
			if (lines & mailIsHtml)
				strcat(buf, "<br>");
			lines = true;
			if (nimages) {
				sprintf(atbuf, "%d images\n", nimages);
				if (nimages == 1)
					strcpy(atbuf, "1 image");
				strcat(buf, atbuf);
				if (nattach > nimages)
					strcat(buf, " + ");
			}
			if (nattach == nimages + 1) {
				strcat(buf, "1 attachment");
				if (firstAttach && firstAttach[0]) {
					strcat(buf, " ");
					strcat(buf, firstAttach);
				}
			}
			if (nattach > nimages + 1) {
				sprintf(atbuf, "%d attachments\n",
					nattach - nimages);
				strcat(buf, atbuf);
			}
			strcat(buf, "\n");
		}
		/* attachments */
		if (w->to[0] && !ismc) {
			if (lines & mailIsHtml)
				strcat(buf, "<br>");
			lines = true;
			strcat(buf, "To ");
			strcat(buf, w->to);
			if (w->andOthers)
				strcat(buf, " and others");
			strcat(buf, "\n");
		}
		if (w->from[0]) {
			if (lines & mailIsHtml)
				strcat(buf, "<br>");
			lines = true;
			strcat(buf, "From ");
			strcat(buf, w->from);
			strcat(buf, "\n");
		}
		if (w->date[0] && !ismc) {
			if (lines & mailIsHtml)
				strcat(buf, "<br>");
			lines = true;
			strcat(buf, "Mail sent ");
			strcat(buf, w->date);
			strcat(buf, "\n");
		}
		if (w->reply[0]) {
			if (lines & mailIsHtml)
				strcat(buf, "<br>");
			lines = true;
			strcat(buf, "Reply to ");
			strcat(buf, w->reply);
			strcat(buf, "\n");
		}
	}

	if (lines)
		strcat(buf, mailIsHtml ? "<P>\n" : "\n");
	strcpy(lastsubject, w->subject);
	return buf;
}				/* headerShow */

/* Depth first block of text determines the type */
static int mailTextType(struct MHINFO *w)
{
	struct MHINFO *v;
	int texttype = CT_OTHER, rc;

	if (w->doAttach)
		return CT_OTHER;

/* jump right into the hard part, multi/alt */
	if (w->ct >= CT_MULTI) {
		foreach(v, w->components) {
			rc = mailTextType(v);
			if (rc == CT_HTML)
				return rc;
			if (rc == CT_OTHER)
				continue;
			if (w->ct == CT_MULTI)
				return rc;
			texttype = rc;
		}
		return texttype;
	}

	/* multi */
	/* If there is no text, return nothing */
	if (w->start == w->end)
		return CT_OTHER;
/* I don't know if this is right, but I override the type,
 * and make it html, if we start out with <html> */
	if (memEqualCI(w->start, "<html>", 6))
		return CT_HTML;
	return w->ct == CT_HTML ? CT_HTML : CT_TEXT;
}				/* mailTextType */

static void formatMail(struct MHINFO *w, bool top)
{
	struct MHINFO *v;
	int ct = w->ct;
	int j, best;

	if (w->doAttach)
		return;
	debugPrint(5, "format headers for content %d subject %s", ct,
		   w->subject);
	stringAndString(&fm, &fm_l, headerShow(w, top));

	if (ct < CT_MULTI) {
		char *start = w->start;
		char *end = w->end;
		int newlen;
/* If mail is not in html, reformat it */
		if (start < end) {
			if (ct == CT_TEXT) {
				breakLineSetup();
				if (breakLine(start, end - start, &newlen)) {
					start = replaceLine;
					end = start + newlen;
				}
			}
			if (mailIsHtml && ct != CT_HTML)
				stringAndString(&fm, &fm_l, "<pre>");
			stringAndBytes(&fm, &fm_l, start, end - start);
			if (mailIsHtml && ct != CT_HTML)
				stringAndString(&fm, &fm_l, "</pre>\n");
		}

		/* text present */
		/* There could be a mail message inline */
		foreach(v, w->components) {
			if (end > start)
				stringAndString(&fm, &fm_l,
						mailIsHtml ? "<P>\n" : "\n");
			formatMail(v, false);
		}

		return;
	}

	if (ct == CT_MULTI) {
		foreach(v, w->components)
		    formatMail(v, false);
		return;
	}

/* alternate presentations, pick the best one */
	best = j = 0;
	foreach(v, w->components) {
		int subtype = mailTextType(v);
		++j;
		if (subtype != CT_OTHER)
			best = j;
		if (mailIsHtml && subtype == CT_HTML ||
		    !mailIsHtml && subtype == CT_TEXT)
			break;
	}

	if (!best)
		best = 1;
	j = 0;
	foreach(v, w->components) {
		++j;
		if (j != best)
			continue;
		formatMail(v, false);
		break;
	}
}				/* formatMail */

/* Browse the email file. */
char *emailParse(char *buf)
{
	struct MHINFO *w, *v;
	nattach = nimages = 0;
	firstAttach = 0;
	mailIsHtml = ignoreImages = false;
	fm = initString(&fm_l);
	w = headerGlean(buf, buf + strlen(buf));
	mailIsHtml = (mailTextType(w) == CT_HTML);
	if (mailIsHtml)
		stringAndString(&fm, &fm_l, "<html>\n");
	formatMail(w, true);
/* Remember, we always need a nonzero buffer */
	if (!fm_l || fm[fm_l - 1] != '\n')
		stringAndChar(&fm, &fm_l, '\n');
	cw->mailInfo =
	    allocMem(strlen(w->ref) + strlen(w->mid) +
		     strlen(w->tolist) + strlen(w->cclist) +
		     strlen(w->reply) + 6);
	sprintf(cw->mailInfo, "%s>%s>%s>%s>%s>", w->reply, w->tolist,
		w->cclist, w->ref, w->mid);
	if (!ismc) {
		writeAttachments(w);
		freeMailInfo(w);
		nzFree(buf);
		debugPrint(5, "mailInfo: %s", cw->mailInfo);
	} else {
		lastMailInfo = w;
		lastMailText = buf;
	}
	return fm;
}				/* emailParse */

/*********************************************************************
Set up for a reply.
This looks at the first 5 lines, which could contain
subject
to
reply to
from
mail send
in no particular order.
Move replyt to the top and get rid of the others.
Then, if you have browsed a mail file,
grab the message id and reference it.
Also, if mailing to all, stick in the other recipients.
*********************************************************************/

bool setupReply(bool all)
{
	int subln, repln;
	char linetype[8];
	int j;
	char *out, *s, *t;
	bool rc;

/* basic sanity */
	if (cw->dirMode) {
		setError(MSG_ReDir);
		return false;
	}

	if (cw->sqlMode) {
		setError(MSG_ReDB);
		return false;
	}

	if (!cw->dol) {
		setError(MSG_ReEmpty);
		return false;
	}

	if (cw->binMode) {
		setError(MSG_ReBinary);
		return false;
	}

	subln = repln = 0;
	strcpy(linetype, " xxxxxx");
	for (j = 1; j <= 6; ++j) {
		if (j > cw->dol)
			break;

		char *p = (char *)fetchLine(j, -1);

		if (memEqualCI(p, "subject:", 8)) {
			linetype[j] = 's';
			subln = j;
			continue;
		}

		if (memEqualCI(p, "to ", 3)) {
			linetype[j] = 't';
			continue;
		}

		if (memEqualCI(p, "from ", 5)) {
			linetype[j] = 'f';
			continue;
		}

		if (memEqualCI(p, "mail sent ", 10)) {
			linetype[j] = 'w';
			continue;
		}

		if (memEqualCI(p, "references:", 11)) {
			linetype[j] = 'v';
			continue;
		}

		if (memEqualCI(p, "reply to ", 9)) {
			linetype[j] = 'r';
			repln = j;
			continue;
		}

/* This one has to be last. */
		while (isdigitByte(*p))
			++p;
		if (memEqualCI(p, " attachment", 11)
		    || memEqualCI(p, " image", 6)) {
			linetype[j] = 'a';
			continue;
		}

		break;
	}

	if (!subln || !repln) {
		setError(MSG_ReSubjectReply);
		return false;
	}

/* delete the lines we don't need */
	linetype[j] = 0;
	for (--j; j >= 1; --j) {
		if (strchr("srv", linetype[j]))
			continue;
		delText(j, j);
		strmove(linetype + j, linetype + j + 1);
	}

/* move reply to 1, if it isn't already there */
	repln = strchr(linetype, 'r') - linetype;
	subln = strchr(linetype, 's') - linetype;
	if (repln != 1) {
		struct lineMap *map = cw->map;
		struct lineMap swap;
		struct lineMap *q1 = map + 1;
		struct lineMap *q2 = map + repln;
		swap = *q1;
		*q1 = *q2;
		*q2 = swap;
		if (subln == 1)
			subln = repln;
		repln = 1;
	}

	j = strlen(linetype) - 1;
	if (j != subln) {
		struct lineMap *map = cw->map;
		struct lineMap swap;
		struct lineMap *q1 = map + j;
		struct lineMap *q2 = map + subln;
		swap = *q1;
		*q1 = *q2;
		*q2 = swap;
	}

	readReplyInfo();

	if (!cw->mailInfo) {
		if (all) {
			setError(MSG_ReNoInfo);
			return false;
		}
		return true;	/* that's all we can do */
	}

/* Build the header lines and put them in the buffer */
	out = initString(&j);
/* step through the to list */
	s = strchr(cw->mailInfo, '>') + 1;
	while (*s != '>') {
		t = strchr(s, ',');
		if (all) {
			stringAndString(&out, &j, "to: ");
			stringAndBytes(&out, &j, s, t - s);
			stringAndChar(&out, &j, '\n');
		}
		s = t + 1;
	}

/* step through the cc list */
	++s;
	while (*s != '>') {
		t = strchr(s, ',');
		if (all) {
			stringAndString(&out, &j, "cc: ");
			stringAndBytes(&out, &j, s, t - s);
			stringAndChar(&out, &j, '\n');
		}
		s = t + 1;
	}

	++s;
	t = strchr(s, '>');
	if (t[1] == '>') {
		i_puts(MSG_ReNoID);
	} else {
		stringAndString(&out, &j, "References: <");
		if (*s != '>') {
			stringAndBytes(&out, &j, s, t - s);
			stringAndString(&out, &j, "> <");
		}
		stringAndString(&out, &j, t + 1);
		stringAndChar(&out, &j, '\n');
	}

	rc = true;
	if (j)
		rc = addTextToBuffer((unsigned char *)out, j, 1, false);
	nzFree(out);
	return rc;
}				/* setupReply */

static void writeReplyInfo(const char *addstring)
{
	int rfh;		/* reply file handle */
	rfh = open(mailReply, O_WRONLY | O_APPEND | O_CREAT, 0666);
	if (rfh < 0)
		return;
	write(rfh, addstring, 12);
	write(rfh, cw->mailInfo, strlen(cw->mailInfo));
	write(rfh, "\n", 1);
	close(rfh);
}				/* writeReplyInfo */

static void readReplyInfo(void)
{
	int rfh;		/* reply file handle */
	char *p;
	int ln, major, minor;
	char prestring[20];
	char *buf;
	int buflen;
	char *s, *t, *cut;

	if (cw->mailInfo)
		return;		/* already there */

/* scan through the buffer looking for the Unformatted line,
 * but stop if you hit an email divider. */
	for (ln = 1; ln <= cw->dol; ++ln) {
		p = fetchLine(ln, -1);
		if (!memcmp
		    (p,
		     "======================================================================\n",
		     71))
			return;
		if (pstLength((pst) p) == 24
		    && sscanf(p, "Unformatted %d.%d", &major, &minor) == 2)
			goto found;
	}
	return;

found:
/* prestring is the key */
	sprintf(prestring, "%05d.%05d:", major, minor);
	rfh = open(mailReply, O_RDONLY);
	if (rfh < 0)
		return;
	if (!fdIntoMemory(rfh, &buf, &buflen))
		return;
	close(rfh);

/* loop through lines looking for the key */
	for (s = buf; *s; s = t + 1) {
		t = strchr(s, '\n');
		if (!t)
			break;
		if (memcmp(s, prestring, 12))
			continue;
/* key match, put this string into mailInfo */
		s += 12;
		cw->mailInfo = pullString(s, t - s);
		break;
	}

	nzFree(buf);
}				/* readReplyInfo */
