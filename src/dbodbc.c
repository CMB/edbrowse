/*********************************************************************
odbc.c: C-level interface to SQL.
This is a layer above ODBC,
since ODBC is often difficult to use, especially for new programmers.
Most SQL queries are relatively simple, whence the ODBC API is overkill.
Why mess with statement handles and parameter bindings when you can write:
sql_select("select this, that from table1, table2 where keycolumn = %d",
27, &this, &that);

Note that this API works within the context of our own C programming
environment.

Note that dbapi.h does NOT include the ODBC system header files.
That would violate the spirit of this layer,
which attempts to sheild the application from the details of ODBC.
If the application needed to see anything in those header files,
we would be doing something wrong.
*********************************************************************/

#ifdef _WIN32
#include <WinSock2.h>		// also includes <windows.h>
	// needed for sql.h which uses say HWND, DWORD, etc...
#endif

#include <sql.h>		/* ODBC header files */
#include <sqlext.h>

#include "eb.h"
#include "dbapi.h"

enum {
	DRIVER_NONE,
	DRIVER_SQLITE,
	DRIVER_MYSQL,
	DRIVER_POSTGRESQL,
	DRIVER_INFORMIX,
	DRIVER_TDS,
	DRIVER_ORACLE,
	DRIVER_DB2,
};
static int current_driver;

/* Some drivers, Microsoft in particular,
 * provide no information until you actually run the query.
 * Prepare is not enough.
 * The openfirst variable tells us whether we are running in that mode. */
static bool openfirst = false;

#define SQL_MONEY 100

/*********************************************************************
The status variable rc holds the return code from an ODBC call.
This is then used by the function errorTrap() below.
If rc != 0, errorTrap() prints a message:
the generic type of error from our translation, and the message it gets from odbc.
It may skip this however, if you have trapped for this type of error.
This lets the application print a message that is shorter,
and easier to understand, and has the potential to be internationalized.
In this case the calling routine should clean up as best it can and return.
*********************************************************************/

/* characteristics of the current ODBC driver */
static short odbc_version;
static long cursors_under_commit, cursors_under_rollback;
static long getdata_opts;
static long bookmarkBits;

static SQLHENV henv = SQL_NULL_HENV;	/* environment handle */
static SQLHDBC hdbc = SQL_NULL_HDBC;	/* identify connect session */
static SQLHSTMT hstmt = SQL_NULL_HSTMT;	/* current statement */
static const char *stmt_text = 0;	/* text of the SQL statement */
static SQLRETURN rc;
static const short *exclist;	/* list of error codes trapped by the application */
static short translevel;
static bool badtrans;

/* Through globals, make error info available to the application. */
int rv_lastStatus, rv_stmtOffset;
long rv_vendorStatus;
char *rv_badToken;

static void debugStatement(void)
{
	if (!stmt_text)
		return;
	if (sql_debug)
		appendFileNF(sql_debuglog, stmt_text);
	if (sql_debug2)
		puts(stmt_text);
}				/* debugStatement */

/* Append the SQL statement to the debug log.  This is not strictly necessary
 * if sql_debug is set, since the statement has already been appended. */
static void showStatement(void)
{
	if (!stmt_text)
		return;
	if (!sql_debug)
		appendFileNF(sql_debuglog, stmt_text);
}				/* showStatement */

/* application sets the exception list */
void sql_exclist(const short *list)
{
	exclist = list;
}

void sql_exception(int errnum)
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

static int errTranslate(const char *code)
{
	const struct ERRORMAP *e;

	for (e = errormap; e->odbc[0]; ++e) {
		if (stringEqual(e->odbc, code))
			return e->excno;
	}
	return EXCSQLMISC;
}				/* errTranslate */

static uchar errorText[200];

static bool errorTrap(const char *cxerr)
{
	short i, waste;
	char errcodes[6];
	bool firstError, errorFound;

	/* innocent until proven guilty */
	rv_lastStatus = 0;
	rv_vendorStatus = 0;
	rv_stmtOffset = 0;
	rv_badToken = 0;
	if (!rc)
		return false;	/* no problem */

	/* log the SQL statement that elicitted the error */
	showStatement();

	if (rc == SQL_INVALID_HANDLE)
		errorPrint
		    ("@ODBC fails to recognize one of the handles (env, connect, stmt)");

	/* get error info from ODBC */
	firstError = true;
	errorFound = false;

	while (true) {
		rc = SQLError(henv, hdbc, hstmt,
			      (uchar *) errcodes, &rv_vendorStatus, errorText,
			      sizeof(errorText), &waste);
		if (rc == SQL_NO_DATA) {
			if (firstError) {
				printf
				    ("ODBC command failed, but SQLError() provided no additional information\n");
				return true;
			}
			return errorFound;
		}

		/* Skip past the ERROR-IN-ROW errors. */
		if (stringEqual(errcodes, "01S01"))
			continue;

		firstError = false;
		if (cxerr && stringEqual(cxerr, errcodes))
			continue;

		if (errorFound)
			continue;
		errorFound = true;
		rv_lastStatus = errTranslate(errcodes);

/* Don't know how to get statement ofset or invalid token from ODBC.
 * I can get them from Informix; see dbinfx.ec */

/* if the application didn't trap for this exception, blow up! */
		if (exclist) {
			for (i = 0; exclist[i]; ++i)
				if (exclist[i] == rv_lastStatus)
					break;
			if (exclist[i]) {
				exclist = 0;	/* we've spent that exception */
				continue;
			}
		}

		printf("ODBC error %s, %s, driver %s\n",
		       errcodes, sqlErrorList[rv_lastStatus], errorText);
		setError(MSG_DBUnexpected, rv_vendorStatus);
	}
}				/* errorTrap */

static void newStatement(void)
{
	rc = SQLAllocStmt(hdbc, &hstmt);
	if (rc)
		errorPrint("@could not alloc singleton ODBC statement handle");
}				/* newStatement */

