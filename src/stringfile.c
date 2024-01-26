/* stringfile.c: manage strings, files, and directories for edbrowse. */

#include "eb.h"

#include <dirent.h>
#include <termios.h>
#include <glob.h>
#include <pwd.h>
#include <grp.h>
#include <utime.h>

char emptyString[] = "";
bool showHiddenFiles, isInteractive;
int debugLevel = 1;
FILE *debugFile = NULL;
char *debugFileName;
bool debugClone, debugEvent, debugThrow, debugCSS;
bool debugLayout;
bool demin = false;
bool gotimers = true;
bool uvw;
int timerspeed = 1;
long long fileSize;
char *downDir, *down_prefile, *home;
bool endMarks;		// ^ $ on listed lines
uchar dirWrite;		// directories read write
bool dno; // directory names only
char *recycleBin;
uchar ls_sort;		// sort method for directory listing
bool ls_reverse;		// reverse sort
char lsformat[12];	// size date etc on a directory listing

/*********************************************************************
Allocate and manage memory.
Allocate and copy strings.
If we're out of memory, the program aborts.  No error legs.
Soooooo much easier! With 32gb of RAM, we shouldn't run out.
*********************************************************************/

void *allocMem(size_t n)
{
	void *s;
	if (!n)
		return emptyString;
	if (!(s = malloc(n)))
		i_printfExit(MSG_MemAllocError, n);
	return s;
}

void *allocZeroMem(size_t n)
{
	void *s;
	if (!n)
		return emptyString;
	if (!(s = calloc(n, 1)))
		i_printfExit(MSG_MemCallocError, n);
	return s;
}

void *reallocMem(void *p, size_t n)
{
	void *s;
	if (!n)
		i_printfExit(MSG_ReallocP);
	if (!p)
		i_printfExit(MSG_Realloc0, n);
	if (p == emptyString) {
		p = allocMem(n);
// keep the null byte that was present in emptyString.
// fileIntoMemory() needs this to keep null on the end of an empty file
// that was just read into memory.
		*(char *)p = 0;
	}
	if (!(s = realloc(p, n)))
		i_printfExit(MSG_ErrorRealloc, n);
	return s;
}

/* When you know the allocated thing is a string. */
char *allocString(size_t n)
{
	return (char *)allocMem(n);
}

char *allocZeroString(size_t n)
{
	return (char *)allocZeroMem(n);
}

char *reallocString(void *p, size_t n)
{
	return (char *)reallocMem(p, n);
}

void nzFree(void *s)
{
	if (s && s != emptyString)
		free(s);
}

/* some compilers care whether it's void * or const void * */
void cnzFree(const void *v)
{
	nzFree((void *)v);
}

char *appendString(char *s, const char *p)
{
	int slen = strlen(s);
	int plen = strlen(p);
	s = reallocString(s, slen + plen + 1);
	strcpy(s + slen, p);
	return s;
}

char *prependString(char *s, const char *p)
{
	int slen = strlen(s);
	int plen = strlen(p);
	char *t = allocString(slen + plen + 1);
	strcpy(t, p);
	strcpy(t + plen, s);
	nzFree(s);
	return t;
}

void skipWhite(const char **s)
{
	const char *t = *s;
	while (isspaceByte(*t))
		++t;
	*s = t;
}

void trimWhite(char *s)
{
	int l;
	if (!s)
		return;
	l = strlen(s);
	while (l && isspaceByte(s[l - 1]))
		--l;
	s[l] = 0;
}

void stripWhite(char *s)
{
	const char *t = s;
	skipWhite(&t);
	if (t > s)
		strmove(s, t);
	trimWhite(s);
}

/* compress white space */
void spaceCrunch(char *s, bool onespace, bool unprint)
{
	int i, j;
	char c;
	bool space = true;
	for (i = j = 0; (c = s[i]); ++i) {
		if (isspaceByte(c)) {
			if (!onespace)
				continue;
			if (!space)
				s[j++] = ' ', space = true;
			continue;
		}
		if (unprint && !isprintByte(c))
			continue;
		s[j++] = c, space = false;
	}
	if (space && j)
		--j;		/* drop trailing space */
	s[j] = 0;
}

/* Like strcpy, but able to cope with overlapping strings. */
char *strmove(char *dest, const char *src)
{
	return (char *)memmove(dest, src, strlen(src) + 1);
}

/* OO has a lot of unnecessary overhead, and a few inconveniences,
 * but I really miss it right now.  The following
 * routines make up for the lack of simple string concatenation in C.
 * The string space allocated is always a power of 2 - 1, starting with 1.
 * Each of these routines puts an extra 0 on the end of the "string". */

char *initString(int *l)
{
	*l = 0;
	return emptyString;
}

/* String management routines realloc to one less than a power of 2 */
void stringAndString(char **s, int *l, const char *t)
{
	char *p = *s;
	int oldlen, newlen, x;
	oldlen = *l;
	newlen = oldlen + strlen(t);
	*l = newlen;
	++newlen;		/* room for the 0 */
	x = oldlen ^ newlen;
	if (x > oldlen) {	/* must realloc */
		newlen |= (newlen >> 1);
		newlen |= (newlen >> 2);
		newlen |= (newlen >> 4);
		newlen |= (newlen >> 8);
		newlen |= (newlen >> 16);
		p = reallocString(p, newlen);
		*s = p;
	}
	strcpy(p + oldlen, t);
}

void stringAndBytes(char **s, int *l, const char *t, int cnt)
{
	char *p = *s;
	int oldlen, newlen, x;
	oldlen = *l;
	newlen = oldlen + cnt;
	*l = newlen;
	++newlen;
	x = oldlen ^ newlen;
	if (x > oldlen) {	/* must realloc */
		newlen |= (newlen >> 1);
		newlen |= (newlen >> 2);
		newlen |= (newlen >> 4);
		newlen |= (newlen >> 8);
		newlen |= (newlen >> 16);
		p = reallocString(p, newlen);
		*s = p;
	}
	memcpy(p + oldlen, t, cnt);
	p[oldlen + cnt] = 0;
}

void stringAndChar(char **s, int *l, char c)
{
	char *p = *s;
	int oldlen, newlen, x;
	oldlen = *l;
	newlen = oldlen + 1;
	*l = newlen;
	++newlen;
	x = oldlen ^ newlen;
	if (x > oldlen) {	/* must realloc */
		newlen |= (newlen >> 1);
		newlen |= (newlen >> 2);
		newlen |= (newlen >> 4);
		newlen |= (newlen >> 8);
		newlen |= (newlen >> 16);
		p = reallocString(p, newlen);
		*s = p;
	}
	p[oldlen] = c;
	p[oldlen + 1] = 0;
}

void stringAndNum(char **s, int *l, int n)
{
	char a[16];
	sprintf(a, "%d", n);
	stringAndString(s, l, a);
}

char *cloneString(const char *s)
{
	char *t;
	unsigned len;

	if (!s)
		return 0;
	if (!*s)
		return emptyString;
	len = strlen(s) + 1;
	t = allocString(len);
	strcpy(t, s);
	return t;
}

char *cloneMemory(const char *s, int n)
{
	char *t = allocString(n);
	if (n)
		memcpy(t, s, n);
	return t;
}

void leftClipString(char *s)
{
	char *t;
	if (!s)
		return;
	for (t = s; *t; ++t)
		if (!isspaceByte(*t))
			break;
	if (t > s)
		strmove(s, t);
}

/* shift string one to the right */
void shiftRight(char *s, char first)
{
	int l = strlen(s);
	for (; l >= 0; --l)
		s[l + 1] = s[l];
	s[0] = first;
}

char *Cify(const char *s, int n)
{
	char *u;
	char *t = allocString(n + 1);
	if (n)
		memcpy(t, s, n);
	for (u = t; u < t + n; ++u)
		if (*u == 0)
			*u = ' ';
	*u = 0;
	return t;
}

