/* stringfile.c: manage strings, files, and directories for edbrowse.
* This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"

#include <sys/stat.h>
#include <arpa/inet.h>
#ifdef DOSLIKE
#include <dos.h>
#else
#include <termios.h>
#include <unistd.h>
#endif
#include <glob.h>
#include <netdb.h>

/*********************************************************************
Allocate and manage memory.
Allocate and copy strings.
If we're out of memory, the program aborts.  No error legs.
*********************************************************************/

void *allocMem(size_t n)
{
	void *s;
	if (!n)
		return EMPTYSTRING;
	if (!(s = malloc(n)))
		i_printfExit(MSG_MemAllocError, n);
	return s;
}				/* allocMem */

void *allocZeroMem(size_t n)
{
	void *s;
	if (!n)
		return EMPTYSTRING;
	if (!(s = calloc(n, 1)))
		i_printfExit(MSG_MemCallocError, n);
	return s;
}				/* allocZeroMem */

void *reallocMem(void *p, size_t n)
{
	void *s;
	if (!n)
		i_printfExit(MSG_ReallocP);
/* small check against allocated strings getting huge */
	if (n < 0)
		i_printfExit(MSG_MemAllocError, n);
	if (!p)
		i_printfExit(MSG_Realloc0, n);
	if (p == EMPTYSTRING)
		return allocMem(n);
	if (!(s = realloc(p, n)))
		i_printfExit(MSG_ErrorRealloc, n);
	return s;
}				/* reallocMem */

void nzFree(void *s)
{
	if (s && s != EMPTYSTRING)
		free(s);
}				/* nzFree */

uchar fromHex(char d, char e)
{
	d |= 0x20, e |= 0x20;
	if (d >= 'a')
		d -= ('a' - '9' - 1);
	if (e >= 'a')
		e -= ('a' - '9' - 1);
	d -= '0', e -= '0';
	return ((((uchar) d) << 4) | (uchar) e);
}				/* fromHex */

char *appendString(char *s, const char *p)
{
	int slen = strlen(s);
	int plen = strlen(p);
	s = reallocMem(s, slen + plen + 1);
	strcpy(s + slen, p);
	return s;
}				/* appendstring */

char *prependString(char *s, const char *p)
{
	int slen = strlen(s);
	int plen = strlen(p);
	char *t = allocMem(slen + plen + 1);
	strcpy(t, p);
	strcpy(t + plen, s);
	nzFree(s);
	return t;
}				/* prependstring */

void skipWhite(const char **s)
{
	const char *t = *s;
	while (isspaceByte(*t))
		++t;
	*s = t;
}				/* skipWhite */

void stripWhite(char *s)
{
	const char *t = s;
	char *u;
	skipWhite(&t);
	if (t > s)
		strmove(s, t);
	u = s + strlen(s);
	while (u > s && isspaceByte(u[-1]))
		--u;
	*u = 0;
}				/* stripWhite */

/* compress white space */
void spaceCrunch(char *s, bool onespace, bool unprint)
{
	int i, j;
	char c;
	bool space = true;
	for (i = j = 0; c = s[i]; ++i) {
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
}				/* spaceCrunch */

/* Like strcpy, but able to cope with overlapping strings. */
char *strmove(char *dest, const char *src)
{
	return memmove(dest, src, strlen(src) + 1);
}				/* strmove */

/* OO has a lot of unnecessary overhead, and a few inconveniences,
 * but I really miss it right now.  The following
 * routines make up for the lack of simple string concatenation in C.
 * The string space allocated is always a power of 2 - 1, starting with 1.
 * Each of these routines puts an extra 0 on the end of the "string". */

char *initString(int *l)
{
	*l = 0;
	return EMPTYSTRING;
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
		p = reallocMem(p, newlen);
		*s = p;
	}
	strcpy(p + oldlen, t);
}				/* stringAndString */

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
		p = reallocMem(p, newlen);
		*s = p;
	}
	memcpy(p + oldlen, t, cnt);
	p[oldlen + cnt] = 0;
}				/* stringAndBytes */

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
		p = reallocMem(p, newlen);
		*s = p;
	}
	p[oldlen] = c;
	p[oldlen + 1] = 0;
}				/* stringAndChar */

