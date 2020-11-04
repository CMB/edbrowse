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
#include <sys/stat.h>
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
#include <direct.h> // for _mkdir, ...
#include <conio.h>  // for _kbhit, getch, getche
#include <stdint.h> // for UINT32_MAX
#include "vsprtf.h" // for WIN32 asprintf, vasprintf, ...
#else
#include <unistd.h>
#endif
#include <pthread.h>

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
#if defined(DOSLIKE) || defined(__ANDROID__)
typedef unsigned short ushort;
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

// Opaque indicator of an object that can be shared
// between edbrowse and the js engine.
typedef void *jsobjtype;

extern const char *jsSourceFile; // sourcefile providing the javascript
extern int jsLineno; // line number

/*********************************************************************
duktape is not threadsafe, so says the programmer's guide.
So javascript runs in the foreground as we push buttons etc,
or, there is at most one thread running javascript in the background,
if we choose to implement that.
Here is the js background thread. It is 0 if no thread is running.
*********************************************************************/

extern pthread_t jsbt;

enum ej_proptype {
	EJ_PROP_NONE,
	EJ_PROP_STRING,
	EJ_PROP_BOOL,
	EJ_PROP_INT,
	EJ_PROP_FLOAT,
	EJ_PROP_OBJECT,
	EJ_PROP_ARRAY,
	EJ_PROP_FUNCTION,
	EJ_PROP_INSTANCE,
	EJ_PROP_NULL,
};

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
#define ABSPATH 1024 // max length of an absolute pathname
#define MAXRE 512 // max length of a regular expression
#define MAXTTYLINE 256 // max length of an entered line
#define MAXHOSTLEN 400
#define MAXPROTLEN 12
#define MAXUSERPASS 80 // user name or password
#define MAXACCOUNT 100 // number of email accounts
#define MAXAGENT 50 // number of user agents
#define MAXMIME 40 // number of mime types
#define MAXPROXY 200 // number of proxy entries
#define MAXDBT 100 // number of configured database tables
#define MAXTCOLS 40 // columns in a configured table
#define MAXSESSION 1000 // concurrent edbrowse sessions
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
extern char *userAgents[], *currentAgent;
extern char *newlocation;
extern int newloc_d; /* delay */
extern bool newloc_r; /* location replaces this page */
extern struct ebFrame *newloc_f; /* frame calling for new web page */
extern const char *ebrc_string; /* default ebrc file */

