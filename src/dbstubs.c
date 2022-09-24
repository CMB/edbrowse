/* dbstubs.c, Stubs for the database functions. */

#include "eb.h"

bool sqlPresent = false;

bool sqlReadRows(const char *filename, char **bufptr)
{
	setError(MSG_DBNotCompiled);
	*bufptr = emptyString;
	return false;
}

void dbClose(void)
{
}

void showColumns(void)
{
}

void showForeign(void)
{
}

bool showTables(void)
{
	return false;
}

bool sqlDelRows(int start, int end)
{
	return false;
}

bool sqlUpdateRow(pst source, int slen, pst dest, int dlen)
{
	return false;
}

bool sqlAddRows(int ln)
{
	return false;
}

bool ebConnect(void)
{
	setError(MSG_DBNotCompiled);
	return false;
}

int goSelect(int *startLine, char **rbuf)
{
	*rbuf = emptyString;
	return -1;
}
