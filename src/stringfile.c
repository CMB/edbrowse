/* stringfile.c: manage strings, files, and directories for edbrowse.
* Copyright (c) Karl Dahlke, 2008
* This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"

#include <sys/stat.h>
#ifdef DOSLIKE
#include <dos.h>
#else
#include <dirent.h>
#ifdef SYSBSD
#include <termios.h>
typedef struct termios termstruct;
#define TTY_GET_COMMAND TIOCGETA
#define TTY_SET_COMMAND TIOCSETA
#else
#include <termio.h>
typedef struct termio termstruct;
#define TTY_GET_COMMAND TCGETA
#define TTY_SET_COMMAND TCSETA
#endif
#include <sys/utsname.h>
#endif
#include <pcre.h>


/*********************************************************************
Allocate and manage memory.
Allocate and copy strings.
If we're out of memory, the program aborts.  No error legs.
*********************************************************************/

void *
allocMem(unsigned n)
{
    void *s;
    if(!n)
	return EMPTYSTRING;
    if(!(s = malloc(n)))
	i_printfExit(MSG_MemAllocError, n);
    return s;
}				/* allocMem */

void *
allocZeroMem(unsigned n)
{
    void *s;
    if(!n)
	return EMPTYSTRING;
    if(!(s = calloc(n, 1)))
	i_printfExit(MSG_MemCallocError, n);
    return s;
}				/* allocZeroMem */

void *
reallocMem(void *p, unsigned n)
{
    void *s;
    if(!n)
	i_printfExit(MSG_ReallocP);
    if(!p)
	i_printfExit(MSG_Realloc0, n);
    if(p == EMPTYSTRING)
	return allocMem(n);
    if(!(s = realloc(p, n)))
	i_printfExit(MSG_ErrorRealloc, n);
    return s;
}				/* reallocMem */

void
nzFree(void *s)
{
    if(s && s != EMPTYSTRING)
	free(s);
}				/* nzFree */

uchar
fromHex(char d, char e)
{
    d |= 0x20, e |= 0x20;
    if(d >= 'a')
	d -= ('a' - '9' - 1);
    if(e >= 'a')
	e -= ('a' - '9' - 1);
    d -= '0', e -= '0';
    return ((((uchar) d) << 4) | (uchar) e);
}				/* fromHex */

char *
appendString(char *s, const char *p)
{
    int slen = strlen(s);
    int plen = strlen(p);
    s = reallocMem(s, slen + plen + 1);
    strcpy(s + slen, p);
    return s;
}				/* appendstring */

char *
prependString(char *s, const char *p)
{
    int slen = strlen(s);
    int plen = strlen(p);
    char *t = allocMem(slen + plen + 1);
    strcpy(t, p);
    strcpy(t + plen, s);
    nzFree(s);
    return t;
}				/* prependstring */

void
skipWhite(const char **s)
{
    const char *t = *s;
    while(isspaceByte(*t))
	++t;
    *s = t;
}				/* skipWhite */

void
stripWhite(char *s)
{
    const char *t = s;
    char *u;
    skipWhite(&t);
    if(t > s)
	strcpy(s, t);
    u = s + strlen(s);
    while(u > s && isspaceByte(u[-1]))
	--u;
    *u = 0;
}				/* stripWhite */

/* compress white space */
void
spaceCrunch(char *s, bool onespace, bool unprint)
{
    int i, j;
    char c;
    bool space = true;
    for(i = j = 0; c = s[i]; ++i) {
	if(isspaceByte(c)) {
	    if(!onespace)
		continue;
	    if(!space)
		s[j++] = ' ', space = true;
	    continue;
	}
	if(unprint && !isprintByte(c))
	    continue;
	s[j++] = c, space = false;
    }
    if(space && j)
	--j;			/* drop trailing space */
    s[j] = 0;
}				/* spaceCrunch */

/* OO has a lot of unnecessary overhead, and a few inconveniences,
 * but I really miss it right now.  The following
 * routines make up for the lack of simple string concatenation in C.
 * The string space allocated is always a power of 2 - 1, starting with 1.
 * Each of these routines puts an extra 0 on the end of the "string". */

char *
initString(int *l)
{
    *l = 0;
    return EMPTYSTRING;
}

