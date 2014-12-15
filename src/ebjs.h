/*********************************************************************
ebjs.h: edbrowse javascript engine interface.
structure and values that are passed between the two processes.
The prefix EJ is short for edbrowse javascript.
This is the only header file shared between edbrowse and the js process.
It does not and should not include other edbrowse files.
It also does not reference bool true false, which are defined constants
in edbrowse, written C, but are reserved words in the js process,
written in C++.
Since there are no function declarations here, we don't need the extern "C" {}
*********************************************************************/

#ifndef EBJS_H
#define EBJS_H 1

#define EJ_MAGIC 0xac97

enum ej_cmd {
	EJ_CMD_CREATE,
	EJ_CMD_DESTROY,
	EJ_CMD_SCRIPT,
	EJ_CMD_GETPROP,
	EJ_CMD_SETPROP,
	EJ_CMD_DELPROP,
	EJ_CMD_GETAREL,
	EJ_CMD_SETAREL,
	EJ_CMD_INSTANCE,
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
	EJ_LOW_MEMORY,
	EJ_LOW_EXEC,
	EJ_LOW_RUNTIME,
	EJ_LOW_OTHER,
};

enum ej_proptype {
	EJ_PROP_UNDEFINED,
	EJ_PROP_STRING,
	EJ_PROP_BOOL,
	EJ_PROP_INT,
	EJ_PROP_FLOAT,
	EJ_PROP_OBJECT,
	EJ_PROP_ARRAY,
	EJ_PROP_FUNCTION,
	EJ_PROP_URL,
};

/* Opaque indicator of an object that can be shared
 * between the two processes. */
typedef void *jsobjtype;

struct EJ_MSG {
	int magic;		/* sanity check */
	jsobjtype jcx;		/* javascript context */
	jsobjtype winobj;	/* window object */
	jsobjtype obj;		/* an object somewhere in the window tree */
	enum ej_highstat highstat;
	enum ej_lowstat lowstat;
/* the property, as a string, follows this struct in the message */
	int proplength;
	enum ej_proptype proptype;
	int n;			/* an overloaded integer */
	int side;		/* length of side effects string */
};

#endif
