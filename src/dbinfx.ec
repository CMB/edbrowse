/*********************************************************************
dbinfx.ec: C-level interface to SQL.
This is a layer above esql/c,
since embedded SQL is often difficult to use, especially for new programmers.
Most SQL queries are relatively simple, whence the esql API is overkill.
Why mess with cryptic $directives when you can write:
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

Note that dbapi.h does NOT include the Informix header files.
That would violate the spirit of this layer,
which attempts to sheild the application from the details of the SQL API.
If the application needed to see anything in the Informix header files,
we would be doing something wrong.
*********************************************************************/

/* bring in the necessary Informix headers */
$include sqlca;
$include sqltypes;
$include sqlda;
$include locator;

#include "eb.h"
#include "dbapi.h"

#define CACHELIMIT 10000 /* number of cached lines */

#define ENGINE_ERRCODE sqlca.sqlcode


/*********************************************************************
The status variable ENGINE_ERRCODE holds the return code from an Informix call.
This is then used by the function errorTrap() below.
If ENGINE_ERRCODE != 0, errorTrap() aborts the program, or performs
a recovery longjmp, as directed by the generic error function errorPrint().
errorTrap() returns true if an SQL error occurred, but that error
was trapped by the application.
In this case the calling routine should clean up as best it can and return.
*********************************************************************/

static const char *stmt_text = 0; /* text of the SQL statement */
static const short *exclist; /* list of error codes trapped by the application */
static short translevel;
static eb_bool badtrans;

/* Through globals, make error info available to the application. */
int rv_lastStatus, rv_stmtOffset;
long rv_vendorStatus;
char *rv_badToken;

static void debugStatement(void)
{
	if(sql_debug && stmt_text)
		appendFileNF(sql_debuglog, stmt_text);
} /* debugStatement */

static void debugExtra(const char *s)
{
	if(sql_debug)
		appendFileNF(sql_debuglog, s);
} /* debugExtra */

/* Append the SQL statement to the debug log.  This is not strictly necessary
 * if sql_debug is set, since the statement has already been appended. */
static void showStatement(void)
{
	if(!sql_debug && stmt_text)
		appendFileNF(sql_debuglog, stmt_text);
} /* showStatement */

/* application sets the exception list */
void sql_exclist(const short *list) { exclist = list; }

void sql_exception(int errnum)
{
	static short list[2];
	list[0] = errnum;
	exclist = list;
} /* sql_exception */

