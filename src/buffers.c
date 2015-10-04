/* buffers.c
 * Text buffer support routines, manage text and edit sessions.
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"

#ifndef _MSC_VER		// no #include <sys/select.h>
#include <sys/select.h>
#endif // _MSC_VER

/* If this include file is missing, you need the pcre package,
 * and the pcre-devel package. */
#include <pcre.h>

#include <readline/readline.h>
#include <readline/history.h>

int displayLength = 500;

/* Static variables for this file. */

static uchar dirWrite;		/* directories read write */
static char lsformat[12];	/* size date etc on a directory listing */
static uchar endMarks;		/* ^ $ on printed lines */
static bool jexmode;
/* The valid edbrowse commands. */
static const char valid_cmd[] = "aAbBcdDefghHijJklmMnpqrstuvwXz=^<";
/* Commands that can be done in browse mode. */
static const char browse_cmd[] = "AbBdDefghHiklMnpqsvwXz=^<";
/* Commands for sql mode. */
static const char sql_cmd[] = "AadDefghHiklmnpqrsvwXz=^<";
/* Commands for directory mode. */
static const char dir_cmd[] = "AdDefghHklmnpqsvwXz=^<";
/* Commands that work at line number 0, in an empty file. */
static const char zero_cmd[] = "aAbefhHMqruwz=^<";
/* Commands that expect a space afterward. */
static const char spaceplus_cmd[] = "befrw";
/* Commands that should have no text after them. */
static const char nofollow_cmd[] = "aAcdDhHjlmnptuX=";
/* Commands that can be done after a g// global directive. */
static const char global_cmd[] = "dDijJlmnpstX";

static int startRange, endRange;	/* as in 57,89p */
static int destLine;		/* as in 57,89m226 */
static int last_z = 1;
static char cmd, icmd, scmd;
static uchar subPrint;		/* print lines after substitutions */
static bool noStack;		/* don't stack up edit sessions */
static bool globSub;		/* in the midst of a g// command */
static bool inscript;		/* run from inside an edbrowse function */
static int lastq, lastqq;
static char icmd;		/* input command, usually the same as cmd */

/*********************************************************************
 * If a rendered line contains a hyperlink, the link is indicated
 * by a code that is stored inline.
 * If the hyperlink is number 17 on the list of hyperlinks for this window,
 * it is indicated by InternalCodeChar 17 { text }.
 * The "text" is what you see on the page, what you click on.
 * {Click here for more information}.
 * And the braces tell you it's a hyperlink.
 * That's just my convention.
 * The prior chars are for internal use only.
 * I'm assuming these chars don't/won't appear on the rendered page.
 * Yeah, sometimes nonascii chars appear, especially if the page is in
 * a European language, but I just assume a rendered page will not contain
 * the sequence: InternalCodeChar number {
 * In fact I assume the rendered text won't contain InternalCodeChar at all.
 * So I use this char to demark encoded constructs within the lines.
 * And why do I encode things right in the text?
 * Well it took me a few versions to reach this point.
 * But it makes so much sense!
 * If I move a line, the referenced hyperlink moves with it.
 * I don't have to update some other structure that says,
 * "At line 73, characters 29 through 47, that's hyperlink 17.
 * I use to do it that way, and wow, what a lot of overhead
 * when you move lines about, or delete them, or make substitutions.
 * Yes, you really need to change rendered html text,
 * because that's how you fill out forms.
 * Add just one letter to the first name in your fill out form,
 * and the hyperlink that comes later on in the line shifts down.
 * I use to go back and update the pointers,
 * so that the hyperlink started at offset 30, rather than 29.
 * That was a lot of work, and very error prone.
 * Finally I got smart, and coded the html tags inline.
 * They can't get lost as text is modified.  They move with the text.
 *
 * So now, certain sequences in the text are for internal use only.
 * This routine strips out these sequences, for display.
 * After all, you don't want to see those code characters.
 * You just want to see {Click here for more information}. */

static void removeHiddenNumbers(pst p, uchar terminate)
{
	pst s, t, u;
	uchar c, d;

	s = t = p;
	while ((c = *s) != terminate) {
		if (c != InternalCodeChar) {
addchar:
			*t++ = c;
			++s;
			continue;
		}
		u = s + 1;
		d = *u;
		if (!isdigitByte(d))
			goto addchar;
		do {
			d = *++u;
		} while (isdigitByte(d));
		if (d == '*') {
			s = u + 1;
			continue;
		}
		if (strchr("<>{}", d)) {
			s = u;
			continue;
		}
/* This is not a code sequence I recognize. */
/* This should never happen; just move along. */
		goto addchar;
	}			/* loop over p */
	*t = c;			/* terminating character */
}				/* removeHiddenNumbers */

/* Fetch line n from the current buffer, or perhaps another buffer.
 * This returns an allocated copy of the string,
 * and you need to free it when you're done.
 * Good grief, how inefficient!
 * I know, but perl use to do it, and I never noticed the slowdown.
 * This is not the Linux kernel, we don't need to be ultra-efficient.
 * We just need to be consistent in our programming efforts.
 * Sometimes the returned line is changed,
 * and if it happens sometimes, we may as well make the new copy
 * all the time, even if the line doesn't change, to be consistent.
 * You can supress the copy feature with -1. */

static pst fetchLineContext(int n, int show, int cx)
{
	struct ebWindow *lw = sessionList[cx].lw;
	struct lineMap *map, *t;
	int dol, idx;
	unsigned len;
	pst p;			/* the resulting copy of the string */

	if (!lw)
		i_printfExit(MSG_InvalidSession, cx);
	map = lw->map;
	dol = lw->dol;
	if (n <= 0 || n > dol)
		i_printfExit(MSG_InvalidLineNb, n);

	t = map + n;
	if (show < 0)
		return t->text;
	p = clonePstring(t->text);
	if (show && lw->browseMode)
		removeHiddenNumbers(p, '\n');
	return p;
}				/* fetchLineContext */

pst fetchLine(int n, int show)
{
	return fetchLineContext(n, show, context);
}				/* fetchLine */

static int apparentSizeW(const struct ebWindow *w, bool browsing)
{
	int ln, size = 0;
	pst p;
	for (ln = 1; ln <= w->dol; ++ln) {
		p = w->map[ln].text;
		while (*p != '\n') {
			if (*p == InternalCodeChar && browsing && w->browseMode) {
				++p;
				while (isdigitByte(*p))
					++p;
				if (strchr("<>{}", *p))
					++size;
				++p;
				continue;
			}
			++p, ++size;
		}
		++size;
	}			/* loop over lines */
	if (w->nlMode)
		--size;
	return size;
}				/* apparentSizeW */

static int apparentSize(int cx, bool browsing)
{
	const struct ebWindow *w;
	if (cx <= 0 || cx >= MAXSESSION || (w = sessionList[cx].lw) == 0) {
		setError(MSG_SessionInactive, cx);
		return -1;
	}
	return apparentSizeW(w, browsing);
}				/* apparentSize */

int currentBufferSize(void)
{
	return apparentSize(context, cw->browseMode);
}				/* currentBufferSize */

/* get the directory suffix for a file.
 * This only makes sense in directory mode. */
static char *dirSuffixContext(int n, int cx)
{
	static char suffix[4];
	struct ebWindow *lw = sessionList[cx].lw;

	suffix[0] = 0;
	if (lw->dirMode) {
		struct lineMap *s = lw->map + n;
		suffix[0] = s->ds1;
		suffix[1] = s->ds2;
		suffix[2] = 0;
	}
	return suffix;
}				/* dirSuffixContext */

static char *dirSuffix(int n)
{
	return dirSuffixContext(n, context);
}				/* dirSuffix */

/* Display a line to the screen, with a limit on output length. */
void displayLine(int n)
{
	pst line = fetchLine(n, 1);
	pst s = line;
	int cnt = 0;
	uchar c;

	if (cmd == 'n')
		printf("%d ", n);
	if (endMarks == 2 || endMarks && cmd == 'l')
		printf("^");

	while ((c = *s++) != '\n') {
		bool expand = false;
		if (c == 0 || c == '\r' || c == '\x1b')
			expand = true;
		if (cmd == 'l') {
/* show tabs and backspaces, ed style */
			if (c == '\b')
				c = '<';
			if (c == '\t')
				c = '>';
			if (c < ' ' || c == 0x7f || c >= 0x80 && listNA)
				expand = true;
		}		/* list */
		if (expand)
			printf("~%02X", c), cnt += 3;
		else
			printf("%c", c), ++cnt;
		if (cnt >= displayLength)
			break;
	}			/* loop over line */

	if (cnt >= displayLength)
		printf("...");
	if (cw->dirMode) {
		printf("%s", dirSuffix(n));
		if (cw->r_map) {
			s = cw->r_map[n].text;
			if (*s)
				printf(" %s", s);
		}
	}
	if (endMarks == 2 || endMarks && cmd == 'l')
		printf("$");
	nl();

	free(line);
}				/* displayLine */

static void printDot(void)
{
	if (cw->dot)
		displayLine(cw->dot);
	else
		i_puts(MSG_Empty);
}				/* printDot */

/* By default, readline's filename completion appends a single space
 * character to a filename when there are no alternative completions.
 * Since the r, w, and e commands treat spaces literally, this default
 * causes tab completion to behave unintuitively in edbrowse.
 * Solution: write our own completion function,
 * which wraps readline's filename completer.  */

static char *edbrowse_completion(const char *text, int state)
{
	rl_completion_append_character = '\0';
	return rl_filename_completion_function(text, state);
}				/* edbrowse_completion */

void initializeReadline(void)
{
	rl_completion_entry_function = edbrowse_completion;
}				/* initializeReadline */

/* Get a line from standard in.  Need not be a terminal.
 * Each input line is limited to 255 chars.
 * On Unix cooked mode, that's as long as a line can be anyways.
 * This routine returns the line in a static string.
 * If you want to keep it, better make a copy.
 * ~xx is converted from hex, a way to enter nonascii chars.
 * This is the opposite of displayLine() above.
 * But you can't enter a newline this way; I won't permit it.
 * The only newline is the one corresponding to the end of your text,
 * when you hit enter. This terminates the line.
 * As we described earlier, this is a perl string.
 * It may contain nulls, and is terminated by newline.
 * The program exits on EOF.
 * If you hit interrupt at this point, I print a message
 * and ask for your line again. */

pst inputLine(void)
{
	static uchar line[MAXTTYLINE];
	int i, j, len;
	uchar c, d, e;
	static char *last_rl;
	uchar *s;
	int delay_sec, delay_ms;

top:
	intFlag = false;
	inInput = true;
	nzFree(last_rl);
	last_rl = 0;
	s = 0;

	if (timerWait(&delay_sec, &delay_ms)) {
/* timers are pending, use select to wait on input or run the first timer. */
		fd_set channels;
		int rc;
		struct timeval tv;
		if (!(delay_sec | delay_ms))
			goto dotimers;
		tv.tv_sec = delay_sec;
		tv.tv_usec = delay_ms * 1000;
		memset(&channels, 0, sizeof(channels));
		FD_SET(0, &channels);
		rc = select(1, &channels, 0, 0, &tv);
		if (rc < 0)
			goto interrupt;
		if (rc == 0) {	/* timeout */
dotimers:
			runTimers();
			goto top;
		}
	}

	if (inputReadLine && isInteractive) {
		last_rl = readline("");
		if (*last_rl)
			add_history(last_rl);
		s = (uchar *) last_rl;
	} else {
		s = (uchar *) fgets((char *)line, sizeof(line), stdin);
	}

	if (!s) {
interrupt:
		if (intFlag)
			goto top;
		i_puts(MSG_EndFile);
		ebClose(1);
	}
	inInput = false;
	intFlag = false;

	i = j = 0;
	if (last_rl) {
		len = strlen(s);
	} else {
		len = sizeof(line) - 1;
	}

	while (i < len && (c = s[i]) != '\n') {
/* A bug in my keyboard causes nulls to be entered from time to time. */
		if (c == 0)
			c = ' ';
		if (c != '~') {
addchar:
			s[j++] = c;
			++i;
			continue;
		}
		d = s[i + 1];
		if (d == '~') {
			++i;
			goto addchar;
		}
		if (!isxdigit(d))
			goto addchar;
		e = s[i + 2];
		if (!isxdigit(e))
			goto addchar;
		c = fromHex(d, e);
		if (c == '\n')
			c = 0;
		i += 2;
		goto addchar;
	}			/* loop over input chars */
	s[j] = 0;

	if (jexmode) {
		if (stringEqual(s, ".")) {
			jexmode = false;
			puts("bye");
		} else {
			char *result =
			    jsRunScriptResult(cw->winobj, s, "jex", 1);
			if (result)
				puts(result);
			nzFree(result);
		}
		goto top;
	}

/* rest of edbrowse expects this line to be nl terminated */
	s[j] = '\n';
	return s;
}				/* inputLine */

static struct {
	char lhs[MAXRE], rhs[MAXRE];
	bool lhs_yes, rhs_yes;
} globalSubs;

static void saveSubstitutionStrings(void)
{
	if (!searchStringsAll)
		return;
	if (!cw)
		return;
	globalSubs.lhs_yes = cw->lhs_yes;
	strcpy(globalSubs.lhs, cw->lhs);
	globalSubs.rhs_yes = cw->rhs_yes;
	strcpy(globalSubs.rhs, cw->rhs);
}				/* saveSubstitutionStrings */

static void restoreSubstitutionStrings(struct ebWindow *nw)
{
	if (!searchStringsAll)
		return;
	if (!nw)
		return;
	nw->lhs_yes = globalSubs.lhs_yes;
	strcpy(nw->lhs, globalSubs.lhs);
	nw->rhs_yes = globalSubs.rhs_yes;
	strcpy(nw->rhs, globalSubs.rhs);
}				/* restoreSubstitutionStrings */

/* Create a new window, with default variables. */
static struct ebWindow *createWindow(void)
{
	struct ebWindow *nw;	/* the new window */
	nw = allocZeroMem(sizeof(struct ebWindow));
	saveSubstitutionStrings();
	restoreSubstitutionStrings(nw);
	return nw;
}				/* createWindow */

/* for debugging */
static void print_pst(pst p)
{
	do
		printf("%c", *p);
	while (*p++ != '\n');
}				/* print_pst */

static void freeLine(struct lineMap *t)
{
	if (debugLevel >= 8) {
		printf("free ");
		print_pst(t->text);
	}
	free(t->text);
}				/* freeLine */

static void freeWindowLines(struct lineMap *map)
{
	struct lineMap *t;
	int cnt = 0;

	if (t = map) {
		for (++t; t->text; ++t) {
			freeLine(t);
			++cnt;
		}
		free(map);
	}

	debugPrint(6, "freeWindowLines = %d", cnt);
}				/* freeWindowLines */

/*********************************************************************
Garbage collection for text lines.
There is an undo window that holds a snapshot of the buffer as it was before.
not a copy of all the text, but a copy of map,
and dot and dollar and some other things.
This starts out with map = 0.
The u command swaps undoWindow and current window (cw).
Thus another u undoes your undo.
You can step back through all your changes,
popping a stack like a modern editor. Sorry.
Just don't screw up more than once,
and don't save your file til you're sure it's ok.
No autosave feature here, I never liked that anyways.
So at the start of every command not under g//, set madeChanges = false.
If we're about to change something in the buffer, set madeChanges = true.
But if madeChanges was false, i.e. this is the first change coming,
call undoPush().
This pushes the undo window off the cliff,
and that means we have to free the text first.
Call undoCompare().
This finds any lines in the undo window that aren't in cw and frees them.
That is a quicksort on both maps and then a comm -23.
Then it frees undoWindow.map just to make sure we don't free things twice.
Then undoPush copies cw onto undoWindow, ready for the u command.
Return, and the calling function makes its change.
But we call undoCompare at other times, like switching buffers,
pop the window stack, browse, or quit.
These make undo impossible, so free the lines in the undo window.
*********************************************************************/

static bool madeChanges;
static struct ebWindow undoWindow;

/* quick sort compare */
static int qscmp(const void *s, const void *t)
{
	return memcmp(s, t, sizeof(char *));
}				/* qscmp */

/* Free undo lines not used by the current session. */
static void undoCompare(void)
{
	const struct lineMap *cmap = cw->map;
	struct lineMap *map = undoWindow.map;
	struct lineMap *cmap2;
	struct lineMap *s, *t;
	int diff, ln, cnt = 0;

	if (!cmap) {
		debugPrint(6, "undoCompare no current map");
		freeWindowLines(undoWindow.map);
		undoWindow.map = 0;
		return;
	}

	if (!map) {
		debugPrint(6, "undoCompare no undo map");
		return;
	}

/* sort both arrays, run comm, and find out which lines are not needed any more,
then free them.
 * Use quick sort; some files are a million lines long.
 * I have to copy the second array, so I can sort it.
 * The first I can sort in place, cause it's going to get thrown away. */

	cmap2 = allocMem((cw->dol + 2) * LMSIZE);
	memcpy(cmap2, cmap, (cw->dol + 2) * LMSIZE);
	debugPrint(8, "qsort %d %d", undoWindow.dol, cw->dol);
	qsort(map + 1, undoWindow.dol, LMSIZE, qscmp);
	qsort(cmap2 + 1, cw->dol, LMSIZE, qscmp);

	s = map + 1;
	t = cmap2 + 1;
	while (s->text && t->text) {
		diff = memcmp(s, t, sizeof(char *));
		if (!diff) {
			++s, ++t;
			continue;
		}
		if (diff > 0) {
			++t;
			continue;
		}
		freeLine(s);
		++s;
		++cnt;
	}

	while (s->text) {
		freeLine(s);
		++s;
		++cnt;
	}

	free(cmap2);
	free(map);
	undoWindow.map = 0;
	debugPrint(6, "undoCompare strip %d", cnt);
}				/* undoCompare */

