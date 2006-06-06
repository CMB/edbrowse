/* buffers.c
 * Text buffer support routines, manage text and edit sessions.
 * Copyright (c) Karl Dahlke, 2006
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"
#include "tcp.h"

/* If this include file is missing, you need the pcre package,
 * and the pcre-devel package. */
#include <pcre.h>

/* Static variables for this file. */

/* The valid edbrowse commands. */
static const char valid_cmd[] = "aAbBcdefghHijJklmMnpqrstuvwz=^<";
/* Commands that can be done in browse mode. */
static const char browse_cmd[] = "AbBdefghHijJklmMnpqsuvwz=^<";
/* Commands for directory mode. */
static const char dir_cmd[] = "AbdefghHklMnpqsvwz=^<";
/* Commands that work at line number 0, in an empty file. */
static const char zero_cmd[] = "aAbefhHMqruw=^<";
/* Commands that expect a space afterward. */
static const char spaceplus_cmd[] = "befrw";
/* Commands that should have no text after them. */
static const char nofollow_cmd[] = "aAcdhHjlmnptu=";
/* Commands that can be done after a g// global directive. */
static const char global_cmd[] = "dijJlmnpst";

static struct ebWindow preWindow, undoWindow;
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
/* If a rendered line contains a hyperlink, the link is indicated
 * by a code that is stored inline.
 * If the hyperlink is number 17 on the list of hyperlinks for this window,
 * it is indicated by 0x80 17 { text }.
 * The "text" is what you see on the page, what you click on.
 * {Click here for more information}.
 * And the braces tell you it's a hyperlink.
 * That's just my convention.
 * The prior chars are for internal use only.
 * I'm assuming these chars don't/won't appear on the rendered page.
 * Yeah, sometimes nonascii chars appear, especially if the page is in
 * a European language, but I just assume a rendered page will not contain
 * the sequence: 0x80 number {
 * In fact I assume the rendered text won't contain 0x80 at all.
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

static void
removeHiddenNumbers(pst p)
{
    pst s, t, u;
    char c, d;

    s = t = p;
    while((c = *s) != '\n') {
	if(c != InternalCodeChar) {
	  addchar:
	    *t++ = c;
	    ++s;
	    continue;
	}
	u = s + 1;
	d = *u;
	if(!isdigit(d))
	    goto addchar;
	do {
	    d = *++u;
	} while(isdigit(d));
	if(d == '*') {
	    s = u + 1;
	    continue;
	}
	if(strchr("<>{}", d)) {
	    s = u;
	    continue;
	}
/* This is not a code sequence I recognize. */
/* This should never happen; just move along. */
	goto addchar;
    }				/* loop over p */
    *t = c;			/* terminating newline */
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

static pst
fetchLineContext(int n, int show, int cx)
{
    struct ebWindow *lw = sessionList[cx].lw;
    char *map, *t;
    int dol, idx;
    unsigned len;
    pst p;			/* the resulting copy of the string */

    if(!lw)
	errorPrint("@invalid session %d in fetchLineContext()", cx);
    map = lw->map;
    dol = lw->dol;
    if(n <= 0 || n > dol)
	errorPrint("@invalid line number %d in fetchLineContext()", n);

    t = map + LNWIDTH * n;
    idx = atoi(t);
    if(!textLines[idx])
	errorPrint("@line %d->%d became null", n, idx);
    if(show < 0)
	return textLines[idx];
    p = clonePstring(textLines[idx]);
    if(show && lw->browseMode)
	removeHiddenNumbers(p);
    return p;
}				/* fetchLineContext */

pst
fetchLine(int n, int show)
{
    return fetchLineContext(n, show, context);
}				/* fetchLine */

static int
apparentSize(int cx, bool browsing)
{
    char c;
    int i, ln, size;
    struct ebWindow *w;
    if(cx <= 0 || cx >= MAXSESSION || (w = sessionList[cx].lw) == 0) {
	setError("session %d is not active", cx);
	return -1;
    }
    size = 0;
    for(ln = 1; ln <= w->dol; ++ln) {
	if(browsing && sessionList[cx].lw->browseMode) {
	    pst line = fetchLineContext(ln, 1, cx);
	    size += pstLength(line);
	    free(line);
	} else {
	    pst line = fetchLineContext(ln, -1, cx);
	    size += pstLength(line);
	}
    }				/* loop over lines */
    if(sessionList[cx].lw->nlMode)
	--size;
    return size;
}				/* apparentSize */

int
currentBufferSize(void)
{
    return apparentSize(context, cw->browseMode);
}				/* currentBufferSize */

/* get the directory suffix for a file.
 * This only makes sense in directory mode. */
static char *
dirSuffixContext(int n, int cx)
{
    static char suffix[4];
    struct ebWindow *lw = sessionList[cx].lw;
    suffix[0] = 0;
    if(lw->dirMode) {
	char *s = lw->map + LNWIDTH * n + 8;
	suffix[0] = s[0];
	if(suffix[0] == ' ')
	    suffix[0] = 0;
	suffix[1] = s[1];
	if(suffix[1] == ' ')
	    suffix[1] = 0;
	suffix[2] = 0;
    }
    return suffix;
}				/* dirSuffixContext */

static char *
dirSuffix(int n)
{
    return dirSuffixContext(n, context);
}				/* dirSuffix */

/* Display a line to the screen, but no more than 500 chars. */
void
displayLine(int n)
{
    pst line = fetchLine(n, 1);
    pst s = line;
    int cnt = 0;
    uchar c;

    if(cmd == 'n')
	printf("%d ", n);
    if(endMarks == 2 || endMarks && cmd == 'l')
	printf("^");

    while((c = *s++) != '\n') {
	bool expand = false;
	if(c == 0 || c == '\r' || c == '\x1b')
	    expand = true;
	if(cmd == 'l') {
/* show tabs and backspaces, ed style */
	    if(c == '\b')
		c = '<';
	    if(c == '\t')
		c = '>';
	    if(!isprint(c))
		expand = true;
	}			/* list */
	if(expand)
	    printf("~%02X", c), cnt += 3;
	else
	    printf("%c", c), ++cnt;
	if(cnt >= 500)
	    break;
    }				/* loop over line */

    if(cnt >= 500)
	printf("...");
    printf("%s", dirSuffix(n));
    if(endMarks == 2 || endMarks && cmd == 'l')
	printf("$");
    printf("\n");

    free(line);
}				/* displayLine */

static void
printDot(void)
{
    if(cw->dot)
	displayLine(cw->dot);
    else
	printf("empty\n");
}				/* printDot */

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

pst
inputLine(void)
{
    static uchar line[MAXTTYLINE];
    int i, j;
    uchar c, d, e;

  top:
    intFlag = false;
    inInput = true;
    if(!fgets((char *)line, sizeof (line), stdin)) {
	if(intFlag)
	    goto top;
	printf("EOF\n");
	exit(1);
    }
    inInput = false;
    intFlag = false;

    i = j = 0;
    while(i < sizeof (line) - 1 && (c = line[i]) != '\n') {
/* A bug in my keyboard causes nulls to be entered from time to time. */
	if(c == 0)
	    c = ' ';
	if(c != '~') {
	  addchar:
	    line[j++] = c;
	    ++i;
	    continue;
	}
	d = line[i + 1];
	if(d == '~') {
	    ++i;
	    goto addchar;
	}
	if(!isxdigit(d))
	    goto addchar;
	e = line[i + 2];
	if(!isxdigit(e))
	    goto addchar;
	c = fromHex(d, e);
	if(c == '\n')
	    c = 0;
	i += 2;
	goto addchar;
    }				/* loop over input chars */
    line[j] = '\n';
    return line;
}				/* inputLine */

static void
carrySubstitutionStrings(const struct ebWindow *w, struct ebWindow *nw)
{
    if(!searchStringsAll)
	return;
    nw->lhs_yes = w->lhs_yes;
    strcpy(nw->lhs, w->lhs);
    nw->rhs_yes = w->rhs_yes;
    strcpy(nw->rhs, w->rhs);
}				/* carrySubstitutionStrings */

/* Create a new window, with default variables. */
static struct ebWindow *
createWindow(void)
{
    struct ebWindow *nw;	/* the new window */
    nw = allocZeroMem(sizeof (struct ebWindow));
    if(cw)
	carrySubstitutionStrings(cw, nw);
    return nw;
}				/* createWindow */

static void
freeWindowLines(char *map)
{
    char *t;
    int cnt = 0;
    if(map) {
	for(t = map + LNWIDTH; *t; t += LNWIDTH) {
	    int ln = atoi(t);
	    if(textLines[ln]) {
		free(textLines[ln]);
		++cnt;
		textLines[ln] = 0;
	    }
	}
	free(map);
    }
    debugPrint(6, "freeWindowLines = %d", cnt);
}				/* freeWindowLines */

/* Free any lines not used by the snapshot of the current session. */
void
freeUndoLines(const char *cmap)
{
    char *map = undoWindow.map;
    char *s;
    const char *t;
    int ln;
    int cnt = 0;

    if(!map) {
	debugPrint(6, "freeUndoLines = null");
	return;
    }

    if(!cmap) {
	debugPrint(6, "freeUndoLines = win");
	freeWindowLines(map);
	undoWindow.map = 0;
	return;
    }

/* This is pretty efficient most of the time,
 * real inefficient sometimes. */
    s = map + LNWIDTH;
    t = cmap + LNWIDTH;
    for(; *s && *t; s += LNWIDTH, t += LNWIDTH)
	if(!memcmp(s, t, 6))
	    *s = '*';
	else
	    break;

    if(*s) {			/* more to do */
	s = map + strlen(map);
	t = cmap + strlen(cmap);
	s -= LNWIDTH, t -= LNWIDTH;
	while(s > map && t > cmap) {
	    if(!memcmp(s, t, 6))
		*s = '*';
	    s -= LNWIDTH, t -= LNWIDTH;
	}

/* Ok, who's left? */
	for(s = map + LNWIDTH; *s; s += LNWIDTH) {
	    if(*s == '*')
		continue;	/* in use */
	    for(t = cmap + LNWIDTH; *t; t += LNWIDTH)
		if(!memcmp(s, t, 6))
		    break;
	    if(*t)
		continue;	/* in use */
	    ln = atoi(s);
	    if(textLines[ln]) {
		free(textLines[ln]);
		textLines[ln] = 0;
		++cnt;
	    }
	}
    }

    free(map);
    undoWindow.map = 0;
    debugPrint(6, "freeUndoLines = %d", cnt);
}				/* freeUndoLines */

static void
freeWindow(struct ebWindow *w)
{
/* The next few are designed to do nothing if not in browseMode */
    freeJavaContext(w->jsc);
    freeWindowLines(w->r_map);
    nzFree(w->dw);
    nzFree(w->ft);
    nzFree(w->fd);
    nzFree(w->fk);
    freeTags(w->tags);
    freeWindowLines(w->map);
    nzFree(w->fileName);
    nzFree(w->referrer);
    nzFree(w->baseDirName);
    free(w);
}				/* freeWindow */

/*********************************************************************
Here are a few routines to switch contexts from one buffer to another.
This is how the user edits multiple sessions, or browses multiple
web pages, simultaneously.
*********************************************************************/

bool
cxCompare(int cx)
{
    if(cx == 0) {
	setError("session 0 is invalid");
	return false;
    }
    if(cx >= MAXSESSION) {
	setError("session %d is out of bounds, limit %d", cx, MAXSESSION - 1);
	return false;
    }
    if(cx != context)
	return true;		/* ok */
    setError("you are already in session %d", cx);
    return false;
}				/*cxCompare */

/* is a context active? */
bool
cxActive(int cx)
{
    if(cx <= 0 || cx >= MAXSESSION)
	errorPrint("@session %d out of range in cxActive", cx);
    if(sessionList[cx].lw)
	return true;
    setError("session %d is not active", cx);
    return false;
}				/* cxActive */

static void
cxInit(int cx)
{
    struct ebWindow *lw = createWindow();
    if(sessionList[cx].lw)
	errorPrint("@double init on session %d", cx);
    sessionList[cx].fw = sessionList[cx].lw = lw;
}				/* cxInit */

bool
cxQuit(int cx, int action)
{
    struct ebWindow *w = sessionList[cx].lw;
    if(!w)
	errorPrint("@quitting a nonactive session %d", cx);

/* We might be trashing data, make sure that's ok. */
    if(w->changeMode &&
       !w->dirMode && lastq != cx && w->fileName && !isURL(w->fileName)) {
	lastqq = cx;
	setError("expecting `w'");
	if(cx != context)
	    setError("expecting `w' on session %d", cx);
	return false;
    }
    /* warning message */
    if(!action)
	return true;		/* just a check */

    if(cx == context) {
/* Don't need to retain the undo lines. */
	freeWindowLines(undoWindow.map);
	undoWindow.map = 0;
	nzFree(preWindow.map);
	preWindow.map = 0;
    }

    if(action == 2) {
	while(w) {
	    struct ebWindow *p = w->prev;
	    freeWindow(w);
	    w = p;
	}
	sessionList[cx].fw = sessionList[cx].lw = 0;
    } else
	freeWindow(w);

    return true;
}				/* cxQuit */

/* Switch to another edit session.
 * This assumes cxCompare has succeeded - we're moving to a different context.
 * Pass the context number and an interactive flag. */
void
cxSwitch(int cx, bool interactive)
{
    bool created = false;
    struct ebWindow *nw = sessionList[cx].lw;	/* the new window */
    if(!nw) {
	cxInit(cx);
	nw = sessionList[cx].lw;
	created = true;
    } else
	carrySubstitutionStrings(cw, nw);

    if(cw) {
	freeUndoLines(cw->map);
	cw->firstOpMode = false;
    }
    cw = nw;
    cs = sessionList + cx;
    context = cx;
    if(interactive && debugLevel) {
	if(created)
	    printf("new session\n");
	else if(cw->fileName)
	    puts(cw->fileName);
	else
	    puts("no file");
    }

/* The next line is required when this function is called from main(),
 * when the first arg is a url and there is a second arg. */
    startRange = endRange = cw->dot;
}				/* cxSwitch */

/* Make sure we have room to store another n lines. */

void
linesReset(void)
{
    int j;
    if(!textLines)
	return;
    for(j = 0; j < textLinesCount; ++j)
	nzFree(textLines[j]);
    nzFree(textLines);
    textLines = 0;
    textLinesCount = textLinesMax = 0;
}				/* linesReset */

bool
linesComing(int n)
{
    int need = textLinesCount + n;
    if(need > LNMAX) {
	setError
	   ("Your limit of 1 million lines has been reached.\nSave your files, then exit and restart this program.");
	return false;
    }
    if(need > textLinesMax) {
	int newmax = textLinesMax * 3 / 2;
	if(need > newmax)
	    newmax = need + 8192;
	if(newmax > LNMAX)
	    newmax = LNMAX;
	if(textLinesMax) {
	    debugPrint(4, "textLines realloc %d", newmax);
	    textLines = reallocMem(textLines, newmax * sizeof (pst));
	} else {
	    textLines = allocMem(newmax * sizeof (pst));
	}
	textLinesMax = newmax;
    }
    /* overflow requires realloc */
    /* We now have room for n new lines, but you have to add n
     * to textLines Count, once you have brought in the lines. */
    return true;
}				/* linesComing */

