/* fetchmail.c
 * Get mail using the pop3 protocol.
 * Format the mail in ascii, or in html.
 * Unpack attachments.
 */

#include "eb.h"

#define MHLINE 512		// length of a mail header line
// headers and other information about an email
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
// recipients and cc recipients
	char *tolist, *cclist;
	int tolen, cclen;
	char mid[MHLINE];	// message id
	char ref[MHLINE];	// references
	char cfn[MHLINE];	// content file name
	uchar ct, ce;		// content type, content encoding
	bool andOthers;
	bool doAttach;
	bool atimage;
	bool pgp;
	bool dispat;
	uchar error64;
	bool startAllocated;
};

static int nattach;		// number of attachments
static int nimages;		// number of attached images
static char *firstAttach;	// name of first file
static bool mailIsHtml;
static bool preferPlain;
static char *fm;		// formatted mail string
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
	if(w->startAllocated) nzFree(w->start);
	nzFree(w);
}

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
			int length = w->end - w-> start;
			if (!looksBinary((uchar *) w->start, length)) {
				diagnoseAndConvert(&w->start, &w->startAllocated, &length, true, true);
				w->end = w->start + length; // in case of realloc
	} else if (binaryDetect & !cw->binMode) {
		if(debugLevel >= 1)
			i_puts(MSG_BinaryData);
		cw->binMode = true;
	}
			if (!addTextToBuffer
			    ((pst) w->start, length, 0, false))
				i_printf(MSG_AttNoCopy, cx);
			else {
				if (debugLevel >= 1)
					printf("%d\n", length);
				if (w->cfn[0])
					cf->fileName = cloneString(w->cfn);
				cw->changeMode = false;
			}
			cxSwitch(svcx, false);	/* back to where we were */
		}
	} else if (!stringEqual(atname, "x")) {
		int fh =
		    open(atname, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC,
			 MODE_rw);
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
}

static void writeAttachments(struct MHINFO *w)
{
	struct MHINFO *v;
	if (w->doAttach) {
		writeAttachment(w);
	} else {
		foreach(v, w->components)
		    writeAttachments(v);
	}
}

/* string to hold the returned data from the mail server */
static char *mailstring;
static int mailstring_l;
static char *mailbox_url, *message_url;

int imapfetch = 100;
static bool earliest;
static const char envelopeFormatChars[] = "tfsdz";
// to from subject date size
static char envelopeFormat[6] = {'f', 's'}; // default is from subject
static char envelopeFormatDef[6] = { 'f', 's'};

bool setEnvelopeFormat(const char *s)
{
	char c;
	bool something = false;
	int i, j;
	char count[6];

// check
	for(j=0; (c = s[j]); ++j) {
		if(isspaceByte(c))
			continue;
		something = true;
		if(!strchr(envelopeFormatChars, c))
			return false;
	}

// e command alone resurrects the default
	if(!something) {
		strcpy(envelopeFormat, envelopeFormatDef);
		return true;
	}

	memset(count, 0, sizeof(count));
	for(i=0; (c = *s); ++s) {
		if(isspaceByte(c))
			continue;
		j = strchr(envelopeFormatChars, c) - envelopeFormatChars;
		if(count[j])
			continue;
		count[j] = 1;
		envelopeFormat[i++] = c;
	}
	envelopeFormat[i] = 0;
	return true;
}

static void setLimit(const char *t)
{
	char early = false;
	skipWhite(&t);
	if(*t == '-')
		early = true, ++t;
	if (!isdigitByte(*t)) {
		i_puts(MSG_NumberExpected);
		return;
	}
	imapfetch = atoi(t);
	earliest = early;
	if (imapfetch < 5) imapfetch = 5;
	i_printf(MSG_FetchN, imapfetch);
}

/* mail message in a folder */
struct MIF {
	int uid;
	int seqno;
	int size;
	char *cbase;		/* allocated string containing the following */
	char *subject, *from, *to, *reply;
// references, principal recipient, carbon recipient
	char *refer, *prec, *ccrec;
	time_t sent;
	bool seen, gone, line2;
};

/* folders at the top of an imap system */
static struct FOLDER {
	const char *name;
	const char *path;
	bool children;
	int nmsgs;		/* number of messages in this folder */
	int nfetch;		/* how many to fetch */
	int unread;		/* how many not yet seen */
	int start;
	int uidnext;		/* uid of next message */
	struct MIF *mlist;	/* allocated */
} *topfolders;

static int n_folders;
static char *tf_cbase;		/* base of strings for folder names and paths */
static bool move_capable = false;

static void examineFolder(CURL * handle, struct FOLDER *f, bool dostats);

/* This routine mucks with the passed in string, which was allocated
 * to receive data from the imap server. So leave it allocated. */
static void setFolders(CURL * handle)
{
	struct FOLDER *f;
	char *s, *t;
	char *child, *lbrk;
	char qc;		/* quote character */
	int i;

// Reset things, in case called on refresh
	nzFree(topfolders);
	topfolders = 0;
	nzFree(tf_cbase);
	tf_cbase = 0;
	n_folders = 0;

	s = mailstring;
	while ((t = strstr(s, "LIST (\\"))) {
		s = t + 7;
		++n_folders;
	}

	topfolders = allocZeroMem(n_folders * sizeof(struct FOLDER));

	f = topfolders;
	s = mailstring;
	while ((t = strstr(s, "LIST (\\"))) {
		s = t + 6;
		lbrk = strpbrk(s, "\r\n");	// line break
		if (!lbrk)
			continue;
		child = strstr(s, "Children");
		if (child && child < lbrk) {
/* HasChildren or HasNoChildren */
			f->children = (child[-1] == 's');
			t = child + 8;
		} else {
// Try another one.
			child = strstr(s, "Inferiors");
			if (!child || child > lbrk)
				continue;
			t = child + 10;
		}
		while (*t == ' ')
			++t;
		while (*child != '\\')
			--child;
		if (child < s)	/* should never happen */
			child = s;
		strmove(child, t);
		lbrk = strpbrk(s, "\r\n");	// recalculate
		if (*s == '\\') {	/* there's a name */
			++s;
			t = strchr(s, ')');
			if (!t || t > lbrk)
				continue;
			while (t > s && t[-1] == ' ')
				--t;
			*t = 0;
			f->name = s;
			s = t + 1;
// \Noselect \UnMarked  not sure what this means but it derails,
// we need to focus on Noselect
			t = strchr(f->name, '\\');
			if(t) {
				while(t > f->name && t[-1] == ' ') --t;
				*t = 0;
			}
/* the null folder at the top, like /, isn't really a folder. */
			if (stringEqual(f->name, "Noselect"))
				continue;
		} else
			f->name = emptyString;
/* now get the path */
		t = lbrk;
		qc = ' ';
		s = t - 1;
		if (*s == '"')
			qc = '"', --s, --t;
		for (; s > child && *s != qc; --s) ;
		if (s == child)
			continue;
		f->path = ++s;
		*t = 0;
		if (s == t)
			continue;
		s = t + 1;
/* successfully built this folder, move on to the next one */
		++f;
	}

	n_folders = f - topfolders;

/* You don't dare free mailstring, because it's pieces
 * are now part of the folders, name and path etc. */
	tf_cbase = mailstring;
	mailstring = 0;

	f = topfolders;
	for (i = 0; i < n_folders; ++i, ++f)
		examineFolder(handle, f, true);
}

static struct FOLDER *folderByName(char *line)
{
	int i, j;
	struct FOLDER *f;
	int cnt = 0;

	stripWhite(line);
	if (!line[0])
		return 0;

	i = stringIsNum(line);
	if (i > 0 && i <= n_folders)
		return topfolders + i - 1;

	f = topfolders;
	for (i = 0; i < n_folders; ++i, ++f)
		if (strcasestr(f->path, line))
			++cnt, j = i;
	if (cnt == 1)
		return topfolders + j;
	if (cnt)
		i_printf(MSG_ManyFolderMatch, line);
	else
		i_printf(MSG_NoFolderMatch, line);
	return 0;
}

/* data block for the curl ccallback write function in http.c */
static struct i_get callback_data;

static CURLcode getMailData(CURL * handle)
{
	static bool first_call = true;
	CURLcode res;
//  puts("get data");
	callback_data.buffer = initString(&callback_data.length);
	callback_data.move_capable = false;
	res = curl_easy_perform(handle);
	mailstring = callback_data.buffer;
	mailstring_l = callback_data.length;
	callback_data.buffer = 0;
	if (first_call) {
		move_capable = callback_data.move_capable;
		if (debugLevel < 4)
			curl_easy_setopt(handle, CURLOPT_VERBOSE, 0);
		debugPrint(3, "imap is %smove capable",
			   (move_capable ? "" : "not "));
		first_call = false;
	}
	return res;
}