static void undoPush(void)
{
	struct ebWindow *uw;

/* if in browse mode, we really shouldn't be here at all!
 * But we could if substituting on an input field, since substitute is also
 * a regular ed command. */
	if (cw->browseMode)
		return;

	if (madeChanges)
		return;
	madeChanges = true;

	cw->undoable = true;
	if (!cw->quitMode)
		cw->changeMode = true;

	undoCompare();

	uw = &undoWindow;
	uw->dot = cw->dot;
	uw->dol = cw->dol;
	memcpy(uw->labels, cw->labels, 26 * sizeof(int));
	uw->binMode = cw->binMode;
	uw->nlMode = cw->nlMode;
	uw->dirMode = cw->dirMode;
	if (cw->map) {
		uw->map = allocMem((cw->dol + 2) * LMSIZE);
		memcpy(uw->map, cw->map, (cw->dol + 2) * LMSIZE);
	}
}				/* undoPush */

static void freeWindow(struct ebWindow *w)
{
	freeTags(w);
	delTimers(w);
	freeJavaContext(w);
	freeWindowLines(w->map);
	freeWindowLines(w->r_map);
	nzFree(w->dw);
	nzFree(w->ft);
	nzFree(w->hbase);
	nzFree(w->lastrender);
	nzFree(w->fd);
	nzFree(w->fk);
	nzFree(w->mailInfo);
	nzFree(w->fileName);
	nzFree(w->firstURL);
	nzFree(w->referrer);
	nzFree(w->baseDirName);
	free(w);
}				/* freeWindow */

/*********************************************************************
Here are a few routines to switch contexts from one buffer to another.
This is how the user edits multiple sessions, or browses multiple
web pages, simultaneously.
*********************************************************************/

bool cxCompare(int cx)
{
	if (cx == 0) {
		setError(MSG_Session0);
		return false;
	}
	if (cx >= MAXSESSION) {
		setError(MSG_SessionHigh, cx, MAXSESSION - 1);
		return false;
	}
	if (cx != context)
		return true;	/* ok */
	setError(MSG_SessionCurrent, cx);
	return false;
}				/*cxCompare */

/* is a context active? */
bool cxActive(int cx)
{
	if (cx <= 0 || cx >= MAXSESSION)
		i_printfExit(MSG_SessionOutRange, cx);
	if (sessionList[cx].lw)
		return true;
	setError(MSG_SessionInactive, cx);
	return false;
}				/* cxActive */

static void cxInit(int cx)
{
	struct ebWindow *lw = createWindow();
	if (sessionList[cx].lw)
		i_printfExit(MSG_DoubleInit, cx);
	sessionList[cx].fw = sessionList[cx].lw = lw;
}				/* cxInit */

bool cxQuit(int cx, int action)
{
	struct ebWindow *w = sessionList[cx].lw;
	if (!w)
		i_printfExit(MSG_QuitNoActive, cx);

/* action = 3 means we can trash data */
	if (action == 3) {
		w->changeMode = false;
		action = 2;
	}

/* We might be trashing data, make sure that's ok. */
	if (w->changeMode &&
/* something in the buffer has changed */
	    lastq != cx &&
/* last command was not q */
	    !ismc &&
/* not in fetchmail mode, which uses the same buffer over and over again */
	    !(w->dirMode | w->sqlMode) &&
/* not directory or sql mode */
	    (!w->fileName || !isURL(w->fileName))) {
/* not changing a url */
		lastqq = cx;
		setError(MSG_ExpectW);
		if (cx != context)
			setError(MSG_ExpectWX, cx);
		return false;
	}

	if (!action)
		return true;	/* just a test */

	if (cx == context) {
/* Don't need to retain the undo lines. */
		undoCompare();
	}

	if (action == 2) {
		while (w) {
			struct ebWindow *p = w->prev;
			freeWindow(w);
			w = p;
		}
		sessionList[cx].fw = sessionList[cx].lw = 0;
	} else
		freeWindow(w);

	if (cx == context)
		cw = 0;

	return true;
}				/* cxQuit */

/* Switch to another edit session.
 * This assumes cxCompare has succeeded - we're moving to a different context.
 * Pass the context number and an interactive flag. */
void cxSwitch(int cx, bool interactive)
{
	bool created = false;
	struct ebWindow *nw = sessionList[cx].lw;	/* the new window */
	if (!nw) {
		cxInit(cx);
		nw = sessionList[cx].lw;
		created = true;
	} else {
		saveSubstitutionStrings();
		restoreSubstitutionStrings(nw);
	}

	if (cw) {
		undoCompare();
		cw->undoable = false;
	}
	cw = nw;
	cs = sessionList + cx;
	context = cx;
	if (interactive && debugLevel) {
		if (created)
			i_puts(MSG_SessionNew);
		else if (cw->fileName)
			puts(cw->fileName);
		else
			i_puts(MSG_NoFile);
	}

/* The next line is required when this function is called from main(),
 * when the first arg is a url and there is a second arg. */
	startRange = endRange = cw->dot;
}				/* cxSwitch */

/* This function is called for web redirection, by the refresh command,
 * or by window.location = new_url. */
char *newlocation;
int newloc_d;			/* possible delay */
static bool newloc_rf;		/* refresh the buffer */
bool js_redirects;

void gotoLocation(char *url, int delay, bool rf)
{
	if (newlocation && delay >= newloc_d) {
		nzFree(url);
		return;
	}
	nzFree(newlocation);
	newlocation = url;
	newloc_d = delay;
	newloc_rf = rf;
	if (!delay)
		js_redirects = true;
}				/* gotoLocation */

static struct lineMap *newpiece;

/* for debugging */
static void show_newpiece(int cnt)
{
	const struct lineMap *t = newpiece;
	const char *s;
	printf("newpiece %d\n", cnt);
	while (cnt) {
		for (s = t->text; *s != '\n'; ++s)
			printf("%c", *s);
		printf("\n");
		++t, --cnt;
	}
}				/* show_newpiece */

/* Adjust the map of line numbers -- we have inserted text.
 * Also shift the downstream labels.
 * Pass the string containing the new line numbers, and the dest line number. */
static void addToMap(int nlines, int destl)
{
	struct lineMap *newmap;
	int i, ln;
	int svdol = cw->dol;

	if (nlines == 0)
		i_printfExit(MSG_EmptyPiece);

	if (sizeof(int) == 4) {
		if (nlines > MAXLINES - cw->dol)
			i_printfExit(MSG_LineLimit);
	}

/* browse has no undo command */
	if (!cw->browseMode)
		undoPush();

/* adjust labels */
	for (i = 0; i < 26; ++i) {
		ln = cw->labels[i];
		if (ln <= destl)
			continue;
		cw->labels[i] += nlines;
	}
	cw->dot = destl + nlines;
	cw->dol += nlines;

	newmap = allocMem((cw->dol + 2) * LMSIZE);
	if (destl)
		memcpy(newmap, cw->map, (destl + 1) * LMSIZE);
	else
		memset(newmap, 0, LMSIZE);
/* insert new piece here */
	memcpy(newmap + destl + 1, newpiece, nlines * LMSIZE);
/* put on the last piece */
	if (destl < svdol)
		memcpy(newmap + destl + nlines + 1, cw->map + destl + 1,
		       (svdol - destl + 1) * LMSIZE);
	else
		memset(newmap + destl + nlines + 1, 0, LMSIZE);

	nzFree(cw->map);
	cw->map = newmap;
	free(newpiece);
	newpiece = 0;
}				/* addToMap */

/* Add a block of text into the buffer; uses addToMap(). */
bool addTextToBuffer(const pst inbuf, int length, int destl, bool onside)
{
	int i, j, linecount = 0;
	struct lineMap *t;

	for (i = 0; i < length; ++i)
		if (inbuf[i] == '\n') {
			++linecount;
			if (sizeof(int) == 4) {
				if (linecount + cw->dol > MAXLINES)
					i_printfExit(MSG_LineLimit);
			}
		}

	if (destl == cw->dol)
		cw->nlMode = false;
	if (inbuf[length - 1] != '\n') {
/* doesn't end in newline */
		++linecount;	/* last line wasn't counted */
		if (destl == cw->dol) {
			cw->nlMode = true;
			if (cmd != 'b' && !cw->binMode && !ismc && !onside)
				i_puts(MSG_NoTrailing);
		}
	}
	/* missing newline */
	newpiece = t = allocZeroMem(linecount * LMSIZE);
	i = 0;
	while (i < length) {	/* another line */
		j = i;
		while (i < length)
			if (inbuf[i++] == '\n')
				break;
		if (inbuf[i - 1] == '\n') {
/* normal line */
			t->text = allocMem(i - j);
		} else {
/* last line with no nl */
			t->text = allocMem(i - j + 1);
			t->text[i - j] = '\n';
		}
		memcpy(t->text, inbuf + j, i - j);
		++t;
	}			/* loop breaking inbuf into lines */

	addToMap(linecount, destl);
	return true;
}				/* addTextToBuffer */

/* Pass input lines straight into the buffer, until the user enters . */

static bool inputLinesIntoBuffer(void)
{
	pst line;
	int linecount = 0, cap;
	struct lineMap *t;
/* I would use the static variable newpiece to build the new map of lines,
 * as other routines do, but this one is multiline input, and a javascript
 * timer can sneak in and add text, thus clobbering newpiece,
 * so I need a local variable. */
	struct lineMap *np;

	cap = 128;
	np = t = allocZeroMem(cap * LMSIZE);

	if (linePending[0])
		line = linePending;
	else
		line = inputLine();

	while (line[0] != '.' || line[1] != '\n') {
		if (linecount == cap) {
			cap *= 2;
			np = reallocMem(np, cap * LMSIZE);
			t = np + linecount;
		}
		t->text = clonePstring(line);
		t->ds1 = t->ds2 = 0;
		++t, ++linecount;
		line = inputLine();
	}

	if (!linecount) {	/* no lines entered */
		free(np);
		cw->dot = endRange;
		if (!cw->dot && cw->dol)
			cw->dot = 1;
		return true;
	}

	if (endRange == cw->dol)
		cw->nlMode = false;
	newpiece = np;
	addToMap(linecount, endRange);
	return true;
}				/* inputLinesIntoBuffer */

/* create the full pathname for a file that you are viewing in directory mode. */
/* This is static, with a limit on path length. */
static char *makeAbsPath(const char *f)
{
	static char path[ABSPATH + 200];
	if (strlen(cw->baseDirName) + strlen(f) > ABSPATH - 2) {
		setError(MSG_PathNameLong, ABSPATH);
		return 0;
	}
	sprintf(path, "%s/%s", cw->baseDirName, f);
	return path;
}				/* makeAbsPath */

/* Delete a block of text. */
void delText(int start, int end)
{
	int i, j, ln;

/* browse has no undo command */
	if (cw->browseMode) {
		for (ln = start; ln <= end; ++ln)
			nzFree(cw->map[ln].text);
	} else {
		undoPush();
	}

	if (end == cw->dol)
		cw->nlMode = false;
	j = end - start + 1;
	memmove(cw->map + start, cw->map + end + 1,
		(cw->dol - end + 1) * LMSIZE);

/* move the labels */
	for (i = 0; i < 26; ++i) {
		ln = cw->labels[i];
		if (ln < start)
			continue;
		if (ln <= end) {
			cw->labels[i] = 0;
			continue;
		}
		cw->labels[i] -= j;
	}

	cw->dol -= j;
	cw->dot = start;
	if (cw->dot > cw->dol)
		cw->dot = cw->dol;
/* by convention an empty buffer has no map */
	if (!cw->dol) {
		free(cw->map);
		cw->map = 0;
	}
}				/* delText */

/* Delete files from a directory as you delete lines.
 * Set dw to move them to your recycle bin.
 * Set dx to delete them outright. */

static bool delFiles(void)
{
	int ln, cnt;

	if (!dirWrite) {
		setError(MSG_DirNoWrite);
		return false;
	}

	if (dirWrite == 1 && !recycleBin) {
		setError(MSG_NoRecycle);
		return false;
	}

	ln = startRange;
	cnt = endRange - startRange + 1;
	while (cnt--) {
		char *file, *t, *path, *ftype;
		file = (char *)fetchLine(ln, 0);
		t = strchr(file, '\n');
		if (!t)
			i_printfExit(MSG_NoNlOnDir, file);
		*t = 0;
		path = makeAbsPath(file);
		if (!path) {
			free(file);
			return false;
		}

		ftype = dirSuffix(ln);
		if (dirWrite == 2 && *ftype == '/') {
			setError(MSG_NoDirDelete);
			free(file);
			return false;
		}

		if (dirWrite == 2 || *ftype && strchr("@<*^|", *ftype)) {
unlink:
			if (unlink(path)) {
				setError(MSG_NoRemove, file);
				free(file);
				return false;
			}
		} else {
			char bin[ABSPATH];
			sprintf(bin, "%s/%s", recycleBin, file);
			if (rename(path, bin)) {
				if (errno == EXDEV) {
					char *rmbuf;
					int rmlen;
					if (*ftype == '/') {
						setError(MSG_CopyMoveDir);
						free(file);
						return false;
					}
					if (!fileIntoMemory
					    (path, &rmbuf, &rmlen)) {
						free(file);
						return false;
					}
					if (!memoryOutToFile(bin, rmbuf, rmlen,
							     MSG_TempNoCreate2,
							     MSG_NoWrite2)) {
						free(file);
						nzFree(rmbuf);
						return false;
					}
					nzFree(rmbuf);
					goto unlink;
				}

				setError(MSG_NoMoveToTrash, file);
				free(file);
				return false;
			}
		}

		free(file);
		delText(ln, ln);
	}

	return true;
}				/* delFiles */

/* Move or copy a block of text. */
/* Uses range variables, hence no parameters. */
static bool moveCopy(void)
{
	int sr = startRange;
	int er = endRange + 1;
	int dl = destLine + 1;
	int i_sr = sr * LMSIZE;	/* indexes into map */
	int i_er = er * LMSIZE;
	int i_dl = dl * LMSIZE;
	int n_lines = er - sr;
	struct lineMap *map = cw->map;
	struct lineMap *newmap, *t;
	int lowcut, highcut, diff, i, ln;

	if (dl > sr && dl < er) {
		setError(MSG_DestInBlock);
		return false;
	}
	if (cmd == 'm' && (dl == er || dl == sr)) {
		if (globSub)
			setError(MSG_NoChange);
		return false;
	}

	undoPush();

	if (cmd == 't') {
		newpiece = t = allocZeroMem(n_lines * LMSIZE);
		for (i = sr; i < er; ++i, ++t)
			t->text = fetchLine(i, 0);
		addToMap(n_lines, destLine);
		return true;
	}

	if (destLine == cw->dol || endRange == cw->dol)
		cw->nlMode = false;

/* All we really need do is rearrange the map. */
	newmap = allocMem((cw->dol + 2) * LMSIZE);
	memcpy(newmap, map, (cw->dol + 2) * LMSIZE);
	if (dl < sr) {
		memcpy(newmap + dl, map + sr, i_er - i_sr);
		memcpy(newmap + dl + er - sr, map + dl, i_sr - i_dl);
	} else {
		memcpy(newmap + sr, map + er, i_dl - i_er);
		memcpy(newmap + sr + dl - er, map + sr, i_er - i_sr);
	}
	free(cw->map);
	cw->map = newmap;

/* now for the labels */
	if (dl < sr) {
		lowcut = dl;
		highcut = er;
		diff = sr - dl;
	} else {
		lowcut = sr;
		highcut = dl;
		diff = dl - er;
	}
	for (i = 0; i < 26; ++i) {
		ln = cw->labels[i];
		if (ln < lowcut)
			continue;
		if (ln >= highcut)
			continue;
		if (ln >= startRange && ln <= endRange) {
			ln += (dl < sr ? -diff : diff);
		} else {
			ln += (dl < sr ? n_lines : -n_lines);
		}
		cw->labels[i] = ln;
	}			/* loop over labels */

	cw->dot = endRange;
	cw->dot += (dl < sr ? -diff : diff);
	return true;
}				/* moveCopy */

/* Join lines from startRange to endRange. */
static bool joinText(void)
{
	int j, size;
	pst newline, t;

	if (startRange == endRange) {
		setError(MSG_Join1);
		return false;
	}

	size = 0;
	for (j = startRange; j <= endRange; ++j)
		size += pstLength(fetchLine(j, -1));
	t = newline = allocMem(size);
	for (j = startRange; j <= endRange; ++j) {
		pst p = fetchLine(j, -1);
		size = pstLength(p);
		memcpy(t, p, size);
		t += size;
		if (j < endRange) {
			t[-1] = ' ';
			if (cmd == 'j')
				--t;
		}
	}

	delText(startRange, endRange);

	newpiece = allocZeroMem(LMSIZE);
	newpiece->text = newline;
	addToMap(1, startRange - 1);

	cw->dot = startRange;
	return true;
}				/* joinText */

/* Read the contents of a directory into the current buffer */
static bool readDirectory(const char *filename)
{
	int len, j, linecount;
	char *v;
	struct lineMap *mptr;
	struct lineMap *backpiece = 0;

	cw->baseDirName = cloneString(filename);
/* get rid of trailing slash */
	len = strlen(cw->baseDirName);
	if (len && cw->baseDirName[len - 1] == '/')
		cw->baseDirName[len - 1] = 0;
/* Understand that the empty string now means / */

/* get the files, or fail if there is a problem */
	if (!sortedDirList(filename, &newpiece, &linecount))
		return false;
	if (!cw->dol) {
		cw->dirMode = true;
		i_puts(MSG_DirMode);
		if (lsformat[0])
			backpiece = allocZeroMem(LMSIZE * (linecount + 2));
	}

	if (!linecount) {	/* empty directory */
		cw->dot = endRange;
		fileSize = 0;
		free(newpiece);
		newpiece = 0;
		nzFree(backpiece);
		goto success;
	}

/* change 0 to nl and count bytes */
	fileSize = 0;
	mptr = newpiece;
	for (j = 0; j < linecount; ++j, ++mptr) {
		char c, ftype;
		pst t = mptr->text;
		char *abspath = makeAbsPath((char *)t);
		while (*t) {
			if (*t == '\n')
				*t = '\t';
			++t;
		}
		*t = '\n';
		len = t - mptr->text;
		fileSize += len + 1;
		if (!abspath)
			continue;	/* should never happen */

		ftype = fileTypeByName(abspath, true);
		if (!ftype)
			continue;
		if (isupperByte(ftype)) {	/* symbolic link */
			if (!cw->dirMode)
				*t = '@', *++t = '\n';
			else
				mptr->ds1 = '@';
			++fileSize;
		}
		ftype = tolower(ftype);
		c = 0;
		if (ftype == 'd')
			c = '/';
		if (ftype == 's')
			c = '^';
		if (ftype == 'c')
			c = '<';
		if (ftype == 'b')
			c = '*';
		if (ftype == 'p')
			c = '|';
		if (c) {
			if (!cw->dirMode)
				*t = c, *++t = '\n';
			else if (mptr->ds1)
				mptr->ds2 = c;
			else
				mptr->ds1 = c;
			++fileSize;
		}

/* extra stat entries on the line */
		if (!lsformat[0])
			continue;
		if (backpiece)
			backpiece[j + 1].text = emptyString;
		v = lsattr(abspath, lsformat);
		if (!v || !*v)
			continue;
		len = strlen(v);
		fileSize += len + 1;
		if (cw->dirMode) {
			backpiece[j + 1].text = cloneString(v);
		} else {
/* have to realloc at this point */
			int l1 = t - mptr->text;
			mptr->text = reallocMem(mptr->text, l1 + len + 3);
			t = mptr->text + l1;
			*t++ = ' ';
			strcpy(t, v);
			t += len;
			*t++ = '\n';
			*t = 0;
		}
	}			/* loop fixing files in the directory scan */

	addToMap(linecount, endRange);
	cw->r_map = backpiece;

success:
	if (cmd == 'r')
		debugPrint(1, "%d", fileSize);
	return true;
}				/* readDirectory */

