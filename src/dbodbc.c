/*********************************************************************
odbc.c: C-level interface to SQL.
This is a layer above ODBC,
since ODBC is often difficult to use, especially for new programmers.
Most SQL queries are relatively simple, whence the ODBC API is overkill.
Why mess with statement handles and parameter bindings when you can write:
sql_select("select this, that from table1, table2 where keycolumn = %d",
27, &this, &that);

More important, this API automatically aborts (or longjumps) if an error
occurs, unless that error has been specifically trapped by the program.
This minimizes application-level error-leg programming,
thereby reducing the code by as much as 1/3.
To accomplish this, the errorPrint() function,
supplied by the application, must never return.
We assume it passes the error message
to stderr and to a logfile,
and then exits, or longjumps to a recovery point.

Note that this API works within the context of our own C programming
environment.

Note that dbapi.h does NOT include the ODBC system header files.
That would violate the spirit of this layer,
which attempts to sheild the application from the details of ODBC.
If the application needed to see anything in those header files,
we would be doing something wrong.
*********************************************************************/

#ifdef _WIN32
#include <windows.h>		// don't know why this is needed
#endif

#include <sql.h>		/* ODBC header files */
#include <sqlext.h>

#include "eb.h"
#include "dbapi.h"


/*********************************************************************
The status variable rc holds the return code from an ODBC call.
This is then used by the function errorTrap() below.
If rc != 0, errorTrap() aborts the program, or performs
a recovery longjmp, as directed by the generic error function errorPrint().
errorTrap() returns true if an SQL error occurred, but that error
was trapped by the application.
In this case the calling routine should clean up as best it can and return.
*********************************************************************/

/* characteristics of the current ODBC driver */
static short odbc_version;
static long cursors_under_commit, cursors_under_rollback;
static long getdata_opts;

static SQLHENV henv = SQL_NULL_HENV;	/* environment handle */
static SQLHDBC hdbc = SQL_NULL_HDBC;	/* identify connect session */
static SQLHSTMT hstmt = SQL_NULL_HSTMT;	/* current statement */
static SQLHSTMT hstmt1 = SQL_NULL_HSTMT;	/* handle for single statements, such as sql_exec() */
static const char *stmt_text = 0;	/* text of the SQL statement */
static SQLRETURN rc;
static const short *exclist;	/* list of error codes trapped by the application */
static short translevel;
static bool badtrans;

/* Through globals, make error info available to the application. */
int rv_lastStatus, rv_stmtOffset;
long rv_vendorStatus;
char *rv_badToken;

static void
debugStatement(void)
{
    if(sql_debug && stmt_text)
	appendFileNF(sql_debuglog, stmt_text);
}				/* debugStatement */

static void
debugExtra(const char *s)
{
    if(sql_debug)
	appendFileNF(sql_debuglog, s);
}				/* debugExtra */

/* Append the SQL statement to the debug log.  This is not strictly necessary
 * if sql_debug is set, since the statement has already been appended. */
static void
showStatement(void)
{
    if(!sql_debug && stmt_text)
	appendFileNF(sql_debuglog, stmt_text);
}				/* showStatement */

/* application sets the exception list */
void
sql_exclist(const short *list)
{
    exclist = list;
}

void
sql_exception(int errnum)
{
    static short list[2];
    list[0] = errnum;
    exclist = list;
}				/* sql_exception */

/* map ODBC errors to our own exception codes, as defined in dbapi.h. */
static const struct ERRORMAP {
    const char odbc[6];
    short excno;
} errormap[] = {
    {
    "00000", 0}, {
    "S1001", EXCRESOURCE}, {
    "S1009", EXCARGUMENT}, {
    "S1012", EXCARGUMENT}, {
    "S1090", EXCARGUMENT}, {
    "01S02", EXCUNSUPPORTED}, {
    "S1096", EXCUNSUPPORTED}, {
    "28000", EXCARGUMENT}, {
    "S1010", EXCACTIVE}, {
    "08002", EXCACTIVE}, {
    "08001", EXCNOCONNECT}, {
    "08003", EXCNOCONNECT}, {
    "08007", EXCMANAGETRANS}, {
    "25000", EXCMANAGETRANS}, {
    "08004", EXCNOCONNECT}, {
    "IM003", EXCRESOURCE}, {
    "IM004", EXCRESOURCE}, {
    "IM005", EXCRESOURCE}, {
    "IM009", EXCRESOURCE}, {
    "IM006", EXCUNSUPPORTED}, {
    "S1092", EXCUNSUPPORTED}, {
    "S1C00", EXCUNSUPPORTED}, {
    "08S01", EXCREMOTE}, {
    "IM001", EXCUNSUPPORTED}, {
    "IM002", EXCNODB}, {
    "S1T00", EXCTIMEOUT}, {
    "24000", EXCNOCURSOR}, {
    "34000", EXCNOCURSOR}, {
    "S1011", EXCACTIVE}, {
    "IM013", EXCTRACE}, {
    "21S01", EXCAMBCOLUMN}, {
    "21S02", EXCAMBCOLUMN}, {
    "22003", EXCTRUNCATE}, {
    "22005", EXCCONVERT}, {
    "22008", EXCCONVERT}, {
    "22012", EXCCONVERT}, {
    "37000", EXCSYNTAX}, {
    "42000", EXCPERMISSION}, {
    "S0001", EXCDUPTABLE}, {
    "S0002", EXCNOTABLE}, {
    "S0011", EXCDUPINDEX}, {
    "S0012", EXCNOINDEX}, {
    "S0021", EXCDUPCOLUMN}, {
    "S0022", EXCNOCOLUMN}, {
    "S1008", EXCINTERRUPT}, {
    "01004", EXCTRUNCATE}, {
    "01006", EXCPERMISSION}, {
    "01S03", EXCNOROW}, {
    "01S04", EXCMANYROW}, {
    "07001", EXCARGUMENT}, {
    "07S01", EXCARGUMENT}, {
    "07006", EXCCONVERT}, {
    "22002", EXCARGUMENT}, {
    "S1002", EXCARGUMENT}, {
    "S1003", EXCARGUMENT}, {
    "23000", EXCCHECK}, {
    "40001", EXCDEADLOCK}, {
    "S1105", EXCARGUMENT}, {
    "S1106", EXCUNSUPPORTED}, {
    "S1109", EXCNOROW}, {
    "01S06", EXCNOROW}, {
    "", 0}
};				/* ends of list */