void stringAndNum(char **s, int *l, int n)
{
	char a[16];
	sprintf(a, "%d", n);
	stringAndString(s, l, a);
}				/* stringAndNum */

/* 64M 16K etc */
void stringAndKnum(char **s, int *l, int n)
{
	char a[16];
	if (n && n / (1024 * 1024) * (1024 * 1024) == n)
		sprintf(a, "%dM", n / (1024 * 1024));
	else if (n && n / 1024 * 1024 == n)
		sprintf(a, "%dK", n / 1024);
	else
		sprintf(a, "%d", n);
	stringAndString(s, l, a);
}				/* stringAndKnum */

char *cloneString(const char *s)
{
	char *t;
	unsigned len;

	if (!s)
		return 0;
	if (!*s)
		return EMPTYSTRING;
	len = strlen(s) + 1;
	t = allocMem(len);
	strcpy(t, s);
	return t;
}				/* cloneString */

char *cloneMemory(const char *s, int n)
{
	char *t = allocMem(n);
	if (n)
		memcpy(t, s, n);
	return t;
}				/* cloneMemory */

void clipString(char *s)
{
	int len;
	if (!s)
		return;
	len = strlen(s);
	while (--len >= 0)
		if (!isspaceByte(s[len]))
			break;
	s[len + 1] = 0;
}				/* clipString */

void leftClipString(char *s)
{
	char *t;
	if (!s)
		return;
	for (t = s; *t; ++t)
		if (!isspace(*t))
			break;
	if (t > s)
		strmove(s, t);
}				/* leftClipString */

/* shift string one to the right */
void shiftRight(char *s, char first)
{
	int l = strlen(s);
	for (; l >= 0; --l)
		s[l + 1] = s[l];
	s[0] = first;
}				/* shiftRight */

char *Cify(const char *s, int n)
{
	char *u;
	char *t = allocMem(n + 1);
	if (n)
		memcpy(t, s, n);
	for (u = t; u < t + n; ++u)
		if (*u == 0)
			*u = ' ';
	*u = 0;
	return t;
}				/* Cify */

/* pull a substring out of a larger string,
 * and make it its own allocated string */
char *pullString(const char *s, int l)
{
	char *t;
	if (!l)
		return EMPTYSTRING;
	t = allocMem(l + 1);
	memcpy(t, s, l);
	t[l] = 0;
	return t;
}				/* pullString */

char *pullString1(const char *s, const char *t)
{
	return pullString(s, t - s);
}

int stringIsNum(const char *s)
{
	int n;
	if (!isdigitByte(s[0]))
		return -1;
	n = strtol(s, (char **)&s, 10);
	if (*s)
		return -1;
	return n;
}				/* stringIsNum */

bool stringIsDate(const char *s)
{
	if (!isdigit(*s))
		return false;
	++s;
	if (isdigit(*s))
		++s;
	if (*s++ != '/')
		return false;
	if (!isdigit(*s))
		return false;
	++s;
	if (isdigit(*s))
		++s;
	if (*s++ != '/')
		return false;
	if (!isdigit(*s))
		return false;
	++s;
	if (isdigit(*s))
		++s;
	if (isdigit(*s))
		++s;
	if (isdigit(*s))
		++s;
	if (*s)
		return false;
	return true;
}				/* stringIsDate */

bool stringIsFloat(const char *s, double *dp)
{
	const char *t;
	*dp = strtod(s, (char **)&t);
	if (*t)
		return false;	/* extra stuff at the end */
	return true;
}				/* stringIsFloat */

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
}				/* memEqualCI */