/* map Informix errors to our own exception codes, as defined in dbapi.h. */
static const struct ERRORMAP {
	short infcode;
	short excno;
} errormap[] = {
	{200, EXCSYNTAX},
	{201, EXCSYNTAX},
	{202, EXCSYNTAX},
	{203, EXCSYNTAX},
	{204, EXCSYNTAX},
	{205, EXCROWIDUSE},
	{206, EXCNOTABLE},
	/* 207 */
	{208, EXCRESOURCE},
	{209, EXCDBCORRUPT},
	{210, EXCFILENAME},
	{211, EXCDBCORRUPT},
	{212, EXCRESOURCE},
	{213, EXCINTERRUPT},
	{214, EXCDBCORRUPT},
	{215, EXCDBCORRUPT},
	{216, EXCDBCORRUPT},
	{217, EXCNOCOLUMN},
	{218, EXCNOSYNONYM},
	{219, EXCCONVERT},
	{220, EXCSYNTAX},
	{221, EXCRESOURCE},
	{222, EXCRESOURCE},
	{223, EXCAMBTABLE},
	{224, EXCRESOURCE},
	{225, EXCRESOURCE},
	{226, EXCRESOURCE},
	{227, EXCROWIDUSE},
	{228, EXCROWIDUSE},
	{229, EXCRESOURCE},
	{230, EXCDBCORRUPT},
	{231, EXCAGGREGATEUSE},
	{232, EXCSERIAL},
	{233, EXCITEMLOCK},
	{234, EXCAMBCOLUMN},
	{235, EXCCONVERT},
	{236, EXCSYNTAX},
	{237, EXCMANAGETRANS},
	{238, EXCMANAGETRANS},
	{239, EXCDUPKEY},
	{240, EXCDBCORRUPT},
	{241, EXCMANAGETRANS},
	{249, EXCAMBCOLUMN},
	{250, EXCDBCORRUPT},
	{251, EXCSYNTAX},
	{252, EXCITEMLOCK},
	{253, EXCSYNTAX},
	{255, EXCNOTINTRANS},
	{256, EXCMANAGETRANS},
	{257, EXCRESOURCE},
	{258, EXCDBCORRUPT},
	{259, EXCNOCURSOR},
	{260, EXCNOCURSOR},
	{261, EXCRESOURCE},
	{262, EXCNOCURSOR},
	{263, EXCRESOURCE},
	{264, EXCRESOURCE},
	{265, EXCNOTINTRANS},
	{266, EXCNOCURSOR},
	{267, EXCNOCURSOR},
	{268, EXCDUPKEY},
	{269, EXCNOTNULLCOLUMN},
	{270, EXCDBCORRUPT},
	{271, EXCDBCORRUPT},
	{272, EXCPERMISSION},
	{273, EXCPERMISSION},
	{274, EXCPERMISSION},
	{275, EXCPERMISSION},
	{276, EXCNOCURSOR},
	{277, EXCNOCURSOR},
	{278, EXCRESOURCE},
	{281, EXCTEMPTABLEUSE},
	{282, EXCSYNTAX},
	{283, EXCSYNTAX},
	{284, EXCMANYROW},
	{285, EXCNOCURSOR},
	{286, EXCNOTNULLCOLUMN},
	{287, EXCSERIAL},
	{288, EXCITEMLOCK},
	{289, EXCITEMLOCK},
	{290, EXCNOCURSOR},
	{292, EXCNOTNULLCOLUMN},
	{293, EXCSYNTAX},
	{294, EXCAGGREGATEUSE},
	{295, EXCCROSSDB},
	{296, EXCNOTABLE},
	{297, EXCNOKEY},
	{298, EXCPERMISSION},
	{299, EXCPERMISSION},
	{300, EXCRESOURCE},
	{301, EXCRESOURCE},
	{302, EXCPERMISSION},
	{ 303, EXCAGGREGATEUSE},
	{304, EXCAGGREGATEUSE},
	{305, EXCSUBSCRIPT},
	{306, EXCSUBSCRIPT},
	{307, EXCSUBSCRIPT},
	{308, EXCCONVERT},
	{309, EXCAMBCOLUMN},
	{310, EXCDUPTABLE},
	{311, EXCDBCORRUPT},
	{312, EXCDBCORRUPT},
	{313, EXCPERMISSION},
	{314, EXCDUPTABLE},
	{315, EXCPERMISSION},
	{316, EXCDUPINDEX},
	{317, EXCUNION},
	{318, EXCFILENAME},
	{319, EXCNOINDEX},
	{320, EXCPERMISSION},
	{321, EXCAGGREGATEUSE},
	{323, EXCTEMPTABLEUSE},
	{324, EXCAMBCOLUMN},
	{325, EXCFILENAME},
	{326, EXCRESOURCE},
	{327, EXCITEMLOCK},
	{328, EXCDUPCOLUMN},
	{329, EXCNOCONNECT},
	{330, EXCRESOURCE},
	{331, EXCDBCORRUPT},
	{332, EXCTRACE},
	{333, EXCTRACE},
	{334, EXCTRACE},
	{335, EXCTRACE},
	{336, EXCTEMPTABLEUSE},
	{337, EXCTEMPTABLEUSE},
	{338, EXCTRACE},
	{339, EXCFILENAME},
	{340, EXCTRACE},
	{341, EXCTRACE},
	{342, EXCREMOTE},
	{343, EXCTRACE},
	{344, EXCTRACE},
	{345, EXCTRACE},
	{346, EXCDBCORRUPT},
	{347, EXCITEMLOCK},
	{348, EXCDBCORRUPT},
	{349, EXCNODB},
	{350, EXCDUPINDEX},
	{352, EXCNOCOLUMN},
	{353, EXCNOTABLE},
	{354, EXCSYNTAX},
	{355, EXCDBCORRUPT},
	{356, EXCCONVERT},
	{361, EXCRESOURCE},
	{362, EXCSERIAL},
	{363, EXCNOCURSOR},
	{365, EXCNOCURSOR},
	{366, EXCCONVERT},
	{367, EXCAGGREGATEUSE},
	{368, EXCDBCORRUPT},
	{369, EXCSERIAL},
	{370, EXCAMBCOLUMN},
	{371, EXCDUPKEY},
	{372, EXCTRACE},
	{373, EXCFILENAME},
	{374, EXCSYNTAX},
	{375, EXCMANAGETRANS},
	{376, EXCMANAGETRANS},
	{377, EXCMANAGETRANS},
	{378, EXCITEMLOCK},
	{382, EXCSYNTAX},
	{383, EXCAGGREGATEUSE},
	{384, EXCVIEWUSE},
	{385, EXCCONVERT},
	{386, EXCNOTNULLCOLUMN},
	{387, EXCPERMISSION},
	{388, EXCPERMISSION},
	{389, EXCPERMISSION},
	{390, EXCDUPSYNONYM},
	{391, EXCNOTNULLCOLUMN},
	{392, EXCDBCORRUPT},
	{393, EXCWHERECLAUSE},
	{394, EXCNOTABLE},
	{395, EXCWHERECLAUSE},
	{396, EXCWHERECLAUSE},
	{397, EXCDBCORRUPT},
	{398, EXCNOTINTRANS},
	{399, EXCMANAGETRANS},
	{400, EXCNOCURSOR},
	{401, EXCNOCURSOR},
	{404, EXCNOCURSOR},
	{406, EXCRESOURCE},
	{407, EXCDBCORRUPT},
	{408, EXCDBCORRUPT},
	{409, EXCNOCONNECT},
	{410, EXCNOCURSOR},
	{413, EXCNOCURSOR},
	{414, EXCNOCURSOR},
	{415, EXCCONVERT},
	{417, EXCNOCURSOR},
	{420, EXCREMOTE},
	{421, EXCREMOTE},
	{422, EXCNOCURSOR},
	{423, EXCNOROW},
	{424, EXCDUPCURSOR},
	{425, EXCITEMLOCK},
	{430, EXCCONVERT},
	{431, EXCCONVERT},
	{432, EXCCONVERT},
	{433, EXCCONVERT},
	{434, EXCCONVERT},
	{439, EXCREMOTE},
	{451, EXCRESOURCE},
	{452, EXCRESOURCE},
	{453, EXCDBCORRUPT},
	{454, EXCDBCORRUPT},
	{455, EXCRESOURCE},
	{457, EXCREMOTE},
	{458, EXCLONGTRANS},
	{459, EXCREMOTE},
	{460, EXCRESOURCE},
	{465, EXCRESOURCE},
	{468, EXCNOCONNECT},
	{472, EXCCONVERT},
	{473, EXCCONVERT},
	{474, EXCCONVERT},
	{482, EXCNOCURSOR},
	{484, EXCFILENAME},
	{500, EXCDUPINDEX},
	{501, EXCDUPINDEX},
	{502, EXCNOINDEX},
	{503, EXCRESOURCE},
	{504, EXCVIEWUSE},
	{505, EXCSYNTAX},
	{506, EXCPERMISSION},
	{507, EXCNOCURSOR},
	{508, EXCTEMPTABLEUSE},
	{509, EXCTEMPTABLEUSE},
	{510, EXCTEMPTABLEUSE},
	{512, EXCPERMISSION},
	{514, EXCPERMISSION},
	{515, EXCNOCONSTRAINT},
	{517, EXCRESOURCE},
	{518, EXCNOCONSTRAINT},
	{519, EXCCONVERT},
	{521, EXCITEMLOCK},
	{522, EXCNOTABLE},
	{524, EXCNOTINTRANS},
	{525, EXCREFINT},
	{526, EXCNOCURSOR},
	{528, EXCRESOURCE},
	{529, EXCNOCONNECT},
	{530, EXCCHECK},
	{531, EXCDUPCOLUMN},
	{532, EXCTEMPTABLEUSE},
	{534, EXCITEMLOCK},
	{535, EXCMANAGETRANS},
	{536, EXCSYNTAX},
	{537, EXCNOCONSTRAINT},
	{538, EXCDUPCURSOR},
	{539, EXCRESOURCE},
	{540, EXCDBCORRUPT},
	{541, EXCPERMISSION},
	{543, EXCAMBCOLUMN},
	{543, EXCSYNTAX},
	{544, EXCAGGREGATEUSE},
	{545, EXCPERMISSION},
	{548, EXCTEMPTABLEUSE},
	{549, EXCNOCOLUMN},
	{550, EXCRESOURCE},
	{551, EXCRESOURCE},
	{554, EXCSYNTAX},
	{559, EXCDUPSYNONYM},
	{560, EXCDBCORRUPT},
	{561, EXCAGGREGATEUSE},
	{562, EXCCONVERT},
	{536, EXCITEMLOCK},
	{564, EXCRESOURCE},
	{565, EXCRESOURCE},
	{566, EXCRESOURCE},
	{567, EXCRESOURCE},
	{568, EXCCROSSDB},
	{569, EXCCROSSDB},
	{570, EXCCROSSDB},
	{571, EXCCROSSDB},
	{573, EXCMANAGETRANS},
	{574, EXCAMBCOLUMN},
	{576, EXCTEMPTABLEUSE},
	{577, EXCDUPCONSTRAINT},
	{578, EXCSYNTAX},
	{579, EXCPERMISSION},
	{580, EXCPERMISSION},
	{582, EXCMANAGETRANS},
	{583, EXCPERMISSION},
	{586, EXCDUPCURSOR},
	{589, EXCREMOTE},
	{590, EXCDBCORRUPT},
	{591, EXCCONVERT},
	{592, EXCNOTNULLCOLUMN},
	{593, EXCSERIAL},
	{594, EXCBLOBUSE},
	{595, EXCAGGREGATEUSE},
	{597, EXCDBCORRUPT},
	{598, EXCNOCURSOR},
	{599, EXCSYNTAX},
	{600, EXCMANAGEBLOB},
	{601, EXCMANAGEBLOB},
	{602, EXCMANAGEBLOB},
	{603, EXCMANAGEBLOB},
	{604, EXCMANAGEBLOB},
	{605, EXCMANAGEBLOB},
	{606, EXCMANAGEBLOB},
	{607, EXCSUBSCRIPT},
	{608, EXCCONVERT},
	{610, EXCBLOBUSE},
	{611, EXCBLOBUSE},
	{612, EXCBLOBUSE},
	{613, EXCBLOBUSE},
	{614, EXCBLOBUSE},
	{615, EXCBLOBUSE},
	{616, EXCBLOBUSE},
	{617, EXCBLOBUSE},
	{618, EXCMANAGEBLOB},
	{622, EXCNOINDEX},
	{623, EXCNOCONSTRAINT},
	{625, EXCDUPCONSTRAINT},
	{628, EXCMANAGETRANS},
	{629, EXCMANAGETRANS},
	{630, EXCMANAGETRANS},
	{631, EXCBLOBUSE},
	{635, EXCPERMISSION},
	{636, EXCRESOURCE},
	{638, EXCBLOBUSE},
	{639, EXCBLOBUSE},
	{640, EXCDBCORRUPT},
	{649, EXCFILENAME},
	{650, EXCRESOURCE},
	{651, EXCRESOURCE},
	/* I'm not about to map all possible compile/runtime SPL errors. */
	/* Here's a few. */
	{655, EXCSYNTAX},
	{667, EXCSYNTAX},
	{673, EXCDUPSPROC},
	{674, EXCNOSPROC},
	{678, EXCSUBSCRIPT},
	{681, EXCDUPCOLUMN},
	{686, EXCMANYROW},
	{690, EXCREFINT},
	{691, EXCREFINT},
	{692, EXCREFINT},
	{702, EXCITEMLOCK},
	{703, EXCNOTNULLCOLUMN},
	{704, EXCDUPCONSTRAINT},
	{706, EXCPERMISSION},
	{707, EXCBLOBUSE},
	{722, EXCRESOURCE},
	{958, EXCDUPTABLE},
	{1214, EXCCONVERT},
	{1262, EXCCONVERT},
	{1264, EXCCONVERT},
	{25553, EXCNOCONNECT},
	{25587, EXCNOCONNECT},
	{25588, EXCNOCONNECT},
	{25596, EXCNOCONNECT},
	{0, 0}
}; /* ends of list */