/* This function is called for web redirection, by the refresh command,
 * or by window.location = new_url. */
static char *newlocation;
static int newloc_d;		/* possible delay */
static bool newloc_rf;		/* refresh the buffer */
bool js_redirects;

void
gotoLocation(char *url, int delay, bool rf)
{
    if(newlocation && delay >= newloc_d) {
	nzFree(url);
	return;
    }
    nzFree(newlocation);
    newlocation = url;
    newloc_d = delay;
    newloc_rf = rf;
    if(!delay)
	js_redirects = true;
}				/* gotoLocation */

/* Adjust the map of line numbers -- we have inserted text.
 * Also shift the downstream labels.
 * Pass the string containing the new line numbers, and the dest line number. */
static void
addToMap(int start, int end, int destl)
{
    char *newmap;
    int i, j;
    int nlines = end - start;
    if(nlines == 0)
	errorPrint("@empty new piece in addToMap()");
    for(i = 0; i < 26; ++i) {
	int ln = cw->labels[i];
	if(ln <= destl)
	    continue;
	cw->labels[i] += nlines;
    }
    cw->dot = destl + nlines;
    cw->dol += nlines;
    newmap = allocMem((cw->dol + 1) * LNWIDTH + 1);
    if(cw->map)
	strcpy(newmap, cw->map);
    else
	strcpy(newmap, LNSPACE);
    i = j = (destl + 1) * LNWIDTH;	/* insert new piece here */
    while(start < end) {
	sprintf(newmap + i, LNFORMAT, start);
	++start;
	i += LNWIDTH;
    }
    if(cw->map)
	strcat(newmap, cw->map + j);
    nzFree(cw->map);
    cw->map = newmap;
    cw->firstOpMode = undoable = true;
    if(!cw->browseMode)
	cw->changeMode = true;
}				/* addToMap */

/* Add a block of text into the buffer; uses addToMap(). */
bool
addTextToBuffer(const pst inbuf, int length, int destl)
{
    int i, j, linecount = 0;
    int start, end;
    for(i = 0; i < length; ++i)
	if(inbuf[i] == '\n')
	    ++linecount;
    if(!linesComing(linecount + 1))
	return false;
    if(destl == cw->dol)
	cw->nlMode = false;
    if(inbuf[length - 1] != '\n') {
/* doesn't end in newline */
	++linecount;		/* last line wasn't counted */
	if(destl == cw->dol) {
	    cw->nlMode = true;
	    if(cmd != 'b' && !cw->binMode && !ismc)
		printf("no trailing newline\n");
	}
    }				/* missing newline */
    start = end = textLinesCount;
    i = 0;
    while(i < length) {		/* another line */
	j = i;
	while(i < length)
	    if(inbuf[i++] == '\n')
		break;
	if(inbuf[i - 1] == '\n') {
/* normal line */
	    textLines[end] = allocMem(i - j);
	} else {
/* last line with no nl */
	    textLines[end] = allocMem(i - j + 1);
	    textLines[end][i - j] = '\n';
	}
	memcpy(textLines[end], inbuf + j, i - j);
	++end;
    }				/* loop breaking inbuf into lines */
    textLinesCount = end;
    addToMap(start, end, destl);
    return true;
}				/* addTextToBuffer */

/* Pass input lines straight into the buffer, until the user enters . */

static bool
inputLinesIntoBuffer(void)
{
    int start = textLinesCount;
    int end = start;
    pst line;
    if(linePending[0])
	line = linePending;
    else
	line = inputLine();
    while(line[0] != '.' || line[1] != '\n') {
	if(!linesComing(1))
	    return false;
	textLines[end++] = clonePstring(line);
	line = inputLine();
    }
    if(end == start) {		/* no lines entered */
	cw->dot = endRange;
	if(!cw->dot && cw->dol)
	    cw->dot = 1;
	return true;
    }
    if(endRange == cw->dol)
	cw->nlMode = false;
    textLinesCount = end;
    addToMap(start, end, endRange);
    return true;
}				/* inputLinesIntoBuffer */

/* Delete a block of text. */

static void
delText(int start, int end)
{
    int i, j;
    if(end == cw->dol)
	cw->nlMode = false;
    j = end - start + 1;
    strcpy(cw->map + start * LNWIDTH, cw->map + (end + 1) * LNWIDTH);
/* move the labels */
    for(i = 0; i < 26; ++i) {
	int ln = cw->labels[i];
	if(ln < start)
	    continue;
	if(ln <= end) {
	    cw->labels[i] = 0;
	    continue;
	}
	cw->labels[i] -= j;
    }
    cw->dol -= j;
    cw->dot = start;
    if(cw->dot > cw->dol)
	cw->dot = cw->dol;
/* by convention an empty buffer has no map */
    if(!cw->dol) {
	free(cw->map);
	cw->map = 0;
    }
    cw->firstOpMode = undoable = true;
    if(!cw->browseMode)
	cw->changeMode = true;
}				/* delText */

/* Delete files from a directory as you delete lines.
 * Set dw to move them to your recycle bin.
 * Set dx to delete them outright. */

static char *
makeAbsPath(char *f)
{
    static char path[ABSPATH];
    if(strlen(cw->baseDirName) + strlen(f) > ABSPATH - 2) {
	setError("absolute path name too long, limit %d chars", ABSPATH);
	return 0;
    }
    sprintf(path, "%s/%s", cw->baseDirName, f);
    return path;
}				/* makeAbsPath */