// Get data from the internet. Zero the structure, set the
// members you need, then call httpConnect.
struct i_get {
// the data returned from the internet fetch
	char *buffer;
	int length;
// in case you want the headers
	char **headers_p;
	const char *url;
	char *urlcopy;
	int urlcopy_l;
	const char *custom_h; // custom http headers passed to curl
	char *cfn; // changed filename
	const char *thisfile;
	char *referrer;
	CURL *h;
	int tsn; // thread sequence number
// State of download to disk, see http.c for state values.
	int down_state;
	int down_fd;	/* downloading file descriptor */
	int down_msg;
	const char *down_file;	/* downloading filename */
	const char *down_file2;	/* without download directory */
	int down_length;
	bool down_ok;
	uchar down_force;
	bool uriEncoded;
	bool foreground;
	bool pg_ok; // watch for plugins
	bool playonly; // only player plugins
	bool csp; // content supresses plugins
	bool is_http;
	bool cacheable;
	bool last_curlin;
	bool move_capable;
	char error[CURL_ERROR_SIZE + 1];
	long code;		/* example, 404 */
/* an assortment of variables that are gleaned from the incoming http headers */
	char *headers;
	int headers_len;
/* http content type is used in many places, and isn't arbitrarily long
 * or case sensitive, so keep our own sanitized copy. */
	char content[60];
	char *charset;	/* extra content info such as charset */
long long hcl;			/* http content length */
	char *cdfn;		/* http content disposition file name */
	time_t modtime;	/* http modification time */
	char *etag;		/* the etag in the header */
	char auth_realm[60];	/* WWW-Authenticate realm header */
	char *newloc;
	int newloc_d;
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

struct MIMETYPE {
	char *type, *desc;
	char *suffix, *prot, *program;
	char *urlmatch;
	char *content;
	char outtype;
	bool down_url, from_file;
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
extern CURLSH *global_share_handle;
extern int debugLevel;		/* 0 to 9 */
extern bool debugClone, debugEvent, debugThrow, debugCSS;
extern bool demin; // deminimize javascript
extern bool uvw; // trace points
extern bool gotimers; // run javascript timers
extern int rr_interval; // rerender the screen after this many seconds
extern FILE *debugFile;
extern char *debugFileName;
extern char *sslCerts;		/* ssl certificates to validate the secure server */
extern int verifyCertificates;	/* is a certificate required for the ssl connection? */
extern int displayLength;	// when printing a line
extern int formatLineLength;	// when formatting html
extern bool formatOverflow;
extern int webTimeout, mailTimeout;
extern uchar browseLocal;
extern bool sqlPresent;		/* Was edbrowse compiled with SQL built in? */
extern bool curlActive; // is curl running?
extern bool ismc;		/* Is the program running as a mail client? */
extern bool isimap;		/* Is the program running as an imap client? */
extern bool down_bg;		// download in background
extern bool down_jsbg;		// download javascript in background
extern char whichproc; // which edbrowse-xx process
extern char showProgress; // feedback as a file is downloaded
extern char eb_language[];		/* edbrowse language, determined by $LANG */
extern int eb_lang; // encoded version of the above, for languages that we recognize
extern bool cons_utf8;		/* does the console expect utf8? */
extern bool iuConvert;		/* perform iso utf8 conversions automatically */
extern char type8859;		/* 1 through 15 */
extern bool js_redirects;	/* window.location = new_url */
extern bool passMail;		/* pass mail across the filters */
extern bool errorExit;		/* exit on any error, for scripting purposes */
extern bool isInteractive;
extern volatile bool intFlag;	/* set this when interrupt signal is caught */
extern time_t intStart;
extern bool binaryDetect;
extern bool inputReadLine;
extern bool curlAuthNegotiate;  /* try curl negotiate (SPNEGO) auth */
extern bool listNA;		/* list nonascii chars */
extern bool inInput;		/* reading line from standard in */
extern int fileSize;		/* when reading/writing files */
extern char errorMsg[];		/* generated error message */
extern int localAccount;	/* this is the smtp server for outgoing mail */
extern char *mailDir;		/* move to this directory when fetching mail */
extern char *mailUnread;	/* place to hold fetched but unread mail */
extern char *mailReply;		/* file to hold reply info for each email */
/* Keep a copy of unformatted mail that you probably won't need again,
 * but you never know. Should probably live somewhere under .Trash */
extern char *mailStash;
extern char *downDir;		/* the download directory */
extern char *ebTempDir;		/* edbrowse temp, such as /tmp/.edbrowse */
extern char *ebUserDir;		/* $ebTempDir/nnn user ID appended */
extern char *dbarea, *dblogin, *dbpw;	/* to log into the database */
extern bool fetchBlobColumns;
extern bool caseInsensitive, searchStringsAll, searchWrap;
extern bool allowRedirection;	/* from http code 301, or http refresh */
extern bool sendReferrer;	/* in the http header */
extern bool allowJS;		/* javascript on */
extern bool blockJS; // javascript is blocked
extern bool htmlGenerated;
extern bool ftpActive;
extern bool helpMessagesOn;	/* no need to type h */
extern bool pluginsOn;		/* plugins are active */
extern bool showHiddenFiles;	/* during directory scan */
extern bool showHover; // messages that appear when you hover
extern bool doColors;
extern int context;		/* which session (buffer) are we in? */
extern pst linePending;
extern char *changeFileName;
extern char *addressFile;	/* your address book */
extern char *serverData;
extern int serverDataLen;
extern char *breakLineResult;
extern char *home;		/* home directory */
extern char *recycleBin;	/* holds deleted files */
extern char *configFile, *sigFile, *sigFileEnd;
extern char *cookieFile;	/* persistent cookies */
extern char *cacheDir;	/* directory for a persistent cache of http pages */
extern int cacheSize; // in megabytes
extern int cacheCount; // number of cache files

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
	bool gflag;		/* for g// */
	char filler;
};
#define LMSIZE sizeof(struct lineMap)

/* an edbrowse frame, as when there are many frames in an html page.
 * There could be several frames in an edbrowse window or buffer, chained
 * together in a linked list, but usually there is just one, as when editing
 * a local file and browsing a simple web page.
*/
struct ebFrame {
	struct ebFrame *next;
	struct ebWindow *owner;
	struct htmlTag *frametag;
	int gsn; // global sequence number
	char *fileName;		/* name of file or url */
	char *firstURL;		// before http redirection
	char *hbase; /* base for href references */
	bool render1; // rendered via protocol or urlmatch
	bool render2; // rendered via suffix
	bool render1b;
	bool baseset; // <base> tag has been seen
	bool uriEncoded; // filename is url encoded
	char *dw;		/* document.write string */
	int dw_l;		/* length of the above */
// document.writes go under the body.
	struct htmlTag *htmltag, *headtag, *bodytag;
/* The javascript context and window corresponding to this url or frame.
 * If this is null then javascript is not operational for this frame.
 * We could still be browsing however, without javascript. */
	jsobjtype cx;
	jsobjtype winobj;
	jsobjtype docobj;	/* window.document */
	const struct MIMETYPE *mt;
	void *cssmaster;
	short jtmin;
};

typedef struct ebFrame Frame;
extern Frame *cf;	/* current frame */
extern int gfsn; // global frame sequence number

/* single linked list for internal jump history */
struct histLabel {
	int label; /* label must be first element */
	struct histLabel *prev;
};

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
	struct ebFrame f0; /* first frame */
	struct ebFrame *jdb_frame; // if in jdb mode
	char *referrer; // another web page that brought this one to life
	char *baseDirName;	/* when scanning a directory */
	char *htmltitle, *htmldesc, *htmlkey;	/* title, description, keywords */
	char *saveURL;		// for the fu command
	char *mailInfo;
	char lhs[MAXRE], rhs[MAXRE];	/* remembered substitution strings */
	struct lineMap *map, *r_map;
/* The labels that you set with the k command, and access via 'x.
 * Basically, that's 26 line numbers.
 * Number 0 means the label is not set.
 * But there's one more to mark, when background javascript
 * adds or deletes lines and we need to keep track of dot. */
#define MARKLETTERS 27
#define MARKDOT 26
	int labels[MARKLETTERS], r_labels[MARKLETTERS];
	struct histLabel *histLabel;
/* Next is an array of html tags, generated by the browse command,
 * and used thereafter for hyperlinks, fill-out forms, etc. */
	struct htmlTag **tags;
	int numTags, allocTags, deadTags;
	struct htmlTag *scriptlist, *inputlist, *optlist, *linklist;
	bool mustrender:1;
	bool sank:1; /* jSyncup has been run */
	bool lhs_yes:1;
	bool rhs_yes:1;
	bool binMode:1;		/* binary file */
	bool nlMode:1;		/* newline at the end */
	bool rnlMode:1;
/* Various text modes, these are incompatible with binMode */
/* All modes convert to utf8, as that is what pcre understands. */
	bool utf8Mode:1;
	bool utf16Mode:1;
	bool utf32Mode:1;
	bool bigMode:1; // big-endian
	bool iso8859Mode:1;
	bool dosMode:1; // \r\n
	bool browseMode:1;	/* browsing html */
	bool changeMode:1;	/* something has changed in this file */
	bool quitMode:1;	/* you can quit this buffer any time */
	bool dirMode:1;		/* directory mode */
	bool undoable:1;	/* undo is possible */
	bool sqlMode:1;		/* accessing a table */
	struct DBTABLE *table;	/* if in sqlMode */
	time_t nextrender;
};
extern struct ebWindow *cw;	/* current window */
#define foregroundWindow (cw == sessionList[context].lw)