static int
errTranslate(const char *code)
{
    const struct ERRORMAP *e;

    for(e = errormap; e->odbc[0]; ++e) {
	if(stringEqual(e->odbc, code))
	    return e->excno;
    }
    return EXCSQLMISC;
}				/* errTranslate */

static bool
errorTrap(char *cxerr)
{
    short i, waste;
    char errcodes[6];
    char msgtext[200];
    bool firstError;

    /* innocent until proven guilty */
    rv_lastStatus = 0;
    rv_vendorStatus = 0;
    rv_stmtOffset = 0;
    rv_badToken = 0;
    if(!rc)
	return false;		/* no problem */

    /* log the SQL statement that elicitted the error */
    showStatement();

    if(rc == SQL_INVALID_HANDLE)
	errorPrint
	   ("@ODBC fails to recognize one of the handles (env, connect, stmt)");

    /* get error info from ODBC */
    firstError = true;
    while(true) {
	rc = SQLError(henv, hdbc, hstmt,
	   errcodes, &rv_vendorStatus, msgtext, sizeof (msgtext), &waste);
	if(rc == SQL_NO_DATA_FOUND) {
	    if(firstError)
		errorPrint
		   ("@ODBC command failed, but SQLError() provided no additional information");
	    return false;
	}

	/* Skip past the ERROR-IN-ROW errors. */
	if(stringEqual(errcodes, "01S01"))
	    continue;

	firstError = false;
	if(cxerr && stringEqual(cxerr, errcodes))
	    continue;
	break;
    }

    rv_lastStatus = errTranslate(errcodes);

    /* Don't know how to get statement ofset or invalid token from ODBC.
       /* I can get them from Informix; see dbinfx.ec */

    /* if the application didn't trap for this exception, blow up! */
    if(exclist)
	for(i = 0; exclist[i]; ++i)
	    if(exclist[i] == rv_lastStatus) {
		exclist = 0;	/* we've spent that exception */
		return true;
	    }

    /* Remember, errorPrint() should not return. */
    errorPrint("2ODBC error %s, %s, driver %s.",
       errcodes, sqlErrorList[rv_lastStatus], msgtext);
    return true;		/* make the compiler happy */
}				/* errorTrap */

static void
cleanStatement(void)
{
    SQLFreeStmt(hstmt, SQL_CLOSE);
    SQLFreeStmt(hstmt, SQL_UNBIND);
    SQLFreeStmt(hstmt, SQL_RESET_PARAMS);
}				/* cleanStatement */


/*********************************************************************
The OCURS structure given below maintains an open SQL cursor.
A static array of these structures allows multiple cursors
to be opened simultaneously.
*********************************************************************/

static struct OCURS {
    SQLHSTMT hstmt;
    long rownum;
    char rv_type[NUMRETS];
    short cid;			/* cursor ID */
    char flag;
    char numrets;
} ocurs[NUMCURSORS];

/* values for struct OCURS.flag */
#define CURSOR_NONE 0
#define CURSOR_PREPARED 1
#define CURSOR_OPENED 2

/* find a free cursor structure */
static struct OCURS *
findNewCursor(void)
{
    struct OCURS *o;
    short i;

    for(o = ocurs, i = 0; i < NUMCURSORS; ++i, ++o) {
	if(o->flag != CURSOR_NONE)
	    continue;
	if(o->hstmt == SQL_NULL_HSTMT) {
	    o->cid = 6000 + i;
	    rc = SQLAllocStmt(hdbc, &o->hstmt);
	    if(rc)
		errorPrint
		   ("@could not alloc ODBC statement handle for cursor %d",
		   o->cid);
	}
	return o;
    }
    errorPrint("2more than %d cursors opend concurrently", NUMCURSORS);
    return 0;			/* make the compiler happy */
}				/* findNewCursor */

/* dereference an existing cursor */
static struct OCURS *
findCursor(int cid)
{
    struct OCURS *o;
    if(cid < 6000 || cid >= 6000 + NUMCURSORS)
	errorPrint("2cursor number %d is out of range", cid);
    cid -= 6000;
    o = ocurs + cid;
    if(o->flag == CURSOR_NONE)
	errorPrint("2cursor %d is not currently active", cid + 6000);
    rv_numRets = o->numrets;
    memcpy(rv_type, o->rv_type, NUMRETS);
    return o;
}				/* findCursor */

/* This doesn't close/free anything; it simply puts variables in an initial state. */
/* part of the disconnect() procedure */
static void
clearAllCursors(void)
{
    short i;
    for(i = 0; i < NUMCURSORS; ++i) {
	ocurs[i].flag = CURSOR_NONE;
	ocurs[i].hstmt = SQL_NULL_HSTMT;
    }
}				/* clearAllCursors */


/*********************************************************************
Connect and disconect to SQL databases.
*********************************************************************/

/* disconnect from the database.  Return true if
 * an error occurs that is trapped by the application. */
static bool
disconnect(void)
{
    stmt_text = 0;
    hstmt = SQL_NULL_HSTMT;

    if(!sql_database)
	return false;		/* already disconnected */

    stmt_text = "disconnect";
    debugStatement();
    rc = SQLDisconnect(hdbc);
    if(errorTrap(0))
	return true;
    hstmt1 = SQL_NULL_HSTMT;	/* Disconnect frees all handles */
    clearAllCursors();		/* those handles are freed as well */
    translevel = 0;
    sql_database = 0;
    return false;
}				/* disconnect */

/* API level disconnect */
void
sql_disconnect(void)
{
    disconnect();
    exclist = 0;
}				/* sql_disconnect */