/* Read a file, or url, into the current buffer.
 * Post/get data is passed, via the second parameter, if it's a URL. */
static bool readFile(const char *filename, const char *post)
{
	char *rbuf;		/* read buffer */
	int readSize;		/* should agree with fileSize */
	int fh;			/* file handle */
	bool rc;		/* return code */
	bool is8859, isutf8;
	char *nopound;
	char filetype;

	serverData = 0;
	serverDataLen = 0;

	if (memEqualCI(filename, "file://", 7)) {
		filename += 7;
		if (!*filename) {
			setError(MSG_MissingFileName);
			return false;
		}
		goto fromdisk;
	}

	if (isURL(filename)) {
		const char *domain = getHostURL(filename);
		if (!domain)
			return false;	/* some kind of error */
		if (!*domain) {
			setError(MSG_DomainEmpty);
			return false;
		}

		rc = httpConnect(filename, (cmd != 'r'), true);
		if (!rc) {
/* The error could have occured after redirection */
			nzFree(changeFileName);
			changeFileName = 0;
			return false;
		}

/* We got some data.  Any warnings along the way have been printed,
 * like 404 file not found, but it's still worth continuing. */
		rbuf = serverData;
		fileSize = readSize = serverDataLen;

		if (fileSize == 0) {	/* empty file */
			nzFree(rbuf);
			cw->dot = endRange;
			return true;
		}

		goto gotdata;
	}

	if (isSQL(filename)) {
		const char *t1, *t2;
		if (!cw->sqlMode) {
			setError(MSG_DBOtherFile);
			return false;
		}
		t1 = strchr(cw->fileName, ']');
		t2 = strchr(filename, ']');
		if (t1 - cw->fileName != t2 - filename ||
		    memcmp(cw->fileName, filename, t2 - filename)) {
			setError(MSG_DBOtherTable);
			return false;
		}
		rc = sqlReadRows(filename, &rbuf);
		if (!rc) {
			nzFree(rbuf);
			if (!cw->dol && cmd != 'r') {
				cw->sqlMode = false;
				nzFree(cw->fileName);
				cw->fileName = 0;
			}
			return false;
		}
		serverData = rbuf;
		fileSize = strlen(rbuf);
		if (rbuf == emptyString)
			return true;
		goto intext;
	}

fromdisk:
/* reading a file from disk */
	filetype = fileTypeByName(filename, false);
	fileSize = 0;
	if (filetype == 'd')
		return readDirectory(filename);

	if ((cmd == 'e' || cmd == 'b') && !cw->mt)
		cw->mt = findMimeByFile(filename);

#if 0
/* special optimization code for going to a renderable file,
 * via a plugin, from directory mode. */
/* Not sure if this is a good idea. */
	if (cmd == 'b' && icmd == 'g' && pluginsOn && cw->mt && cw->mt->outtype) {
		fileSize = 0;
		return true;
	}
#endif

	nopound = cloneString(filename);
	rbuf = findHash(nopound);
	if (rbuf && !filetype)
		*rbuf = 0;
	rc = fileIntoMemory(nopound, &rbuf, &fileSize);
	nzFree(nopound);
	if (!rc)
		return false;
	serverData = rbuf;
	if (fileSize == 0) {	/* empty file */
		free(rbuf);
		cw->dot = endRange;
		return true;
	}

gotdata:

	if (!looksBinary(rbuf, fileSize)) {
		char *tbuf;
/* looks like text.  In DOS, we should have compressed crlf.
 * Let's do that now. */
#ifdef DOSLIKE
		int i, j;
		for (i = j = 0; i < fileSize - 1; ++i) {
			char c = rbuf[i];
			if (c == '\r' && rbuf[i + 1] == '\n')
				continue;
			rbuf[j++] = c;
		}
		fileSize = j;
#endif
		if (iuConvert) {
/* Classify this incoming text as ascii or 8859 or utf8 */
			looks_8859_utf8(rbuf, fileSize, &is8859, &isutf8);
			debugPrint(3, "text type is %s",
				   (isutf8 ? "utf8"
				    : (is8859 ? "8859" : "ascii")));
			if (cons_utf8 && is8859) {
				if (debugLevel >= 2 || debugLevel == 1
				    && !isURL(filename))
					i_puts(MSG_ConvUtf8);
				iso2utf(rbuf, fileSize, &tbuf, &fileSize);
				nzFree(rbuf);
				rbuf = tbuf;
			}
			if (!cons_utf8 && isutf8) {
				if (debugLevel >= 2 || debugLevel == 1
				    && !isURL(filename))
					i_puts(MSG_Conv8859);
				utf2iso(rbuf, fileSize, &tbuf, &fileSize);
				nzFree(rbuf);
				rbuf = tbuf;
			}
			if (!cw->dol) {
				if (isutf8) {
					cw->utf8Mode = true;
					debugPrint(3, "setting utf8 mode");
				}
				if (is8859) {
					cw->iso8859Mode = true;
					debugPrint(3, "setting 8859 mode");
				}
			}
		}
	} else if (binaryDetect & !cw->binMode) {
		i_puts(MSG_BinaryData);
		cw->binMode = true;
	}

intext:
	rc = addTextToBuffer((const pst)rbuf, fileSize, endRange, false);
	free(rbuf);
	return rc;
}				/* readFile */

/* from the command line */
bool readFileArgv(const char *filename)
{
	cmd = 'e';
	return readFile(filename, emptyString);
}				/* readFileArgv */

/* Write a range to a file. */
static bool writeFile(const char *name, int mode)
{
	int i;
	FILE *fh;
	char *modeString;
	int modeString_l;

	if (memEqualCI(name, "file://", 7))
		name += 7;

	if (!*name) {
		setError(MSG_MissingFileName);
		return false;
	}

	if (isURL(name)) {
		setError(MSG_NoWriteURL);
		return false;
	}

	if (isSQL(name)) {
		setError(MSG_WriteDB);
		return false;
	}

	if (!cw->dol) {
		setError(MSG_WriteEmpty);
		return false;
	}

/* mode should be TRUNC or APPEND */
	modeString = initString(&modeString_l);
	if (mode & O_APPEND)
		stringAndChar(&modeString, &modeString_l, 'a');
	else
		stringAndChar(&modeString, &modeString_l, 'w');
	if (cw->binMode)
		stringAndChar(&modeString, &modeString_l, 'b');

	fh = fopen(name, modeString);
	nzFree(modeString);

	if (fh == NULL) {
		setError(MSG_NoCreate2, name);
		return false;
	}

	if (name == cw->fileName && iuConvert) {
/* should we locale convert back? */
		if (cw->iso8859Mode && cons_utf8)
			if (debugLevel >= 1)
				i_puts(MSG_Conv8859);
		if (cw->utf8Mode && !cons_utf8)
			if (debugLevel >= 1)
				i_puts(MSG_ConvUtf8);
	}

	fileSize = 0;
	for (i = startRange; i <= endRange; ++i) {
		pst p = fetchLine(i, (cw->browseMode ? 1 : -1));
		int len = pstLength(p);
		char *suf = dirSuffix(i);
		char *tp;
		int tlen;
		bool alloc_p = cw->browseMode;
		bool rc = true;

		if (!cw->dirMode) {
			if (i == cw->dol && cw->nlMode)
				--len;

			if (name == cw->fileName && iuConvert) {
				if (cw->iso8859Mode && cons_utf8) {
					utf2iso((char *)p, len, &tp, &tlen);
					if (alloc_p)
						free(p);
					alloc_p = true;
					p = (pst) tp;
					if (fwrite(p, tlen, 1, fh) <= 0)
						rc = false;
					len = tlen;
					goto endline;
				}

				if (cw->utf8Mode && !cons_utf8) {
					iso2utf((char *)p, len, &tp, &tlen);
					if (alloc_p)
						free(p);
					alloc_p = true;
					p = (pst) tp;
					if (fwrite(p, tlen, 1, fh) <= 0)
						rc = false;
					len = tlen;
					goto endline;
				}
			}

			if (fwrite(p, len, 1, fh) <= 0)
				rc = false;
			goto endline;
		}

/* Write this line with directory suffix, and possibly attributes */
		--len;
		if (fwrite(p, len, 1, fh) <= 0) {
badwrite:
			rc = false;
			goto endline;
		}
		fileSize += len;

		if (cw->r_map) {
			int l;
/* extra ls stats to write */
			char *extra;
			len = strlen(suf);
			if (len && fwrite(suf, len, 1, fh) <= 0)
				goto badwrite;
			++len;	/* for nl */
			extra = cw->r_map[i].text;
			l = strlen(extra);
			if (l) {
				if (fwrite(" ", 1, 1, fh) <= 0)
					goto badwrite;
				++len;
				if (fwrite(extra, l, 1, fh) <= 0)
					goto badwrite;
				len += l;
			}
			if (fwrite("\n", 1, 1, fh) <= 0)
				goto badwrite;
			goto endline;
		}

		strcat(suf, "\n");
		len = strlen(suf);
		if (fwrite(suf, len, 1, fh) <= 0)
			goto badwrite;

endline:
		if (alloc_p)
			free(p);
		if (!rc) {
			setError(MSG_NoWrite2, name);
			fclose(fh);
			return false;
		}
		fileSize += len;
	}			/* loop over lines */

	fclose(fh);
/* This is not an undoable operation, nor does it change data.
 * In fact the data is "no longer modified" if we have written all of it. */
	if (startRange == 1 && endRange == cw->dol)
		cw->changeMode = false;
	return true;
}				/* writeFile */

static bool readContext(int cx)
{
	struct ebWindow *lw;
	int i, fardol;
	struct lineMap *t;

	if (!cxCompare(cx))
		return false;
	if (!cxActive(cx))
		return false;

	fileSize = 0;
	lw = sessionList[cx].lw;
	fardol = lw->dol;
	if (!fardol)
		return true;
	if (cw->dol == endRange)
		cw->nlMode = false;
	newpiece = t = allocZeroMem(fardol * LMSIZE);
	for (i = 1; i <= fardol; ++i, ++t) {
		pst p = fetchLineContext(i, (lw->dirMode ? -1 : 1), cx);
		int len = pstLength(p);
		pst q;
		if (lw->dirMode) {
			char *suf = dirSuffixContext(i, cx);
			char *q;
			if (lw->r_map) {
				char *extra = lw->r_map[i].text;
				int elen = strlen(extra);
				q = allocMem(len + 4 + elen);
				memcpy(q, p, len);
				--len;
				strcpy(q + len, suf);
				if (elen) {
					strcat(q, " ");
					strcat(q, extra);
				}
				strcat(q, "\n");
			} else {
				q = allocMem(len + 3);
				memcpy(q, p, len);
				--len;
				strcat(suf, "\n");
				strcpy(q + len, suf);
			}
			len = strlen(q);
			p = (pst) q;
		}
		t->text = p;
		fileSize += len;
	}			/* loop over lines in the "other" context */

	addToMap(fardol, endRange);
	if (lw->nlMode) {
		--fileSize;
		if (cw->dol == endRange)
			cw->nlMode = true;
	}
	if (binaryDetect & !cw->binMode && lw->binMode) {
		cw->binMode = true;
		i_puts(MSG_BinaryData);
	}
	return true;
}				/* readContext */

static bool writeContext(int cx)
{
	struct ebWindow *lw;
	int i, len;
	struct lineMap *newmap, *t;
	pst p;
	int fardol = endRange - startRange + 1;

	if (!startRange)
		fardol = 0;
	if (!cxCompare(cx))
		return false;
	if (cxActive(cx) && !cxQuit(cx, 2))
		return false;

	cxInit(cx);
	lw = sessionList[cx].lw;
	fileSize = 0;
	if (startRange) {
		newmap = t = allocZeroMem((fardol + 2) * LMSIZE);
		for (i = startRange, ++t; i <= endRange; ++i, ++t) {
			p = fetchLine(i, (cw->dirMode ? -1 : 1));
			len = pstLength(p);
			if (cw->dirMode) {
				pst q;
				char *suf = dirSuffix(i);
				if (cw->r_map) {
					char *extra = cw->r_map[i].text;
					int elen = strlen(extra);
					q = allocMem(len + 4 + elen);
					memcpy(q, p, len);
					--len;
					strcpy((char *)q + len, suf);
					if (elen) {
						strcat(q, " ");
						strcat(q, extra);
					}
					strcat(q, "\n");
				} else {
					q = allocMem(len + 3);
					memcpy(q, p, len);
					--len;
					strcat(suf, "\n");
					strcpy((char *)q + len, suf);
				}
				len = strlen((char *)q);
				p = q;
			}
			t->text = p;
			fileSize += len;
		}
		lw->map = newmap;
		lw->binMode = cw->binMode;
		if (cw->nlMode && endRange == cw->dol) {
			lw->nlMode = true;
			--fileSize;
		}
	}

	lw->dot = lw->dol = fardol;
	return true;
}				/* writeContext */

static void debrowseSuffix(char *s)
{
	if (!s)
		return;
	while (*s) {
		if (*s == '.' && stringEqual(s, ".browse")) {
			*s = 0;
			return;
		}
		++s;
	}
}				/* debrowseSuffix */

static bool shellEscape(const char *line)
{
	char *newline, *s;
	const char *t;
	pst p;
	char key;
	int linesize, pass, n;
	char *sh;
	char subshell[ABSPATH];

#ifdef DOSLIKE
/* cmd.exe is the windows shell */
	sh = "cmd";
#else
/* preferred shell */
	sh = getenv("SHELL");
	if (!sh || !*sh)
		sh = "/bin/sh";
#endif

	linesize = strlen(line);
	if (!linesize) {
/* interactive shell */
		if (!isInteractive) {
			setError(MSG_SessionBackground);
			return false;
		}
#ifdef DOSLIKE
		system(sh);
#else
/* Ignoring of SIGPIPE propagates across fork-exec. */
/* So stop ignoring it for the duration of system(). */
		signal(SIGPIPE, SIG_DFL);
		sprintf(subshell, "exec %s -i", sh);
		system(subshell);
		signal(SIGPIPE, SIG_IGN);
#endif
		i_puts(MSG_OK);
		return true;
	}

/* Make substitutions within the command line. */
	for (pass = 1; pass <= 2; ++pass) {
		for (t = line; *t; ++t) {
			if (*t != '\'')
				goto addchar;
			if (t > line && isalnumByte(t[-1]))
				goto addchar;
			key = t[1];
			if (key && isalnumByte(t[2]))
				goto addchar;

			if (key == '_') {
				++t;
				if (!cw->fileName)
					continue;
				if (pass == 1) {
					linesize += strlen(cw->fileName);
				} else {
					strcpy(s, cw->fileName);
					s += strlen(s);
				}
				continue;
			}
			/* '_ filename */
			if (key == '.' || key == '-' || key == '+') {
				n = cw->dot;
				if (key == '-')
					--n;
				if (key == '+')
					++n;
				if (n > cw->dol || n == 0) {
					setError(MSG_OutOfRange, key);
					return false;
				}
frombuf:
				++t;
				if (pass == 1) {
					p = fetchLine(n, -1);
					linesize += pstLength(p) - 1;
				} else {
					p = fetchLine(n, 1);
					if (perl2c((char *)p)) {
						free(p);
						setError(MSG_ShellNull);
						return false;
					}
					strcpy(s, (char *)p);
					s += strlen(s);
					free(p);
				}
				continue;
			}
			/* '. current line */
			if (islowerByte(key)) {
				n = cw->labels[key - 'a'];
				if (!n) {
					setError(MSG_NoLabel, key);
					return false;
				}
				goto frombuf;
			}
			/* 'x the line labeled x */
addchar:
			if (pass == 1)
				++linesize;
			else
				*s++ = *t;
		}		/* loop over chars */

		if (pass == 1)
			s = newline = allocMem(linesize + 1);
		else
			*s = 0;
	}			/* two passes */

/* Run the command.  Note that this routine returns success
 * even if the shell command failed.
 * Edbrowse succeeds if it is *able* to run the system command. */
#ifdef DOSLIKE
	system(newline);
#else
	signal(SIGPIPE, SIG_DFL);
	system(newline);
	signal(SIGPIPE, SIG_IGN);
#endif
	i_puts(MSG_OK);
	free(newline);
	return true;
}				/* shellEscape */

/* Valid delimiters for search/substitute.
 * note that \ is conspicuously absent, not a valid delimiter.
 * I alsso avoid nestable delimiters such as parentheses.
 * And no alphanumerics please -- too confusing.
 * ed allows it, but I don't. */
static const char valid_delim[] = "_=!;:`\"',/?@-";
/* And a valid char for starting a line address */
static const char valid_laddr[] = "0123456789-'.$+/?";

/* Check the syntax of a regular expression, before we pass it to pcre.
 * The first char is the delimiter -- we stop at the next delimiter.
 * A pointer to the second delimiter is returned, along with the
 * (possibly reformatted) regular expression. */

