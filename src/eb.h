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
#include <curl/curl.h>
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

/* We use uchar for boolean fields.
 * The type is eb_bool, an edbrowse bool, so as not to conflict with C++.
 * When you convert edbrowse back to C, please put these symbols back to
 * the more readable bool, true, false. */
typedef uchar eb_bool;
#define eb_false 0
#define eb_true 1

typedef unsigned int IP32bit;
#define NULL_IP (IP32bit)(-1)

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

#define stringEqual !strcmp

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

/* How long can an absolute path be? */
#define ABSPATH 256
/* How long can a regular expression be? */
#define MAXRE 400
/* How long can an entered line be? */
#define MAXTTYLINE 256
/* Longest line that can take the breakline command */
#define REPLACELINELEN 50000
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

struct eb_curl_callback_data {
	char **buffer;
	int *length;
};

struct MACCOUNT {		/* pop3 account */
	char *login, *password, *from, *reply;
	char *inurl, *outurl;
	int inport, outport;
	uchar inssl, outssl;
	char nofetch;
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
	eb_bool stream;
};
extern struct MIMETYPE mimetypes[];
extern int maxMime;		/* how many mime types specified */

struct DBTABLE {
	char *name, *shortname;
	char *cols[MAXTCOLS];
	char ncols;
	unsigned char key1, key2, key3, key4;
	char *types;
	char *nullable;
};

/* various globals */
extern CURL *http_curl_handle;
extern int debugLevel;		/* 0 to 9 */
extern char *sslCerts;		/* ssl certificates to validate the secure server */
extern int verifyCertificates;	/* is a certificate required for the ssl connection? */
extern int displayLength;	/* when printing a line */
extern int jsPool;		/* size of js pool in megabytes */
extern int webTimeout, mailTimeout;
extern int browseLine;		/* line number, for error reporting */
extern eb_bool sqlPresent;	/* Was edbrowse compiled with SQL built in? */
extern eb_bool ismc;		/* Is the program running as a mail client? */
extern int eb_lang;		/* edbrowse language, determined by $LANG */
extern eb_bool cons_utf8;	/* does the console expect utf8? */
extern eb_bool iuConvert;	/* perform iso utf8 conversions automatically */
extern char type8859;		/* 1 through 15 */
extern eb_bool js_redirects;	/* window.location = new_url */
extern uchar browseLocal;	/* browsing a local file */
extern eb_bool parsePage;	/* parsing the html page and any js therein */
extern eb_bool htmlAttrVal_nl;	/* allow nl in the attribute of an html tag */
extern eb_bool passMail;	/* pass mail across the filters */
extern eb_bool errorExit;	/* exit on any error, for scripting purposes */
extern eb_bool isInteractive;
extern volatile eb_bool intFlag;	/* set this when interrupt signal is caught */
extern eb_bool binaryDetect;
extern eb_bool inputReadLine;
extern eb_bool listNA;		/* list nonascii chars */
extern eb_bool inInput;		/* reading line from standard in */
extern int fileSize;		/* when reading/writing files */
extern int mssock;		/* mail server socket */
extern long hcode;		/* http code, like 404 file not found */
extern char errorMsg[];		/* generated error message */
extern char serverLine[];	/* lines to and from the mail server */
extern int localAccount;	/* this is the smtp server for outgoing mail */
extern char *mailDir;		/* move to this directory when fetching mail */
extern char *mailUnread;	/* place to hold fetched but unread mail */
/* Keep a copy of unformatted mail that you probably won't need again,
 * but you never know. Should probably live somewhere under .Trash */
extern char *mailStash;
extern char *dbarea, *dblogin, *dbpw;	/* to log into the database */
extern eb_bool fetchBlobColumns;
extern eb_bool caseInsensitive, searchStringsAll;
extern eb_bool allowRedirection;	/* from http code 301, or http refresh */
extern eb_bool sendReferrer;	/* in the http header */
extern eb_bool allowJS;		/* javascript on */
extern eb_bool helpMessagesOn;	/* no need to type h */
extern eb_bool showHiddenFiles;	/* during directory scan */
extern uchar dirWrite;		/* directory write mode, e.g. rename files */
extern uchar endMarks;		/* do we print ^ $ at the start and end of lines? */
extern int context;		/* which session (buffer) are we in? */
extern uchar linePending[];
extern char *changeFileName;
extern char *addressFile;	/* your address book */
extern char *serverData;
extern int serverDataLen;
extern char replaceLine[];
extern char *currentReferrer;
extern char *home;		/* home directory */
extern char *recycleBin;	/* holds deleted files */
extern char *configFile, *sigFile, *sigFileEnd;
extern char *cookieFile;	/* persistent cookies */
extern char *edbrowseTempFile;
extern char *edbrowseTempPDF;
extern char *edbrowseTempHTML;

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