void
sql_connect(const char *db, const char *login, const char *pw)
{
    short waste;

    if(isnullstring(db)) {
	db = getenv("DBNAME");
	if(isnullstring(db))
	    errorPrint("2sql_connect receives no database, check $DBNAME");
    }

    /* first disconnect the old one */
    if(disconnect())
	return;

    /* initial call to sql_connect sets up ODBC */
    if(henv == SQL_NULL_HENV) {
	char verstring[6];

	/* Allocate environment and connection handles */
	/* these two handles are never freed */
	rc = SQLAllocEnv(&henv);
	if(rc)
	    errorPrint("@could not alloc ODBC environment handle");
	rc = SQLAllocConnect(henv, &hdbc);
	if(rc)
	    errorPrint("@could not alloc ODBC connection handle");

	/* Establish the ODBC major version number.
	 * Course the call to make this determination doesn't exist
	 * prior to version 2.0. */
	odbc_version = 1;
	rc = SQLGetInfo(hdbc, SQL_DRIVER_ODBC_VER, verstring, 6, &waste);
	if(!rc) {
	    verstring[2] = 0;
	    odbc_version = atoi(verstring);
	}
    }

    /* connect to the database */
    stmt_text = "connect";
    debugStatement();
    /* Guess odbc doesn't believe in const */
    rc = SQLConnect(hdbc, (char *)db, SQL_NTS,
       (char *)login, SQL_NTS, (char *)pw, SQL_NTS);
    if(errorTrap(0))
	return;
    sql_database = db;
    exclist = 0;

    /* Set the persistent connect/statement options.
     * Note that some of these merely reassert the default,
     * but it's good documentation to spell it out here.
     * Marked items fail under Informix CLI on NT,
     * but they are only trying to set the default.
     * Except TXN_ISOLATION.  Don't know if this will cause a problem. */
    stmt_text = "noscan on";
    rc = SQLSetConnectOption(hdbc, SQL_NOSCAN, SQL_NOSCAN_ON);
    stmt_text = "repeatable read";
    rc = SQLSetConnectOption(hdbc, SQL_TXN_ISOLATION, SQL_TXN_REPEATABLE_READ);	/* fail */
    stmt_text = "rowset size";
    rc = SQLSetConnectOption(hdbc, SQL_ROWSET_SIZE, 1);
    stmt_text = "login timeout";
    rc = SQLSetConnectOption(hdbc, SQL_LOGIN_TIMEOUT, 15);	/* fail */
    stmt_text = "query timeout";
    rc = SQLSetConnectOption(hdbc, SQL_QUERY_TIMEOUT, 0);	/* fail */
    stmt_text = "async disable";
    rc = SQLSetConnectOption(hdbc, SQL_ASYNC_ENABLE, SQL_ASYNC_ENABLE_OFF);	/* fail */
    stmt_text = "autocommit";
    rc = SQLSetConnectOption(hdbc, SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_ON);
    stmt_text = "cursor forward";
    rc = SQLSetConnectOption(hdbc, SQL_CURSOR_TYPE, SQL_CURSOR_FORWARD_ONLY);
    stmt_text = "concurrent reads";
    rc = SQLSetConnectOption(hdbc, SQL_CONCURRENCY, SQL_CONCUR_READ_ONLY);
    stmt_text = "use driver";
    rc = SQLSetConnectOption(hdbc, SQL_ODBC_CURSORS, SQL_CUR_USE_DRIVER);	/* fail */
    stmt_text = "no bookmarks";
    rc = SQLSetConnectOption(hdbc, SQL_USE_BOOKMARKS, SQL_UB_OFF);	/* fail */

    /* this call is only necessary if SQL_NULL_HSTMT != 0 */
    clearAllCursors();

    /* set defaults, in case the GetInfo command fails */
    cursors_under_commit = cursors_under_rollback = SQL_CB_DELETE;
    SQLGetInfo(hdbc, SQL_CURSOR_COMMIT_BEHAVIOR, &cursors_under_commit, 4,
       &waste);
    SQLGetInfo(hdbc, SQL_CURSOR_ROLLBACK_BEHAVIOR, &cursors_under_rollback, 4,
       &waste);
    getdata_opts = 0;
    SQLGetInfo(hdbc, SQL_GETDATA_EXTENSIONS, &getdata_opts, 4, &waste);

    /* Allocate a statement handle.  This handle survives for the duration
     * of the connection, and is used for single SQL directives
     * such as sql_exec() or sql_select().
     * Note that this handle allocation must take place after
     * the database connect statement above. */
    rc = SQLAllocStmt(hdbc, &hstmt1);
    if(rc)
	errorPrint("@could not alloc singleton ODBC statement handle");
    exclist = 0;
}				/* sql_connect */

/* make sure we're connected to a database */
static void
checkConnect(void)
{
    if(!sql_database)
	errorPrint("2SQL command issued, but no database selected");
}				/* checkConnect */


/*********************************************************************
Begin, commit, and abort transactions.
Remember that a completed transaction might blow away all cursors.
SQL does not permit nested transactions; this API does, to a limited degree.
An inner transaction cannot fail while an outer one succeeds;
that would require SQL support, which is not forthcoming.
However, as long as all transactions succeed, or the outer most fails,
everything works properly.
The static variable transLevel holds the number of nested transactions.
*********************************************************************/

/* begin a transaction */
void
sql_begTrans(void)
{
    checkConnect();
    stmt_text = 0;
    hstmt = SQL_NULL_HSTMT;
    rv_lastStatus = 0;		/* might never call errorTrap(0) */

    /* count the nesting level of transactions. */
    if(!translevel) {
	badtrans = false;
	stmt_text = "begin work";
	debugStatement();
	rc = SQLSetConnectOption(hdbc, SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF);
	if(errorTrap(0))
	    return;
    }

    ++translevel;
    exclist = 0;
}				/* sql_begTrans */

/* end a transaction */
static void
endTrans(bool commit)
{
    checkConnect();
    stmt_text = 0;
    hstmt = SQL_NULL_HSTMT;
    rv_lastStatus = 0;		/* might never call errorTrap(0) */

    if(!translevel)
	errorPrint("2end transaction without a matching begTrans()");
    --translevel;

    if(commit) {
	if(badtrans)
	    errorPrint
	       ("2Cannot commit a transaction around an aborted transaction");
	if(!translevel) {
	    stmt_text = "commit work";
	    debugStatement();
	    rc = SQLTransact(SQL_NULL_HENV, hdbc, SQL_COMMIT);
	    if(rc)
		++translevel;
	    errorTrap(0);
	}
    } else {			/* success or failure */
	badtrans = true;
	if(!translevel) {	/* bottom level */
	    stmt_text = "rollback work";
	    rc = SQLTransact(SQL_NULL_HENV, hdbc, SQL_ROLLBACK);
	    debugStatement();
	    if(rc)
		++translevel;
	    errorTrap(0);
	    badtrans = false;
	}
    }				/* success or failure */

    if(!translevel) {
	struct OCURS *o;
	short i, newstate;

	/* change the state of all cursors, if necessary */
	newstate = CURSOR_OPENED;
	if(commit) {
	    if(cursors_under_commit == SQL_CB_DELETE)
		newstate = CURSOR_NONE;
	    if(cursors_under_commit == SQL_CB_CLOSE)
		newstate = CURSOR_PREPARED;
	} else {
	    if(cursors_under_rollback == SQL_CB_DELETE)
		newstate = CURSOR_NONE;
	    if(cursors_under_rollback == SQL_CB_CLOSE)
		newstate = CURSOR_PREPARED;
	}

	for(i = 0; i < NUMCURSORS; ++i) {
	    o = ocurs + i;
	    if(o->flag > newstate)
		o->flag = newstate;
	}

	/* back to singleton transactions */
	rc = SQLSetConnectOption(hdbc, SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_ON);
	errorTrap(0);
    }
    /* just closed out the top level transaction */
    exclist = 0;
}				/* endTrans */