/*********************************************************************
The OCURS structure given below maintains an open SQL cursor.
A static array of these structures allows multiple cursors
to be opened simultaneously.
*********************************************************************/

static struct OCURS {
	SQLHSTMT hstmt;
	long rownum;
	char rv_type[NUMRETS];
	short cid;		/* cursor ID */
	char flag;
	char numrets;
} ocurs[NUMCURSORS];

/* values for struct OCURS.flag */
#define CURSOR_NONE 0
#define CURSOR_PREPARED 1
#define CURSOR_OPENED 2

/* find a free cursor structure */
static struct OCURS *findNewCursor(void)
{
	struct OCURS *o;
	short i;

	for (o = ocurs, i = 0; i < NUMCURSORS; ++i, ++o) {
		if (o->flag != CURSOR_NONE)
			continue;
		o->cid = 6000 + i;
		rc = SQLAllocStmt(hdbc, &o->hstmt);
		if (rc)
			errorPrint
			    ("@could not alloc ODBC statement handle for cursor %d",
			     o->cid);
		return o;
	}

	errorPrint("2more than %d cursors opend concurrently", NUMCURSORS);
	return 0;		/* make the compiler happy */
}				/* findNewCursor */

/* dereference an existing cursor */
static struct OCURS *findCursor(int cid)
{
	struct OCURS *o;
	if (cid < 6000 || cid >= 6000 + NUMCURSORS)
		errorPrint("2cursor number %d is out of range", cid);
	cid -= 6000;
	o = ocurs + cid;
	if (o->flag == CURSOR_NONE)
		errorPrint("2cursor %d is not currently active", cid + 6000);
	rv_numRets = o->numrets;
	memcpy(rv_type, o->rv_type, NUMRETS);
	return o;
}				/* findCursor */

/* This doesn't close/free anything; it simply puts variables in an initial state. */
/* part of the disconnect() procedure */
static void clearAllCursors(void)
{
	short i;
	for (i = 0; i < NUMCURSORS; ++i) {
		ocurs[i].flag = CURSOR_NONE;
		ocurs[i].hstmt = SQL_NULL_HSTMT;
	}
}				/* clearAllCursors */

/*********************************************************************
Connect and disconect to SQL databases.
*********************************************************************/

/* disconnect from the database.  Return true if
 * an error occurs that is trapped by the application. */
static bool disconnect(void)
{
	stmt_text = 0;
	hstmt = SQL_NULL_HSTMT;

	if (!sql_database)
		return false;	/* already disconnected */

	stmt_text = "disconnect";
	debugStatement();
	rc = SQLDisconnect(hdbc);
	if (errorTrap(0))
		return true;
	clearAllCursors();	/* those handles are freed as well */
	translevel = 0;
	sql_database = 0;
	return false;
}				/* disconnect */

/* API level disconnect */
void sql_disconnect(void)
{
	disconnect();
	exclist = 0;
}				/* sql_disconnect */

void sql_connect(const char *db, const char *login, const char *pw)
{
	short waste;
	char constring[200];
	uchar outstring[200];
	char drivername[40];
	char *s;

	if (isnullstring(db))
		errorPrint
		    ("2sql_connect receives no data source, check your edbrowse config file");
	if (debugLevel >= 1)
		i_printf(MSG_DBConnecting, db);

	/* first disconnect the old one */
	if (disconnect())
		return;

	/* initial call to sql_connect sets up ODBC */
	if (henv == SQL_NULL_HENV) {
		char verstring[6];

		/* Allocate environment and connection handles */
		/* these two handles are never freed */
		rc = SQLAllocEnv(&henv);
		if (rc)
			errorPrint("@could not alloc ODBC environment handle");
		rc = SQLAllocConnect(henv, &hdbc);
		if (rc)
			errorPrint("@could not alloc ODBC connection handle");

		/* Establish the ODBC major version number.
		 * Course the call to make this determination doesn't exist
		 * prior to version 2.0. */
		odbc_version = 1;
		rc = SQLGetInfo(hdbc, SQL_DRIVER_ODBC_VER, verstring, 6,
				&waste);
		if (!rc) {
			verstring[2] = 0;
			odbc_version = atoi(verstring);
		}
	}

	/* connect to the database */
	sprintf(constring, "DSN=%s", db);
	if (login) {
		s = constring + strlen(constring);
		sprintf(s, ";UID=%s", login);
	}
	if (pw) {
		s = constring + strlen(constring);
		sprintf(s, ";PWD=%s", pw);
	}

	stmt_text = constring;
	debugStatement();
	rc = SQLDriverConnect(hdbc, NULL,
			      (uchar *) constring, SQL_NTS,
			      outstring, sizeof(outstring), &waste,
			      SQL_DRIVER_NOPROMPT);
	if (errorTrap(0))
		return;
	sql_database = db;
	exclist = 0;

	/* Set the persistent connect/statement options.
	 * Note that some of these merely reassert the default,
	 * but it's good documentation to spell it out here. */
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
	rc = SQLSetConnectOption(hdbc, SQL_CURSOR_TYPE,
				 SQL_CURSOR_FORWARD_ONLY);
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
	SQLGetInfo(hdbc, SQL_CURSOR_ROLLBACK_BEHAVIOR, &cursors_under_rollback,
		   4, &waste);
	getdata_opts = 0;
	SQLGetInfo(hdbc, SQL_GETDATA_EXTENSIONS, &getdata_opts, 4, &waste);
	bookmarkBits = false;
	SQLGetInfo(hdbc, SQL_BOOKMARK_PERSISTENCE, &bookmarkBits, 4, &waste);

	exclist = 0;

/* Time to find out what the driver is, so we can have driver specific tweaks. */
	SQLGetInfo(hdbc, SQL_DRIVER_NAME, drivername, sizeof(drivername),
		   &waste);
	current_driver = DRIVER_NONE;
	if (stringEqual(drivername, "libsqliteodbc.so") ||
	    stringEqual(drivername, "sqlite3odbc.so"))
		current_driver = DRIVER_SQLITE;
	if (stringEqual(drivername, "libmyodbc.so"))
		current_driver = DRIVER_MYSQL;
	if (stringEqual(drivername, "libodbcpsql.so"))
		current_driver = DRIVER_POSTGRESQL;
	if (stringEqual(drivername, "iclis09b.so"))
		current_driver = DRIVER_INFORMIX;
	if (stringEqual(drivername, "libtdsodbc.so")) {
		current_driver = DRIVER_TDS;
		openfirst = true;
	}

	if (sql_debug) {
		if (current_driver)
			appendFile(sql_debuglog, "driver is %d",
				   current_driver);
		else
			appendFile(sql_debuglog, "driver string is %s",
				   drivername);
	}

	exclist = 0;
}				/* sql_connect */

