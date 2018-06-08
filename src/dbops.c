/* dbops.c
 * Database operations.
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"
#include "dbapi.h"

bool sqlPresent = true;

const char *sql_debuglog = "/tmp/ebsql.log";	/* log of debug prints */
const char *sql_database;	/* name of current database */
int rv_numRets;
char rv_type[NUMRETS + 1];
bool rv_nullable[NUMRETS];
/* names of returned data, usually SQL column names */
char rv_name[NUMRETS + 1][COLNAMELEN];
LF rv_data[NUMRETS];		/* the returned values */
long rv_lastNrows, rv_lastSerial, rv_lastRowid;
void *rv_blobLoc;		/* location of blob in memory */
int rv_blobSize;
const char *rv_blobFile;
bool rv_blobAppend;

/* text descriptions corresponding to our generic SQL error codes */
/* This has yet to be internationalized. */
const char *sqlErrorList[] = { 0,
	"miscellaneous SQL error",
	"syntax error in SQL statement",
	"filename cannot be used by SQL",
	"cannot convert/compare the columns/constants in the SQL statement",
	"bad string subscripting",
	"bad use of the rowid construct",
	"bad use of a blob column",
	"bad use of aggregate operators or columns",
	"bad use of a view",
	"bad use of a serial column",
	"bad use of a temp table",
	"operation cannot cross databases",
	"database is fucked up",
	"query interrupted by user",
	"could not connect to the database",
	"database has not yet been selected",
	"table not found",
	"duplicate table",
	"ambiguous table",
	"column not found",
	"duplicate column",
	"ambiguous column",
	"index not found",
	"duplicate index",
	"constraint not found",
	"duplicate constraint",
	"stored procedure not found",
	"duplicate stored procedure",
	"synonym not found",
	"duplicate synonym",
	"table has no primary or unique key",
	"duplicate primary or unique key",
	"cursor not specified, or cursor is not available",
	"duplicate cursor",
	"the database lacks the resources needed to complete this query",
	"check constrain violated",
	"referential integrity violated",
	"cannot manage or complete the transaction",
	"long transaction, too much log data generated",
	"this operation must be run inside a transaction",
	"cannot open, read, write, close, or otherwise manage a blob",
	"row, table, page, or database is already locked, or cannot be locked",
	"inserting null into a not null column",
	"no permission to modify the database in this way",
	"no current row established",
	"many rows were found where one was expected",
	"cannot union these select statements together",
	"cannot access or write the audit trail",
	"could not run SQL or gather data from a remote host",
	"where clause is semantically unmanageable",
	"deadlock detected",
	0
};

char *lineFormat(const char *line, ...)
{
	char *s;
	va_list p;
	va_start(p, line);
	s = lineFormatStack(line, 0, &p);
	va_end(p);
	return s;
}				/* lineFormat */

#define LFBUFSIZE 100000
static char lfbuf[LFBUFSIZE];	/* line formatting buffer */
static const char selfref[] =
    "@lineFormat attempts to expand within its own static buffer";
static const char lfoverflow[] = "@lineFormat(), line is too long, limit %d";

char *lineFormatStack(const char *line,	/* the sprintf-like formatting string */
		      LF * argv,	/* pointer to array of values */
		      va_list * parmv)
{
	short i, len, maxlen, len_given, flags;
	long n;
	double dn;		/* double number */
	char *q, *r, pdir, inquote;
	const char *t, *perc;
	char fmt[12];

	if ((parmv && argv) || (!parmv && !argv))
		errorPrint
		    ("@exactly one of the last two arguments to lineFormatStack should be null");

	if (line == lfbuf) {
		if (strchr(line, '%'))
			errorPrint(selfref);
		return (char *)line;
	}

	lfbuf[0] = 0;
	q = lfbuf;
	t = line;

	while (*t) {		/* more text to format */
/* copy up to the next % */
		if (*t != '%' || (t[1] == '%' && ++t)) {
			if (q - lfbuf >= LFBUFSIZE - 1)
				errorPrint(lfoverflow, LFBUFSIZE);
			*q++ = *t++;
			continue;
		}

/* % found */
		perc = t++;
		inquote = 0;
		len = 0;
		len_given = 0;

		if (*t == '-')
			++t;
		for (; isdigit(*t); ++t) {
			len_given = 1;
			len = 10 * len + *t - '0';
		}
		while (*t == '.' || isdigit(*t))
			++t;
		pdir = *t++;
		if (isupper(pdir)) {
			pdir = tolower(pdir);
			inquote = '"';
		}
		if (t - perc >= sizeof(fmt))
			errorPrint("2percent directive in lineFormat too long");
		strncpy(fmt, perc, t - perc);
		fmt[t - perc] = 0;
		maxlen = len;
		if (maxlen < 11)
			maxlen = 11;

/* get the next vararg */
		if (pdir == 'f') {
			if (parmv)
				dn = va_arg(*parmv, double);
			else
				dn = argv->f;
		} else {
			if (parmv)
				n = va_arg(*parmv, int);
			else
				n = argv->l;
		}
		if (argv)
			++argv;

		if (pdir == 's' && n) {
			i = strlen((char *)n);
			if (i > maxlen)
				maxlen = i;
			if (inquote && strchr((char *)n, inquote)) {
				inquote = '\'';
				if (strchr((char *)n, inquote))
					errorPrint
					    ("2lineFormat() cannot put quotes around %s",
					     n);
			}
		}
		if (inquote)
			maxlen += 2;
		if (q + maxlen >= lfbuf + LFBUFSIZE)
			errorPrint(lfoverflow, LFBUFSIZE);

/* check for null parameter */
		if ((pdir == 'c' && !n) ||
		    (pdir == 's' && isnullstring((char *)n)) ||
		    (pdir == 'f' && dn == nullfloat) ||
		    (!strchr("scf", pdir) && isnull(n))) {
			if (!len_given) {
				char *q1;
/* turn = %d to is null */
				for (q1 = q - 1; q1 >= lfbuf && *q1 == ' ';
				     --q1) ;
				if (q1 >= lfbuf && *q1 == '=') {
					if (q1 > lfbuf && q1[-1] == '!') {
						strcpy(q1 - 1, "IS NOT ");
						q = q1 + 6;
					} else {
						strcpy(q1, "IS ");
						q = q1 + 3;
					}
				}
				strcpy(q, "NULL");
				q += 4;
				continue;
			}	/* null with no length specified */
			pdir = 's';
			n = (int)"";
		}
		/* parameter is null */
		if (inquote)
			*q++ = inquote;
		fmt[t - perc - 1] = pdir;
		switch (pdir) {
		case 'i':
			flags = DTDELIMIT;
			if (len) {
				if (len >= 11)
					flags |= DTAMPM;
				if (len < 8)
					flags = DTDELIMIT | DTCRUNCH;
				if (len < 5)
					flags = DTCRUNCH;
			}
			strcpy(q, timeString(n, flags));
			break;
		case 'a':
			flags = DTDELIMIT;
			if (len) {
				if (len < 10)
					flags = DTCRUNCH | DTDELIMIT;
				if (len < 8)
					flags = DTCRUNCH;
				if (len == 5)
					flags = DTCRUNCH | DTDELIMIT;
			}
			strcpy(q, dateString(n, flags));
			if (len == 4 || len == 5)
				q[len] = 0;
			break;
		case 'm':
			strcpy(q, moneyString(n));
			break;
		case 'f':
			sprintf(q, fmt, dn);
/* show float as an integer, if it is an integer, and it usually is */
			r = strchr(q, '.');
			if (r) {
				while (*++r == '0') ;
				if (!*r) {
					r = strchr(q, '.');
					*r = 0;
				}
			}
			break;
		case 's':
			if (n == (int)lfbuf)
				errorPrint(selfref);
/* extra code to prevent %09s from printing out all zeros
when the argument is null (empty string) */
			if (!*(char *)n && fmt[1] == '0')
				strmove(fmt + 1, fmt + 2);
/* fall through */
		default:
			sprintf(q, fmt, n);
		}		/* switch */
		q += strlen(q);
		if (inquote)
			*q++ = inquote;
	}			/* loop printing pieces of the string */

	*q = 0;			/* null terminate */

/* we relie on the calling function to invoke va_end(), since the arg list
is not always the ... varargs of a function, though it usually is.
See lineFormat() above for a typical example.
Note that the calling function may wish to process additional arguments
before calling va_end. */

	return lfbuf;
}				/* lineFormatStack */

