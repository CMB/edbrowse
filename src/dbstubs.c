/* dbstubs.c
 * Stubs for the database functions.
 * After all, most people will compile this without database access.
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"

bool sqlPresent = false;

bool sqlReadRows(const char *filename, char **bufptr)
{
	setError(MSG_DBNotCompiled);
	*bufptr = emptyString;
	return false;
}				/* sqlReadRows */

void dbClose(void)
{
}				/* dbClose */

void showColumns(void)
{
}				/* showColumns */

void showForeign(void)
{
}				/* showForeign */

bool showTables(void)
{
	return false;
}				/* showTables */

bool sqlDelRows(int start, int end)
{
	return false;
}				/* sqlDelRows */

bool sqlUpdateRow(pst source, int slen, pst dest, int dlen)
{
	return false;
}				/* sqlUpdateRow */

bool sqlAddRows(int ln)
{
	return false;
}				/* sqlAddRows */

bool ebConnect(void)
{
	setError(MSG_DBNotCompiled);
	return false;
}				/* ebConnect */

int goSelect(int *startLine, char **rbuf)
{
	*rbuf = emptyString;
	return -1;
}				/* goSelect */