void
sql_commitWork(void)
{
    endTrans(true);
}
void
sql_rollbackWork(void)
{
    endTrans(false);
}

void
sql_deferConstraints(void)
{
    if(!translevel)
	errorPrint("2Cannot defer constraints unless inside a transaction");
    stmt_text = "defere constraints";
    debugStatement();
    /* is there a way to do this through ODBC? */
    hstmt = hstmt1;
    cleanStatement();
    rc = SQLExecDirect(hstmt, (char *)stmt_text, SQL_NTS);
    errorTrap(0);
    exclist = 0;
}				/* sql_deferConstraints */


/*********************************************************************
Blob management routines, a somewhat awkward interface.
Global variables tell SQL where to unload the next fetched blob:
either a file (truncate or append) or an allocated chunk of memory.
This assumes each fetch or select statement retrieves at most one blob.
Since there is no %blob directive in lineFormat(),
one cannot simply slip a blob in with the rest of the data as a row is
updated or inserted.  Instead the row must be created first,
then the blob is entered separately, using blobInsert().
This means every blob column must permit nulls, at least within the schema.
Also, what use to be an atomic insert might become a multi-statement
transaction if data integrity is important.
Future versions of our line formatting software may support a %blob directive,
which makes sense only when the formatted string is destined for SQL.
*********************************************************************/

static char blobbuf[100000];

void
sql_blobInsert(const char *tabname, const char *colname, int rowid,
   const char *filename, void *offset, int length)
{
    char blobcmd[100];
    SQLINTEGER output_length;
    bool isfile;
    int fd;

    /* basic sanity checks */
    checkConnect();
    if(isnullstring(tabname))
	errorPrint("2blobInsert, null table name");
    if(isnullstring(colname))
	errorPrint("2blobInsert, null column name");
    if(rowid <= 0)
	errorPrint("2invalid rowid in blobInsert");
    if(length < 0)
	errorPrint("2invalid length in blobInsert");
    if(strlen(tabname) + strlen(colname) + 42 >= sizeof (blobcmd))
	errorPrint("@internal blobInsert command too long");

    isfile = true;
    if(isnullstring(filename)) {
	isfile = false;
	if(!offset)
	    errorPrint("2blobInsert is given null filename and null buffer");
    } else {
	offset = blobbuf;
	fd = eopen(filename, O_RDONLY | O_BINARY, 0);
	length = fileSizeByHandle(fd);
	if(length == 0) {
	    isfile = false;
	    close(fd);
	}
    }

    /* set up the blob insert command, using one host variable */
    sprintf(blobcmd, "update %s set %s = %s where rowid = %d",
       tabname, colname, (length ? "?" : "NULL"), rowid);
    stmt_text = blobcmd;
    debugStatement();
    hstmt = hstmt1;
    cleanStatement();
    rv_lastNrows = 0;

    output_length = length;
    rc = SQL_SUCCESS;
    if(isfile) {
	output_length = SQL_LEN_DATA_AT_EXEC(length);
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
	   SQL_C_BINARY, SQL_LONGVARCHAR, length, 0,
	   blobcmd, length, &output_length);
	if(rc)
	    close(fd);
    } else if(length) {
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
	   SQL_C_BINARY, SQL_LONGVARCHAR, length, 0,
	   offset, length, &output_length);
    }
    if(errorTrap(0))
	return;

    rc = SQLExecDirect(hstmt, blobcmd, SQL_NTS);

    if(isfile) {
	if(rc != SQL_NEED_DATA) {
	    close(fd);
	    if(rc == SQL_SUCCESS)
		errorPrint("@blobInsert expected SQL_NEED_DATA");
	    errorTrap(0);
	    return;
	}
	/* return code other than need-more-data */
	output_length = 0;
	rc = SQLParamData(hstmt, (void **)&output_length);
	if((char *)output_length != blobcmd) {
	    close(fd);
	    errorPrint("2blobInsert got bad key from SQLParamData");
	}

	lseek(fd, 0L, 0);
	while(length) {
	    int n = length;
	    if(n > sizeof (blobbuf))
		n = sizeof (blobbuf);
	    if(read(fd, blobbuf, n) != n) {
		close(fd);
		errorPrint("2cannot read file %s, errno %d", filename, errno);
	    }
	    length -= n;

	    rc = SQLPutData(hstmt, blobbuf, n);
	    if(rc) {
		close(fd);
		errorTrap(0);
		return;
	    }
	}			/* loop reading the file */

	close(fd);

	/* since there are no more exec-time parameters,
	 * this call completes the execution of the SQL statement. */
	rc = SQLParamData(hstmt, (void **)&output_length);
    }
    /* blob is drawn from a file */
    if(errorTrap(0))
	return;
    rc = SQLRowCount(hstmt, &rv_lastNrows);
    errorTrap(0);
    if(sql_debug)
	appendFile(sql_debuglog, "%d rows affected", rv_lastNrows);
    exclist = 0;
}				/* sql_blobInsert */


/*********************************************************************
Pass back a variable number of returns from
an SQL data gathering function such as select() or fetch().
This uses SQLGetData(), assuming a select or fetch has been performed.
The prefix rv_ on the following global variables indicates returned values.
*********************************************************************/

/* Where to stash the retrieved values */
static va_list sqlargs;