/* make sure we're connected to a database */
static void checkConnect(void)
{
	if (!sql_database)
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
void sql_begTrans(void)
{
	checkConnect();
	stmt_text = 0;
	hstmt = SQL_NULL_HSTMT;
	rv_lastStatus = 0;	/* might never call errorTrap(0) */

	/* count the nesting level of transactions. */
	if (!translevel) {
		badtrans = false;
		stmt_text = "begin work";
		debugStatement();
		rc = SQLSetConnectOption(hdbc, SQL_AUTOCOMMIT,
					 SQL_AUTOCOMMIT_OFF);
		if (errorTrap(0))
			return;
	}

	++translevel;
	exclist = 0;
}				/* sql_begTrans */

/* end a transaction */
static void endTrans(bool commit)
{
	checkConnect();
	stmt_text = 0;
	hstmt = SQL_NULL_HSTMT;
	rv_lastStatus = 0;	/* might never call errorTrap(0) */

	if (!translevel)
		errorPrint("2end transaction without a matching begTrans()");
	--translevel;

	if (commit) {
		if (badtrans)
			errorPrint
			    ("2Cannot commit a transaction around an aborted transaction");
		if (!translevel) {
			stmt_text = "commit work";
			debugStatement();
			rc = SQLTransact(SQL_NULL_HENV, hdbc, SQL_COMMIT);
			if (rc)
				++translevel;
			errorTrap(0);
		}
	} else {		/* success or failure */
		badtrans = true;
		if (!translevel) {	/* bottom level */
			stmt_text = "rollback work";
			debugStatement();
			rc = SQLTransact(SQL_NULL_HENV, hdbc, SQL_ROLLBACK);
			if (rc)
				++translevel;
			errorTrap(0);
			badtrans = false;
		}
	}			/* success or failure */

	if (!translevel) {
		struct OCURS *o;
		short i, newstate;

		/* change the state of all cursors, if necessary */
		newstate = CURSOR_OPENED;
		if (commit) {
			if (cursors_under_commit == SQL_CB_DELETE)
				newstate = CURSOR_NONE;
			if (cursors_under_commit == SQL_CB_CLOSE)
				newstate = CURSOR_PREPARED;
		} else {
			if (cursors_under_rollback == SQL_CB_DELETE)
				newstate = CURSOR_NONE;
			if (cursors_under_rollback == SQL_CB_CLOSE)
				newstate = CURSOR_PREPARED;
		}

		for (i = 0; i < NUMCURSORS; ++i) {
			o = ocurs + i;
			if (o->flag <= newstate)
				continue;
			o->flag = newstate;
			if (newstate > CURSOR_NONE)
				continue;
			if (o->hstmt == SQL_NULL_HSTMT)
				continue;
			SQLFreeHandle(SQL_HANDLE_STMT, o->hstmt);
			o->hstmt = SQL_NULL_HSTMT;
		}

		/* back to singleton transactions */
		rc = SQLSetConnectOption(hdbc, SQL_AUTOCOMMIT,
					 SQL_AUTOCOMMIT_ON);
		errorTrap(0);
	}

	exclist = 0;
}				/* endTrans */

void sql_commitWork(void)
{
	endTrans(true);
}

void sql_rollbackWork(void)
{
	endTrans(false);
}

void sql_deferConstraints(void)
{
	if (!translevel)
		errorPrint
		    ("2Cannot defer constraints unless inside a transaction");
	stmt_text = "defere constraints";
	debugStatement();
	/* is there a way to do this through ODBC? */
	newStatement();
	rc = SQLExecDirect(hstmt, (uchar *) stmt_text, SQL_NTS);
	errorTrap(0);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
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
	if (isnullstring(tabname))
		errorPrint("2blobInsert, null table name");
	if (isnullstring(colname))
		errorPrint("2blobInsert, null column name");
	if (rowid <= 0)
		errorPrint("2invalid rowid in blobInsert");
	if (length < 0)
		errorPrint("2invalid length in blobInsert");
	if (strlen(tabname) + strlen(colname) + 42 >= sizeof(blobcmd))
		errorPrint("@internal blobInsert command too long");

	isfile = true;
	if (isnullstring(filename)) {
		isfile = false;
		if (!offset)
			errorPrint
			    ("2blobInsert is given null filename and null buffer");
	} else {
		offset = blobbuf;
		fd = eopen(filename, O_RDONLY | O_BINARY, 0);
		length = fileSizeByHandle(fd);
		if (length == 0) {
			isfile = false;
			close(fd);
		}
	}

	/* set up the blob insert command, using one host variable */
	sprintf(blobcmd, "update %s set %s = %s where rowid = %d",
		tabname, colname, (length ? "?" : "NULL"), rowid);
	stmt_text = blobcmd;
	debugStatement();
	newStatement();
	rv_lastNrows = 0;

	output_length = length;
	rc = SQL_SUCCESS;
	if (isfile) {
		output_length = SQL_LEN_DATA_AT_EXEC(length);
		rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
				      SQL_C_BINARY, SQL_LONGVARCHAR, length, 0,
				      blobcmd, length, &output_length);
		if (rc)
			close(fd);
	} else if (length) {
		rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
				      SQL_C_BINARY, SQL_LONGVARCHAR, length, 0,
				      offset, length, &output_length);
	}
	if (errorTrap(0)) {
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
		return;
	}

	rc = SQLExecDirect(hstmt, (uchar *) blobcmd, SQL_NTS);
	SQLRowCount(hstmt, &rv_lastNrows);

	if (isfile) {
		if (rc != SQL_NEED_DATA) {
			close(fd);
			if (rc == SQL_SUCCESS)
				errorPrint
				    ("@blobInsert expected SQL_NEED_DATA");
			errorTrap(0);
			SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
			return;
		}

		output_length = 0;
		rc = SQLParamData(hstmt, (void **)&output_length);
		if ((char *)output_length != blobcmd) {
			close(fd);
			errorPrint("2blobInsert got bad key from SQLParamData");
		}

		lseek(fd, 0L, 0);
		while (length) {
			int n = length;
			if (n > sizeof(blobbuf))
				n = sizeof(blobbuf);
			if (read(fd, blobbuf, n) != n) {
				close(fd);
				errorPrint("2cannot read file %s, errno %d",
					   filename, errno);
			}
			length -= n;

			rc = SQLPutData(hstmt, blobbuf, n);
			if (rc) {
				close(fd);
				errorTrap(0);
				SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
				return;
			}
		}		/* loop reading the file */

		close(fd);

		/* since there are no more exec-time parameters,
		 * this call completes the execution of the SQL statement. */
		rc = SQLParamData(hstmt, (void **)&output_length);
	}

	if (errorTrap(0)) {
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
		return;
	}

	if (sql_debug)
		appendFile(sql_debuglog, "%d rows affected", rv_lastNrows);
	if (sql_debug2)
		printf("%ld rows affected\n", rv_lastNrows);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
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