/* quickly grab a tag from the current window via its sequence number:
 * tagList[n] */
#define tagList (cw->tags)

/* js is running in the current session */
#define isJSAlive (cf->cx && allowJS)

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
extern int maxSession;

/* The information on an html tag */
#define MAXTAGNAME 20
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
	struct htmlTag *same; // same action
	struct ebFrame *f0; /* frame that owns this tag */
	struct ebFrame *f1; /* subordinate frame if this is a <frame> tag */
	jsobjtype jv;		/* corresponding javascript variable */
	jsobjtype style; // style object
	int seqno;
	int gsn; // global sequence number, for rooting
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
	pthread_t loadthread;
	long hcode;
	bool loadsuccess;
	uchar step; // prerender, decorate, load script, runscript
	bool slash:1;		/* as in </A> */
	bool textin:1; /* <a> some text </a> */
	bool deleted:1; /* deleted from the current buffer */
	bool dead:1; // removed by garbage collection
	bool contracted:1; /* frame is contracted */
	bool multiple:1;
	bool async:1; // asynchronous script
	bool intimer:1; // asynchronous script in timer
	bool inxhr:1; // script is really an xhr
	bool rdonly:1;
	bool disabled:1;
	bool clickable:1;	/* but not an input field */
	bool secure:1;
	bool scriptgen:1; // script generated, not from source
	bool checked:1;
	bool rchecked:1;	/* for reset */
	bool post:1;		/* post, rather than get */
	bool javapost:1;	// post by calling javascript
	bool jslink:1;	// linked to a js object
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
	bool visited:1;
	bool masked:1;
	bool iscolor:1;
	char subsup;		/* span turned into sup or sub */
	uchar itype;		// input type =
	uchar itype_minor;