int rv_numRets;
char rv_type[NUMRETS + 1];
/* names of returned data, usually SQL column names */
char rv_name[NUMRETS + 1][COLNAMELEN];
LF rv_data[NUMRETS];		/* the returned values */
/* Temp area to read the Informix values, as strings */
static char retstring[NUMRETS][STRINGLEN + 4];
long rv_lastNrows, rv_lastSerial, rv_lastRowid;
void *rv_blobLoc;		/* location of blob in memory */
int rv_blobSize;
const char *rv_blobFile;
bool rv_blobAppend;
static bool everything_null;

static void
retsFromOdbc(void)
{
    void *q, *q1;
    int i, l;
    int fd, flags;
    bool yearfirst, indata = false;
    long dt;			/* temporarily hold date or time */
    char *s;
    short c_type;		/* C data type */
    long input_length, output_length;
    char tbuf[20];		/* temp buf, for dates and times */
    double fmoney;		/* float version of money */

    /* no blobs unless proven otherwise */
    rv_blobLoc = 0;
    rv_blobSize = nullint;

    if(!rv_numRets)
	errorPrint("@calling retsFromOdbc() with no returns pending");

    stmt_text = "retsFromOdbc";
    for(i = 0; i < rv_numRets; ++i) {
	if(!indata) {
	    q = va_arg(sqlargs, void *);
	    if(!q) {
		if(i)
		    break;
		indata = true;
	    }
	}
	if(indata) {
	    if(rv_type[i] == 'S') {
		q = retstring[i];
		rv_data[i].ptr = q;
	    } else
		q = rv_data + i;
	}
	if((int)q < 1000 && (int)q > -1000)
	    errorPrint("2retsFromOdbc, pointer too close to 0");
	q1 = q;
	tbuf[0] = 0;
	output_length = 0;

	switch (rv_type[i]) {
	case 'S':
	    c_type = SQL_C_CHAR;
	    input_length = STRINGLEN + 1;
	    *(char *)q = 0;	/* null */
	    break;

	case 'C':
	    c_type = SQL_C_CHAR;
	    input_length = 2;
	    *(char *)q = 0;	/* null */
	    q1 = tbuf;
	    break;

	case 'F':
	    c_type = SQL_C_DOUBLE;
	    input_length = 8;
	    *(double *)q = nullfloat;	/* null */
	    break;

	case 'N':
	    c_type = SQL_C_SLONG;
	    input_length = 4;
	    *(long *)q = nullint;	/* null */
	    break;

	case 'M':
	    c_type = SQL_C_DOUBLE;
	    input_length = 8;
	    fmoney = nullfloat;
	    q1 = &fmoney;
	    break;

	case 'D':
	    c_type = SQL_C_CHAR;
	    input_length = 11;
	    q1 = tbuf;
	    break;

	case 'I':
	    c_type = SQL_C_CHAR;
	    input_length = 10;
	    q1 = tbuf;
	    break;

	case 'B':
	case 'T':
	    c_type = SQL_C_BINARY;
	    input_length = sizeof (blobbuf);
	    q1 = blobbuf;
	    *(long *)q = nullint;
	    break;

	default:
	    errorPrint("@retsFromOdbc, rv_type[%d] = %c", i, rv_type[i]);
	}			/* switch */

	if(everything_null) {
	    rc = SQL_SUCCESS;
	    output_length = SQL_NULL_DATA;
	} else {
	    rc = SQLGetData(hstmt, (ushort) (i + 1),
	       c_type, q1, input_length, &output_length);
	    /* we'll deal with blob overflow later */
	    if(rc == SQL_SUCCESS_WITH_INFO && c_type == SQL_C_BINARY &&
	       output_length > sizeof (blobbuf))
		rc = SQL_SUCCESS;
	    if(errorTrap(0))
		break;
	    if(output_length == SQL_NO_TOTAL)
		errorPrint
		   ("@retsFromOdbc cannot get size of data for column %d",
		   i + 1);
	}

	/* Postprocess the return values. */
	/* For instance, turn string dates into our own 4-byte format. */
	s = tbuf;
	clipString(s);
	switch (rv_type[i]) {
	case 'C':
	    *(char *)q = tbuf[0];
	    break;

	case 'S':
	    clipString(q);
	    break;

	case 'D':
	    yearfirst = false;
	    if(s[4] == '-')
		yearfirst = true;
	    dt = stringDate(s, yearfirst);
	    if(dt < 0)
		errorPrint("@database holds invalid date %s", s);
	    *(long *)q = dt;
	    break;

	case 'I':
	    /* thanks to stringTime(), this works
	       for either hh:mm or hh:mm:ss */
	    if(s[0] == 0)
		*(long *)q = nullint;
	    else {
		/* Note that Informix introduces a leading space,
		   how about ODBC? */
		leftClipString(s);
		if(s[1] == ':')
		    shiftRight(s, '0');
		dt = stringTime(s);
		if(dt < 0)
		    errorPrint("@database holds invalid time %s", s);
		*(long *)q = dt;
	    }
	    break;

	case 'M':
	    if(fmoney == nullfloat)
		dt = nullint;
	    else
		dt = fmoney * 100.0 + 0.5;
	    *(long *)q = dt;
	    break;

	case 'B':
	case 'T':
	    if(output_length == SQL_NULL_DATA)
		break;
	    /* note, 0 length blob is treated as a null blob */
	    if(output_length == 0)
		break;
	    /* the size of the blob is returned, in an int. */
	    *(long *)q = output_length;
	    rv_blobSize = output_length;

	    if(isnullstring(rv_blobFile)) {
		/* the blob is always allocated; you have to free it! */
		/* SQL doesn't null terminate its text blobs, but we do. */
		rv_blobLoc = allocMem(output_length + 1);
		l = output_length;
		if(l > sizeof (blobbuf))
		    l = sizeof (blobbuf);
		memcpy(rv_blobLoc, blobbuf, l);
		if(l < output_length) {	/* more to do */
		    long waste;
		    rc = SQLGetData(hstmt, (ushort) (i + 1),
		       c_type, (char *)rv_blobLoc + l,
		       output_length - l, &waste);
		    if(rc) {
			nzFree(rv_blobLoc);
			rv_blobLoc = 0;
			*(long *)q = nullint;
			errorTrap(0);
			goto breakloop;
		    }		/* error getting rest of blob */
		}		/* blob is larger than the buffer */
		if(rv_type[i] == 'T')	/* null terminate */
		    ((char *)rv_blobLoc)[output_length] = 0;
		break;
	    }

	    /* blob in memory */
	    /* at this point the blob is being dumped into a file */
	    flags = O_WRONLY | O_BINARY | O_CREAT | O_TRUNC;
	    if(rv_blobAppend)
		flags = O_WRONLY | O_BINARY | O_CREAT | O_APPEND;
	    fd = eopen(rv_blobFile, flags, 0666);
	    rc = SQL_SUCCESS;
	    while(true) {
		int outbytes;
		l = output_length;
		if(l > sizeof (blobbuf))
		    l = sizeof (blobbuf);
		outbytes = write(fd, blobbuf, l);
		if(outbytes < l) {
		    close(fd);
		    errorPrint("2cannot write to file %s, errno %d",
		       rv_blobFile, errno);
		}
		if(l == output_length)
		    break;

		/* get the next chunk from ODBC */
		rc = SQLGetData(hstmt, (ushort) (i + 1),
		   c_type, q1, input_length, &output_length);
		if(rc == SQL_SUCCESS_WITH_INFO && output_length > input_length)
		    rc = SQL_SUCCESS;	/* data truncation error */
	    }

	    close(fd);
	    errorTrap(0);
	    break;

	}			/* switch */

    }				/* loop over returned elements */
  breakloop:

    va_end(sqlargs);
    exclist = 0;
}				/* retsFromOdbc */