/* pull a substring out of a larger string,
 * and make it its own allocated string */
char *pullString(const char *s, int l)
{
	char *t;
	if (!l)
		return emptyString;
	t = allocString(l + 1);
	memcpy(t, s, l);
	t[l] = 0;
	return t;
}

char *pullString1(const char *s, const char *t)
{
	return pullString(s, t - s);
}

/* return the number, if string is a number, else -1 */
int stringIsNum(const char *s)
{
	int n;
	if (!isdigitByte(s[0]))
		return -1;
	n = strtol(s, (char **)&s, 10);
	if (*s)
		return -1;
	return n;
}

bool stringIsDate(const char *s)
{
	if (!isdigitByte(*s))
		return false;
	++s;
	if (isdigitByte(*s))
		++s;
	if (*s++ != '/')
		return false;
	if (!isdigitByte(*s))
		return false;
	++s;
	if (isdigitByte(*s))
		++s;
	if (*s++ != '/')
		return false;
	if (!isdigitByte(*s))
		return false;
	++s;
	if (isdigitByte(*s))
		++s;
	if (isdigitByte(*s))
		++s;
	if (isdigitByte(*s))
		++s;
	if (*s)
		return false;
	return true;
}

bool stringIsFloat(const char *s, double *dp)
{
	const char *t;
	*dp = strtod(s, (char **)&t);
	if (*t)
		return false;	/* extra stuff at the end */
	return true;
}

bool memEqualCI(const char *s, const char *t, int len)
{
	char c, d;
	if (s == t)
		return true;
	if (!s || !t)
		return false;
	while (len--) {
		c = *s, d = *t;
		if (islowerByte(c))
			c = toupper(c);
		if (islowerByte(d))
			d = toupper(d);
		if (c != d)
			return false;
		++s, ++t;
	}
	return true;
}

bool stringEqual(const char *s, const char *t)
{
/* check equality of strings with handling of null pointers */
	if (s == t)
		return true;
	if (!s || !t)
		return false;
	if (strcmp(s, t))
		return false;
	return true;
}

bool stringEqualCI(const char *s, const char *t)
{
	char c, d;
/* if two pointers are equal we can return */
	if (s == t)
		return true;
/* if one is NULL then the strings can't be equal */
	if (!s || !t)
		return false;
	while ((c = *s) && (d = *t)) {
		if (islowerByte(c))
			c = toupper(c);
		if (islowerByte(d))
			d = toupper(d);
		if (c != d)
			return false;
		++s, ++t;
	}
	if (*s)
		return false;
	if (*t)
		return false;
	return true;
}

// a text line in the buffer isn't a string; you can't use strstr.
// It ends in \n
const char *stringInBufLine(const char *s, const char *t)
{
	int n = strlen(t);
	for (; *s != '\n'; ++s) {
		if (!strncmp(s, t, n))
			return s;
	}
	return 0;
}

int stringInList(const char *const *list, const char *s)
{
	int i = 0;
	if (!list)
		i_printfExit(MSG_NullStrList);
	if (s)
		while (*list) {
			if (stringEqual(s, *list))
				return i;
			++i;
			++list;
		}
	return -1;
}

int stringInListCI(const char *const *list, const char *s)
{
	int i = 0;
	if (!list) return -1;
//		i_printfExit(MSG_NullStrListCI);
	if (s)
		while (*list) {
			if (stringEqualCI(s, *list))
				return i;
			++i;
			++list;
		}
	return -1;
}

int charInList(const char *list, char c)
{
	char *s;
	if (!list)
		i_printfExit(MSG_NullCharInList);
	s = (char *)strchr(list, c);
	if (!s)
		return -1;
	return s - list;
}

/* In an empty list, next and prev point back to the list, not to 0. */
/* We also allow zero. */
bool listIsEmpty(const struct listHead * l)
{
	return l->next == l || l->next == 0;
}

void initList(struct listHead *l)
{
	l->prev = l->next = l;
}

void delFromList(void *x)
{
	struct listHead *xh = (struct listHead *)x;
	((struct listHead *)xh->next)->prev = xh->prev;
	((struct listHead *)xh->prev)->next = xh->next;
}

void addToListFront(struct listHead *l, void *x)
{
	struct listHead *xh = (struct listHead *)x;
	xh->next = l->next;
	xh->prev = l;
	l->next = (struct listHead *)x;
	((struct listHead *)xh->next)->prev = (struct listHead *)x;
}

void addToListBack(struct listHead *l, void *x)
{
	struct listHead *xh = (struct listHead *)x;
	xh->prev = l->prev;
	xh->next = l;
	l->prev = (struct listHead *)x;
	((struct listHead *)xh->prev)->next = (struct listHead *)x;
}

void addAtPosition(void *p, void *x)
{
	struct listHead *xh = (struct listHead *)x;
	struct listHead *ph = (struct listHead *)p;
	xh->prev = p;
	xh->next = ph->next;
	ph->next = (struct listHead *)x;
	((struct listHead *)xh->next)->prev = (struct listHead *)x;
}

void freeList(struct listHead *l)
{
	while (!listIsEmpty(l)) {
		void *p = l->next;
		delFromList(p);
		nzFree(p);
	}
}

/* like isalnumByte, but allows _ and - */
bool isA(char c)
{
	if (isalnumByte(c))
		return true;
	return (c == '_' || c == '-');
}

bool isquote(char c)
{
	return c == '"' || c == '\'';
}

/* print an error message */
void errorPrint(const char *msg, ...)
{
	char bailflag = 0;
	va_list p;

	va_start(p, msg);

	if (*msg == '@') {
		bailflag = 1;
		++msg;
/* I should internationalize this, but it's never suppose to happen! */
		fprintf(stderr, "disaster, ");
	} else if (isdigitByte(*msg)) {
		bailflag = *msg - '0';
		++msg;
	}

	vfprintf(stderr, msg, p);
	fprintf(stderr, "\n");
	va_end(p);

	if (bailflag)
		exit(bailflag);
}

void debugPrint(int lev, const char *msg, ...)
{
	va_list p;
	if (lev > debugLevel)
		return;
	if (!debugFile || lev <= 2) {
		va_start(p, msg);
		vprintf(msg, p);
		va_end(p);
		printf("\n");
	}
	if (debugFile) {
		if (fseek(debugFile, 0, SEEK_END) >= 10000000) {
			puts("debug file overflow, program aborted");
			fclose(debugFile);
			ebClose(3);
		}
		va_start(p, msg);
		vfprintf(debugFile, msg, p);
		va_end(p);
		fprintf(debugFile, "\n");
	}
	if (lev == 0 && !memcmp(msg, "warning", 7))
		eeCheck();
}

void setDebugFile(const char *name)
{
	if (debugFile)
		fclose(debugFile);
	debugFile = 0;
	nzFree(debugFileName);
	debugFileName = 0;
	if (!name || !*name)
		return;
	debugFileName = cloneString(name);
	debugFile = fopen(name, "w");
	if (debugFile) {
		setlinebuf(debugFile);
	} else
		printf("cannot create %s\n", name);
}

void nl(void)
{
	eb_puts("");
}

/* Turn perl string into C string, and complain about nulls. */
int perl2c(char *t)
{
	int n = 0;
	while (*t != '\n') {
		if (*t == 0)
			++n;
		++t;
	}
	*t = 0;			/* now it's a C string */
	return n;		/* number of nulls */
}

/* The length of a perl string includes its terminating newline */
unsigned pstLength(const uchar *s)
{
	const uchar *t;
	if (!s)
		i_printfExit(MSG_NullPtr);
	t = s;
	while (*t != '\n')
		++t;
	return t + 1 - s;
}

pst clonePstring(const uchar *s)
{
	pst t;
	unsigned len;
	if (!s) return 0;
	len = pstLength(s);
	t = (pst) allocMem(len);
	memcpy(t, s, len);
	return t;
}