/*********************************************************************
imap emails come in through the headers, not the data.
No kidding! I don't understand it either.
This callback function doesn't use a data block, mailstring is assumed.
*********************************************************************/

static size_t imap_header_callback(char *i, size_t size,
				   size_t nitems, void *data)
{
	size_t b = nitems * size;
	int dots1, dots2;
	dots1 = mailstring_l / CHUNKSIZE;
	stringAndBytes(&mailstring, &mailstring_l, i, b);
	dots2 = mailstring_l / CHUNKSIZE;
	if (dots1 < dots2) {
		for (; dots1 < dots2; ++dots1)
			putchar('.');
		fflush(stdout);
	}
	return b;
}

/* after the email has been fetched via pop3 or imap */
static void undosOneMessage(void)
{
	int j, k;

	if (mailstring_l >= CHUNKSIZE)
		nl();		/* We printed dots, so we terminate them with newline */

/* Remove DOS newlines. */
	for (j = k = 0; j < mailstring_l; j++) {
		if (mailstring[j] == '\r' && j < mailstring_l - 1
		    && mailstring[j + 1] == '\n')
			continue;
		mailstring[k++] = mailstring[j];
	}
	mailstring_l = k;
	mailstring[k] = 0;
}

static char presentMail(void);
static void envelopes(CURL * handle, struct FOLDER *f);
static void isoDecode(char *vl, char **vrp);

static void cleanFolder(struct FOLDER *f)
{
	int j;
	struct MIF *mif;
	if (!f->mlist)
		return;
	mif = f->mlist;
	for (j = 0; j < f->nfetch; ++j, ++mif)
		nzFree(mif->cbase);
	nzFree(f->mlist);
	f->mlist = NULL;
	f->nmsgs = f->nfetch = f->unread = 0;
}

// search through imap server for a particular string.
// Return 1 if the search ran successfully and found some messages.
// Return 0 for no messages and -1 for error.
static int imapSearch(CURL * handle, struct FOLDER *f, char *line)
{
	char searchtype = 's';
	char *t, *u;
	CURLcode res;
	int cnt;
	struct MIF *mif;
	char cust_cmd[200];

	if (*line && line[1] == ' ' && strchr("sfb", *line)) {
		searchtype = *line;
		line += 2;
	} else if (line[0] && !isspaceByte(line[0]) &&
		   (isspaceByte(line[1]) || !line[1])) {
		i_puts(MSG_ImapSearchHelp);
		return 0;
	}

	stripWhite(line);
	if (!line[0]) {
		i_puts(MSG_Empty);
		return 0;
	}

	if (strchr(line, '"')) {
		i_puts(MSG_SearchQuote);
		return 0;
	}

	strcpy(cust_cmd, "SEARCH ");
	if (cons_utf8)
		strcat(cust_cmd, "CHARSET UTF-8 ");
	if (searchtype == 's')
		strcat(cust_cmd, "SUBJECT");
	if (searchtype == 'f')
		strcat(cust_cmd, "FROM");
	if (searchtype == 'b')
		strcat(cust_cmd, "BODY");
	strcat(cust_cmd, " \"");
	strcat(cust_cmd, line);
	strcat(cust_cmd, "\"");
	curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, cust_cmd);
	res = getMailData(handle);
	if (res != CURLE_OK) {
		nzFree(mailstring);
		ebcurl_setError(res, mailbox_url, 1, emptyString);
		return -1;
	}

	t = strstr(mailstring, "SEARCH ");
	if (!t) {
none:
		nzFree(mailstring);
		i_puts(MSG_NoMatch);
		return 0;
	}
	t += 6;
	cnt = 0;
	while (*t == ' ')
		++t;
	u = t;
	while (*u) {
		if (!isdigitByte(*u))
			break;
		++cnt;
		while (isdigitByte(*u))
			++u;
		skipWhite2(&u);
	}
	if (!cnt)
		goto none;

	cleanFolder(f);

	f->nmsgs = f->nfetch = cnt;
	if (cnt > imapfetch)
		f->nfetch = imapfetch;
	f->mlist = allocZeroMem(sizeof(struct MIF) * f->nfetch);
	mif = f->mlist;
	u = t;
	while (*u && cnt) {
		int seqno = strtol(u, &u, 10);
		if (cnt <= f->nfetch) {
			mif->seqno = seqno;
			++mif;
		}
		--cnt;
		skipWhite2(&u);
	}

	envelopes(handle, f);

	return 1;
}

static void printEnvelope(const struct MIF *mif)
{
	char *envp;		// print the envelope concisely
	char *envp_end;
	int envp_l;
	int i;
	char c;
	envp = initString(&envp_l);
#if 0
	if (!mif->seen)
		stringAndChar(&envp, &envp_l, '*');
#endif
	if(mif->line2 && debugLevel >= 3)
		stringAndString(&envp, &envp_l, "~ ");

	for (i = 0; (c = envelopeFormat[i]); ++i) {
		if(i)
			stringAndString(&envp, &envp_l, " | ");
		switch(c) {
		case 'f':
			stringAndString(&envp, &envp_l, mif->from[0] ? mif->from : mif->reply);
			break;
		case 't':
			stringAndString(&envp, &envp_l, mif->to[0] ? mif->to : mif->prec);
			break;
		case 's':
			stringAndString(&envp, &envp_l, mif->subject);
			break;
		case 'd':
			if (mif->sent)
				stringAndString(&envp, &envp_l, conciseTime(mif->sent));
			break;
		case 'z':
			stringAndString(&envp, &envp_l, conciseSize(mif->size));
			break;
		}
	}

	envp_end = envp + envp_l;
	isoDecode(envp, &envp_end);
	*envp_end = 0;

// Resulting line could contain utf8, use the portable puts routine.
	eb_puts(envp);
	nzFree(envp);
}

static void viewAll(struct FOLDER *f)
{
	int j;
	struct MIF *mif = f->mlist;
	for (j = 0; j < f->nfetch; ++j, ++mif) {
		if (!mif->gone)
			printEnvelope(mif);
	}
}

static bool expunge(CURL * handle)
{
	CURLcode res;
	curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "EXPUNGE");
	mailstring = initString(&mailstring_l);
	res = getMailData(handle);
	nzFree(mailstring);
	return res == CURLE_OK;
}

static bool bulkMoveDelete(CURL * handle, struct FOLDER *f,
			  struct MIF *this_mif, char key, char subkey,
			  struct FOLDER *destination)
{
	CURLcode res;
	char cust_cmd[80];
	int j;
	struct MIF *mif = f->mlist;
	int ex_cnt = 0;		// expunge count
	char *t, *fromline = 0;
	bool marked_for_delete = false;

	if (key == 'f') {
		fromline = this_mif->from;
		if (!fromline[0]) {
			i_puts(MSG_NoFromLine);
			return 0;
		}
	}

	for (j = 0; j < f->nfetch; ++j, ++mif) {
		bool delflag = false;
		if (mif->gone)
			continue;
		mif->seqno -= ex_cnt;
		if (fromline && !stringEqual(fromline, mif->from))
			continue;
		if (subkey == 'm') {
			if (asprintf(&t, "%s %d \"%s\"",
				     (move_capable ? "MOVE" : "COPY"),
				     mif->seqno, destination->path) == -1)
				i_printfExit(MSG_MemAllocError, 24);
			curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, t);
			free(t);
			res = getMailData(handle);
			nzFree(mailstring);
			if (res != CURLE_OK)
				goto abort;
// The move command shifts all the sequence numbers down.
			if (move_capable) {
				mif->gone = true;
				++ex_cnt;
			} else {
// we did copy not move, so delete
				delflag = true;
			}
		}

