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
typedef void (*nodeFunction) (TidyNode node, int level);
static nodeFunction traverse_callback;
/* possible callback functions */
static void printNode(TidyNode node, int level);

static void traverseNode(TidyNode node, int level)
{
	TidyNode child;

/* first the callback function */
	(*traverse_callback) (node, level);

/* and now the children */
	for (child = tidyGetChild(node); child; child = tidyGetNext(child))
		traverseNode(child, level + 1);
}				/* traverseNode */

static void traverseBody(void)
{
	traverseNode(tidyGetBody(tdoc), 0);
}				/* traverseBody */

static void traverseHead(void)
{
	traverseNode(tidyGetHead(tdoc), 0);
}				/* traverseHead */

/* Like the default tidy error reporter, except messages are suppressed
 * unless debugLevel >= 3, and they are sent to stdout
 * rather than stderr, like most edbrowse messages.
 * Since these are debug messages, they are not internationalized. */

static Bool tidyErrorHandler(TidyDoc tdoc, TidyReportLevel lvl,
			     uint line, uint col, ctmbstr mssg)
{
	if (debugLevel >= 3)
		printf("line %d column %d: %s\n", line, col, mssg);
	return no;
}				/* tidyErrorHandler */

/* the entry point */
void html2nodes(const char *htmltext,
/* result parameters */
		int *num_nodes, struct htmlTag **nodelist)
{
	tdoc = tidyCreate();
	tidySetReportFilter(tdoc, tidyErrorHandler);
	tidySetCharEncoding(tdoc, (cons_utf8 ? "utf8" : "latin1"));

	tidyParseString(tdoc, htmltext);
	tidyCleanAndRepair(tdoc);
	if (debugLevel >= 5) {
		traverse_callback = printNode;
		traverseHead();
		traverseBody();
	}

/* loop through tidy nodes and build our nodes, not yet implemented */
	*num_nodes = 0;
	*nodelist = 0;

	tidyRelease(tdoc);
}				/* html2nodes */

/* this is strictly for debugging, level >= 5 */
static void printNode(TidyNode node, int level)
{
	ctmbstr name;

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
	printf("Node(%d): %s\n", level, ((char *)name));
	if (debugLevel >= 6) {
/* the ifs could be combined with && */
		if (stringEqual(((char *)name), "Text")) {
			TidyBuffer tnv = { 0 };	/* text-node value */
			tidyBufClear(&tnv);
			tidyNodeGetValue(tdoc, node, &tnv);
			printf("Text: %s\n", tnv.bp);
			if (tnv.size > 0)
				tidyBufFree(&tnv);
		}
	}

/* Get the first attribute for the node */
	TidyAttr tattr = tidyAttrFirst(node);
	while (tattr != NULL) {
/* Print the node and its attribute */
		printf("@%s = %s\n", tidyAttrName(tattr), tidyAttrValue(tattr));
/* Get the next attribute */
		tattr = tidyAttrNext(tattr);
	}
}				/* printNode */