static bool
regexpCheck(const char *line, bool isleft, bool ebmuck,
	    char **rexp, const char **split)
{				/* result parameters */
	static char re[MAXRE + 20];
	const char *start;
	char *e = re;
	char c, d;
/* Remember whether a char is "on deck", ready to be modified by * etc. */
	bool ondeck = false;
	bool was_ques = true;	/* previous modifier was ? */
	bool cc = false;	/* are we in a [...] character class */
	int mod;		/* length of modifier */
	int paren = 0;		/* nesting level of parentheses */
/* We wouldn't be here if the line was empty. */
	char delim = *line++;

	*rexp = re;
	if (!strchr(valid_delim, delim)) {
		setError(MSG_BadDelimit);
		return false;
	}
	start = line;

	c = *line;
	if (ebmuck) {
		if (isleft) {
			if (c == delim || c == 0) {
				if (!cw->lhs_yes) {
					setError(MSG_NoSearchString);
					return false;
				}
				strcpy(re, cw->lhs);
				*split = line;
				return true;
			}
/* Interpret lead * or lone [ as literal */
			if (strchr("*?+", c) || c == '[' && !line[1]) {
				*e++ = '\\';
				*e++ = c;
				++line;
				ondeck = true;
			}
		} else if (c == '%' && (line[1] == delim || line[1] == 0)) {
			if (!cw->rhs_yes) {
				setError(MSG_NoReplaceString);
				return false;
			}
			strcpy(re, cw->rhs);
			*split = line + 1;
			return true;
		}
	}
	/* ebmuck tricks */
	while (c = *line) {
		if (e >= re + MAXRE - 3) {
			setError(MSG_RexpLong);
			return false;
		}
		d = line[1];

		if (c == '\\') {
			line += 2;
			if (d == 0) {
				setError(MSG_LineBackslash);
				return false;
			}
			ondeck = true;
			was_ques = false;
/* I can't think of any reason to remove the escaping \ from any character,
 * except ()|, where we reverse the sense of escape. */
			if (ebmuck && isleft && !cc
			    && (d == '(' || d == ')' || d == '|')) {
				if (d == '|')
					ondeck = false, was_ques = true;
				if (d == '(')
					++paren, ondeck = false, was_ques =
					    false;
				if (d == ')')
					--paren;
				if (paren < 0) {
					setError(MSG_UnexpectedRight);
					return false;
				}
				*e++ = d;
				continue;
			}
			if (d == delim || ebmuck && !isleft && d == '&') {
				*e++ = d;
				continue;
			}
/* Nothing special; we retain the escape character. */
			*e++ = c;
			if (isleft && d >= '0' && d <= '7'
			    && (*line < '0' || *line > '7'))
				*e++ = '0';
			*e++ = d;
			continue;
		}

		/* escaping backslash */
		/* Break out if we hit the delimiter. */
		if (c == delim)
			break;

/* Remember, I reverse the sense of ()| */
		if (isleft) {
			if (ebmuck && (c == '(' || c == ')' || c == '|') || c == '^' && line != start && !cc)	/* don't know why we have to do this */
				*e++ = '\\';
			if (c == '$' && d && d != delim)
				*e++ = '\\';
		}

		if (c == '$' && !isleft && isdigitByte(d)) {
			if (d == '0' || isdigitByte(line[2])) {
				setError(MSG_RexpDollar);
				return false;
			}
		}
		/* dollar digit on the right */
		if (!isleft && c == '&' && ebmuck) {
			*e++ = '$';
			*e++ = '0';
			++line;
			continue;
		}

/* push the character */
		*e++ = c;
		++line;

/* No more checks for the rhs */
		if (!isleft)
			continue;

		if (cc) {	/* character class */
			if (c == ']')
				cc = false;
			continue;
		}
		if (c == '[')
			cc = true;

/* Skip all these checks for javascript,
 * it probably has the expression right anyways. */
		if (!ebmuck)
			continue;

/* Modifiers must have a preceding character.
 * Except ? which can reduce the greediness of the others. */
		if (c == '?' && !was_ques) {
			ondeck = false;
			was_ques = true;
			continue;
		}

		mod = 0;
		if (c == '?' || c == '*' || c == '+')
			mod = 1;
		if (c == '{' && isdigitByte(d)) {
			const char *t = line + 1;
			while (isdigitByte(*t))
				++t;
			if (*t == ',')
				++t;
			while (isdigitByte(*t))
				++t;
			if (*t == '}')
				mod = t + 2 - line;
		}
		if (mod) {
			--mod;
			if (mod) {
				strncpy(e, line, mod);
				e += mod;
				line += mod;
			}
			if (!ondeck) {
				*e = 0;
				setError(MSG_RexpModifier, e - mod - 1);
				return false;
			}
			ondeck = false;
			continue;
		}
		/* modifier */
		ondeck = true;
		was_ques = false;
	}			/* loop over chars in the pattern */
	*e = 0;

	*split = line;

	if (ebmuck) {
		if (cc) {
			setError(MSG_NoBracket);
			return false;
		}
		if (paren) {
			setError(MSG_NoParen);
			return false;
		}

		if (isleft) {
			cw->lhs_yes = true;
			strcpy(cw->lhs, re);
		} else {
			cw->rhs_yes = true;
			strcpy(cw->rhs, re);
		}
	}

	debugPrint(7, "%s regexp %s", (isleft ? "search" : "replace"), re);
	return true;
}				/* regexpCheck */

/* regexp variables */
static int re_count;
static int re_vector[11 * 3];
static pcre *re_cc;		/* compiled */
static bool re_utf8 = true;

static void regexpCompile(const char *re, bool ci)
{
	static signed char try8 = 0;	/* 1 is utf8 on, -1 is utf8 off */
	const char *re_error;
	int re_offset;
	int re_opt;

top:
/* Do we need PCRE_NO_AUTO_CAPTURE? */
	re_opt = 0;
	if (ci)
		re_opt |= PCRE_CASELESS;

	if (re_utf8) {
		if (cons_utf8 && !cw->binMode && try8 >= 0) {
			if (try8 == 0) {
				const char *s = getenv("PCREUTF8");
				if (s && stringEqual(s, "off")) {
					try8 = -1;
					goto top;
				}
			}
			try8 = 1;
			re_opt |= PCRE_UTF8;
		}
	}

	re_cc = pcre_compile(re, re_opt, &re_error, &re_offset, 0);
	if (!re_cc && try8 > 0 && strstr(re_error, "PCRE_UTF8 support")) {
		i_puts(MSG_PcreUtf8);
		try8 = -1;
		goto top;
	}
	if (!re_cc && try8 > 0 && strstr(re_error, "invalid UTF-8 string")) {
		i_puts(MSG_BadUtf8String);
	}

	if (!re_cc)
		setError(MSG_RexpError, re_error);
}				/* regexpCompile */

/* Get the start or end of a range.
 * Pass the line containing the address. */
static bool getRangePart(const char *line, int *lineno, const char **split)
{				/* result parameters */
	int ln = cw->dot;	/* this is where we start */
	char first = *line;

	if (isdigitByte(first)) {
		ln = strtol(line, (char **)&line, 10);
	} else if (first == '.') {
		++line;
/* ln is already set */
	} else if (first == '$') {
		++line;
		ln = cw->dol;
	} else if (first == '\'' && islowerByte(line[1])) {
		ln = cw->labels[line[1] - 'a'];
		if (!ln) {
			setError(MSG_NoLabel, line[1]);
			return false;
		}
		line += 2;
	} else if (first == '/' || first == '?') {

		char *re;	/* regular expression */
		bool ci = caseInsensitive;
		signed char incr;	/* forward or back */
/* Don't look through an empty buffer. */
		if (cw->dol == 0) {
			setError(MSG_EmptyBuffer);
			return false;
		}
		if (!regexpCheck(line, true, true, &re, &line))
			return false;
		if (*line == first) {
			++line;
			if (*line == 'i')
				ci = true, ++line;
		}

		/* second delimiter */
		regexpCompile(re, ci);
		if (!re_cc)
			return false;
/* We should probably study the pattern, if the file is large.
 * But then again, it's probably not worth it,
 * since the expressions are simple, and the lines are short. */
		incr = (first == '/' ? 1 : -1);
		while (true) {
			char *subject;
			ln += incr;
			if (ln > cw->dol)
				ln = 1;
			if (ln == 0)
				ln = cw->dol;
			subject = (char *)fetchLine(ln, 1);
			re_count =
			    pcre_exec(re_cc, 0, subject,
				      pstLength((pst) subject) - 1, 0, 0,
				      re_vector, 33);
			free(subject);
			if (re_count < -1) {
				pcre_free(re_cc);
				setError(MSG_RexpError2, ln);
				return (globSub = false);
			}
			if (re_count >= 0)
				break;
			if (ln == cw->dot) {
				pcre_free(re_cc);
				setError(MSG_NotFound);
				return false;
			}
		}		/* loop over lines */
		pcre_free(re_cc);
/* and ln is the line that matches */
	}

	/* search pattern */
	/* Now add or subtract from this number */
	while ((first = *line) == '+' || first == '-') {
		int add = 1;
		++line;
		if (isdigitByte(*line))
			add = strtol(line, (char **)&line, 10);
		ln += (first == '+' ? add : -add);
	}

	if (ln > cw->dol) {
		setError(MSG_LineHigh);
		return false;
	}
	if (ln < 0) {
		setError(MSG_LineLow);
		return false;
	}

	*lineno = ln;
	*split = line;
	return true;
}				/* getRangePart */

/* Apply a regular expression to each line, and then execute
 * a command for each matching, or nonmatching, line.
 * This is the global feature, g/re/p, which gives us the word grep. */
static bool doGlobal(const char *line)
{
	int gcnt = 0;		/* global count */
	bool ci = caseInsensitive;
	bool change;
	char delim = *line;
	struct lineMap *t;
	char *re;		/* regular expression */
	int i, origdot, yesdot, nodot;

	if (!delim) {
		setError(MSG_RexpMissing, icmd);
		return false;
	}

	if (!regexpCheck(line, true, true, &re, &line))
		return false;
	if (*line != delim) {
		setError(MSG_NoDelimit);
		return false;
	}
	++line;
	if (*line == 'i')
		++line, ci = true;
	skipWhite(&line);

/* clean up any previous global flags.
 * Also get ready for javascript, as in g/<->/ i=+
 * which I use in web based gmail to clear out spam etc. */
	for (t = cw->map + 1; t->text; ++t)
		t->gflag = false;

/* Find the lines that match the pattern. */
	regexpCompile(re, ci);
	if (!re_cc)
		return false;
	for (i = startRange; i <= endRange; ++i) {
		char *subject = (char *)fetchLine(i, 1);
		re_count =
		    pcre_exec(re_cc, 0, subject, pstLength((pst) subject) - 1,
			      0, 0, re_vector, 33);
		free(subject);
		if (re_count < -1) {
			pcre_free(re_cc);
			setError(MSG_RexpError2, i);
			return false;
		}
		if (re_count < 0 && cmd == 'v' || re_count >= 0 && cmd == 'g') {
			++gcnt;
			cw->map[i].gflag = true;
		}
	}			/* loop over line */
	pcre_free(re_cc);

	if (!gcnt) {
		setError((cmd == 'v') + MSG_NoMatchG);
		return false;
	}

/* apply the subcommand to every line with a star */
	globSub = true;
	setError(-1);
	if (!*line)
		line = "p";
	origdot = cw->dot;
	yesdot = nodot = 0;
	change = true;
	while (gcnt && change) {
		change = false;	/* kinda like bubble sort */
		for (i = 1; i <= cw->dol; ++i) {
			t = cw->map + i;
			if (!t->gflag)
				continue;
			if (intFlag)
				goto done;
			change = true, --gcnt;
			t->gflag = false;
			cw->dot = i;	/* so we can run the command at this line */
			if (runCommand(line)) {
				yesdot = cw->dot;
/* try this line again, in case we deleted or moved it somewhere else */
				--i;
			} else {
/* error in subcommand might turn global flag off */
				if (!globSub) {
					nodot = i, yesdot = 0;
					goto done;
				}	/* serious error */
			}	/* subcommand succeeds or fails */
		}		/* loop over lines */
	}			/* loop making changes */

done:
	globSub = false;
/* yesdot could be 0, even on success, if all lines are deleted via g/re/d */
	if (yesdot || !cw->dol) {
		cw->dot = yesdot;
		if ((cmd == 's' || cmd == 'i') && subPrint == 1)
			printDot();
	} else if (nodot) {
		cw->dot = nodot;
	} else {
		cw->dot = origdot;
		if (!errorMsg[0])
			setError(MSG_NotModifiedG);
	}
	if (!errorMsg[0] && intFlag)
		setError(MSG_Interrupted);
	return (errorMsg[0] == 0);
}				/* doGlobal */

static void fieldNumProblem(int desc, char c, int n, int nt, int nrt)
{
	if (!nrt) {
		setError(MSG_NoInputFields + desc);
		return;
	}
	if (!n) {
		setError(MSG_ManyInputFields + desc, c, c, nt);
		return;
	}
	if (nt > 1)
		setError(MSG_InputRange, n, c, c, nt);
	else
		setError(MSG_InputRange2, n, c, c);
}				/* fieldNumProblem */

/* Perform a substitution on a given line.
 * The lhs has been compiled, and the rhs is passed in for replacement.
 * Refer to the static variable re_cc for the compiled lhs.
 * Return true for a replacement, false for no replace, and -1 for a problem. */

static char *replaceString;
static int replaceStringLength;
static char *replaceStringEnd;

static int
replaceText(const char *line, int len, const char *rhs,
	    bool ebmuck, int nth, bool global, int ln)
{
	int offset = 0, lastoffset, instance = 0;
	int span;
	char *r;
	int rlen;
	const char *s = line, *s_end, *t;
	char c, d;

	r = initString(&rlen);

	while (true) {
/* find the next match */
		re_count =
		    pcre_exec(re_cc, 0, line, len, offset, 0, re_vector, 33);
		if (re_count < -1) {
			setError(MSG_RexpError2, ln);
			nzFree(r);
			return -1;
		}

		if (re_count < 0)
			break;
		++instance;	/* found another match */
		lastoffset = offset;
		offset = re_vector[1];	/* ready for next iteration */
		if (offset == lastoffset && (nth > 1 || global)) {
			setError(MSG_ManyEmptyStrings);
			nzFree(r);
			return -1;
		}

		if (!global &&instance != nth)
			continue;

/* copy up to the match point */
		s_end = line + re_vector[0];
		span = s_end - s;
		stringAndBytes(&r, &rlen, s, span);
		s = line + offset;

/* Now copy over the rhs */
/* Special case lc mc uc */
		if (ebmuck && (rhs[0] == 'l' || rhs[0] == 'm' || rhs[0] == 'u')
		    && rhs[1] == 'c' && rhs[2] == 0) {
			int savelen = rlen;
			span = re_vector[1] - re_vector[0];
			stringAndBytes(&r, &rlen, line + re_vector[0], span);
			caseShift(r + savelen, rhs[0]);
			if (!global)
				break;
			continue;
		}

		/* copy rhs, watching for $n */
		t = rhs;
		while (c = *t) {
			d = t[1];
			if (c == '\\') {
				t += 2;
				if (d == '$') {
					stringAndChar(&r, &rlen, d);
					continue;
				}
				if (d == 'n') {
					stringAndChar(&r, &rlen, '\n');
					continue;
				}
				if (d == 't') {
					stringAndChar(&r, &rlen, '\t');
					continue;
				}
				if (d == 'b') {
					stringAndChar(&r, &rlen, '\b');
					continue;
				}
				if (d == 'r') {
					stringAndChar(&r, &rlen, '\r');
					continue;
				}
				if (d == 'f') {
					stringAndChar(&r, &rlen, '\f');
					continue;
				}
				if (d == 'a') {
					stringAndChar(&r, &rlen, '\a');
					continue;
				}
				if (d >= '0' && d <= '7') {
					int octal = d - '0';
					d = *t;
					if (d >= '0' && d <= '7') {
						++t;
						octal = 8 * octal + d - '0';
						d = *t;
						if (d >= '0' && d <= '7') {
							++t;
							octal =
							    8 * octal + d - '0';
						}
					}
					stringAndChar(&r, &rlen, octal);
					continue;
				}	/* octal */
				if (!ebmuck)
					stringAndChar(&r, &rlen, '\\');
				stringAndChar(&r, &rlen, d);
				continue;
			}
			/* backslash */
			if (c == '$' && isdigitByte(d)) {
				int y, z;
				t += 2;
				d -= '0';
				if (d > re_count)
					continue;
				y = re_vector[2 * d];
				z = re_vector[2 * d + 1];
				if (y < 0)
					continue;
				span = z - y;
				stringAndBytes(&r, &rlen, line + y, span);
				continue;
			}

			stringAndChar(&r, &rlen, c);
			++t;
		}

		if (!global)
			break;
	}			/* loop matching the regular expression */

	if (!instance) {
		nzFree(r);
		return false;
	}

	if (!global &&instance < nth) {
		nzFree(r);
		return false;
	}

/* We got a match, copy the last span. */
	s_end = line + len;
	span = s_end - s;
	stringAndBytes(&r, &rlen, s, span);

	replaceString = r;
	replaceStringLength = rlen;
	return true;
}				/* replaceText */