		if (subkey == 'd' || delflag) {
			sprintf(cust_cmd, "STORE %d +Flags \\Deleted",
				mif->seqno);
			curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST,
					 cust_cmd);
			res = getMailData(handle);
			nzFree(mailstring);
			if (res != CURLE_OK)
				goto abort;
			mif->gone = true;
			marked_for_delete = true;

/*********************************************************************
We have thus far marked thismessage for delete, but it's not really gone
until we expunge. In contrast, moving a message always expunges it.
It is after all somewhere else.
Expunge means the message is not here, and the server resequences the numbers,
and we have to do the same in our representation of the folder.
If we don't stay in sync with the server, the next command could read or move
or delete the wrong message. Ouch!
ex_cnt is an expunge count, how many messages expunged,
how far should we shift our sequence numbers to stay in sync with the server?
At this point, this message is marked for deletion but not expunged,
don't increment ex_cnt.
At the end of the folder we can command the server to expunge,
and the deleted messages will all go away.
Fine, but, if you issue move and delete commands in one pass,
imap servers behave in very different ways.
Sometimes the move command expunges messages that were marked for deletion,
along with the message being moved.
Sometimes it doesn't. And sometimes it depends
on whether the deleted message comes before or after the moved message.
And sometimes it depends on the folder.
We can't possibly predict the behavior of the server,
and adjust our sequence numbers accordingly.
There are two ways around this: the right way and the easy way.
We can watch the curl traffic and look for
* n EXPUNGE
and each time we see that, decrement the sequence numbers beyond n,
for message n is gone.
Or, we can expunge after every delete. That forces the server
to behave in a predictable manner, whence we can follow along.
And that's what I do.
Unfortunately, d is now an irreversible command.
Type d and the message is gone.
You can't say "oops I didn't mean to delete that" and q to quit
and log back in and it will still be there.
A hybrid solution might be to remember the deletes and then expunge at end of
folder or when a move command is issued.
Now d is reversible again.
I may implement that some day, but it's not as easy as it seems.
On some servers move is actually copy + delete, and in that case
we want to delete + expunge.
So 'd' means mark for delete; delete implied from 'm' means delete + expunge.
I haven't worked all this out, so for now let's just expunge every time.
A hybrid solution wouldn't be implemented here anyways,
this is bulk move or bulk delete.
Bear in mind, d irreversible is consistent with the pop3 interface,
so maybe it's not such a bad thihng.
Now - all that said, we don't have to do the delete expunge here,
because it isn't a mix of move and delete commands.
It is either bulk move or bulk delete.
We may have deleted or moved before this bulk command was issued,
but thence we are in sync with the server.
So just mark for delete and expunge at the end,
like we did before.
*********************************************************************/

#if 0
			debugPrint(3, "` %d EXPUNGE", mif->seqno);
			if(!expunge(handle)) goto abort;
			++ex_cnt;
#endif
		}
	}

	if(marked_for_delete) {
		debugPrint(3, "` EXPUNGE");
		if(!expunge(handle)) goto abort;
	}
	return true;

abort:
	return false;
}


/* scan through the messages in a folder */
static char postkey;
static void scanFolder(CURL * handle, struct FOLDER *f)
{
	struct MIF *mif, *mif2;
	int j, j2, rc;
	CURLcode res = CURLE_OK;
	char *t;
	char key;
	char cust_cmd[80];
	char inputline[80];
	bool delflag;

	if (!f->nmsgs) {
		i_puts(MSG_NoMessages);
		return;
	}

/* tell the server to descend into this folder */
	if (asprintf(&t, "SELECT \"%s\"", f->path) == -1)
		i_printfExit(MSG_MemAllocError, strlen(f->path) + 12);
	curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, t);
	free(t);
	res = getMailData(handle);
	nzFree(mailstring);
	if (res != CURLE_OK) {
abort:
		ebcurl_setError(res, mailbox_url, 1, emptyString);
		i_puts(MSG_EndFolder);
		return;
	}

showmessages:

	mif = f->mlist;
	for (j = 0; j < f->nfetch; ++j, ++mif) {
		if (mif->gone)
			continue;
reaction:
		printEnvelope(mif);
action:
		delflag = false;
		postkey = 0;
		preferPlain = false;
		printf("? ");
		fflush(stdout);
		key = getLetter("h?qvbefdsmnp gtwWuUa/");
		printf("\b\b\b");
		fflush(stdout);
		if (key == '?' || key == 'h') {
			i_puts(MSG_ImapMessageHelp);
			goto action;
		}

		if (key == 'q') {
			i_puts(MSG_Quit);
imap_done:
			curl_easy_cleanup(handle);
			exit(0);
		}

		if (key == '/') {
			i_printf(MSG_Search);
			fflush(stdout);
			if (!fgets(inputline, sizeof(inputline), stdin))
				goto imap_done;
			j2 = imapSearch(handle, f, inputline);
			if(j2 == 0) goto reaction;
			if(j2 < 0) goto abort;
			if (f->nmsgs > f->nfetch)
				i_printf(MSG_ShowLast + earliest, f->nfetch, f->nmsgs);
			else
				i_printf(MSG_MessagesX, f->nmsgs);
			goto showmessages;
		}

		if(strchr("wWuUa", key))
			postkey = key, key = ' ';

dispmail:
		if (key == ' ' || key == 'g' || key == 't') {
			if(key == 't') preferPlain = true;
// download the email from the imap server
			sprintf(cust_cmd, "FETCH %d BODY[]", mif->seqno);
			curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST,
					 cust_cmd);
			mailstring = initString(&mailstring_l);

/*********************************************************************
I wanted to turn the write function off here, because we don't need it.
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, NULL);
But turning it off and leaving HEADERFUNCTION on causes a seg fault,
I have no idea why.
I have to leave them both on.
Unless I write more specialized code, this brings in both data and header info,
so I have to strip some things off after the fact.
You'll see this after the perform function runs.
*********************************************************************/

			curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION,
					 imap_header_callback);
			callback_data.buffer =
			    initString(&callback_data.length);
			res = curl_easy_perform(handle);
/* and put things back */
			nzFree(callback_data.buffer);
			curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, NULL);
			undosOneMessage();
			if (res != CURLE_OK) {
// I don't know why the read message didn't work, probably connection error,
// curl is probably going to log back in, so abort this folder.
				nzFree(mailstring);
				goto abort;
			}

/* have to strip 2 fetch BODY lines off the front,
 * and ) A018 OK off the end. */
			t = strchr(mailstring, '\n');
			if (t)
				t = strchr(t + 1, '\n');
			if (t) {
				++t;
				mailstring_l -= (t - mailstring);
				strmove(mailstring, t);
			}
			t = mailstring + mailstring_l;
			if (t > mailstring && t[-1] == '\n')
				t[-1] = 0, --mailstring_l;
			t = strrchr(mailstring, '\n');
			if (t && t - 2 >= mailstring &&
			    !strncmp(t - 2, "\n)\n", 3) && strstr(t, " OK ")) {
				--t;
				*t = 0;
				mailstring_l = t - mailstring;
			}

			key = presentMail();
/* presentMail has already freed mailstring */
			postkey = 0;
			if(key == 'g') goto dispmail;
			if(strchr("wua", key)) goto reaction;
		}

		if (key == 'p') {
			j2 = j;
			while(--j >= 0) {
				--mif;
				if(!mif->gone)
					break;
			}
			if(j >= 0)
				goto reaction;
			i_puts(MSG_NoPrevMail);
			mif = f->mlist + (j = j2);
			goto reaction;
		}

		if (key == 'v') {
			static const char delim[] = "----------";
			puts(delim);
			viewAll(f);
			puts(delim);
			goto reaction;
		}

		if (key == 'b' || key == 'f') {
			char subkey;
			struct FOLDER *g = 0;
			if (key == 'b')
				i_printf(MSG_Batch);
			else
				i_printf(MSG_From, mif->from);
			fflush(stdout);
			subkey = getLetter("mdx");
			printf("\b");
			if (subkey == 'x') {
				i_puts(MSG_Abort);
				goto reaction;
			}
			if (subkey == 'd')
				i_puts(MSG_Delete);
			if (subkey == 'm') {
				i_printf(MSG_MoveTo);
				fflush(stdout);
				if (!fgets(inputline, sizeof(inputline), stdin))
					goto imap_done;
				g = folderByName(inputline);
				if (!g || g == f) {
// should have a better error message here.
					i_puts(MSG_Abort);
					goto reaction;
				}
			}
			if(!bulkMoveDelete(handle, f, mif, key, subkey, g))
				goto abort;
// If current mail is gone, step ahead.
			if (mif->gone)
				continue;
			goto reaction;
		}

		if (key == 's') {
			i_puts(MSG_Stop);
			break;
		}

		if (key == 'l') {
			i_printf(MSG_Limit);
			fflush(stdout);
			if (!fgets(inputline, sizeof(inputline), stdin))
				goto imap_done;
			setLimit(inputline);
			goto action;
		}

		if (key == 'e') {
			i_printf(MSG_Envelope);
			fflush(stdout);
			if (!fgets(inputline, sizeof(inputline), stdin))
				goto imap_done;
			setEnvelopeFormat(inputline);
			goto reaction;
		}

		if (key == 'm') {
			struct FOLDER *g;
			i_printf(MSG_MoveTo);
			fflush(stdout);
			if (!fgets(inputline, sizeof(inputline), stdin))
				goto imap_done;
			g = folderByName(inputline);
			if (g && g != f) {
				if (asprintf(&t, "%s %d \"%s\"",
					     (move_capable ? "MOVE" : "COPY"),
					     mif->seqno, g->path) == -1)
					i_printfExit(MSG_MemAllocError, 24);
				curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST,
						 t);
				free(t);
				res = getMailData(handle);
				nzFree(mailstring);
				if (res != CURLE_OK)
					goto abort;
// The move command shifts all the sequence numbers down.
				if (move_capable) {
					mif->gone = true;
					j2 = j + 1;
					mif2 = mif + 1;
					while (j2 < f->nfetch) {
						--mif2->seqno;
						++j2, ++mif2;
					}
				} else {
					delflag = true;
				}
			} else goto action;
		}

		if (key == 'd') {
			delflag = true;
			i_puts(MSG_Delete);
		}

		if (!delflag)
			continue;
		sprintf(cust_cmd, "STORE %d +Flags \\Deleted", mif->seqno);
		curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, cust_cmd);
		res = getMailData(handle);
		nzFree(mailstring);
		if (res != CURLE_OK)
			goto abort;
		mif->gone = true;
		debugPrint(3, "` %d EXPUNGE", mif->seqno);
		if(!expunge(handle)) goto abort;
		j2 = j + 1;
		mif2 = mif + 1;
		while (j2 < f->nfetch) {
			--mif2->seqno;
			++j2, ++mif2;
		}
	}

	puts("end of folder");
}