char *strstrCI(const char *base, const char *search)
{
	int l = strlen(search);
	while (*base) {
		if (memEqualCI(base, search, l))
			return (char *)base;
		++base;
	}
	return 0;
}				/* strstrCI */

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
}				/* stringEqual */

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
}				/* stringEqualCI */

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
}				/* stringInList */

int stringInListCI(const char *const *list, const char *s)
{
	int i = 0;
	if (!list)
		i_printfExit(MSG_NullStrListCI);
	if (s)
		while (*list) {
			if (stringEqualCI(s, *list))
				return i;
			++i;
			++list;
		}
	return -1;
}				/* stringInListCI */

int charInList(const char *list, char c)
{
	char *s;
	if (!list)
		i_printfExit(MSG_NullCharInList);
	s = strchr(list, c);
	if (!s)
		return -1;
	return s - list;
}				/* charInList */

/* In an empty list, next and prev point back to the list, not to 0. */
/* We also allow zero. */
bool listIsEmpty(const struct listHead * l)
{
	return l->next == l || l->next == 0;
}				/* listIsEmpty */

void initList(struct listHead *l)
{
	l->prev = l->next = l;
}				/* initList */

void delFromList(void *x)
{
	struct listHead *xh = x;
	((struct listHead *)xh->next)->prev = xh->prev;
	((struct listHead *)xh->prev)->next = xh->next;
}				/* delFromList */

void addToListFront(struct listHead *l, void *x)
{
	struct listHead *xh = x;
	xh->next = l->next;
	xh->prev = l;
	l->next = x;
	((struct listHead *)xh->next)->prev = x;
}				/* addToListFront */

void addToListBack(struct listHead *l, void *x)
{
	struct listHead *xh = x;
	xh->prev = l->prev;
	xh->next = l;
	l->prev = x;
	((struct listHead *)xh->prev)->next = x;
}				/* addToListBack */

void addAtPosition(void *p, void *x)
{
	struct listHead *xh = x;
	struct listHead *ph = p;
	xh->prev = p;
	xh->next = ph->next;
	ph->next = x;
	((struct listHead *)xh->next)->prev = x;
}				/* addAtPosition */

void freeList(struct listHead *l)
{
	while (!listIsEmpty(l)) {
		void *p = l->next;
		delFromList(p);
		nzFree(p);
	}
}				/* freeList */

/* like isalnumByte, but allows _ and - */
bool isA(char c)
{
	if (isalnumByte(c))
		return true;
	return (c == '_' || c == '-');
}				/* isA */

bool isquote(char c)
{
	return c == '"' || c == '\'';
}				/* isquote */

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
}				/* errorPrint */

void debugPrint(int lev, const char *msg, ...)
{
	va_list p;
	if (lev > debugLevel)
		return;
	va_start(p, msg);
	vprintf(msg, p);
	va_end(p);
	nl();
	if (lev == 0 && !memcmp(msg, "warning", 7))
		eeCheck();
}				/* debugPrint */

void nl(void)
{
	puts("");
}				/* nl */

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
}				/* perl2c */

/* The length of a perl string includes its terminating newline */
unsigned pstLength(pst s)
{
	pst t;
	if (!s)
		i_printfExit(MSG_NullPtr);
	t = s;
	while (*t != '\n')
		++t;
	return t + 1 - s;
}				/* pstLength */

pst clonePstring(pst s)
{
	pst t;
	unsigned len;
	if (!s)
		return s;
	len = pstLength(s);
	t = allocMem(len);
	memcpy(t, s, len);
	return t;
}				/* clonePstring */

// Strings are assumed distinct, hence the use of memcpy.
void copyPstring(pst s, const pst t)
{
	int len = pstLength(t);
	memcpy(s, t, len);
}				/* copyPstring */

/*
 * fdIntoMemory reads data from a file descriptor, until EOF is reached.
 * It works even if we don't know the size beforehand.
 * We can now use it to read /proc files, pipes, and stdin.
 * This solves an outstanding issue, and it is needed for forthcoming
 * functionality, such as edpager.
 */
