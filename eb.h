/* eb.h
 * Copyright (c) Karl Dahlke, 2006
 * This file is part of the edbrowse project, released under GPL.
 */

#ifndef EB_H
#define EB_H 1

/* the symbol DOSLIKE is used to conditionally compile those constructs
 * that are common to DOS and NT, but not typical of Unix. */
#ifdef MSDOS
#define DOSLIKE 1
#endif
#ifdef _WIN32
#define DOSLIKE 1
#endif

/* seems like everybody needs these header files */
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#ifdef DOSLIKE
#include <io.h>
#else
#include <unistd.h>
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifndef O_TEXT
#define O_TEXT 0
#endif
#ifndef O_SYNC
#define O_SYNC 0
#endif

/* WARNING:  the following typedef is pseudo-standard in C.
 * Some systems will define ushort in sys/types.h, others will not.
 * Unfortunately there is no #define symbol to key on;
 * no way to conditionally compile the following statement. */
#ifdef DOSLIKE
typedef unsigned short ushort;
typedef unsigned long ulong;
#endif
/* sys/types.h defines unsigned char as unchar.  I prefer uchar.
 * It is consistent with ushort uint and ulong, and doesn't remind
 * me of the uncola, a char that isn't really a char. */
typedef unsigned char uchar;

/* We use uchar for boolean fields. */
typedef uchar bool;
#define true 1
#define false 0

typedef ushort idNameCode;

#define stringEqual !strcmp

/* http encoding, content type, content transfer encoding */
enum { ENC_PLAIN, ENC_COMPRESS, ENC_GZIP, ENC_URL, ENC_MFD };
enum { CT_OTHER, CT_TEXT, CT_HTML, CT_RICH, CT_APPLIC, CT_MULTI, CT_ALT };
enum { CE_7BIT, CE_8BIT, CE_QP, CE_64 };

/* This program was originally written in perl.
 * So I got use to perl strings, which admit nulls.
 * In our case, they will be terminated by newline. */
typedef uchar *pst;		/* perl string */

/* A specific nonascii char denotes an html tag
 * in the rendered html text.
 * See the comments in bufsub.c for the rationale. */
#define InternalCodeChar '\x80'
#define InternalCodeCharAlternate '\x81'

/* Unix/Linux has a max absolute path length of 256, I think. */
#define ABSPATH 256

/* How long can a regular expression be? */
#define MAXRE 400
/* How long can an entered line be? */
#define MAXTTYLINE 256
/* Longest line that can take the substitute command */
#define REPLACELINELEN 50000
/* How many pages can we fetch, through frames etc. */
#define MAXFETCH 100
/* The longest string, in certain contexts. */
#define MAXSTRLEN 1024
/* How about user login and password? */
#define MAXUSERPASS 40
/* Number of pop3 mail accounts */
#define MAXACCOUNT 100
/* How many sessions open concurrently */
#define MAXSESSION 1000
/* Allocation increment for a growing string, that we don't expect
 * to get too large.  This must be a power of 2. */
#define ALLOC_GR        0x100

/* alignments */
#define AL_LEFT		0
#define AL_CENTER	1
#define AL_RIGHT	2
#define AL_BLOCK	3
#define AL_NO		4

/* left and top borders for dialog box, stubs. */
#define DIALOG_LB 1
#define DIALOG_TB 1
#define COLOR_DIALOG_TEXT 0
#define G_BFU_FONT_SIZE 0

/* the version of edbrowse */
extern const char *version;
extern const char eol[];	/* internet end of line */
extern char EMPTYSTRING[];	/* use this whenever you would use "" */

/* Here are the strings you can send out to identify this browser.
 * Most of the time we will send out the first string, edbrowse-2.15.3.
 * But sometimes we have to lie.
 * When you deal with clickbank, for instance, they won't let you in the
 * door unless you are one of three approved browsers.
 * I've written to them about this particular flavor of stupidity,
 * but they obviously don't care.  So lie!
 * Tell them you're Explorer, and walk right in.
 * Anyways, this array holds up to 10 user agent strings. */
extern char *userAgents[10], *currentAgent;

struct MACCOUNT {		/* pop3 account */
    char *login, *password, *from, *reply;
    char *inurl, *outurl;
    int inport, outport;
};