/* make sure we got one return value, and it is integer compatible */
/* The dots make the compiler happy. */
/* You must call this with args of 0,0 */
static long
oneRetValue(void *pre_x, ...)
{
    char coltype = rv_type[0];
    char c;
    long n;
    double f;
    void **x = (void **)((char *)&pre_x + 4);

    va_end(sqlargs);
    if(rv_numRets != 1)
	errorPrint("2SQL statement has %d return values, 1 value expected",
	   rv_numRets);
    if(!strchr("MNDIC", coltype))
	errorPrint
	   ("2SQL statement returns a value whose type is not compatible with a 4-byte integer");

    va_start(sqlargs, pre_x);
/* I'm not sure float to int really works. */
    if(coltype == 'F') {
	*x = &f;
	retsFromOdbc();
	n = f;
    } else if(coltype == 'C') {
	*x = &c;
	retsFromOdbc();
	n = c;
    } else {
	*x = &n;
	retsFromOdbc();
    }
    return n;
}				/* oneRetValue */


/*********************************************************************
Prepare a formatted SQL statement.
Gather the types and names of the fetched columns and make this information
available to the rest of the C routines in this file, and to the application.
Returns false if the prepare failed.
*********************************************************************/

static bool
prepare(SQLHSTMT h, const char *stmt)
{
    short i, nc, coltype, colscale, nullable, namelen;
    unsigned long colprec;
    bool blobpresent = false;

    checkConnect();
    everything_null = true;
    if(isnullstring(stmt))
	errorPrint("2null SQL statement");
    stmt_text = stmt;
    if(sql_debug)
	appendFileNF(sql_debuglog, stmt);
    hstmt = h;			/* set working statement handle */
    cleanStatement();

    /* look for delete with no where clause */
    skipWhite(&stmt);
    if(!strncmp(stmt, "delete", 6) || !strncmp(stmt, "update", 6))
	/* delete or update */
	if(!strstr(stmt, "where") && !strstr(stmt, "WHERE")) {
	    showStatement();
	    errorPrint("2Old Mcdonald bug");
	}

    rv_numRets = 0;
    memset(rv_type, 0, NUMRETS);
    rv_lastNrows = 0;

    rc = SQLPrepare(hstmt, (char *)stmt, SQL_NTS);
    if(errorTrap(0))
	return false;

    /* gather column headings and types */
    rc = SQLNumResultCols(hstmt, &nc);
    if(errorTrap(0))
	return false;

    if(nc > NUMRETS) {
	showStatement();
	errorPrint("2cannot select more than %d values", NUMRETS);
    }

    for(i = 0; i < nc; ++i) {
	rc = SQLDescribeCol(hstmt, (USHORT) (i + 1),
	   rv_name[i], COLNAMELEN, &namelen,
	   &coltype, &colprec, &colscale, &nullable);
	if(errorTrap("01004"))
	    return false;

/*********************************************************************
The following is an Informix kludge,
because intervals are not mapped back into SQL_TIME,
as they should be.  Don't create any varchar(24,0) columns.
Also, ODBC hasn't got any money type.
Under informix, it comes back as decimal.
We never use decimal columns, so we can turn decimal back into money.
However, some aggregate expressions also come back as decimal.
Count(*) becomes decimal(15,0).  So be careful.
*********************************************************************/

	if(coltype == SQL_VARCHAR && colprec == 24 && colscale == 0)
	    coltype = SQL_TIME;
#define SQL_MONEY 100
	if(coltype == SQL_DECIMAL && (colprec != 15 || colscale != 0))
	    coltype = SQL_MONEY;

	switch (coltype) {
	case SQL_BIT:
	case SQL_TINYINT:
	case SQL_SMALLINT:
	case SQL_INTEGER:
	case SQL_BIGINT:
	case SQL_NUMERIC:
	    rv_type[i] = 'N';
	    break;

	case SQL_TIMESTAMP:
	    /* We only process datetime year to minute,
	     * for databases other than Informix,
	     * which don't have a date type. */
	    if(colprec != 4)
		errorPrint("@timestamp field must be year to minute");
	case SQL_DATE:
	    rv_type[i] = 'D';
	    break;

	case SQL_DOUBLE:
	case SQL_FLOAT:
	case SQL_DECIMAL:
	case SQL_REAL:
	    rv_type[i] = 'F';
	    break;

	case SQL_TIME:
	    rv_type[i] = 'I';
	    break;

	case SQL_CHAR:
	case SQL_VARCHAR:
	    rv_type[i] = 'S';
	    if(colprec == 1)
		rv_type[i] = 'C';
	    if(colprec > STRINGLEN) {
		showStatement();
		errorPrint("2column %s exceeds %d characters", rv_name[i],
		   STRINGLEN);
	    }
	    break;

	case SQL_LONGVARCHAR:
	case SQL_BINARY:
	case SQL_VARBINARY:
	case SQL_LONGVARBINARY:
	    if(blobpresent) {
		showStatement();
		errorPrint("2Statement selects more than one blob column");
	    }
	    blobpresent = true;
	    rv_type[i] = (coltype == SQL_LONGVARCHAR ? 'T' : 'B');
	    break;

	case SQL_MONEY:
	    rv_type[i] = 'M';
	    break;

	default:
	    errorPrint("@Unknown sql datatype %d", coltype);
	}			/* switch on type */
    }				/* loop over returns */

    rv_numRets = nc;
    return true;
}				/* prepare */