// \" is an escaped quote inside the string.
static char *nextRealQuote(char *p)
{
	char *q = p, *r;
	char c;
	for (; (c = *p); ++p) {
		if (c == '\\' && p[1] == '"') {
			*q++ = '"';
			++p;
			continue;
		}
		if (c == '"')
			break;
		*q++ = c;
	}
	if (!c)
		return 0;
	for (r = q; q <= p; ++q)
		*q = ' ';
	return r;
}

static bool grabEmailFromEnvelope(char **t0,
char **name, char **addr)
{
	char *t = *t0, *u;
	if(!strncmp(t, "NIL ", 4)) {
		t += 4;
	} else if(*t == '{') {
		 int l = strtol(t+1, &t, 10);
		if(*t == '}') ++t;
		// with number in braces, subject is on next line.
// isspace takes us past crlf
		while(isspaceByte(*t)) ++t;
		u = t + l;
		*u = 0;
// allow for quotes either side
		if(*t == '"' && u[-1] == '"') ++t, u[-1] = 0;
		if(name) *name = t;
		*t0 = t = u + 2;
	} else {
		if(*t != '"') return false;
		u = nextRealQuote(++t);
		if (!u)
			return false;
		*u = 0;
		if(name) *name = t;
		*t0 = t = u + 2;
	}
// next should come NIL "email" "domain"
	if (strncmp(t, "NIL \"", 5))
		return false;
	t += 5;
	u = strchr(t, '"');
	if (!u)
		return false;
	*u = '@';
	u += 2;
	if (*u != '"')
		return false;
	strmove(strchr(t, '@') + 1, u + 1);
	u = strchr(t, '"');
	if (!u)
		return false;
	if(addr)
		*addr = t;
	*u = 0;
	*t0 = u + 1;
	return true;
}

static void envelopes(CURL * handle, struct FOLDER *f)
{
	int j;
	char *t, *u;
	CURLcode res;
	int sublength;
	char cust_cmd[80];

	for (j = 0; j < f->nfetch; ++j) {
		struct MIF *mif = f->mlist + j;

#if 0
// nobody is using the uid, don't fetch it, save time.
		sprintf(cust_cmd, "FETCH %d UID", mif->seqno);
		curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, cust_cmd);
		res = getMailData(handle);
		if (res != CURLE_OK)
			goto abort;
		t = strstr(mailstring, "FETCH (UID ");
		if (t) {
			t += 11;
			while (*t == ' ')
				++t;
			if (isdigitByte(*t))
				mif->uid = atoi(t);
		}
		nzFree(mailstring);
#endif

		sprintf(cust_cmd, "FETCH %d ALL", mif->seqno);
		curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, cust_cmd);

/*********************************************************************
Originally I used the WRITEFUNCTION to ge the enveloope data.
That returns the untagged line, and that was right 99% of the time.
That held the entire envelope.
But once ina while the envelope continued on the next line,
the bodyline, the other line, whatever you call it.
For that eventuality I have to use the HEADERFUNCTION as well.
Both do the same thing, the same function, gather data.
But now I get the untagged line twice.
We'll deal with that later.
*********************************************************************/

		curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, eb_curl_callback);
		curl_easy_setopt(handle, CURLOPT_HEADERDATA, &callback_data);
		res = getMailData(handle);
		curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, NULL);
		curl_easy_setopt(handle, CURLOPT_HEADERDATA, NULL);
		if (res != CURLE_OK) {
			ebcurl_setError(res, mailbox_url, 2, emptyString);
		}

/* have to strip the untagged line off the front,
 * as it appears twice,
 * and A018 OK off the end. */
		t = strchr(mailstring, '\n');
		if (t) {
			++t;
			mailstring_l -= (t - mailstring);
			strmove(mailstring, t);
		}
		t = mailstring + mailstring_l;
		if (t > mailstring && t[-1] == '\n')
			t[-1] = 0, --mailstring_l;
		t = strrchr(mailstring, '\n');
		if (t && strstr(t, " OK ")) {
			++t;
			*t = 0;
			mailstring_l = t - mailstring;
		}

		mif->cbase = mailstring;
		mif->subject = emptyString;
		mif->from = emptyString;
		mif->to = emptyString;
		mif->reply = emptyString;
		mif->prec = emptyString;
		mif->ccrec = emptyString;

/*********************************************************************
Before we start cracking, here are a couple of illustrative examples.
You can get these, or another envelope that doesn't seem to parse
correctly, by db4. Note in the second example,
that the encoded subject is 105 bytes, which matches {105}
* 358 FETCH (FLAGS (\Seen $HasAttachment) INTERNALDATE "03-Dec-2022 12:57:23 +0000" RFC822.SIZE 121285 ENVELOPE ("Sat, 03 Dec 2022 12:57:12 +0000" "Your Daily Digest for Sat, Dec 03" ((NIL NIL "USPSInformeddelivery" "email.informeddelivery.usps.com")) ((NIL NIL "USPSInformeddelivery" "email.informeddelivery.usps.com")) ((NIL NIL "USPSInformeddelivery" "email.informeddelivery.usps.com")) ((NIL NIL "KWNRE" "comcast.net")) NIL NIL NIL "<20221203125712.0a463f28d45908a3@email.informeddelivery.usps.com>"))
* 359 FETCH (FLAGS ($HasNoAttachment) INTERNALDATE "03-Dec-2022 16:06:55 +0000" RFC822.SIZE 33094 ENVELOPE ("3 Dec 2022 16:01:33 +0000" {105}
=?Windows-1252?Q?Wendy=2C_very_important=2E_=28Note=3A_Failur?= =?Windows-1252?Q?e_to_act_in_time=85=29?= (("Publishers Clearing House" NIL "PublishersClearingHouse" "e.superprize.pch.com")) (("Publishers Clearing House" NIL "PublishersClearingHouse" "e.superprize.pch.com")) (("Publishers Clearing House" NIL "PublishersClearingHouse" "e.superprize.pch.com")) (("Wendy Dahlke" NIL "kwnre" "comcast.net")) NIL NIL NIL "<543.30570379008.202212031601292260087.0123746943@e.superprize.pch.com>"))
* 360 FETCH (FLAGS ($HasNoAttachment) INTERNALDATE "04-Dec-2022 06:27:36 +0000" RFC822.SIZE 7083 ENVELOPE ("Sun, 4 Dec 2022 06:27:35 +0000" "Discover The Power Of Comparison Shopping" (({21}
Plan Offers, Medicare NIL "deck" "uniqueencrypter.com")) (({21}
Plan Offers, Medicare NIL "deck" "uniqueencrypter.com")) (({21}
Plan Offers, Medicare NIL "deck" "uniqueencrypter.com")) ((NIL NIL "kwnre" "comcast.net")) NIL NIL NIL "<01000184dbd191f0-a0febe67-5ffe-4875-b066-8bf71e7bcedb-000000@email.amazonses.com>"))
* 361 FETCH (FLAGS ($HasNoAttachment) INTERNALDATE "04-Dec-2022 08:26:13 +0000" RFC822.SIZE 5968 ENVELOPE ("Sun, 4 Dec 2022 08:26:11 +0000" "=?UTF-8?B?4biMbyB5b3Ugd2FrZSB1cCDhua1pcmVkLCB3aXRoIGRyeSBza2luICYgIOG2gWFtYWdlZCBoYWlyPw==?=" (({20}
"=?UTF-8?Q?BLlSSY?=" NIL "WeCare4U" "cateringadvetiser.com")) (({20}
"=?UTF-8?Q?BLlSSY?=" NIL "WeCare4U" "cateringadvetiser.com")) (({20}
"=?UTF-8?Q?BLlSSY?=" NIL "WeCare4U" "cateringadvetiser.com")) ((NIL NIL "kwnre" "comcast.net")) NIL NIL NIL "<01010184dc3e266d-c67755c2-facc-4650-9375-f85825069dd6-000000@us-west-2.amazonses.com>"))
*********************************************************************/

		t = strstr(mailstring, "ENVELOPE (");
		if (!t) {
			nzFree(mailstring);
			continue;
		}