/* various globals */
extern int debugLevel;		/* 0 to 9 */
extern int webTimeout, mailTimeout;
extern int browseLine;		/* line number, for error reporting */
extern bool ismc;		/* Is the program running as a mail client? */
extern bool zapMail;		/* just get rid of the mail */
extern bool js_redirects;	/* window.location = new_url */
extern uchar browseLocal;	/* browsing a local file */
extern bool parsePage;		/* parsing the html page, and any java therein */
extern bool htmlAttrVal_nl;	/* allow nl in the attribute of an html tag */
extern bool unformatMail;	/* suppress formatting */
extern bool passMail;		/* pass mail across the filters */
extern bool errorExit;		/* exit on any error, for scripting purposes */
extern bool isInteractive;
extern bool intFlag;		/* set this when interrupt signal is caught */
extern bool binaryDetect;
extern const char opint[];
extern bool inInput;		/* reading line from standard in */
extern int fileSize;		/* when reading/writing files */
extern int maxFileSize;		/* max size of an editable file */
extern int mssock;		/* mail server socket */
extern int hcode;		/* http code, like 404 file not found */
extern char herror[];		/* html error */
extern char errorMsg[];		/* generated error message */
extern char serverLine[MAXTTYLINE];	/* lines to and from the mail server */
extern void *jcx;		/* javascript context */
extern void *jwin;		/* javascript window object */
extern void *jdoc;		/* javascript document object */
extern void *jwloc;		/* javascript location object */
extern int maxAccount;		/* how many email accounts specified */
extern int localAccount;	/* this is the smtp server for outgoing mail */
extern struct MACCOUNT accounts[];	/* all the email accounts */
extern char *mailDir;		/* move to this directory when fetching mail */
extern bool caseInsensitive, searchStringsAll, displayAlt;
extern bool textAreaDosNewlines;	/* when transmitting a textarea */
extern bool undoable;		/* an undoable operation is taking place */
extern bool allowRedirection;	/* from http code 301, or http refresh */
extern bool sendReferrer;	/* in the http header */
extern bool allowJS;		/* javascript on */
extern bool helpMessagesOn;	/* no need to type h */
extern bool verifyCertificates;	/* ssl protocol */
extern char ftpMode;
extern bool showHiddenFiles;	/* during directory scan */
extern uchar dirWrite;		/* directory write mode, e.g. rename files */
extern uchar endMarks;		/* do we print ^ $ at the start and end of lines? */
extern int context;		/* which session (buffer) are we in? */
extern uchar linePending[];
extern char *changeFileName;
extern char *addressFile;	/* your address book */
extern char *ipbFile;		/* file holding ip blacklist */
extern char *serverData;
extern int serverDataLen;
extern char replaceLine[];
extern char *currentReferrer;
extern char *home;		/* home directory */
extern char *recycleBin;	/* holds deleted files */
extern char *configFile, *sigFile;
extern char *sslCerts;		/* ssl certificates, to validate the secure server */
extern char *cookieFile;	/* persistent cookies */
extern char *spamCan;		/* dump spam emails */
extern char *edbrowseTempFile;

/* Ok, this is kinda simple and stupid.
 * Every line in every window in every session is a string, without the
 * newline, and all these strings are managed by an array of pointers.
 * So your particular buffer may have 3 lines in it,
 * and these are represented internally by three numbers, 12 29 and 83.
 * So the text is textLines[12], textLines[29], textLines[83].
 * Now, if you copy a line of text, I actually copy the string.
 * This is a waste I suppose, but then I don't have to worry about
 * what happens if you edit one line and not the other.
 * They're already separate strings, so we're ok. */
extern pst *textLines;
extern int textLinesMax, textLinesCount;	/* size of textLines array */

/* If a file has 30 lines, it is represented by 30 numbers,
 * indexes into textLines[] above.
 * Should we use an array of numbers, or a string of numbers
 * represented by decimal digits?
 * Both are painful, in different ways.
 * Consider inserting a block of text, a very common operation.
 * In a list, we would have to slide all the following numbers down.
 * Granted, that's better than moving all those lines of text,
 * but it's still a pain to program, and somewhat inefficient.
 * If we use strings, we take the original string of numbers,
 * break it at the insert point, and make a new string
 * by concatenating these two pieces with the new block.
 * The same issues arise when deleting text near the top of a file.
 * This and other considerations push me towards strings.
 * I currently use 6 characters for a line number,
 * a seventh for the g// flag.
 * 8th is reserved,
 * and 9 and 10 hold the directory suffix.
 * You know, the slash that we put on the end of a directory,
 * or the | on the end of a fifo etc.
 * Thus the following definitions. */
#define LNMAX 1000000
#define LNWIDTH 10
#define LNSPACE "          "
#define LNFORMAT "%06d    "

struct listHead {
    void *next;
    void *prev;
};

/* Macros to loop through the items in a list. */
#define foreach(e,l) for((e)=(l).next; \
(e) != (void*)&(l); \
(e) = ((struct listHead *)e)->next)
#define foreachback(e,l) for((e)=(l).prev; \
(e) != (void*)&(l); \
(e) = ((struct listHead *)e)->prev)

/* an edbrowse window */
struct ebWindow {
/* windows stack up as you open new files or follow hyperlinks.
 * Use the back command to pop the stack.
 * The back command follows this link, which is 0 if you are at the top. */
    struct ebWindow *prev;
/* This is right out of the source for ed.  Current and last line numbers. */
    int dot, dol;
/* remember dot and dol for the raw text, when in browse mode */
    int r_dot, r_dol;
    char *fileName;		/* name of file or url */
    char *referrer;
    char *baseDirName;		/* when scanning a directory */
    char *ft, *fd, *fk;		/* title, description, keywords */
    char lhs[MAXRE], rhs[MAXRE];	/* remembered substitution strings */
    char *map;			/* indexes into textLines[] */
    char *r_map;
/* The labels that you set with the k command, and access via 'x.
 * Basically, that's 26 line numbers.
 * Number 0 means the label is not set. */
    int labels[26], r_labels[26];
/* Yeah, these could be bit fields in the structure :1; but who cares. */
    bool lhs_yes, rhs_yes;
    bool binMode;		/* binary file */
    bool nlMode;		/* newline at the end */
    bool rnlMode;
    bool browseMode;		/* browsing html */
    bool changeMode;		/* something has changed in this file */
    bool dirMode;		/* directory mode */
    bool firstOpMode;		/* first change has been made, undo is possible */
    bool jsdead;		/* javascript is dead, for this window */
    char *dw;			/* document.write string */
    int dw_l;			/* length of the above */
    void *tags;			/* array of html tags, when browsing */
    long *iplist;		/* ip addresses referenced by this page */
    void *jsc;			/* js context, if in browse mode, and running javascript */
};
extern struct ebWindow *cw;	/* current window */

/* An edit session */
struct ebSession {
    struct ebWindow *fw, *lw;	/* first window, last window */
};
extern struct ebSession sessionList[];
extern struct ebSession *cs;	/* current session */


/* function prototypes */
#include "eb.p"

#endif