/* A pointer to the text of a line, and other line attributes */
struct lineMap {
	pst text;
	char ds1, ds2;		/* directory suffix */
	eb_bool gflag;		/* for g// */
	char filler;		/* C is going to pad the structure anyways */
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
	char *fto;		/* original title, before andTranslate */
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
	eb_bool lhs_yes:1;
	eb_bool rhs_yes:1;
	eb_bool binMode:1;	/* binary file */
	eb_bool nlMode:1;	/* newline at the end */
	eb_bool rnlMode:1;
/* Two text modes:1; these are incompatible with binMode */
	eb_bool utf8Mode:1;
	eb_bool iso8859Mode:1;
	eb_bool browseMode:1;	/* browsing html */
	eb_bool changeMode:1;	/* something has changed in this file */
	eb_bool dirMode:1;	/* directory mode */
	eb_bool undoable:1;	/* undo is possible */
	eb_bool sqlMode:1;	/* accessing a table */
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
struct tagInfo {
	const char *name;
	const char *desc;
	int action;
	uchar nest;		/* must nest, like parentheses */
	uchar para;		/* paragraph and line breaks */
	ushort bits;		/* a bunch of boolean attributes */
};

/* The structure for an html tag.
 * These tags are at times linked with js objects,
 * or even created by js objects. */
struct htmlTag {
	jsobjtype jv;		/* corresponding java variable */
	int seqno;
	int ln;			/* line number */
	int lic;		/* list item count, highly overloaded */
	int action;
	const struct tagInfo *info;
/* the form that owns this input tag */
	struct htmlTag *controller;
	eb_bool slash:1;	/* as in </A> */
	eb_bool balanced:1;	/* <foo> and </foo> */
	eb_bool retain:1;
	eb_bool multiple:1;
	eb_bool rdonly:1;
	eb_bool clickable:1;	/* but not an input field */
	eb_bool secure:1;
	eb_bool checked:1;
	eb_bool rchecked:1;	/* for reset */
	eb_bool post:1;		/* post, rather than get */
	eb_bool javapost:1;	/* post by calling javascript */
	eb_bool mime:1;		/* encode as mime, rather than url encode */
	eb_bool bymail:1;	/* send by mail, rather than http */
	eb_bool submitted:1;
	eb_bool onclick:1;
	eb_bool onsubmit:1;
	eb_bool onreset:1;
	eb_bool onchange:1;
	char subsup;		/* span turned into sup or sub */
	uchar itype;		/* input type = */
	short ninp;		/* number of nonhidden inputs */
	char *attrib;
	char *name, *id, *value, *href;
/* class=foo becomes className = "foo" when you carry from html to javascript,
 * don't ask me why. */
	char *classname;
	const char *inner;	/* for inner html */
};

/* htmlTag.action */
enum {
	TAGACT_ZERO, TAGACT_A, TAGACT_INPUT, TAGACT_TITLE, TAGACT_TA,
	TAGACT_BUTTON, TAGACT_SELECT, TAGACT_OPTION,
	TAGACT_NOP, TAGACT_JS, TAGACT_H, TAGACT_SUB, TAGACT_SUP, TAGACT_OVB,
	TAGACT_DW, TAGACT_BODY, TAGACT_HEAD,
	TAGACT_MUSIC, TAGACT_IMAGE, TAGACT_BR, TAGACT_IBR, TAGACT_P,
	TAGACT_BASE, TAGACT_META, TAGACT_PRE,
	TAGACT_DT, TAGACT_LI, TAGACT_HR, TAGACT_TABLE, TAGACT_TR, TAGACT_TD,
	TAGACT_DIV, TAGACT_SPAN, TAGACT_HTML,
	TAGACT_FORM, TAGACT_FRAME,
	TAGACT_MAP, TAGACT_AREA, TAGACT_SCRIPT, TAGACT_EMBED, TAGACT_OBJ,
};

/* htmlTag.itype */
enum {
	INP_RESET, INP_BUTTON, INP_IMAGE, INP_SUBMIT,
	INP_HIDDEN,
	INP_TEXT, INP_PW, INP_NUMBER, INP_FILE,
	INP_SELECT, INP_TA, INP_RADIO, INP_CHECKBOX,
};

/* Last tag in the list parsed from html */
extern struct htmlTag *topTag;

/* function prototypes */
#include "eb.p"

/* Symbolic constants for language independent messages */
#include "messages.h"

#endif