// pull out subject, reply, etc
// Don't free mailstring, we're using pieces of it
		t += 10;
		while (*t == ' ')
			++t;
// date first, and it must be quoted.
// We don't use this date because it isn't standardized,
// it's whatever the sender's email client puts on the Date field.
// We use INTERNALDATE later.
		if(!strncmp(t, "NIL", 3)) {
			t += 3;
		} else {
			if (*t != '"')
				continue;
			t = strchr(++t, '"');
			if (!t)
				continue;
			++t;
	}

// subject next, I'll assume it is always quoted
		while (*t == ' ')
			++t;

		if(!strncmp(t, "NIL ", 4)) { // missing subject
			t += 4;
			goto doreply;
		}

// imap sometimes has number in braces, don't know why
		sublength = -1;
		if(*t == '{') {
			 sublength = strtol(t+1, &t, 10);
			if(*t == '}') ++t;
		// with number in braces, subject is on next line.
// isspace takes us past crlf
			while(isspaceByte(*t)) ++t;
		}

//  printf("%d,%d,%d|%s|\n", mailstring_l, t-mailstring, strlen(t), t);

		if (*t == '"') {
			++t;
			u = nextRealQuote(t);
		} else {
			mif->line2 = true;
			u = t + sublength;
			if(sublength <= 0 || u - mailstring >= mailstring_l)
				u = 0;
		}

		if (!u)
			continue;
		*u = 0;
		if (*t == '[' && u[-1] == ']')
			++t, u[-1] = 0;
		mif->subject = t;
		t = u + 1;

doreply:
		while (*t == ' ')
			++t;
		if (t[0] != '(' || t[1] != '(')
			goto doref;
		t += 2;
		if (!grabEmailFromEnvelope(&t, &mif->from, &mif->reply))
			goto doref;

// We have parsed from-reply in block 1, block 4 contains the recipients,
// Don't know what is in blocks 2 and 3.
// shouldn't we be checking for (( or NIL here?
		u = strstr(t, "(("); // block 2
		if(!u)
			goto doref;
		t = u + 2;
		u = strstr(t, "(("); // block 3
		if(!u)
			goto doref;
		t = u + 2;
		u = strstr(t, "(("); // block 4
		if(!u)
			goto doref;
		t = u + 2;
		if (!grabEmailFromEnvelope(&t, &mif->to, &mif->prec))
			goto doref;

// block 5 is the carbon copies, I guess, I don't know.
// It doesn't have to be there.
// Block 6 is bcc.
#if 0
		u = strstr(t, ")) (("); // block 5
		u = strstr(t, ")) NIL (("); // block 6
		if(!u)
			goto doref;
		t = u + 5;
		if (!grabEmailFromEnvelope(&t, &f->ccrec, 0))
			goto doref;
#endif

//  printf("%s %s %s %s\n", mif->from, mif->reply, mif->to, mif->prec);

doref:
/* find the reference string, for replies */
		u = strstr(t, " \"<");
		if (!u)
			goto doflags;
		t = u + 2;
		u = strchr(t, '"');
		if (!u)
			goto doflags;
		*u = 0;
		mif->refer = t;	// not used
		t = u + 1;

doflags:
/* flags, mostly looking for has this been read */
		u = strstr(t, "FLAGS (");
		if(!u) {
// sometimes flags and stuff comes at the beginning.
// It should be somewhere!
			t = mailstring;
			u = strstr(t, "FLAGS (");
		}
		if (!u)
			goto dodate;
		t = u + 7;
		if (strstr(t, "\\Seen"))
			mif->seen = true;
		else
			++f->unread;

dodate:
		u = strstr(t, "INTERNALDATE ");
		if (!u)
			goto dosize;
		t = u + 13;
		while (*t == ' ')
			++t;
		if (*t != '"')
			goto dosize;
		++t;
		u = strchr(t, '"');
		if (!u)
			goto dosize;
		*u = 0;
		mif->sent = parseHeaderDate(t);
		t = u + 1;

dosize:
		u = strstr(t, "SIZE ");
		if (!u)
			continue;
		t = u + 5;
		if (!isdigitByte(*t))
			continue;
		mif->size = atoi(t);

	}
}

/* examine the specified folder, gather message envelopes */
static void examineFolder(CURL * handle, struct FOLDER *f, bool dostats)
{
	int j;
	char *t;
	CURLcode res;

	cleanFolder(f);

/* interrogate folder */
	if (asprintf(&t, "EXAMINE \"%s\"", f->path) == -1)
		i_printfExit(MSG_MemAllocError, strlen(f->path) + 12);
	curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, t);
	free(t);
	res = getMailData(handle);
	if (res != CURLE_OK)
		ebcurl_setError(res, mailbox_url, 2, emptyString);

/* look for message count */
	t = strstr(mailstring, " EXISTS");
	if (t) {
		while (*t == ' ')
			--t;
		if (isdigitByte(*t)) {
			while (isdigitByte(*t))
				--t;
			++t;
			f->nmsgs = atoi(t);
		}
	}
	f->nfetch = f->nmsgs;
	if (f->nfetch > imapfetch)
		f->nfetch = imapfetch;
	f->start = 1;
	if (f->nmsgs > f->nfetch && !earliest)
		f->start += (f->nmsgs - f->nfetch);

	t = strstr(mailstring, "UIDNEXT ");
	if (t) {
		t += 8;
		while (*t == ' ')
			++t;
		if (isdigitByte(*t))
			f->uidnext = atoi(t);
	}

	nzFree(mailstring);
	if (dostats) {
		printf("%2lld %s", (unsigned long long)(f - topfolders + 1),
		       f->path);
/*
		if (f->children)
			printf(" with children");
*/
		printf(", ");
		i_printf(MSG_MessagesX, f->nmsgs);
		return;
	}

	if (!f->nmsgs)
		return;

/* get some information on each message */
	f->mlist = allocZeroMem(sizeof(struct MIF) * f->nfetch);
	for (j = 0; j < f->nfetch; ++j) {
		struct MIF *mif = f->mlist + j;
		mif->seqno = f->start + j;
	}

	envelopes(handle, f);

	if (f->nmsgs > f->nfetch)
		i_printf(MSG_ShowLast + earliest, f->nfetch, f->nmsgs);
	else
		i_printf(MSG_MessagesX, f->nmsgs);
}

/* find the last mail in the local unread directory */
static int unreadMax, unreadMin, unreadCount;
static int unreadBase;		/* find min larger than base */

static void unreadStats(void)
{
	const char *f;
	int n;

	unreadMax = 0;
	unreadMin = 0;
	unreadCount = 0;

	while ((f = nextScanFile(mailUnread))) {
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
}

static char *umf;		/* unread mail file */
static char *umf_end;
static int umfd;		/* file descriptor for the above */
/* convert mail message to/from utf8 if need be. */
/* This isn't really right, cause it should be done per mime component. */
static char *mailu8;
static int mailu8_l;

static CURL *newFetchmailHandle(const char *username, const char *password)
{
	CURLcode res;
	CURL *handle = curl_easy_init();
	if (!handle)
		i_printfExit(MSG_LibcurlNoInit);

	curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, mailTimeout);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, eb_curl_callback);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, &callback_data);
	curl_easy_setopt(handle, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(handle, CURLOPT_DEBUGFUNCTION, ebcurl_debug_handler);
	curl_easy_setopt(handle, CURLOPT_DEBUGDATA, &callback_data);

	res = curl_easy_setopt(handle, CURLOPT_USERNAME, username);
	if (res != CURLE_OK)
		ebcurl_setError(res, mailbox_url, 1, emptyString);

	res = curl_easy_setopt(handle, CURLOPT_PASSWORD, password);
	if (res != CURLE_OK)
		ebcurl_setError(res, mailbox_url, 2, emptyString);

	return handle;
}

static void get_mailbox_url(const struct MACCOUNT *account)
{
	const char *scheme = "pop3";
	char *url = NULL;

	if (account->inssl)
		scheme = "pop3s";

	if (isimap) {
		scheme = (account->inssl ? "imaps" : "imap");
		loadAddressBook();
	}

	if (asprintf(&url,
		     "%s://%s:%d/", scheme, account->inurl,
		     account->inport) == -1) {
/* The byte count is a little white lie / guess, we don't know
 * how much asprintf *really* requested. */
		i_printfExit(MSG_MemAllocError,
			     strlen(scheme) + strlen(account->inurl) + 8);
	}
	mailbox_url = url;
}

/* reads message into mailstring, it's up to you to free it */
static CURLcode fetchOneMessage(CURL * handle, int message_number)
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

	res = getMailData(handle);
	undosOneMessage();
	if (res != CURLE_OK)
		return res;