bool fdIntoMemory(int fd, char **data, int *len)
{
	int length, n;
	const int blocksize = 8192;
	char *chunk, *buf;

	chunk = allocZeroMem(blocksize);
	buf = initString(&length);

	n = 0;
	do {
		n = read(fd, chunk, blocksize);
		if (n < 0) {
			nzFree(buf);
			nzFree(chunk);
			*data = EMPTYSTRING;
			*len = 0;
			setError(MSG_NoRead, "file descriptor");
			return false;
		}

		if (n > 0)
			stringAndBytes(&buf, &length, chunk, n);
	} while (n != 0);

	nzFree(chunk);
	buf = reallocMem(buf, length + 2);
	*data = buf;
	*len = length;
	return true;
}				/* fdIntoMemory */

bool fileIntoMemory(const char *filename, char **data, int *len)
{
	int fh;
	char ftype = fileTypeByName(filename, false);
	bool ret;
	if (ftype && ftype != 'f') {
		setError(MSG_RegularFile, filename);
		return false;
	}
	fh = open(filename, O_RDONLY | O_BINARY);
	if (fh < 0) {
		setError(MSG_NoOpen, filename);
		return false;
	}

	ret = fdIntoMemory(fh, data, len);
	if (ret == false)
		setError(MSG_NoRead2, filename);

	close(fh);
	return ret;
}				/* fileIntoMemory */

/* inverse of the above */
bool memoryOutToFile(const char *filename, const char *data, int len,
/* specify the error messages */
		     int msgcreate, int msgwrite)
{
	int fh = open(filename, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0666);
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
}				/* memoryOutToFile */

/* shift string to upper, lower, or mixed case */
/* action is u, l, or m. */
void caseShift(char *s, char action)
{
	char c;
	int mc = 0;
	bool ws = true;

	for (; c = *s; ++s) {
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
}				/* caseShift */

/*********************************************************************
Manage files, directories, and terminal IO.
You'll see some conditional compilation when this program
is ported to other operating systems.
*********************************************************************/

/* Return the type of a file.
 * Make it a capital letter if you are going through a link.
 * I think this will work on Windows, not sure.
 * But the link feature is Unix specific. */

char fileTypeByName(const char *name, bool showlink)
{
	struct stat buf;
	bool islink = false;
	char c;
	int mode;
	if (lstat(name, &buf)) {
		setError(MSG_NoAccess, name);
		return 0;
	}
	mode = buf.st_mode & S_IFMT;
	if (mode == S_IFLNK) {	/* symbolic link */
		islink = true;
/* If this fails, I'm guessing it's just a file. */
		if (stat(name, &buf))
			return (showlink ? 'F' : 0);
		mode = buf.st_mode & S_IFMT;
	}
	c = 'f';
	if (mode == S_IFDIR)
		c = 'd';
#ifndef DOSLIKE
/* I don't think these are Windows constructs. */
	if (mode == S_IFBLK)
		c = 'b';
	if (mode == S_IFCHR)
		c = 'c';
	if (mode == S_IFIFO)
		c = 'p';
	if (mode == S_IFSOCK)
		c = 's';
#endif
	if (islink & showlink)
		c = toupper(c);
	return c;
}				/* fileTypeByName */

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
#ifndef DOSLIKE
/* I don't think these are Windows constructs. */
	if (mode == S_IFBLK)
		c = 'b';
	if (mode == S_IFCHR)
		c = 'c';
	if (mode == S_IFIFO)
		c = 'p';
	if (mode == S_IFSOCK)
		c = 's';
#endif
	return c;
}				/* fileTypeByHandle */

int fileSizeByName(const char *name)
{
	struct stat buf;
	if (stat(name, &buf)) {
		setError(MSG_NoAccess, name);
		return -1;
	}
	return buf.st_size;
}				/* fileSizeByName */