/* Temp area to read the values as strings */
static char retstring[NUMRETS][STRINGLEN + 4];
static bool everything_null;

static void retsFromOdbc(void)
{
	void *q, *q1;
	int i, l;
	int fd, flags;
	bool yearfirst, indata = false;
	long dt;		/* temporarily hold date or time */
	char *s;
	short c_type;		/* C data type */
	long input_length, output_length;
	char tbuf[20];		/* temp buf, for dates and times */
	double fmoney;		/* float version of money */
	int blobcount = 0;
	bool fbc = fetchBlobColumns;

	/* no blobs unless proven otherwise */
	rv_blobLoc = 0;
	rv_blobSize = nullint;

	if (!rv_numRets)
		errorPrint("@calling retsFromOdbc() with no returns pending");

	stmt_text = "retsFromOdbc";
	debugStatement();

/* count the blobs */
	if (fbc)
		for (i = 0; i < rv_numRets; ++i)
			if (rv_type[i] == 'B' || rv_type[i] == 'T')
				++blobcount;
	if (blobcount > 1) {
		i_puts(MSG_DBManyBlobs);
		fbc = false;
	}

	for (i = 0; i < rv_numRets; ++i) {
		if (!indata) {
			q = va_arg(sqlargs, void *);
			if (!q) {
				if (i)
					break;
				indata = true;
			}
		}
		if (indata) {
			if (rv_type[i] == 'S') {
				q = retstring[i];
				rv_data[i].ptr = q;
			} else
				q = rv_data + i;
		}
		if ((int)q < 1000 && (int)q > -1000)
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
			input_length = sizeof(blobbuf);
			q1 = blobbuf;
			*(long *)q = nullint;
			break;

		default:
			errorPrint("@retsFromOdbc, rv_type[%d] = %c", i,
				   rv_type[i]);
		}		/* switch */

		if (everything_null || (c_type == SQL_C_BINARY && !fbc)) {
			rc = SQL_SUCCESS;
			output_length = SQL_NULL_DATA;
		} else {
			rc = SQLGetData(hstmt, (ushort) (i + 1),
					c_type, q1, input_length,
					&output_length);
			/* we'll deal with blob overflow later */
			if (rc == SQL_SUCCESS_WITH_INFO
			    && c_type == SQL_C_BINARY
			    && output_length > sizeof(blobbuf))
				rc = SQL_SUCCESS;
			if (errorTrap(0))
				break;
			if (output_length == SQL_NO_TOTAL)
				errorPrint
				    ("@retsFromOdbc cannot get size of data for column %d",
				     i + 1);
		}

		/* Postprocess the return values. */
		/* For instance, turn string dates into our own 4-byte format. */
		s = tbuf;
		trimWhite(s);
		switch (rv_type[i]) {
		case 'C':
			*(char *)q = tbuf[0];
			break;

		case 'S':
			trimWhite(q);
			break;

		case 'D':
			yearfirst = false;
			if (s[4] == '-')
				yearfirst = true;
			dt = stringDate(s, yearfirst);
			if (dt < 0)
				errorPrint("@database holds invalid date %s",
					   s);
			*(long *)q = dt;
			break;

		case 'I':
			/* thanks to stringTime(), this works
			   for either hh:mm or hh:mm:ss */
			if (s[0] == 0)
				*(long *)q = nullint;
			else {
				/* Note that Informix introduces a leading space,
				   how about ODBC? */
				leftClipString(s);
				if (s[1] == ':')
					shiftRight(s, '0');
				dt = stringTime(s);
				if (dt < 0)
					errorPrint
					    ("@database holds invalid time %s",
					     s);
				*(long *)q = dt;
			}
			break;

		case 'M':
			if (fmoney == nullfloat)
				dt = nullint;
			else
				dt = fmoney * 100.0 + 0.5;
			*(long *)q = dt;
			break;

		case 'B':
		case 'T':
			if (output_length == SQL_NULL_DATA)
				break;
			/* note, 0 length blob is treated as a null blob */
			if (output_length == 0)
				break;
			/* the size of the blob is returned, in an int. */
			*(long *)q = output_length;
			rv_blobSize = output_length;

			if (isnullstring(rv_blobFile)) {
				/* the blob is always allocated; you have to free it! */
				/* SQL doesn't null terminate its text blobs, but we do. */
				rv_blobLoc = allocMem(output_length + 1);
				l = output_length;
				if (l > sizeof(blobbuf))
					l = sizeof(blobbuf);
				memcpy(rv_blobLoc, blobbuf, l);
				if (l < output_length) {	/* more to do */
					long waste;
					rc = SQLGetData(hstmt, (ushort) (i + 1),
							c_type,
							(char *)rv_blobLoc + l,
							output_length - l,
							&waste);
					if (rc) {
						nzFree(rv_blobLoc);
						rv_blobLoc = 0;
						*(long *)q = nullint;
						errorTrap(0);
						goto breakloop;
					}	/* error getting rest of blob */
				}	/* blob is larger than the buffer */
				if (rv_type[i] == 'T')	/* null terminate */
					((char *)rv_blobLoc)[output_length] = 0;
				break;
			}

			/* blob in memory */
			/* at this point the blob is being dumped into a file */
			flags = O_WRONLY | O_BINARY | O_CREAT | O_TRUNC;
			if (rv_blobAppend)
				flags =
				    O_WRONLY | O_BINARY | O_CREAT | O_APPEND;
			fd = eopen(rv_blobFile, flags, MODE_rw);
			rc = SQL_SUCCESS;
			while (true) {
				int outbytes;
				l = output_length;
				if (l > sizeof(blobbuf))
					l = sizeof(blobbuf);
				outbytes = write(fd, blobbuf, l);
				if (outbytes < l) {
					close(fd);
					errorPrint
					    ("2cannot write to file %s, errno %d",
					     rv_blobFile, errno);
				}
				if (l == output_length)
					break;

				/* get the next chunk from ODBC */
				rc = SQLGetData(hstmt, (ushort) (i + 1),
						c_type, q1, input_length,
						&output_length);
				if (rc == SQL_SUCCESS_WITH_INFO
				    && output_length > input_length)
					rc = SQL_SUCCESS;	/* data truncation error */
			}

			close(fd);
			errorTrap(0);
			break;

		}		/* switch */

	}			/* loop over returned elements */