/* got the file, save it in unread */
	sprintf(umf_end, "%d", unreadMax + message_number);
	umfd = open(umf, O_WRONLY | O_TEXT | O_CREAT, MODE_rw);
	if (umfd < 0)
		i_printfExit(MSG_NoCreate, umf);
	if (write(umfd, mailstring, mailstring_l) < mailstring_l)
		i_printfExit(MSG_NoWrite, umf);
	close(umfd);

	return res;
}

static CURLcode deleteOneMessage(CURL * handle)
{
	CURLcode res = curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "DELE");
	if (res != CURLE_OK)
		return res;
	res = curl_easy_setopt(handle, CURLOPT_NOBODY, 1L);
	if (res != CURLE_OK)
		return res;
	res = getMailData(handle);
	return res;
}

static CURLcode count_messages(CURL * handle, int *message_count)
{
	CURLcode res = setCurlURL(handle, mailbox_url);
	int i, num_messages = 0;
	bool last_nl = true;

	if (res != CURLE_OK)
		return res;

	res = getMailData(handle);
	if (res != CURLE_OK)
		return res;

	if (isimap) {
		struct FOLDER *f;
		char inputline[80];
		char *t;

		setFolders(handle);
		if (!n_folders) {
			i_puts(MSG_NoFolders);
imap_done:
			curl_easy_cleanup(handle);
			exit(0);
		}

		i_puts(MSG_SelectFolder);
input:
		if (!fgets(inputline, sizeof(inputline), stdin))
			goto imap_done;
		stripWhite(inputline);

		if (stringEqual(inputline, "rf")) {
refresh:
			curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, 0);
			res = getMailData(handle);
			if (res != CURLE_OK)
				goto imap_done;
			setFolders(handle);
			goto input;
		}

		if (stringEqual(inputline, "q")
		|| stringEqual(inputline, "qt")) {
			i_puts(MSG_Quit);
			goto imap_done;
		}

		t = inputline;
		if (t[0] == 'd' && t[1] == 'b' && isdigitByte(t[2]) && !t[3]) {
			debugLevel = t[2] - '0';
			curl_easy_setopt(handle, CURLOPT_VERBOSE,
					 (debugLevel >= 4));
			goto input;
		}

		if (*t == 'l' && isspaceByte(t[1])) {
			setLimit(t + 1);
			goto input;
		}

		if (*t == 'e') {
			if(!t[1]) {
				setEnvelopeFormat("");
				goto input;
			}
			if(isspaceByte(t[1]) && setEnvelopeFormat(t + 2))
				goto input;
		}

		if (!strncmp(t, "create ", 7)) {
			char *w;
			t += 7;
			stripWhite(t);
			if (!*t)
				goto input;
			if (asprintf(&w, "CREATE \"%s\"", t) < 0)
				goto imap_done;
			curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, w);
			res = getMailData(handle);
			free(w);
			nzFree(mailstring);
			if (res != CURLE_OK) {
				i_printf(MSG_NoCreate2, t);
				nl();
				goto input;
			}
			goto refresh;
		}

		if (!strncmp(t, "delete ", 7)) {
			char *w;
			t += 7;
			stripWhite(t);
			if (!*t)
				goto input;
			if (asprintf(&w, "DELETE \"%s\"", t) < 0)
				goto imap_done;
			curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, w);
			res = getMailData(handle);
			free(w);
			nzFree(mailstring);
			if (res != CURLE_OK) {
				i_printf(MSG_NoAccess, t);
				nl();
				goto input;
			}
			goto refresh;
		}

		if (!strncmp(t, "rename ", 7)) {
			char *u, *w;
			t += 7;
			stripWhite(t);
			if (!*t)
				goto input;
			u = strchr(t, ' ');
			if (!u)
				goto input;
			*u++ = 0;
			stripWhite(u);
			if (!*u)
				goto input;
			if (asprintf(&w, "RENAME \"%s\" \"%s\"", t, u) < 0)
				goto imap_done;
			curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, w);
			res = getMailData(handle);
			free(w);
			nzFree(mailstring);
			if (res != CURLE_OK) {
				i_printf(MSG_NoAccess, t);
				nl();
				goto input;
			}
			goto refresh;
		}

		f = folderByName(t);
		if (!f)
			goto input;
		examineFolder(handle, f, false);
		scanFolder(handle, f);
		goto input;
	}

	for (i = 0; i < mailstring_l; i++) {
		if (mailstring[i] == '\n' || mailstring[i] == '\r') {
			last_nl = true;
			continue;
		}
		if (last_nl && isdigitByte(mailstring[i]))
			num_messages++;
		last_nl = false;
	}

	*message_count = num_messages;
	return CURLE_OK;
}

/* Returns number of messages fetched */
int fetchMail(int account)
{
	CURL *mail_handle;
	const struct MACCOUNT *a = accounts + account - 1;
	const char *login = a->login;
	const char *pass = a->password;
	int nfetch = 0;		/* number of messages actually fetched */
	CURLcode res = CURLE_OK;
	const char *url_for_error;
	int message_count = 0, message_number;

// remember the envelope format we got from the config file
	strcpy(envelopeFormatDef, envelopeFormat);

	get_mailbox_url(a);
	url_for_error = mailbox_url;

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

	mail_handle = newFetchmailHandle(login, pass);
	res = count_messages(mail_handle, &message_count);
	if (res != CURLE_OK)
		goto fetchmail_cleanup;

	for (message_number = 1; message_number <= message_count;
	     message_number++) {
		if (asprintf(&message_url, "%s%u", mailbox_url, message_number)
		    == -1) {
/* Again, the byte count in the error message is a bit of a fib. */
			i_printfExit(MSG_MemAllocError,
				     strlen(mailbox_url) + 11);
		}
		res = fetchOneMessage(mail_handle, message_number);
		if (res != CURLE_OK)
			goto fetchmail_cleanup;
		nfetch++;
		res = deleteOneMessage(mail_handle);
		if (res != CURLE_OK)
			goto fetchmail_cleanup;
		nzFree(message_url);
		message_url = 0;
	}

fetchmail_cleanup:
	if (message_url)
		url_for_error = message_url;
	if (res != CURLE_OK)
		ebcurl_setError(res, url_for_error, 1, emptyString);
	curl_easy_cleanup(mail_handle);
	nzFree(message_url);
	nzFree(mailbox_url);
	message_url = mailbox_url = 0;
	nzFree(mailstring);
	mailstring = initString(&mailstring_l);
	return nfetch;
}

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
}

static void readReplyInfo(void);
static void writeReplyInfo(const char *addstring);

void scanMail(void)
{
	int nmsgs, m;
	char key;

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

	preferPlain = false;
	for (m = 1; m <= nmsgs; ++m) {
		nzFree(lastMailText);
		lastMailText = 0;
/* Now grab the entire message */
		unreadStats();
		sprintf(umf_end, "%d", unreadMin);
		if (!fileIntoMemory(umf, &mailstring, &mailstring_l, 0))
			showErrorAbort();
		unreadBase = unreadMin;

		key = presentMail();
		if(key == 'g') { --m, --unreadBase; continue; }
		if(key == 't') { preferPlain ^= 1; --m, --unreadBase; continue; }
		if (key == 'd')
			unlink(umf);
		preferPlain = false;
	}			// loop over mail messages

	exit(0);
}

/* a mail message is in mailstring, present it to the user */
/* Return the key that was pressed.
 * stop is only meaningful for imap. */