static int errTranslate(int code)
{
	const struct ERRORMAP *e;

	for(e=errormap; e->infcode; ++e) {
		if(e->infcode == code)
			return e->excno;
	}
	return EXCSQLMISC;
} /* errTranslate */

static eb_bool errorTrap(void)
{
short i;

/* innocent until proven guilty */
rv_lastStatus = 0;
rv_vendorStatus = 0;
rv_stmtOffset = 0;
rv_badToken = 0;
if(ENGINE_ERRCODE >= 0) return eb_false; /* no problem */

        /* log the SQL statement that elicitted the error */
showStatement();
rv_vendorStatus = -ENGINE_ERRCODE;
rv_lastStatus = errTranslate(rv_vendorStatus);
rv_stmtOffset = sqlca.sqlerrd[4];
rv_badToken = sqlca.sqlerrm;
if(!rv_badToken[0]) rv_badToken = 0;

/* if the application didn't trap for this exception, blow up! */
if(exclist)
for(i=0; exclist[i]; ++i)
if(exclist[i] == rv_lastStatus) {
exclist = 0; /* we've spent that exception */
return eb_true;
}

/* Remember, errorPrint() should not return. */
errorPrint("2SQL error %d, %s", rv_vendorStatus, sqlErrorList[rv_lastStatus]);
return eb_true; /* make the compiler happy */
} /* errorTrap */


/*********************************************************************
The OCURS structure given below maintains an open SQL cursor.
A static array of these structures allows multiple cursors
to be opened simultaneously.
*********************************************************************/

static struct OCURS {
char sname[8]; /* statement name */
char cname[8]; /* cursor name */
struct sqlda *desc;
char rv_type[NUMRETS];
long rownum;
short cid; /* cursor ID */
char flag;
char numRets;
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
for(o=ocurs, i=0; i<NUMCURSORS; ++i, ++o) {
if(o->flag != CURSOR_NONE) continue;
sprintf(o->cname, "c%u", i);
sprintf(o->sname, "s%u", i);
o->cid = 6000+i;
return o;
}
errorPrint("2more than %d cursors opend concurrently", NUMCURSORS);
return 0; /* make the compiler happy */
} /* findNewCursor */