// Strings are assumed distinct, hence the use of memcpy.
void copyPstring(pst s, const pst t)
{
	int len = pstLength(t);
	memcpy(s, t, len);
}

int comparePstring(const uchar * s, const uchar * t)
{
	uchar c, d;
	while(true) {
		c = *s, d = *t;
		if(c == d) {
			if(c == '\n') return 0;
			 ++s, ++t;
			continue;
		}
		if(c == '\n') return -1;
		if(d == '\n') return 1;
		return c < d ? -1 : 1;
	}
}

/*********************************************************************
fdIntoMemory reads data from a file descriptor, until EOF is reached.
It works even if we don't know the size beforehand.
We can now use it to read /proc files, pipes, and stdin.
This solves an outstanding issue, and it is needed for forthcoming
functionality, such as edpager.
inpart = 0: read the whole file
inpart = 1: read the first part
inpart = 2: read the next part
*********************************************************************/
#define FILEPARTSIZE 10000000

int fdIntoMemory(int fd, char **data, int *len, bool inparts)
{
	int length, n, j;
	const int blocksize = 8192;
	char *chunk, *buf;
	static char *leftover;
	static int lolen; // leftover length

	chunk = allocZeroString(blocksize);
	if(inparts <= 1)
		buf = initString(&length);
	else
		buf = leftover, length = lolen;

	while(true) {
		if(length >= 0x7fffff00 - blocksize) {
			nzFree(buf);
			nzFree(chunk);
			*data = emptyString;
			*len = 0;
			setError(MSG_BigFile);
			return 0;
		}
		n = read(fd, chunk, blocksize);
		if (n < 0) {
			nzFree(buf);
			nzFree(chunk);
			*data = emptyString;
			*len = 0;
			setError(MSG_NoRead, "file descriptor");
			return 0;
		}

		if (!n) break;
		stringAndBytes(&buf, &length, chunk, n);
		if(!inparts || length < FILEPARTSIZE) continue;
// Can't read in parts if chars are 16 bit or 32 bit wide
		if(inparts == 1 && byteOrderMark((uchar *) buf, length)) {
			inparts = 0; // back to default read
			continue;
		}
// we have to have some leftover
		for(j = length - 2; j > 0 && buf[j] != '\n'; --j)  ;
		if(!j) continue;
		++j;
		leftover = initString(&lolen);
		stringAndBytes(&leftover, &lolen, buf + j, length - j);
		nzFree(chunk);
// do we need room for the extra \n\0 on the end for a piece of the file?
// I don't think so.
		*data = buf;
		*len = j;
// success, but more to read
		debugPrint(4, "part");
		return 2;
	}

	nzFree(chunk);
	buf = reallocString(buf, length + 2);
	*data = buf;
	*len = length;
	return 1;
}

int fileIntoMemory(const char *filename, char **data, int *len, bool inparts)
{
	static int fh;
	char ftype;
	int ret;

	if(inparts == 2) goto fh_set;
	ftype = fileTypeByName(filename, 0);
	if (ftype && ftype != 'f' && ftype != 'p') {
		setError(MSG_RegularFile, filename);
		return 0;
	}
	fh = open(filename, O_RDONLY | O_BINARY);
	if (fh < 0) {
		setError(MSG_NoOpen, filename);
		return 0;
	}

fh_set:
	ret = fdIntoMemory(fh, data, len, inparts);
	if (ret == 0 && filename)
		setError(MSG_NoRead2, filename);
	if(ret <= 1) close(fh);
	return ret;
}

// inverse of the above
bool memoryOutToFile(const char *filename, const char *data, int len,
/* specify the error messages */
		     int msgcreate, int msgwrite)
{
	int fh =
	    open(filename, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, MODE_rw);
	if (fh < 0) {
		setError(msgcreate, filename, errno);
		return false;
	}
	if (write(fh, data, len) < len) {
		setError(msgwrite, filename, errno);
		close(fh);
		return false;
	}
	close(fh);
	return true;
}

// portable function to truncate to 0
void truncate0(const char *filename, int fh)
{
	if (fh < 0)
		truncate(filename, 0l);
	else
		ftruncate(fh, 0l);
}

long long bufferSizeW(const Window *w, bool browsing)
{
	int ln;
	long long size = 0;
	pst p;
	if (!w)
		return -1;
	for (ln = 1; ln <= w->dol; ++ln) {
		p = w->map[ln].text;
		while (*p != '\n') {
			if (*p == InternalCodeChar && browsing && w->browseMode) {
				++p;
				while (isdigitByte(*p)) ++p;
				if (strchr("<>{}", *p)) ++size;
				++p;
				continue;
			}
			++p, ++size;
		}
		++size;
	}			// loop over lines
	if (w->nlMode)
		--size;
	return size;
}

long long bufferSize(int cx, bool browsing)
{
	const Window *w;
	if (cx <= 0 || cx > maxSession || (w = sessionList[cx].lw) == 0) {
		setError(MSG_SessionInactive, cx);
		return -1;
	}
	return bufferSizeW(w, browsing);
}

// Unfold the buffer into one long, allocated string
bool unfoldBufferW(const Window *w, bool cr, char **data, int *len)
{
	char *buf;
	int l, ln;
	long long size = bufferSizeW(w, false);
	if (size < 0)
		return false;
	if (size >= 2000000000) {
		setError(MSG_BigFile);
		return false;
	}
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
	}			// loop over lines
	if (w->dol && w->nlMode) {
		if (cr)
			--size;
	}
	*len = size;
	(*data)[size] = 0;
	return true;
}

bool unfoldBuffer(int cx, bool cr, char **data, int *len)
{
	const Window *w = sessionList[cx].lw;
	return unfoldBufferW(w, cr, data, len);
}

/* shift string to upper, lower, or mixed case */
/* action is u, l, or m. */
void caseShift(char *s, char action)
{
	char c;
	int mc = 0;
	bool ws = true;

	for (; (c = *s); ++s) {
		if (action == 'u') {
			if (isalphaByte(c))
				*s = toupper(c);
			continue;
		}
		if (action == 'l') {
			if (isalphaByte(c))
				*s = tolower(c);
			continue;
		}
/* mixed case left */
		if (isalphaByte(c)) {
			if (ws)
				c = toupper(c);
			else
				c = tolower(c);
			if (ws && c == 'M')
				mc = 1;
			else if (mc == 1 && c == 'c')
				mc = 2;
			else if (mc == 2) {
				c = toupper(c);
				mc = 0;
			} else
				mc = 0;
			*s = c;
			ws = false;
			continue;
		}
		ws = true, mc = 0;
	}			/* loop */
}

// foo-bar has to become fooBar
void camelCase(char *s)
{
	char *t, *w;
	for (t = w = s; *t; ++t)
		if (*t == '-' && isalphaByte(t[1]))
			t[1] = toupper(t[1]);
		else
			*w++ = *t;
	*w = 0;
}

/*********************************************************************
Manage files, directories, and terminal IO.
You'll see some conditional compilation when this program
is ported to other operating systems.
*********************************************************************/

/* Return the type of a file.
 * Make it a capital letter if you are going through a link.
 * I think this will work on Windows, not sure.
 * But the link feature is Unix specific.
 * Remember the stat structure for other things. */
static struct stat this_stat;
static bool this_waslink, this_brokenlink;

char fileTypeByName(const char *name, int showlink)
{
	bool islink = false;
	char c;
	int mode;

	this_waslink = false;
	this_brokenlink = false;

	if(showlink == 2 && dno) {
		this_brokenlink = true;
		return 'f';
	}

	if (lstat(name, &this_stat)) {
		setError(MSG_NoAccess, name);
		return 0;
	}
	mode = this_stat.st_mode & S_IFMT;
	if (mode == S_IFLNK) {	/* symbolic link */
		islink = this_waslink = true;
// If this fails, I'm guessing it's just a file.
		if (stat(name, &this_stat)) {
			this_brokenlink = true;
			return (showlink ? 'F' : 0);
		}
		mode = this_stat.st_mode & S_IFMT;
	}

	c = 'f';
	if (mode == S_IFDIR)
		c = 'd';
	if (mode == S_IFBLK)
		c = 'b';
	if (mode == S_IFCHR)
		c = 'c';
	if (mode == S_IFIFO)
		c = 'p';
	if (mode == S_IFSOCK)
		c = 's';
	if (islink && showlink)
		c = toupper(c);
	return c;
}