static void
findField(const char *line, int ftype, int n,
	  int *total, int *realtotal, int *tagno, char **href,
	  const struct htmlTag **tagp)
{
	const struct htmlTag *t;
	int nt = 0;		/* number of fields total */
	int nrt = 0;		/* the real total, for input fields */
	int nm = 0;		/* number match */
	int j;
	const char *s, *ss;
	char *h, *nmh;
	char c;
	static const char urlok[] =
	    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890,./?@#%&-_+=:~";

	if (href)
		*href = 0;
	if (tagp)
		*tagp = 0;

	if (cw->browseMode) {

		s = line;
		while ((c = *s) != '\n') {
			++s;
			if (c != InternalCodeChar)
				continue;
			j = strtol(s, (char **)&s, 10);
			if (!ftype) {
				if (*s != '{')
					continue;
				++nt, ++nrt;
				if (n == nt || n < 0)
					nm = j;
				if (!n) {
					if (!nm)
						nm = j;
					else
						nm = -1;
				}
			} else {
				if (*s != '<')
					continue;
				if (n > 0) {
					++nt, ++nrt;
					if (n == nt)
						nm = j;
					continue;
				}
				if (n < 0) {
					nm = j;
					++nt, ++nrt;
					continue;
				}
				++nrt;
				t = tagList[j];
				if (ftype == 1 && t->itype <= INP_SUBMIT)
					continue;
				if (ftype == 2 && t->itype > INP_SUBMIT)
					continue;
				++nt;
				if (!nm)
					nm = j;
				else
					nm = -1;
			}
		}		/* loop over line */
	}

	if (nm < 0)
		nm = 0;
	if (total)
		*total = nrt;
	if (realtotal)
		*realtotal = nt;
	if (tagno)
		*tagno = nm;
	if (!ftype && nm) {
		t = tagList[nm];
		if (href)
			*href = cloneString(t->href);
		if (tagp)
			*tagp = t;
		if (href && isJSAlive && t->jv) {
/* defer to the java variable for the reference */
			char *jh = get_property_url(t->jv, false);
			if (jh) {
				if (!*href || !stringEqual(*href, jh)) {
					nzFree(*href);
					*href = jh;
				}
			}
		}
	}

	if (nt || ftype)
		return;

/* Second time through, maybe the url is in plain text. */
	nmh = 0;
	s = line;
	while (true) {
/* skip past weird characters */
		while ((c = *s) != '\n') {
			if (strchr(urlok, c))
				break;
			++s;
		}
		if (c == '\n')
			break;
		ss = s;
		while (strchr(urlok, *s))
			++s;
		h = pullString1(ss, s);
		unpercentURL(h);
		if (!isURL(h)) {
			free(h);
			continue;
		}
		++nt;
		if (n == nt || n < 0) {
			nm = nt;
			nzFree(nmh);
			nmh = h;
			continue;
		}
		if (n) {
			free(h);
			continue;
		}
		if (!nm) {
			nm = nt;
			nmh = h;
			continue;
		}
		free(h);
		nm = -1;
		free(nmh);
		nmh = 0;
	}			/* loop over line */

	if (nm < 0)
		nm = 0;
	if (total)
		*total = nt;
	if (realtotal)
		*realtotal = nt;
	if (href)
		*href = nmh;
	else
		nzFree(nmh);
}				/* findField */

static void
findInputField(const char *line, int ftype, int n, int *total, int *realtotal,
	       int *tagno)
{
	findField(line, ftype, n, total, realtotal, tagno, 0, 0);
}				/* findInputField */

/* Substitute text on the lines in startRange through endRange.
 * We could be changing the text in an input field.
 * If so, we'll call infReplace().
 * Also, we might be indirectory mode, whence we must rename the file.
 * This is a complicated function!
 * The return can be true or false, with the usual meaning,
 * but also a return of -1, which is failure,
 * and an indication that we need to abort any g// in progress.
 * It's a serious problem. */

static int substituteText(const char *line)
{
	int whichField = 0;
	bool bl_mode = false;	/* running the bl command */
	bool g_mode = false;	/* s/x/y/g */
	bool ci = caseInsensitive;
	bool save_nlMode;
	char c, *s, *t;
	int nth = 0;		/* s/x/y/7 */
	int lastSubst = 0;	/* last successful substitution */
	char *re;		/* the parsed regular expression */
	int ln;			/* line number */
	int j, linecount, slashcount, nullcount, tagno, total, realtotal;
	char lhs[MAXRE], rhs[MAXRE];
	struct lineMap *mptr;

	replaceString = 0;

	subPrint = 1;		/* default is to print the last line substituted */
	re_cc = 0;
	if (stringEqual(line, "`bl"))
		bl_mode = true, breakLineSetup();

	if (!bl_mode) {
/* watch for s2/x/y/ for the second input field */
		if (isdigitByte(*line))
			whichField = strtol(line, (char **)&line, 10);
		else if (*line == '$')
			whichField = -1, ++line;
		if (!*line) {
			setError(MSG_RexpMissing2, icmd);
			return -1;
		}

		if (cw->dirMode && !dirWrite) {
			setError(MSG_DirNoWrite);
			return -1;
		}

		if (!regexpCheck(line, true, true, &re, &line))
			return -1;
		strcpy(lhs, re);
		if (!*line) {
			setError(MSG_NoDelimit);
			return -1;
		}
		if (!regexpCheck(line, false, true, &re, &line))
			return -1;
		strcpy(rhs, re);

		if (*line) {	/* third delimiter */
			++line;
			subPrint = 0;
			while (c = *line) {
				if (c == 'g') {
					g_mode = true;
					++line;
					continue;
				}
				if (c == 'i') {
					ci = true;
					++line;
					continue;
				}
				if (c == 'p') {
					subPrint = 2;
					++line;
					continue;
				}
				if (isdigitByte(c)) {
					if (nth) {
						setError(MSG_SubNumbersMany);
						return -1;
					}
					nth = strtol(line, (char **)&line, 10);
					continue;
				}	/* number */
				setError(MSG_SubSuffixBad);
				return -1;
			}	/* loop gathering suffix flags */
			if (g_mode && nth) {
				setError(MSG_SubNumberG);
				return -1;
			}
		}		/* closing delimiter */
		if (nth == 0 && !g_mode)
			nth = 1;

		regexpCompile(lhs, ci);
		if (!re_cc)
			return -1;
	} else {

		subPrint = 0;
	}			/* bl_mode or not */

	if (!globSub)
		setError(-1);

	for (ln = startRange; ln <= endRange && !intFlag; ++ln) {
		char *p;
		int len;

		replaceString = 0;

		p = (char *)fetchLine(ln, -1);
		len = pstLength((pst) p);

		if (bl_mode) {
			int newlen;
			if (!breakLine(p, len, &newlen)) {
/* you just should never be here */
				setError(MSG_BreakLong, 0);
				nzFree(breakLineResult);
				breakLineResult = 0;
				return -1;
			}
/* empty line is not allowed */
			if (!newlen)
				breakLineResult[newlen++] = '\n';
/* perhaps no changes were made */
			if (newlen == len && !memcmp(p, breakLineResult, len)) {
				nzFree(breakLineResult);
				breakLineResult = 0;
				continue;
			}
			replaceString = breakLineResult;
/* But the regular substitute doesn't have the \n on the end.
 * We need to make this one conform. */
			replaceStringLength = newlen - 1;
		} else {

			if (cw->browseMode) {
				char search[20];
				char searchend[4];
				undoPush();
				findInputField(p, 1, whichField, &total,
					       &realtotal, &tagno);
				if (!tagno) {
					fieldNumProblem(0, 'i', whichField,
							total, realtotal);
					continue;
				}
				sprintf(search, "%c%d<", InternalCodeChar,
					tagno);
				sprintf(searchend, "%c0>", InternalCodeChar);
/* Ok, if the line contains a null, this ain't gonna work. */
				s = strstr(p, search);
				if (!s)
					continue;
				s = strchr(s, '<') + 1;
				t = strstr(s, searchend);
				if (!t)
					continue;
				j = replaceText(s, t - s, rhs, true, nth,
						g_mode, ln);
			} else {
				j = replaceText(p, len - 1, rhs, true, nth,
						g_mode, ln);
			}
			if (j < 0)
				goto abort;
			if (!j)
				continue;
		}

/* Did we split this line into many lines? */
		replaceStringEnd = replaceString + replaceStringLength;
		linecount = slashcount = nullcount = 0;
		for (t = replaceString; t < replaceStringEnd; ++t) {
			c = *t;
			if (c == '\n')
				++linecount;
			if (c == 0)
				++nullcount;
			if (c == '/')
				++slashcount;
		}

		if (cw->sqlMode) {
			if (linecount) {
				setError(MSG_ReplaceNewline);
				goto abort;
			}

			if (nullcount) {
				setError(MSG_ReplaceNull);
				goto abort;
			}

			*replaceStringEnd = '\n';
			if (!sqlUpdateRow((pst) p, len - 1, (pst) replaceString,
					  replaceStringLength))
				goto abort;
		}

		if (cw->dirMode) {
/* move the file, then update the text */
			char src[ABSPATH], *dest;
			if (slashcount + nullcount + linecount) {
				setError(MSG_DirNameBad);
				goto abort;
			}
			p[len - 1] = 0;	/* temporary */
			t = makeAbsPath(p);
			p[len - 1] = '\n';
			if (!t)
				goto abort;
			strcpy(src, t);
			*replaceStringEnd = 0;
			dest = makeAbsPath(replaceString);
			if (!dest)
				goto abort;
			if (!stringEqual(src, dest)) {
				if (fileTypeByName(dest, true)) {
					setError(MSG_DestFileExists);
					goto abort;
				}
				if (rename(src, dest)) {
					setError(MSG_NoRename, dest);
					goto abort;
				}
			}	/* source and dest are different */
		}

		if (cw->browseMode) {
			if (nullcount) {
				setError(MSG_InputNull2);
				goto abort;
			}
			if (linecount) {
				setError(MSG_InputNewline2);
				goto abort;
			}
			*replaceStringEnd = 0;
/* We're managing our own printing, so leave notify = 0 */
			if (!infReplace(tagno, replaceString, false))
				goto abort;
			undoCompare();
			cw->undoable = false;
		} else {

			*replaceStringEnd = '\n';
			if (!linecount) {
/* normal substitute */
				undoPush();
				mptr = cw->map + ln;
				mptr->text = allocMem(replaceStringLength + 1);
				memcpy(mptr->text, replaceString,
				       replaceStringLength + 1);
				if (cw->dirMode || cw->sqlMode) {
					undoCompare();
					cw->undoable = false;
				}
			} else {
/* Becomes many lines, this is the tricky case. */
				save_nlMode = cw->nlMode;
				delText(ln, ln);
				addTextToBuffer((pst) replaceString,
						replaceStringLength + 1, ln - 1,
						false);
				cw->nlMode = save_nlMode;
				endRange += linecount;
				ln += linecount;
/* There's a quirk when adding newline to the end of a buffer
 * that had no newline at the end before. */
				if (cw->nlMode && ln == cw->dol
				    && replaceStringEnd[-1] == '\n') {
					delText(ln, ln);
					--ln, --endRange;
				}
			}
		}		/* browse or not */

		if (subPrint == 2)
			displayLine(ln);
		lastSubst = ln;
		nzFree(replaceString);
/* we may have just freed the result of a breakline command */
		breakLineResult = 0;
	}			/* loop over lines in the range */

	if (re_cc)
		pcre_free(re_cc);

	if (intFlag) {
		setError(MSG_Interrupted);
		return -1;
	}

	if (!lastSubst) {
		if (!globSub) {
			if (!errorMsg[0])
				setError(bl_mode ? MSG_NoChange : MSG_NoMatch);
		}
		return false;
	}
	cw->dot = lastSubst;
	if (subPrint == 1 && !globSub)
		printDot();
	return true;

abort:
	if (re_cc)
		pcre_free(re_cc);
	nzFree(replaceString);
/* we may have just freed the result of a breakline command */
	breakLineResult = 0;
	return -1;
}				/* substituteText */

/*********************************************************************
Implement various two letter commands.
Most of these set and clear modes.
Return 1 or 0 for success or failure as usual.
But return 2 if there is a new command to run.
The second parameter is a result parameter, the new command to run.
In rare cases we might allocate a new (longer) command line to run,
like rf (refresh), which could be a long url.
*********************************************************************/

static char *allocatedLine = 0;

