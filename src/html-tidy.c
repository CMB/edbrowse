/*********************************************************************
html-tidy.c: use tidy5 to parse html tags.
This is the only file that includes tidy.h, or accesses the tidy library.
If you prefer a different parser in the future,
write another file, html-foo.c, having the same connection to edbrowse,
and change the makefile accordingly.
If tidy5 changes its API, you only need edit this file.
Note that tidy has Bool yes and no, which, fortunately, do not collide with
edbrowse bool true false.
*********************************************************************/

#include "eb.h"

#include <tidy.h>
#include <tidybuffio.h>

/* the tidy structure corresponding to the html */
static TidyDoc tdoc;

/* traverse the tidy tree with a callback function */
typedef void (*nodeFunction) (TidyNode node, int level, bool opentag);
static nodeFunction traverse_callback;
/* possible callback functions */
static void printNode(TidyNode node, int level, bool opentag);
static void convertNode(TidyNode node, int level, bool opentag);

static void traverseNode(TidyNode node, int level)
{
	TidyNode child;

/* first the callback function */
	(*traverse_callback) (node, level, true);

/* and now the children */
	for (child = tidyGetChild(node); child; child = tidyGetNext(child))
		traverseNode(child, level + 1);

	(*traverse_callback) (node, level, false);
}				/* traverseNode */

static void traverseAll(void)
{
	traverseNode(tidyGetRoot(tdoc), 0);
}

/* Like the default tidy error reporter, except messages are suppressed
 * unless debugLevel >= 3, and they are sent to stdout
 * rather than stderr, like most edbrowse messages.
 * Since these are debug messages, they are not internationalized. */

static Bool TIDY_CALL tidyErrorHandler(TidyDoc tdoc, TidyReportLevel lvl,
			     uint line, uint col, ctmbstr mssg)
{
	if (debugLevel >= 3)
		printf("line %d column %d: %s\n", line, col, mssg);
	return no;
}				/* tidyErrorHandler */

/** Callback to filter messages by diagnostic level:
**  info, warning, etc.  Just set diagnostic output 
**  handler to redirect all diagnostics output.  Return true
**  to proceed with output, false to cancel.
*/
Bool TIDY_CALL tidyReportFilter( TidyDoc tdoc, TidyReportLevel lvl,
                                           uint line, uint col, ctmbstr mssg )
{
    return yes;
}

/* Work around a nasty bug in tidy5 wherein "<script>" anywhere
 * in a javascript will totally derail things.
 * I turn < into \x3c. */
static char *escapeLessScript(const char *htmltext)
{
	char *ns;		/* new string */
	int ns_l;
	const char *s1, *s2;	/* start and end of script */
	const char *lw;		/* last write */
	const char *q1;		/* inner <script */
	const char *q2;		/* inner <\/script */

	ns = initString(&ns_l);
	lw = htmltext;

	while (true) {
		s1 = strstrCI(lw, "<script");
		if (!s1)
			break;
		s1 += 7;
		if (isalnumByte(*s1)) {	/* <scriptx */
			stringAndBytes(&ns, &ns_l, lw, s1 - lw);
			lw = s1;
			continue;
		}
		s2 = strstrCI(s1, "</script");
		if (!s2)
			goto abort;

/* script now has a start and end */
		stringAndBytes(&ns, &ns_l, lw, s1 - lw);
		lw = s1;

		while (true) {
			q1 = strstrCI(lw, "<script");
			if (q1 && q1 > s2)
				q1 = 0;
			q2 = strstrCI(lw, "<\\/script");
			if (q2 && q2 > s2)
				q2 = 0;
			if (!q1)
				q1 = q2;
			if (!q1)
				break;
			if (q2 && q2 < q1)
				q1 = q2;
			stringAndBytes(&ns, &ns_l, lw, q1 - lw);
			stringAndString(&ns, &ns_l, "\\x3c");
			lw = q1 + 1;
		}

		stringAndBytes(&ns, &ns_l, lw, s2 - lw);
		lw = s2;
	}

	stringAndString(&ns, &ns_l, lw);
	return ns;

abort:
	nzFree(ns);
	return 0;
}				/* escapeLessScript */

/* the entry point */
void html2nodes(const char *htmltext)
{
	char *htmlfix = 0;

	tdoc = tidyCreate();
	tidyOptSetBool(tdoc, TidySkipQuotes, yes);
	tidySetReportFilter(tdoc, tidyErrorHandler);
//    tidySetReportFilter(tdoc, tidyReportFilter);

	tidySetCharEncoding(tdoc, (cons_utf8 ? "utf8" : "latin1"));

//      htmlfix = escapeLessScript(htmltext);
	if (htmlfix) {
		tidyParseString(tdoc, htmlfix);
		nzFree(htmlfix);
	} else
		tidyParseString(tdoc, htmltext);

	tidyCleanAndRepair(tdoc);

	if (debugLevel >= 5) {
		traverse_callback = printNode;
		traverseAll();
	}

/* convert tidy nodes into edbrowse nodes */
	traverse_callback = convertNode;
	traverseAll();

	tidyRelease(tdoc);
}				/* html2nodes */