#define DIS_INVISIBLE 1
#define DIS_HOVER 2
#define DIS_COLOR 3
#define DIS_TRANSPARENT 4
#define DIS_HOVERCOLOR 5
	uchar disval; // displayable value for the node
	int ninp;		/* number of nonhidden inputs */
// class is reserved word in c++, so use jclass for javascript class
	char *name, *id, *jclass, *nodeName, *value, *href;
	const char *rvalue; /* for reset */
	char *innerHTML; /* the html string under this tag */
	int inner;		/* for inner html */
	int highspec; // specificity of a selector that matches this node
};

typedef struct htmlTag Tag;

/* htmlTag.action */
enum {
	TAGACT_HTML, TAGACT_A, TAGACT_INPUT, TAGACT_TITLE, TAGACT_TA,
	TAGACT_BUTTON, TAGACT_SELECT, TAGACT_OPTION, TAGACT_LABEL,
	TAGACT_NOP, TAGACT_JS, TAGACT_H, TAGACT_SUB, TAGACT_SUP, TAGACT_OVB,
	TAGACT_OL, TAGACT_UL, TAGACT_DL, TAGACT_TEXT,
	TAGACT_BODY, TAGACT_HEAD, TAGACT_DOC, TAGACT_FRAG, TAGACT_COMMENT,
	TAGACT_MUSIC, TAGACT_IMAGE, TAGACT_BR, TAGACT_IBR, TAGACT_P,
	TAGACT_BASE, TAGACT_META, TAGACT_LINK, TAGACT_PRE,
	TAGACT_TBODY, TAGACT_THEAD, TAGACT_TFOOT,
	TAGACT_DT, TAGACT_DD, TAGACT_LI, TAGACT_TABLE, TAGACT_TR, TAGACT_TD,
	TAGACT_DIV, TAGACT_SPAN, TAGACT_HR, TAGACT_OBJECT, TAGACT_FOOTER,
	TAGACT_HEADER, // <header> tag, not the same as <head> tag
	TAGACT_FORM, TAGACT_FRAME, TAGACT_STYLE,
	TAGACT_MAP, TAGACT_AREA, TAGACT_SCRIPT, TAGACT_NOSCRIPT, TAGACT_EMBED,
	TAGACT_OBJ, TAGACT_UNKNOWN,
};

/* htmlTag.itype */
/* Warning - the order of these is important! */
/* Corresponds to inp_types in decorate.c */
enum {
	INP_RESET, INP_BUTTON, INP_IMAGE, INP_SUBMIT,
	INP_HIDDEN, INP_TEXT, INP_FILE,
	INP_SELECT, INP_TA, INP_RADIO, INP_CHECKBOX,
};
extern const char *const inp_types[];

/* htmlTag.itype_minor */
/* The order corresponds to inp_others in decorate.c */
enum {
	INP_NO_MINOR, INP_DATE, INP_DATETIME, INP_DATETIME_LOCAL,
	INP_MONTH, INP_WEEK, INP_TIME, INP_EMAIL, INP_RANGE,
	INP_SEARCH, INP_TEL, INP_URL, INP_NUMBER, INP_PW,
};
extern const char *const inp_others[];

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
// give the above we probably don't need rwx mode but here we go
#define MODE_rw 0600
#define MODE_rwx 0700
#define MODE_private 0600
#else
#define MODE_rw 0666
#define MODE_rwx 0777
#define MODE_private 0600
#endif

/* function prototypes */
#include "ebprot.h"

/* Symbolic constants for language independent messages */
#include "messages.h"

#endif