static int twoLetter(const char *line, const char **runThis)
{
	static char shortline[60];
	char c;
	bool rc, ub;
	int i, n;

	*runThis = shortline;

	if (stringEqual(line, "qt"))
		ebClose(0);

	if (line[0] == 'd' && line[1] == 'b' && isdigitByte(line[2])
	    && !line[3]) {
		debugLevel = line[2] - '0';
		if (debugLevel >= 4)
			curl_easy_setopt(http_curl_handle, CURLOPT_VERBOSE, 1);
		else
			curl_easy_setopt(http_curl_handle, CURLOPT_VERBOSE, 0);
		return true;
	}

	if (stringEqual(line, "bw")) {
		cw->changeMode = false;
		cw->quitMode = true;
		return true;
	}

	if (stringEqual(line, "rr")) {
		cmd = 'e';
		if (!cw->browseMode) {
			setError(MSG_NoBrowse);
			return false;
		}
		if (!isJSAlive) {
			setError(MSG_JavaOff);
			return false;
		}
		rerender(true);
		return true;
	}

	if (line[0] == 'u' && line[1] == 'a' && isdigitByte(line[2])
	    && !line[3]) {
		char *t = userAgents[line[2] - '0'];
		cmd = 'e';
		if (!t) {
			setError(MSG_NoAgent, line[2]);
			return false;
		}
		currentAgent = t;
		curl_easy_setopt(http_curl_handle, CURLOPT_USERAGENT,
				 currentAgent);
		if (helpMessagesOn || debugLevel >= 1)
			puts(currentAgent);
		return true;
	}

	if (stringEqual(line, "re") || stringEqual(line, "rea")) {
		undoCompare();
		cw->undoable = false;
		cmd = 'e';	/* so error messages are printed */
		rc = setupReply(line[2] == 'a');
		if (rc && cw->browseMode) {
			ub = false;
			cw->browseMode = false;
			goto et_go;
		}
		return rc;
	}

/* ^^^^ is the same as ^4 */
	if (line[0] == '^' && line[1] == '^') {
		const char *t = line + 2;
		while (*t == '^')
			++t;
		if (!*t) {
			sprintf(shortline, "^%ld", t - line);
			return 2;
		}
	}

	if (line[0] == 'l' && line[1] == 's') {
		char lsmode[12];
		bool setmode = false;
		char *file, *path, *t;
		const char *s = line + 2;
		cmd = 'e';	/* so error messages are printed */
		skipWhite(&s);
		if (*s == '=') {
			setmode = true;
			++s;
			skipWhite(&s);
		} else {
			if (!cw->dirMode) {
				setError(MSG_NoDir);
				return false;
			}
			if (cw->dot == 0) {
				setError(MSG_AtLine0);
				return false;
			}
		}
		if (!lsattrChars(s, lsmode)) {
			setError(MSG_LSBadChar);
			return false;
		}
		if (setmode) {
			strcpy(lsformat, lsmode);
			return true;
		}
/* default ls mode is size time */
		if (!lsmode[0])
			strcpy(lsmode, "st");
		file = (char *)fetchLine(cw->dot, -1);
		t = strchr(file, '\n');
		if (!t)
			i_printfExit(MSG_NoNlOnDir, file);
		*t = 0;
		path = makeAbsPath(file);
		*t = '\n';
		if (!path)
			t = emptyString;
		else
			t = lsattr(path, lsmode);
		if (*t)
			puts(t);
		else
			i_puts(MSG_Inaccess);
		return true;
	}

	if (line[0] == 'c' && line[1] == 'd') {
		c = line[2];
		if (!c || isspaceByte(c)) {
			const char *t = line + 2;
			skipWhite(&t);
			c = *t;
			cmd = 'e';	/* so error messages are printed */
			if (!c) {
				char cwdbuf[ABSPATH];
pwd:
				if (!getcwd(cwdbuf, sizeof(cwdbuf))) {
					setError(c ? MSG_CDGetError :
						 MSG_CDSetError);
					return false;
				}
				puts(cwdbuf);
				return true;
			}
			if (!envFile(t, &t))
				return false;
			if (!chdir(t))
				goto pwd;
			setError(MSG_CDInvalid);
			return false;
		}
	}

	if (line[0] == 'p' && line[1] == 'b') {
		rc = playBuffer(line, NULL);
		if (rc == 2)
			goto no_action;
		return rc;
	}

	if (stringEqual(line, "rf")) {
		cmd = 'e';
		if (!cw->fileName) {
			setError(MSG_NoRefresh);
			return false;
		}
		if (cw->browseMode)
			cmd = 'b';
		noStack = true;
		allocatedLine = allocMem(strlen(cw->fileName) + 3);
		sprintf(allocatedLine, "%c %s", cmd, cw->fileName);
		debrowseSuffix(allocatedLine);
		*runThis = allocatedLine;
		return 2;
	}

	if (stringEqual(line, "shc")) {
		if (!cw->sqlMode) {
			setError(MSG_NoDB);
			return false;
		}
		showColumns();
		return true;
	}

	if (stringEqual(line, "shf")) {
		if (!cw->sqlMode) {
			setError(MSG_NoDB);
			return false;
		}
		showForeign();
		return true;
	}

	if (stringEqual(line, "sht")) {
		if (!ebConnect())
			return false;
		return showTables();
	}

	if (stringEqual(line, "jex")) {
		cmd = 'e';
		if (!cw->browseMode) {
			setError(MSG_NoBrowse);
			return false;
		}
		if (!isJSAlive) {
			setError(MSG_JavaOff);
			return false;
		}
		jexmode = true;
		return true;
	}

	if (stringEqual(line, "ub") || stringEqual(line, "et")) {
		ub = (line[0] == 'u');
		rc = true;
		cmd = 'e';
		if (!cw->browseMode) {
			setError(MSG_NoBrowse);
			return false;
		}
		undoCompare();
		cw->undoable = false;
		cw->browseMode = false;
		if (ub) {
			debrowseSuffix(cw->fileName);
			cw->nlMode = cw->rnlMode;
			cw->dot = cw->r_dot, cw->dol = cw->r_dol;
			memcpy(cw->labels, cw->r_labels, sizeof(cw->labels));
			freeWindowLines(cw->map);
			cw->map = cw->r_map;
			cw->r_map = 0;
		} else {
et_go:
			for (i = 1; i <= cw->dol; ++i)
				removeHiddenNumbers(cw->map[i].text, '\n');
			freeWindowLines(cw->r_map);
			cw->r_map = 0;
		}
		freeTags(cw);
		delTimers(cw);
		freeJavaContext(cw);
		cw->jcx = NULL;
		nzFree(cw->dw);
		cw->dw = 0;
		nzFree(cw->ft);
		cw->ft = 0;
		nzFree(cw->hbase);
		cw->hbase = 0;
		nzFree(cw->lastrender);
		cw->lastrender = 0;
		nzFree(cw->fd);
		cw->fd = 0;
		nzFree(cw->fk);
		cw->fk = 0;
		nzFree(cw->mailInfo);
		cw->mailInfo = 0;
		if (ub)
			fileSize = apparentSize(context, false);
		return rc;
	}

	if (stringEqual(line, "f/") || stringEqual(line, "w/")) {
		char *t;
		cmd = line[0];
		if (!cw->fileName) {
			setError(MSG_NoRefresh);
			return false;
		}
		t = strrchr(cw->fileName, '/');
		if (!t) {
			setError(MSG_NoSlash);
			return false;
		}
		++t;
		if (!*t) {
			setError(MSG_YesSlash);
			return false;
		}
		t = getFileURL(cw->fileName, false);
		allocatedLine = allocMem(strlen(t) + 8);
/* ` prevents wildcard expansion, which normally happens on an f command */
		sprintf(allocatedLine, "%c `%s", cmd, t);
		*runThis = allocatedLine;
		return 2;
	}

	if (line[0] == 'f' && line[2] == 0 &&
	    (line[1] == 'd' || line[1] == 'k' || line[1] == 't')) {
		const char *s;
		int t;
		cmd = 'e';
		if (!cw->browseMode) {
			setError(MSG_NoBrowse);
			return false;
		}
		if (line[1] == 't')
			s = cw->ft, t = MSG_NoTitle;
		if (line[1] == 'd')
			s = cw->fd, t = MSG_NoDesc;
		if (line[1] == 'k')
			s = cw->fk, t = MSG_NoKeywords;
		if (s)
			puts(s);
		else
			i_puts(t);
		return true;
	}

	if (line[0] == 's' && line[1] == 'm') {
		const char *t = line + 2;
		bool dosig = true;
		int account = 0;
		cmd = 'e';
		if (*t == '-') {
			dosig = false;
			++t;
		}
		if (isdigitByte(*t))
			account = strtol(t, (char **)&t, 10);
		if (!*t) {
/* send mail */
			return sendMailCurrent(account, dosig);
		} else {
			setError(MSG_SMBadChar);
			return false;
		}
	}

	if (stringEqual(line, "sg")) {
		searchStringsAll = true;
		if (helpMessagesOn)
			i_puts(MSG_SubGlobal);
		return true;
	}

	if (stringEqual(line, "sl")) {
		searchStringsAll = false;
		if (helpMessagesOn)
			i_puts(MSG_SubLocal);
		return true;
	}

	if (stringEqual(line, "ci")) {
		caseInsensitive = true;
		if (helpMessagesOn)
			i_puts(MSG_CaseIns);
		return true;
	}

	if (stringEqual(line, "cs")) {
		caseInsensitive = false;
		if (helpMessagesOn)
			i_puts(MSG_CaseSen);
		return true;
	}

	if (stringEqual(line, "dr")) {
		dirWrite = 0;
		if (helpMessagesOn)
			i_puts(MSG_DirReadonly);
		return true;
	}

	if (stringEqual(line, "dw")) {
		dirWrite = 1;
		if (helpMessagesOn)
			i_puts(MSG_DirWritable);
		return true;
	}

	if (stringEqual(line, "dx")) {
		dirWrite = 2;
		if (helpMessagesOn)
			i_puts(MSG_DirX);
		return true;
	}

	if (stringEqual(line, "hr")) {
		allowRedirection ^= 1;
/* We're doing this manually for now.
        curl_easy_setopt(http_curl_handle, CURLOPT_FOLLOWLOCATION, allowRedirection);
*/
		if (helpMessagesOn || debugLevel >= 1)
			i_puts(allowRedirection + MSG_RedirectionOff);
		return true;
	}

	if (stringEqual(line, "pg")) {
		pluginsOn ^= 1;
		if (helpMessagesOn || debugLevel >= 1)
			i_puts(pluginsOn + MSG_PluginsOff);
		return true;
	}

	if (stringEqual(line, "bg")) {
#ifdef DOSLIKE
		puts("download in background not available on Windows at this time.");
#else
		down_bg ^= 1;
		if (helpMessagesOn || debugLevel >= 1)
			i_puts(down_bg + MSG_DownForeground);
#endif
		return true;
	}

	if (stringEqual(line, "bgl")) {
		bg_jobs(false);
		return true;
	}

	if (stringEqual(line, "iu")) {
		iuConvert ^= 1;
		if (helpMessagesOn || debugLevel >= 1)
			i_puts(iuConvert + MSG_IUConvertOff);
		return true;
	}

	if (stringEqual(line, "sr")) {
		sendReferrer ^= 1;
/* In case of redirect, let libcurl send URL of redirecting page. */
		curl_easy_setopt(http_curl_handle, CURLOPT_AUTOREFERER,
				 sendReferrer);
		if (helpMessagesOn || debugLevel >= 1)
			i_puts(sendReferrer + MSG_RefererOff);
		return true;
	}

	if (stringEqual(line, "js")) {
		allowJS ^= 1;
		if (helpMessagesOn || debugLevel >= 1)
			i_puts(allowJS + MSG_JavaOff);
		return true;
	}

	if (stringEqual(line, "bd")) {
		binaryDetect ^= 1;
		if (helpMessagesOn || debugLevel >= 1)
			i_puts(binaryDetect + MSG_BinaryIgnore);
		return true;
	}

	if (stringEqual(line, "rl")) {
		inputReadLine ^= 1;
		if (helpMessagesOn || debugLevel >= 1)
			i_puts(inputReadLine + MSG_InputTTY);
		return true;
	}

	if (stringEqual(line, "lna")) {
		listNA ^= 1;
		if (helpMessagesOn || debugLevel >= 1)
			i_puts(listNA + MSG_ListControl);
		return true;
	}

	if (line[0] == 'f' && line[1] == 'm' &&
	    line[2] && strchr("pa", line[2]) && !line[3]) {
		bool doHelp = helpMessagesOn || debugLevel >= 1;
/* Can't support passive/active mode with libcurl, or at least not easily. */
		if (line[2] == 'p') {
			curl_easy_setopt(http_curl_handle, CURLOPT_FTPPORT,
					 NULL);
			if (doHelp)
				i_puts(MSG_PassiveMode);
		} else {
			curl_easy_setopt(http_curl_handle, CURLOPT_FTPPORT,
					 "-");
			if (doHelp)
				i_puts(MSG_ActiveMode);
		}
/* See "man curl_easy_setopt.3" for info on CURLOPT_FTPPORT.  Supplying
* "-" makes libcurl select the best IP address for active ftp.
*/
		return true;
	}

	if (stringEqual(line, "vs")) {
		verifyCertificates ^= 1;
		if (helpMessagesOn || debugLevel >= 1)
			i_puts(verifyCertificates + MSG_CertifyOff);
		return true;
	}

	if (stringEqual(line, "hf")) {
		showHiddenFiles ^= 1;
		if (helpMessagesOn || debugLevel >= 1)
			i_puts(showHiddenFiles + MSG_HiddenOff);
		return true;
	}

	if (stringEqual(line, "su8")) {
		re_utf8 ^= 1;
		if (helpMessagesOn || debugLevel >= 1)
			i_puts(re_utf8 + MSG_ReAscii);
		return true;
	}

	if (!strncmp(line, "ds=", 3)) {
		if (!line[3]) {
			if (!dbarea || !*dbarea) {
				i_puts(MSG_DBNoSource);
			} else {
				printf("%s", dbarea);
				if (dblogin)
					printf(",%s", dblogin);
				if (dbpw)
					printf(",%s", dbpw);
				nl();
			}
			return true;
		}
		dbClose();
		setDataSource(cloneString(line + 3));
		return true;
	}

	if (stringEqual(line, "fbc")) {
		fetchBlobColumns ^= 1;
		if (helpMessagesOn || debugLevel >= 1)
			i_puts(MSG_FetchBlobOff + fetchBlobColumns);
		return true;
	}

	if (stringEqual(line, "eo")) {
		endMarks = 0;
		if (helpMessagesOn)
			i_puts(MSG_MarkOff);
		return true;
	}

	if (stringEqual(line, "el")) {
		endMarks = 1;
		if (helpMessagesOn)
			i_puts(MSG_MarkList);
		return true;
	}

	if (stringEqual(line, "ep")) {
		endMarks = 2;
		if (helpMessagesOn)
			i_puts(MSG_MarkOn);
		return true;
	}

no_action:
	*runThis = line;
	return 2;		/* no change */
}				/* twoLetter */

/* Return the number of unbalanced punctuation marks.
 * This is used by the next routine. */
static void unbalanced(char c, char d, int ln, int *back_p, int *for_p)
{				/* result parameters */
	char *t, *open;
	char *p = (char *)fetchLine(ln, 1);
	bool change;
	int backward, forward;

	change = true;
	while (change) {
		change = false;
		open = 0;
		for (t = p; *t != '\n'; ++t) {
			if (*t == c)
				open = t;
			if (*t == d && open) {
				*open = 0;
				*t = 0;
				change = true;
				open = 0;
			}
		}
	}

	backward = forward = 0;
	for (t = p; *t != '\n'; ++t) {
		if (*t == c)
			++forward;
		if (*t == d)
			++backward;
	}

	free(p);
	*back_p = backward;
	*for_p = forward;
}				/* unbalanced */

/* Find the line that balances the unbalanced punctuation. */
static bool balanceLine(const char *line)
{
	char c, d;		/* open and close */
	char selected;
	static char openlist[] = "{([<`";
	static char closelist[] = "})]>'";
	static const char alllist[] = "{}()[]<>`'";
	char *t;
	int level = 0;
	int i, direction, forward, backward;

	if (c = *line) {
		if (!strchr(alllist, c) || line[1]) {
			setError(MSG_BalanceChar, alllist);
			return false;
		}
		if (t = strchr(openlist, c)) {
			d = closelist[t - openlist];
			direction = 1;
		} else {
			d = c;
			t = strchr(closelist, d);
			c = openlist[t - closelist];
			direction = -1;
		}
		unbalanced(c, d, endRange, &backward, &forward);
		if (direction > 0) {
			if ((level = forward) == 0) {
				setError(MSG_BalanceNoOpen, c);
				return false;
			}
		} else {
			if ((level = backward) == 0) {
				setError(MSG_BalanceNoOpen, d);
				return false;
			}
		}
	} else {

/* Look for anything unbalanced, probably a brace. */
		for (i = 0; i <= 2; ++i) {
			c = openlist[i];
			d = closelist[i];
			unbalanced(c, d, endRange, &backward, &forward);
			if (backward && forward) {
				setError(MSG_BalanceAmbig, c, d, c, d);
				return false;
			}
			level = backward + forward;
			if (!level)
				continue;
			direction = 1;
			if (backward)
				direction = -1;
			break;
		}
		if (!level) {
			setError(MSG_BalanceNothing);
			return false;
		}
	}			/* explicit character passed in, or look for one */

	selected = (direction > 0 ? c : d);

/* search for the balancing line */
	i = endRange;
	while ((i += direction) > 0 && i <= cw->dol) {
		unbalanced(c, d, i, &backward, &forward);
		if (direction > 0 && backward >= level ||
		    direction < 0 && forward >= level) {
			cw->dot = i;
			printDot();
			return true;
		}
		level += (forward - backward) * direction;
	}			/* loop over lines */

	setError(MSG_Unbalanced, selected);
	return false;
}				/* balanceLine */

/* Unfold the buffer into one long, allocated string. */
bool unfoldBufferW(const struct ebWindow * w, bool cr, char **data, int *len)
{
	char *buf;
	int l, ln;
	int size = apparentSizeW(w, false);
	if (size < 0)
		return false;
	if (w->dirMode) {
		setError(MSG_SessionDir, context);
		return false;
	}
	if (cr)
		size += w->dol;
/* a few bytes more, just for safety */
	buf = allocMem(size + 4);
	*data = buf;
	for (ln = 1; ln <= w->dol; ++ln) {
		pst line = w->map[ln].text;
		l = pstLength(line) - 1;
		if (l) {
			memcpy(buf, line, l);
			buf += l;
		}
		if (cr) {
			*buf++ = '\r';
			if (l && buf[-2] == '\r')
				--buf, --size;
		}
		*buf++ = '\n';
	}			/* loop over lines */
	if (w->dol && w->nlMode) {
		if (cr)
			--size;
	}
	*len = size;
	(*data)[size] = 0;
	return true;
}				/* unfoldBufferW */

bool unfoldBuffer(int cx, bool cr, char **data, int *len)
{
	const struct ebWindow *w = sessionList[cx].lw;
	return unfoldBufferW(w, cr, data, len);
}				/* unfoldBuffer */

static char *showLinks(void)
{
	int a_l;
	char *a = initString(&a_l);
	bool click, dclick;
	char c, *p, *s, *t, *q, *line, *h, *h2;
	int j, k = 0, tagno;
	const struct htmlTag *tag;

	if (cw->browseMode && endRange) {
		line = (char *)fetchLine(endRange, -1);
		for (p = line; (c = *p) != '\n'; ++p) {
			if (c != InternalCodeChar)
				continue;
			if (!isdigitByte(p[1]))
				continue;
			j = strtol(p + 1, &s, 10);
			if (*s != '{')
				continue;
			p = s;
			++k;
			findField(line, 0, k, 0, 0, &tagno, &h, &tag);
			if (tagno != j)
				continue;	/* should never happen */

			click = tagHandler(tagno, "onclick");
			dclick = tagHandler(tagno, "ondblclick");

/* find the closing brace */
/* It might not be there, could be on the next line. */
			for (s = p + 1; (c = *s) != '\n'; ++s)
				if (c == InternalCodeChar && s[1] == '0'
				    && s[2] == '}')
					break;
/* Ok, everything between p and s exclusive is the description */
			if (!h)
				h = emptyString;
			if (stringEqual(h, "#")) {
				nzFree(h);
				h = emptyString;
			}

			if (memEqualCI(h, "mailto:", 7)) {
				stringAndBytes(&a, &a_l, p + 1, s - p - 1);
				stringAndChar(&a, &a_l, ':');
				s = h + 7;
				t = s + strcspn(s, "?");
				stringAndBytes(&a, &a_l, s, t - s);
				stringAndChar(&a, &a_l, '\n');
				nzFree(h);
				continue;
			}
			/* mail link */
			stringAndString(&a, &a_l, "<br><a href=");

			if (memEqualCI(h, "javascript:", 11)) {
				stringAndString(&a, &a_l, "javascript:>\n");
			} else if (!*h && (click | dclick)) {
				char buf[20];
				sprintf(buf, "%s>\n",
					click ? "onclick" : "ondblclick");
				stringAndString(&a, &a_l, buf);
			} else {
				if (*h) {
					h2 = htmlEscape(h);
					stringAndString(&a, &a_l, h2);
					nzFree(h2);
				}
				stringAndString(&a, &a_l, ">\n");
			}

			nzFree(h);
/* next line is the description of the bookmark */
			h = pullString(p + 1, s - p - 1);
			h2 = htmlEscape(h);
			stringAndString(&a, &a_l, h2);
			stringAndString(&a, &a_l, "\n</a>\n");
			nzFree(h);
			nzFree(h2);
		}		/* loop looking for hyperlinks */
	}

	if (!a_l) {		/* nothing found yet */
		if (!cw->fileName) {
			setError(MSG_NoFileName);
			return 0;
		}

		h = htmlEscape(cw->fileName);
		debrowseSuffix(h);
		stringAndString(&a, &a_l, "<br><a href=");
		stringAndString(&a, &a_l, h);
		stringAndString(&a, &a_l, ">\n");
/* get text from the html title if you can */
		s = cw->ft;
		if (s && *s) {
			h2 = htmlEscape(s);
			stringAndString(&a, &a_l, h2);
			nzFree(h2);
		} else {
/* no title - getting the text from the url, very kludgy */
			s = (char *)getDataURL(h);
			if (!s || !*s)
				s = h;
			t = findHash(s);
			if (t)
				*t = 0;
			t = s + strcspn(s, "\1?");
			if (t > s && t[-1] == '/')
				--t;
			*t = 0;
			q = strrchr(s, '/');
			if (q && q < t)
				s = q + 1;
			stringAndBytes(&a, &a_l, s, t - s);
		}
		stringAndString(&a, &a_l, "\n</a>\n");
		nzFree(h);
	}

	removeHiddenNumbers(a, 0);
	return a;
}				/* showLinks */

static bool lineHasTag(const char *p, const char *s)
{
	const struct htmlTag *t;
	char c;
	int j;

	while ((c = *p++) != '\n') {
		if (c != InternalCodeChar)
			continue;
		j = strtol(p, (char **)&p, 10);
		t = tagList[j];
		if (t->id && stringEqual(t->id, s))
			return true;
		if (t->action == TAGACT_A && t->name && stringEqual(t->name, s))
			return true;
	}

	return false;
}				/* lineHasTag */

/*********************************************************************
Run the entered edbrowse command.
This is indirectly recursive, as in g/x/d
Pass in the ed command, and return success or failure.
We assume it has been turned into a C string.
This means no embeded nulls.
If you want to use null in a search or substitute, use \0.
*********************************************************************/