static char presentMail(void)
{
	int j, k;
	const char *redirect = NULL;	/* send mail elsewhere */
	char key = 0;
	const char *atname = NULL;	/* name of file or attachment */
	bool delflag = false;	/* delete this mail */
	bool scanat = false;	/* scan for attachments */
	int displine;
	int stashNumber = -1;
	char exists;
	int fsize;		/* file size */
	int fh;

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
			if (stringEqual(redirect, "x")) {
				i_printf(MSG_Junk);
				printf("[%s]\n", lastMailInfo->subject);
			} else
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
	if (!delflag && !postkey) {		/* show next page */
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

	if(postkey) {
		key = postkey, postkey = 0;
	} else {
/* interactive prompt depends on whether there is more text or not */
		printf("%c ", displine > cw->dol ? '?' : '*');
		fflush(stdout);
		key = getLetter((isimap ? "qvbfh? gtnpwWuUasdm" : "qh? gtnwud"));
		printf("\b\b\b");
		fflush(stdout);
	}

	switch (key) {
	case 'q':
		i_puts(MSG_Quit);
		exit(0);

	case 'n':
		i_puts(MSG_Next);
		goto afterinput;

	case 'g':
		i_puts(MSG_Restart);
		goto afterinput;

	case 't':
		i_puts(MSG_Restart);
		preferPlain ^= 1;
		key = 'g';
		goto afterinput;

	case 'p':
		i_puts(MSG_Previous);
		goto afterinput;

	case 's':
	case 'm':
	case 'v':
	case 'f':
	case 'b':
		goto afterinput;

	case 'd':
		if (!isimap)
			i_puts(MSG_Delete);
		delflag = true;
		goto afterinput;

	case ' ':
		if (displine > cw->dol)
			i_puts(MSG_EndMessage);
		goto paging;

	case '?':
	case 'h':
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
	}			// switch

// At this point we're saving the mail somewhere.
writeMail:
	if (!isimap || isupperByte(key))
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

	exists = fileTypeByName(atname, 0);
	fh = open(atname, O_WRONLY | O_TEXT | O_CREAT | O_APPEND, MODE_rw);
	if (fh < 0) {
		i_printf(MSG_NoCreate, atname);
		goto saveMail;
	}
	if (exists)
		write(fh,
		      "======================================================================\n",
		      71);
	if (key == 'u' || key == 'U') {
		if (write(fh, mailstring, mailstring_l) < mailstring_l) {
badsave:
			i_printf(MSG_NoWrite, atname);
			close(fh);
			goto saveMail;
		}
		close(fh);
		fsize = mailstring_l;
	} else {

/* key = w, write the file and save the original unformatted */
		if (mailStash) {
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
				if (fileTypeByName(rmf, 0))
					continue;
/* dump the original mail into the file */
				rmfh =
				    open(rmf,
					 O_WRONLY | O_TEXT | O_CREAT | O_APPEND,
					 MODE_rw);
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
		return 'd';
	return strchr("smvbfpguwa", key) ? key : 'n';
}

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
		if (first == '\n' || (first == '\r' && p[1] == '\n'))
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
}

void mail64Error(int err)
{
	switch (err) {
	case BAD_BASE64_DECODE:
		runningError(MSG_AttBad64);
		break;
	case EXTRA_CHARS_BASE64_DECODE:
		runningError(MSG_AttAfterChars);
		break;
	}			/* switch on error code */
}

static void unpackQP(struct MHINFO *w)
{
	char c, d, *q, *r;
	for (q = r = w->start; q < w->end; ++q) {
		c = *q;
		if (c == '_') { *r++ = ' '; continue; }
		if (c != '=') { *r++ = c; continue; }
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
}

/* Look for the name of the attachment and boundary */
static void ctExtras(struct MHINFO *w, const char *s, const char *t)
{
	char quote;
	const char *q, *al, *ar;

	if (w->ct < CT_MULTI) {
// look for name= or filename=
		quote = 0;
		for (q = s + 1; q < t; ++q) {
			if (isalnumByte(q[-1])) continue;
// could be name= or filename=
			if (memEqualCI(q, "file", 4)) q += 4;
			if (!memEqualCI(q, "name", 4)) continue;
			q += 4;
			while(*q == ' ') ++q;
			if(*q != '=') continue;
			++q;
			while(*q == ' ') ++q;
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
// filename coulde be encoded
			char *e = w->cfn + (ar - al);
			isoDecode(w->cfn, &e);
			*e = 0;
// for security reasons, don't allow an absolute path, or even relative path
// away from the current directory or the download directory.
// tr / _    this seems to be what other clients do.
			for(e = w->cfn; *e; ++e)
				if(*e == '/')
					*e = '_';
			break;
		}
	}

	if (w->ct >= CT_MULTI) {
// look for boundary=
		quote = 0;
		for (q = s + 1; q < t; ++q) {
			if (isalnumByte(q[-1])) continue;
			if (!memEqualCI(q, "boundary", 8)) continue;
			q += 8;
			while(*q == ' ') ++q;
			if(*q != '=') continue;
			++q;
			while(*q == ' ') ++q;
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
	}			// multi or alt
}

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
			if (c == '_') c = ' ';
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
// remove spaces between blocks
	for(t = start; t < vr; ++t) {
		if(!isspaceByte(*t)) break;
	}
	if(t > start && (t == vr || (t[0] == '=' && t[1] == '?'))) {
		len = vr - t;
		memmove(start, t, len);
		vr = start + len;
	}
	goto restart;

finish:
	for (s = vl; s < vr; ++s) {
		c = *s;
		if (c == 0 || c == '\t')
			*s = ' ';
	}

	*vrp = vr;
}

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
}

static void extractLessGreater(char *s)
{
	char *vl, *vr;
	vl = strchr(s, '<');
	vr = strchr(s, '>');
	if (vl && vr && vl < vr) {
		*vr = 0;
		strmove(s, vl + 1);
	}
}

/* Now that we know it's mail, see what information we can
 * glean from the headers.
 * Returns a pointer to an allocated MHINFO structure.
 * This routine is recursive. */
static struct MHINFO *headerGlean(char *start, char *end, bool top)
{
	char *s, *t, *q;
	char *vl, *vr;		/* value left and value right */
	struct MHINFO *w;
	int j, k, n;

/* defaults */
	w = allocZeroMem(sizeof(struct MHINFO));
	initList(&w->components);
	w->ct = CT_OTHER;
// email sometimes has no content-type and no components - treat it as text.
	if(top) w->ct = CT_TEXT;
	w->ce = CE_8BIT;
	w->andOthers = false;
	w->tolist = initString(&w->tolen);
	w->cclist = initString(&w->cclen);
	w->start = start, w->end = end;

// join lines together
	for(s = t = start; s < end; ++s) {
		if(*s == '\n') {
			if(s == end - 1 || s[1] == '\n')
				break;
			if(s[1] == ' ' || s[1] == '\t')
				continue; // join lines together
			}
		*t++ = *s;
	}

// I don't know what to do with the gap.
	while(t < s)
		*t++ = '\n';

// step through the header lines
	for (s = start; s < end; s = t + 1) {
		char quote;
		t = strchr(s, '\n');
		if (!t)
			t = end - 1;	/* should never happen */
		if (t == s)
			break;	// empty line, end of headers

// find the lead word
		for (q = s; isalnumByte(*q) || *q == '_' || *q == '-'; ++q) ;
		if (q == s)
			continue;	// should never happen
		if (*q++ != ':')
			continue;	// should never happen
		for (vl = q; *vl == ' ' || *vl == '\t'; ++vl) ;
		for (vr = t; vr > vl && (vr[-1] == ' ' || vr[-1] == '\t');
		     --vr) ;
		if (vr == vl)
			continue;	// empty

// is it too long?
// Should print out an error or something.
		if (vr - vl > MHLINE - 1)
			vr = vl + MHLINE - 1;

/* This is sort of a switch statement on the word */
		if (memEqualCI(s, "subject:", q - s)) {
			if (w->subject[0])
				continue;
// get rid of forward/reply prefixes
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
			strncpy(w->subject, vl, vr - vl);
			mhReformat((vl = w->subject));
			vr = vl + strlen(vl);
			isoDecode(vl, &vr);
			*vr = 0;
			continue;
		}

		if (memEqualCI(s, "reply-to:", q - s)) {
			if (!w->reply[0])
				strncpy(w->reply, vl, vr - vl);
			continue;
		}

		if (memEqualCI(s, "message-id:", q - s)) {
			if (!w->mid[0])
				strncpy(w->mid, vl, vr - vl);
			continue;
		}

		if (memEqualCI(s, "references:", q - s)) {
			if (!w->ref[0])
				strncpy(w->ref, vl, vr - vl);
			continue;
		}

		if (memEqualCI(s, "from:", q - s)) {
			if (w->from[0])
				continue;
			isoDecode(vl, &vr);
			strncpy(w->from, vl, vr - vl);
			mhReformat(w->from);
			continue;
		}

		if (memEqualCI(s, "date:", q - s)
		    || memEqualCI(s, "sent:", q - s)) {
			if (w->date[0])
				continue;
// don't need the weekday, seconds, or timezone
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
			if (w->tolen)
				stringAndChar(&w->tolist, &w->tolen, ',');
			stringAndBytes(&w->tolist, &w->tolen, q, vr - q);
			if (w->to[0])
				continue;
			strncpy(w->to, vl, vr - vl);
// Only retain the first recipient
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
			if (w->cclen)
				stringAndChar(&w->cclist, &w->cclen, ',');
			stringAndBytes(&w->cclist, &w->cclen, q, vr - q);
			w->andOthers = true;
			continue;
		}

		if (memEqualCI(s, "content-type:", q - s)) {
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
			if (memEqualCI(vl, "multipart/related", 17))
				w->ct = CT_ALT;

			ctExtras(w, s, t);
			continue;
		}

		if (memEqualCI(s, "content-disposition:", q - s)) {
			if (memEqualCI(vl, "attachment", 10)) {
				w->dispat = true;
				ctExtras(w, s, t);
			}
			continue;
		}

		if (memEqualCI(s, "content-transfer-encoding:", q - s)) {
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

	}			// loop over lines

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
		printf("length %d\n", (int)(w->end - w->start));
		printf("content %d/%d\n", w->ct, w->ce);
	}

// look for line of equals separating successive emails in the same file.
// Yeah, a false positive is possible here.
// Fortunately, most emails, even those generated by edbrowse, are multipart,
// and won't fall into this code.
	if(top && w->ct < CT_MULTI) {
		t = strstr(w->start, "\n======================================================================\n");
		if(t && t < w->end) w->end = t + 1;
	}


	if (w->ce == CE_QP)
		unpackQP(w);
	if (w->ce == CE_64) {
		w->error64 = base64Decode(w->start, &w->end);
		if (w->error64 != GOOD_BASE64_DECODE)
			mail64Error(w->error64);
	}
	if(w->ct < CT_MULTI &&
	(w->dispat || w->ct == CT_OTHER || w->ct == CT_APPLIC)) {
		w->doAttach = true, ++nattach;
		if(*(q = w->cfn)) { // name present
			if (stringEqual(q, "winmail.dat")) {
				w->atimage = true, ++nimages;
			} else if ((q = strrchr(q, '.'))) {
				static const char *const imagelist[] = {
					"gif", "jpg", "tif", "bmp", "asc",
					"png", 0
				};
// the asc isn't an image, it's a signature card.
// Similarly for the winmail.dat
				if (stringInListCI(imagelist, q + 1) >= 0)
					w->atimage = true, ++nimages;
			}
			if (!w->atimage && nattach == nimages + 1)
				firstAttach = w->cfn;
		}
		return w;
	}

/* loop over the mime components */
	if (w->ct >= CT_MULTI) {
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
				child = headerGlean(lastbound, t, false);
				addToListBack(&w->components, child);
			}
			s = lastbound = q;
		}
		w->start = w->end = 0;
		return w;
	}

#if 0
	// Scan through, we might have a mail message included inline
// Trying to pull mime components out of my ass? What was I thinking?
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
				struct MHINFO *child = headerGlean(vl, end, false);
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
		struct MHINFO *child = headerGlean(vl, end, false);
		addToListBack(&w->components, child);
		w->end = end = vl;
	}