/* this is strictly for debugging, level >= 5 */
static void printNode(TidyNode node, int level, bool opentag)
{
	ctmbstr name;
	TidyAttr tattr;

	if (!opentag) {
		puts("}");
		return;
	}

	switch (tidyNodeGetType(node)) {
	case TidyNode_Root:
		name = "Root";
		break;
	case TidyNode_DocType:
		name = "DOCTYPE";
		break;
	case TidyNode_Comment:
		name = "Comment";
		break;
	case TidyNode_ProcIns:
		name = "Processing Instruction";
		break;
	case TidyNode_Text:
		name = "Text";
		break;
	case TidyNode_CDATA:
		name = "CDATA";
		break;
	case TidyNode_Section:
		name = "XML Section";
		break;
	case TidyNode_Asp:
		name = "ASP";
		break;
	case TidyNode_Jste:
		name = "JSTE";
		break;
	case TidyNode_Php:
		name = "PHP";
		break;
	case TidyNode_XmlDecl:
		name = "XML Declaration";
		break;
	case TidyNode_Start:
	case TidyNode_End:
	case TidyNode_StartEnd:
	default:
		name = tidyNodeGetName(node);
		break;
	}
	assert(name != NULL);
	printf("Node(%d): %s {\n", level, ((char *)name));
/* the ifs could be combined with && */
	if (stringEqual(((char *)name), "Text")) {
		TidyBuffer tnv = { 0 };	/* text-node value */
		tidyBufClear(&tnv);
		tidyNodeGetValue(tdoc, node, &tnv);
		printf("Text: %s\n", tnv.bp);
		if (tnv.size)
			tidyBufFree(&tnv);
	}

/* Get the first attribute for the node */
	tattr = tidyAttrFirst(node);
	while (tattr != NULL) {
/* Print the node and its attribute */
		printf("@%s = %s\n", tidyAttrName(tattr), tidyAttrValue(tattr));
/* Get the next attribute */
		tattr = tidyAttrNext(tattr);
	}
}				/* printNode */

/* remove tags from start and end of a string, for innerHTML */
static void tagStrip(char *line)
{
	char *s = line;
	if (*s != '<') {
/* this shouldn't happen, don't know what to do. */
		return;
	}

	s = strchr(line, '>');
	if (!s)
		return;
	++s;
	while (isspace(*s))
		++s;
	strmove(line, s);

	s = line + strlen(line);
	while (s > line && isspace(s[-1]))
		--s;
	*s = 0;
	if (s == line || s[-1] != '>')
		return;
/* back up over </foo> */
	--s;
	while (s >= line) {
		if (*s == '<') {
			*s = 0;
			return;
		}
		--s;
	}
}				/* tagStrip */

static void convertNode(TidyNode node, int level, bool opentag)
{
	ctmbstr name;
	TidyAttr tattr;
	struct htmlTag *t;
	int nattr;		/* number of attributes */
	int i;

	switch (tidyNodeGetType(node)) {
	case TidyNode_Text:
		name = "Text";
		break;
	case TidyNode_Start:
	case TidyNode_End:
	case TidyNode_StartEnd:
		name = tidyNodeGetName(node);
		break;
	default:
		return;
	}

	t = newTag((char *)name);
	if (!t)
		return;

	if (!opentag) {
		t->slash = true;
		return;
	}

/* if a js script, remember the line number for error messages */
	if (t->action == TAGACT_SCRIPT)
		t->js_ln = tidyNodeLine(node);

/* this is the open tag, set the attributes */
/* special case for text tag */
	if (t->action == TAGACT_TEXT) {
		TidyBuffer tnv = { 0 };	/* text-node value */
		tidyBufClear(&tnv);
		tidyNodeGetValue(tdoc, node, &tnv);
		if (tnv.size) {
			t->textval = cloneString(tnv.bp);
			tidyBufFree(&tnv);
		}
	}

	nattr = 0;
	tattr = tidyAttrFirst(node);
	while (tattr != NULL) {
		++nattr;
		tattr = tidyAttrNext(tattr);
	}

	t->attributes = allocMem(sizeof(char *) * (nattr + 1));
	t->atvals = allocMem(sizeof(char *) * (nattr + 1));
	i = 0;
	tattr = tidyAttrFirst(node);
	while (tattr != NULL) {
		t->attributes[i] = cloneString(tidyAttrName(tattr));
		t->atvals[i] = cloneString(tidyAttrValue(tattr));
		++i;
		tattr = tidyAttrNext(tattr);
	}
	t->attributes[i] = 0;
	t->atvals[i] = 0;

/* innerHTML, only for certain tags */
	if (t->action == TAGACT_DIV) {
		TidyBuffer tnv = { 0 };	/* text-node value */
		tidyBufClear(&tnv);
		t->innerHTML = emptyString;
		tidyNodeGetText(tdoc, node, &tnv);
		if (tnv.size) {
/* But it's not the original html, it has been sanitized.
 * Put a cap on size, else memory consumed could, theoretically,
 * grow as the size of the document squared. */
			if (tnv.size <= 4096)
				t->innerHTML = cloneString(tnv.bp);
			tagStrip(t->innerHTML);
			tidyBufFree(&tnv);
		}
	}

}				/* convertNode */