bool runCommand(const char *line)
{
	int i, j, n;
	int writeMode = O_TRUNC;
	struct ebWindow *w = NULL;
	const struct htmlTag *tag = NULL;	/* event variables */
	bool nogo = true, rc = true;
	bool postSpace = false, didRange = false;
	char first;
	int cx = 0;		/* numeric suffix as in s/x/y/3 or w2 */
	int tagno;
	const char *s = NULL;
	static char newline[MAXTTYLINE];

	if (allocatedLine) {
		nzFree(allocatedLine);
		allocatedLine = 0;
	}
	nzFree(currentReferrer);
	currentReferrer = cloneString(cw->fileName);
	js_redirects = false;

	cmd = icmd = 'p';
	skipWhite(&line);
	first = *line;

	if (!globSub) {
		madeChanges = false;

/* Allow things like comment, or shell escape, but not if we're
 * in the midst of a global substitute, as in g/x/ !echo hello world */
		if (first == '#')
			return true;

		if (first == '!')
			return shellEscape(line + 1);

/* Watch for successive q commands. */
		lastq = lastqq, lastqq = 0;
		noStack = false;

/* special 2 letter commands - most of these change operational modes */
		j = twoLetter(line, &line);
		if (j != 2)
			return j;
	}

	startRange = endRange = cw->dot;	/* default range */
/* Just hit return to read the next line. */
	first = *line;
	if (first == 0) {
		didRange = true;
		++startRange, ++endRange;
		if (endRange > cw->dol) {
			setError(MSG_EndBuffer);
			return false;
		}
	}

	if (first == ',') {
		didRange = true;
		++line;
		startRange = 1;
		if (cw->dol == 0)
			startRange = 0;
		endRange = cw->dol;
	}

	if (first == ';') {
		didRange = true;
		++line;
		startRange = cw->dot;
		endRange = cw->dol;
	}

	if (first == 'j' || first == 'J') {
		didRange = true;
		endRange = startRange + 1;
		if (endRange > cw->dol) {
			setError(MSG_EndJoin);
			return false;
		}
	}

	if (first == '=') {
		didRange = true;
		startRange = endRange = cw->dol;
	}

	if (first == 'w' || first == 'v' || first == 'g' &&
	    line[1] && strchr(valid_delim, line[1])) {
		didRange = true;
		startRange = 1;
		if (cw->dol == 0)
			startRange = 0;
		endRange = cw->dol;
	}

	if (!didRange) {
		if (!getRangePart(line, &startRange, &line))
			return (globSub = false);
		endRange = startRange;
		if (line[0] == ',') {
			++line;
			endRange = cw->dol;	/* new default */
			first = *line;
			if (first && strchr(valid_laddr, first)) {
				if (!getRangePart(line, &endRange, &line))
					return (globSub = false);
			}
		}
	}
	if (endRange < startRange) {
		setError(MSG_BadRange);
		return false;
	}

/* change uc into a substitute command, converting the whole line */
	skipWhite(&line);
	first = *line;
	if ((first == 'u' || first == 'l' || first == 'm') && line[1] == 'c' &&
	    line[2] == 0) {
		sprintf(newline, "s/.*/%cc/", first);
		line = newline;
	}

/* Breakline is actually a substitution of lines. */
	if (stringEqual(line, "bl")) {
		if (cw->dirMode) {
			setError(MSG_BreakDir);
			return false;
		}
		if (cw->sqlMode) {
			setError(MSG_BreakDB);
			return false;
		}
		if (cw->browseMode) {
			setError(MSG_BreakBrowse);
			return false;
		}
		line = "s`bl";
	}

/* get the command */
	cmd = *line;
	if (cmd)
		++line;
	else
		cmd = 'p';
	icmd = cmd;

	if (!strchr(valid_cmd, cmd)) {
		setError(MSG_UnknownCommand, cmd);
		return (globSub = false);
	}

	first = *line;
	if (cmd == 'w' && first == '+')
		writeMode = O_APPEND, first = *++line;

	if (cw->dirMode && !strchr(dir_cmd, cmd)) {
		setError(MSG_DirCommand, icmd);
		return (globSub = false);
	}

	if (cw->sqlMode && !strchr(sql_cmd, cmd)) {
		setError(MSG_DBCommand, icmd);
		return (globSub = false);
	}

	if (cw->browseMode && !strchr(browse_cmd, cmd)) {
		setError(MSG_BrowseCommand, icmd);
		return (globSub = false);
	}

	if (startRange == 0 && !strchr(zero_cmd, cmd)) {
		setError(MSG_AtLine0);
		return (globSub = false);
	}

	while (isspaceByte(first))
		postSpace = true, first = *++line;

	if (strchr(spaceplus_cmd, cmd) && !postSpace && first) {
		s = line;
		while (isdigitByte(*s))
			++s;
		if (*s) {
			setError(MSG_NoSpaceAfter);
			return (globSub = false);
		}
	}

	if (globSub && !strchr(global_cmd, cmd)) {
		setError(MSG_GlobalCommand, icmd);
		return (globSub = false);
	}

/* move/copy destination, the third address */
	if (cmd == 't' || cmd == 'm') {
		if (!first) {
			destLine = cw->dot;
		} else {
			if (!strchr(valid_laddr, first)) {
				setError(MSG_BadDest);
				return (globSub = false);
			}
			if (!getRangePart(line, &destLine, &line))
				return (globSub = false);
			first = *line;
		}		/* was there something after m or t */
	}

/* env variable and wild card expansion */
	if (strchr("brewf", cmd) && first && !isURL(line) && !isSQL(line)) {
		if (cmd != 'r' || !cw->sqlMode) {
			if (!envFile(line, &line))
				return false;
			first = *line;
		}
	}

	if (cmd == 'z') {
		if (isdigitByte(first)) {
			last_z = strtol(line, (char **)&line, 10);
			if (!last_z)
				last_z = 1;
			first = *line;
		}
		startRange = endRange + 1;
		endRange = startRange;
		if (startRange > cw->dol) {
			startRange = endRange = 0;
			setError(MSG_LineHigh);
			return false;
		}
		cmd = 'p';
		endRange += last_z - 1;
		if (endRange > cw->dol)
			endRange = cw->dol;
	}

	/* the a+ feature, when you thought you were in append mode */
	if (cmd == 'a') {
		if (stringEqual(line, "+"))
			++line, first = 0;
		else
			linePending[0] = 0;
	} else
		linePending[0] = 0;

	if (first && strchr(nofollow_cmd, cmd)) {
		setError(MSG_TextAfter, icmd);
		return (globSub = false);
	}

	if (cmd == 'h') {
		showError();
		return true;
	}

	if (cmd == 'H') {
		if (helpMessagesOn ^= 1)
			if (debugLevel >= 1)
				i_puts(MSG_HelpOn);
		return true;
	}

	if (cmd == 'X') {
		cw->dot = endRange;
		return true;
	}

	if (strchr("Llpn", cmd)) {
		for (i = startRange; i <= endRange; ++i) {
			displayLine(i);
			cw->dot = i;
			if (intFlag)
				break;
		}
		return true;
	}

	if (cmd == '=') {
		printf("%d\n", endRange);
		return true;
	}

	if (cmd == 'B') {
		return balanceLine(line);
	}

	if (cmd == 'u') {
		struct ebWindow *uw = &undoWindow;
		struct lineMap *swapmap;
		if (!cw->undoable) {
			setError(MSG_NoUndo);
			return false;
		}
/* swap, so we can undo our undo, if need be */
		i = uw->dot, uw->dot = cw->dot, cw->dot = i;
		i = uw->dol, uw->dol = cw->dol, cw->dol = i;
		for (j = 0; j < 26; ++j) {
			i = uw->labels[j], uw->labels[j] =
			    cw->labels[j], cw->labels[j] = i;
		}
		swapmap = uw->map, uw->map = cw->map, cw->map = swapmap;
		return true;
	}

	if (cmd == 'k') {
		if (!islowerByte(first) || line[1]) {
			setError(MSG_EnterKAZ);
			return false;
		}
		if (startRange < endRange) {
			setError(MSG_RangeLabel);
			return false;
		}
		cw->labels[first - 'a'] = endRange;
		return true;
	}

	/* Find suffix, as in 27,59w2 */
	if (!postSpace) {
		cx = stringIsNum(line);
		if (!cx) {
			setError((cmd == '^') ? MSG_Backup0 : MSG_Session0);
			return false;
		}
		if (cx < 0)
			cx = 0;
	}

	if (cmd == 'q') {
		if (cx) {
			if (!cxCompare(cx))
				return false;
			if (!cxActive(cx))
				return false;
		} else {
			cx = context;
			if (first) {
				setError(MSG_QAfter);
				return false;
			}
		}
		saveSubstitutionStrings();
		if (!cxQuit(cx, 2))
			return false;
		if (cx != context)
			return true;
/* look around for another active session */
		while (true) {
			if (++cx == MAXSESSION)
				cx = 1;
			if (cx == context)
				ebClose(0);
			if (!sessionList[cx].lw)
				continue;
			cxSwitch(cx, true);
			return true;
		}		/* loop over sessions */
	}

	if (cmd == 'f') {
		if (cx) {
			if (!cxCompare(cx))
				return false;
			if (!cxActive(cx))
				return false;
			s = sessionList[cx].lw->fileName;
			if (s)
				printf("%s", s);
			else
				i_printf(MSG_NoFile);
			if (sessionList[cx].lw->binMode)
				i_printf(MSG_BinaryBrackets);
			nl();
			return true;
		}		/* another session */
		if (first) {
			if (cw->dirMode) {
				setError(MSG_DirRename);
				return false;
			}
			if (cw->sqlMode) {
				setError(MSG_TableRename);
				return false;
			}
			nzFree(cw->fileName);
			cw->fileName = cloneString(line);
		}
		s = cw->fileName;
		if (s)
			printf("%s", s);
		else
			i_printf(MSG_NoFile);
		if (cw->binMode)
			i_printf(MSG_BinaryBrackets);
		nl();
		return true;
	}

	if (cmd == 'w') {
		if (cx) {	/* write to another buffer */
			if (writeMode == O_APPEND) {
				setError(MSG_BufferAppend);
				return false;
			}
			return writeContext(cx);
		}
		if (!first)
			line = cw->fileName;
		if (!line) {
			setError(MSG_NoFileSpecified);
			return false;
		}
		if (cw->dirMode && stringEqual(line, cw->fileName)) {
			setError(MSG_NoDirWrite);
			return false;
		}
		if (cw->sqlMode && stringEqual(line, cw->fileName)) {
			setError(MSG_NoDBWrite);
			return false;
		}
		return writeFile(line, writeMode);
	}

	if (cmd == '^') {	/* back key, pop the stack */
		if (first && !cx) {
			setError(MSG_ArrowAfter);
			return false;
		}
		if (!cx)
			cx = 1;
		while (cx) {
			struct ebWindow *prev = cw->prev;
			if (!prev) {
				setError(MSG_NoPrevious);
				return false;
			}
			saveSubstitutionStrings();
			if (!cxQuit(context, 1))
				return false;
			sessionList[context].lw = cw = prev;
			restoreSubstitutionStrings(cw);
			--cx;
		}
		printDot();
		return true;
	}

	if (cmd == 'M') {	/* move this to another session */
		if (first && !cx) {
			setError(MSG_MAfter);
			return false;
		}
		if (!first) {
			setError(MSG_NoDestSession);
			return false;
		}
		if (!cw->prev) {
			setError(MSG_NoBackup);
			return false;
		}
		if (!cxCompare(cx))
			return false;
		if (cxActive(cx) && !cxQuit(cx, 2))
			return false;
/* If changes were made to this buffer, they are undoable after the move */
		undoCompare();
		cw->undoable = false;
/* Magic with pointers, hang on to your hat. */
		sessionList[cx].fw = sessionList[cx].lw = cw;
		cs->lw = cw->prev;
		cw->prev = 0;
		cw = cs->lw;
		printDot();
		return true;
	}

	if (cmd == 'A') {
		char *a;
		if (!cxQuit(context, 0))
			return false;
		if (!(a = showLinks()))
			return false;
		undoCompare();
		cw->undoable = cw->changeMode = false;
		w = createWindow();
		w->prev = cw;
		cw = w;
		cs->lw = w;
		rc = addTextToBuffer((pst) a, strlen(a), 0, false);
		nzFree(a);
		cw->changeMode = false;
		fileSize = apparentSize(context, false);
		return rc;
	}

	if (cmd == '<') {	/* run a function */
		return runEbFunction(line);
	}

	/* go to a file in a directory listing */
	if (cmd == 'g' && cw->dirMode && !first) {
		char *p, *dirline, *endline;
		const struct MIMETYPE *gmt;	/* the go mime type */
		if (endRange > startRange) {
			setError(MSG_RangeG);
			return false;
		}
		p = (char *)fetchLine(endRange, -1);
		j = pstLength((pst) p);
		--j;
		p[j] = 0;	/* temporary */
		dirline = makeAbsPath(p);
		p[j] = '\n';
		cmd = 'e';
		if (!dirline)
			return false;
		gmt = findMimeByFile(dirline);
		if (pluginsOn && gmt) {
			if (gmt->outtype)
				cmd = 'b';
			else if (!gmt->stream)
				return playBuffer("pb", dirline);
		}
/* I don't think we need to make a copy here. */
		line = dirline;
		first = *line;
	}

	if (cmd == 'e') {
		if (cx) {
			if (!cxCompare(cx))
				return false;
			cxSwitch(cx, true);
			return true;
		}
		if (!first) {
			i_printf(MSG_SessionX, context);
			return true;
		}
/* more e to come */
	}

	/* see if it's a go command */
	if (cmd == 'g' && !(cw->sqlMode | cw->binMode)) {
		char *p, *h;
		int tagno;
		bool click, dclick, over;
		bool jsh, jsgo, jsdead;

		/* Check to see if g means run an sql command. */
		if (!first) {
			char *rbuf;
			j = goSelect(&startRange, &rbuf);
			if (j >= 0) {
				cmd = 'e';	/* for autoprint of errors */
				cw->dot = startRange;
				if (*rbuf) {
					int savedol = cw->dol;
					addTextToBuffer((pst) rbuf,
							strlen(rbuf), cw->dot,
							false);
					nzFree(rbuf);
					if (cw->dol > savedol) {
						cw->labels[0] = startRange + 1;
						cw->labels[1] =
						    startRange + cw->dol -
						    savedol;
					}
					cw->dot = startRange;
				}
				return j ? true : false;
			}
		}

/* Now try to go to a hyperlink */
		s = line;
		j = 0;
		if (first) {
			if (isdigitByte(first))
				j = strtol(s, (char **)&s, 10);
			else if (first == '$')
				j = -1, ++s;
		}
		if (!*s) {
			if (cw->sqlMode) {
				setError(MSG_DBG);
				return false;
			}
			jsh = jsgo = nogo = false;
			jsdead = !isJSAlive;
			click = dclick = over = false;
			cmd = 'b';
			if (endRange > startRange) {
				setError(MSG_RangeG);
				return false;
			}
			p = (char *)fetchLine(endRange, -1);
			findField(p, 0, j, &n, 0, &tagno, &h, &tag);
			debugPrint(5, "findField returns %d, %s", tagno, h);
			if (!h) {
				fieldNumProblem(1, 'g', j, n, n);
				return false;
			}
			jsh = memEqualCI(h, "javascript:", 11);
			if (tagno) {
				over = tagHandler(tagno, "onmouseover");
				click = tagHandler(tagno, "onclick");
				dclick = tagHandler(tagno, "ondblclick");
			}
			if (click)
				jsgo = true;
			jsgo |= jsh;
			nogo = stringEqual(h, "#");
			nogo |= jsh;
			debugPrint(5, "go %d nogo %d jsh %d dead %d", jsgo,
				   nogo, jsh, jsdead);
			debugPrint(5, "click %d dclick %d over %d", click,
				   dclick, over);
			if (jsgo & jsdead) {
				if (nogo)
					i_puts(MSG_NJNoAction);
				else
					i_puts(MSG_NJGoing);
				jsgo = jsh = false;
			}
			line = allocatedLine = h;
			first = *line;
			setError(-1);
			rc = false;
			if (jsgo) {
/* javascript might update fields */
/* The program might depend on the mouseover code running first */
				if (over) {
					jSyncup();
					rc = handlerGoBrowse(tag,
							     "onmouseover");
					jSideEffects();
					if (newlocation)
						goto redirect;
				}
			}
/* This is the only handler where false tells the browser to do something else. */
			if (!rc && !jsdead)
				set_property_string(cw->winobj, "status", h);
			if (jsgo && click) {
				jSyncup();
				rc = handlerGoBrowse(tag, "onclick");
				jSideEffects();
				if (newlocation)
					goto redirect;
				if (!rc)
					return true;
			}
			if (jsh) {
				jSyncup();
				jsRunScript(cw->winobj, h, 0, 0);
				jSideEffects();
				if (newlocation)
					goto redirect;
				return true;
			}
			if (nogo)
				return true;
		}
	}

	if (cmd == 's') {
/* Some shorthand, like s,2 to split the line at the second comma */
		if (!first) {
			strcpy(newline, "//%");
			line = newline;
		} else if (strchr(",.;:!?)-\"", first) &&
			   (!line[1] || isdigitByte(line[1]) && !line[2])) {
			char esc[2];
			esc[0] = esc[1] = 0;
			if (first == '.' || first == '?')
				esc[0] = '\\';
			sprintf(newline, "/%s%c +/%c\\n%s%s",
				esc, first, first, (line[1] ? "/" : ""),
				line + 1);
			debugPrint(7, "shorthand regexp %s", newline);
			line = newline;
		}
		first = *line;
	}

	scmd = ' ';
	if ((cmd == 'i' || cmd == 's') && first) {
		char c;
		s = line;
		if (isdigitByte(*s))
			cx = strtol(s, (char **)&s, 10);
		else if (*s == '$')
			cx = -1, ++s;
		c = *s;
		if (c
		    && (strchr(valid_delim, c) || cmd == 'i'
			&& strchr("*<?=", c))) {
			if (!cw->browseMode && (cmd == 'i' || cx)) {
				setError(MSG_NoBrowse);
				return false;
			}
			if (endRange > startRange && cmd == 'i') {
				setError(MSG_RangeI, c);
				return false;
			}

			if (cmd == 'i' && strchr("?=<*", c)) {
				char *p;
				int realtotal;
				scmd = c;
				line = s + 1;
				first = *line;
				debugPrint(5, "scmd = %c", scmd);
				cw->dot = endRange;
				p = (char *)fetchLine(cw->dot, -1);
				j = 1;
				if (scmd == '*')
					j = 2;
				if (scmd == '?')
					j = 3;
				findInputField(p, j, cx, &n, &realtotal,
					       &tagno);
				debugPrint(5, "findField returns %d.%d", n,
					   tagno);
				if (!tagno) {
					fieldNumProblem((c == '*' ? 2 : 0), 'i',
							cx, n, realtotal);
					return false;
				}

				if (scmd == '?') {
					infShow(tagno, line);
					return true;
				}

				cw->undoable = false;

				if (c == '<') {
					bool fromfile = false;
					if (globSub) {
						setError(MSG_IG);
						return (globSub = false);
					}
					skipWhite(&line);
					if (!*line) {
						setError(MSG_NoFileSpecified);
						return false;
					}
					n = stringIsNum(line);
					if (n >= 0) {
						char *p;
						int plen, dol;
						if (!cxCompare(n)
						    || !cxActive(n))
							return false;
						dol = sessionList[n].lw->dol;
						if (!dol) {
							setError
							    (MSG_BufferXEmpty,
							     n);
							return false;
						}
						if (dol > 1) {
							setError
							    (MSG_BufferXLines,
							     n);
							return false;
						}
						p = (char *)fetchLineContext(1,
									     1,
									     n);
						plen = pstLength((pst) p);
						if (plen > sizeof(newline))
							plen = sizeof(newline);
						memcpy(newline, p, plen);
						n = plen;
						nzFree(p);
					} else {
						int fd;
						fromfile = true;
						if (!envFile(line, &line))
							return false;
						fd = open(line,
							  O_RDONLY | O_TEXT);
						if (fd < 0) {
							setError(MSG_NoOpen,
								 line);
							return false;
						}
						n = read(fd, newline,
							 sizeof(newline));
						close(fd);
						if (n < 0) {
							setError(MSG_NoRead,
								 line);
							return false;
						}
					}
					for (j = 0; j < n; ++j) {
						if (newline[j] == 0) {
							setError(MSG_InputNull,
								 line);
							return false;
						}
						if (newline[j] == '\r'
						    && !fromfile && j < n - 1
						    && newline[j + 1] != '\n') {
							setError(MSG_InputCR);
							return false;
						}
						if (newline[j] == '\r'
						    || newline[j] == '\n')
							break;
					}
					if (j == sizeof(newline)) {
						setError(MSG_FirstLineLong,
							 line);
						return false;
					}
					newline[j] = 0;
					prepareForField(newline);
					line = newline;
					scmd = '=';
				}

				if (scmd == '=') {
					rc = infReplace(tagno, line, true);
					if (newlocation)
						goto redirect;
					return rc;
				}

				if (c == '*') {
					jSyncup();
					if (!infPush(tagno, &allocatedLine))
						return false;
					if (newlocation)
						goto redirect;
/* No url means it was a reset button */
					if (!allocatedLine)
						return true;
					line = allocatedLine;
					first = *line;
					cmd = 'b';
				}

			} else
				cmd = 's';
		} else {
			setError(MSG_TextAfter, icmd);
			return false;
		}
	}

rebrowse:
	if (cmd == 'e' || cmd == 'b' && first && first != '#') {
		if (cw->fileName && !noStack && sameURL(line, cw->fileName)) {
			if (stringEqual(line, cw->fileName)) {
				setError(MSG_AlreadyInBuffer);
				return false;
			}
/* Same url, but a different #section */
			s = findHash(line);
			if (!s) {	/* no section specified */
				cw->dot = 1;
				if (!cw->dol)
					cw->dot = 0;
				printDot();
				return true;
			}
			line = s;
			first = '#';
			goto browse;
		}

/* Different URL, go get it. */
/* did you make changes that you didn't write? */
		if (!cxQuit(context, 0))
			return false;
		undoCompare();
		cw->undoable = cw->changeMode = false;
		startRange = endRange = 0;
		changeFileName = 0;	/* should already be zero */
		w = createWindow();
		cw = w;		/* we might wind up putting this back */
/* Check for sendmail link */
		if (cmd == 'b' && memEqualCI(line, "mailto:", 7)) {
			char *addr, *subj, *body;
			char *q;
			int ql;
			decodeMailURL(line, &addr, &subj, &body);
			ql = strlen(addr);
			ql += 4;	/* to:\n */
			ql += subj ? strlen(subj) : 5;
			ql += 9;	/* subject:\n */
			if (body)
				ql += strlen(body);
			q = allocMem(ql + 1);
			sprintf(q, "to:%s\nSubject:%s\n%s", addr,
				subj ? subj : "Hello", body ? body : "");
			j = addTextToBuffer((pst) q, ql, 0, false);
			nzFree(q);
			nzFree(addr);
			nzFree(subj);
			nzFree(body);
			if (j)
				i_puts(MSG_MailHowto);
		} else {
			cw->fileName = cloneString(line);
			cw->firstURL = cloneString(line);
			if (isSQL(line))
				cw->sqlMode = true;
			if (icmd == 'g' && !nogo && isURL(line))
				debugPrint(2, "*%s", line);
			j = readFile(line, emptyString);
		}
		w->undoable = w->changeMode = false;
		cw = cs->lw;
/* Don't push a new session if we were trying to read a url,
 * and didn't get anything. */
		if (!serverData && (isURL(line) || isSQL(line))) {
			fileSize = -1;
			freeWindow(w);
			if (noStack && cw->prev) {
				w = cw;
				cw = w->prev;
				cs->lw = cw;
				freeWindow(w);
			}
			return j;
		}
		if (noStack) {
			w->prev = cw->prev;
			nzFree(w->firstURL);
			w->firstURL = cw->firstURL;
			cw->firstURL = 0;
			cxQuit(context, 1);
		} else {
			w->prev = cw;
		}
		cs->lw = cw = w;
		if (!w->prev)
			cs->fw = w;
		if (!j)
			return false;
		if (changeFileName) {
			nzFree(w->fileName);
			w->fileName = changeFileName;
			changeFileName = 0;
		}
/* Some files we just can't browse */
		if (!cw->dol || cw->dirMode)
			cmd = 'e';
		if (cw->binMode && (!cw->mt || !cw->mt->outtype))
			cmd = 'e';
		if (cmd == 'e')
			return true;
	}

browse:
	if (cmd == 'b') {
		if (!cw->browseMode) {
			if (cw->binMode && (!cw->mt || !cw->mt->outtype)) {
				setError(MSG_BrowseBinary);
				return false;
			}
			if (!cw->dol) {
				setError(MSG_BrowseEmpty);
				return false;
			}
			if (fileSize >= 0) {
				debugPrint(1, "%d", fileSize);
				fileSize = -1;
			}
			if (!browseCurrentBuffer()) {
				if (icmd == 'b')
					return false;
				return true;
			}
		} else if (!first) {
			setError(MSG_BrowseAlready);
			return false;
		}

		if (newlocation) {
			if (!refreshDelay(newloc_d, newlocation)) {
				nzFree(newlocation);
				newlocation = 0;
			} else {
redirect:
				noStack = newloc_rf;
				nzFree(allocatedLine);
				line = allocatedLine = newlocation;
				debugPrint(2, "redirect %s", line);
				newlocation = 0;
				icmd = cmd = 'b';
				first = *line;
				if (intFlag) {
					i_puts(MSG_RedirectionInterrupted);
					return true;
				}
				goto rebrowse;
			}
		}

/* Jump to the #section, if specified in the url */
		s = findHash(line);
		if (!s)
			return true;
		++s;
/* Sometimes there's a # in the midst of a long url,
 * probably with post data.  It really screws things up.
 * Here is a kludge to avoid this problem.
 * Some day I need to figure this out. */
		if (strpbrk(line, "?\1"))
			return true;
/* Print the file size before we print the line. */
		if (fileSize >= 0) {
			debugPrint(1, "%d", fileSize);
			fileSize = -1;
		}
		for (i = 1; i <= cw->dol; ++i) {
			char *p = (char *)fetchLine(i, -1);
			if (lineHasTag(p, s)) {
				cw->dot = i;
				printDot();
				return true;
			}
		}
		setError(MSG_NoLable2, s);
		return false;
	}

	if (cmd == 'g' || cmd == 'v') {
		return doGlobal(line);
	}

	if (cmd == 'm' || cmd == 't') {
		return moveCopy();
	}

	if (cmd == 'i') {
		if (cw->browseMode) {
			setError(MSG_BrowseI);
			return false;
		}
		cmd = 'a';
		--startRange, --endRange;
	}

	if (cmd == 'c') {
		delText(startRange, endRange);
		endRange = --startRange;
		cmd = 'a';
	}

	if (cmd == 'a') {
		if (inscript) {
			setError(MSG_InsertFunction);
			return false;
		}
		if (cw->sqlMode) {
			j = cw->dol;
			rc = sqlAddRows(endRange);
/* adjust dot */
			j = cw->dol - j;
			if (j)
				cw->dot = endRange + j;
			else if (!endRange && cw->dol)
				cw->dot = 1;
			else
				cw->dot = endRange;
			return rc;
		}
		return inputLinesIntoBuffer();
	}

	if (cmd == 'd' || cmd == 'D') {
		if (cw->dirMode) {
			j = delFiles();
			undoCompare();
			cw->undoable = false;
			goto afterdelete;
		}
		if (cw->sqlMode) {
			j = sqlDelRows(startRange, endRange);
			undoCompare();
			cw->undoable = false;
			goto afterdelete;
		}
		if (cw->browseMode)
			delTags(startRange, endRange);
		delText(startRange, endRange);
		j = 1;
afterdelete:
		if (!j)
			globSub = false;
		else if (cmd == 'D')
			printDot();
		return j;
	}

	if (cmd == 'j' || cmd == 'J') {
		return joinText();
	}

	if (cmd == 'r') {
		if (cx)
			return readContext(cx);
		if (first) {
			if (cw->sqlMode && !isSQL(line)) {
				strcpy(newline, cw->fileName);
				strmove(strchr(newline, ']') + 1, line);
				line = newline;
			}
			j = readFile(line, emptyString);
			if (!serverData)
				fileSize = -1;
			return j;
		}
		setError(MSG_NoFileSpecified);
		return false;
	}

	if (cmd == 's') {
		j = substituteText(line);
		if (j < 0) {
			globSub = false;
			j = false;
		}
		if (newlocation)
			goto redirect;
		return j;
	}

	setError(MSG_CNYI, icmd);
	return (globSub = false);
}				/* runCommand */

