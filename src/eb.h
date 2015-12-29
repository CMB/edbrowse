/* eb.h
 * Copyright (c) Karl Dahlke, 2008
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

/* Define _GNU_SOURCE on Linux, so we don't have an implicit declaration
 * of asprintf, but only if we are not compiling C++.
 * Turns out that when compiling C++ on LInux, _GNU_SOURCE is helpfully
 * predefined, so defining it twice generates a nasty warning.
 */

#if defined(EDBROWSE_ON_LINUX) && !defined(__cplusplus)
#define _GNU_SOURCE
#endif

/* seems like everybody needs these header files */
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <memory.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <curl/curl.h>
#ifdef DOSLIKE
#include <io.h>
#include <direct.h> // for _mkdir, ...
#include <conio.h>  // for _kbhit, getch, getche
#include <stdint.h> // for UINT32_MAX
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

/* We use unsigned char for boolean fields. */
#ifndef __cplusplus
typedef uchar bool;
#define false 0
#define true 1
#endif

/* Some source files are shared between edbrowse, a C program,
 * and edbrowse-js, currently a C++ function program, thus the prototypes,
 * and some other structures, must be C protected. */
#ifdef __cplusplus
// Because of this line, you can't meaningfully run this file through indent.
extern "C" {
#endif

/*********************************************************************
Include the header file that connects edbrowse to the js process.
This is a series of enums, and the interprocess message structure,
and most importantly, the type jsobjtype,
which is an opaque number that corresponds to an object in the javascript world.
Edbrowse uses this number to connect an html tag, such as <input>,
with its corresponding js object.
These object numbers are passed back and forth to connect edbrowse and js.
*********************************************************************/

#include "ebjs.h"

/* ctype macros, when you're passing a byte,
 * and you don't want to worry about whether it's char or uchar.
 * Call the regular routines when c is an int, from fgetc etc. */
#define isspaceByte(c) isspace((uchar)c)
#define isalphaByte(c) isalpha((uchar)c)
#define isalnumByte(c) isalnum((uchar)c)
#define islowerByte(c) islower((uchar)c)
#define isupperByte(c) isupper((uchar)c)
#define isdigitByte(c) isdigit((uchar)c)
#define ispunctByte(c) ispunct((uchar)c)
#define isprintByte(c) isprint((uchar)c)

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
 * See the comments in buffers.c for the rationale. */
#define InternalCodeChar '\2'
#define InternalCodeCharAlternate '\1'
#define TableCellChar '\3'

/* How long can an absolute path be? */
#define ABSPATH 264 // max length of an absolute pathname, in windows or unix
/* How long can a regular expression be? */
#define MAXRE 400
/* How long can an entered line be? */
#define MAXTTYLINE 256
/* The longest string, in certain contexts. */
#define MAXSTRLEN 1024
/* How about user login and password? */
#define MAXUSERPASS 40
/* Number of pop3 mail accounts */
#define MAXACCOUNT 100
/* Number of mime types */
#define MAXMIME 40
/* Number of proxy entries */
#define MAXPROXY 200
/* number of db tables */
#define MAXDBT 100
#define MAXTCOLS 40
/* How many sessions open concurrently */
#define MAXSESSION 1000
/* Allocation increment for a growing string, that we don't expect
 * to get too large.  This must be a power of 2. */
#define ALLOC_GR        0x100
/* print a dot on download for each chunk of this size */
#define CHUNKSIZE 1000000

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

extern const char *version; // the version of edbrowse
extern const char *progname; // edbrowse, or the absolute path to edbrowse
extern const char eol[];	/* internet end of line */
extern char emptyString[];	/* use this whenever you would use "" */

/* Here are the strings you can send out to identify this browser.
 * Most of the time we will send out the first string, edbrowse-2.15.3.
 * But sometimes we have to lie.
 * When you deal with clickbank, for instance, they won't let you in the
 * door unless you are one of three approved browsers.
 * Tell them you're Explorer, and walk right in.
 * Anyways, this array holds up to 10 user agent strings. */
extern char *userAgents[10], *currentAgent;
extern char *newlocation;
extern int newloc_d; /* delay */
extern bool newloc_r; /* location replaces this page */
extern const char *ebrc_string; /* default ebrc file */

struct eb_curl_callback_data {
// where to put the captured data.
	char **buffer;
	int *length;
// State of download to disk, see http.c for state values.
	int down_state;
	int down_fd;	/* downloading file descriptor */
	const char *down_file;	/* downloading filename */
	const char *down_file2;	/* without download directory */
	int down_length;
};

struct MACCOUNT {		/* pop3 account */
	char *login, *password, *from, *reply;
	char *inurl, *outurl;
	int inport, outport;
	uchar inssl, outssl;
	bool nofetch, imap, secure;
};
extern struct MACCOUNT accounts[];	/* all the email accounts */
extern int maxAccount;		/* how many email accounts specified */

struct PXENT {			/* proxy entry */
	char *prot;		/* null means any */
	char *domain;		/* null means any */
	char *proxy;		/* null means direct */
};
extern struct PXENT proxyEntries[];
extern int maxproxy;

struct MIMETYPE {
	char *type, *desc;
	char *suffix, *prot, *program;
	char *content;
	char outtype;
	bool stream, download;
};
extern struct MIMETYPE mimetypes[];
extern int maxMime;		/* how many mime types specified */

struct DBTABLE {
	char *name, *shortname;
	char *cols[MAXTCOLS];
	int ncols;
	unsigned char key1, key2, key3, key4;
	char *types;
	char *nullable;
};

// This curl handle is always open, to retain the cookie space
// and to accept cookies generated by javascript.
extern CURL *global_http_handle;
extern int debugLevel;		/* 0 to 9 */
extern char *sslCerts;		/* ssl certificates to validate the secure server */
extern int verifyCertificates;	/* is a certificate required for the ssl connection? */
extern int displayLength;	/* when printing a line */
extern int jsPool;		/* size of js pool in megabytes */
extern int webTimeout, mailTimeout;
extern uchar browseLocal;
extern bool sqlPresent;		/* Was edbrowse compiled with SQL built in? */
extern bool ismc;		/* Is the program running as a mail client? */
extern bool isimap;		/* Is the program running as an imap client? */
extern bool down_bg;		/* download in background */
extern char whichproc; // which edbrowse-xx process
extern int eb_lang;		/* edbrowse language, determined by $LANG */
extern bool cons_utf8;		/* does the console expect utf8? */
extern bool iuConvert;		/* perform iso utf8 conversions automatically */
extern char type8859;		/* 1 through 15 */
extern bool js_redirects;	/* window.location = new_url */
extern bool passMail;		/* pass mail across the filters */
extern bool errorExit;		/* exit on any error, for scripting purposes */
extern bool isInteractive;
extern volatile bool intFlag;	/* set this when interrupt signal is caught */
extern bool binaryDetect;
extern bool inputReadLine;
extern bool listNA;		/* list nonascii chars */
extern bool inInput;		/* reading line from standard in */
extern int fileSize;		/* when reading/writing files */
extern int mssock;		/* mail server socket */
extern long hcode;		/* http code, like 404 file not found */
extern char errorMsg[];		/* generated error message */
extern char serverLine[];	/* lines to and from the mail server */
extern int localAccount;	/* this is the smtp server for outgoing mail */
extern char *mailDir;		/* move to this directory when fetching mail */
extern char *downDir;		/* the download directory */
extern char *mailUnread;	/* place to hold fetched but unread mail */
extern char *mailReply;		/* file to hold reply info for each email */
/* Keep a copy of unformatted mail that you probably won't need again,
 * but you never know. Should probably live somewhere under .Trash */
extern char *mailStash;
extern char *dbarea, *dblogin, *dbpw;	/* to log into the database */
extern bool fetchBlobColumns;
extern bool caseInsensitive, searchStringsAll;
extern bool allowRedirection;	/* from http code 301, or http refresh */
extern bool sendReferrer;	/* in the http header */
extern bool allowJS;		/* javascript on */
extern bool allowXHR;		/* xhr on */
extern bool htmlGenerated;
extern bool ftpActive;
extern bool helpMessagesOn;	/* no need to type h */
extern bool pluginsOn;		/* plugins are active */
extern bool showHiddenFiles;	/* during directory scan */
extern int context;		/* which session (buffer) are we in? */
extern pst linePending;
extern char *changeFileName;
extern char *addressFile;	/* your address book */
extern char *serverData;
extern int serverDataLen;
extern char *breakLineResult;
extern char *currentReferrer;
extern char *home;		/* home directory */
extern char *recycleBin;	/* holds deleted files */
extern char *configFile, *sigFile, *sigFileEnd;
extern char *cookieFile;	/* persistent cookies */

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

/*********************************************************************
a queue of input field values that have been changed by javascript.
these are applied to the edbrowse buffer after js has run.
Other changes can also accumulate in this queue, such as innerHTML.
The major number indicates the change to be made.
v = value in form, i is innerHTML or innerText (via minor),
x is unspecified.
*********************************************************************/

struct inputChange {
	struct inputChange *next, *prev;
	struct htmlTag *t;
	int tagno;
	char major, minor;
	char filler1, filler2;
	char value[4];
};
extern struct listHead inputChangesPending;

/* A pointer to the text of a line, and other line attributes */
struct lineMap {
	pst text;
	char ds1, ds2;		/* directory suffix */
	bool gflag;		/* for g// */
	char filler;
};
#define LMSIZE sizeof(struct lineMap)

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
	char *firstURL;		/* before http redirection */
	char *referrer;
	char *baseDirName;	/* when scanning a directory */
	char *ft, *fd, *fk;	/* title, description, keywords */
	char *hbase; /* base for href references */
	char *mailInfo;
	char lhs[MAXRE], rhs[MAXRE];	/* remembered substitution strings */
	struct lineMap *map, *r_map;
/* The labels that you set with the k command, and access via 'x.
 * Basically, that's 26 line numbers.
 * Number 0 means the label is not set. */
	int labels[26], r_labels[26];
/* Next is an array of html tags, generated by the browse command,
 * and used thereafter for hyperlinks, fill-out forms, etc. */
	struct htmlTag **tags;
	int numTags, allocTags;
	const struct MIMETYPE *mt;
	bool lhs_yes:1;
	bool rhs_yes:1;
	bool binMode:1;		/* binary file */
	bool nlMode:1;		/* newline at the end */
	bool rnlMode:1;
/* Two text modes:1; these are incompatible with binMode */
	bool utf8Mode:1;
	bool iso8859Mode:1;
	bool browseMode:1;	/* browsing html */
	bool changeMode:1;	/* something has changed in this file */
	bool quitMode:1;	/* you can quit this buffer any time */
	bool dirMode:1;		/* directory mode */
	bool undoable:1;	/* undo is possible */
	bool sqlMode:1;		/* accessing a table */
	bool baseset:1; /* <base> tag seen */
	char *dw;		/* document.write string */
	int dw_l;		/* length of the above */
/* The javascript context and window corresponding to this edbrowse buffer.
 * If this is null then javascript is not operational for this window.
 * We could still be browsing however, without javascript.
 * Refer to browseMode to test for that. */
	jsobjtype jcx;
	jsobjtype winobj;
	jsobjtype docobj;	/* window.document */
	struct DBTABLE *table;	/* if in sqlMode */
};
extern struct ebWindow *cw;	/* current window */
extern struct ebWindow in_js_cw; /* window within edbrowse-js */