/*********************************************************************
Run an SQL statement internally, and gather any fetched values.
This statement stands alone; it fetches at most one row.
You might simply know this, perhaps because of a unique constraint,
or you might be running a stored procedure.
For efficiency we do not look for a second row, so this is really
like the "select first" construct that some databases support.
A mode variable says whether execution or selection or both are allowed.
Return true if data was successfully fetched.
*********************************************************************/

static bool
execInternal(const char *stmt, int mode)
{
    bool notfound = false;
    SQLINTEGER rowcnt;
    short rowstat[2];

    if(!prepare(hstmt1, stmt))
	return false;		/* error */

    if(!rv_numRets) {
	if(!(mode & 1)) {
	    showStatement();
	    errorPrint("2SQL select statement returns no values");
	}
	notfound = true;
    } else {			/* end no return values */

	if(!(mode & 2)) {
	    showStatement();
	    errorPrint("2SQL statement returns %d values", rv_numRets);
	}
    }

    rc = SQLExecute(hstmt);

    if(!rc) {			/* statement succeeded */
	/* fetch the data, or get a count of the rows affected */
	if(rv_numRets) {
	    rc = SQLExtendedFetch(hstmt, (ushort) SQL_FD_FETCH_NEXT, 1,
	       &rowcnt, rowstat);
	    if(rc == SQL_NO_DATA_FOUND)
		rc = SQL_SUCCESS;
	    if(!rowcnt)
		notfound = true;
	    else
		everything_null = false;
	} else {
	    rc = SQLRowCount(hstmt, &rv_lastNrows);
	    if(sql_debug && rv_lastNrows)
		appendFile(sql_debuglog, "%d rows affected", rv_lastNrows);
	}
    }
    /* successful query */
    if(errorTrap(0))
	return false;
    return !notfound;
}				/* execInternal */


/*********************************************************************
Run individual select or execute statements, using the above internal routine.
*********************************************************************/

/* execute a stand-alone statement with no % formatting of the string */
void
sql_execNF(const char *stmt)
{
    execInternal(stmt, 1);
    exclist = 0;
}				/* sql_execNF */

/* execute a stand-alone statement with % formatting */
void
sql_exec(const char *stmt, ...)
{
    va_start(sqlargs, stmt);
    stmt = lineFormatStack(stmt, 0, &sqlargs);
    execInternal(stmt, 1);
    exclist = 0;
    va_end(sqlargs);
}				/* sql_exec */

/* run a select statement with % formatting */
/* return true if the row was found */
bool
sql_select(const char *stmt, ...)
{
    bool rowfound;
    va_start(sqlargs, stmt);
    stmt = lineFormatStack(stmt, 0, &sqlargs);
    rowfound = execInternal(stmt, 2);
    retsFromOdbc();
    return rowfound;
}				/* sql_select */

/* run a select statement with no % formatting of the string */
bool
sql_selectNF(const char *stmt, ...)
{
    bool rowfound;
    va_start(sqlargs, stmt);
    rowfound = execInternal(stmt, 2);
    retsFromOdbc();
    return rowfound;
}				/* sql_selectNF */

/* run a select statement with one return value */
int
sql_selectOne(const char *stmt, ...)
{
    bool rowfound;
    va_start(sqlargs, stmt);
    stmt = lineFormatStack(stmt, 0, &sqlargs);
    rowfound = execInternal(stmt, 2);
    if(!rowfound) {
	exclist = 0;
	va_end(sqlargs);
	return nullint;
    }
    return oneRetValue(0, 0);
}				/* sql_selectOne */

/* run a stored procedure with no % formatting */
static bool
sql_procNF(const char *stmt)
{
    bool rowfound;
    char *s = allocMem(20 + strlen(stmt));
    strcpy(s, "execute procedure ");
    strcat(s, stmt);
    rowfound = execInternal(s, 3);
    /* if execInternal doesn't return, we have a memory leak */
    nzFree(s);
    return rowfound;
}				/* sql_procNF */

/* run a stored procedure */
bool
sql_proc(const char *stmt, ...)
{
    bool rowfound;
    va_start(sqlargs, stmt);
    stmt = lineFormatStack(stmt, 0, &sqlargs);
    rowfound = sql_procNF(stmt);
    retsFromOdbc();
    return rowfound;
}				/* sql_proc */

/* run a stored procedure with one return */
int
sql_procOne(const char *stmt, ...)
{
    bool rowfound;
    va_start(sqlargs, stmt);
    stmt = lineFormatStack(stmt, 0, &sqlargs);
    rowfound = sql_procNF(stmt);
    if(!rowfound) {
	exclist = 0;
	va_end(sqlargs);
	return 0;
    }
    return oneRetValue(0, 0);
}				/* sql_procOne */


/*********************************************************************
Prepare, open, close, and free SQL cursors.
*********************************************************************/

/* prepare a cursor; return the ID number of that cursor */
static int
prepareCursor(const char *stmt, bool scrollflag)
{
    struct OCURS *o = findNewCursor();
    stmt = lineFormatStack(stmt, 0, &sqlargs);
    va_end(sqlargs);

    hstmt = o->hstmt;
    rc = SQLSetStmtOption(hstmt, SQL_CURSOR_TYPE,
       scrollflag ? SQL_CURSOR_STATIC : SQL_CURSOR_FORWARD_ONLY);
    if(errorTrap(0))
	return -1;

    if(!prepare(hstmt, stmt))
	return -1;
    if(!rv_numRets) {
	showStatement();
	errorPrint("2statement passed to sql_prepare has no returns");
    }

    o->numrets = rv_numRets;
    memcpy(o->rv_type, rv_type, NUMRETS);
    o->flag = CURSOR_PREPARED;
    return o->cid;
}				/* prepareCursor */

int
sql_prepare(const char *stmt, ...)
{
    int n;
    va_start(sqlargs, stmt);
    n = prepareCursor(stmt, false);
    exclist = 0;
    return n;
}				/* sql_prepare */

int
sql_prepareScrolling(const char *stmt, ...)
{
    int n;
    va_start(sqlargs, stmt);
    n = prepareCursor(stmt, true);
    exclist = 0;
    return n;
}				/* sql_prepareScrolling */