bool edbrowseCommand(const char *line, bool script)
{
	bool rc;
	globSub = intFlag = false;
	inscript = script;
	fileSize = -1;
	skipWhite(&line);
	rc = runCommand(line);
	if (fileSize >= 0)
		debugPrint(1, "%d", fileSize);
	fileSize = -1;
	if (!rc) {
		if (!script)
			showErrorConditional(cmd);
		eeCheck();
	}
	return rc;
}				/* edbrowseCommand */

/* Take some text, usually empty, and put it in a side buffer. */
int sideBuffer(int cx, const char *text, int textlen, const char *bufname)
{
	int svcx = context;
	bool rc;
	if (cx) {
		cxQuit(cx, 3);
	} else {
		for (cx = 1; cx < MAXSESSION; ++cx)
			if (!sessionList[cx].lw)
				break;
		if (cx == MAXSESSION) {
			i_puts(MSG_NoBufferExtraWindow);
			return 0;
		}
	}
	cxSwitch(cx, false);
	if (bufname) {
		cw->fileName = cloneString(bufname);
		debrowseSuffix(cw->fileName);
	}
	if (textlen < 0) {
		textlen = strlen(text);
	} else {
		cw->binMode = looksBinary(text, textlen);
	}
	if (textlen) {
		rc = addTextToBuffer((pst) text, textlen, 0, true);
		cw->changeMode = false;
		if (!rc)
			i_printf(MSG_BufferPreload, cx);
	}
	/* back to original context */
	cxSwitch(svcx, false);
	return cx;
}				/* sideBuffer */

void freeEmptySideBuffer(int n)
{
	struct ebWindow *side;
	if (!(side = sessionList[n].lw))
		return;
	if (side->fileName)
		return;
	if (side->dol)
		return;
	if (side != sessionList[n].fw)
		return;
/* We could have added a line, then deleted it */
	cxQuit(n, 3);
}				/* freeEmptySideBuffer */

bool browseCurrentBuffer(void)
{
	char *rawbuf, *newbuf, *tbuf;
	int rawsize, tlen, j;
	bool rc, remote = false;
	bool save_ch = cw->changeMode;
	uchar bmode = 0;

	if (cw->fileName)
		remote = isURL(cw->fileName);

	if (cw->mt && cw->mt->outtype)
		bmode = 3;
	else
/* A mail message often contains lots of html tags,
 * so we need to check for email headers first. */
	if (!remote && emailTest())
		bmode = 1;
	else if (htmlTest())
		bmode = 2;
	else {
		setError(MSG_Unbrowsable);
		return false;
	}

	rawbuf = NULL;
	rawsize = 0;
	if (bmode != 3 || remote || access(cw->fileName, 4)) {
		if (!unfoldBuffer(context, false, &rawbuf, &rawsize))
			return false;	/* should never happen */
	}

	if (bmode == 3) {
/* convert raw text via a plugin */
		char *outfile = runPluginConverter(rawbuf, rawsize);
		if (!outfile) {
			nzFree(rawbuf);
			return false;
		}
		nzFree(rawbuf);
		if (stringEqual(outfile, "|")) {
			rawbuf = serverData;
			rawsize = serverDataLen;
			serverData = NULL;
		} else {
			rc = fileIntoMemory(outfile, &rawbuf, &rawsize);
			unlink(outfile);
			if (!rc)
				return false;
		}
		iuReformat(rawbuf, rawsize, &tbuf, &tlen);
		if (tbuf) {
			nzFree(rawbuf);
			rawbuf = tbuf;
			rawsize = tlen;
		}
/* make it look like remote html, so we don't get a lot of errors printed */
		remote = true;
		bmode = (cw->mt->outtype == 'h' ? 2 : 0);
		if (!allowRedirection)
			bmode = 0;
	}

/* this shouldn't do any harm if the output is text */
	prepareForBrowse(rawbuf, rawsize);

/* No harm in running this code in mail client, but no help either,
 * and it begs for bugs, so leave it out. */
	if (!ismc) {
		undoCompare();
		cw->undoable = false;
	}

	if (bmode == 1) {
		newbuf = emailParse(rawbuf);
		j = strlen(newbuf);

/* mail could need utf8 conversion, after qp decode */
		iuReformat(newbuf, j, &tbuf, &tlen);
		if (tbuf) {
			nzFree(newbuf);
			newbuf = tbuf;
			j = tlen;
		}

		if (memEqualCI(newbuf, "<html>\n", 7) && allowRedirection) {
/* double browse, mail then html */
			bmode = 2;
			remote = true;
			rawbuf = newbuf;
			rawsize = j;
			prepareForBrowse(rawbuf, rawsize);
		}
	}

	if (bmode == 2) {
		if (javaOK(cw->fileName))
			createJavaContext();
		nzFree(newlocation);	/* should already be 0 */
		newlocation = 0;
		newbuf = htmlParse(rawbuf, remote);
	}

	if (bmode == 0)
		newbuf = rawbuf;

	cw->rnlMode = cw->nlMode;
	cw->nlMode = false;
/* I'm gonna assume it ain't binary no more */
	cw->binMode = false;
	cw->r_dot = cw->dot, cw->r_dol = cw->dol;
	cw->dot = cw->dol = 0;
	cw->r_map = cw->map;
	cw->map = 0;
	memcpy(cw->r_labels, cw->labels, sizeof(cw->labels));
	memset(cw->labels, 0, sizeof(cw->labels));
	j = strlen(newbuf);
	rc = addTextToBuffer((pst) newbuf, j, 0, false);
	free(newbuf);
	cw->undoable = false;
	cw->changeMode = save_ch;

	if (cw->fileName) {
		j = strlen(cw->fileName);
		cw->fileName = reallocMem(cw->fileName, j + 8);
		strcat(cw->fileName, ".browse");
	}

	if (!rc) {
/* should never happen */
		fileSize = -1;
		cw->browseMode = true;
		return false;
	}

	if (bmode == 2)
		cw->dot = cw->dol;
	cw->browseMode = true;
	fileSize = apparentSize(context, true);
	return true;
}				/* browseCurrentBuffer */

bool locateTagInBuffer(int tagno, int *ln_p, char **p_p, char **s_p, char **t_p)
{
	int ln, n;
	char *p, *s, *t, c;
	char search[20];
	char searchend[4];

	sprintf(search, "%c%d<", InternalCodeChar, tagno);
	sprintf(searchend, "%c0>", InternalCodeChar);
	n = strlen(search);
	for (ln = 1; ln <= cw->dol; ++ln) {
		p = (char *)fetchLine(ln, -1);
		for (s = p; (c = *s) != '\n'; ++s) {
			if (c != InternalCodeChar)
				continue;
			if (!memcmp(s, search, n))
				break;
		}
		if (c == '\n')
			continue;	/* not here, try next line */
		s = strchr(s, '<') + 1;
		t = strstr(s, searchend);
		if (!t)
			i_printfExit(MSG_NoClosingLine, ln);
		*ln_p = ln;
		*p_p = p;
		*s_p = s;
		*t_p = t;
		return true;
	}

	return false;
}				/* locateTagInBuffer */

bool locateInvisibleAnchor(int tagno, int *ln_p, char **p_p, char **s_p,
			   char **t_p)
{
	int ln, n;
	char *p, *s, *t, c;
	char search[20];

	sprintf(search, "%c%d", InternalCodeChar, tagno);
	n = strlen(search);
	for (ln = 1; ln <= cw->dol; ++ln) {
		p = (char *)fetchLine(ln, -1);
		for (s = p; (c = *s) != '\n'; ++s) {
			if (c != InternalCodeChar)
				continue;
			if (!memcmp(s, search, n))
				break;
		}
		if (c == '\n')
			continue;	/* not here, try next line */

		for (t = s + 1; isdigitByte(*t); ++t) ;
		if (*t != '*')
			return false;
		*ln_p = ln;
		*p_p = p;
		*s_p = s;
		*t_p = t + 1;
		return true;
	}

	return false;
}				/* locateInvisibleAnchor */

char *getFieldFromBuffer(int tagno)
{
	int ln;
	char *p, *s, *t;
	if (locateTagInBuffer(tagno, &ln, &p, &s, &t))
		return pullString1(s, t);
	/* line has been deleted, revert to the reset value */
	return 0;
}				/* getFieldFromBuffer */

int fieldIsChecked(int tagno)
{
	int ln;
	char *p, *s, *t, *new;
	if (locateTagInBuffer(tagno, &ln, &p, &s, &t))
		return (*s == '+');
	return -1;
}				/* fieldIsChecked */