int fileSizeByHandle(int fd)
{
	struct stat buf;
	if (fstat(fd, &buf)) {
/* should never happen */
		return -1;
	}
	return buf.st_size;
}				/* fileSizeByHandle */

time_t fileTimeByName(const char *name)
{
	struct stat buf;
	if (stat(name, &buf)) {
		setError(MSG_NoAccess, name);
		return -1;
	}
	return buf.st_mtime;
}				/* fileTimeByName */

#ifndef DOSLIKE

static struct termios savettybuf;
void ttySaveSettings(void)
{
	isInteractive = isatty(0);
	if (isInteractive) {
		if (tcgetattr(0, &savettybuf))
			i_printfExit(MSG_IoctlError);
	}
}				/* ttySaveSettings */

static void ttyRestoreSettings(void)
{
	if (isInteractive)
		tcsetattr(0, TCSANOW, &savettybuf);
}				/* ttyRestoreSettings */

/* put the tty in raw mode.
 * Review your Unix manual on termio.
 * min>0 time>0:  return min chars, or as many as you have received
 *   when time/10 seconds have elapsed between characters.
 * min>0 time=0:  block until min chars are received.
 * min=0 time>0:  return 1 char, or 0 if the timer expires.
 * min=0 time=0:  nonblocking, return whatever chars have been received. */
static void ttyRaw(int charcount, int timeout, bool isecho)
{
	struct termios buf = savettybuf;	/* structure copy */
	buf.c_cc[VMIN] = charcount;
	buf.c_cc[VTIME] = timeout;
	buf.c_lflag &= ~(ICANON | ECHO);
	if (isecho)
		buf.c_lflag |= ECHO;
	tcsetattr(0, TCSANOW, &buf);
}				/* ttyRaw */

/* simulate MSDOS getche() system call */
int getche(void)
{
	char c;
	fflush(stdout);
	ttyRaw(1, 0, true);
	read(0, &c, 1);
	ttyRestoreSettings();
	return c;
}				/* getche */

int getch(void)
{
	char c;
	fflush(stdout);
	ttyRaw(1, 0, false);
	read(0, &c, 1);
	ttyRestoreSettings();
	return c;
}				/* getche */

#endif

char getLetter(const char *s)
{
	char c;
	while (true) {
		c = getch();
		if (strchr(s, c))
			break;
		printf("\a\b");
		fflush(stdout);
	}
	printf("%c", c);
	return c;
}				/* getLetter */

/* Parameters: message, default file name, must this file be new,
 * and can we except an input of white space,
 * that being converted to a single space. */
char *getFileName(int msg, const char *defname, bool isnew, bool ws)
{
	static char buf[ABSPATH];
	int l;
	char *p;
	bool allspace;

	while (true) {
		i_printf(msg);
		if (defname)
			printf("[%s] ", defname);
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
				return " ";
			if (!defname)
				continue;
/* make a copy just to be safe */
			l = strlen(defname);
			if (l >= ABSPATH)
				l = ABSPATH - 1;
			strncpy(buf, defname, l);
			buf[l] = 0;
			p = buf;
		} else
			defname = 0;
		if (isnew && fileTypeByName(p, false)) {
			i_printf(MSG_FileExists, p);
			defname = 0;
			continue;
		}
		return p;
	}
}				/* getFileName */

/* Protect a filename from expansion by the shell */
static const char shellmeta[] = "\\\n\t |&;<>(){}#'\"~$*?";
int shellProtectLength(const char *s)
{
	int l = 0;
	while (*s) {
		if (strchr(shellmeta, *s))
			++l;
		++l, ++s;
	}
	return l;
}				/* shellProtectLength */

void shellProtect(char *t, const char *s)
{
	while (*s) {
		if (strchr(shellmeta, *s))
			*t++ = '\\';
		*t++ = *s++;
	}
}				/* shellProtect */

