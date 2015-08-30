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

static void tidyDumpBody(void);
static void tidyDumpNode(TidyNode tnod, int indent);

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
	if (debugLevel >= 5)
		tidyDumpBody();

/* loop through tidy nodes and build our nodes, not yet implemented */
	*num_nodes = 0;
	*nodelist = 0;

	tidyRelease(tdoc);
}				/* html2nodes */

static void tidyDumpBody(void)
{
/* just for debugging - we only reach this routine at db5 or above */
	tidyDumpNode(tidyGetBody(tdoc), 0);
}

static void tidyDumpNode(TidyNode tnod, int indent)
{
/* just for debugging - we only reach this routine at db5 or above */
	TidyNode child;
	TidyBuffer tnv = { 0 };	/* text-node value */
	for (child = tidyGetChild(tnod); child; child = tidyGetNext(child)) {
		ctmbstr name;
		tidyBufClear(&tnv);
		switch (tidyNodeGetType(child)) {
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
			name = tidyNodeGetName(child);
			break;
		}
		assert(name != NULL);
		printf("Node(%d): %s\n", (indent / 4), ((char *)name));
		if (debugLevel >= 6) {
/* the ifs could be combined with && */
			if (stringEqual(((char *)name), "Text")) {
				tidyNodeGetValue(tdoc, child, &tnv);
				printf("Text: %s", tnv.bp);
/* no trailing newline because it appears that there already is one */
			}
		}

/* Get the first attribute for the node */
		TidyAttr tattr = tidyAttrFirst(child);
		while (tattr != NULL) {
/* Print the node and its attribute */
			printf("%s = %s\n", tidyAttrName(tattr),
			       tidyAttrValue(tattr));
/* Get the next attribute */
			tattr = tidyAttrNext(tattr);
		}
		tidyDumpNode(child, indent + 4);
	}
	if (tnv.size > 0) {
		tidyBufFree(&tnv);
	}
}				/* tidyDumpNode */