/* given a datatype, return the character that, when appended to %,
causes lineFormat() to print the data element properly. */
static char sprintfChar(char datatype)
{
	char c;
	switch (datatype) {
	case 'S':
		c = 's';
		break;
	case 'C':
		c = 'c';
		break;
	case 'M':
	case 'N':
		c = 'd';
		break;
	case 'D':
		c = 'a';
		break;
	case 'I':
		c = 'i';
		break;
	case 'F':
		c = 'f';
		break;
	case 'B':
	case 'T':
		c = 'd';
		break;
	default:
		c = 0;
	}			/* switch */
	return c;
}				/* sprintfChar */

/*********************************************************************
Using the values just fetched or selected, build a line in unload format.
All fields are expanded into ascii, with pipes between them.
Conversely, given a line of pipe separated fields,
put them back into binary, ready for retsCopy().
*********************************************************************/

char *sql_mkunld(char delim)
{
	char fmt[NUMRETS * 4 + 1];
	int i;
	char pftype;

	for (i = 0; i < rv_numRets; ++i) {
		pftype = sprintfChar(rv_type[i]);
		if (!pftype)
			errorPrint("2sql_mkunld cannot convert datatype %c",
				   rv_type[i]);
		sprintf(fmt + 4 * i, "%%0%c%c", pftype, delim);
	}			/* loop over returns */

	return lineFormatStack(fmt, rv_data, 0);
}				/* sql_mkunld */

/* like the above, but we build a comma-separated list with quotes,
ready for SQL insert or update.
You might be tempted to call this routine first, obtaining a string,
and then call lineFormat("insert into foo values(%s)",
but don't do that!
The returned string is built by lineFormat and is already in the buffer.
You instead need to make a copy of the string and then call lineFormat. */
char *sql_mkinsupd()
{
	char fmt[NUMRETS * 3 + 1];
	int i;
	char pftype;

	for (i = 0; i < rv_numRets; ++i) {
		pftype = sprintfChar(rv_type[i]);
		if (!pftype)
			errorPrint("2sql_mkinsupd cannot convert datatype %c",
				   rv_type[i]);
		if (pftype != 'd' && pftype != 'f')
			pftype = toupper(pftype);
		sprintf(fmt + 3 * i, "%%%c,", pftype);
	}			/* loop over returns */
	fmt[3 * i - 1] = 0;

	return lineFormatStack(fmt, rv_data, 0);
}				/* sql_mkinsupd */

/*********************************************************************
Date time functions.
*********************************************************************/