/* dereference an existing cursor */
static struct OCURS *findCursor(int cid)
{
struct OCURS *o;
if(cid < 6000 || cid >= 6000+NUMCURSORS)
errorPrint("2cursor number %d is out of range", cid);
cid -= 6000;
o = ocurs+cid;
if(o->flag == CURSOR_NONE)
errorPrint("2cursor %d is not currently active", cid);
rv_numRets = o->numRets;
memcpy(rv_type, o->rv_type, NUMRETS);
return o;
} /* findCursor */

/* This doesn't close/free anything; it simply puts variables in an initial state. */
/* part of the disconnect() procedure */
static void clearAllCursors(void)
{
	int i, j;
	struct OCURS *o;

	for(i=0, o=ocurs; i<NUMCURSORS; ++i, ++o) {
		if(o->flag == CURSOR_NONE) continue;
		o->flag = CURSOR_NONE;
o->rownum = 0;
	} /* loop over cursors */

	translevel = 0;
	badtrans = eb_false;
} /* clearAllCursors */


/*********************************************************************
Connect and disconect to SQL databases.
*********************************************************************/

void sql_connect(const char *db, const char *login, const char *pw) 
{
$char *dblocal = (char*)db;
login = pw = 0; /* not used here, so make the compiler happy */
if(isnullstring(dblocal)) {
dblocal = getenv("DBNAME");
if(isnullstring(dblocal))
errorPrint("2sql_connect receives no database, check $DBNAME");
}

if(sql_database) {
	stmt_text = "disconnect";
	debugStatement();
$disconnect current;
clearAllCursors();
sql_database = 0;
}

	stmt_text = "connect";
	debugStatement();
$connect to :dblocal;
if(errorTrap()) return;
sql_database = dblocal;

/* set default lock mode and isolation level for transaction management */
stmt_text = "lock isolation";
debugStatement();
$ set lock mode to wait;
if(errorTrap()) {
abort:
sql_disconnect();
return;
}
$ set isolation to committed read;
if(errorTrap()) goto abort;
exclist = 0;
} /* sql_connect */

void sql_disconnect(void)
{
if(sql_database) {
	stmt_text = "disconnect";
	debugStatement();
$disconnect current;
clearAllCursors();
sql_database = 0;
}
exclist = 0;
} /* sql_disconnect */

/* make sure we're connected to a database */
static void checkConnect(void)
{
	if(!sql_database)
		errorPrint("2SQL command issued, but no database selected");
} /* checkConnect */


/*********************************************************************
Begin, commit, and abort transactions.
SQL does not permit nested transactions; this API does, to a limited degree.
An inner transaction cannot fail while an outer one succeeds;
that would require SQL support which is not forthcoming.
However, as long as all transactions succeed, or the outer most fails,
everything works properly.
The static variable transLevel holds the number of nested transactions.
*********************************************************************/

/* begin a transaction */
void sql_begTrans(void)
{
	rv_lastStatus = 0;
	checkConnect();
stmt_text = 0;

	/* count the nesting level of transactions. */
	if(!translevel) {
		badtrans = eb_false;
		stmt_text = "begin work";
		debugStatement();
		$begin work;
		if(errorTrap()) return;
	}
	++translevel;
	exclist = 0;
} /* sql_begTrans */

/* end a transaction */
static void endTrans(eb_bool commit)
{
	rv_lastStatus = 0;
	checkConnect();
stmt_text = 0;

	if(translevel == 0)
		errorPrint("2end transaction without a matching begTrans()");
	--translevel;

	if(commit) {
			stmt_text = "commit work";
			debugStatement();
		if(badtrans)
			errorPrint("2Cannot commit a transaction around an aborted transaction");
		if(translevel == 0) {
			$commit work;
			if(ENGINE_ERRCODE) ++translevel;
			errorTrap();
		}
	} else { /* success or failure */
			stmt_text = "rollback work";
			debugStatement();
		badtrans = eb_true;
		if(!translevel) { /* bottom level */
			$rollback work;
			if(ENGINE_ERRCODE) --translevel;
			errorTrap();
			badtrans = eb_false;
		}
	} /* success or failure */

	/* At this point I will make a bold assumption --
	 * that all cursors are declared with hold.
	 * Hence they remain valid after the transaction is closed,
	 * and we don't have to change any of the OCURS structures. */

	exclist = 0;
} /* endTrans */

void sql_commitWork(void) { endTrans(eb_true); }
void sql_rollbackWork(void) { endTrans(eb_false); }

void sql_deferConstraints(void)
{
	if(!translevel)
		errorPrint("2Cannot defer constraints unless inside a transaction");
	stmt_text = "defer constraints";
	debugStatement();
	$set constraints all deferred;
	errorTrap();
	exclist = 0;
} /* sql_deferConstraints */


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

static loc_t blobstruct; /* Informix structure to manage the blob */

/* insert a blob into the database */
void sql_blobInsert(const char *tabname, const char *colname, int rowid,
const char *filename, void *offset, int length)
{
$char blobcmd[100];
$loc_t insblob;

/* basic sanity checks */
checkConnect();
if(isnullstring(tabname)) errorPrint("2blobInsert, missing table name");
if(isnullstring(colname)) errorPrint("2blobInsert, missing column name");
if(rowid <= 0) errorPrint("2invalid rowid in blobInsert");
if(length < 0) errorPrint("2invalid length in blobInsert");
if(strlen(tabname) + strlen(colname) + 42 >= sizeof(blobcmd))
errorPrint("2internal blobInsert command too long");

/* set up the blob structure */
memset(&insblob, 0, sizeof(insblob));
if(!filename) {
insblob.loc_loctype = LOCMEMORY;
if(offset) {
if(length == 0) offset = 0;
}
if(!offset) length = -1;
insblob.loc_buffer = offset;
insblob.loc_bufsize = length;
insblob.loc_size = length;
if(!offset) insblob.loc_indicator = -1;
} else {
insblob.loc_loctype = LOCFNAME;
insblob.loc_fname = (char*)filename;
insblob.loc_oflags = LOC_RONLY;
insblob.loc_size = -1;
}

/* set up the blob insert command, using one host variable */
sprintf(blobcmd, "update %s set %s = ? where rowid = %d",
tabname, colname, rowid);
stmt_text = blobcmd;
debugStatement();
$prepare blobinsert from :blobcmd;
if(errorTrap()) return;
$execute blobinsert using :insblob;
errorTrap();
rv_lastNrows = sqlca.sqlerrd[2];
rv_lastRowid = sqlca.sqlerrd[5];
if(sql_debug) appendFile(sql_debuglog, "%d rows affected", rv_lastNrows);
exclist = 0;
} /* sql_blobInsert */