/* quickly grab a tag from the current window via its sequence number:
 * tagList[n] */
#define tagList (cw->tags)

/* js is running in the current session */
#define isJSAlive (cw->jcx != NULL && allowJS)

/*********************************************************************
Temporary cap on the number of lines, so the integer index into cw->map
doesn't overflow. This is basically signed int over LMSIZE.
The former is 2^31 on most machines,
the latter is at most 12 on a 64-bit machine.
If ints are larger then I don't even use this constant.
*********************************************************************/

#define MAXLINES 170000000

/* An edit session */
struct ebSession {
	struct ebWindow *fw, *lw;	/* first window, last window */
};
extern struct ebSession sessionList[];
extern struct ebSession *cs;	/* current session */

/* The information on an html tag */
#define MAXTAGNAME 12
struct tagInfo {
	const char name[MAXTAGNAME];
	const char *desc;
	int action;
	uchar para;		/* paragraph and line breaks */
	ushort bits;		/* a bunch of boolean attributes */
};
extern const struct tagInfo availableTags[];

/* Information on tagInfo->bits */
/* support innerHTML */
#define TAG_INNERHTML 1
/* You won't see the text between <foo> and </fooo> */
#define TAG_INVISIBLE 2
/* sometimes </foo> means nothing. */
#define TAG_NOSLASH 4

