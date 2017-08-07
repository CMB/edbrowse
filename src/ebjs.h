/*********************************************************************
ebjs.h: edbrowse javascript engine interface.
Contains structures and values that are passed between the two processes.
The prefix EJ in symbolic constants is short for edbrowse javascript.
This is the only header file shared between edbrowse and the js process.
It does not and should not include other edbrowse files.
It also does not reference bool true false, which are defined constants
in edbrowse, written in C, but are reserved words in the js process,
written in C++.
Since there are no function declarations here, we don't need the extern "C" {}
*********************************************************************/

#ifndef EBJS_H
#define EBJS_H 1

#define EJ_MAGIC 0xac97

enum ej_cmd {
	EJ_CMD_NONE,
	EJ_CMD_CREATE,
	EJ_CMD_DESTROY,
	EJ_CMD_EXIT,
	EJ_CMD_SCRIPT,
	EJ_CMD_GETPROP,
	EJ_CMD_SETPROP,
	EJ_CMD_DELPROP,
	EJ_CMD_HASPROP,
	EJ_CMD_GETAREL,
	EJ_CMD_SETAREL,
	EJ_CMD_ARLEN,
	EJ_CMD_CALL,
	EJ_CMD_VARUPDATE,
};

enum ej_highstat {
	EJ_HIGH_OK,
	EJ_HIGH_STMT_FAIL,
	EJ_HIGH_CX_FAIL,
	EJ_HIGH_HEAP_FAIL,
	EJ_HIGH_PROC_FAIL,
};

enum ej_lowstat {
	EJ_LOW_OK,
	EJ_LOW_SYNTAX,
	EJ_LOW_CLOSE,
	EJ_LOW_CX,
	EJ_LOW_WIN,
	EJ_LOW_DOC,
	EJ_LOW_STD,
	EJ_LOW_VARS,
	EJ_LOW_MEMORY,
	EJ_LOW_EXEC,
	EJ_LOW_RUNTIME,
	EJ_LOW_SYNC,
};

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
};

enum ej_varupdates {
	EJ_VARUPDATE_NONE,
	EJ_VARUPDATE_XHR,
	EJ_VARUPDATE_DEBUG,
	EJ_VARUPDATE_VERIFYCERT,
	EJ_VARUPDATE_USERAGENT,
	EJ_VARUPDATE_CURLAUTHNEG,
	EJ_VARUPDATE_FILENAME,
	EJ_VARUPDATE_DEBUGFILE,
	EJ_VARUPDATE_COUNT, /* special value to iterate over varupdates */
};

/* Opaque indicator of an object that can be shared
 * between the two processes. */
typedef void *jsobjtype;

extern jsobjtype jcx; // the javascript context
extern jsobjtype winobj;	// window object
extern jsobjtype docobj;	// document object
extern const char *jsSourceFile; // sourcefile providing the javascript
extern int jsLineno; // line number

struct EJ_MSG {
	int magic;		/* sanity check */
	enum ej_cmd cmd;
	jsobjtype jcx;		/* javascript context */
	jsobjtype winobj;	/* window object */
	jsobjtype docobj;	/* document object */
	jsobjtype obj;		/* an object somewhere in the window tree */
	enum ej_highstat highstat;
	enum ej_lowstat lowstat;
/* the property, as a string, follows this struct in the message */
	int proplength;
	enum ej_proptype proptype;
	int n;			/* an overloaded integer */
	int side;		/* length of side effects string */
	int msglen;		/* error message from JS */
	int lineno;		/* line number */
};

#endif