static const char globmeta[] = "\\*?[]";
static int globProtectLength(const char *s)
{
	int l = 0;
	while (*s) {
		if (strchr(globmeta, *s))
			++l;
		++l, ++s;
	}
	return l;
}				/* globProtectLength */

static void globProtect(char *t, const char *s)
{
	while (*s) {
		if (strchr(globmeta, *s))
			*t++ = '\\';
		*t++ = *s++;
	}
}				/* globProtect */

/* loop through the files in a directory */
const char *nextScanFile(const char *base)
{
	static char *dirquoted;	// directory/*
	static glob_t g;
	static int baselen, word_idx, cnt;
	const char *s;
	char *t;
	int rc, flags;

	if (!dirquoted) {
		if (!base)
			base = ".";
		baselen = strlen(base);
		s = base + baselen;
		cnt = globProtectLength(base);
/* make room for /* */
		dirquoted = allocMem(cnt + 3);
		globProtect(dirquoted, base);
		t = dirquoted + cnt;
		if (s[-1] != '/')
			*t++ = '/', ++baselen;
		*t++ = '*';
		*t++ = 0;

		flags = GLOB_ERR;
		flags |= (showHiddenFiles ? GLOB_PERIOD : 0);
		rc = glob(dirquoted, flags, NULL, &g);
/* this call should not fail except for NOMATCH */
		if (rc && rc != GLOB_NOMATCH) {
			i_puts(MSG_NoDirNoList);
			free(dirquoted);
			dirquoted = 0;
			globfree(&g);
			return 0;
		}

		word_idx = 0;
	}

	while (word_idx < g.gl_pathc) {
		s = g.gl_pathv[word_idx++] + baselen;
		if (stringEqual(s, "."))
			continue;
		if (stringEqual(s, ".."))
			continue;
		return s;
	}			/* end loop over files in directory */

	globfree(&g);
	free(dirquoted);
	dirquoted = 0;
	return 0;
}				/* nextScanFile */

bool sortedDirList(const char *dir, struct lineMap ** map_p, int *count_p)
{
	const char *f;
	int linecount = 0, cap;
	struct lineMap *t, *map;

	cap = 128;
	map = t = allocZeroMem(cap * LMSIZE);

	while (f = nextScanFile(dir)) {
		if (linecount == cap) {
			cap *= 2;
			map = reallocMem(map, cap * LMSIZE);
			t = map + linecount;
		}
/* leave room for @ / newline */
		t->text = allocMem(strlen(f) + 3);
		strcpy((char *)t->text, f);
		t->ds1 = t->ds2 = 0;
		++t, ++linecount;
	}

	*count_p = linecount;
	*map_p = map;

	if (!linecount)
		return true;

/* glob sorts the entries, so no need to sort them here */

	return true;
}				/* sortedDirList */

/* Expand environment variables, then wild cards.
 * But the result should be one and only one file.
 * Return the new expanded line.
 * Neither the original line nore the new line is allocated.
 * They are static char buffers that are just plain long enough. */