/* The structure for an html tag.
 * These tags are at times linked with js objects,
 * or even created by js objects. */
struct htmlTag {
/* maintain a tree structure */
	struct htmlTag *parent, *firstchild, *sibling;
/* connect <foo> and </foo> */
	struct htmlTag *balance;
	jsobjtype jv;		/* corresponding java variable */
	int seqno;
	char *js_file;
	int js_ln;			/* line number of javascript */
	int lic;		/* list item count, highly overloaded */
	int slic; /* start list item count */
	int action;
	const struct tagInfo *info;
	char *textval;	/* for text tags only */
	const char **attributes;
	const char **atvals;
/* the form that owns this input tag */
	struct htmlTag *controller;
	uchar step; /* prerender, decorate, runscript */
	bool slash:1;		/* as in </A> */
	bool textin:1; /* <a> some text </a> */
	bool deleted:1; /* deleted from the current buffer */
	bool multiple:1;
	bool rdonly:1;
	bool clickable:1;	/* but not an input field */
	bool secure:1;
	bool scriptgen:1; // script generated, not from source
	bool checked:1;
	bool rchecked:1;	/* for reset */
	bool post:1;		/* post, rather than get */
	bool javapost:1;	/* post by calling javascript */
	bool mime:1;		/* encode as mime, rather than url encode */
	bool bymail:1;		/* send by mail, rather than http */
	bool submitted:1;
	bool onclick:1;
	bool onchange:1;
	bool onsubmit:1;
	bool onreset:1;
	bool onload:1;
	bool onunload:1;
	bool doorway:1; /* doorway to javascript */
	char subsup;		/* span turned into sup or sub */
	uchar itype;		/* input type = */
	int ninp;		/* number of nonhidden inputs */
	char *name, *id, *value, *href;
	const char *rvalue; /* for reset */
/* class=foo becomes className = "foo" when you carry from html to javascript,
 * don't ask me why. */
	char *classname;
	char *innerHTML; /* the html string under this tag */
	int inner;		/* for inner html */
};