char fileTypeByHandle(int fd)
{
	struct stat buf;
	char c;
	int mode;
	if (fstat(fd, &buf)) {
		setError(MSG_NoAccess, "handle");
		return 0;
	}
	mode = buf.st_mode & S_IFMT;
	c = 'f';
	if (mode == S_IFDIR)
		c = 'd';
	if (mode == S_IFBLK)
		c = 'b';
	if (mode == S_IFCHR)
		c = 'c';
	if (mode == S_IFIFO)
		c = 'p';
	if (mode == S_IFSOCK)
		c = 's';
	return c;
}

off_t fileSizeByName(const char *name)
{
	struct stat buf;
	if (stat(name, &buf)) {
		setError(MSG_NoAccess, name);
		return -1;
	}
	return buf.st_size;
}

off_t fileSizeByHandle(int fd)
{
	struct stat buf;
	if (fstat(fd, &buf)) {
/* should never happen */
		return -1;
	}
	return buf.st_size;
}

time_t fileTimeByName(const char *name)
{
	struct stat buf;
	if (stat(name, &buf)) {
		setError(MSG_NoAccess, name);
		return -1;
	}
	return buf.st_mtime;
}

char *conciseSize(size_t n)
{
	static char buf[32];
	unsigned long u, v;
	if (n >= 1000000000) {
		u = n/1000000000;
		v = n/100000000 % 10;
		if(u >= 10 || !v)
			sprintf(buf, "%luG", u);
		else
			sprintf(buf, "%lu.%0luG", u, v);
	} else if (n >= 1000000) {
		u = n/1000000;
		v = n/100000 % 10;
		if(u >= 10 || !v)
			sprintf(buf, "%luM", u);
		else
			sprintf(buf, "%lu.%0luM", u, v);
	} else if (n >= 1000) {
		u = n/1000;
		v = n/100 % 10;
		if(u >= 10 || !v)
			sprintf(buf, "%luK", u);
		else
			sprintf(buf, "%lu.%0luK", u, v);
	} else {
		u = n;
		sprintf(buf, "%lu", u);
	}
	return buf;
}