static bool envExpand(const char *line, const char **expanded)
{
	const char *s;
	const char *v;
	char *t;
	bool inbrace;
	int l;
	static char varline[ABSPATH];
	char var1[40];

/* quick check */
	if (!strchr(line, '$')) {
		*expanded = line;
		return true;
	}

/* ok, need to crunch along */
	t = varline;
	for (s = line; *s; ++s) {
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
			setError(MSG_NoEnvVar);
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
}				/* envExpand */

bool envFile(const char *line, const char **expanded, bool expect_file)
{
	static char line2[ABSPATH];
	const char *varline;
	const char *s;
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
/* But first see if the user wants to glob */
	if (varline[0] == '~') {
		char c = varline[1];
		if (!c)
			goto doglob;
		if (c == '/')
			goto doglob;
		if (isalphaByte(c))
			goto doglob;
	}
	for (s = varline; *s; ++s)
		if (strchr("*?[", *s) && (s == varline || s[-1] != '\\'))
			goto doglob;

noglob:
	*expanded = varline;
	return true;

doglob:
	flags = (GLOB_NOSORT | GLOB_TILDE_CHECK);
	rc = glob(varline, flags, NULL, &g);

	if (rc == GLOB_NOMATCH) {
		if (!expect_file)
			goto noglob;
		setError(MSG_ShellNoMatch);
		return false;
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
}				/* envFile */

/* Call the above routine if filename contains a  slash,
 * or prepend the download directory if it does not.
 * If there is no download directory then always expand as above. */
bool envFileDown(const char *line, const char **expanded)
{
	static char line2[MAXTTYLINE];

	if (!downDir || strchr(line, '/'))
/* we don't necessarily expect there to be a file here */
		return envFile(line, expanded, false);

	if (strlen(downDir) + strlen(line) >= sizeof(line2) - 1) {
		setError(MSG_ShellLineLong);
		return false;
	}

	sprintf(line2, "%s/%s", downDir, line);
	*expanded = line2;
	return true;
}				/* envFileDown */

/* create the full pathname for a file that you are viewing in directory mode. */
/* This is static, with a limit on path length. */
char *makeAbsPath(const char *f)
{
	static char path[ABSPATH];
	if (strlen(cw->baseDirName) + strlen(f) > ABSPATH - 2) {
		setError(MSG_PathNameLong, ABSPATH);
		return 0;
	}
	sprintf(path, "%s/%s", cw->baseDirName, f);
	return path;
}				/* makeAbsPath */

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
		i_printfExit(MSG_OpenFail, name);
	else if (*mode == 'w' || *mode == 'a')
		i_printfExit(MSG_CreateFail, name);
	else
		i_printfExit(MSG_InvalidFopen, mode);
	return 0;
}				/* efopen */

int eopen(const char *name, int mode, int perms)
{
	int fd;
	fd = open(name, mode, perms);
	if (fd >= 0)
		return fd;
	if (mode & O_WRONLY)
		i_printfExit(MSG_CreateFail, name);
	else
		i_printfExit(MSG_OpenFail, name);
	return -1;
}				/* eopen */

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
}				/* appendFile */

/* like the above, but no formatting */
void appendFileNF(const char *filename, const char *msg)
{
	FILE *f = efopen(filename, "a");
	fprintf(f, "%s\n", msg);
	fclose(f);
}				/* appendFileNF */

/*********************************************************************
Routines to convert between names and IP addresses.
This is ipv4; need to write similar for ipv6.
*********************************************************************/

IP32bit tcp_name_ip(const char *name)
{
	struct hostent *hp;
	IP32bit *ip;

	hp = gethostbyname(name);
	if (!hp)
		return NULL_IP;
	ip = (IP32bit *) * (hp->h_addr_list);
	if (!ip)
		return NULL_IP;
	return *ip;
}				/* tcp_name_ip */

char *tcp_ip_dots(IP32bit ip)
{
	return inet_ntoa(*(struct in_addr *)&ip);
}				/* tcp_ip_dots */

int tcp_isDots(const char *s)
{
	const char *t;
	char c;
	int nd = 0;		/* number of dots */
	if (!s)
		return 0;
	for (t = s; (c = *t); ++t) {
		if (c == '.') {
			++nd;
			if (t == s || !t[1])
				return 0;
			if (t[-1] == '.' || t[1] == '.')
				return 0;
			continue;
		}
		if (!isdigit(c))
			return 0;
	}
	return (nd == 3);
}				/* tcp_isDots */

IP32bit tcp_dots_ip(const char *s)
{
	struct in_addr a;
/* this for SCO unix */
#ifdef SCO
	inet_aton(s, &a);
#else
	*(IP32bit *) & a = inet_addr(s);
#endif
	return *(IP32bit *) & a;
}				/* tcp_dots_ip */