/* htmlTag.action */
enum {
	TAGACT_ZERO, TAGACT_A, TAGACT_INPUT, TAGACT_TITLE, TAGACT_TA,
	TAGACT_BUTTON, TAGACT_SELECT, TAGACT_OPTION,
	TAGACT_NOP, TAGACT_JS, TAGACT_H, TAGACT_SUB, TAGACT_SUP, TAGACT_OVB,
	TAGACT_OL, TAGACT_UL, TAGACT_DL,
	TAGACT_TEXT, TAGACT_BODY, TAGACT_HEAD,
	TAGACT_MUSIC, TAGACT_IMAGE, TAGACT_BR, TAGACT_IBR, TAGACT_P,
	TAGACT_BASE, TAGACT_META, TAGACT_PRE, TAGACT_TBODY,
	TAGACT_DT, TAGACT_DD, TAGACT_LI, TAGACT_TABLE, TAGACT_TR, TAGACT_TD,
	TAGACT_DIV, TAGACT_SPAN, TAGACT_HR,
	TAGACT_FORM, TAGACT_FRAME,
	TAGACT_MAP, TAGACT_AREA, TAGACT_SCRIPT, TAGACT_EMBED, TAGACT_OBJ,
};

/* htmlTag.itype */
/* Warning - the order of these is important! */
/* Corresponds to inp_types in decorate.c */
enum {
	INP_RESET, INP_BUTTON, INP_IMAGE, INP_SUBMIT,
	INP_HIDDEN,
	INP_TEXT, INP_PW, INP_NUMBER, INP_FILE,
	INP_SELECT, INP_TA, INP_RADIO, INP_CHECKBOX,
};

/* For traversing a tree of html nodes, this is the callback function */
typedef void (*nodeFunction) (struct htmlTag * node, bool opentag);
extern nodeFunction traverse_callback;

/* Return codes for base64Decode() */
#define GOOD_BASE64_DECODE 0
#define BAD_BASE64_DECODE 1
#define EXTRA_CHARS_BASE64_DECODE 2

#ifdef DOSLIKE
/* windows mkdir takes only one argument */
#define mkdir(a,b) _mkdir(a)
#endif

/* function prototypes */
#include "ebprot.h"

/* Symbolic constants for language independent messages */
#include "messages.h"

#ifdef __cplusplus
}
#endif

#endif