/*********************************************************************
When an SQL statement is prepared, the engine tells us the types and lengths
of the columns.  Use this information to "normalize" the sqlda
structure, so that columns are fetched using our preferred formats.
For instance, smallints and ints both map into int variables,
varchars become chars, dates map into strings (so that we can convert
them into our own vendor-independent binary representations later), etc.
We assume the number and types of returns have been established.
Once retsSetup has "normalized" the sqlda structure,
run the select or fetch, and then call retsCleanup to post-process the data.
This will, for example, turn dates, fetched into strings,
into our own 4-byte representations.
The same for time intervals, money, etc.
*********************************************************************/

/* Temp area to read the Informix values, as strings */
static char retstring[NUMRETS][STRINGLEN+4];
static va_list sqlargs;

static void retsSetup(struct sqlda *desc)
{
short i;
eb_bool blobpresent = eb_false;
struct sqlvar_struct   *v;

for(i=0; (unsigned)i< NUMRETS; ++i) {
rv_data[i].l = nullint;
retstring[i][0] = 0;
rv_name[i][0] = 0;
}
if(!desc) return;

  for(i=0,v=desc->sqlvar; i<rv_numRets; ++i,++v ) {
strncpy(rv_name[i], v->sqlname, COLNAMELEN);
switch(rv_type[i]) {
case 'S':
case 'C':
case 'D':
case 'I':
v->sqltype = CCHARTYPE;
v->sqllen = STRINGLEN+2;
v->sqldata = retstring[i];
rv_data[i].ptr = retstring[i];
break;

case 'N':
v->sqltype = CINTTYPE;
v->sqllen = 4;
v->sqldata =  (char *) &rv_data[i].l;
break;

case 'F':
case 'M':
v->sqltype = CDOUBLETYPE;
v->sqllen = 8;
v->sqldata = (char*) &rv_data[i].f;
rv_data[i].f = nullfloat;
break;

case 'B':
case 'T':
if(blobpresent)
errorPrint("2Cannot select more than one blob at a time");
blobpresent = eb_true;
v->sqltype = CLOCATORTYPE;
v->sqllen = sizeof(blobstruct);
v->sqldata = (char*) &blobstruct;
memset(&blobstruct, 0, sizeof(blobstruct));
if(!rv_blobFile) {
blobstruct.loc_loctype = LOCMEMORY;
blobstruct.loc_mflags = LOC_ALLOC;
blobstruct.loc_bufsize = -1;
} else {
blobstruct.loc_loctype = LOCFNAME;
blobstruct.loc_fname = (char*)rv_blobFile;
blobstruct.lc_union.lc_file.lc_mode = 0600;
blobstruct.loc_oflags =
(rv_blobAppend ? LOC_WONLY|LOC_APPEND : LOC_WONLY);
}
break;

default:
errorPrint("@bad character %c in retsSetup", rv_type[i]);
} /* switch */
} /* loop over fetched columns */
} /* retsSetup */

/* clean up fetched values, eg. convert date to our proprietary format. */
static void retsCleanup(void)
{
short i, l;
eb_bool yearfirst;

/* no blobs unless proven otherwise */
rv_blobLoc = 0;
rv_blobSize = nullint;

for(i=0; i<rv_numRets; ++i) {
clipString(retstring[i]);
switch(rv_type[i]) {
case 'D':
yearfirst = eb_false;
if(retstring[i][4] == '-') yearfirst = eb_true;
rv_data[i].l = stringDate(retstring[i],yearfirst);
break;

case 'I':
/* thanks to stringTime(), this works for either hh:mm or hh:mm:ss */
if(retstring[i][0] == 0) rv_data[i].l = nullint;
else {
/* convert space to 0 */
if(retstring[i][1] == ' ') retstring[i][1] = '0';
/* skip the leading space that is produced when Informix converts interval to string */
rv_data[i].l = stringTime(retstring[i]+1);
}
break;

case 'C':
rv_data[i].l = retstring[i][0];
break;

case 'M':
case 'F':
/* null floats look different from null dates and ints. */
if(rv_data[i].l == 0xffffffff) {
rv_data[i].f = nullfloat;
if(rv_type[i] == 'M') rv_data[i].l = nullint;
break;
}
/* represent monitary amounts as an integer number of pennies. */
if(rv_type[i] == 'M')
rv_data[i].l = rv_data[i].f * 100.0 + 0.5;
break;

case 'S':
/* map the empty string into the null string */
l = strlen(retstring[i]);
if(!l) rv_data[i].ptr = 0;
if(l > STRINGLEN) errorPrint("2fetched string is too long, limit %d chars", STRINGLEN);
break;

case 'B':
case 'T':
if(blobstruct.loc_indicator >= 0) { /* not null blob */
rv_blobSize = blobstruct.loc_size;
if(!rv_blobFile) rv_blobLoc = blobstruct.loc_buffer;
if(rv_blobSize == 0) { /* turn empty blob into null blob */
nzFree(rv_blobLoc);
rv_blobLoc = 0;
rv_blobSize = nullint;
}
}
rv_data[i].l = rv_blobSize;
break;

case 'N':
/* Convert from Informix null to our nullint */
if(rv_data[i].l == 0x80000000) rv_data[i].l = nullint;
break;

default:
errorPrint("@bad character %c in retsCleanup", rv_type[i]);
} /* switch on datatype */
} /* loop over columsn fetched */
} /* retsCleanup */