void
stringAndString(char **s, int *l, const char *t)
{
    char *p = *s;
    int oldlen, newlen, x;
    oldlen = *l;
    newlen = oldlen + strlen(t);
    *l = newlen;
    ++newlen;			/* room for the 0 */
    x = oldlen ^ newlen;
    if(x > oldlen) {		/* must realloc */
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

void
stringAndBytes(char **s, int *l, const char *t, int cnt)
{
    char *p = *s;
    int oldlen, newlen, x;
    oldlen = *l;
    newlen = oldlen + cnt;
    *l = newlen;
    ++newlen;
    x = oldlen ^ newlen;
    if(x > oldlen) {		/* must realloc */
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

void
stringAndChar(char **s, int *l, char c)
{
    char *p = *s;
    int oldlen, newlen, x;
    oldlen = *l;
    newlen = oldlen + 1;
    *l = newlen;
    ++newlen;
    x = oldlen ^ newlen;
    if(x > oldlen) {		/* must realloc */
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

void
stringAndNum(char **s, int *l, int n)
{
    char a[16];
    sprintf(a, "%d", n);
    stringAndString(s, l, a);
}				/* stringAndNum */

/* 64M 16K etc */
void
stringAndKnum(char **s, int *l, int n)
{
    char a[16];
    if(n && n / (1024 * 1024) * (1024 * 1024) == n)
	sprintf(a, "%dM", n / (1024 * 1024));
    else if(n && n / 1024 * 1024 == n)
	sprintf(a, "%dK", n / 1024);
    else
	sprintf(a, "%d", n);
    stringAndString(s, l, a);
}				/* stringAndKnum */

char *
cloneString(const char *s)
{
    char *t;
    unsigned len;

    if(!s)
	return 0;
    if(!*s)
	return EMPTYSTRING;
    len = strlen(s) + 1;
    t = allocMem(len);
    strcpy(t, s);
    return t;
}				/* cloneString */

char *
cloneMemory(const char *s, int n)
{
    char *t = allocMem(n);
    if(n)
	memcpy(t, s, n);
    return t;
}				/* cloneMemory */

void
clipString(char *s)
{
    int len;
    if(!s)
	return;
    len = strlen(s);
    while(--len >= 0)
	if(!isspaceByte(s[len]))
	    break;
    s[len + 1] = 0;
}				/* clipString */

char *
Cify(const char *s, int n)
{
    char *u;
    char *t = allocMem(n + 1);
    if(n)
	memcpy(t, s, n);
    for(u = t; u < t + n; ++u)
	if(*u == 0)
	    *u = ' ';
    *u = 0;
    return t;
}				/* Cify */

/* pull a substring out of a larger string,
 * and make it its own allocated string */
char *
pullString(const char *s, int l)
{
    char *t;
    if(!l)
	return EMPTYSTRING;
    t = allocMem(l + 1);
    memcpy(t, s, l);
    t[l] = 0;
    return t;
}				/* pullString */

char *
pullString1(const char *s, const char *t)
{
    return pullString(s, t - s);
}

int
stringIsNum(const char *s)
{
    int n;
    if(!isdigitByte(s[0]))
	return -1;
    n = strtol(s, (char **)&s, 10);
    if(*s)
	return -1;
    return n;
}				/* stringIsNum */

bool
stringIsFloat(const char *s, double *dp)
{
    const char *t;
    *dp = strtod(s, (char **)&t);
    if(*t)
	return false;		/* extra stuff at the end */
    return true;
}				/* stringIsFloat */

bool
stringIsPDF(const char *s)
{
    int j = 0;
    if(s)
	j = strlen(s);
    return j >= 5 && stringEqual(s + j - 4, ".pdf");
}				/* stringIsPDF */

bool
isSQL(const char *s)
{
    char c;
    const char *c1 = 0, *c2 = 0;
    c = *s;
    if(!isalphaByte(c))
	goto no;
    for(++s; c = *s; ++s) {
	if(c == '_')
	    continue;
	if(isalnumByte(c))
	    continue;
	if(c == ':') {
	    if(c1)
		goto no;
	    c1 = s;
	    continue;
	}
	if(c == ']') {
	    c2 = s;
	    goto yes;
	}
    }
  no:
    return false;
  yes:
    return true;
}				/* isSQL */

bool
memEqualCI(const char *s, const char *t, int len)
{
    char c, d;
    while(len--) {
	c = *s, d = *t;
	if(islowerByte(c))
	    c = toupper(c);
	if(islowerByte(d))
	    d = toupper(d);
	if(c != d)
	    return false;
	++s, ++t;
    }
    return true;
}				/* memEqualCI */

char *
strstrCI(const char *base, const char *search)
{
    int l = strlen(search);
    while(*base) {
	if(memEqualCI(base, search, l))
	    return (char *)base;
	++base;
    }
    return 0;
}				/* strstrCI */

bool
stringEqualCI(const char *s, const char *t)
{
    char c, d;
    while((c = *s) && (d = *t)) {
	if(islowerByte(c))
	    c = toupper(c);
	if(islowerByte(d))
	    d = toupper(d);
	if(c != d)
	    return false;
	++s, ++t;
    }
    if(*s)
	return false;
    if(*t)
	return false;
    return true;
}				/* stringEqualCI */

int
stringInList(const char *const *list, const char *s)
{
    int i = 0;
    if(!list)
	i_printfExit(MSG_NullStrList);
    if(s)
	while(*list) {
	    if(stringEqual(s, *list))
		return i;
	    ++i;
	    ++list;
	}
    return -1;
}				/* stringInList */

int
stringInListCI(const char *const *list, const char *s)
{
    int i = 0;
    if(!list)
	i_printfExit(MSG_NullStrListCI);
    if(s)
	while(*list) {
	    if(stringEqualCI(s, *list))
		return i;
	    ++i;
	    ++list;
	}
    return -1;
}				/* stringInListCI */

int
charInList(const char *list, char c)
{
    char *s;
    if(!list)
	i_printfExit(MSG_NullCharInList);
    s = strchr(list, c);
    if(!s)
	return -1;
    return s - list;
}				/* charInList */

/* In an empty list, next and prev point back to the list, not to 0. */
/* We also allow zero. */
bool
listIsEmpty(const struct listHead * l)
{
    return l->next == l || l->next == 0;
}				/* listIsEmpty */

void
initList(struct listHead *l)
{
    l->prev = l->next = l;
}				/* initList */

void
delFromList(void *x)
{
    struct listHead *xh = x;
    ((struct listHead *)xh->next)->prev = xh->prev;
    ((struct listHead *)xh->prev)->next = xh->next;
}				/* delFromList */

void
addToListFront(struct listHead *l, void *x)
{
    struct listHead *xh = x;
    xh->next = l->next;
    xh->prev = l;
    l->next = x;
    ((struct listHead *)xh->next)->prev = x;
}				/* addToListFront */

void
addToListBack(struct listHead *l, void *x)
{
    struct listHead *xh = x;
    xh->prev = l->prev;
    xh->next = l;
    l->prev = x;
    ((struct listHead *)xh->prev)->next = x;
}				/* addToListBack */

void
addAtPosition(void *p, void *x)
{
    struct listHead *xh = x;
    struct listHead *ph = p;
    xh->prev = p;
    xh->next = ph->next;
    ph->next = x;
    ((struct listHead *)xh->next)->prev = x;
}				/* addAtPosition */

void
freeList(struct listHead *l)
{
    while(!listIsEmpty(l)) {
	void *p = l->next;
	delFromList(p);
	nzFree(p);
    }
}				/* freeList */

/* like isalnumByte, but allows _ and - */
bool
isA(char c)
{
    if(isalnumByte(c))
	return true;
    return (c == '_' || c == '-');
}				/* isA */

bool
isquote(char c)
{
    return c == '"' || c == '\'';
}				/* isquote */

/* print an error message */
void
errorPrint(const char *msg, ...)
{
    char bailflag = 0;
    va_list p;

    va_start(p, msg);

    if(*msg == '@') {
	bailflag = 1;
	++msg;
/* I should internationalize this, but it's never suppose to happen! */
	fprintf(stderr, "disaster, ");
    } else if(isdigitByte(*msg)) {
	bailflag = *msg - '0';
	++msg;
    }

    vfprintf(stderr, msg, p);
    fprintf(stderr, "\n");
    va_end(p);

    if(bailflag)
	exit(bailflag);
}				/* errorPrint */

void
debugPrint(int lev, const char *msg, ...)
{
    va_list p;
    if(lev > debugLevel)
	return;
    va_start(p, msg);
    vprintf(msg, p);
    va_end(p);
    nl();
    if(lev == 0 && !memcmp(msg, "warning", 7))
	eeCheck();
}				/* debugPrint */

void
nl(void)
{
    puts("");
}				/* nl */

/* Turn perl string into C string, and complain about nulls. */
int
perl2c(char *t)
{
    int n = 0;
    while(*t != '\n') {
	if(*t == 0)
	    ++n;
	++t;
    }
    *t = 0;			/* now it's a C string */
    return n;			/* number of nulls */
}				/* perl2c */

/* The length of a perl string includes its terminating newline */
unsigned
pstLength(pst s)
{
    pst t;
    if(!s)
	i_printfExit(MSG_NullPtr);
    t = s;
    while(*t != '\n')
	++t;
    return t + 1 - s;
}				/* pstLength */

pst
clonePstring(pst s)
{
    pst t;
    unsigned len;
    if(!s)
	return s;
    len = pstLength(s);
    t = allocMem(len);
    memcpy(t, s, len);
    return t;
}				/* clonePstring */

void
copyPstring(pst s, const pst t)
{
    int len = pstLength(t);
    memcpy(s, t, len);
}				/* copyPstring */

bool
fileIntoMemory(const char *filename, char **data, int *len)
{
    int length, n, fh;
    char *buf;
    char ftype = fileTypeByName(filename, false);
    if(ftype && ftype != 'f') {
	setError(MSG_RegularFile, filename);
	return false;
    }
    fh = open(filename, O_RDONLY | O_BINARY);
    if(fh < 0) {
	setError(MSG_NoOpen, filename);
	return false;
    }
    length = fileSizeByName(filename);
    if(length < 0) {
	close(fh);
	return false;
    }				/* should never hapen */
    if(length > maxFileSize) {
	setError(MSG_LargeFile);
	close(fh);
	return false;
    }
    buf = allocMem(length + 2);
    n = 0;
    if(length)
	n = read(fh, buf, length);
    close(fh);			/* don't need that any more */
    if(n < length) {
	setError(MSG_NoRead2, filename);
	free(buf);
	return false;
    }
    *data = buf;
    *len = length;
    return true;
}				/* fileIntoMemory */

/* inverse of the above */
bool
memoryOutToFile(const char *filename, const char *data, int len,
/* specify the error messages */
   int msgcreate, int msgwrite)
{
    int fh = open(filename, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0666);
    if(fh < 0) {
	setError(msgcreate, filename, errno);
	return false;
    }
    if(write(fh, data, len) < len) {
	setError(msgwrite, filename, errno);
	close(fh);
	return false;
    }
    close(fh);
    return true;
}				/* memoryOutToFile */

/* shift string to upper, lower, or mixed case */
/* action is u, l, or m. */
void
caseShift(char *s, char action)
{
    char c;
    int mc = 0;
    bool ws = true;

    for(; c = *s; ++s) {
	if(action == 'u') {
	    if(isalphaByte(c))
		*s = toupper(c);
	    continue;
	}
	if(action == 'l') {
	    if(isalphaByte(c))
		*s = tolower(c);
	    continue;
	}
/* mixed case left */
	if(isalphaByte(c)) {
	    if(ws)
		c = toupper(c);
	    else
		c = tolower(c);
	    if(ws && c == 'M')
		mc = 1;
	    else if(mc == 1 && c == 'c')
		mc = 2;
	    else if(mc == 2) {
		c = toupper(c);
		mc = 0;
	    } else
		mc = 0;
	    *s = c;
	    ws = false;
	    continue;
	}
	ws = true, mc = 0;
    }				/* loop */
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

char
fileTypeByName(const char *name, bool showlink)
{
    struct stat buf;
    bool islink = false;
    char c;
    int mode;
    if(lstat(name, &buf)) {
	setError(MSG_NoAccess, name);
	return 0;
    }
    mode = buf.st_mode & S_IFMT;
    if(mode == S_IFLNK) {	/* symbolic link */
	islink = true;
/* If this fails, I'm guessing it's just a file. */
	if(stat(name, &buf))
	    return (showlink ? 'F' : 0);
	mode = buf.st_mode & S_IFMT;
    }
    c = 'f';
    if(mode == S_IFDIR)
	c = 'd';
#ifndef DOSLIKE
/* I don't think these are Windows constructs. */
    if(mode == S_IFBLK)
	c = 'b';
    if(mode == S_IFCHR)
	c = 'c';
    if(mode == S_IFIFO)
	c = 'p';
    if(mode == S_IFSOCK)
	c = 's';
#endif
    if(islink & showlink)
	c = toupper(c);
    return c;
}				/* fileTypeByName */

char
fileTypeByHandle(int fd)
{
    struct stat buf;
    char c;
    int mode;
    if(fstat(fd, &buf)) {
	setError(MSG_NoAccess, "handle");
	return 0;
    }
    mode = buf.st_mode & S_IFMT;
    c = 'f';
    if(mode == S_IFDIR)
	c = 'd';
#ifndef DOSLIKE
/* I don't think these are Windows constructs. */
    if(mode == S_IFBLK)
	c = 'b';
    if(mode == S_IFCHR)
	c = 'c';
    if(mode == S_IFIFO)
	c = 'p';
    if(mode == S_IFSOCK)
	c = 's';
#endif
    return c;
}				/* fileTypeByHandle */

int
fileSizeByName(const char *name)
{
    struct stat buf;
    if(stat(name, &buf)) {
	setError(MSG_NoAccess, name);
	return -1;
    }
    return buf.st_size;
}				/* fileSizeByName */

time_t
fileTimeByName(const char *name)
{
    struct stat buf;
    if(stat(name, &buf)) {
	setError(MSG_NoAccess, name);
	return -1;
    }
    return buf.st_mtime;
}				/* fileSizeByName */

#ifndef DOSLIKE

static termstruct savettybuf;
void
ttySaveSettings(void)
{
    isInteractive = isatty(0);
    if(isInteractive) {
	if(ioctl(0, TTY_GET_COMMAND, &savettybuf))
	    i_printfExit(MSG_IoctlError);
    }
}				/* ttySaveSettings */

static void
ttyRestoreSettings(void)
{
    if(isInteractive)
	ioctl(0, TTY_SET_COMMAND, &savettybuf);
}				/* ttyRestoreSettings */

/* put the tty in raw mode.
 * Review your Unix manual on termio.
 * min>0 time>0:  return min chars, or as many as you have received
 *   when time/10 seconds have elapsed between characters.
 * min>0 time=0:  block until min chars are received.
 * min=0 time>0:  return 1 char, or 0 if the timer expires.
 * min=0 time=0:  nonblocking, return whatever chars have been received. */
static void
ttyRaw(int charcount, int timeout, bool isecho)
{
    termstruct buf = savettybuf;	/* structure copy */
    buf.c_cc[VMIN] = charcount;
    buf.c_cc[VTIME] = timeout;
    buf.c_lflag &= ~(ICANON | ECHO);
    if(isecho)
	buf.c_lflag |= ECHO;
    ioctl(0, TTY_SET_COMMAND, &buf);
}				/* ttyRaw */

/* simulate MSDOS getche() system call */
int
getche(void)
{
    char c;
    fflush(stdout);
    ttyRaw(1, 0, true);
    read(0, &c, 1);
    ttyRestoreSettings();
    return c;
}				/* getche */

int
getch(void)
{
    char c;
    fflush(stdout);
    ttyRaw(1, 0, false);
    read(0, &c, 1);
    ttyRestoreSettings();
    return c;
}				/* getche */

#endif

char
getLetter(const char *s)
{
    char c;
    while(true) {
	c = getch();
	if(strchr(s, c))
	    break;
	printf("\a\b");
	fflush(stdout);
    }
    printf("%c", c);
    return c;
}				/* getLetter */

/* loop through the files in a directory */
/* Hides the differences between DOS, Unix, and NT. */
static bool dirstart = true;

char *
nextScanFile(const char *base)
{
    char *s;
#ifdef DOSLIKE
    static char global[] = "/*.*";
    bool rc;
    short len;
    char *p;
    bool allocate = false;
#ifdef MSDOS
    static struct _find_t dta;
#else
    static struct _finddata_t dta;
    static int handle;
#endif
#else
    struct dirent *de;
    static DIR *df;
#endif

#ifdef DOSLIKE
    if(dirstart) {
	if(base) {
	    len = strlen(base) - 1;
	    p = allocMem(len + 6);
	    strcpy(p, base);
	    allocate = true;
	    if(p[len] == '/' || p[len] == '\\')
		p[len] = 0;
	    strcat(p, global);
	} else
	    p = global +1;
#ifdef MSDOS
	rc = _dos_findfirst(p, (showHiddenFiles ? 077 : 073), &dta);
#else
	rc = false;
	handle = _findfirst(p, &dta);
	if(handle < 0)
	    rc = true;
#endif
	if(allocate)
	    nzFree(p);
    }
#else
    if(!df) {
	if(!base)
	    base = ".";
	df = opendir(base);
	if(!df) {
	    i_puts(MSG_NoDirNoList);
	    return 0;
	}
    }
#endif

#ifdef DOSLIKE
    while(true) {
/* read the next file */
	if(!dirStart) {
#ifdef MSDOS
	    rc = _dos_findnext(&dta);
#else
	    rc = _findnext(handle, &dta);
#endif
	    dirstart = false;
	}
	if(rc)
	    break;
/* extract the base name */
	s = strrchr(dta.name, '/');
	s = s ? s + 1 : dta.name;
/* weed out unwanted directories */
	if(stringEqual(s, "."))
	    continue;
	if(stringEqual(s, ".."))
	    continue;
	return s;
    }				/* end loop over files in directory */
#else
    while(de = readdir(df)) {
	if(de->d_ino == 0)
	    continue;
	if(de->d_name[0] == '.') {
	    if(!showHiddenFiles)
		continue;
	    if(de->d_name[1] == 0)
		continue;
	    if(de->d_name[1] == '.' && de->d_name[2] == 0)
		continue;
	}
	return de->d_name;
    }				/* end loop over files in directory */
    closedir(df);
    df = 0;
#endif

    return 0;
}				/* nextScanFile */

/* Sorted directory list.  Uses textLines[]. */
bool
sortedDirList(const char *dir, int *start, int *end)
{
    char *f;
    int j;
    bool change;

    *start = *end = textLinesCount;

    while(f = nextScanFile(dir)) {
	if(!linesComing(1))
	    return false;
	textLines[textLinesCount] = allocMem(strlen(f) + 3);
	strcpy((char *)textLines[textLinesCount], f);
	textLinesCount++;
    }

    *end = textLinesCount;
    if(*end == *start)
	return true;

/* Bubble sort, the list shouldn't be too long. */
    change = true;
    while(change) {
	change = false;
	for(j = *start; j < *end - 1; ++j) {
	    if(strcmp((char *)textLines[j], (char *)textLines[j + 1]) > 0) {
		pst swap = textLines[j];
		textLines[j] = textLines[j + 1];
		textLines[j + 1] = swap;
		change = true;
	    }
	}
    }

    return true;
}				/* sortedDirList */

/* Expand environment variables, then wild cards.
 * But the result should be one and only one file.
 * Return the new expanded line.
 * Neither the original line nore the new line is allocated.
 * They are static char buffers that are just plain long enough. */

bool
envFile(const char *line, const char **expanded)
{
    static char line1[MAXTTYLINE];
    static char line2[MAXTTYLINE];
    const char *s, *value, *basedir;
    char *t, *dollar, *cut, *file;
    char c;
    bool cc, badBrackets;
    int filecount;
    char re[MAXRE + 20];
    const char *re_error;
    int re_offset, re_count, re_vector[3];

    pcre *re_cc;

/* ` supresses this stuff */
    if(*line == '`') {
	*expanded = line + 1;
	return true;
    }

/* quick check, nothing to do */
    if(!strpbrk(line, "$[*?") && line[0] != '~') {
	*expanded = line;
	return true;
    }

/* first the env variables */
    s = line;
    t = line1;
    dollar = 0;
    while(true) {
	if(t >= line1 + sizeof (line1))
	    goto longvar;
	c = *s;
	if(c == '~' && s == line && (s[1] == '/' || s[1] == 0)) {
	    if(strlen(home) >= sizeof (line1) - 1)
		goto longvar;
	    strcpy(t, home);
	    t = t + strlen(t);
	    ++s;
	    continue;
	}
	if(dollar && !isalnumByte(c) && c != '_') {
	    *t = 0;
	    value = getenv(dollar + 1);
	    if(!value) {
		setError(MSG_NoEnvVar, dollar + 1);
		return false;
	    }
	    if(dollar + strlen(value) >= line1 + sizeof (line1) - 1)
		goto longvar;
	    strcpy(dollar, value);
	    t = dollar + strlen(dollar);
	    dollar = 0;
	}
	if(c == '$' && (s[1] == '_' || isalphaByte(s[1])))
	    dollar = t;
	*t++ = c;
	if(!c)
	    break;
	++s;
    }

/* Wildcard expansion, but somewhat limited.
 * The directory has to be hard coded;
 * we only expand stars in the filename. */

    cut = dollar = 0;
    for(t = line1; *t; ++t) {
	c = *t;
	if(c == '/')
	    cut = t;
	if(c == '[' || c == '*' || c == '?')
	    dollar = t;
    }
    if(!dollar) {		/* nothing meta */
	*expanded = line1;
	return true;
    }
    if(cut && dollar < cut) {
	setError(MSG_EarlyExpand);
	return false;
    }

/* Make sure its a directory, and build a perl regexp for the pattern. */
    if(cut) {
	*cut = 0;
	if(cut > line1 && fileTypeByName(line1, false) != 'd') {
	    setError(MSG_NoAccessDir, line1);
	    return false;
	}
    }

    s = cut ? cut + 1 : line1;
    t = re;
    *t++ = '^';
    cc = badBrackets = false;
    while(c = *s) {
	if(t >= re + sizeof (re) - 3) {
	    setError(MSG_ShellPatternLong);
	    return false;
	}
	if(c == '\\') {
	    setError(MSG_ExpandBackslash);
	    return false;
	}
/* things we need to escape */
	if(strchr("().+|", c))
	    *t++ = '\\';
	if(c == '?')
	    c = '.';
	if(c == '*')
	    *t++ = '.';
	*t++ = c;
	if(c == '[') {
	    if(cc)
		badBrackets = true;
	    cc = true;
	}
	if(c == ']') {
	    if(!cc)
		badBrackets = true;
	    cc = false;
	}
	++s;
    }				/* loop over shell pattern */
    *t++ = '$';
    *t = 0;
    if(badBrackets | cc) {
	setError(MSG_ShellSyntax);
	return false;
    }

    debugPrint(7, "shell regexp %s", re);
    re_cc = pcre_compile(re, 0, &re_error, &re_offset, 0);
    if(!re_cc) {
	setError(MSG_ShellCompile, re_error);
	return false;
    }

    filecount = 0;
    cc = false;			/* long flag */
    basedir = 0;
    if(cut) {
	if(cut == line1)
	    basedir = "/";
	else
	    basedir = line1;
    }
    while(file = nextScanFile(basedir)) {
	if(filecount > 1)
	    continue;
	re_count = pcre_exec(re_cc, 0, file, strlen(file), 0, 0, re_vector, 3);
	if(re_count < -1) {
	    pcre_free(re_cc);
	    setError(MSG_ShellExpand);
	    return false;
	}
	if(re_count < 0)
	    continue;
	++filecount;
	if(filecount > 1)
	    continue;
	if((cut ? strlen(line1) : 0) + strlen(file) >= sizeof (line2) - 2)
	    cc = true;
	else if(cut)
	    sprintf(line2, "%s/%s", line1, file);
	else
	    strcpy(line2, file);
    }
    pcre_free(re_cc);
    if(filecount != 1) {
	setError((filecount > 0) + MSG_ShellNoMatch);
	return false;
    }
    if(cc)
	goto longvar;

    *expanded = line2;
    return true;

  longvar:
    setError(MSG_ShellLineLong);
    return false;
}				/* envFile */

static struct utsname utsbuf;

const char *
currentOS(void)
{
    uname(&utsbuf);
    return utsbuf.sysname;
}				/* currentOS */

const char *
currentMachine(void)
{
    uname(&utsbuf);
    return utsbuf.machine;
}				/* currentMachine */

FILE *
efopen(const char *name, const char *mode)
{
    FILE *f;

    if(name[0] == '-' && name[1] == 0) {
	if(*mode == 'r')
	    return stdin;
	if(*mode == 'w' || *mode == 'a')
	    return stdout;
    }

    f = fopen(name, mode);
    if(f)
	return f;

    if(*mode == 'r')
	i_printfExit(MSG_OpenFail, name);
    else if(*mode == 'w' || *mode == 'a')
	i_printfExit(MSG_CreateFail, name);
    else
	i_printfExit(MSG_InvalidFopen, mode);
    return 0;
}				/* efopen */

void
appendFile(const char *fname, const char *message, ...)
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
void
appendFileNF(const char *filename, const char *msg)
{
    FILE *f = efopen(filename, "a");
    fprintf(f, "%s\n", msg);
    fclose(f);
}				/* appendFileNF */