static bool
delFiles(void)
{
    int ln, cnt;
    if(!dirWrite) {
	setError
	   ("directories are readonly, type dw to enable directory writes");
	return false;
    }
    if(dirWrite == 1 && !recycleBin) {
	setError
	   ("could not create .recycle under your home directory, to hold the deleted files");
	return false;
    }
    ln = startRange;
    cnt = endRange - startRange + 1;
    while(cnt--) {
	char *file, *t, *path, *ftype;
	file = (char *)fetchLine(ln, 0);
	t = strchr(file, '\n');
	if(!t)
	    errorPrint("@no newline on directory entry %s", file);
	*t = 0;
	path = makeAbsPath(file);
	if(!path) {
	    free(file);
	    return false;
	}
	ftype = dirSuffix(ln);
	if(dirWrite == 2 || *ftype == '@') {
	    if(unlink(path)) {
		setError("could not remove file %s", file);
		free(file);
		return false;
	    }
	} else {
	    char bin[ABSPATH];
	    sprintf(bin, "%s/%s", recycleBin, file);
	    if(rename(path, bin)) {
		setError
		   ("Could not move %s to the recycle bin, set dx mode to actually remove the file",
		   file);
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
static bool
moveCopy(void)
{
    int sr = startRange;
    int er = endRange + 1;
    int dl = destLine + 1;
    int i_sr = sr * LNWIDTH;	/* indexes into map */
    int i_er = er * LNWIDTH;
    int i_dl = dl * LNWIDTH;
    int n_lines = er - sr;
    char *map = cw->map;
    char *newmap;
    int lowcut, highcut, diff, i;

    if(dl > sr && dl < er) {
	setError("destination lies inside the block to be moved or copied");
	return false;
    }
    if(cmd == 'm' && (dl == er || dl == sr)) {
	if(globSub)
	    setError("no change");
	return false;
    }

    if(cmd == 't') {
	if(!linesComing(n_lines))
	    return false;
	for(i = sr; i < er; ++i)
	    textLines[textLinesCount++] = fetchLine(i, 0);
	addToMap(textLinesCount - n_lines, textLinesCount, destLine);
	return true;
    }
    /* copy */
    if(destLine == cw->dol || endRange == cw->dol)
	cw->nlMode = false;
/* All we really need do is rearrange the map. */
    newmap = allocMem((cw->dol + 1) * LNWIDTH + 1);
    strcpy(newmap, map);
    if(dl < sr) {
	memcpy(newmap + i_dl, map + i_sr, i_er - i_sr);
	memcpy(newmap + i_dl + i_er - i_sr, map + i_dl, i_sr - i_dl);
    } else {
	memcpy(newmap + i_sr, map + i_er, i_dl - i_er);
	memcpy(newmap + i_sr + i_dl - i_er, map + i_sr, i_er - i_sr);
    }
    free(cw->map);
    cw->map = newmap;

/* now for the labels */
    if(dl < sr) {
	lowcut = dl;
	highcut = er;
	diff = sr - dl;
    } else {
	lowcut = sr;
	highcut = dl;
	diff = dl - er;
    }
    for(i = 0; i < 26; ++i) {
	int ln = cw->labels[i];
	if(ln < lowcut)
	    continue;
	if(ln >= highcut)
	    continue;
	if(ln >= startRange && ln <= endRange) {
	    ln += (dl < sr ? -diff : diff);
	} else {
	    ln += (dl < sr ? n_lines : -n_lines);
	}
	cw->labels[i] = ln;
    }				/* loop over labels */

    cw->dot = endRange;
    cw->dot += (dl < sr ? -diff : diff);
    cw->firstOpMode = undoable = true;
    if(!cw->browseMode)
	cw->changeMode = true;
    return true;
}				/* moveCopy */

/* Join lines from startRange to endRange. */
static bool
joinText(void)
{
    int j, size;
    pst newline, t;
    if(startRange == endRange) {
	setError("cannot join one line");
	return false;
    }
    if(!linesComing(1))
	return false;
    size = 0;
    for(j = startRange; j <= endRange; ++j)
	size += pstLength(fetchLine(j, -1));
    t = newline = allocMem(size);
    for(j = startRange; j <= endRange; ++j) {
	pst p = fetchLine(j, -1);
	size = pstLength(p);
	memcpy(t, p, size);
	t += size;
	if(j < endRange) {
	    t[-1] = ' ';
	    if(cmd == 'j')
		--t;
	}
    }
    textLines[textLinesCount] = newline;
    sprintf(cw->map + startRange * LNWIDTH, LNFORMAT, textLinesCount);
    ++textLinesCount;
    delText(startRange + 1, endRange);
    cw->dot = startRange;
    return true;
}				/* joinText */

/* Read a file into the current buffer, or a URL.
 * Post/get data is passed, via the second parameter, if it's a URL. */
bool
readFile(const char *filename, const char *post)
{
    char *rbuf;			/* read buffer */
    int readSize;		/* should agree with fileSize */
    int fh;			/* file handle */
    int i, bincount;
    bool rc;			/* return code */
    char *nopound;

    if(memEqualCI(filename, "file://", 7))
	filename += 7;
    if(!*filename) {
	setError("missing file name");
	return false;
    }

    if(isURL(filename)) {
	const char *domain = getHostURL(filename);
	if(!domain)
	    return false;	/* some kind of error */
	if(!*domain) {
	    setError("empty domain in url");
	    return false;
	}
	serverData = 0;
	serverDataLen = 0;

	rc = httpConnect(0, filename);

	if(!rc) {
/* The error could have occured after redirection */
	    nzFree(changeFileName);
	    changeFileName = 0;
	    return false;
	}

/* We got some data.  Any warnings along the way have been printed,
 * like 404 file not found, but it's still worth continuing. */
	rbuf = serverData;
	fileSize = readSize = serverDataLen;

	if(fileSize == 0) {	/* empty file */
	    nzFree(rbuf);
	    cw->dot = endRange;
	    return true;
	}

    } else {			/* url or file */

/* reading a file from disk */
	fileSize = 0;
	if(fileTypeByName(filename, false) == 'd') {
/* directory scan */
	    int len, j, start, end;
	    cw->baseDirName = cloneString(filename);
/* get rid of trailing slash */
	    len = strlen(cw->baseDirName);
	    if(len && cw->baseDirName[len - 1] == '/')
		cw->baseDirName[len - 1] = 0;
/* Understand that the empty string now means / */
/* get the files, or fail if there is a problem */
	    if(!sortedDirList(filename, &start, &end))
		return false;
	    if(!cw->dol) {
		cw->dirMode = true;
		printf("directory mode\n");
	    }
	    if(start == end) {	/* empty directory */
		cw->dot = endRange;
		fileSize = 0;
		return true;
	    }

	    addToMap(start, end, endRange);

/* change 0 to nl and count bytes */
	    fileSize = 0;
	    for(j = start; j < end; ++j) {
		char *s, c, ftype;
		pst t = textLines[j];
		char *abspath = makeAbsPath((char *)t);
		while(*t) {
		    if(*t == '\n')
			*t = '\t';
		    ++t;
		}
		*t = '\n';
		len = t - textLines[j];
		fileSize += len + 1;
		if(!abspath)
		    continue;	/* should never happen */
		ftype = fileTypeByName(abspath, true);
		if(!ftype)
		    continue;
		s = cw->map + (endRange + 1 + j - start) * LNWIDTH + 8;
		if(isupper(ftype)) {	/* symbolic link */
		    if(!cw->dirMode)
			*t = '@', *++t = '\n';
		    else
			*s++ = '@';
		    ++fileSize;
		}
		ftype = tolower(ftype);
		c = 0;
		if(ftype == 'd')
		    c = '/';
		if(ftype == 's')
		    c = '^';
		if(ftype == 'c')
		    c = '<';
		if(ftype == 'b')
		    c = '*';
		if(ftype == 'p')
		    c = '|';
		if(!c)
		    continue;
		if(!cw->dirMode)
		    *t = c, *++t = '\n';
		else
		    *s++ = c;
		++fileSize;
	    }			/* loop fixing files in the directory scan */
	    return true;
	}
	/* reading a directory */
	nopound = cloneString(filename);
	rbuf = strchr(nopound, '#');
	if(rbuf)
	    *rbuf = 0;
	rc = fileIntoMemory(nopound, &rbuf, &fileSize);
	nzFree(nopound);
	if(!rc)
	    return false;
	serverData = rbuf;
	if(fileSize == 0) {	/* empty file */
	    free(rbuf);
	    cw->dot = endRange;
	    return true;
	}			/* empty */
    }				/* file or URL */

/* We got some data, from a file or from the internet.
 * Count the binary characters and decide if this is, on the whole,
 * binary or text.  I allow some nonascii chars,
 * like you might see in Spanish or German, and still call it text,
 * but if there's too many such chars, I call it binary.
 * It's not an exact science. */
    bincount = 0;
    for(i = 0; i < fileSize; ++i) {
	char c = rbuf[i];
	if(c <= 0)
	    ++bincount;
    }
    if(bincount * 4 - 10 < fileSize) {
/* looks like text.  In DOS, we should have compressed crlf.
 * Let's do that now. */
#ifdef DOSLIKE
	for(i = j = 0; i < fileSize - 1; ++i) {
	    char c = rbuf[i];
	    if(c == '\r' && rbuf[i + 1] == '\n')
		continue;
	    rbuf[j++] = c;
	}
	fileSize = j;
#endif
    } else if(binaryDetect & !cw->binMode) {
	printf("binary data\n");
	cw->binMode = true;
    }

    rc = addTextToBuffer((const pst)rbuf, fileSize, endRange);
    free(rbuf);
    return rc;
}				/* readFile */

/* Write a range to a file. */
static bool
writeFile(const char *name, int mode)
{
    int i, fh;
    if(memEqualCI(name, "file://", 7))
	name += 7;
    if(!*name) {
	setError("missing file name");
	return false;
    }
    if(isURL(name)) {
	setError("cannot write to a url");
	return false;
    }
    if(!cw->dol) {
	setError("writing an empty file");
	return false;
    }
/* mode should be TRUNC or APPEND */
    mode |= O_WRONLY | O_CREAT;
    if(cw->binMode)
	mode |= O_BINARY;
    fh = open(name, mode, 0666);
    if(fh < 0) {
	setError("cannot create %s", name);
	return false;
    }
    fileSize = 0;
    for(i = startRange; i <= endRange; ++i) {
	pst p = fetchLine(i, (cw->browseMode ? 1 : -1));
	int len = pstLength(p);
	char *suf = dirSuffix(i);
	if(!suf[0]) {
	    if(i == cw->dol && cw->nlMode)
		--len;
	    if(write(fh, p, len) < len) {
	      badwrite:
		setError("cannot write to %s", name);
		close(fh);
		return false;
	    }
	    fileSize += len;
	} else {
/* must write this line with the suffix on the end */
	    --len;
	    if(write(fh, p, len) < len)
		goto badwrite;
	    fileSize += len;
	    strcat(suf, "\n");
	    len = strlen(suf);
	    if(write(fh, suf, len) < len)
		goto badwrite;
	    fileSize += len;
	}
	if(cw->browseMode)
	    free(p);
    }				/* loop over lines */
    close(fh);
/* This is not an undoable operation, nor does it change data.
 * In fact the data is "no longer modified" if we have written all of it. */
    if(startRange == 1 && endRange == cw->dol)
	cw->changeMode = false;
    return true;
}				/* writeFile */

static bool
readContext(int cx)
{
    struct ebWindow *lw;
    int i, start, end, fardol;
    if(!cxCompare(cx))
	return false;
    if(!cxActive(cx))
	return false;
    fileSize = 0;
    lw = sessionList[cx].lw;
    fardol = lw->dol;
    if(!fardol)
	return true;
    if(!linesComing(fardol))
	return false;
    if(cw->dol == endRange)
	cw->nlMode = false;
    start = end = textLinesCount;
    for(i = 1; i <= fardol; ++i) {
	pst p = fetchLineContext(i, (lw->dirMode ? -1 : 1), cx);
	int len = pstLength(p);
	pst q;
	if(lw->dirMode) {
	    char *suf = dirSuffixContext(i, cx);
	    char *q = allocMem(len + 3);
	    memcpy(q, p, len);
	    --len;
	    strcat(suf, "\n");
	    strcpy(q + len, suf);
	    len = strlen(q);
	    p = (pst) q;
	}
	textLines[end++] = p;
	fileSize += len;
    }				/* loop over lines in the "other" context */
    textLinesCount = end;
    addToMap(start, end, endRange);
    if(lw->nlMode) {
	--fileSize;
	if(cw->dol == endRange)
	    cw->nlMode = true;
    }
    if(binaryDetect & !cw->binMode && lw->binMode) {
	cw->binMode = true;
	printf("binary data\n");
    }
    return true;
}				/* readContext */

static bool
writeContext(int cx)
{
    struct ebWindow *lw;
    int i, j, len;
    char *newmap;
    pst p;
    int fardol = endRange - startRange + 1;
    if(!startRange)
	fardol = 0;
    if(!linesComing(fardol))
	return false;
    if(!cxCompare(cx))
	return false;
    if(cxActive(cx) && !cxQuit(cx, 2))
	return false;

    cxInit(cx);
    lw = sessionList[cx].lw;
    fileSize = 0;
    if(startRange) {
	newmap = allocMem((fardol + 1) * LNWIDTH + 1);
	strcpy(newmap, LNSPACE);
	for(i = startRange, j = 1; i <= endRange; ++i, ++j) {
	    p = fetchLine(i, (cw->dirMode ? -1 : 1));
	    len = pstLength(p);
	    sprintf(newmap + j * LNWIDTH, LNFORMAT, textLinesCount);
	    if(cw->dirMode) {
		pst q;
		char *suf = dirSuffix(i);
		q = allocMem(len + 3);
		memcpy(q, p, len);
		--len;
		strcat(suf, "\n");
		strcpy((char *)q + len, suf);
		len = strlen((char *)q);
		p = q;
	    }
	    textLines[textLinesCount++] = p;
	    fileSize += len;
	}
	lw->map = newmap;
	lw->binMode = cw->binMode;
	if(cw->nlMode && endRange == cw->dol) {
	    lw->nlMode = true;
	    --fileSize;
	}
    }				/* nonempty range */
    lw->dot = lw->dol = fardol;

    return true;
}				/* writeContext */

static void
debrowseSuffix(char *s)
{
    if(!s)
	return;
    while(*s) {
	if(*s == '.' && stringEqual(s, ".browse")) {
	    *s = 0;
	    return;
	}
	++s;
    }
}				/* debrowseSuffix */

static bool
shellEscape(const char *line)
{
    char *newline, *s;
    const char *t;
    pst p;
    char key;
    int linesize, pass, n;
    char *sh;
    char subshell[ABSPATH];

/* preferred shell */
    sh = getenv("SHELL");
    if(!sh || !*sh)
	sh = "/bin/sh";

    linesize = strlen(line);
    if(!linesize) {
/* interactive shell */
	if(!isInteractive) {
	    setError("session is not interactive");
	    return false;
	}
#ifdef DOSLIKE
	system(line);		/* don't know how to spawn a shell here */
#else
	sprintf(subshell, "exec %s -i", sh);
	system(subshell);
#endif
	printf("ok\n");
	return true;
    }

/* Make substitutions within the command line. */
    for(pass = 1; pass <= 2; ++pass) {
	for(t = line; *t; ++t) {
	    if(*t != '\'')
		goto addchar;
	    if(t > line && isalnum(t[-1]))
		goto addchar;
	    key = t[1];
	    if(key && isalnum(t[2]))
		goto addchar;

	    if(key == '_') {
		++t;
		if(!cw->fileName)
		    continue;
		if(pass == 1) {
		    linesize += strlen(cw->fileName);
		} else {
		    strcpy(s, cw->fileName);
		    s += strlen(s);
		}
		continue;
	    }
	    /* '_ filename */
	    if(key == '.' || key == '-' || key == '+') {
		n = cw->dot;
		if(key == '-')
		    --n;
		if(key == '+')
		    ++n;
		if(n > cw->dol || n == 0) {
		    setError("line %c is out of range", key);
		    return false;
		}
	      frombuf:
		++t;
		if(pass == 1) {
		    p = fetchLine(n, -1);
		    linesize += pstLength(p) - 1;
		} else {
		    p = fetchLine(n, 1);
		    if(perl2c((char *)p)) {
			free(p);
			setError("cannot embed nulls in a shell command");
			return false;
		    }
		    strcpy(s, (char *)p);
		    s += strlen(s);
		    free(p);
		}
		continue;
	    }
	    /* '. current line */
	    if(islower(key)) {
		n = cw->labels[key - 'a'];
		if(!n) {
		    setError("label %c not set", key);
		    return false;
		}
		goto frombuf;
	    }
	    /* 'x the line labeled x */
	  addchar:
	    if(pass == 1)
		++linesize;
	    else
		*s++ = *t;
	}			/* loop over chars */

	if(pass == 1)
	    s = newline = allocMem(linesize + 1);
	else
	    *s = 0;
    }				/* two passes */

/* Run the command.  Note that this routine returns success
 * even if the shell command failed.
 * Edbrowse succeeds if it is *able* to run the system command. */
    system(newline);
    printf("ok\n");
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
    bool cc = false;		/* are we in a [...] character class */
    int mod;			/* length of modifier */
    int paren = 0;		/* nesting level of parentheses */
/* We wouldn't be here if the line was empty. */
    char delim = *line++;

    *rexp = re;
    if(!strchr(valid_delim, delim)) {
	setError("invalid delimiter");
	return false;
    }
    start = line;

    c = *line;
    if(ebmuck) {
	if(isleft) {
	    if(c == delim || c == 0) {
		if(!cw->lhs_yes) {
		    setError("no remembered search string");
		    return false;
		}
		strcpy(re, cw->lhs);
		*split = line;
		return true;
	    }
/* Interpret lead * or lone [ as literal */
	    if(strchr("*?+", c) || c == '[' && !line[1]) {
		*e++ = '\\';
		*e++ = c;
		++line;
		ondeck = true;
	    }
	} else if(c == '%' && (line[1] == delim || line[1] == 0)) {
	    if(!cw->rhs_yes) {
		setError("no remembered replacement string");
		return false;
	    }
	    strcpy(re, cw->rhs);
	    *split = line + 1;
	    return true;
	}
    }
    /* ebmuck tricks */
    while(c = *line) {
	if(e >= re + MAXRE - 3) {
	    setError("regular expression too long");
	    return false;
	}
	d = line[1];

	if(c == '\\') {
	    line += 2;
	    if(d == 0) {
		setError("line ends in backslash");
		return false;
	    }
	    ondeck = true;
	    was_ques = false;
/* I can't think of any reason to remove the escaping \ from any character,
 * except ()|, where we reverse the sense of escape. */
	    if(ebmuck && isleft && !cc && (d == '(' || d == ')' || d == '|')) {
		if(d == '|')
		    ondeck = false, was_ques = true;
		if(d == '(')
		    ++paren, ondeck = false, was_ques = true;
		if(d == ')')
		    --paren;
		if(paren < 0) {
		    setError("Unexpected closing )");
		    return false;
		}
		*e++ = d;
		continue;
	    }
	    if(d == delim || ebmuck && !isleft && d == '&') {
		*e++ = d;
		continue;
	    }
/* Nothing special; we retain the escape character. */
	    *e++ = c;
	    if(isleft && d >= '0' && d <= '7' && (*line < '0' || *line > '7'))
		*e++ = '0';
	    *e++ = d;
	    continue;
	}

	/* escaping backslash */
	/* Break out if we hit the delimiter. */
	if(c == delim)
	    break;

/* Remember, I reverse the sense of ()| */
	if(isleft) {
	    if(ebmuck && (c == '(' || c == ')' || c == '|') || c == '^' && line != start && !cc)	/* don't know why we have to do this */
		*e++ = '\\';
	    if(c == '$' && d && d != delim)
		*e++ = '\\';
	}

	if(c == '$' && !isleft && isdigit(d)) {
	    if(d == '0' || isdigit(line[2])) {
		setError("replacement string can only use $1 through $9");
		return false;
	    }
	}
	/* dollar digit on the right */
	if(!isleft && c == '&' && ebmuck) {
	    *e++ = '$';
	    *e++ = '0';
	    ++line;
	    continue;
	}

/* push the character */
	*e++ = c;
	++line;

/* No more checks for the rhs */
	if(!isleft)
	    continue;

	if(cc) {		/* character class */
	    if(c == ']')
		cc = false;
	    continue;
	}
	if(c == '[')
	    cc = true;

/* Skip all these checks for javascript,
 * it probably has the expression right anyways. */
	if(!ebmuck)
	    continue;

/* Modifiers must have a preceding character.
 * Except ? which can reduce the greediness of the others. */
	if(c == '?' && !was_ques) {
	    ondeck = false;
	    was_ques = true;
	    continue;
	}

	mod = 0;
	if(c == '?' || c == '*' || c == '+')
	    mod = 1;
	if(c == '{' && isdigit(d)) {
	    const char *t = line + 1;
	    while(isdigit(*t))
		++t;
	    if(*t == ',')
		++t;
	    while(isdigit(*t))
		++t;
	    if(*t == '}')
		mod = t + 2 - line;
	}
	if(mod) {
	    --mod;
	    if(mod) {
		strncpy(e, line, mod);
		e += mod;
		line += mod;
	    }
	    if(!ondeck) {
		*e = 0;
		setError("%s modifier has no preceding character", e - mod - 1);
		return false;
	    }
	    ondeck = false;
	    continue;
	}
	/* modifier */
	ondeck = true;
	was_ques = false;
    }				/* loop over chars in the pattern */
    *e = 0;

    *split = line;

    if(ebmuck) {
	if(cc) {
	    setError("no closing ]");
	    return false;
	}
	if(paren) {
	    setError("no closing )");
	    return false;
	}

	if(isleft) {
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
static const char *re_error;
static int re_offset;
static int re_opt;
static int re_vector[11 * 3];
static pcre *re_cc;		/* compiled */

/* Get the start or end of a range.
 * Pass the line containing the address. */
static bool
getRangePart(const char *line, int *lineno, const char **split)
{				/* result parameters */
    int ln = cw->dot;		/* this is where we start */
    char first = *line;

    if(isdigit(first)) {
	ln = strtol(line, (char **)&line, 10);
    } else if(first == '.') {
	++line;
/* ln is already set */
    } else if(first == '$') {
	++line;
	ln = cw->dol;
    } else if(first == '\'' && islower(line[1])) {
	ln = cw->labels[line[1] - 'a'];
	if(!ln) {
	    setError("label %c not set", line[1]);
	    return false;
	}
	line += 2;
    } else if(first == '/' || first == '?') {

	char *re;		/* regular expression */
	bool ci = caseInsensitive;
	char incr;		/* forward or back */
/* Don't look through an empty buffer. */
	if(cw->dol == 0) {
	    setError("empty buffer");
	    return false;
	}
	if(!regexpCheck(line, true, true, &re, &line))
	    return false;
	if(*line == first) {
	    ++line;
	    if(*line == 'i')
		ci = true, ++line;
	}

	/* second delimiter */
	/* Earlier versions of pcre don't support this. */
	/* re_opt = PCRE_NO_AUTO_CAPTURE; */
	re_opt = 0;
	if(ci)
	    re_opt |= PCRE_CASELESS;
	re_cc = pcre_compile(re, re_opt, &re_error, &re_offset, 0);
	if(!re_cc) {
	    setError("error in regular expression, %s", re_error);
	    return false;
	}
/* We should probably study the pattern, if the file is large.
 * But then again, it's probably not worth it,
 * since the expressions are simple, and the lines are short. */
	incr = (first == '/' ? 1 : -1);
	while(true) {
	    char *subject;
	    ln += incr;
	    if(ln > cw->dol)
		ln = 1;
	    if(ln == 0)
		ln = cw->dol;
	    subject = (char *)fetchLine(ln, 1);
	    re_count =
	       pcre_exec(re_cc, 0, subject, pstLength((pst) subject) - 1, 0, 0,
	       re_vector, 33);
	    free(subject);
	    if(re_count < -1) {
		pcre_free(re_cc);
		setError
		   ("unexpected error while evaluating the regular expression at line %d",
		   ln);
		return (globSub = false);
	    }
	    if(re_count >= 0)
		break;
	    if(ln == cw->dot) {
		pcre_free(re_cc);
		setError("search string not found");
		return false;
	    }
	}			/* loop over lines */
	pcre_free(re_cc);
/* and ln is the line that matches */
    }

    /* search pattern */
    /* Now add or subtract from this number */
    while((first = *line) == '+' || first == '-') {
	int add = 1;
	++line;
	if(isdigit(*line))
	    add = strtol(line, (char **)&line, 10);
	ln += (first == '+' ? add : -add);
    }

    if(ln > cw->dol) {
	setError("line number too large");
	return false;
    }
    if(ln < 0) {
	setError("negative line number");
	return false;
    }

    *lineno = ln;
    *split = line;
    return true;
}				/* getRangePart */

/* Apply a regular expression to each line, and then execute
 * a command for each matching, or nonmatching, line.
 * This is the global feature, g/re/p, which gives us the word grep. */
static bool
doGlobal(const char *line)
{
    int gcnt = 0;		/* global count */
    bool ci = caseInsensitive;
    bool change;
    char delim = *line;
    char *t;
    char *re;			/* regular expression */
    int i, origdot, yesdot, nodot;

    if(!delim) {
	setError("no regular expression after %c", icmd);
	return false;
    }

    if(!regexpCheck(line, true, true, &re, &line))
	return false;
    if(*line != delim) {
	setError("missing delimiter");
	return false;
    }
    ++line;
    if(*line == 'i')
	++line, ci = true;
    skipWhite(&line);

/* clean up any previous stars */
    for(t = cw->map + LNWIDTH; *t; t += LNWIDTH)
	t[6] = ' ';

/* Find the lines that match the pattern. */
    re_opt = 0;
    if(ci)
	re_opt |= PCRE_CASELESS;
    re_cc = pcre_compile(re, re_opt, &re_error, &re_offset, 0);
    if(!re_cc) {
	setError("error in regular expression, %s", re_error);
	return false;
    }
    for(i = startRange; i <= endRange; ++i) {
	char *subject = (char *)fetchLine(i, 1);
	re_count =
	   pcre_exec(re_cc, 0, subject, pstLength((pst) subject), 0, 0,
	   re_vector, 33);
	free(subject);
	if(re_count < -1) {
	    pcre_free(re_cc);
	    setError
	       ("unexpected error while evaluating the regular expression at line %d",
	       i);
	    return false;
	}
	if(re_count < 0 && cmd == 'v' || re_count >= 0 && cmd == 'g') {
	    ++gcnt;
	    cw->map[i * LNWIDTH + 6] = '*';
	}
    }				/* loop over line */
    pcre_free(re_cc);

    if(!gcnt) {
	setError(cmd ==
	   'g' ? "no lines match the g pattern" :
	   "all lines match the v pattern");
	return false;
    }

/* apply the subcommand to every line with a star */
    globSub = true;
    setError(0);
    if(!*line)
	line = "p";
    origdot = cw->dot;
    yesdot = nodot = 0;
    change = true;
    while(gcnt && change) {
	change = false;		/* kinda like bubble sort */
	for(i = 1; i <= cw->dol; ++i) {
	    t = cw->map + i * LNWIDTH + 6;
	    if(*t != '*')
		continue;
	    if(intFlag)
		goto done;
	    change = true, --gcnt;
	    *t = ' ';
	    cw->dot = i;	/* so we can run the command at this line */
	    if(runCommand(line)) {
		yesdot = cw->dot;
/* try this line again, in case we deleted or moved it somewhere else */
		if(undoable)
		    --i, t -= LNWIDTH;
	    } else {
/* error in subcommand might turn global flag off */
		if(!globSub) {
		    nodot = i, yesdot = 0;
		    goto done;
		}		/* serious error */
	    }			/* subcommand succeeds or fails */
	}			/* loop over lines */
    }				/* loop making changes */
  done:

    globSub = false;
/* yesdot could be 0, even on success, if all lines are deleted via g/re/d */
    if(yesdot || !cw->dol) {
	cw->dot = yesdot;
	if((cmd == 's' || cmd == 'i') && subPrint == 1)
	    printDot();
    } else if(nodot) {
	cw->dot = nodot;
    } else {
	cw->dot = origdot;
	if(!errorMsg[0])
	    setError("none of the marked lines were successfully modified");
    }
    if(!errorMsg[0] && intFlag)
	setError(opint);
    return (errorMsg[0] == 0);
}				/* doGlobal */

static void
fieldNumProblem(const char *desc, char c, int n, int nt)
{
    if(!nt) {
	setError("no %s present", desc);
	return;
    }
    if(!n) {
	setError("multiple %s present, please use %c1 through %c%d", desc, c, c,
	   nt);
	return;
    }
    if(nt > 1)
	setError("%d is out of range, please use %c1 through %c%d", n, c, c,
	   nt);
    else
	setError("%d is out of range, please use %c1, or simply %c", n, c, c);
}				/* fieldNumProblem */

/* Perform a substitution on a given line.
 * The lhs has been compiled, and the rhs is passed in for replacement.
 * Refer to the static variable re_cc for the compiled lhs.
 * The replacement line is static, with a fixed length.
 * Return 0 for no match, 1 for a replacement, and -1 for a real problem. */

char replaceLine[REPLACELINELEN];
static char *replaceLineEnd;
static int replaceLineLen;
static int
replaceText(const char *line, int len, const char *rhs,
   bool ebmuck, int nth, bool global, int ln)
{
    int offset = 0, lastoffset, instance = 0;
    int span;
    char *r = replaceLine;
    char *r_end = replaceLine + REPLACELINELEN - 8;
    const char *s = line, *s_end, *t;
    char c, d;

    while(true) {
/* find the next match */
	re_count = pcre_exec(re_cc, 0, line, len, offset, 0, re_vector, 33);
	if(re_count < -1) {
	    setError
	       ("unexpected error while evaluating the regular expression at line %d",
	       ln);
	    return -1;
	}
	if(re_count < 0)
	    break;
	++instance;		/* found another match */
	lastoffset = offset;
	offset = re_vector[1];	/* ready for next iteration */
	if(offset == lastoffset && (nth > 1 || global)) {
	    setError("cannot replace multiple instances of the empty string");
	    return -1;
	}
	if(!global &&instance != nth)
	    continue;

/* copy up to the match point */
	s_end = line + re_vector[0];
	span = s_end - s;
	if(r + span >= r_end)
	    goto longvar;
	memcpy(r, s, span);
	r += span;
	s = line + offset;

/* Now copy over the rhs */
/* Special case lc mc uc */
	if(ebmuck && (rhs[0] == 'l' || rhs[0] == 'm' || rhs[0] == 'u') &&
	   rhs[1] == 'c' && rhs[2] == 0) {
	    span = re_vector[1] - re_vector[0];
	    if(r + span + 1 > r_end)
		goto longvar;
	    memcpy(r, line + re_vector[0], span);
	    r[span] = 0;
	    caseShift(r, rhs[0]);
	    r += span;
	    if(!global)
		break;
	    continue;
	}

	/* case shift */
	/* copy rhs, watching for $n */
	t = rhs;
	while(c = *t) {
	    if(r >= r_end)
		goto longvar;
	    d = t[1];
	    if(c == '\\') {
		t += 2;
		if(d == '$') {
		    *r++ = d;
		    continue;
		}
		if(d == 'n') {
		    *r++ = '\n';
		    continue;
		}
		if(d == 't') {
		    *r++ = '\t';
		    continue;
		}
		if(d == 'b') {
		    *r++ = '\b';
		    continue;
		}
		if(d == 'r') {
		    *r++ = '\r';
		    continue;
		}
		if(d == 'f') {
		    *r++ = '\f';
		    continue;
		}
		if(d == 'a') {
		    *r++ = '\a';
		    continue;
		}
		if(d >= '0' && d <= '7') {
		    int octal = d - '0';
		    d = *t;
		    if(d >= '0' && d <= '7') {
			++t;
			octal = 8 * octal + d - '0';
			d = *t;
			if(d >= '0' && d <= '7') {
			    ++t;
			    octal = 8 * octal + d - '0';
			}
		    }
		    *r++ = octal;
		    continue;
		}		/* octal */
		if(!ebmuck)
		    *r++ = '\\';
		*r++ = d;
		continue;
	    }			/* backslash */
	    if(c == '$' && isdigit(d)) {
		int y, z;
		t += 2;
		d -= '0';
		if(d > re_count)
		    continue;
		y = re_vector[2 * d];
		z = re_vector[2 * d + 1];
		if(y < 0)
		    continue;
		span = z - y;
		if(r + span >= r_end)
		    goto longvar;
		memcpy(r, line + y, span);
		r += span;
		continue;
	    }
	    *r++ = c;
	    ++t;
	}

	if(!global)
	    break;
    }				/* loop matching the regular expression */

    if(!instance)
	return false;
    if(!global &&instance < nth)
	return false;

/* We got a match, copy the last span. */
    s_end = line + len;
    span = s_end - s;
    if(r + span >= r_end)
	goto longvar;
    memcpy(r, s, span);
    r += span;
    replaceLineEnd = r;
    replaceLineLen = r - replaceLine;
    return true;

  longvar:
    setError("line exceeds %d bytes after substitution", REPLACELINELEN);
    return -1;
}				/* replaceText */

/* Substitute text on the lines in startRange through endRange.
 * We could be changing the text in an input field.
 * If so, we'll call infReplace().
 * Also, we might be indirectory mode, whence we must rename the file.
 * This is a complicated function!
 * The return can be true or false, with the usual meaning,
 * but also a return of -1, which is failure,
 * and an indication that we need to abort any g// in progress.
 * It's a serious problem. */

static int
substituteText(const char *line)
{
    int whichField = 0;
    bool bl_mode = false;	/* running the bl command */
    bool g_mode = false;	/* s/x/y/g */
    bool ci = caseInsensitive;
    bool save_nlMode;
    char c, *s, *t;
    int nth = 0;		/* s/x/y/7 */
    int lastSubst = 0;		/* last successful substitution */
    char *re;			/* the parsed regular expression */
    int ln;			/* line number */
    int j, linecount, slashcount, nullcount, tagno, total;
    char lhs[MAXRE], rhs[MAXRE];

    subPrint = 1;		/* default is to print the last line substituted */
    re_cc = 0;
    if(stringEqual(line, "`bl"))
	bl_mode = true, breakLineSetup();

    if(!bl_mode) {
/* watch for s2/x/y/ for the second input field */
	if(isdigit(*line))
	    whichField = strtol(line, (char **)&line, 10);
	if(!*line) {
	    setError("no regular expression after %c", icmd);
	    return -1;
	}

	if(cw->dirMode && !dirWrite) {
	    setError
	       ("directories are readonly, type dw to enable directory writes");
	    return -1;
	}

	if(!regexpCheck(line, true, true, &re, &line))
	    return -1;
	strcpy(lhs, re);
	if(!*line) {
	    setError("missing delimiter");
	    return -1;
	}
	if(!regexpCheck(line, false, true, &re, &line))
	    return -1;
	strcpy(rhs, re);

	if(*line) {		/* third delimiter */
	    ++line;
	    subPrint = 0;
	    while(c = *line) {
		if(c == 'g') {
		    g_mode = true;
		    ++line;
		    continue;
		}
		if(c == 'i') {
		    ci = true;
		    ++line;
		    continue;
		}
		if(c == 'p') {
		    subPrint = 2;
		    ++line;
		    continue;
		}
		if(isdigit(c)) {
		    if(nth) {
			setError("multiple numbers after the third delimiter");
			return -1;
		    }
		    nth = strtol(line, (char **)&line, 10);
		    continue;
		}		/* number */
		setError
		   ("unexpected substitution suffix after the third delimiter");
		return -1;
	    }			/* loop gathering suffix flags */
	    if(g_mode && nth) {
		setError
		   ("cannot use both a numeric suffix and the `g' suffix simultaneously");
		return -1;
	    }
	}			/* closing delimiter */
	if(nth == 0 && !g_mode)
	    nth = 1;

	re_opt = 0;
	if(ci)
	    re_opt |= PCRE_CASELESS;
	re_cc = pcre_compile(lhs, re_opt, &re_error, &re_offset, 0);
	if(!re_cc) {
	    setError("error in regular expression, %s", re_error);
	    return -1;
	}
    } else {

	subPrint = 0;
    }				/* bl_mode or not */

    if(!globSub)
	setError(0);

    for(ln = startRange; ln <= endRange && !intFlag; ++ln) {
	char *p = (char *)fetchLine(ln, -1);
	int len = pstLength((pst) p);

	if(bl_mode) {
	    int newlen;
	    if(!breakLine(p, len, &newlen)) {
		setError
		   ("sorry, cannot apply the bl command to lines longer than %d bytes",
		   REPLACELINELEN);
		return -1;
	    }
/* empty line is not allowed */
	    if(!newlen)
		replaceLine[newlen++] = '\n';
/* perhaps no changes were made */
	    if(newlen == len && !memcmp(p, replaceLine, len))
		continue;
	    replaceLineLen = newlen;
	    replaceLineEnd = replaceLine + newlen;
/* But the regular substitute doesn't have the \n on the end.
 * We need to make this one conform. */
	    --replaceLineEnd, --replaceLineLen;
	} else {

	    if(cw->browseMode) {
		char search[20];
		findInputField(p, 1, whichField, &total, &tagno);
		if(!tagno) {
		    fieldNumProblem("input fields", 'i', whichField, total);
		    continue;
		}
		sprintf(search, "%c%d<", InternalCodeChar, tagno);
/* Ok, if the line contains a null, this ain't gonna work. */
		s = strstr(p, search);
		if(!s)
		    continue;
		s = strchr(s, '<') + 1;
		t = strstr(s, "\2000>");
		if(!t)
		    continue;
		j = replaceText(s, t - s, rhs, true, nth, g_mode, ln);
	    } else {
		j = replaceText(p, len - 1, rhs, true, nth, g_mode, ln);
	    }
	    if(j < 0)
		goto abort;
	    if(!j)
		continue;
	}

/* Did we split this line into many lines? */
	linecount = slashcount = nullcount = 0;
	for(t = replaceLine; t < replaceLineEnd; ++t) {
	    c = *t;
	    if(c == '\n')
		++linecount;
	    if(c == 0)
		++nullcount;
	    if(c == '/')
		++slashcount;
	}
	if(!linesComing(linecount + 1))
	    goto abort;

	if(cw->dirMode) {
/* move the file, then update the text */
	    char src[ABSPATH], *dest;
	    if(slashcount + nullcount + linecount) {
		setError
		   ("cannot embed slash, newline, or null in a directory name");
		goto abort;
	    }
	    p[len - 1] = 0;	/* temporary */
	    t = makeAbsPath(p);
	    p[len - 1] = '\n';
	    if(!t)
		goto abort;
	    strcpy(src, t);
	    *replaceLineEnd = 0;
	    dest = makeAbsPath(replaceLine);
	    if(!dest)
		goto abort;
	    if(!stringEqual(src, dest)) {
		if(fileTypeByName(dest, true)) {
		    setError("destination file already exists");
		    goto abort;
		}
		if(rename(src, dest)) {
		    setError("cannot rename file to %s", dest);
		    goto abort;
		}
	    }			/* source and dest are different */
	}
	/* directory */
	if(cw->browseMode) {
	    if(nullcount) {
		setError("cannot embed nulls in an input field");
		goto abort;
	    }
	    if(linecount) {
		setError("cannot embed newlines in an input field");
		goto abort;
	    }
	    replaceLine[replaceLineLen] = 0;
/* We're managing our own printing, so leave notify = 0 */
	    if(!infReplace(tagno, replaceLine, 0))
		goto abort;
	} else {

	    replaceLine[replaceLineLen] = '\n';
	    if(!linecount) {
/* normal substitute */
		char newnum[LNWIDTH];
		textLines[textLinesCount] = allocMem(replaceLineLen + 1);
		memcpy(textLines[textLinesCount], replaceLine,
		   replaceLineLen + 1);
		sprintf(newnum, "%06d", textLinesCount);
		memcpy(cw->map + ln * LNWIDTH, newnum, 6);
		++textLinesCount;
	    } else {
/* Becomes many lines, this is the tricky case. */
		save_nlMode = cw->nlMode;
		delText(ln, ln);
		addTextToBuffer((pst) replaceLine, replaceLineLen + 1, ln - 1);
		cw->nlMode = save_nlMode;
		endRange += linecount;
		ln += linecount;
/* There's a quirk when adding newline to the end of a buffer
 * that had no newline at the end before. */
		if(cw->nlMode &&
		   ln == cw->dol && replaceLine[replaceLineLen - 1] == '\n') {
		    delText(ln, ln);
		    --ln, --endRange;
		}
	    }
	}			/* browse or not */

	if(subPrint == 2)
	    displayLine(ln);
	lastSubst = ln;
	cw->firstOpMode = undoable = true;
	if(!cw->browseMode)
	    cw->changeMode = true;
    }				/* loop over lines in the range */
    if(re_cc)
	pcre_free(re_cc);

    if(intFlag) {
	setError(opint);
	return -1;
    }

    if(!lastSubst) {
	if(!globSub) {
	    if(!errorMsg[0])
		setError(bl_mode ? "no changes" : "no match");
	}
	return false;
    }
    cw->dot = lastSubst;
    if(subPrint == 1 && !globSub)
	printDot();
    return true;

  abort:
    if(re_cc)
	pcre_free(re_cc);
    return -1;
}				/* substituteText */

/*********************************************************************
Implement various two letter commands.
Most of these set and clear modes.
Return 1 or 0 for success or failure as usual.
But return 2 if there is a new command to run.
The second parameter is a result parameter, the new command.
*********************************************************************/

static int
twoLetter(const char *line, const char **runThis)
{
    static char newline[MAXTTYLINE];
    char c;
    bool rc;
    int i;

    *runThis = newline;

    if(stringEqual(line, "qt"))
	exit(0);

/*
if(stringEqual(line, "ws")) return stripChild();
if(stringEqual(line, "us")) return unstripChild();
*/

    if(line[0] == 'd' && line[1] == 'b' && isdigit(line[2]) && !line[3]) {
	debugLevel = line[2] - '0';
	return true;
    }

    if(line[0] == 'u' && line[1] == 'a' && isdigit(line[2]) && !line[3]) {
	char *t = userAgents[line[2] - '0'];
	cmd = 'e';
	if(!t) {
	    setError("agent number %c is not defined", line[2]);
	    return false;
	}
	currentAgent = t;
	if(helpMessagesOn || debugLevel >= 1)
	    puts(currentAgent);
	return true;
    }

/* ^^^^ is the same as ^4 */
    if(line[0] == '^' && line[1] == '^') {
	const char *t = line + 2;
	while(*t == '^')
	    ++t;
	if(!*t) {
	    sprintf(newline, "^%d", t - line);
	    return 2;
	}
    }

    if(line[0] == 'm' && (i = stringIsNum(line + 1)) >= 0) {
	sprintf(newline, "^M%d", i);
	return 2;
    }

    if(line[0] == 'c' && line[1] == 'd') {
	c = line[2];
	if(!c || isspace(c)) {
	    const char *t = line + 2;
	    skipWhite(&t);
	    c = *t;
	    cmd = 'e';		/* so error messages are printed */
	    if(!c) {
		char cwdbuf[ABSPATH];
	      pwd:
		if(!getcwd(cwdbuf, sizeof (cwdbuf))) {
		    setError("could not %s directory",
		       (c ? "print new" : "establish current"));
		    return false;
		}
		puts(cwdbuf);
		return true;
	    }
	    if(!envFile(t, &t))
		return false;
	    if(!chdir(t))
		goto pwd;
	    setError("invalid directory");
	    return false;
	}
    }

    if(line[0] == 'p' && line[1] == 'b') {
	c = line[2];
	if(!c || c == '.') {
	    const struct MIMETYPE *mt;
	    char *cmd;
	    const char *suffix = 0;
	    bool trailPercent = false;
	    if(!cw->dol) {
		setError("cannot play an empty buffer");
		return false;
	    }
	    if(c == '.') {
		suffix = line + 3;
	    } else {
		if(cw->fileName)
		    suffix = strrchr(cw->fileName, '.');
		if(!suffix) {
		    setError
		       ("file has no suffix, use mt.xxx to specify your own suffix");
		    return false;
		}
		++suffix;
	    }
	    if(strlen(suffix) > 5) {
		setError("suffix is limited to 5 characters");
		return false;
	    }
	    mt = findMimeBySuffix(suffix);
	    if(!mt) {
		setError
		   ("suffix .%s is not a recognized mime type, please check your config file.",
		   suffix);
		return false;
	    }
	    if(mt->program[strlen(mt->program) - 1] == '%')
		trailPercent = true;
	    cmd = pluginCommand(mt, 0, suffix);
	    rc = bufferToProgram(cmd, suffix, trailPercent);
	    nzFree(cmd);
	    return rc;
	}
    }

    if(stringEqual(line, "rf")) {
	cmd = 'e';
	if(!cw->fileName) {
	    setError("no file name or url to refresh");
	    return false;
	}
	if(cw->browseMode)
	    cmd = 'b';
	noStack = true;
	sprintf(newline, "%c %s", cmd, cw->fileName);
	debrowseSuffix(newline);
	return 2;
    }
    /* rf */
    if(stringEqual(line, "ub") || stringEqual(line, "et")) {
	bool ub = (line[0] == 'u');
	cmd = 'e';
	if(!cw->browseMode) {
	    setError("not in browse mode");
	    return false;
	}
	freeUndoLines(cw->map);
	undoWindow.map = 0;
	nzFree(preWindow.map);
	preWindow.map = 0;
	cw->firstOpMode = undoable = false;
	cw->browseMode = false;
	cw->iplist = 0;
	if(ub) {
	    debrowseSuffix(cw->fileName);
	    cw->nlMode = cw->rnlMode;
	    cw->dot = cw->r_dot, cw->dol = cw->r_dol;
	    memcpy(cw->labels, cw->r_labels, sizeof (cw->labels));
	    freeWindowLines(cw->map);
	    cw->map = cw->r_map;
	} else {
	    for(i = 1; i <= cw->dol; ++i) {
		int ln = atoi(cw->map + i * LNWIDTH);
		removeHiddenNumbers(textLines[ln]);
	    }
	    freeWindowLines(cw->r_map);
	}
	cw->r_map = 0;
	freeTags(cw->tags);
	cw->tags = 0;
	freeJavaContext(cw->jsc);
	cw->jsc = 0;
	nzFree(cw->dw);
	cw->dw = 0;
	nzFree(cw->ft);
	cw->ft = 0;
	nzFree(cw->fd);
	cw->fd = 0;
	nzFree(cw->fk);
	cw->fk = 0;
	if(ub)
	    fileSize = apparentSize(context, false);
	return true;
    }
    /* ub */
    if(stringEqual(line, "ip")) {
	sethostent(1);
	allIPs();
	endhostent();
	if(!cw->iplist || cw->iplist[0] == -1) {
	    puts("none");
	} else {
	    long ip;
	    for(i = 0; (ip = cw->iplist[i]) != -1; ++i) {
		puts(tcp_ip_dots(ip));
	    }
	}
	return true;
    }
    /* ip */
    if(stringEqual(line, "f/") || stringEqual(line, "w/")) {
	char *t;
	cmd = line[0];
	if(!cw->fileName) {
	    setError("no file name or url to refresh");
	    return false;
	}
	t = strrchr(cw->fileName, '/');
	if(!t) {
	    setError("file name does not contain /");
	    return false;
	}
	++t;
	if(!*t) {
	    setError("file name ends in /");
	    return false;
	}
	sprintf(newline, "%c `%s", cmd, t);
	return 2;
    }

    if(line[0] == 'f' && line[2] == 0 &&
       (line[1] == 'd' || line[1] == 'k' || line[1] == 't')) {
	const char *s, *t;
	cmd = 'e';
	if(!cw->browseMode) {
	    setError("not in browse mode");
	    return false;
	}
	if(line[1] == 't')
	    s = cw->ft, t = "title";
	if(line[1] == 'd')
	    s = cw->fd, t = "description";
	if(line[1] == 'k')
	    s = cw->fk, t = "keywords";
	if(s)
	    puts(s);
	else
	    printf("no %s\n", t);
	return true;
    }

    if(line[0] == 's' && line[1] == 'm') {
	const char *t = line + 2;
	bool dosig = true;
	int account = 0;
	cmd = 'e';
	if(*t == '-') {
	    dosig = false;
	    ++t;
	}
	if(isdigit(*t))
	    account = strtol(t, (char **)&t, 10);
	if(!*t) {
/* send mail */
	    return sendMailCurrent(account, dosig);
	} else {
	    setError("invalid characters after the sm command");
	    return false;
	}
    }

    if(stringEqual(line, "sg")) {
	searchStringsAll = true;
	if(helpMessagesOn)
	    puts("substitutions global");
	return true;
    }

    if(stringEqual(line, "sl")) {
	searchStringsAll = false;
	if(helpMessagesOn)
	    puts("substitutions local");
	return true;
    }

    if(stringEqual(line, "ci")) {
	caseInsensitive = true;
	if(helpMessagesOn)
	    puts("case insensitive");
	return true;
    }

    if(stringEqual(line, "cs")) {
	caseInsensitive = false;
	if(helpMessagesOn)
	    puts("case sensitive");
	return true;
    }

    if(stringEqual(line, "dr")) {
	dirWrite = 0;
	if(helpMessagesOn)
	    puts("directories readonly");
	return true;
    }

    if(stringEqual(line, "dw")) {
	dirWrite = 1;
	if(helpMessagesOn)
	    puts("directories writable");
	return true;
    }

    if(stringEqual(line, "dx")) {
	dirWrite = 2;
	if(helpMessagesOn)
	    puts("directories writable with delete");
	return true;
    }

    if(stringEqual(line, "hr")) {
	allowRedirection ^= 1;
	if(helpMessagesOn || debugLevel >= 1)
	    puts(allowRedirection ? "http redirection" : "no http redirection");
	return true;
    }

    if(stringEqual(line, "sr")) {
	sendReferrer ^= 1;
	if(helpMessagesOn || debugLevel >= 1)
	    puts(sendReferrer ? "send referrer" : "do not send referrer");
	return true;
    }

    if(stringEqual(line, "js")) {
	allowJS ^= 1;
	if(helpMessagesOn || debugLevel >= 1)
	    puts(allowJS ? "javascript enabled" : "javascript disabled");
	return true;
    }

    if(stringEqual(line, "bd")) {
	binaryDetect ^= 1;
	if(helpMessagesOn || debugLevel >= 1)
	    puts(binaryDetect ? "watching for binary files" :
	       "treating binary like text");
	return true;
    }

    if(line[0] == 'f' && line[1] == 'm' &&
       line[2] && strchr("pad", line[2]) && !line[3]) {
	ftpMode = 0;
	if(line[2] == 'p')
	    ftpMode = 'F';
	if(line[2] == 'a')
	    ftpMode = 'E';
	if(helpMessagesOn || debugLevel >= 1) {
	    if(ftpMode == 'F')
		puts("passive mode");
	    if(ftpMode == 'E')
		puts("active mode");
	    if(ftpMode == 0)
		puts("passive/active mode");
	}
	return true;
    }
    /* fm */
    if(stringEqual(line, "vs")) {
	verifyCertificates ^= 1;
	if(helpMessagesOn || debugLevel >= 1)
	    puts(verifyCertificates ? "verify ssl connections" :
	       "don't verify ssl connections (less secure)");
	ssl_must_verify(verifyCertificates);
	return true;
    }

    if(stringEqual(line, "hf")) {
	showHiddenFiles ^= 1;
	if(helpMessagesOn || debugLevel >= 1)
	    puts(showHiddenFiles ? "show hidden files in directory mode" :
	       "don't show hidden files in directory mode");
	return true;
    }

    if(stringEqual(line, "tn")) {
	textAreaDosNewlines ^= 1;
	if(helpMessagesOn || debugLevel >= 1)
	    puts(textAreaDosNewlines ? "text areas use dos newlines" :
	       "text areas use unix newlines");
	return true;
    }

    if(stringEqual(line, "eo")) {
	endMarks = 0;
	if(helpMessagesOn)
	    puts("end markers off");
	return true;
    }

    if(stringEqual(line, "el")) {
	endMarks = 1;
	if(helpMessagesOn)
	    puts("end markers on listed lines");
	return true;
    }

    if(stringEqual(line, "ep")) {
	endMarks = 2;
	if(helpMessagesOn)
	    puts("end markers on");
	return true;
    }

    *runThis = line;
    return 2;			/* no change */
}				/* twoLetter */

/* Return the number of unbalanced punctuation marks.
 * This is used by the next routine. */
static void
unbalanced(char c, char d, int ln, int *back_p, int *for_p)
{				/* result parameters */
    char *t, *open;
    char *p = (char *)fetchLine(ln, 1);
    bool change;
    int backward, forward;

    change = true;
    while(change) {
	change = false;
	open = 0;
	for(t = p; *t != '\n'; ++t) {
	    if(*t == c)
		open = t;
	    if(*t == d && open) {
		*open = 0;
		*t = 0;
		change = true;
		open = 0;
	    }
	}
    }

    backward = forward = 0;
    for(t = p; *t != '\n'; ++t) {
	if(*t == c)
	    ++forward;
	if(*t == d)
	    ++backward;
    }

    free(p);
    *back_p = backward;
    *for_p = forward;
}				/* unbalanced */

/* Find the line that balances the unbalanced punctuation. */
static bool
balanceLine(const char *line)
{
    char c, d;			/* open and close */
    char selected;
    static char openlist[] = "{([<`";
    static char closelist[] = "})]>'";
    static char alllist[] = "{}()[]<>`'";
    char *t;
    int level = 0;
    int i, direction, forward, backward;

    if(c = *line) {
	if(!strchr(alllist, c) || line[1]) {
	    setError("you must specify exactly one of %s after the B command",
	       alllist);
	    return false;
	}
	if(t = strchr(openlist, c)) {
	    d = closelist[t - openlist];
	    direction = 1;
	} else {
	    d = c;
	    t = strchr(closelist, d);
	    c = openlist[t - closelist];
	    direction = -1;
	}
	unbalanced(c, d, endRange, &backward, &forward);
	if(direction > 0) {
	    if((level = forward) == 0) {
		setError("line does not contain an open %c", c);
		return false;
	    }
	} else {
	    if((level = backward) == 0) {
		setError("line does not contain an open %c", d);
		return false;
	    }
	}
    } else {

/* Look for anything unbalanced, probably a brace. */
	for(i = 0; i <= 2; ++i) {
	    c = openlist[i];
	    d = closelist[i];
	    unbalanced(c, d, endRange, &backward, &forward);
	    if(backward && forward) {
		setError
		   ("both %c and %c are unbalanced on this line, try B%c or B%c",
		   c, d, c, d);
		return false;
	    }
	    level = backward + forward;
	    if(!level)
		continue;
	    direction = 1;
	    if(backward)
		direction = -1;
	    break;
	}
	if(!level) {
	    setError
	       ("line does not contain an unbalanced brace, parenthesis, or bracket");
	    return false;
	}
    }				/* explicit character passed in, or look for one */

    selected = (direction > 0 ? c : d);

/* search for the balancing line */
    i = endRange;
    while((i += direction) > 0 && i <= cw->dol) {
	unbalanced(c, d, i, &backward, &forward);
	if(direction > 0 && backward >= level ||
	   direction < 0 && forward >= level) {
	    cw->dot = i;
	    printDot();
	    return true;
	}
	level += (forward - backward) * direction;
    }				/* loop over lines */

    setError("cannot find the line that balances %c", selected);
    return false;
}				/* balanceLine */

/* Unfold the buffer into one long, allocated string. */
bool
unfoldBuffer(int cx, bool cr, char **data, int *len)
{
    char *buf;
    int l, ln;
    struct ebWindow *w;
    int size = apparentSize(cx, false);
    if(size < 0)
	return false;
    w = sessionList[cx].lw;
    if(w->browseMode) {
	setError("session %d is currently in browse mode", cx);
	return false;
    }
    if(w->dirMode) {
	setError("session %d is currently in directory mode", cx);
	return false;
    }
    if(cr)
	size += w->dol;
/* a few bytes more, just for safety */
    buf = allocMem(size + 4);
    *data = buf;
    for(ln = 1; ln <= w->dol; ++ln) {
	pst line = fetchLineContext(ln, -1, cx);
	l = pstLength(line) - 1;
	if(l) {
	    memcpy(buf, line, l);
	    buf += l;
	}
	if(cr) {
	    *buf++ = '\r';
	    if(l && buf[-2] == '\r')
		--buf, --size;
	}
	*buf++ = '\n';
    }				/* loop over lines */
    if(w->dol && w->nlMode) {
	if(cr)
	    --size;
    }
    *len = size;
    (*data)[size] = 0;
    return true;
}				/* unfoldBuffer */

static char *
showLinks(void)
{
    int a_l;
    char *a = initString(&a_l);
    bool click, dclick;
    char c, *p, *s, *t, *q, *line, *h;
    int j, k = 0, tagno;
    void *ev;

    if(cw->browseMode && endRange) {
	jMyContext();
	line = (char *)fetchLine(endRange, -1);
	for(p = line; (c = *p) != '\n'; ++p) {
	    if(c != (char)InternalCodeChar)
		continue;
	    if(!isdigit(p[1]))
		continue;
	    j = strtol(p + 1, &s, 10);
	    if(*s != '{')
		continue;
	    p = s;
	    ++k;
	    findField(line, 0, k, 0, &tagno, &h, &ev);
	    if(tagno != j)
		continue;	/* should never happen */

	    jMyContext();
	    click = tagHandler(tagno, "onclick");
	    dclick = tagHandler(tagno, "ondblclick");

/* find the closing brace */
/* It might not be there, could be on the next line. */
	    for(s = p + 1; (c = *s) != '\n'; ++s)
		if(c == (char)InternalCodeChar && s[1] == '0' && s[2] == '}')
		    break;
/* Ok, everything between p and s exclusive is the description */
	    if(!h)
		h = EMPTYSTRING;
	    if(stringEqual(h, "#")) {
		nzFree(h);
		h = EMPTYSTRING;
	    }

	    if(memEqualCI(h, "mailto:", 7)) {
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
	    stringAndString(&a, &a_l, "<a href=");

	    if(memEqualCI(h, "javascript:", 11)) {
		stringAndString(&a, &a_l, "javascript:>\n");
	    } else if(!*h && (click | dclick)) {
		char buf[20];
		sprintf(buf, "%s>\n", click ? "onclick" : "ondblclick");
		stringAndString(&a, &a_l, buf);
	    } else {
		if(*h)
		    stringAndString(&a, &a_l, h);
		stringAndString(&a, &a_l, ">\n");
	    }

	    nzFree(h);
/* next line is the description of the bookmark */
	    stringAndBytes(&a, &a_l, p + 1, s - p - 1);
	    stringAndString(&a, &a_l, "\n</a>\n");
	}			/* loop looking for hyperlinks */
    }
    /* browse mode */
    if(!a_l) {			/* nothing found yet */
	if(!cw->fileName) {
	    setError("no file name");
	    return 0;
	}
	h = cloneString(cw->fileName);
	debrowseSuffix(h);
	stringAndString(&a, &a_l, "<a href=");
	stringAndString(&a, &a_l, h);
	stringAndString(&a, &a_l, ">\n");
	s = (char *)getDataURL(h);
	if(!s || !*s)
	    s = h;
	t = s + strcspn(s, "\1?#");
	if(t > s && t[-1] == '/')
	    --t;
	*t = 0;
	q = strrchr(s, '/');
	if(q && q < t)
	    s = q + 1;
	stringAndBytes(&a, &a_l, s, t - s);
	stringAndString(&a, &a_l, "\n</a>\n");
	nzFree(h);
    }
    /* using the filename */
    return a;
}				/* showLinks */

/* Run the entered edbrowse command.
 * This is indirectly recursive, as in g/x/d
 * Pass in the ed command, and return success or failure.
 * We assume it has been turned into a C string.
 * This means no embeded nulls.
 * If you want to use null in a search or substitute, use \0. */
bool
runCommand(const char *line)
{
    int i, j, n, writeMode;
    struct ebWindow *w;
    void *ev;			/* event variables */
    bool nogo, rc;
    bool postSpace = false, didRange = false;
    char first;
    int cx = 0;			/* numeric suffix as in s/x/y/3 or w2 */
    int tagno;
    const char *s;
    static char newline[MAXTTYLINE];
    static char *allocatedLine = 0;

    if(allocatedLine) {
	nzFree(allocatedLine);
	allocatedLine = 0;
    }
    nzFree(currentReferrer);
    currentReferrer = cloneString(cw->fileName);
    js_redirects = false;

    cmd = icmd = 'p';
    skipWhite(&line);
    first = *line;

    if(!globSub) {
/* Allow things like comment, or shell escape, but not if we're
 * in the midst of a global substitute, as in g/x/ !echo hello world */
	if(first == '#')
	    return true;
	if(first == '!')
	    return shellEscape(line + 1);

/* Watch for successive q commands. */
	lastq = lastqq, lastqq = 0;
	noStack = false;

/* special 2 letter commands - most of these change operational modes */
	j = twoLetter(line, &line);
	if(j != 2)
	    return j;
    }
    /* not global command */
    startRange = endRange = cw->dot;	/* default range */
/* Just hit return to read the next line. */
    first = *line;
    if(first == 0) {
	didRange = true;
	++startRange, ++endRange;
	if(endRange > cw->dol) {
	    setError("end of buffer");
	    return false;
	}
    }
    if(first == ',') {
	didRange = true;
	++line;
	startRange = 1;
	if(cw->dol == 0)
	    startRange = 0;
	endRange = cw->dol;
    }
    if(first == ';') {
	didRange = true;
	++line;
	startRange = cw->dot;
	endRange = cw->dol;
    }
    if(first == 'j' || first == 'J') {
	didRange = true;
	endRange = startRange + 1;
	if(endRange > cw->dol) {
	    setError("no more lines to join");
	    return false;
	}
    }
    if(first == '=') {
	didRange = true;
	startRange = endRange = cw->dol;
    }
    if(first == 'w' || first == 'v' || first == 'g' &&
       line[1] && strchr(valid_delim, line[1])) {
	didRange = true;
	startRange = 1;
	if(cw->dol == 0)
	    startRange = 0;
	endRange = cw->dol;
    }

    if(!didRange) {
	if(!getRangePart(line, &startRange, &line))
	    return (globSub = false);
	endRange = startRange;
	if(line[0] == ',') {
	    ++line;
	    endRange = cw->dol;	/* new default */
	    first = *line;
	    if(first && strchr(valid_laddr, first)) {
		if(!getRangePart(line, &endRange, &line))
		    return (globSub = false);
	    }
	}
    }
    if(endRange < startRange) {
	setError("bad range");
	return false;
    }

/* change uc into a substitute command, converting the whole line */
    skipWhite(&line);
    first = *line;
    if((first == 'u' || first == 'l' || first == 'm') && line[1] == 'c' &&
       line[2] == 0) {
	sprintf(newline, "s/.*/%cc/", first);
	line = newline;
    }
/* Breakline is actually a substitution of lines. */
    if(stringEqual(line, "bl")) {
	if(cw->dirMode) {
	    setError("cannot break lines in directory mode");
	    return false;
	}
	if(cw->browseMode) {
	    setError("cannot break lines in browse mode");
	    return false;
	}
	line = "s`bl";
    }

/* get the command */
    cmd = *line;
    if(cmd)
	++line;
    else
	cmd = 'p';
    icmd = cmd;

    if(!strchr(valid_cmd, cmd)) {
	setError("unknown command %c", cmd);
	return (globSub = false);
    }

    first = *line;
    writeMode = O_TRUNC;
    if(cmd == 'w' && first == '+')
	writeMode = O_APPEND, first = *++line;

    if(cw->dirMode && !strchr(dir_cmd, cmd)) {
	setError("%c not available in directory mode", icmd);
	return (globSub = false);
    }
    if(cw->browseMode && !strchr(browse_cmd, cmd)) {
	setError("%c not available in browse mode", icmd);
	return (globSub = false);
    }
    if(startRange == 0 && !strchr(zero_cmd, cmd)) {
	setError("zero line number");
	return (globSub = false);
    }
    while(isspace(first))
	postSpace = true, first = *++line;
    if(strchr(spaceplus_cmd, cmd) && !postSpace && first) {
	s = line;
	while(isdigit(*s))
	    ++s;
	if(*s) {
	    setError("no space after command");
	    return (globSub = false);
	}
    }
    if(globSub && !strchr(global_cmd, cmd)) {
	setError("the %c command cannot be applied globally", icmd);
	return (globSub = false);
    }

/* move/copy destination, the third address */
    if(cmd == 't' || cmd == 'm') {
	if(!first) {
	    destLine = cw->dot;
	} else {
	    if(!strchr(valid_laddr, first)) {
		setError("invalid move/copy destination");
		return (globSub = false);
	    }
	    if(!getRangePart(line, &destLine, &line))
		return (globSub = false);
	    first = *line;
	}			/* was there something after m or t */
    }

    /* m or t */
    /* Any command other than a lone b resets history */
    if(cmd != 'b' || first) {
	fetchHistory(0, 0);	/* reset history */
    }

/* env variable and wild card expansion */
    if(strchr("brewf", cmd) && first && !isURL(line)) {
	if(!envFile(line, &line))
	    return false;
	first = *line;
    }

    if(cmd == 'z') {
	if(isdigit(first)) {
	    last_z = strtol(line, (char **)&line, 10);
	    if(!last_z)
		last_z = 1;
	    first = *line;
	}
	startRange = endRange + 1;
	endRange = startRange;
	if(startRange > cw->dol) {
	    setError("line number too large");
	    return false;
	}
	cmd = 'p';
	endRange += last_z - 1;
	if(endRange > cw->dol)
	    endRange = cw->dol;
    }

    /* z */
    /* the a+ feature, when you thought you were in append mode */
    if(cmd == 'a') {
	if(stringEqual(line, "+"))
	    ++line, first = 0;
	else
	    linePending[0] = 0;
    } else
	linePending[0] = 0;

    if(first && strchr(nofollow_cmd, cmd)) {
	setError("unexpected text after the %c command", icmd);
	return (globSub = false);
    }

    if(cmd == 'h') {
	showError();
	return true;
    }
    /* h */
    if(cmd == 'H') {
	if(helpMessagesOn ^= 1)
	    debugPrint(1, "help messages on");
	return true;
    }
    /* H */
    if(strchr("lpn", cmd)) {
	for(i = startRange; i <= endRange; ++i) {
	    displayLine(i);
	    cw->dot = i;
	    if(intFlag)
		break;
	}
	return true;
    }
    /* lpn */
    if(cmd == '=') {
	printf("%d\n", endRange);
	return true;
    }
    /* = */
    if(cmd == 'B') {
	return balanceLine(line);
    }
    /* B */
    if(cmd == 'u') {
	struct ebWindow *uw = &undoWindow;
	char *swapmap;
	if(!cw->firstOpMode) {
	    setError("nothing to undo");
	    return false;
	}
/* swap, so we can undo our undo, if need be */
	i = uw->dot, uw->dot = cw->dot, cw->dot = i;
	i = uw->dol, uw->dol = cw->dol, cw->dol = i;
	for(j = 0; j < 26; ++j) {
	    i = uw->labels[j], uw->labels[j] = cw->labels[j], cw->labels[j] = i;
	}
	swapmap = uw->map, uw->map = cw->map, cw->map = swapmap;
	return true;
    }
    /* u */
    if(cmd == 'k') {
	if(!islower(first) || line[1]) {
	    setError("please enter k[a-z]");
	    return false;
	}
	if(startRange < endRange) {
	    setError("cannot label an entire range");
	    return false;
	}
	cw->labels[first - 'a'] = endRange;
	return true;
    }

    /* k */
    /* Find suffix, as in 27,59w2 */
    if(!postSpace) {
	cx = stringIsNum(line);
	if(!cx) {
	    setError("%s 0 is invalid", cmd == '^' ? "backing up" : "session");
	    return false;
	}
	if(cx < 0)
	    cx = 0;
    }

    if(cmd == 'q') {
	if(cx) {
	    if(!cxCompare(cx))
		return false;
	    if(!cxActive(cx))
		return false;
	} else {
	    cx = context;
	    if(first) {
		setError("unexpected text after the q command");
		return false;
	    }
	}
	if(!cxQuit(cx, 2))
	    return false;
	if(cx != context)
	    return true;
/* look around for another active session */
	while(true) {
	    if(++cx == MAXSESSION)
		cx = 1;
	    if(cx == context)
		exit(0);
	    if(!sessionList[cx].lw)
		continue;
	    cxSwitch(cx, true);
	    return true;
	}			/* loop over sessions */
    }
    /* q */
    if(cmd == 'f') {
	if(cx) {
	    if(!cxCompare(cx))
		return false;
	    if(!cxActive(cx))
		return false;
	    s = sessionList[cx].lw->fileName;
	    printf("%s", s ? s : "no file");
	    if(sessionList[cx].lw->binMode)
		printf(" [binary]");
	    printf("\n");
	    return true;
	}			/* another session */
	if(first) {
	    if(cw->dirMode) {
		setError("cannot change the name of a directory");
		return false;
	    }
	    nzFree(cw->fileName);
	    cw->fileName = cloneString(line);
	}
	s = cw->fileName;
	printf("%s", s ? s : "no file");
	if(cw->binMode)
	    printf(" [binary]");
	printf("\n");
	return true;
    }
    /* f */
    if(cmd == 'w') {
	if(cx) {		/* write to another buffer */
	    if(writeMode == O_APPEND) {
		setError("cannot append to another buffer");
		return false;
	    }
	    return writeContext(cx);
	}
	if(!first)
	    line = cw->fileName;
	if(!line) {
	    setError("no file specified");
	    return false;
	}
	if(cw->dirMode && stringEqual(line, cw->fileName)) {
	    setError
	       ("cannot write to the directory; files are modified as you go");
	    return false;
	}
	return writeFile(line, writeMode);
    }
    /* w */
    if(cmd == '^') {		/* back key, pop the stack */
	if(first && !cx) {
	    setError("unexpected text after the ^ command");
	    return false;
	}
	if(!cx)
	    cx = 1;
	while(cx) {
	    struct ebWindow *prev = cw->prev;
	    if(!prev) {
		setError("no previous text");
		return false;
	    }
	    if(!cxQuit(context, 1))
		return false;
	    carrySubstitutionStrings(cw, prev);
	    sessionList[context].lw = cw = prev;
	    --cx;
	}
	printDot();
	return true;
    }
    /* ^ */
    if(cmd == 'M') {		/* move this to another session */
	if(first && !cx) {
	    setError("unexpected text after the M command");
	    return false;
	}
	if(!first) {
	    setError("destination session not specified");
	    return false;
	}
	if(!cw->prev) {
	    setError("no previous text, cannot back up");
	    return false;
	}
	if(!cxCompare(cx))
	    return false;
	if(cxActive(cx) && !cxQuit(cx, 2))
	    return false;
/* Magic with pointers, hang on to your hat. */
	sessionList[cx].fw = sessionList[cx].lw = cw;
	cs->lw = cw->prev;
	cw->prev = 0;
	cw = cs->lw;
	printDot();
	return true;
    }
    /* M */
    if(cmd == 'A') {
	char *a;
	if(!cxQuit(context, 0))
	    return false;
	if(!(a = showLinks()))
	    return false;
	freeUndoLines(cw->map);
	undoWindow.map = 0;
	nzFree(preWindow.map);
	preWindow.map = 0;
	cw->firstOpMode = cw->changeMode = false;
	w = createWindow();
	w->prev = cw;
	cw = w;
	cs->lw = w;
	rc = addTextToBuffer((pst) a, strlen(a), 0);
	nzFree(a);
	undoable = cw->changeMode = false;
	fileSize = apparentSize(context, false);
	return rc;
    }
    /* A */
    if(cmd == '<') {		/* run a function */
	return runEbFunction(line);
    }

    /* < */
    /* go to a file in a directory listing */
    if(cmd == 'g' && cw->dirMode && !first) {
	char *p, *dirline, *endline;
	if(endRange > startRange) {
	    setError("cannot apply the g command to a range");
	    return false;
	}
	p = (char *)fetchLine(endRange, -1);
	j = pstLength((pst) p);
	--j;
	p[j] = 0;		/* temporary */
	dirline = makeAbsPath(p);
	p[j] = '\n';
	cmd = 'e';
	if(!dirline)
	    return false;
/* I don't think we need to make a copy here. */
	line = dirline;
	first = *line;
    }
    /* g in directory mode */
    if(cmd == 'e') {
	if(cx) {
	    if(!cxCompare(cx))
		return false;
	    cxSwitch(cx, true);
	    return true;
	}
	if(!first) {
	    printf("session %d\n", context);
	    return true;
	}
/* more e to come */
    }
    /* e */
    if(cmd == 'g') {		/* see if it's a go command */
	char *p, *h;
	int tagno;
	bool click, dclick, over;
	bool jsh, jsgo, jsdead;
	s = line;
	j = 0;
	if(first)
	    j = strtol(line, (char **)&s, 10);
	if(j >= 0 && (!*s || stringEqual(s, "?"))) {
	    jsh = jsgo = nogo = false;
	    jsdead = cw->jsdead;
	    if(!cw->jsc)
		jsdead = true;
	    cmd = 'b';
	    if(endRange > startRange) {
		setError("cannot apply the g command to a range");
		return false;
	    }
	    p = (char *)fetchLine(endRange, -1);
	    findField(p, 0, j, &n, &tagno, &h, &ev);
	    debugPrint(5, "findField returns %d, %s", tagno, h);
	    if(!h) {
		fieldNumProblem("links", 'g', j, n);
		return false;
	    }
	    jMyContext();
	    jsh = memEqualCI(h, "javascript:", 11);
	    if(tagno) {
		over = tagHandler(tagno, "onmouseover");
		click = tagHandler(tagno, "onclick");
		dclick = tagHandler(tagno, "ondblclick");
	    }
	    if(click)
		jsgo = true;
	    jsgo |= jsh;
	    nogo = stringEqual(h, "#");
	    nogo |= jsh;
	    debugPrint(5, "go%d nogo%d jsh%d dead%d", jsgo, nogo, jsh, jsdead);
	    debugPrint(5, "click %d dclick %d over %d", click, dclick, over);
	    if(jsgo & jsdead) {
		if(nogo)
		    puts("javascript is disabled, no action taken");
		else
		    puts("javascript is disabled, going straight to the url");
		jsgo = false;
	    }
	    line = allocatedLine = h;
	    first = *line;
	    setError(0);
	    rc = false;
	    if(jsgo) {
		jSyncup();
/* The program might depend on the mouseover code running first */
		if(over) {
		    rc = handlerGo(ev, "onmouseover");
		    jsdw();
		    if(newlocation)
			goto redirect;
		}
	    }
/* This is the only handler where false tells the browser to do something else. */
	    if(!rc && !jsdead)
		set_property_string(jwin, "status", h);
	    if(jsgo && click) {
		rc = handlerGo(ev, "onclick");
		jsdw();
		if(newlocation)
		    goto redirect;
		if(!rc)
		    return true;
	    }
	    if(jsh) {
		rc = javaParseExecute(jwin, h, 0, 0);
		jsdw();
		if(newlocation)
		    goto redirect;
		return true;
	    }
	    if(nogo)
		return true;
	}			/* go command */
    }
    /* g */
    if(cmd == 's') {
/* Some shorthand, like s,2 to split the line at the second comma */
	if(!first) {
	    strcpy(newline, "//%");
	    line = newline;
	} else if(strchr(",.;:!?)-\"", first) &&
	   (!line[1] || isdigit(line[1]) && !line[2])) {
	    char esc[2];
	    esc[0] = esc[1] = 0;
	    if(first == '.' || first == '?')
		esc[0] = '\\';
	    sprintf(newline, "/%s%c +/%c\\n%s%s",
	       esc, first, first, (line[1] ? "/" : ""), line + 1);
	    debugPrint(7, "shorthand regexp %s", newline);
	    line = newline;
	}
	first = *line;
    }
    /* s */
    scmd = ' ';
    if((cmd == 'i' || cmd == 's') && first) {
	char c;
	s = line;
	if(isdigit(*s))
	    cx = strtol(s, (char **)&s, 10);
	c = *s;
	if(c && (strchr(valid_delim, c) || cmd == 'i' && strchr("*<?=", c))) {
	    if(!cw->browseMode && (cmd == 'i' || cx)) {
		setError("not in browse mode");
		return false;
	    }
	    if(endRange > startRange && cmd == 'i') {
		setError("cannot apply the i%c command to a range", c);
		return false;
	    }
	    if(cmd == 'i' && strchr("?=<*", c)) {
		char *p;
		scmd = c;
		line = s + 1;
		first = *line;
		debugPrint(5, "scmd = %c", scmd);
		cw->dot = endRange;
		p = (char *)fetchLine(cw->dot, -1);
		j = 1;
		if(scmd == '*')
		    j = 2;
		if(scmd == '?')
		    j = 3;
		findInputField(p, j, cx, &n, &tagno);
		debugPrint(5, "findField returns %d.%d", n, tagno);
		if(!tagno) {
		    fieldNumProblem((c == '*' ? "buttons" : "input fields"),
		       'i', cx, n);
		    return false;
		}
		if(scmd == '?') {
		    infShow(tagno, line);
		    return true;
		}
		if(c == '<') {
		    bool fromfile = false;
		    if(globSub) {
			setError("cannot use i< in a global command");
			return (globSub = false);
		    }
		    skipWhite(&line);
		    if(!*line) {
			setError("no file specified");
			return false;
		    }
		    n = stringIsNum(line);
		    if(n >= 0) {
			char *p;
			int plen, dol;
			if(!cxCompare(n) || !cxActive(n))
			    return false;
			dol = sessionList[n].lw->dol;
			if(!dol) {
			    setError("buffer %d is empty", n);
			    return false;
			}
			if(dol > 1) {
			    setError("buffer %d contains more than one line",
			       n);
			    return false;
			}
			p = (char *)fetchLineContext(1, 1, n);
			plen = pstLength((pst) p);
			if(plen > sizeof (newline))
			    plen = sizeof (newline);
			memcpy(newline, p, plen);
			n = plen;
			nzFree(p);
		    } else {
			int fd;
			fromfile = true;
			if(!envFile(line, &line))
			    return false;
			fd = open(line, O_RDONLY | O_TEXT);
			if(fd < 0) {
			    setError("cannot open %s", line);
			    return false;
			}
			n = read(fd, newline, sizeof (newline));
			close(fd);
			if(n < 0) {
			    setError("cannot read from %s", line);
			    return false;
			}
		    }
		    for(j = 0; j < n; ++j) {
			if(newline[j] == 0) {
			    setError("input text contains nulls", line);
			    return false;
			}
			if(newline[j] == '\r' && !fromfile &&
			   j < n - 1 && newline[j + 1] != '\n') {
			    setError
			       ("line contains an embeded carriage return");
			    return false;
			}
			if(newline[j] == '\r' || newline[j] == '\n')
			    break;
		    }
		    if(j == sizeof (newline)) {
			setError("first line of %s is too long", line);
			return false;
		    }
		    newline[j] = 0;
		    line = newline;
		    scmd = '=';
		}
		if(c == '*') {
		    jSyncup();
		    if(!infPush(tagno, &allocatedLine))
			return false;
		    if(newlocation)
			goto redirect;
/* No url means it was a reset button */
		    if(!allocatedLine)
			return true;
		    line = allocatedLine;
		    first = *line;
		    cmd = 'b';
		}
	    } else
		cmd = 's';
	} else {
	    setError("unexpected text after the %c command", icmd);
	    return false;
	}
    }
    /* s or i */
  rebrowse:
    if(cmd == 'e' || cmd == 'b' && first && first != '#') {
	if(cw->fileName && !noStack && sameURL(line, cw->fileName)) {
	    if(stringEqual(line, cw->fileName)) {
		setError
		   ("file is currently in buffer - please use the rf command to refresh");
		return false;
	    }
/* Same url, but a different #section */
	    s = strchr(line, '#');
	    if(!s) {		/* no section specified */
		cw->dot = 1;
		if(!cw->dol)
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
	if(!cxQuit(context, 0))
	    return false;
	freeUndoLines(cw->map);
	undoWindow.map = 0;
	nzFree(preWindow.map);
	preWindow.map = 0;
	cw->firstOpMode = cw->changeMode = false;
	startRange = endRange = 0;
	changeFileName = 0;	/* should already be zero */
	w = createWindow();
	cw = w;			/* we might wind up putting this back */
/* Check for sendmail link */
	if(cmd == 'b' && memEqualCI(line, "mailto:", 7)) {
	    char *addr, *subj, *body;
	    char *q;
	    int ql;
	    decodeMailURL(line, &addr, &subj, &body);
	    ql = strlen(addr);
	    ql += 4;		/* to:\n */
	    ql += subj ? strlen(subj) : 5;
	    ql += 9;		/* subject:\n */
	    if(body)
		ql += strlen(body);
	    q = allocMem(ql + 1);
	    sprintf(q, "to:%s\nSubject:%s\n%s", addr, subj ? subj : "Hello",
	       body ? body : "");
	    j = addTextToBuffer((pst) q, ql, 0);
	    nzFree(q);
	    nzFree(addr);
	    nzFree(subj);
	    nzFree(body);
	    if(j)
		printf
		   ("SendMail link.  Compose your mail, type sm to send, then ^ to get back.\n");
	} else {
	    w->fileName = cloneString(line);
	    if(icmd == 'g' && !nogo && isURL(cw->fileName))
		debugPrint(2, "*%s", cw->fileName);
	    j = readFile(cw->fileName, "");
	}
	w->firstOpMode = w->changeMode = false;
	undoable = false;
	cw = cs->lw;
/* Don't push a new session if we were trying to read a url,
 * and didn't get anything.  This is a feature that I'm
 * not sure if I really like. */
	if(!serverData && isURL(w->fileName)) {
	    fileSize = -1;
	    freeWindow(w);
	    if(noStack && cw->prev) {
		w = cw;
		cw = w->prev;
		cs->lw = cw;
		freeWindow(w);
	    }
	    return j;
	}
	if(noStack) {
	    w->prev = cw->prev;
	    cxQuit(context, 1);
	} else {
	    w->prev = cw;
	}
	cs->lw = cw = w;
	if(!w->prev)
	    cs->fw = w;
	if(!j)
	    return false;
	if(changeFileName) {
	    nzFree(w->fileName);
	    w->fileName = changeFileName;
	    changeFileName = 0;
	}
/* Some files we just can't browse */
	if(!cw->dol || cw->binMode | cw->dirMode)
	    cmd = 'e';
	if(cmd == 'e')
	    return true;
    }

  browse:
    if(cmd == 'b') {
	if(!cw->browseMode) {
	    if(cw->binMode) {
		setError("cannot browse a binary file");
		return false;
	    }
	    if(cw->dirMode) {
		setError("cannot browse a directory");
		return false;
	    }
	    if(!cw->dol) {
		setError("cannot browse an empty file");
		return false;
	    }
	    if(fileSize >= 0) {
		debugPrint(1, "%d", fileSize);
		fileSize = -1;
	    }
	    if(!browseCurrentBuffer()) {
		if(icmd == 'b') {
		    setError("this doesn't look like browsable text");
		    return false;
		}
		return true;
	    }
	} else if(!first) {
	    setError("already browsing");
	    return false;
	}

	if(newlocation) {
	    if(!refreshDelay(newloc_d, newlocation)) {
		nzFree(newlocation);
		newlocation = 0;
	    } else if(fetchHistory(cw->fileName, newlocation) < 0) {
		nzFree(newlocation);
		newlocation = 0;
		showError();
	    } else {
	      redirect:
		noStack = newloc_rf;
		nzFree(allocatedLine);
		line = allocatedLine = newlocation;
		debugPrint(2, "redirect %s", line);
		newlocation = 0;
		icmd = cmd = 'b';
		first = *line;
		goto rebrowse;
	    }
	}

/* Jump to the #section, if specified in the url */
	s = strchr(line, '#');
	if(!s)
	    return true;
	++s;
/* Sometimes there's a 3 in the midst of a long url,
 * probably with post data.  It really screws things up.
 * Here is a kludge to avoid this problem.
 * Some day I need to figure this out. */
	if(strlen(s) > 24)
	    return true;
/* Print the file size before we print the line. */
	if(fileSize >= 0) {
	    debugPrint(1, "%d", fileSize);
	    fileSize = -1;
	}
	for(i = 1; i <= cw->dol; ++i) {
	    char *p = (char *)fetchLine(i, -1);
	    if(lineHasTag(p, s)) {
		cw->dot = i;
		printDot();
		return true;
	    }
	}
	setError("label %s not found", s);
	return false;
    }
    /* b */
    if(!globSub) {		/* get ready for subsequent undo */
	struct ebWindow *pw = &preWindow;
	pw->dot = cw->dot;
	pw->dol = cw->dol;
	memcpy(pw->labels, cw->labels, 26 * sizeof (int));
	pw->binMode = cw->binMode;
	pw->nlMode = cw->nlMode;
	pw->dirMode = cw->dirMode;
	if(pw->map) {
	    if(!cw->map || !stringEqual(pw->map, cw->map)) {
		free(pw->map);
		pw->map = 0;
	    }
	}
	if(cw->map && !pw->map)
	    pw->map = cloneString(cw->map);
    }
    /* data saved for undo */
    if(cmd == 'g' || cmd == 'v') {
	return doGlobal(line);
    }
    /* g or v */
    if(cmd == 'm' || cmd == 't') {
	return moveCopy();
    }
    /* m or t */
    if(cmd == 'i') {
	if(scmd == '=') {
	    rc = infReplace(tagno, line, 1);
	    if(newlocation)
		goto redirect;
	    return rc;
	}
	if(cw->browseMode) {
	    setError("i not available in browse mode");
	    return false;
	}
	cmd = 'a';
	--startRange, --endRange;
    }
    /* i */
    if(cmd == 'c') {
	delText(startRange, endRange);
	endRange = --startRange;
	cmd = 'a';
    }
    /* c */
    if(cmd == 'a') {
	if(inscript) {
	    setError("cannot run an insert command from an edbrowse function");
	    return false;
	}
	return inputLinesIntoBuffer();
    }
    /* a */
    if(cmd == 'd') {
	if(cw->dirMode) {
	    j = delFiles();
	    if(!j)
		globSub = false;
	    return j;
	}
	if(endRange == cw->dol)
	    cw->nlMode = false;
	delText(startRange, endRange);
	return true;
    }
    /* d */
    if(cmd == 'j' || cmd == 'J') {
	return joinText();
    }
    /* j */
    if(cmd == 'r') {
	if(cx)
	    return readContext(cx);
	if(first) {
	    j = readFile(line, "");
	    if(!serverData)
		fileSize = -1;
	    return j;
	}
	setError("no file specified");
	return false;
    }
    /* r */
    if(cmd == 's') {
	j = substituteText(line);
	if(j < 0) {
	    globSub = false;
	    j = false;
	}
	if(newlocation)
	    goto redirect;
	return j;
    }
    /* s */
    setError("command %c not yet implemented", icmd);
    return (globSub = false);
}				/* runCommand */

bool
edbrowseCommand(const char *line, bool script)
{
    bool rc;
    globSub = intFlag = false;
    inscript = script;
    fileSize = -1;
    skipWhite(&line);
    rc = runCommand(line);
    if(fileSize >= 0)
	debugPrint(1, "%d", fileSize);
    fileSize = -1;
    if(!rc) {
	if(!script)
	    showErrorConditional(cmd);
	eeCheck();
    }
    if(undoable) {
	struct ebWindow *pw = &preWindow;
	struct ebWindow *uw = &undoWindow;
	debugPrint(6, "undoable");
	uw->dot = pw->dot;
	uw->dol = pw->dol;
	if(uw->map && pw->map && stringEqual(uw->map, pw->map)) {
	    free(pw->map);
	} else {
	    debugPrint(6, "success freeUndo");
	    freeUndoLines(pw->map);
	    uw->map = pw->map;
	}
	pw->map = 0;
	memcpy(uw->labels, pw->labels, 26 * sizeof (int));
	uw->binMode = pw->binMode;
	uw->nlMode = pw->nlMode;
	uw->dirMode = pw->dirMode;
	undoable = false;
    }
    return rc;
}				/* edbrowseCommand */

/* Take some text, usually empty, and put it in a side buffer. */
int
sideBuffer(int cx, const char *text, const char *bufname, bool autobrowse)
{
    int svcx = context;
    bool rc;
    if(cx) {
	cxQuit(cx, 2);
    } else {
	for(cx = 1; cx < MAXSESSION; ++cx)
	    if(!sessionList[cx].lw)
		break;
	if(cx == MAXSESSION) {
	    debugPrint(0,
	       "warning: no buffers available to handle the ancillary window");
	    return 0;
	}
    }
    cxSwitch(cx, false);
    if(bufname) {
	cw->fileName = cloneString(bufname);
	debrowseSuffix(cw->fileName);
    }
    if(*text) {
	rc = addTextToBuffer((pst) text, strlen(text), 0);
	if(!rc)
	    debugPrint(0,
	       "warning: could not preload <buffer %d> with its initial text",
	       cx);
	if(autobrowse) {
/* This is html; we need to render it.
 * I'm disabling javascript in this window.
 * Why?
 * Because this window is being created by javascript,
 * and if we call more javascript, well, I don't think
 * any of that code is reentrant.
 * Smells like a disaster in the making. */
	    allowJS = false;
	    browseCurrentBuffer();
	    allowJS = true;
	}			/* browse the side window */
    }
    /* put text in the side window */
    /* back to original context */
    cxSwitch(svcx, false);
    return cx;
}				/* sideBuffer */

/* Bailing wire and duct tape, here it comes.
 * You'd never know I was a professional programmer.  :-)
 * Ok, the text buffer, that you edit, is the final arbiter on most
 * (but not all) input fields.  And when javascript changes
 * one of these values, it is copied back to the text buffer,
 * where you can see it, and modify it as you like.
 * But what happens if that javascript is running while the html is parsed,
 * and before the page even exists?
 * I hadn't thought of that.
 * The following is a cache of input fields that have been changed,
 * before we even had a chance to render the page. */

struct inputChange {
    struct inputChange *next, *prev;
    int tagno;
    char value[4];
};
static struct listHead inputChangesPending = {
    &inputChangesPending, &inputChangesPending
};
static struct inputChange *ic;

bool
browseCurrentBuffer(void)
{
    char *rawbuf, *newbuf;
    int rawsize, j;
    bool rc, remote = false, do_ip = false;
    bool save_ch = cw->changeMode;
    uchar bmode = 0;

    if(cw->fileName)
	remote = isURL(cw->fileName);
/* A mail message often contains lots of html tags,
 * so we need to check for email headers first. */
    if(!remote && emailTest())
	bmode = 1;
    else if(htmlTest())
	bmode = 2;
    else
	return false;

    if(!unfoldBuffer(context, false, &rawbuf, &rawsize))
	return false;		/* should never happen */
    prepareForBrowse(rawbuf, rawsize);

/* No harm in running this code in mail client, but no help either,
 * and it begs for bugs, so leave it out. */
    if(!ismc) {
	freeUndoLines(cw->map);
	undoWindow.map = 0;
	nzFree(preWindow.map);
	preWindow.map = 0;
	cw->firstOpMode = false;

/* There shouldn't be anything in the input pending list, but clear
 * it out, just to be safe. */
	freeList(&inputChangesPending);
    }

    if(bmode == 1) {
	newbuf = emailParse(rawbuf);
	do_ip = true;
	if(!ipbFile)
	    do_ip = false;
	if(passMail)
	    do_ip = false;
	if(memEqualCI(newbuf, "<html>\n", 7)) {
/* double browse, mail then html */
	    bmode = 2;
	    rawbuf = newbuf;
	    rawsize = strlen(rawbuf);
	    prepareForBrowse(rawbuf, rawsize);
	}
    }

    if(bmode == 2) {
	cw->jsdead = !javaOK(cw->fileName);
	if(!cw->jsdead)
	    cw->jsc = createJavaContext();
	nzFree(newlocation);	/* should already be 0 */
	newlocation = 0;
	newbuf = htmlParse(rawbuf, remote);
    }

    cw->browseMode = true;
    cw->rnlMode = cw->nlMode;
    cw->nlMode = false;
    cw->r_dot = cw->dot, cw->r_dol = cw->dol;
    cw->dot = cw->dol = 0;
    cw->r_map = cw->map;
    cw->map = 0;
    memcpy(cw->r_labels, cw->labels, sizeof (cw->labels));
    memset(cw->labels, 0, sizeof (cw->labels));
    j = strlen(newbuf);
    rc = addTextToBuffer((pst) newbuf, j, 0);
    free(newbuf);
    cw->firstOpMode = undoable = false;
    cw->changeMode = save_ch;
    cw->iplist = 0;

    if(cw->fileName) {
	j = strlen(cw->fileName);
	cw->fileName = reallocMem(cw->fileName, j + 8);
	strcat(cw->fileName, ".browse");
    }
    if(!rc) {
	fileSize = -1;
	return false;
    }				/* should never happen */
    fileSize = apparentSize(context, true);

    if(bmode == 2) {
/* apply any input changes pending */
	foreach(ic, inputChangesPending)
	   updateFieldInBuffer(ic->tagno, ic->value, 0, false);
	freeList(&inputChangesPending);
    }

    if(do_ip & ismc)
	allIPs();
    return true;
}				/* browseCurrentBuffer */

static bool
locateTagInBuffer(int tagno, int *ln_p, char **p_p, char **s_p, char **t_p)
{
    int ln, n;
    char *p, *s, *t, c;
    char search[20];

    if(parsePage) {
	foreachback(ic, inputChangesPending) {
	    if(ic->tagno != tagno)
		continue;
	    *s_p = ic->value;
	    *t_p = ic->value + strlen(ic->value);
/* we don't need to set the others in this special case */
	    return true;
	}
	return false;
    }
    /* still rendering the page */
    sprintf(search, "%c%d<", InternalCodeChar, tagno);
    n = strlen(search);
    for(ln = 1; ln <= cw->dol; ++ln) {
	p = (char *)fetchLine(ln, -1);
	for(s = p; (c = *s) != '\n'; ++s) {
	    if(c != (char)InternalCodeChar)
		continue;
	    if(!memcmp(s, search, n))
		break;
	}
	if(c == '\n')
	    continue;		/* not here, try next line */
	s = strchr(s, '<') + 1;
	t = strstr(s, "\2000>");
	if(!t)
	    errorPrint("@no closing > at line %d", ln);
	*ln_p = ln;
	*p_p = p;
	*s_p = s;
	*t_p = t;
	return true;
    }

    return false;
}				/* locateTagInBuffer */

/* Update an input field in the current buffer.
 * The input field may not be here, if you've deleted some lines. */
void
updateFieldInBuffer(int tagno, const char *newtext, int notify, bool required)
{
    int ln, idx, n, plen;
    char *p, *s, *t, *new;
    char newidx[LNWIDTH + 1];

    if(parsePage) {		/* we don't even have the buffer yet */
	ic = allocMem(sizeof (struct inputChange) + strlen(newtext));
	ic->tagno = tagno;
	strcpy(ic->value, newtext);
	addToListBack(&inputChangesPending, ic);
	return;
    }
    /* still rendering the page */
    if(locateTagInBuffer(tagno, &ln, &p, &s, &t)) {
	n = (plen = pstLength((pst) p)) + strlen(newtext) - (t - s);
	new = allocMem(n);
	memcpy(new, p, s - p);
	strcpy(new + (s - p), newtext);
	memcpy(new + strlen(new), t, plen - (t - p));
	idx = textLinesCount++;
	sprintf(newidx, LNFORMAT, idx);
	memcpy(cw->map + ln * LNWIDTH, newidx, LNWIDTH);
	textLines[idx] = (pst) new;
/* In case a javascript routine updates a field that you weren't expecting */
	if(notify == 1)
	    displayLine(ln);
	if(notify == 2)
	    printf("line %d has been updated\n", ln);
	cw->firstOpMode = undoable = true;
	return;
    }
    /* tag found */
    if(required)
	errorPrint("fieldInBuffer could not find tag %d newtext %s", tagno,
	   newtext);
}				/* updateFieldInBuffer */

/* This is the inverse of the above function, fetch instead of update. */
char *
getFieldFromBuffer(int tagno)
{
    int ln;
    char *p, *s, *t;
    if(locateTagInBuffer(tagno, &ln, &p, &s, &t)) {
	return pullString1(s, t);
    }
    /* tag found */
    /* line has been deleted, revert to the reset value */
    return 0;
}				/* getFieldFromBuffer */

int
fieldIsChecked(int tagno)
{
    int ln;
    char *p, *s, *t, *new;
    if(locateTagInBuffer(tagno, &ln, &p, &s, &t))
	return (*s == '+');
    return -1;
}				/* fieldIsChecked */