void retsCopy(eb_bool allstrings, void *first, ...)
{
void *q;
int i;

	if(!rv_numRets)
		errorPrint("@calling retsCopy() with no returns pending");

	for(i=0; i<rv_numRets; ++i) {
		if(first) {
			q = first;
			va_start(sqlargs, first);
			first = 0;
		} else {
			q = va_arg(sqlargs, void*);
		}
		if(!q) break;
		if((int)q < 1000 && (int)q > -1000)
			errorPrint("2retsCopy, pointer too close to 0");

if(allstrings) *(char*)q = 0;

if(rv_type[i] == 'S') {
*(char*)q = 0;
if(rv_data[i].ptr)
strcpy(q,  rv_data[i].ptr);
} else if(rv_type[i] == 'C') {
*(char *)q = rv_data[i].l;
if(allstrings) ((char*)q)[1] = 0;
} else if(rv_type[i] == 'F') {
if(allstrings) {
if(isnotnullfloat(rv_data[i].f)) sprintf(q, "%lf", rv_data[i].f);
} else {
*(double *)q = rv_data[i].f;
}
} else if(allstrings) {
char type = rv_type[i];
long l = rv_data[i].l;
if(isnotnull(l)) {
if(type == 'D') {
strcpy(q, dateString(l, DTDELIMIT));
} else if(type == 'I') {
strcpy(q, timeString(l, DTDELIMIT));
} else if(type == 'M') {
sprintf(q, "%ld.%02d", l/100, l%100);
} else sprintf(q, "%ld", l);
}
} else {
*(long *)q = rv_data[i].l;
}
} /* loop over result parameters */

if(!first) va_end(sqlargs);
} /* retsCopy */

/* make sure we got one return value, and it is integer compatible */
static long oneRetValue(void)
{
char coltype = rv_type[0];
long n = rv_data[0].l;
if(rv_numRets != 1)
errorPrint("2SQL statement has %d return values, 1 value expected", rv_numRets);
if(!strchr("MNFDIC", coltype))
errorPrint("2SQL statement returns a value whose type is not compatible with a 4-byte integer");
if(coltype == 'F') n = rv_data[0].f;
return n;
} /* oneRetValue */


/*********************************************************************
Prepare a formatted SQL statement.
Gather the types and names of the fetched columns and make this information
available to the rest of the C routines in this file, and to the application.
Returns the populated sqlda structure for the statement.
Returns null if the prepare failed.
*********************************************************************/

static struct sqlda *prepare(const char *stmt_parm, const char *sname_parm)
{
$char*stmt = (char*)stmt_parm;
$char*sname = (char*)sname_parm;
struct sqlda *desc;
struct sqlvar_struct   *v;
short i, coltype;

checkConnect();
if(isnullstring(stmt)) errorPrint("2null SQL statement");
stmt_text = stmt;
debugStatement();

/* look for delete with no where clause */
while(*stmt == ' ') ++stmt;
if(!strncmp(stmt, "delete", 6) || !strncmp(stmt, "update", 6))
/* delete or update */
if(!strstr(stmt, "where") && !strstr(stmt, "WHERE")) {
showStatement();
errorPrint("2Old Mcdonald bug");
}

/* set things up to nulls, in case the prepare fails */
retsSetup(0);
rv_numRets = 0;
memset(rv_type, 0, NUMRETS);
rv_lastNrows = rv_lastRowid = rv_lastSerial = 0;

$prepare :sname from :stmt;
if(errorTrap()) return 0;

/* gather types and column headings */
$describe: sname into desc;
if(!desc) errorPrint("2$describe couldn't allocate descriptor");
rv_numRets = desc->sqld;
if(rv_numRets > NUMRETS) {
showStatement();
errorPrint("2cannot select more than %d values", NUMRETS);
}

  for(i=0,v=desc->sqlvar; i<rv_numRets; ++i,++v ) {
coltype = v->sqltype & SQLTYPE;
/* kludge, count(*) should be int, not float, in my humble opinion */
if(stringEqual(v->sqlname, "(count(*))"))
coltype = SQLINT;

switch(coltype) {
case SQLCHAR:
case SQLVCHAR:
rv_type[i] = 'S';
if(v->sqllen == 1)
rv_type[i] = 'C';
break;

case SQLDTIME:
/* We only process datetime year to minute, for databases
 * other than Informix,  which don't have a date type. */
if(v->sqllen != 5) errorPrint("2datetime field must be year to minute");
case SQLDATE:
rv_type[i] = 'D';
break;

case SQLINTERVAL:
rv_type[i] = 'I';
break;

case SQLSMINT:
case SQLINT:
case SQLSERIAL:
case SQLNULL:
rv_type[i] = 'N';
break;

case SQLFLOAT:
case SQLSMFLOAT:
case SQLDECIMAL:
rv_type[i] = 'F';
break;

case SQLMONEY:
rv_type[i] = 'M';
break;

case SQLBYTES:
rv_type[i] = 'B';
break;

case SQLTEXT:
rv_type[i] = 'T';
break;

default:
errorPrint ("@Unknown informix sql datatype %d", coltype);
} /* switch on type */
} /* loop over returns */

retsSetup(desc);
return desc;
} /* prepare */


/*********************************************************************
Run an SQL statement internally, and gather any fetched values.
This statement stands alone; it fetches at most one row.
You might simply know this, perhaps because of a unique key,
or you might be running a stored procedure.
For efficiency we do not look for a second row, so this is really
like the "select first" construct that some databases support.
A mode variable says whether execution or selection or both are allowed.
Return true if data was successfully fetched.
*********************************************************************/