breakloop:

	va_end(sqlargs);
	exclist = 0;
}				/* retsFromOdbc */

/* make sure we got one return value, and it is integer compatible */
/* The dots make the compiler happy. */
/* You must call this with args of 0,0 */
static long oneRetValue(void *pre_x, ...)
{
	char coltype = rv_type[0];
	char c;
	long n;
	double f;
	void **x = (void **)((char *)&pre_x + 4);

	va_end(sqlargs);
	if (rv_numRets != 1)
		errorPrint
		    ("2SQL statement has %d return values, 1 value expected",
		     rv_numRets);
	if (!strchr("MNFDICS", coltype))
		errorPrint
		    ("2SQL statement returns a value whose type is not compatible with a 4-byte integer");

	va_start(sqlargs, pre_x);
/* I'm not sure float to int really works. */
	if (coltype == 'F') {
		*x = &f;
		retsFromOdbc();
		n = f;
	} else if (coltype == 'S') {
		*x = retstring[0];
		retsFromOdbc();
		if (!stringIsNum(retstring[0]))
			errorPrint
			    ("2SQL statement returns a string %s that cannot be converted into a 4-byte integer",
			     retstring[0]);
		n = atoi(retstring[0]);
	} else if (coltype == 'C') {
		*x = &c;
		retsFromOdbc();
		n = c;
	} else {
		*x = &n;
		retsFromOdbc();
	}

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	return n;
}				/* oneRetValue */

/*********************************************************************
Prepare a formatted SQL statement.
Gather the types and names of the fetched columns and make this information
available to the rest of the C routines in this file, and to the application.
Returns false if the prepare failed.
*********************************************************************/

static bool prepareInternal(const char *stmt)
{
	short i, nc, coltype, colscale, nullable, namelen;
	unsigned long colprec;
	bool blobpresent = false;

	checkConnect();
	everything_null = true;
	if (isnullstring(stmt))
		errorPrint("2null SQL statement");
	stmt_text = stmt;
	debugStatement();

	/* look for delete with no where clause */
	skipWhite(&stmt);
	if (!strncmp(stmt, "delete", 6) || !strncmp(stmt, "update", 6))
		/* delete or update */
		if (!strstr(stmt, "where") && !strstr(stmt, "WHERE")) {
			showStatement();
			setError(MSG_DBNoWhere);
			return false;
		}

	rv_numRets = 0;
	memset(rv_type, 0, NUMRETS);
	memset(rv_nullable, 0, NUMRETS);
	rv_lastNrows = 0;

	if (openfirst)
		rc = SQLExecDirect(hstmt, (uchar *) stmt, SQL_NTS);
	else
		rc = SQLPrepare(hstmt, (uchar *) stmt, SQL_NTS);
	if (errorTrap(0))
		return false;

	/* gather column headings and types */
	rc = SQLNumResultCols(hstmt, &nc);
	if (errorTrap(0))
		return false;

	if (nc > NUMRETS) {
		showStatement();
		errorPrint("2cannot select more than %d values", NUMRETS);
	}

	for (i = 0; i < nc; ++i) {
		rc = SQLDescribeCol(hstmt, (USHORT) (i + 1),
				    (uchar *) rv_name[i], COLNAMELEN, &namelen,
				    &coltype, &colprec, &colscale, &nullable);
		if (errorTrap("01004"))
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

		if (current_driver == DRIVER_INFORMIX) {
			if (coltype == SQL_VARCHAR && colprec == 24
			    && colscale == 0)
				coltype = SQL_TIME;
#if 0
			if (coltype == SQL_DECIMAL
			    && (colprec != 15 || colscale != 0))
				coltype = SQL_MONEY;
#endif
		}

		if (current_driver == DRIVER_SQLITE) {
/* Every column looks like a text blob, but it is really a string. */
			coltype = SQL_CHAR;
			colprec = STRINGLEN;
		}

		rv_nullable[i] = (nullable != SQL_NO_NULLS);

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
/* I don't know what to do with these; just make them strings. */
			rv_type[i] = 'S';
			break;

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
			if (colprec == 1)
				rv_type[i] = 'C';
			if (colprec > STRINGLEN && sql_debug) {
				appendFile(sql_debuglog,
					   "column %s exceeds %d characters",
					   rv_name[i], STRINGLEN);
			}
			break;

		case SQL_LONGVARCHAR:
		case SQL_BINARY:
		case SQL_VARBINARY:
		case SQL_LONGVARBINARY:
			if (blobpresent) {
				showStatement();
				errorPrint
				    ("2Statement selects more than one blob column");
			}
			blobpresent = true;
			rv_type[i] = (coltype == SQL_LONGVARCHAR ? 'T' : 'B');
			break;

		case SQL_MONEY:
			rv_type[i] = 'M';
			break;

		default:
			errorPrint("@Unknown sql datatype %d", coltype);
		}		/* switch on type */
	}			/* loop over returns */

	rv_numRets = nc;
	return true;
}				/* prepareInternal */

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