#endif

textonly:
/* Any additional processing of the text, from start to end, can go here. */
/* Remove leading blank lines or lines with useless words */
	for (s = start; s < end; s = t + 1) {
		t = strchr(s, '\n');
		if (!t || t > end)
			t = end;
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
		break;		// something real
	}
// s could be end + 1
	if(s > w->end) s = w->end;
	w->start = start = s;

	return w;
}

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
		if (w->reply[0]) {
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

// This is at the top of the file
		if (w->subject[0]) {
// the <pre nowspc keeps the subject on one line
			sprintf(buf, "%sSubject: %s%s\n",
			(mailIsHtml ? "<pre nowspc>" : ""),
			w->subject,
			(mailIsHtml ? "</pre>" : ""));
			lines = true;
		}
		if (nattach && ismc) {
			char atbuf[20];
			if (lines & mailIsHtml)
				strcat(buf, "<br>");
			lines = true;
			if (nimages) {
				sprintf(atbuf, "%d images", nimages);
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
				sprintf(atbuf, "%d attachments",
					nattach - nimages);
				strcat(buf, atbuf);
			}
			strcat(buf, "\n");
		}
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
// it's very rare for a from line to break; you have to have a small fll value.
			if(mailIsHtml) strcat(buf, "<pre nowspc>");
			strcat(buf, "From ");
			strcat(buf, w->from);
			if(mailIsHtml) strcat(buf, "</pre>");
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
}

/* Depth first block of text determines the type */
static int mailTextType(struct MHINFO *w)
{
	struct MHINFO *v;
	int texttype = CT_OTHER, rc;

	if (w->doAttach)
		return CT_OTHER;

// jump right into the hard part, multi/alt
	if (w->ct >= CT_MULTI) {
		foreach(v, w->components) {
			rc = mailTextType(v);
			if(!rc) continue;
			if(!texttype) { texttype = rc; continue; }
			if(rc == texttype) continue;
			if(preferPlain ^ (rc == CT_HTML)) texttype = rc;
		}
		return texttype;
	}

	// If there is no text, return nothing
	if (w->start == w->end)
		return CT_OTHER;
// I don't know if this is right, but I override the type,
// and make it html if we start out with <html>
	if (memEqualCI(w->start, "<html>", 6))
		return CT_HTML;
	return w->ct;
}

static void formatMail(struct MHINFO *w, bool top)
{
	struct MHINFO *v;
	int ct = w->ct;
	int j, best;

// This use to be a test for attachment, but I got text messages
// from a friend where the text was in an attachment.
// So now I fold it into the display if it is plain text not encoded.
	if (w->doAttach) {
		if(w->ct != CT_TEXT || w->ce > CE_8BIT)
			return;
		stringAndString(&fm, &fm_l, mailIsHtml ? "Attachment:<br>\n" : "Attachment:\n");
	}

	debugPrint(5, "format mail content %d subject %s", ct,    w->subject);
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
					start = breakLineResult;
					end = start + newlen;
				}
			}
			if (mailIsHtml && ct != CT_HTML)
				stringAndString(&fm, &fm_l, "<pre>");
			stringAndBytes(&fm, &fm_l, start, end - start);
			if (mailIsHtml && ct != CT_HTML)
				stringAndString(&fm, &fm_l, "</pre>\n");
		} // text present

		// There could be a mail message inline
		foreach(v, w->components) {
			if (end > start)
				stringAndString(&fm, &fm_l,
						mailIsHtml ? "<P>\n" : "\n");
			formatMail(v, false);
		}

		return;
	}

// alternate presentations, pick the best one.
// Even if it is generically multipart, I go down this path,
// cause I don't know what to do.
// I can't present plain components if we are in html mode,
// nor html components in plain text mode.
// This algorithm isn't right, at many levels, but seems to work for
// virtually every email out there.
	best = j = 0;
	foreach(v, w->components) {
		int subtype = mailTextType(v);
		++j;
		if (subtype != CT_OTHER)
			best = j;
		if ((mailIsHtml && subtype == CT_HTML) ||
		    (!mailIsHtml && subtype == CT_TEXT))
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
}

/* Browse the email file. */
char *emailParse(char *buf)
{
	struct MHINFO *w;
	nattach = nimages = 0;
	firstAttach = 0;
	mailIsHtml = ignoreImages = false;
	fm = initString(&fm_l);
	w = headerGlean(buf, buf + strlen(buf), true);
	mailIsHtml = (mailTextType(w) == CT_HTML);
	if(mailIsHtml & preferPlain) i_puts(MSG_NoPlain);
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
}

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

	if (cw->irciMode | cw->ircoMode) {
		setError(MSG_ReIrc);
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
		char *p;
		if (j > cw->dol)
			break;

		p = (char *)fetchLine(j, 1);

		if (memEqualCI(p, "subject:", 8) || memEqualCI(p, "sub:", 4)) {
			linetype[j] = 's';
			subln = j;
			goto nextline;
		}

		if (memEqualCI(p, "to ", 3)) {
			linetype[j] = 't';
			goto nextline;
		}

		if (memEqualCI(p, "from ", 5)) {
			linetype[j] = 'f';
			goto nextline;
		}

		if (memEqualCI(p, "mail sent ", 10)) {
			linetype[j] = 'w';
			goto nextline;
		}

		if (memEqualCI(p, "references:", 11)) {
			linetype[j] = 'v';
			goto nextline;
		}

		if (memEqualCI(p, "reply to ", 9)) {
			linetype[j] = 'r';
			repln = j;
			goto nextline;
		}

/* This one has to be last. */
		s = p;
		while (isdigitByte(*s))
			++s;
		if (memEqualCI(s, " attachment", 11)
		    || memEqualCI(s, " image", 6)) {
			linetype[j] = 'a';
			goto nextline;
		}

/* line doesn't match anything we know */
		nzFree(p);
		break;

nextline:
		nzFree(p);
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
}

static void writeReplyInfo(const char *addstring)
{
	int rfh;		/* reply file handle */
	rfh = open(mailReply, O_WRONLY | O_APPEND | O_CREAT, MODE_private);
	if (rfh < 0)
		return;
	write(rfh, addstring, 12);
	write(rfh, cw->mailInfo, strlen(cw->mailInfo));
	write(rfh, "\n", 1);
	close(rfh);
}

static void readReplyInfo(void)
{
	int rfh;		/* reply file handle */
	const char *p;
	int ln, major, minor;
	char prestring[20];
	char *buf;
	int buflen;
	char *s, *t;

	if (cw->mailInfo)
		return;		/* already there */

/* scan through the buffer looking for the Unformatted line,
 * but stop if you hit an email divider. */
	for (ln = 1; ln <= cw->dol; ++ln) {
		p = (char *)fetchLine(ln, -1);
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
	if (!fdIntoMemory(rfh, &buf, &buflen, 0))
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
}