static char ndays[] = { 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

bool isLeapYear(int year)
{
	if (year % 4)
		return false;
	if (year % 100)
		return true;
	if (year % 400)
		return false;
	return true;
}				/* isLeapYear */

/* convert year, month, and day into a date. */
/* return -1 = bad year, -2 = bad month, -3 = bad day */
date dateEncode(int year, int month, int day)
{
	short i;
	long d;
	if ((year | month | day) == 0)
		return nullint;
	if (year < 1640 || year > 3000)
		return -1;
	if (month <= 0 || month > 12)
		return -2;
	if (day <= 0 || day > ndays[month])
		return -3;
	if (day == 29 && month == 2 && !isLeapYear(year))
		return -3;
	--year;
	d = year * 365L + year / 4 - year / 100 + year / 400;
	for (i = 1; i < month; ++i)
		d += ndays[i];
	++year;
	if (month > 2 && !isLeapYear(year))
		--d;
	d += (day - 1);
	d -= 598632;
	return d;
}				/* dateEncode */

/* convert a date back into year, month, and day */
/* the inverse of the above */
void dateDecode(date d, int *yp, int *mp, int *dp)
{
	int year, month, day;
	year = month = day = 0;
	if (d >= 0 && d <= 497094) {
/* how many years have rolled by; at worst 366 days in each */
		year = d / 366;
		year += 1640;
		while (dateEncode(++year, 1, 1) <= d) ;
		--year;
		d -= dateEncode(year, 1, 1);
		if (!isLeapYear(year))
			ndays[2] = 28;
		for (month = 1; month <= 12; ++month) {
			if (d < ndays[month])
				break;
			d -= ndays[month];
		}
		day = d + 1;
		ndays[2] = 29;	/* put it back */
	}
	*yp = year;
	*mp = month;
	*dp = day;
}				/* dateDecode */

/* convert a string into a date */
/* return -4 for bad format */
date stringDate(const char *s, bool yearfirst)
{
	short year, month, day, i, l;
	char delim;
	char buf[12];
	char *t;

	if (!s)
		return nullint;
	l = strlen(s);
	while (l && s[l - 1] == ' ')
		--l;
	if (!l)
		return nullint;
	if (l != 8 && l != 10)
		return -4;
	strncpy(buf, s, l);
	buf[l] = 0;
	delim = yearfirst ? '-' : '/';
	t = strchr(buf, delim);
	if (t)
		strmove(t, t + 1);
	t = strchr(buf, delim);
	if (t)
		strmove(t, t + 1);
	l = strlen(buf);
	if (l != 8)
		return -4;
	if (!strcmp(buf, "        "))
		return nullint;
	if (yearfirst) {
		char swap[4];
		strncpy(swap, buf, 4);
		strncpy(buf, buf + 4, 4);
		strncpy(buf + 4, swap, 4);
	}
	for (i = 0; i < 8; ++i)
		if (!isdigit(buf[i]))
			return -4;
	month = 10 * (buf[0] - '0') + buf[1] - '0';
	day = 10 * (buf[2] - '0') + buf[3] - '0';
	year = atoi(buf + 4);
	return dateEncode(year, month, day);
}				/* stringDate */

/* convert a date into a string, held in a static buffer */
/* cram squashes out the century, delimit puts in slashes */
char *dateString(date d, int flags)
{
	static char buf[12];
	char swap[8];
	int year, month, day;
	dateDecode(d, &year, &month, &day);
	if (!year)
		strcpy(buf, "  /  /    ");
	else
		sprintf(buf, "%02d/%02d/%04d", month, day, year);
	if (flags & DTCRUNCH)
		strmove(buf + 6, buf + 8);
	if (flags & YEARFIRST) {
		strncpy(swap, buf, 6);
		swap[2] = swap[5] = 0;
		strmove(buf, buf + 6);
		if (flags & DTDELIMIT)
			strcat(buf, "-");
		strcat(buf, swap);
		if (flags & DTDELIMIT)
			strcat(buf, "-");
		strcat(buf, swap + 3);
	} else if (!(flags & DTDELIMIT)) {
		char *s;
		s = strchr(buf, '/');
		strmove(s, s + 1);
		s = strchr(buf, '/');
		strmove(s, s + 1);
	}
	return buf;
}				/* dateString */

char *timeString(interval seconds, int flags)
{
	short h, m, s;
	char c = 'A';
	static char buf[12];
	if (seconds < 0 || seconds >= 86400)
		strcpy(buf, "  :  :   AM");
	else {
		h = seconds / 3600;
		seconds -= h * 3600L;
		m = seconds / 60;
		seconds -= m * 60;
		s = (short)seconds;
		if (flags & DTAMPM) {
			if (h == 0)
				h = 12;
			else if (h >= 12) {
				c = 'P';
				if (h > 12)
					h -= 12;
			}
		}
		sprintf(buf, "%02d:%02d:%02d %cM", h, m, s, c);
	}
	if (!(flags & DTAMPM))
		buf[8] = 0;
	if (flags & DTCRUNCH)
		strmove(buf + 5, buf + 8);
	if (!(flags & DTDELIMIT)) {
		strmove(buf + 2, buf + 3);
		if (buf[4] == ':')
			strmove(buf + 4, buf + 5);
	}
	return buf;
}				/* timeString */

/* convert string into time.
 * Like stringDate, we can return bad hour, bad minute, bad second, or bad format */
interval stringTime(const char *t)
{
	short h, m, s;
	bool ampm = false;
	char c;
	char buf[12];
	short i, l;
	if (!t)
		return nullint;
	l = strlen(t);
	while (l && t[l - 1] == ' ')
		--l;
	if (!l)
		return nullint;
	if (l < 4 || l > 11)
		return -4;
	strncpy(buf, t, l);
	buf[l] = 0;
	if (buf[l - 1] == 'M' && buf[l - 3] == ' ') {
		ampm = true;
		c = buf[l - 2];
		if (c != 'A' && c != 'P')
			return -4;
		buf[l - 3] = 0;
		l -= 3;
	}
	if (l < 4 || l > 8)
		return -4;
	if (buf[2] == ':')
		strmove(buf + 2, buf + 3);
	if (buf[4] == ':')
		strmove(buf + 4, buf + 5);
	l = strlen(buf);
	if (l != 4 && l != 6)
		return -4;
	if (!strncmp(buf, "      ", l))
		return nullint;
	for (i = 0; i < l; ++i)
		if (!isdigit(buf[i]))
			return -4;
	h = 10 * (buf[0] - '0') + buf[1] - '0';
	m = 10 * (buf[2] - '0') + buf[3] - '0';
	s = 0;
	if (l == 6)
		s = 10 * (buf[4] - '0') + buf[5] - '0';
	if (ampm) {
		if (h == 12) {
			if (c == 'A')
				h = 0;
		} else if (c == 'P')
			h += 12;
	}
	if (h < 0 || h >= 24)
		return -1;
	if (m < 0 || m >= 60)
		return -2;
	if (s < 0 || s >= 60)
		return -3;
	return h * 3600L + m * 60 + s;
}				/* stringTime */

char *moneyString(money m)
{
	static char buf[20], *s = buf;
	if (m == nullint)
		return "";
	if (m < 0)
		*s++ = '-', m = -m;
	sprintf(s, "$%ld.%02d", m / 100, (int)(m % 100));
	return buf;
}				/* moneyString */

money stringMoney(const char *s)
{
	short sign = 1;
	long m;
	double d;
	if (!s)
		return nullint;
	skipWhite(&s);
	if (*s == '-')
		sign = -sign, ++s;
	skipWhite(&s);
	if (*s == '$')
		++s;
	skipWhite(&s);
	if (!*s)
		return nullint;
	if (!stringIsFloat(s, &d))
		return -nullint;
	m = (long)(d * 100.0 + 0.5);
	return m * sign;
}				/* stringMoney */

/* Make sure edbrowse is connected to the database */
bool ebConnect(void)
{
	if (sql_database)
		return true;
	if (!dbarea) {
		setError(MSG_DBUnspecified);
		return false;
	}
	sql_connect(dbarea, dblogin, dbpw);
	if (!sql_database) {
		setError(MSG_DBConnect, rv_vendorStatus);
		return false;
	}

	return true;
}				/* ebConnect */

void dbClose(void)
{
	sql_disconnect();
}				/* dbClose */

static char myTab[64];
static const char *myWhere;
static char *scl;		/* select clause */
static int scllen;
static char *wcl;		/* where clause */
static int wcllen;
static char wherecol[COLNAMELEN + 2];
static struct DBTABLE *td;

/* put quotes around a column value */
static void pushQuoted(char **s, int *slen, const char *value, int colno)
{
	char quotemark = 0;
	char coltype = td->types[colno];

	if (!value || !*value) {
		stringAndString(s, slen, "NULL");
		return;
	}

	if (coltype != 'F' && coltype != 'N') {
/* Microsoft insists on single quote. */
		quotemark = '\'';
		if (strchr(value, quotemark))
			quotemark = '"';
	}
	if (quotemark)
		stringAndChar(s, slen, quotemark);
	stringAndString(s, slen, value);
	if (quotemark)
		stringAndChar(s, slen, quotemark);
}				/* pushQuoted */

static char *lineFields[MAXTCOLS];

static char *keysQuoted(void)
{
	char *u;
	int ulen;
	int key1 = td->key1, key2 = td->key2;
	int key3 = td->key3;
	if (!key1)
		return 0;
	u = initString(&ulen);
	--key1;
	stringAndString(&u, &ulen, "where ");
	stringAndString(&u, &ulen, td->cols[key1]);
	stringAndString(&u, &ulen, " = ");
	pushQuoted(&u, &ulen, lineFields[key1], key1);
	if (!key2)
		return u;

	--key2;
	stringAndString(&u, &ulen, " and ");
	stringAndString(&u, &ulen, td->cols[key2]);
	stringAndString(&u, &ulen, " = ");
	pushQuoted(&u, &ulen, lineFields[key2], key2);
	if (!key3)
		return u;

	--key3;
	stringAndString(&u, &ulen, " and ");
	stringAndString(&u, &ulen, td->cols[key3]);
	stringAndString(&u, &ulen, " = ");
	pushQuoted(&u, &ulen, lineFields[key3], key3);
	return u;
}				/* keysQuoted */

static void buildSelectClause(void)
{
	int i;
	scl = initString(&scllen);
	stringAndString(&scl, &scllen, "select ");
	for (i = 0; i < td->ncols; ++i) {
		if (i)
			stringAndChar(&scl, &scllen, ',');
		stringAndString(&scl, &scllen, td->cols[i]);
	}
	stringAndString(&scl, &scllen, " from ");
	stringAndString(&scl, &scllen, td->name);
}				/* buildSelectClause */

static char date2buf[24];
static bool dateBetween(const char *s)
{
	char *e;

	if (strlen(s) >= 24)
		return false;

	strcpy(date2buf, s);
	e = strchr(date2buf, '-');
	if (!e)
		return false;
	*e = 0;
	return stringIsDate(date2buf) | stringIsDate(e + 1);
}				/* dateBetween */

static bool buildWhereClause(void)
{
	int i, l, n, colno;
	const char *w = myWhere;
	const char *e;

	wcl = initString(&wcllen);
	wherecol[0] = 0;
	if (stringEqual(w, "*"))
		return true;

	e = strchr(w, '=');
	if (!e) {
		if (!td->key1) {
			setError(MSG_DBNoKey);
			return false;
		}
		colno = td->key1;
		e = td->cols[colno - 1];
		l = strlen(e);
		if (l > COLNAMELEN) {
			setError(MSG_DBColumnLong, e, COLNAMELEN);
			return false;
		}
		strcpy(wherecol, e);
		e = w - 1;
	} else if (isdigit(*w)) {
		colno = strtol(w, (char **)&w, 10);
		if (w != e) {
			setError(MSG_DBSyntax);
			return false;
		}
		if (colno == 0 || colno > td->ncols) {
			setError(MSG_DBColRange, colno);
			return false;
		}
		goto setcol_n;
	} else {
		colno = 0;
		if (e - w <= COLNAMELEN) {
			strncpy(wherecol, w, e - w);
			wherecol[e - w] = 0;
			for (i = 0; i < td->ncols; ++i) {
				if (!strstr(td->cols[i], wherecol))
					continue;
				if (colno) {
					setError(MSG_DBManyColumns, wherecol);
					return false;
				}
				colno = i + 1;
			}
		}
		if (!colno) {
			setError(MSG_DBNoColumn, wherecol);
			return false;
		}
setcol_n:
		w = td->cols[colno - 1];
		l = strlen(w);
		if (l > COLNAMELEN) {
			setError(MSG_DBColumnLong, w, COLNAMELEN);
			return false;
		}
		strcpy(wherecol, w);
	}

	stringAndString(&wcl, &wcllen, "where ");
	stringAndString(&wcl, &wcllen, wherecol);
	++e;
	w = e;
	if (!*e) {
		stringAndString(&wcl, &wcllen, " is null");
	} else if ((i = strtol(e, (char **)&e, 10)) >= 0 &&
		   *e == '-' && (n = strtol(e + 1, (char **)&e, 10)) >= 0
		   && *e == 0) {
		stringAndString(&wcl, &wcllen, " between ");
		stringAndNum(&wcl, &wcllen, i);
		stringAndString(&wcl, &wcllen, " and ");
		stringAndNum(&wcl, &wcllen, n);
	} else if (dateBetween(w)) {
		stringAndString(&wcl, &wcllen, " between \"");
		stringAndString(&wcl, &wcllen, date2buf);
		stringAndString(&wcl, &wcllen, "\" and \"");
		stringAndString(&wcl, &wcllen, date2buf + strlen(date2buf) + 1);
		stringAndChar(&wcl, &wcllen, '"');
	} else if (w[strlen(w) - 1] == '*') {
		stringAndString(&wcl, &wcllen, lineFormat(" matches %S", w));
	} else {
		stringAndString(&wcl, &wcllen, " = ");
		pushQuoted(&wcl, &wcllen, w, colno - 1);
	}

	return true;
}				/* buildWhereClause */

static bool setTable(void)
{
	static const short exclist[] = { EXCNOTABLE, EXCNOCOLUMN, 0 };
	int cid, nc, i, part1, part2, part3, part4;
	const char *s = cf->fileName;
	const char *t = strchr(s, ']');
	if (t - s >= sizeof(myTab))
		errorPrint("2table name too long, limit %d characters",
			   sizeof(myTab) - 4);
	strncpy(myTab, s, t - s);
	myTab[t - s] = 0;
	myWhere = t + 1;

	td = cw->table;
	if (td)
		return true;

/* haven't glommed onto this table yet */
	td = findTableDescriptor(myTab);
	if (td) {
		if (!td->types) {
			buildSelectClause();
			sql_exclist(exclist);
			cid = sql_prepare(scl);
			nzFree(scl);
			if (rv_lastStatus) {
				if (rv_lastStatus == EXCNOTABLE)
					setError(MSG_DBNoTable, td->name);
				else if (rv_lastStatus == EXCNOCOLUMN)
					setError(MSG_DBBadColumn);
				return false;
			}
			td->types = cloneString(rv_type);
			nc = rv_numRets;
			td->nullable = allocMem(nc);
			memcpy(td->nullable, rv_nullable, nc);
			sql_free(cid);
		}

	} else {

		sql_exclist(exclist);
		cid = sql_prepare("select * from %s", myTab);
		if (rv_lastStatus) {
			if (rv_lastStatus == EXCNOTABLE)
				setError(MSG_DBNoTable, myTab);
			return false;
		}
		td = newTableDescriptor(myTab);
		if (!td) {
			sql_free(cid);
			return false;
		}
		nc = rv_numRets;
		if (nc > MAXTCOLS) {
			printf
			    ("warning, only the first %d columns will be selected\n",
			     MAXTCOLS);
			nc = MAXTCOLS;
		}
		td->types = cloneString(rv_type);
		td->types[nc] = 0;
		td->ncols = nc;
		td->nullable = allocMem(nc);
		memcpy(td->nullable, rv_nullable, nc);
		for (i = 0; i < nc; ++i)
			td->cols[i] = cloneString(rv_name[i]);
		sql_free(cid);

		getPrimaryKey(myTab, &part1, &part2, &part3, &part4);
		if (part1 > nc)
			part1 = 0;
		if (part2 > nc)
			part2 = 0;
		if (part3 > nc)
			part3 = 0;
		if (part4 > nc)
			part4 = 0;
		td->key1 = part1;
		td->key2 = part2;
		td->key3 = part3;
		td->key4 = part4;
	}

	cw->table = td;
	return true;
}				/* setTable */

void showColumns(void)
{
	char c;
	const char *desc;
	int i;

	if (!setTable())
		return;
	i_printf(MSG_Table);
	printf(" %s", td->name);
	if (!stringEqual(td->name, td->shortname))
		printf(" [%s]", td->shortname);
	i = sql_selectOne("select count(*) from %s", td->name);
	printf(", %d ", i);
	i_printf(i == 1 ? MSG_Row : MSG_Rows);
	nl();

	for (i = 0; i < td->ncols; ++i) {
		printf("%d ", i + 1);
		if (td->key1 == i + 1 || td->key2 == i + 1 || td->key3 == i + 1
		    || td->key4 == i + 1)
			printf("*");
		if (td->nullable[i])
			printf("+");
		printf("%s ", td->cols[i]);
		c = td->types[i];
		switch (c) {
		case 'N':
			desc = "int";
			break;
		case 'D':
			desc = "date";
			break;
		case 'I':
			desc = "time";
			break;
		case 'M':
			desc = "money";
			break;
		case 'F':
			desc = "float";
			break;
		case 'S':
			desc = "string";
			break;
		case 'C':
			desc = "char";
			break;
		case 'B':
			desc = "blob";
			break;
		case 'T':
			desc = "text";
			break;
		default:
			desc = "?";
			break;
		}		/* switch */
		printf("%s\n", desc);
	}
}				/* showColumns */

void showForeign(void)
{
	if (!setTable())
		return;
	i_printf(MSG_Fkeys, td->name);
	fetchForeign(td->name);
}				/* showForeign */

/* Select rows of data and put them into the text buffer */
static bool rowsIntoBuffer(int cid, const char *types, char **bufptr, int *lcnt)
{
	char *rbuf, *unld, *u, *v, *s, *end;
	int rbuflen;
	bool rc = false;

	*bufptr = emptyString;
	*lcnt = 0;
	rbuf = initString(&rbuflen);

	while (sql_fetchNext(cid, 0)) {
		unld = sql_mkunld('\177');
		if (strchr(unld, '|')) {
			setError(MSG_DBPipes);
			goto abort;
		}
		if (strchr(unld, '\n')) {
			setError(MSG_DBNewline);
			goto abort;
		}
		for (s = unld; *s; ++s)
			if (*s == '\177')
				*s = '|';
		s[-1] = '\n';	/* overwrite the last pipe */

/* look for blob column */
		if (rv_blobLoc && (s = strpbrk(types, "BT"))) {
			int bfi = s - types;	/* blob field index */
			int cx = 0;	/* context, where to put the blob */
			int j;

			u = unld;
			for (j = 0; j < bfi; ++j)
				u = strchr(u, '|') + 1;
			v = strpbrk(u, "|\n");
			end = v + strlen(v);
			cx = sideBuffer(0, rv_blobLoc, rv_blobSize, 0);
			nzFree(rv_blobLoc);
			sprintf(myTab, "<%d>", cx);
			if (!cx)
				myTab[0] = 0;
			j = strlen(myTab);
/* unld is pretty long; I'm just going to assume there is enough room for this */
			memmove(u + j, v, end + 1 - v);
			memcpy(u, myTab, j);
		}

		stringAndString(&rbuf, &rbuflen, unld);
		++*lcnt;
	}
	rc = true;

abort:
	sql_closeFree(cid);
	*bufptr = rbuf;
	return rc;
}				/* rowsIntoBuffer */

bool sqlReadRows(const char *filename, char **bufptr)
{
	int cid, lcnt;

	*bufptr = emptyString;
	if (!ebConnect())
		return false;
	if (!setTable())
		return false;

	myWhere = strchr(filename, ']') + 1;
	if (!*myWhere)
		return true;

	if (!buildWhereClause())
		return false;
	buildSelectClause();
	rv_blobFile = 0;
	cid = sql_prepOpen("%s %0s", scl, wcl);
	nzFree(scl);
	nzFree(wcl);
	if (cid < 0)
		return false;

	return rowsIntoBuffer(cid, td->types, bufptr, &lcnt);
}				/* sqlReadRows */

/* Split a line at pipe boundaries, and make sure the field count is correct */
static bool intoFields(char *line)
{
	char *s = line;
	int j = 0;
	int c;

	while (1) {
		lineFields[j] = s;
		s = strpbrk(s, "|\n");
		c = *s;
		*s++ = 0;
		++j;
		if (c == '\n')
			break;
		if (j < td->ncols)
			continue;
		setError(MSG_DBAddField);
		return false;
	}

	if (j == td->ncols)
		return true;
	setError(MSG_DBLostField);
	return false;
}				/* intoFields */

static bool rowCountCheck(int action, int cnt1)
{
	int cnt2 = rv_lastNrows;

	if (cnt1 == cnt2)
		return true;

	setError(MSG_DBDeleteCount + action, cnt1, cnt2);
	return false;
}				/* rowCountCheck */

static int keyCountCheck(void)
{
	if (!td->key1) {
		setError(MSG_DBNoKeyCol);
		return false;
	}
	if (!td->key2)
		return 1;
	if (!td->key3)
		return 2;
	if (!td->key4)
		return 3;
	setError(MSG_DBManyKeyCol);
	return 0;
}				/* keyCountCheck */

/* Typical error conditions for insert update delete */
static const short insupdExceptions[] = {
	EXCVIEWUSE, EXCREFINT, EXCITEMLOCK, EXCPERMISSION,
	EXCDEADLOCK, EXCCHECK, EXCTIMEOUT, EXCNOTNULLCOLUMN, 0
};

static bool insupdError(int action, int rcnt)
{
	int rc = rv_lastStatus;
	int msg;

	if (rc) {
		switch (rc) {
		case EXCVIEWUSE:
			msg = MSG_DBView;
			break;
		case EXCREFINT:
			msg = MSG_DBRefInt;
			break;
		case EXCITEMLOCK:
			msg = MSG_DBLocked;
			break;
		case EXCPERMISSION:
			msg = MSG_DBPerms;
			break;
		case EXCDEADLOCK:
			msg = MSG_DBDeadlock;
			break;
		case EXCNOTNULLCOLUMN:
			msg = MSG_DBNotNull;
			break;
		case EXCCHECK:
			msg = MSG_DBCheck;
			break;
		case EXCTIMEOUT:
			msg = MSG_DBTimeout;
			break;
		default:
			setError(MSG_DBMisc, rv_vendorStatus);
			return false;
		}

		setError(msg);
		return false;
	}

	return rowCountCheck(action, rcnt);
}				/* insupdError */

bool sqlDelRows(int start, int end)
{
	int nkeys, ndel, ln;

	if (!setTable())
		return false;

	nkeys = keyCountCheck();
	if (!nkeys)
		return false;

	ndel = end - start + 1;
	ln = start;
	if (ndel > 100) {
		setError(MSG_DBMassDelete);
		return false;
	}

/* We could delete all the rows with one statement, using an in(list),
 * but that won't work when the key is two columns.
 * I have to write the one-line-at-a-time code anyways,
 * I'll just use that for now. */
	while (ndel--) {
		char *wherekeys;
		char *line = (char *)fetchLine(ln, 0);
		intoFields(line);
		wherekeys = keysQuoted();
		sql_exclist(insupdExceptions);
		sql_exec("delete from %s %s", td->name, wherekeys);
		nzFree(wherekeys);
		nzFree(line);
		if (!insupdError(0, 1))
			return false;
		delText(ln, ln);
	}

	return true;
}				/* sqlDelRows */

bool sqlUpdateRow(pst source, int slen, pst dest, int dlen)
{
	char *d2;		/* clone of dest */
	char *wherekeys;
	char *s, *t;
	int j, l1, l2, nkeys, key1, key2;
	char *u1;		/* column=value of the update statement */
	int u1len;

/* compare all the way out to newline, so we know both strings end at the same time */
	if (slen == dlen && !memcmp(source, dest, slen + 1))
		return true;

	if (!setTable())
		return false;

	nkeys = keyCountCheck();
	if (!nkeys)
		return false;
	key1 = td->key1 - 1;
	key2 = td->key2 - 1;

	d2 = (char *)clonePstring(dest);
	if (!intoFields(d2)) {
		nzFree(d2);
		return false;
	}

	j = 0;
	u1 = initString(&u1len);
	s = (char *)source;

	while (1) {
		t = strpbrk(s, "|\n");
		l1 = t - s;
		l2 = strlen(lineFields[j]);
		if (l1 != l2 || memcmp(s, lineFields[j], l1)) {
			if (j == key1 || j == key2) {
				setError(MSG_DBChangeKey);
				goto abort;
			}
			if (td->types[j] == 'B') {
				setError(MSG_DBChangeBlob);
				goto abort;
			}
			if (td->types[j] == 'T') {
				setError(MSG_DBChangeText);
				goto abort;
			}
			if (*u1)
				stringAndString(&u1, &u1len, ", ");
			stringAndString(&u1, &u1len, td->cols[j]);
			stringAndString(&u1, &u1len, " = ");
			pushQuoted(&u1, &u1len, lineFields[j], j);
		}

		if (*t == '\n')
			break;
		s = t + 1;
		++j;
	}

	wherekeys = keysQuoted();
	sql_exclist(insupdExceptions);
	sql_exec("update %s set %s %s", td->name, u1, wherekeys);
	nzFree(wherekeys);
	if (!insupdError(2, 1))
		goto abort;

	nzFree(d2);
	nzFree(u1);
	return true;

abort:
	nzFree(d2);
	nzFree(u1);
	return false;
}				/* sqlUpdateRow */

bool sqlAddRows(int ln)
{
	char *u1, *u2;		/* pieces of the insert statement */
	char *u3;		/* line with pipes */
	char *unld, *s;
	int u1len, u2len, u3len;
	int j, l;
	double dv;
	char inp[256];
	bool rc;

	if (!setTable())
		return false;

	while (1) {
		u1 = initString(&u1len);
		u2 = initString(&u2len);
		u3 = initString(&u3len);

		for (j = 0; j < td->ncols; ++j) {
reenter:
			if (strchr("BT", td->types[j]))
				continue;
			printf("%s: ", td->cols[j]);
			fflush(stdout);
			if (!fgets(inp, sizeof(inp), stdin)) {
				puts("EOF");
				ebClose(1);
			}
			l = strlen(inp);
			if (l && inp[l - 1] == '\n')
				inp[--l] = 0;
			if (stringEqual(inp, ".")) {
				nzFree(u1);
				nzFree(u2);
				nzFree(u3);
				return true;
			}

			if (inp[0] == 0) {
/* I thought it was a good idea to prevent nulls from going into not-null
 * columns, but then I remembered  not null default value,
 * where the database converts null into something real.
 * I want to allow this. */
				goto goodfield;
			}

/* verify the integrity of the entered field */
			if (strchr(inp, '|')) {
				puts("please, no pipes in the data");
				goto reenter;
			}

			switch (td->types[j]) {
			case 'N':
				s = inp;
				if (*s == '-')
					++s;
				if (stringIsNum(s) < 0) {
					puts("number expected");
					goto reenter;
				}
				break;
			case 'F':
				if (!stringIsFloat(inp, &dv)) {
					puts("decimal number expected");
					goto reenter;
				}
				break;
			case 'C':
				if (strlen(inp) > 1) {
					puts("one character expected");
					goto reenter;
				}
				break;
			case 'D':
				if (stringDate(inp, false) < 0) {
					puts("date expected");
					goto reenter;
				}
				break;
			case 'I':
				if (stringTime(inp) < 0) {
					puts("time expected");
					goto reenter;
				}
				break;
			}

goodfield:

/* turn 0 into next serial number */
			if (j == td->key1 - 1 && td->types[j] == 'N' &&
			    stringEqual(inp, "0")) {
				int nextkey =
				    sql_selectOne("select max(%s) from %s",
						  td->cols[j], td->name);
				if (isnull(nextkey)) {
					i_puts(MSG_DBNextSerial);
					goto reenter;
				}
				sprintf(inp, "%d", nextkey + 1);
			}

			if (*u1)
				stringAndChar(&u1, &u1len, ',');
			stringAndString(&u1, &u1len, td->cols[j]);

			if (*u2)
				stringAndChar(&u2, &u2len, ',');
			pushQuoted(&u2, &u2len, inp, j);

			stringAndString(&u3, &u3len, inp);
			stringAndChar(&u3, &u3len, '|');
		}

		sql_exclist(insupdExceptions);
		sql_exec("insert into %s (%s) values (%s)", td->name, u1, u2);
		nzFree(u1);
		nzFree(u2);
		if (!insupdError(1, 1)) {
			nzFree(u3);
			printf("Error: ");
			showError();
			continue;
		}
#if 0
/* Fetch the row just entered. */
/* Don't know how to do this without rowid. */
		rowid = rv_lastRowid;
		buildSelectClause();
		sql_select("%s where rowid = %d", scl, rowid, 0);
		nzFree(scl);
		unld = sql_mkunld('|');
		l = strlen(unld);
		unld[l - 1] = '\n';	/* overwrite the last pipe */
#else
		unld = u3;
		l = strlen(unld);
		unld[l - 1] = '\n';	/* overwrite the last pipe */
#endif

		rc = addTextToBuffer((pst) unld, l, ln, false);
		nzFree(u3);
		if (!rc)
			return false;
		++ln;
	}

/* This pointis not reached; make the compilerhappy */
	return true;
}				/* sqlAddRows */

/*********************************************************************
run the analog of /bin/comm on two open cursors,
rather than two Unix files.
This assumes a common unique key that we use to sync up the rows.
The cursors should be sorted by this key.
*********************************************************************/

static void cursor_comm(const char *stmt1, const char *stmt2,	/* the two select statements */
			const char *orderby,	/* which fetched column is the unique key */
			fnptr f,	/* call this function for differences */
			char delim)
{				/* sql_mkunld() delimiter, or call mkinsupd if delim = 0 */
	short cid1, cid2;	/* the cursor ID numbers */
	char *line1, *line2, *s;	/* the two fetched rows */
	void *blob1, *blob2;	/* one blob per table */
	bool eof1, eof2, get1, get2;
	int sortval1, sortval2;
	char sortstring1[80], sortstring2[80];
	int sortcol;
	char sorttype;
	int passkey1, passkey2;
	static const char sortnull[] = "cursor_comm, sortval%d is null";
	static const char sortlong[] =
	    "cursor_comm cannot key on strings longer than %d";
	static const char noblob[] =
	    "sorry, cursor_comm cannot handle blobs yet";

	cid1 = sql_prepOpen(stmt1);
	cid2 = sql_prepOpen(stmt2);

	sortcol = findColByName(orderby);
	sorttype = rv_type[sortcol];
	if (charInList("NDIS", sorttype) < 0)
		errorPrint("2cursor_com(), column %s has bad type %c", orderby,
			   sorttype);
	if (sorttype == 'S')
		passkey1 = (int)sortstring1, passkey2 = (int)sortstring2;

	eof1 = eof2 = false;
	get1 = get2 = true;
	rv_blobFile = 0;	/* in case the cursor has a blob */
	line1 = line2 = 0;
	blob1 = blob2 = 0;

	while (true) {
		if (get1) {	/* fetch first row */
			eof1 = !sql_fetchNext(cid1, 0);
			nzFree(line1);
			line1 = 0;
			nzFree(blob1);
			blob1 = 0;
			if (!eof1) {
				if (sorttype == 'S') {
					s = rv_data[sortcol].ptr;
					if (isnullstring(s))
						errorPrint(sortnull, 1);
					if (strlen(s) >= sizeof(sortstring1))
						errorPrint(sortlong,
							   sizeof(sortstring1));
					strcpy(sortstring1, s);
				} else {
					passkey1 = sortval1 =
					    rv_data[sortcol].l;
					if (isnull(sortval1))
						errorPrint(sortnull, 1);
				}
				line1 =
				    cloneString(delim ? sql_mkunld(delim) :
						sql_mkinsupd());
				if (rv_blobLoc) {
					blob1 = rv_blobLoc;
					errorPrint(noblob);
				}
			}	/* not eof */
		}
		/* looking for first line */
		if (get2) {	/* fetch second row */
			eof2 = !sql_fetchNext(cid2, 0);
			nzFree(line2);
			line2 = 0;
			nzFree(blob2);
			blob2 = 0;
			if (!eof2) {
				if (sorttype == 'S') {
					s = rv_data[sortcol].ptr;
					if (isnullstring(s))
						errorPrint(sortnull, 2);
					if (strlen(s) >= sizeof(sortstring2))
						errorPrint(sortlong,
							   sizeof(sortstring2));
					strcpy(sortstring2,
					       rv_data[sortcol].ptr);
				} else {
					passkey2 = sortval2 =
					    rv_data[sortcol].l;
					if (isnull(sortval2))
						errorPrint(sortnull, 2);
				}
				line2 =
				    cloneString(delim ? sql_mkunld(delim) :
						sql_mkinsupd());
				if (rv_blobLoc) {
					blob2 = rv_blobLoc;
					errorPrint(noblob);
				}
			}	/* not eof */
		}
		/* looking for second line */
		if (eof1 & eof2)
			break;	/* done */
		get1 = get2 = false;

/* in cid2, but not in cid1 */
		if (eof1 || (!eof2 &&
			     ((sorttype == 'S'
			       && strcmp(sortstring1, sortstring2) > 0)
			      || (sorttype != 'S' && sortval1 > sortval2)))) {
			(*f) ('>', line1, line2, passkey2);
			get2 = true;
			continue;
		}

/* in cid1, but not in cid2 */
		if (eof2 || (!eof1 &&
			     ((sorttype == 'S'
			       && strcmp(sortstring1, sortstring2) < 0)
			      || (sorttype != 'S' && sortval1 < sortval2)))) {
			(*f) ('<', line1, line2, passkey1);
			get1 = true;
			continue;
		}
		/* insert case */
		get1 = get2 = true;
/* perhaps the lines are equal */
		if (stringEqual(line1, line2))
			continue;

/* lines are different between the two cursors */
		(*f) ('*', line1, line2, passkey2);
	}			/* loop over parallel cursors */

	nzFree(line1);
	nzFree(line2);
	nzFree(blob1);
	nzFree(blob2);
	sql_closeFree(cid1);
	sql_closeFree(cid2);
}				/* cursor_comm */

/*********************************************************************
Sync up two tables, or corresponding sections of two tables.
These are usually equischema tables in parallel databases or machines.
This isn't used by edbrowse; it's just something I wrote,
and I thought you might find it useful.
It follows the C convention of copying the second argument
to the first, like the string and memory functions,
rather than the shell convention of copying (cp) the first argument to the second.
Hey - why have one standard, when you can have two?
*********************************************************************/

static const char *synctable;	/* table being sync-ed */
static const char *synckeycol;	/* key column */
static const char *sync_clause;	/* additional clause, to sync only part of the table */

/* convert column name into column index */
int findColByName(const char *name)
{
	int i;
	for (i = 0; rv_name[i][0]; ++i)
		if (stringEqual(name, rv_name[i]))
			break;
	if (!rv_name[i][0])
		errorPrint
		    ("2Column %s not found in the columns or aliases of your select statement",
		     name);
	return i;
}				/* findColByName */

static int syncup_comm_fn(char action, char *line1, char *line2, int key)
{
	switch (action) {
	case '<':		/* delete */
		sql_exec("delete from %s where %s = %d %0s",
			 synctable, synckeycol, key, sync_clause);
		break;
	case '>':		/* insert */
		sql_exec("insert into %s values(%s)", synctable, line2);
		break;
	case '*':		/* update */
		sql_exec("update %s set * = (%s) where %s = %d %0s",
			 synctable, line2, synckeycol, key, sync_clause);
		break;
	}			/* switch */
	return 0;
}				/* syncup_comm_fn */

/* make table1 look like table2 */
void syncup_table(const char *table1, const char *table2,	/* the two tables */
		  const char *keycol,	/* the key column */
		  const char *otherclause)
{
	char stmt1[200], stmt2[200];
	int len;

	synctable = table1;
	synckeycol = keycol;
	sync_clause = otherclause;
	len = strlen(table1);
	if ((int)strlen(table2) > len)
		len = strlen(table2);
	if (otherclause)
		len += strlen(otherclause);
	len += strlen(keycol);
	if (len + 30 > sizeof(stmt1))
		errorPrint
		    ("2constructed select statement in syncup_table() is too long");

	if (otherclause) {
		skipWhite(&otherclause);
		if (strncmp(otherclause, "and ", 4)
		    && strncmp(otherclause, "AND ", 4))
			errorPrint
			    ("2restricting clause in syncup_table() does not start with \"and\".");
		sprintf(stmt1, "select * from %s where %s order by %s", table1,
			otherclause + 4, keycol);
		sprintf(stmt2, "select * from %s where %s order by %s", table2,
			otherclause + 4, keycol);
	} else {
		sprintf(stmt1, "select * from %s order by %s", table1, keycol);
		sprintf(stmt2, "select * from %s order by %s", table2, keycol);
	}

	cursor_comm(stmt1, stmt2, keycol, (fnptr) syncup_comm_fn, 0);
}				/* syncup_table */

int goSelect(int *startLine, char **rbuf)
{
	int lineno = *startLine;
	pst line;
	char *cmd, *s;
	int cmdlen;
	int i, j, l, action, cid;
	bool rc;
	static const char *actionWords[] = {
		"select", "insert", "update", "delete", "execute",
		0
	};
	static const int actionCodes[] = {
		MSG_Selected, MSG_Inserted, MSG_Updated, MSG_Deleted,
		MSG_ProcExec
	};

	*rbuf = emptyString;

/* Make sure first line begins with ] */
	line = fetchLine(lineno, -1);
	if (!line || line[0] != ']')
		return -1;

	j = pstLength(line);
	cmd = initString(&cmdlen);
	stringAndBytes(&cmd, &cmdlen, (char *)line, j);
	cmd[0] = ' ';

	while (j == 1 || line[j - 2] != ';') {
		if (++lineno > cw->dol || !(line = fetchLine(lineno, -1))) {
			setError(MSG_UnterminatedSelect);
			nzFree(cmd);
			return 0;
		}
		if (line[0] == ']') {
			--lineno;
			break;
		}
		j = pstLength(line);
		stringAndBytes(&cmd, &cmdlen, (char *)line, j);
	}

/* Try to infer action from the first word of the command. */
	action = -1;
	s = cmd;
	skipWhite2(&s);
	for (i = 0; actionWords[i]; ++i) {
		l = strlen(actionWords[i]);
		if (memEqualCI(s, actionWords[i], l) && isspace(s[l])) {
			action = actionCodes[i];
			break;
		}
	}

	if (!ebConnect()) {
		nzFree(cmd);
		return 0;
	}

	rv_blobFile = 0;

	if (action == MSG_Selected) {
		cid = sql_prepOpen(cmd);
		nzFree(cmd);
		if (cid < 0)
			return 0;
grabrows:
		*startLine = lineno;
		rc = rowsIntoBuffer(cid, rv_type, rbuf, &j);
printrows:
		printf("%d ", j);
		i_printf(j == 1 ? MSG_Row : MSG_Rows);
		printf(" ");
		i_printf(action);
		nl();
		return rc;
	}

	if (action == MSG_ProcExec) {
		cid = sql_prepOpen(cmd);
		nzFree(cmd);
		if (cid < 0)
			return 0;
		if (rv_numRets) {
			action = MSG_Selected;
			goto grabrows;
		}
		sql_closeFree(cid);
		*startLine = lineno;
		i_puts(MSG_ProcExec);
		return 1;
	}

/* Don't know what kind of sql command this is. */
/* Run it anyways. */
	rc = sql_execNF(cmd);
	nzFree(cmd);
	j = rv_lastNrows;
	if (rc)
		*startLine = lineno;
	if (action >= MSG_Selected && action <= MSG_Deleted && (j || rc))
		goto printrows;
	if (rc)
		i_puts(MSG_OK);
	return rc;
}				/* goSelect */