static eb_bool execInternal(const char *stmt, int mode)
{
struct sqlda *desc;
$static char singlestatement[] = "single_use_stmt";
$static char singlecursor[] = "single_use_cursor";
int i;
eb_bool notfound = eb_false;
short errorcode = 0;

desc = prepare(stmt, singlestatement);
if(!desc) return eb_false; /* error */

if(!rv_numRets) {
if(!(mode&1)) {
showStatement();
errorPrint("2SQL select statement returns no values");
}
$execute :singlestatement;
notfound = eb_true;
} else { /* end no return values */

if(!(mode&2)) {
showStatement();
errorPrint("2SQL statement returns %d values", rv_numRets);
}
$execute: singlestatement into descriptor desc;
}

if(errorTrap()) {
errorcode = rv_vendorStatus;
} else {
/* select or execute ran properly */
/* error 100 means not found in Informix */
if(ENGINE_ERRCODE == 100) notfound = eb_true;
/* set "last" parameters, in case the application is interested */
rv_lastNrows = sqlca.sqlerrd[2];
rv_lastRowid = sqlca.sqlerrd[5];
rv_lastSerial = sqlca.sqlerrd[1];
} /* successful run */

$free :singlestatement;
errorTrap();
nzFree(desc);

retsCleanup();

if(errorcode) {
rv_vendorStatus = errorcode;
rv_lastStatus = errTranslate(rv_vendorStatus);
return eb_false;
}

exclist = 0;
return !notfound;
} /* execInternal */


/*********************************************************************
Run individual select or execute statements, using the above internal routine.
*********************************************************************/

/* pointer to vararg list; most of these are vararg functions */
/* execute a stand-alone statement with no % formatting of the string */
eb_bool sql_execNF(const char *stmt)
{
	return execInternal(stmt, 1);
} /* sql_execNF */

/* execute a stand-alone statement with % formatting */
eb_bool sql_exec(const char *stmt, ...)
{
eb_bool ok;
	va_start(sqlargs, stmt);
	stmt = lineFormatStack(stmt, 0, &sqlargs);
	ok = execInternal(stmt, 1);
	va_end(sqlargs);
return ok;
} /* sql_exec */

/* run a select statement with no % formatting of the string */
/* return true if the row was found */
eb_bool sql_selectNF(const char *stmt, ...)
{
	eb_bool rc;
	va_start(sqlargs, stmt);
	rc = execInternal(stmt, 2);
	retsCopy(eb_false, 0);
	return rc;
} /* sql_selectNF */

/* run a select statement with % formatting */
eb_bool sql_select(const char *stmt, ...)
{
	eb_bool rc;
	va_start(sqlargs, stmt);
	stmt = lineFormatStack(stmt, 0, &sqlargs);
	rc = execInternal(stmt, 2);
	retsCopy(eb_false, 0);
	return rc;
} /* sql_select */

/* run a select statement with one return value */
int sql_selectOne(const char *stmt, ...)
{
	eb_bool rc;
	va_start(sqlargs, stmt);
	stmt = lineFormatStack(stmt, 0, &sqlargs);
	rc = execInternal(stmt, 2);
		if(!rc) { va_end(sqlargs); return nullint; }
	return oneRetValue();
} /* sql_selectOne */

/* run a stored procedure with no % formatting */
static eb_bool sql_procNF(const char *stmt)
{
	eb_bool rc;
	char *s = allocMem(20+strlen(stmt));
	strcpy(s, "execute procedure ");
	strcat(s, stmt);
	rc = execInternal(s, 3);
	/* if execInternal doesn't return, we have a memory leak */
	nzFree(s);
	return rc;
} /* sql_procNF */

/* run a stored procedure */
eb_bool sql_proc(const char *stmt, ...)
{
	eb_bool rc;
	va_start(sqlargs, stmt);
	stmt = lineFormatStack(stmt, 0, &sqlargs);
	rc = sql_procNF(stmt);
	if(rv_numRets) retsCopy(eb_false, 0);
	return rc;
} /* sql_proc */

/* run a stored procedure with one return */
int sql_procOne(const char *stmt, ...)
{
	eb_bool rc;
	va_start(sqlargs, stmt);
	stmt = lineFormatStack(stmt, 0, &sqlargs);
	rc = sql_procNF(stmt);
		if(!rc) { va_end(sqlargs); return 0; }
	return oneRetValue();
} /* sql_procOne */


/*********************************************************************
Prepare, open, close, and free SQL cursors.
*********************************************************************/

/* prepare a cursor; return the ID number of that cursor */
static int prepareCursor(const char *stmt, eb_bool scrollflag)
{
$char *internal_sname, *internal_cname;
struct OCURS *o = findNewCursor();

stmt = lineFormatStack(stmt, 0, &sqlargs);
va_end(sqlargs);
internal_sname = o->sname;
internal_cname = o->cname;
o->desc = prepare(stmt, internal_sname);
if(!o->desc) return -1;
if(o->desc->sqld == 0) {
showStatement();
errorPrint("2statement passed to sql_prepare has no returns");
}

/* declare with hold;
 * you might run transactions within this cursor. */
if(scrollflag)
$declare :internal_cname scroll cursor with hold for :internal_sname;
else
$declare :internal_cname cursor with hold for :internal_sname;
if(errorTrap()) {
nzFree(o->desc);
return -1;
}

o->numRets = rv_numRets;
memcpy(o->rv_type, rv_type, NUMRETS);
o->flag = CURSOR_PREPARED;
return o->cid;
} /* prepareCursor */

int sql_prepare(const char *stmt, ...)
{
	int n;
	va_start(sqlargs, stmt);
	n = prepareCursor(stmt, eb_false);
	exclist = 0;
	return n;
} /* sql_prepare */

int sql_prepareScrolling(const char *stmt, ...)
{
	int n;
	va_start(sqlargs, stmt);
	n = prepareCursor(stmt, eb_true);
	exclist = 0;
	return n;
} /* sql_prepareScrolling */

void sql_open(int cid)
{
short i;
$char *internal_sname, *internal_cname;
struct OCURS *o = findCursor(cid);
if(o->flag == CURSOR_OPENED)
errorPrint("2cannot open cursor %d, already opened", cid);
internal_sname = o->sname;
internal_cname = o->cname;
debugExtra("open");
$open :internal_cname;
if(!errorTrap()) o->flag = CURSOR_OPENED;
o->rownum = 0;
exclist = 0;
} /* sql_open */

int sql_prepOpen(const char *stmt, ...)
{
int n;
va_start(sqlargs, stmt);
n = prepareCursor(stmt, eb_false);
if(n < 0) return n;
sql_open(n);
if(rv_lastStatus) {
short ev = rv_vendorStatus;
short el = rv_lastStatus;
sql_free(n);
rv_vendorStatus = ev;
rv_lastStatus = el;
n = -1;
}
return n;
} /* sql_prepOpen */