static bool execInternal(const char *stmt, int mode)
{
	bool notfound = false;

	newStatement();
	if (!prepareInternal(stmt))
		return false;	/* error */

	if (!rv_numRets) {
		if (!(mode & 1)) {
			showStatement();
			errorPrint("2SQL select statement returns no values");
		}
		notfound = true;
	} else {		/* end no return values */
		if (!(mode & 2)) {
			showStatement();
			errorPrint("2SQL statement returns %d values",
				   rv_numRets);
		}
	}

	if (!openfirst)
		rc = SQLExecute(hstmt);

	if (!rc) {		/* statement succeeded */
		/* fetch the data, or get a count of the rows affected */
		if (rv_numRets) {
			stmt_text = "fetch";
			debugStatement();
			rc = SQLFetchScroll(hstmt, (ushort) SQL_FD_FETCH_NEXT,
					    1);
			if (rc == SQL_NO_DATA) {
				rc = SQL_SUCCESS;
				notfound = true;
			} else
				everything_null = false;
		} else {
			rc = SQLRowCount(hstmt, &rv_lastNrows);
			if (sql_debug)
				appendFile(sql_debuglog, "%d rows affected",
					   rv_lastNrows);
			if (sql_debug2)
				printf("%ld rows affected\n", rv_lastNrows);
		}
	}

	if (errorTrap(0))
		return false;

	if (!rv_numRets)
		return true;
	return !notfound;
}				/* execInternal */

/*********************************************************************
Run individual select or execute statements, using the above internal routine.
*********************************************************************/

/* execute a stand-alone statement with no % formatting of the string */
bool sql_execNF(const char *stmt)
{
	bool ok = execInternal(stmt, 1);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	exclist = 0;
	return ok;
}				/* sql_execNF */

/* execute a stand-alone statement with % formatting */
bool sql_exec(const char *stmt, ...)
{
	bool ok;
	va_start(sqlargs, stmt);
	stmt = lineFormatStack(stmt, 0, &sqlargs);
	ok = execInternal(stmt, 1);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	exclist = 0;
	va_end(sqlargs);
	return ok;
}				/* sql_exec */

/* run a select statement with % formatting */
/* return true if the row was found */
bool sql_select(const char *stmt, ...)
{
	bool rowfound;
	va_start(sqlargs, stmt);
	stmt = lineFormatStack(stmt, 0, &sqlargs);
	rowfound = execInternal(stmt, 2);
	retsFromOdbc();
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	return rowfound;
}				/* sql_select */

/* run a select statement with no % formatting of the string */
bool sql_selectNF(const char *stmt, ...)
{
	bool rowfound;
	va_start(sqlargs, stmt);
	rowfound = execInternal(stmt, 2);
	retsFromOdbc();
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	return rowfound;
}				/* sql_selectNF */

/* run a select statement with one return value */
int sql_selectOne(const char *stmt, ...)
{
	bool rowfound;
	va_start(sqlargs, stmt);
	stmt = lineFormatStack(stmt, 0, &sqlargs);
	rowfound = execInternal(stmt, 2);
	if (!rowfound) {
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
		exclist = 0;
		va_end(sqlargs);
		return nullint;
	}
	return oneRetValue(0, 0);
}				/* sql_selectOne */

/* run a stored procedure with no % formatting */
static bool sql_procGo(const char *stmt)
{
	bool rowfound;
	char *s = allocMem(20 + strlen(stmt));
	strcpy(s, "execute procedure ");
	strcat(s, stmt);
	rowfound = execInternal(s, 3);
	/* if execInternal doesn't return, we have a memory leak */
	nzFree(s);
	return rowfound;
}				/* sql_procGo */

/* run a stored procedure */
bool sql_proc(const char *stmt, ...)
{
	bool rowfound;
	va_start(sqlargs, stmt);
	stmt = lineFormatStack(stmt, 0, &sqlargs);
	rowfound = sql_procGo(stmt);
	retsFromOdbc();
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	return rowfound;
}				/* sql_proc */