char *conciseTime(time_t t)
{
	static char buffer[24];
// longest month is 5 bytes
	static const char *const englishMonths[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	static const char *const frenchMonths[] = {
		"Janv", "Fév", "Mars", "Avr", "Mai", "Juin",
		"Juill", "Août", "Sept", "Oct", "Nov", "Déc",
		0
	};
	static const char *const portMonths[] = {
		"Jan", "Fev", "Mar", "Abr", "Mai", "Jun",
		"Jul", "Ago", "Set", "Out", "Nov", "Dec",
	};
	static const char *const germanMonths[] = {
		"Jan", "Feb", "Mär", "Apr", "Mai", "Jun",
		"Jul", "Aug", "Sep", "Okt", "Nov", "Dez",
	};
	static const char *const italMonths[] = {
		"Gen", "Feb", "Mar", "Apr", "Mag", "Giu",
		"Lug", "Ago", "Set", "Ott", "Nov", "Dic",
	};
	static const char *const spanishMonths[] = {
		"Ene", "Feb", "Mar", "Abr", "May",
		"Jun", "Jul", "Ago", "Sep", "Oct", "Nov", "Dic"
	};
	static const char *const *const allMonths[] = { 0,
		englishMonths,
		frenchMonths,
		portMonths,
		englishMonths,
		germanMonths,
		englishMonths,
		italMonths,
		spanishMonths,
	};
	struct tm *tm = localtime(&t);
	sprintf(buffer, "%s %2d %d %02d:%02d",
		allMonths[eb_lang][tm->tm_mon], tm->tm_mday, tm->tm_year + 1900,
		tm->tm_hour, tm->tm_min);
	return buffer;
}

/* retain only characters l s t i k p m, for ls attributes */
bool lsattrChars(const char *buf, char *dest)
{
	bool rc = true;
	const char *s;
	char c, *t;
	static const char ok_chars[] = "lstikpmy";
	char used[26];
	memset(used, 0, sizeof(used));
	t = dest;
	for (s = buf; (c = *s); ++s) {
		if (isspaceByte(c))
			continue;
		if (!strchr(ok_chars, c)) {
			rc = false;
			continue;
		}
		if (used[c - 'a'])
			continue;
		used[c - 'a'] = 1;
		*t++ = c;
	}
	*t = 0;
	return rc;
}

// expand the ls attributes for a file into a static string.
// This assumes user/group names will not be too long.
// Assumes we just called fileTypeByName.
char *lsattr(const char *path, const char *flags)
{
	static char buf[200 + ABSPATH];
	char p[40];
	struct passwd *pwbuf;
	struct group *grpbuf;
	char *s;
	int l, modebits;
	char newpath[ABSPATH];

	buf[0] = 0;

	if (!path || !path[0] || !flags || !flags[0])
		return buf;

	while (*flags) {
		if (buf[0])
			strcat(buf, " ");
		if (this_brokenlink && *flags != 'y') {
			strcat(buf, "~");
			++flags;
			continue;
		}

		switch (*flags) {
		case 't':
			strcat(buf, conciseTime(this_stat.st_mtime));
			break;
		case 'l':
			sprintf(p, "%lld", (long long)this_stat.st_size);
p:
			strcat(buf, p);
			break;
		case 's':
			strcat(buf, conciseSize(this_stat.st_size));
			break;
		case 'i':
			sprintf(p, "%lu", (unsigned long)this_stat.st_ino);
			goto p;
		case 'k':
			sprintf(p, "%lu", (unsigned long)this_stat.st_nlink);
			goto p;
		case 'm':
			strcpy(p, "-");
			if (this_stat.st_rdev)
				sprintf(p, "%d/%d",
					(int)(this_stat.st_rdev >> 8),
					(int)(this_stat.st_rdev & 0xff));
			goto p;
		case 'p':
			s = buf + strlen(buf);
			pwbuf = getpwuid(this_stat.st_uid);
			if (pwbuf) {
				l = strlen(pwbuf->pw_name);
				if (l > 20)
					l = 20;
				strncpy(s, pwbuf->pw_name, l);
				s[l] = 0;
			} else
				sprintf(s, "%d", this_stat.st_uid);
			s += strlen(s);
			*s++ = ' ';
			grpbuf = getgrgid(this_stat.st_gid);
			if (grpbuf) {
				l = strlen(grpbuf->gr_name);
				if (l > 20)
					l = 20;
				strncpy(s, grpbuf->gr_name, l);
				s[l] = 0;
			} else
				sprintf(s, "%d", this_stat.st_gid);
			s += strlen(s);
			*s++ = ' ';
			modebits = this_stat.st_mode;
			modebits &= 07777;
			if (modebits & 07000)
				*s++ = '0' + (modebits >> 9);
			modebits &= 0777;
			*s++ = '0' + (modebits >> 6);
			modebits &= 077;
			*s++ = '0' + (modebits >> 3);
			modebits &= 7;
			*s++ = '0' + modebits;
			*s = 0;
			break;

		case 'y':
			if (!this_waslink) {
				strcat(buf, "-");
				break;
			}
/* yes it's a link, read the path */
			l = readlink(path, newpath, sizeof(newpath));
			if (l <= 0)
				strcat(buf, "...");
			else {
				s = buf + strlen(buf);
				strncpy(s, newpath, l);
				s[l] = 0;
			}
			break;
		}

		++flags;
	}

	return buf;
}

static struct termios savettybuf;
void ttySaveSettings(void)
{
	isInteractive = isatty(0);
	if (isInteractive) {
		if (tcgetattr(0, &savettybuf))
			i_printfExit(MSG_IoctlError);
	}
}

void ttyRestoreSettings(void)
{
	if (isInteractive)
		tcsetattr(0, TCSANOW, &savettybuf);
}

/* put the tty in raw mode.
 * Review your Unix manual on termio.
 * min>0 time>0:  return min chars, or as many as you have received
 *   when time/10 seconds have elapsed between characters.
 * min>0 time=0:  block until min chars are received.
 * min=0 time>0:  return 1 char, or 0 if the timer expires.
 * min=0 time=0:  nonblocking, return whatever chars have been received. */
void ttyRaw(int charcount, int timeout, bool isecho)
{
	struct termios buf = savettybuf;	/* structure copy */
	buf.c_cc[VMIN] = charcount;
	buf.c_cc[VTIME] = timeout;
	buf.c_lflag &= ~(ICANON | ECHO);
	if (isecho)
		buf.c_lflag |= ECHO;
	tcsetattr(0, TCSANOW, &buf);
}

void ttySetEcho(bool enable_echo)
{
	struct termios termios;

	if (!isInteractive)
		return;

	tcgetattr(0, &termios);
	if (enable_echo) {
		termios.c_lflag |= ECHO;
		termios.c_lflag &= ~ECHONL;
	} else {
		termios.c_lflag &= ~ECHO;
		termios.c_lflag |= ECHONL;
	}
	tcsetattr(0, TCSANOW, &termios);
}

/* simulate MSDOS getche() system call */
int getche(void)
{
	char c;
	if(!isInteractive) return getchar();
	fflush(stdout);
	ttyRaw(1, 0, true);
	read(0, &c, 1);
	ttyRestoreSettings();
	return c;
}

int getch(void)
{
	char c;
	if(!isInteractive) return getchar();
	fflush(stdout);
	ttyRaw(1, 0, false);
	read(0, &c, 1);
	ttyRestoreSettings();
	return c;
}


char getLetter(const char *s)
{
	char c;
	while (true) {
		c = getch();
		if (strchr(s, c))
			break;
		if(isInteractive) {
			printf("\a\b");
			fflush(stdout);
		}
	}
	printf("%c", c);
	return c;
}

/* Parameters: message, default file name, must this file be new,
 * and can we except an input of white space,
 * that being converted to a single space. */
char *getFileName(int msg, const char *defname, bool isnew, bool ws)
{
	static char buf[ABSPATH];
	static char spacename[] = " ";
	int l;
	char *p;
	bool allspace;

	while (true) {
		i_printf(msg);
		if (defname)
			printf("[%s] ", defname);
		fflush(stdout);
		if (!fgets(buf, sizeof(buf), stdin))
			exit(0);
		allspace = false;
		for (p = buf; isspaceByte(*p); ++p)
			if (*p == ' ')
				allspace = true;
		l = strlen(p);
		while (l && isspaceByte(p[l - 1]))
			--l;
		p[l] = 0;
		if (!l) {
			if (ws & allspace)
				return spacename;
			if (!defname)
				continue;
// make a copy just to be safe
			l = strlen(defname);
			if (l >= ABSPATH)
				l = ABSPATH - 1;
			strncpy(buf, defname, l);
			buf[l] = 0;
			p = buf;
		} else
			defname = 0;
// see if file exists
		if (isnew && !stringEqual(p, "x") && !stringEqual(p, "X") && fileTypeByName(p, 0)) {
			i_printf(MSG_FileExists, p);
			defname = 0;
			continue;
		}
		return p;
	}
}

/* Protect a filename from expansion by the shell */
static const char shellmeta[] = "\\\n\t |&;<>(){}#'\"~$*?`";
int shellProtectLength(const char *s)
{
	int l = 0;
	while (*s) {
		if (strchr(shellmeta, *s))
			++l;
		++l, ++s;
	}
	return l;
}

void shellProtect(char *t, const char *s)
{
	while (*s) {
		if (strchr(shellmeta, *s))
			*t++ = '\\';
		*t++ = *s++;
	}
}

// get the directory suffix for a file.
// This only makes sense in directory mode.
char *dirSuffixWindow(int n, const Window *w)
{
	static char suffix[4];
	suffix[0] = 0;
	if (w->dirMode) {
		suffix[0] = w->dmap[DTSIZE*n];
		suffix[1] = w->dmap[DTSIZE*n + 1];
		suffix[2] = 0;
	}
	return suffix;
}

static char *dirSuffixContext(int n, int cx)
{
	return dirSuffixWindow(n, sessionList[cx].lw);
}

char *dirSuffix(int n)
{
	return dirSuffixContext(n, context);
}

char *dirSuffix2(int n, const char *path)
{
	static char suffix[4], *t;
	char ftype, c;
	if(!cw->dnoMode)
		return dirSuffixContext(n, context);
// names only, don't have file type information, have to go get it
	t = suffix;
	ftype = fileTypeByName(path, 1);
	if(isupperByte(ftype)) *t++ = '@', ftype = tolower(ftype);
	c = 0;
	if (ftype == 'd') c = '/';
	if (ftype == 's') c = '^';
	if (ftype == 'c') c = '<';
	if (ftype == 'b') c = '*';
	if (ftype == 'p') c = '|';
	*t++ = c;
	*t = 0;
	return suffix;
}

// create the full pathname for a file that you are viewing in directory mode.
// The return is static, with a limit on path length.
char *makeAbsPath(const char *f)
{
	static char path[ABSPATH + 200];
	const char *b = cw->baseDirName ? cw->baseDirName : emptyString;
	if (strlen(b) + strlen(f) > ABSPATH - 2) {
		setError(MSG_PathNameLong, ABSPATH);
		return 0;
	}
	if(cw->baseDirName)
		sprintf(path, "%s/%s", b, f);
	else
		strcpy(path, f);
	return path;
}

/* loop through the files in a directory */
const char *nextScanFile(const char *base)
{
	static DIR *df;
	struct dirent *de;
	const char *s;

	if (!df) {
		if (!base)
			base = ".";
		df = opendir(base);
/* directory could be unreadable */
		if (!df) {
			i_puts(MSG_NoDirNoList);
			return 0;
		}
	}

	while ((de = readdir(df))) {
		s = de->d_name;
		if (s[0] == '.') {
			if (stringEqual(s, "."))
				continue;
			if (!showHiddenFiles)
				continue;
		}
		return s;
	}			/* end loop over files in directory */

	closedir(df);
	df = 0;
	return 0;
}

// compare routine for quicksort directory scan, alphabetical
static bool dir_reverse;
static int dircmp_alph(const void *s, const void *t)
{
	int rc = strcoll((const char *)((const struct lineMap *)s)->text,
			 (const char *)((const struct lineMap *)t)->text);
	if (dir_reverse)
		rc = -rc;
	return rc;
}

static bool sortedDirList(const char *dir, struct lineMap ** map_p, int *count_p,
		   int othersort, bool reverse)
{
	const char *f;
	int linecount = 0, cap;
	struct lineMap *t, *map;

	cap = 128;
	map = t = (struct lineMap *)allocZeroMem(cap * LMSIZE);

	while ((f = nextScanFile(dir))) {
		if (linecount == cap) {
			cap *= 2;
			map = (struct lineMap *)reallocMem(map, cap * LMSIZE);
			t = map + linecount;
		}
/* leave room for @ / newline */
		t->text = (pst) allocMem(strlen(f) + 3);
		strcpy((char *)t->text, f);
		++t, ++linecount;
	}

	*count_p = linecount;
	*map_p = map;

	if (!linecount)
		return true;

// Sort the entries alphabetical,
// unless we plan to sort them some other way.
	if (!othersort) {
		dir_reverse = reverse;
		qsort(map, linecount, LMSIZE, dircmp_alph);
	}

	return true;
}

// directory sort record, for nonalphabetical sorts.
struct DSR {
	int idx;
	union {
#ifdef linux
		struct timespec spec;
#else
		time_t t;
#endif
		off_t z;
	} u;
};
static struct DSR *dsr_list;

// compare routine for quicksort directory scan, size or time
static int dircmp_st(const void *s, const void *t)
{
	const struct DSR *q = s;
	const struct DSR *r = t;
	int rc;
	if (ls_sort == 1) {
		rc = 0;
		if (q->u.z < r->u.z)
			rc = -1;
		if (q->u.z > r->u.z)
			rc = 1;
	}
	if (ls_sort == 2) {
		rc = 0;
#ifdef linux
		if (q->u.spec.tv_sec < r->u.spec.tv_sec)
			rc = -1;
		if (q->u.spec.tv_sec > r->u.spec.tv_sec)
			rc = 1;
		if(!rc) {
// Honor sub-second timestamp precision if the operating system supports it.
		if (q->u.spec.tv_nsec < r->u.spec.tv_nsec)
			rc = -1;
		if (q->u.spec.tv_nsec > r->u.spec.tv_nsec)
			rc = 1;
		}
#else
		if (q->u.t < r->u.t)
			rc = -1;
		if (q->u.t > r->u.t)
			rc = 1;
#endif
	}
	if (ls_reverse)
		rc = -rc;
	return rc;
}

// Read the contents of a directory into the current buffer
bool readDirectory(const char *filename, int endline, char cmd, struct lineMap **map_p)
{
	int len, j, linecount;
	char *v;
	char *dmap = 0;
	struct lineMap *mptr;
	struct lineMap *backpiece = 0;
	uchar innersort = (dno ? 0 : ls_sort);
	bool innerrev = (dno ? false : ls_reverse);

	cw->baseDirName = cloneString(filename);
/* get rid of trailing slash */
	len = strlen(cw->baseDirName);
	if (len && cw->baseDirName[len - 1] == '/')
		cw->baseDirName[len - 1] = 0;
/* Understand that the empty string now means / */

/* get the files, or fail if there is a problem */
	if (!sortedDirList
	    (filename, map_p, &linecount, innersort, innerrev))
		return false;
	if (!cw->dol) {
		cw->dirMode = true;
		cw->dnoMode = dno;
		dmap = allocZeroMem(linecount * DTSIZE);
		if(debugLevel >= 1)
			i_puts(MSG_DirMode);
		if (lsformat[0])
			backpiece = allocZeroMem(LMSIZE * (linecount + 2));
	}

	if (!linecount) {	/* empty directory */
		cw->dot = endline;
		fileSize = 0;
		free(*map_p);
		*map_p = 0;
		nzFree(backpiece);
		nzFree(dmap);
		goto success;
	}

	if (innersort)
		dsr_list = allocZeroMem(sizeof(struct DSR) * linecount);

/* change 0 to nl and count bytes */
	fileSize = 0;
	mptr = *map_p;
	for (j = 0; j < linecount; ++j, ++mptr) {
		char c, ftype;
		pst t = mptr->text;
		char *abspath = makeAbsPath((char *)t);

// make sure this gets done.
		if (backpiece)
			backpiece[j + 1].text = (uchar *) emptyString;

		while (*t) {
			if (*t == '\n')
				*t = '\t';
			++t;
		}
		*t = '\n';
		len = t - mptr->text;
		fileSize += len + 1;
		if (innersort)
			dsr_list[j].idx = j;
		if (!abspath)
			continue;	/* should never happen */

		ftype = fileTypeByName(abspath, 2);
		if (!ftype)
			continue;
		if (isupperByte(ftype)) {	/* symbolic link */
			if (!cw->dirMode)
				*t = '@', *++t = '\n';
			else
				dmap[j*DTSIZE] = '@';
			++fileSize;
		}
		ftype = tolower(ftype);
		c = 0;
		if (ftype == 'd') c = '/';
		if (ftype == 's') c = '^';
		if (ftype == 'c') c = '<';
		if (ftype == 'b') c = '*';
		if (ftype == 'p') c = '|';
		if (c) {
			if (!cw->dirMode)
				*t = c, *++t = '\n';
			else if (dmap[j*DTSIZE])
				dmap[DTSIZE*j + 1] = c;
			else
				dmap[DTSIZE*j] = c;
			++fileSize;
		}
// If sorting a different way, get the attribute.
		if (innersort) {
			if (innersort == 1)
				dsr_list[j].u.z = this_stat.st_size;
			if (innersort == 2) {
// Honor sub-second timestamp precision if the operating system supports it.
// Currently only linux - other systems?
#ifdef linux
				dsr_list[j].u.spec = this_stat.st_mtim;
#else
				dsr_list[j].u.t = this_stat.st_mtime;
#endif
			}
		}

/* extra stat entries on the line */
		if (!lsformat[0] || dno)
			continue;
		v = lsattr(abspath, lsformat);
		if (!*v)
			continue;
		len = strlen(v);
		fileSize += len + 1;
		if (cw->dirMode) {
			backpiece[j + 1].text = (uchar *) cloneString(v);
		} else {
/* have to realloc at this point */
			int l1 = t - mptr->text;
			mptr->text = reallocMem(mptr->text, l1 + len + 3);
			t = mptr->text + l1;
			*t++ = ' ';
			strcpy((char *)t, v);
			t += len;
			*t++ = '\n';
			*t = 0;
		}
	}			// loop fixing files in the directory scan

	if (innersort) {
		struct lineMap *tmp;
		char *dmap2;
		qsort(dsr_list, linecount, sizeof(struct DSR), dircmp_st);
// Now I have to remap everything.
		tmp = allocMem(LMSIZE * linecount);
		if(dmap) dmap2 = allocZeroMem(DTSIZE * linecount);
		for (j = 0; j < linecount; ++j) {
			tmp[j] = (*map_p)[dsr_list[j].idx];
			if(!dmap) continue;
			dmap2[DTSIZE*j] = dmap[DTSIZE*dsr_list[j].idx];
			dmap2[DTSIZE*j + 1] = dmap[DTSIZE*dsr_list[j].idx + 1];
		}
		free(*map_p);
		*map_p = tmp;
		if(dmap) free(dmap), dmap = dmap2;
		if (backpiece) {
			tmp = allocMem(LMSIZE * (linecount + 2));
			for (j = 0; j < linecount; ++j)
				tmp[j + 1] = backpiece[dsr_list[j].idx + 1];
			memset(tmp, 0, LMSIZE);
			memset(tmp + linecount + 1, 0, LMSIZE);
			free(backpiece);
			backpiece = tmp;
		}
		free(dsr_list);
	}

	addToMap(linecount, endline);
	cw->r_map = backpiece;
	if(dmap) {
		cw->dmap = allocMem((linecount + 1) * DTSIZE);
		memcpy(cw->dmap + DTSIZE, dmap, linecount*DTSIZE);
	}

success:
	if (cmd == 'r')
		debugPrint(1, "%lld", fileSize);
	return true;
}

// Delete files from a directory as you delete lines.
// Set dw to move them to your recycle bin.
// Set dx to delete them outright.
bool delFiles(int start, int end, bool withtext, char origcmd, char *cmd_p)
{
	int ln, j;

	if (!dirWrite) {
		setError(MSG_DirNoWrite);
		return false;
	}

	if (dirWrite == 1 && !recycleBin) {
		setError(MSG_NoRecycle);
		return false;
	}

	if(end < start) return true;
	*cmd_p = 'e';		// show errors

	for (ln = start; ln <= end; ++ln) {
		char *file, *t, *path, *ftype, *a;
		char qc = '\''; // quote character
		file = (char *)fetchLine(ln, 0);
		t = strchr(file, '\n');
		if (!t)
			i_printfExit(MSG_NoNlOnDir, file);
		*t = 0;
		path = makeAbsPath(file);
		if (!path) {
abort:
			free(file);
			if (ln != start && withtext)
				delText(start, ln - 1);
			return false;
		}

// check formeta chars in path
		if(strchr(path, qc)) {
			qc = '"';
			if(strpbrk(path, "\"$"))
// I can't easily turn this into a shell command, so just hang it.
				qc = 0;
		}
		if(strstr(path, "\\\\"))
			qc = 0;

		ftype = dirSuffix2(ln, path);
		if (dirWrite == 2 || (*ftype && strchr("@<*^|", *ftype)))
			debugPrint(1, "%s%s ↓", file, ftype);
		else
			debugPrint(1, "%s%s → 🗑", file, ftype);

		if (dirWrite == 2 && *ftype == '/') {
			if(!qc) {
				setError(MSG_MetaChar);
				goto abort;
			}
			asprintf(&a, "rm -rf %c%s%c",
			qc, path, qc);
			j = system(a);
			free(a);
			if(!j) {
				free(file);
				continue;
			} else {
				setError(MSG_NoDirDelete);
				goto abort;
			}
		}

		if (dirWrite == 2 || (*ftype && strchr("@<*^|", *ftype))) {
unlink:
			if (unlink(path)) {
				setError(MSG_NoRemove, file);
				goto abort;
			}
		} else {
			char bin[ABSPATH];
			sprintf(bin, "%s/%s", recycleBin, file);
			if (rename(path, bin)) {
				if (errno == EXDEV) {
					char *rmbuf;
					int rmlen;
					if (*ftype == '/' ||
					fileSizeByName(path) > 200000000) {
// let mv do the work
						if(!qc || strchr(bin, qc)) {
							setError(MSG_MetaChar);
							goto abort;
						}
						asprintf(&a, "mv -n %c%s%c %c%s%c",
						qc, path, qc, qc, bin, qc);
						j = system(a);
						free(a);
						if(!j) {
							free(file);
							continue;
						} else {
							setError(MSG_MoveFileSystem , path);
							goto abort;
						}
					}
					if (!fileIntoMemory    (path, &rmbuf, &rmlen, 0))
						goto abort;
					if (!memoryOutToFile(bin, rmbuf, rmlen,
							     MSG_TempNoCreate2,
							     MSG_NoWrite2)) {
						nzFree(rmbuf);
						goto abort;
					}
					nzFree(rmbuf);
					goto unlink;
				}

// some other rename error
				setError(MSG_NoMoveToTrash, file);
				goto abort;
			}
		}
	}
	if(withtext) delText(start, end);

// if you type D instead of d, I don't want to lose that.
	*cmd_p = origcmd;
	return true;
}

// Move or copy files from one directory to another
bool moveFiles(int start, int end, int dest, char origcmd, char relative)
{
	Window *cw1 = cw, *cw2, *w;
	char *path1, *path2;
	int ln, cnt, dol;

	if (!dirWrite) {
		setError(MSG_DirNoWrite);
		return false;
	}

	if(!relative) {
				if (dest == 0) {
					setError(MSG_Session0);
					return false;
				}
				if (dest == context) {
					setError(MSG_SessionCurrent, dest);
					return false;
				}
				if (dest > maxSession || !(cw2 = sessionList[dest].lw) || !cw2->dirMode) {
					char numstring[12];
					sprintf(numstring, "%d", dest);
					setError(MSG_NotDir, numstring);
					return false;
				}
	}

	if(relative && !dest) {
		setError(MSG_NoChange);
		return false;
	}

	if(relative == '+') {
		for(cnt = 0, w = cw; w; w = w->prev, ++cnt) ;
		if(dest >= cnt) {
			setError(MSG_NoUp);
			return false;
		}
		for(w = cw; dest; w = w->prev, --dest)  ;
		cw2 = w;
	}

	if(relative == '-') {
		for(cnt = 0, w = cs->lw2; w; w = w->prev, ++cnt) ;
		if(dest > cnt) {
			setError(MSG_NoDown);
			return false;
		}
		cnt -= dest;
		for(w = cs->lw2; cnt; w = w->prev, --cnt)  ;
		cw2 = w;
	}

	ln = start;
	cnt = end - start + 1;
	while (cnt--) {
		char *file, *t, *ftype;
		file = (char *)fetchLine(ln, 0);
		t = strchr(file, '\n');
		if (!t)
			i_printfExit(MSG_NoNlOnDir, file);
		*t = 0;
		path1 = makeAbsPath(file);
		if (!path1) {
			free(file);
			return false;
		}
		path1 = cloneString(path1);
		ftype = dirSuffix2(ln, path1);

		debugPrint(1, "%s%s %s %s",
		file, ftype, (origcmd == 'm' ? "→" : "≡"), cw2->baseDirName);

		cw = cw2;
		path2 = makeAbsPath(file);
		cw = cw1;
		if (!path2) {
			free(file);
			free(path1);
			return false;
		}

		if (!access(path2, 0)) {
			setError(MSG_DestFileExists, path2);
			free(file);
			free(path1);
			return false;
		}

		errno = EXDEV;
		if (origcmd == 't' || rename(path1, path2)) {
			if (errno == EXDEV) {
				char *rmbuf;
				int rmlen, j;
				if (*ftype || fileSizeByName(path1) > 200000000) {
// let mv or cp do the work
					char *a, qc = '\'';
					if(strchr(path1, qc) || strchr(path2, qc)) {
						qc = '"';
						if(strpbrk(path1, "\"$") || strpbrk(path2, "\"$"))
// I can't easily turn this into a shell command, so just hang it.
							qc = 0;
					}
					if(strstr(path1, "\\\\") || strstr(path2, "\\\\"))
						qc = 0;
					if(!qc) {
						setError(MSG_MetaChar);
						free(file);
						free(path1);
						return false;
					}
					asprintf(&a, "%s %c%s%c %c%s%c",
					(origcmd == 'm' ? "mv -n" : "cp -an"),
					qc, path1, qc, qc, cw2->baseDirName, qc);
					j = system(a);
					free(a);
					if(j) {
						setError((origcmd == 'm' ? MSG_MoveFileSystem : MSG_CopyFail), file);
						free(file);
						free(path1);
						return false;
					}
					if(origcmd == 'm')
						unlink(path1);
					goto moved;
				}
// A small file, copy it ourselves.
				if (!fileIntoMemory(path1, &rmbuf, &rmlen, 0)) {
					free(file);
					free(path1);
					return false;
				}
				if (!memoryOutToFile(path2, rmbuf, rmlen,
						     MSG_TempNoCreate2,
						     MSG_NoWrite2)) {
					free(file);
					free(path1);
					nzFree(rmbuf);
					return false;
				}

// reset the time stamp, cause that's what cp does
				struct stat buf;
				if (!stat(path1, &buf)) {
// we can march on if the time isn't adjusted
#if defined(__APPLE__)
					struct timespec times[2] = {buf.st_atimespec, buf.st_mtimespec};
#else
					struct timespec times[2] = {buf.st_atim, buf.st_mtim};
#endif
					utimensat(AT_FDCWD, path2, times, 0);
				}

				nzFree(rmbuf);
				if(origcmd == 'm')
					unlink(path1);
				goto moved;
			}

			setError(MSG_MoveError, file, errno);
			free(file);
			free(path1);
			return false;
		}

moved:
		free(path1);
		if(origcmd == 'm')
			delText(ln, ln);
		else
			cw->dot = ln;
// add it to the other directory
		*t++ = '\n';
		cw = cw2;
		dol = cw->dol;
		addTextToBuffer((pst)file, t-file, dol, false);
		free(file);
		cw->dot = ++dol;
		if(cw->dmap)
			cw->dmap = reallocMem(cw->dmap, DTSIZE * (dol + 1));
		else
			cw->dmap = allocZeroMem(DTSIZE * (dol + 1));
		memset(cw->dmap + DTSIZE*dol, 0, DTSIZE);
		cw->dmap[DTSIZE*dol] = ftype[0];
		if(ftype[0])
		cw->dmap[DTSIZE*dol + 1] = ftype[1];
// if attributes were displayed in that directory - more work to do.
// I just leave a space for them; I don't try to derive them.
		if(cw->r_map) {
			cw->r_map = reallocMem(cw->r_map, LMSIZE * (dol + 2));
			memset(cw->r_map + dol, 0, LMSIZE*2);
			cw->r_map[dol].text = (uchar*)emptyString;
		}
		cw = cw1; // go back to original window
		if(origcmd == 't') cw->dot = ln++;
	}

	return true;
}

/* Expand environment variables, then wild cards.
 * But the result should be one and only one file.
 * Return the new expanded line.
 * Neither the original line nore the new line is allocated.
 * They are static char buffers that are just plain long enough. */

static bool envExpand(const char *line, const char **expanded)
{
	const char *s;
	char *t;
	const char *v;		/* result of getenv call */
	bool inbrace;		/* ${foo} */
	struct passwd *pw;
	const char *udir;	/* user directory */
	int l;
	static char varline[ABSPATH];
	char var1[40];

/* quick check */
	if (line[0] != '~' && !strchr(line, '$')) {
		*expanded = line;
		return true;
	}

/* ok, need to crunch along */
	t = varline;
	s = line;

	if (line[0] != '~')
		goto dollars;

	l = 0;
	for (s = line + 1; isalnumByte(*s) || *s == '_'; ++s)
		++l;
	if ((unsigned)l >= sizeof(var1) || isdigitByte(line[1]) || (*s && *s != '/')) {
/* invalid syntax, put things back */
		s = line;
		goto dollars;
	}

	udir = 0;
	strncpy(var1, line + 1, l);
	var1[l] = 0;
	if (l) {
		pw = getpwnam(var1);
		if (!pw) {
			setError(MSG_NoTilde, var1);
			return false;
		}
		if (pw->pw_dir && *pw->pw_dir)
			udir = pw->pw_dir;
	} else
		udir = home;
	if (!udir) {
		s = line;
		goto dollars;
	}
	l = strlen(udir);
	if ((unsigned)l >= sizeof(varline))
		goto longline;
	strcpy(varline, udir);
	t = varline + l;

dollars:
	for (; *s; ++s) {
		if (t - varline == ABSPATH - 1) {
longline:
			setError(MSG_ShellLineLong);
			return false;
		}
		if (*s == '\\' && s[1] == '$') {
/* this $ is escaped */
			++s;
appendchar:
			*t++ = *s;
			continue;
		}
		if (*s != '$')
			goto appendchar;

/* this is $, see if it is $var or ${var} */
		inbrace = false;
		v = s + 1;
		if (*v == '{')
			inbrace = true, ++v;
		if (!isalphaByte(*v) && *v != '_')
			goto appendchar;
		l = 0;
		while (isalnumByte(*v) || *v == '_') {
			if (l == sizeof(var1) - 1)
				goto longline;
			var1[l++] = *v++;
		}
		var1[l] = 0;
		if (inbrace) {
			if (*v != '}')
				goto appendchar;
			++v;
		}
		s = v - 1;
		v = getenv(var1);
		if (!v) {
			setError(MSG_NoEnvVar, var1);
			return false;
		}
		l = strlen(v);
		if (t - varline + l >= ABSPATH)
			goto longline;
		strcpy(t, v);
		t += l;
	}
	*t = 0;

	*expanded = varline;
	return true;
}

bool envFile(const char *line, const char **expanded)
{
	static char line2[ABSPATH];
	const char *varline;
	const char *s;
	char *t;
	glob_t g;
	int rc, flags;

/* ` disables this stuff */
/* but `` is a literal ` */
	if (line[0] == '`') {
		if (line[1] != '`') {
			*expanded = line + 1;
			return true;
		}
		++line;
	}

	if (!envExpand(line, &varline))
		return false;

/* expanded the environment variables, if any, now time to glob */
	flags = GLOB_NOSORT;
	rc = glob(varline, flags, NULL, &g);

	if (rc == GLOB_NOMATCH) {
/* unescape the metas */
		t = line2;
		for (s = varline; *s; ++s) {
			if (*s == '\\' && s[1] && strchr("*?[", s[1]))
				++s;
			*t++ = *s;
		}
		*t = 0;
		*expanded = line2;
		return true;
	}

	if (rc) {
/* some other syntax error, whereup we can't expand. */
		setError(MSG_ShellExpand);
		globfree(&g);
		return false;
	}

	if (g.gl_pathc != 1) {
		setError(MSG_ShellManyMatch);
		globfree(&g);
		return false;
	}

/* looks good, if it isn't too long */
	s = g.gl_pathv[0];
	if (strlen(s) >= sizeof(line2)) {
		setError(MSG_ShellLineLong);
		globfree(&g);
		return false;
	}

	strcpy(line2, s);
	globfree(&g);
	*expanded = line2;
	return true;
}

// Like the above, but string is allocated, and errors are printed
// This is only used to expand pathnames in .ebrc
char *envFileAlloc(const char *line)
{
	const char *line2;
	if(envFile(line, &line2))
		return cloneString(line2);
	showError();
	setError(-1);
	return 0;
}

/* Call the above routine if filename contains a  slash,
 * or prepend the download directory if it does not.
 * If there is no download directory then always expand as above. */
bool envFileDown(const char *line, const char **expanded)
{
	static char line2[MAXTTYLINE];

	if (!downDir || strchr(line, '/'))
/* we don't necessarily expect there to be a file here */
		return envFile(line, expanded);

	if (strlen(downDir) + strlen(line) >= sizeof(line2) - 1) {
		setError(MSG_ShellLineLong);
		return false;
	}

	sprintf(line2, "%s/%s", downDir, line);
	*expanded = line2;
	return true;
}

FILE *efopen(const char *name, const char *mode)
{
	FILE *f;

	if (name[0] == '-' && name[1] == 0) {
		if (*mode == 'r')
			return stdin;
		if (*mode == 'w' || *mode == 'a')
			return stdout;
	}

	f = fopen(name, mode);
	if (f)
		return f;

	if (*mode == 'r')
		i_printfExit(MSG_NoOpen, name);
	else if (*mode == 'w' || *mode == 'a')
		i_printfExit(MSG_CreateFail, name);
	else
		i_printfExit(MSG_InvalidFopen, mode);
	return 0;
}

int eopen(const char *name, int mode, int perms)
{
	int fd;
	fd = open(name, mode, perms);
	if (fd >= 0)
		return fd;
	if (mode & O_WRONLY)
		i_printfExit(MSG_CreateFail, name);
	else
		i_printfExit(MSG_NoOpen, name);
	return -1;
}

void appendFile(const char *fname, const char *message, ...)
{
	FILE *f;
	va_list p;
	f = efopen(fname, "a");
	va_start(p, message);
	vfprintf(f, message, p);
	va_end(p);
	fprintf(f, "\n");
	fclose(f);
}

/* like the above, but no formatting */
void appendFileNF(const char *filename, const char *msg)
{
	FILE *f = efopen(filename, "a");
	fprintf(f, "%s\n", msg);
	fclose(f);
}

/* Wrapper around system(). */
int eb_system(const char *cmd, bool print_on_success)
{
	int system_ret = system(cmd);
	if (system_ret != -1) {
		if (print_on_success && debugLevel > 0 && !system_ret)
			i_puts(MSG_OK);
		if(system_ret) setError(MSG_CmdFail);
	} else {
		i_printf(MSG_SystemCmdFail, system_ret);
		nl();
		setError(MSG_CmdFail);
	}
	return system_ret;
}