void
sql_open(int cid)
{
    short i;
    struct OCURS *o = findCursor(cid);
    if(o->flag == CURSOR_OPENED)
	errorPrint("2cannot open cursor %d, already opened", cid);

    stmt_text = "open";
    hstmt = o->hstmt;
    rc = SQLExecute(hstmt);
    if(errorTrap(0))
	return;
    o->flag = CURSOR_OPENED;
    o->rownum = 0;
    exclist = 0;
}				/* sql_open */

int
sql_prepOpen(const char *stmt, ...)
{
    int n;
    struct OCURS *o;

    va_start(sqlargs, stmt);
    n = prepareCursor(stmt, false);
    if(n < 0)
	return n;

    o = findCursor(n);
    sql_open(n);
    if(rv_lastStatus) {
	o->flag = CURSOR_NONE;	/* back to square 0 */
	rv_numRets = 0;
	memset(rv_type, 0, sizeof (rv_type));
	n = -1;
    }
    /* open failed */
    exclist = 0;
    return n;
}				/* sql_prepOpen */

void
sql_close(int cid)
{
    struct OCURS *o = findCursor(cid);
    if(o->flag < CURSOR_OPENED)
	errorPrint("2cannot close cursor %d, not yet opened", cid);

    stmt_text = "close";
    hstmt = o->hstmt;
    rc = SQLFreeStmt(hstmt, SQL_CLOSE);
    if(errorTrap(0))
	return;
    o->flag = CURSOR_PREPARED;
    exclist = 0;
}				/* sql_close */

void
sql_free(int cid)
{
    struct OCURS *o = findCursor(cid);
    if(o->flag == CURSOR_OPENED)
	errorPrint("2cannot free cursor %d, not yet closed", cid);

    stmt_text = "free";
    hstmt = o->hstmt;
    rc = SQLFreeStmt(hstmt, SQL_UNBIND);
    if(errorTrap(0))
	return;
    o->flag = CURSOR_NONE;
    rv_numRets = 0;
    memset(rv_type, 0, sizeof (rv_type));
    exclist = 0;
}				/* sql_free */

void
sql_closeFree(int cid)
{
    const short *exc = exclist;
    sql_close(cid);
    if(!rv_lastStatus) {
	exclist = exc;
	sql_free(cid);
    }
}				/* sql_closeFree */

/* fetch row n from the open cursor.
 * Flag can be used to fetch first, last, next, or previous. */
static bool
fetchInternal(int cid, long n, int flag)
{
    long nextrow, lastrow;
    SQLINTEGER rowcnt;
    short rowstat[2];
    struct OCURS *o = findCursor(cid);

    everything_null = true;

    /* don't do the fetch if we're looking for row 0 absolute,
     * that just nulls out the return values */
    if(flag == SQL_FD_FETCH_ABSOLUTE && !n) {
	o->rownum = 0;
      fetchZero:
	return false;
    }

    lastrow = nextrow = o->rownum;
    if(flag == SQL_FD_FETCH_ABSOLUTE)
	nextrow = n;
    if(flag == SQL_FD_FETCH_FIRST)
	nextrow = 1;
    if(isnotnull(lastrow)) {	/* we haven't lost track yet */
	if(flag == SQL_FD_FETCH_NEXT)
	    ++nextrow;
	if(flag == SQL_FD_FETCH_PREV && nextrow)
	    --nextrow;
    }
    if(flag == SQL_FD_FETCH_LAST) {
	nextrow = nullint;	/* we just lost track */
    }

    if(!nextrow)
	goto fetchZero;

    if(o->flag != CURSOR_OPENED)
	errorPrint("2cannot fetch from cursor %d, not yet opened", cid);

    /* The next line of code is very subtle.
       I use to declare all cursors as scroll cursors.
       It's a little inefficient, but who cares.
       Then I discovered you can't fetch blobs from scroll cursors.
       You can however fetch them from regular cursors,
       even with an order by clause.
       So cursors became non-scrolling by default.
       If the programmer chooses to fetch by absolute number,
       but he is really going in sequence, I turn them into
       fetch-next statements, so that the cursor need not be a scroll cursor. */
    if(flag == SQL_FD_FETCH_ABSOLUTE) {
	if(isnull(nextrow))
	    errorPrint("2sql fetches absolute row using null index");
	if(isnotnull(lastrow) && nextrow == lastrow + 1)
	    flag = SQL_FD_FETCH_NEXT;
    }

    stmt_text = "fetch";
    hstmt = o->hstmt;
    rc = SQLExtendedFetch(hstmt, (ushort) flag, 1, &rowcnt, rowstat);
    if(rc == SQL_NO_DATA_FOUND)
	rc = SQL_SUCCESS;
    if(errorTrap(0))
	return false;
    if(!rowcnt)
	return false;		/* not found */
    o->rownum = nextrow;
    everything_null = false;
    return true;
}				/* fetchInternal */

bool
sql_fetchFirst(int cid, ...)
{
    bool rowfound;
    va_start(sqlargs, cid);
    rowfound = fetchInternal(cid, 0L, SQL_FD_FETCH_FIRST);
    retsFromOdbc();
    return rowfound;
}				/* sql_fetchFirst */

bool
sql_fetchLast(int cid, ...)
{
    bool rowfound;
    va_start(sqlargs, cid);
    rowfound = fetchInternal(cid, 0L, SQL_FD_FETCH_LAST);
    retsFromOdbc();
    return rowfound;
}				/* sql_fetchLast */

bool
sql_fetchNext(int cid, ...)
{
    bool rowfound;
    va_start(sqlargs, cid);
    rowfound = fetchInternal(cid, 0L, SQL_FD_FETCH_NEXT);
    retsFromOdbc();
    return rowfound;
}				/* sql_fetchNext */

bool
sql_fetchPrev(int cid, ...)
{
    bool rowfound;
    va_start(sqlargs, cid);
    rowfound = fetchInternal(cid, 0L, SQL_FD_FETCH_PREV);
    retsFromOdbc();
    return rowfound;
}				/* sql_fetchPrev */

bool
sql_fetchAbs(int cid, long rownum, ...)
{
    bool rowfound;
    va_start(sqlargs, rownum);
    rowfound = fetchInternal(cid, rownum, SQL_FD_FETCH_ABSOLUTE);
    retsFromOdbc();
    return rowfound;
}				/* sql_fetchAbs */