void sql_close(int cid)
{
$char *internal_sname, *internal_cname;
struct OCURS *o = findCursor(cid);
if(o->flag < CURSOR_OPENED)
errorPrint("2cannot close cursor %d, not yet opened", cid);
internal_cname = o->cname;
debugExtra("close");
$close :internal_cname;
if(errorTrap()) return;
o->flag = CURSOR_PREPARED;
exclist = 0;
} /* sql_close */

void
sql_free( int cid)
{
$char *internal_sname, *internal_cname;
struct OCURS *o = findCursor(cid);
if(o->flag == CURSOR_OPENED)
errorPrint("2cannot free cursor %d, not yet closed", cid);
internal_sname = o->sname;
debugExtra("free");
$free :internal_sname;
if(errorTrap()) return;
o->flag = CURSOR_NONE;
nzFree(o->desc);
rv_numRets = 0;
memset(rv_name, 0, sizeof(rv_name));
memset(rv_type, 0, sizeof(rv_type));
exclist = 0;
} /* sql_free */

void sql_closeFree(int cid)
{
const short *exc = exclist;
sql_close(cid);
if(!rv_lastStatus) {
exclist = exc;
sql_free(cid);
}
} /* sql_closeFree */

/* fetch row n from the open cursor.
 * Flag can be used to fetch first, last, next, or previous. */
static eb_bool
fetchInternal(int cid, long n, int flag, eb_bool remember)
{
$char *internal_sname, *internal_cname;
$long nextrow, lastrow;
struct sqlda *internal_desc;
struct OCURS *o = findCursor(cid);

internal_cname = o->cname;
internal_desc = o->desc;
retsSetup(internal_desc);

/* don't do the fetch if we're looking for row 0 absolute,
 * that just nulls out the return values */
if(flag == 6 && !n) {
o->rownum = 0;
fetchZero:
retsCleanup();
exclist = 0;
return eb_false;
}

lastrow = nextrow = o->rownum;
if(flag == 6) nextrow = n;
if(flag == 3) nextrow = 1;
if(isnotnull(lastrow)) { /* we haven't lost track yet */
if(flag == 1) ++nextrow;
if(flag == 2 && nextrow) --nextrow;
}
if(flag == 4) { /* fetch the last row */
nextrow = nullint; /* we just lost track */
}

if(!nextrow) goto fetchZero;

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
if(flag == 6 &&
isnotnull(lastrow) && isnotnull(nextrow) &&
nextrow == lastrow+1)
flag=1;

debugExtra("fetch");

switch(flag) {
case 1:
$fetch :internal_cname using descriptor internal_desc;
break;
case 2:
$fetch previous :internal_cname using descriptor internal_desc;
break;
case 3:
$fetch first :internal_cname using descriptor internal_desc;
break;
case 4:
$fetch last :internal_cname using descriptor internal_desc;
break;
case 6:
if(isnull(nextrow))
errorPrint("2sql fetches absolute row using null index");
$fetch absolute :nextrow :internal_cname using descriptor internal_desc;
break;
default:
errorPrint("@fetchInternal() receives bad flag %d", flag);
} /* switch */
retsCleanup();

if(errorTrap()) return eb_false;
exclist = 0;
if(ENGINE_ERRCODE == 100) return eb_false; /* not found */
o->rownum = nextrow;

return eb_true;
} /* fetchInternal */

eb_bool sql_fetchFirst(int cid, ...)
{
	eb_bool rc;
	va_start(sqlargs, cid);
	rc = fetchInternal(cid, 0L, 3, eb_false);
	retsCopy(eb_false, 0);
	return rc;
} /* sql_fetchFirst */

eb_bool sql_fetchLast(int cid, ...)
{
	eb_bool rc;
	va_start(sqlargs, cid);
	rc = fetchInternal(cid, 0L, 4, eb_false);
	retsCopy(eb_false, 0);
	return rc;
} /* sql_fetchLast */

eb_bool sql_fetchNext(int cid, ...)
{
	eb_bool rc;
	va_start(sqlargs, cid);
	rc = fetchInternal(cid, 0L, 1, eb_false);
	retsCopy(eb_false, 0);
	return rc;
} /* sql_fetchNext */

eb_bool sql_fetchPrev(int cid, ...)
{
	eb_bool rc;
	va_start(sqlargs, cid);
	rc = fetchInternal(cid, 0L, 2, eb_false);
	retsCopy(eb_false, 0);
	return rc;
} /* sql_fetchPrev */

eb_bool sql_fetchAbs(int cid, long rownum, ...)
{
	eb_bool rc;
	va_start(sqlargs, rownum);
	rc = fetchInternal(cid, rownum, 6, eb_false);
	retsCopy(eb_false, 0);
	return rc;
} /* sql_fetchAbs */


/*********************************************************************
Get the primary key for a table.
In informix, you can use system tables to get this information.
I haven't yet expanded this to a 3 part key.
*********************************************************************/

/* The prototype looks right; but this function only returns the first 2 key columns */
void
getPrimaryKey(char *tname, int *part1, int *part2, int *part3, int *part4)
{
    int p1, p2, rc;
    char *s = strchr(tname, ':');
    *part1 = *part2 = *part3 = *part4 = 0;
    if(!s) {
	rc = sql_select("select part1, part2 \
from sysconstraints c, systables t, sysindexes i \
where tabname = %S and t.tabid = c.tabid \
and constrtype = 'P' and c.idxname = i.idxname", tname, &p1, &p2);
    } else {
	*s = 0;
	rc = sql_select("select part1, part2 \
from %s:sysconstraints c, %s:systables t, %s:sysindexes i \
where tabname = %S and t.tabid = c.tabid \
and constrtype = 'P' and c.idxname = i.idxname", tname, tname, tname, s + 1, &p1, &p2);
	*s = ':';
    }
    if(rc)
	*part1 = p1, *part2 = p2;
}				/* getPrimaryKey */

eb_bool
showTables(void)
{
puts("Not implemented in Informix, but certainly doable through systables");
}				/* showTables */

/* This can also be done by catalogs; it's on my list of things to do. */
eb_bool
fetchForeign(char *tname)
{
i_puts(MSG_NYI);
} /* fetchForeign */
