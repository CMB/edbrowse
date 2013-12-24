/* dbstubs.c
 * Stubs for the database functions.
 * After all, most people will compile this without database access.
 * Copyright (c) Karl Dahlke, 2008
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"


eb_bool
sqlReadRows(const char *filename, char **bufptr)
{
    setError(MSG_DBNotCompiled);
    *bufptr = EMPTYSTRING;
    return eb_false;
}				/* sqlReadRows */

void
dbClose(void)
{
}				/* dbClose */

void
showColumns(void)
{
}				/* showColumns */

void
showForeign(void)
{
}				/* showForeign */

eb_bool
showTables(void)
{
}				/* showTables */

eb_bool
sqlDelRows(int start, int end)
{
}				/* sqlDelRows */

eb_bool
sqlUpdateRow(pst source, int slen, pst dest, int dlen)
{
}				/* sqlUpdateRow */

eb_bool
sqlAddRows(int ln)
{
}				/* sqlAddRows */

eb_bool
ebConnect(void)
{
    setError(MSG_DBNotCompiled);
    return eb_false;
}				/* ebConnect */

int
goSelect(int *startLine, char **rbuf)
{
    *rbuf = EMPTYSTRING;
    return -1;
}				/* goSelect */