/* run a stored procedure with one return */
int sql_procOne(const char *stmt, ...)
{
	bool rowfound;
	va_start(sqlargs, stmt);
	stmt = lineFormatStack(stmt, 0, &sqlargs);
	rowfound = sql_procGo(stmt);
	if (!rowfound) {
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
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
static int prepareCursor(const char *stmt, bool scrollflag)
{
	struct OCURS *o = findNewCursor();
	stmt = lineFormatStack(stmt, 0, &sqlargs);
	va_end(sqlargs);

	hstmt = o->hstmt;
	if (sql_debug)
		appendFile(sql_debuglog, "new cursor %d", o->cid);
	if (sql_debug2)
		printf("new cursor %d\n", o->cid);
	rc = SQLSetStmtOption(hstmt, SQL_CURSOR_TYPE,
			      scrollflag ? SQL_CURSOR_STATIC :
			      SQL_CURSOR_FORWARD_ONLY);
	if (errorTrap(0))
		return -1;

	if (!prepareInternal(stmt))
		return -1;
	o->numrets = rv_numRets;
	memcpy(o->rv_type, rv_type, NUMRETS);
	o->flag = (openfirst ? CURSOR_OPENED : CURSOR_PREPARED);
	o->rownum = 0;
	return o->cid;
}				/* prepareCursor */

int sql_prepare(const char *stmt, ...)
{
	int n;
	va_start(sqlargs, stmt);
	n = prepareCursor(stmt, false);
	exclist = 0;
	if (n < 0)
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	return n;
}				/* sql_prepare */

int sql_prepareScrolling(const char *stmt, ...)
{
	int n;
	va_start(sqlargs, stmt);
	n = prepareCursor(stmt, true);
	exclist = 0;
	if (n < 0)
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	return n;
}				/* sql_prepareScrolling */

void sql_open(int cid)
{
	struct OCURS *o = findCursor(cid);
	if (o->flag == CURSOR_OPENED)
		return;		/* already open */
	if (!o->numrets)
		errorPrint("2cursor is being opened with no returns");

	stmt_text = "open";
	debugStatement();
	hstmt = o->hstmt;
	rc = SQLExecute(hstmt);
	if (errorTrap(0))
		return;
	o->flag = CURSOR_OPENED;
	o->rownum = 0;
	exclist = 0;
}				/* sql_open */

int sql_prepOpen(const char *stmt, ...)
{
	int n;
	struct OCURS *o;

	va_start(sqlargs, stmt);
	n = prepareCursor(stmt, false);
	if (n < 0) {
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
		return n;
	}

	if (openfirst)
		goto done;

	o = findCursor(n);
	sql_open(n);
	if (rv_lastStatus) {
		o->flag = CURSOR_NONE;	/* back to square 0 */
		SQLFreeHandle(SQL_HANDLE_STMT, o->hstmt);
		o->hstmt = SQL_NULL_HSTMT;
		rv_numRets = 0;
		memset(rv_type, 0, sizeof(rv_type));
		n = -1;
	}

done:
	exclist = 0;
	return n;
}				/* sql_prepOpen */

void sql_close(int cid)
{
	struct OCURS *o = findCursor(cid);
	if (o->flag < CURSOR_OPENED)
		errorPrint("2cannot close cursor %d, not yet opened", cid);

	stmt_text = "close";
	debugStatement();
	hstmt = o->hstmt;
	rc = SQLCloseCursor(hstmt);
	if (errorTrap(0))
		return;
	o->flag = CURSOR_PREPARED;
	exclist = 0;
}				/* sql_close */

void sql_free(int cid)
{
	struct OCURS *o = findCursor(cid);
	if (o->flag == CURSOR_OPENED)
		sql_close(cid);

	stmt_text = "free";
	debugStatement();
	hstmt = o->hstmt;
	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	o->flag = CURSOR_NONE;
	o->hstmt = SQL_NULL_HSTMT;
	rv_numRets = 0;
	memset(rv_type, 0, sizeof(rv_type));
/* free should never fail */
	errorTrap(0);
	exclist = 0;
}				/* sql_free */

void sql_closeFree(int cid)
{
	const short *exc = exclist;
	sql_close(cid);
	if (!rv_lastStatus) {
		exclist = exc;
		sql_free(cid);
	}
}				/* sql_closeFree */

/* fetch row n from the open cursor.
 * Flag can be used to fetch first, last, next, or previous. */
static bool fetchInternal(int cid, long n, int flag)
{
	long nextrow, lastrow;
	struct OCURS *o = findCursor(cid);

	everything_null = true;

	/* don't do the fetch if we're looking for row 0 absolute,
	 * that just nulls out the return values */
	if (flag == SQL_FD_FETCH_ABSOLUTE && !n) {
		o->rownum = 0;
fetchZero:
		return false;
	}

	lastrow = nextrow = o->rownum;
	if (flag == SQL_FD_FETCH_ABSOLUTE)
		nextrow = n;
	if (flag == SQL_FD_FETCH_FIRST)
		nextrow = 1;
	if (isnotnull(lastrow)) {	/* we haven't lost track yet */
		if (flag == SQL_FD_FETCH_NEXT)
			++nextrow;
		if (flag == SQL_FD_FETCH_PREV && nextrow)
			--nextrow;
	}
	if (flag == SQL_FD_FETCH_LAST) {
		nextrow = nullint;	/* we just lost track */
	}

	if (!nextrow)
		goto fetchZero;

	if (o->flag != CURSOR_OPENED)
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
	if (flag == SQL_FD_FETCH_ABSOLUTE) {
		if (isnull(nextrow))
			errorPrint
			    ("2sql fetches absolute row using null index");
		if (isnotnull(lastrow) && nextrow == lastrow + 1)
			flag = SQL_FD_FETCH_NEXT;
	}

	stmt_text = "fetch";
	debugStatement();
	hstmt = o->hstmt;
	rc = SQLFetchScroll(hstmt, (ushort) flag, nextrow);
	if (rc == SQL_NO_DATA)
		return false;
	if (errorTrap(0))
		return false;
	o->rownum = nextrow;
	everything_null = false;
	return true;
}				/* fetchInternal */

bool sql_fetchFirst(int cid, ...)
{
	bool rowfound;
	va_start(sqlargs, cid);
	rowfound = fetchInternal(cid, 0L, SQL_FD_FETCH_FIRST);
	retsFromOdbc();
	return rowfound;
}				/* sql_fetchFirst */

bool sql_fetchLast(int cid, ...)
{
	bool rowfound;
	va_start(sqlargs, cid);
	rowfound = fetchInternal(cid, 0L, SQL_FD_FETCH_LAST);
	retsFromOdbc();
	return rowfound;
}				/* sql_fetchLast */

bool sql_fetchNext(int cid, ...)
{
	bool rowfound;
	va_start(sqlargs, cid);
	rowfound = fetchInternal(cid, 0L, SQL_FD_FETCH_NEXT);
	retsFromOdbc();
	return rowfound;
}				/* sql_fetchNext */

bool sql_fetchPrev(int cid, ...)
{
	bool rowfound;
	va_start(sqlargs, cid);
	rowfound = fetchInternal(cid, 0L, SQL_FD_FETCH_PREV);
	retsFromOdbc();
	return rowfound;
}				/* sql_fetchPrev */

bool sql_fetchAbs(int cid, long rownum, ...)
{
	bool rowfound;
	va_start(sqlargs, rownum);
	rowfound = fetchInternal(cid, rownum, SQL_FD_FETCH_ABSOLUTE);
	retsFromOdbc();
	return rowfound;
}				/* sql_fetchAbs */

void getPrimaryKey(char *tname, int *part1, int *part2, int *part3, int *part4)
{
	char colname[COLNAMELEN];
	SQLLEN pcbValue;
	char *dot;

	*part1 = *part2 = *part3 = *part4 = 0;
	newStatement();
	stmt_text = "get primary key";
	debugStatement();
	dot = strchr(tname, '.');
	if (dot)
		*dot++ = 0;

	rc = SQLPrimaryKeys(hstmt,
			    NULL, SQL_NTS,
			    (uchar *) (dot ? tname : NULL), SQL_NTS,
			    (uchar *) (dot ? dot : tname), SQL_NTS);
	if (dot)
		dot[-1] = '.';
	if (rc)
		goto abort;

/* bind column 4, the name of the key column */
	rc = SQLBindCol(hstmt, 4, SQL_CHAR, (SQLPOINTER) & colname,
			sizeof(colname), &pcbValue);
	if (rc)
		goto abort;

/* I'm only grabbing the first 4 columns in a multi-column key */
	rc = SQLFetch(hstmt);
	if (rc == SQL_NO_DATA)
		goto done;
	if (rc)
		goto abort;
	*part1 = findColByName(colname) + 1;

	rc = SQLFetch(hstmt);
	if (rc == SQL_NO_DATA)
		goto done;
	if (rc)
		goto abort;
	*part2 = findColByName(colname) + 1;

	rc = SQLFetch(hstmt);
	if (rc == SQL_NO_DATA)
		goto done;
	if (rc)
		goto abort;
	*part3 = findColByName(colname) + 1;

	rc = SQLFetch(hstmt);
	if (rc == SQL_NO_DATA)
		goto done;
	if (rc)
		goto abort;
	*part4 = findColByName(colname) + 1;

	goto done;

abort:
	errorTrap(0);
done:
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	return;
}				/* getPrimaryKey */

bool showTables(void)
{
	char tabname[40];
	char tabtype[40];
	char tabowner[40];
	SQLLEN tabnameOut, tabtypeOut, tabownerOut;
	char *buf;
	int buflen, cx;

/*
	int truevalue = SQL_TRUE;
SQLSetConnectAttr(hdbc, SQL_ATTR_METADATA_ID,
&truevalue, SQL_IS_INTEGER);
*/

	newStatement();
	stmt_text = "get tables";
	debugStatement();
	rc = SQLTables(hstmt,
		       NULL, SQL_NTS, NULL, SQL_NTS, NULL, SQL_NTS, NULL,
		       SQL_NTS);
	if (rc)
		goto abort;

	SQLBindCol(hstmt, 2, SQL_CHAR, (SQLPOINTER) tabowner, sizeof(tabowner),
		   &tabownerOut);
	SQLBindCol(hstmt, 3, SQL_CHAR, (SQLPOINTER) tabname, sizeof(tabname),
		   &tabnameOut);
	SQLBindCol(hstmt, 4, SQL_CHAR, (SQLPOINTER) tabtype, sizeof(tabtype),
		   &tabtypeOut);

	buf = initString(&buflen);
	while (SQLFetch(hstmt) == SQL_SUCCESS) {
		char tabline[140];
		sprintf(tabline, "%s.%s|%s\n", tabowner, tabname, tabtype);
		stringAndString(&buf, &buflen, tabline);
	}

	cx = sideBuffer(0, buf, buflen, 0);
	nzFree(buf);
	i_printf(MSG_ShowTables, cx);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	return true;

abort:
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	return false;
}				/* showTables */

/* display foreign keys, from this table to others */
bool fetchForeign(char *tname)
{
	char farschema[40], fartab[40];
	char farcol[40];
	char nearcol[40];
	SQLLEN nearcolOut, farschemaOut, fartabOut, farcolOut;
	char *dot;

	newStatement();
	stmt_text = "foreign keys";
	debugStatement();

	dot = strchr(tname, '.');
	if (dot)
		*dot++ = 0;

	rc = SQLForeignKeys(hstmt,
			    NULL, SQL_NTS, NULL, SQL_NTS, NULL, SQL_NTS,
			    NULL, SQL_NTS,
			    (uchar *) (dot ? tname : NULL), SQL_NTS,
			    (uchar *) (dot ? dot : tname), SQL_NTS);
	if (dot)
		dot[-1] = '.';
	if (rc)
		goto abort;

	SQLBindCol(hstmt, 2, SQL_CHAR, (SQLPOINTER) farschema,
		   sizeof(farschema), &farschemaOut);
	SQLBindCol(hstmt, 3, SQL_CHAR, (SQLPOINTER) fartab, sizeof(fartab),
		   &fartabOut);
	SQLBindCol(hstmt, 4, SQL_CHAR, (SQLPOINTER) farcol, sizeof(farcol),
		   &farcolOut);
	SQLBindCol(hstmt, 8, SQL_CHAR, (SQLPOINTER) nearcol, sizeof(nearcol),
		   &nearcolOut);

	while (SQLFetch(hstmt) == SQL_SUCCESS) {
		printf("%s > ", nearcol);
		if (farschema[0])
			printf("%s.", farschema);
		printf("%s.%s\n", fartab, farcol);
	}

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	return true;

abort:
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	return false;
}				/* fetchForeign */
