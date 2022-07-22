/*********************************************************************
html-tags.c: parse the html tags and attributes.
This was originally handled by the tidy library, but tidy is no longer maintained.
Bugs are accumulating, and we continue to work around those bugs,
in html-tidy.c and in decorate.c, but we can't continue along this path.
Thus I wrote this html tag scanner.
I believe it is easier, in the long run, to roll our own, rather than using
tidy, or any other library.
Note that we can paste the tags directly into the edbrowse tree;
any other library, including tidy, builds its own tree of nodes,
which we must then import into the edbrowse tree of tags.
*********************************************************************/

#include "eb.h"

bool dhs; // debug html scanner
// This makes a lot of output; maybe it should go to a file like debug css does

static void findAttributes(const char *start, const char *end);
static void setAttribute(const char *a1, const char *a2, const char *v1, const char *v2);
static char *pullAnd(const char *start, const char *end);
static unsigned andLookup(char *entity, char *v);
static void pushState(const char *start, bool head_ok);

static Tag *working_t;
static int ln; // line number
// 0 prehtml 1 prehead, 2 inhead, 3 posthead, 4 inbody, 5 postbody 6 posthtml
static int headbody;
static bool premode;

// compress whitespace
static void compress(char *s)
{
	int i, j;
	char c;
	bool space = false;
	for (i = j = 0; (c = s[i]); ++i) {
		if (isspaceByte(c)) {
			if (!space)
				s[j++] = ' ', space = true;
			continue;
		}
		s[j++] = c, space = false;
	}
	s[j] = 0;
}

static struct opentag {
	struct opentag *next;
	char name[MAXTAGNAME];
	const char *start; // for innerHTML
	Tag *t;
} *stack;
static bool lastPush;

static struct opentag *balance(const char *name)
{
	struct opentag *k = stack;
	while(k) {
		if(stringEqualCI(name, k->name)) return k;
		k = k->next;
	}
	return 0;
}

static const struct specialtag {
	const char *name;
	bool autoclose, nestable, inhead;
	const char *second;
	} specialtags[] = {
{"script", 0, 0, 1, 0},
{"style", 0, 0, 1, 0},
{"meta", 1, 0, 1, 0},
{"bgsound", 1, 0, 1, 0},
{"title", 0, 0, 1, 0},
{"base", 1, 0, 1, 0},
{"link", 1, 0, 1, 0},
{"br", 1, 0, 0, 0},
{"hr", 1, 0, 0, 0},
{"img", 1, 0, 0, 0},
{"area", 1, 0, 0, 0},
{"image", 1, 0, 0, 0},
{"input", 1, 0, 0, 0},
{"ul",0,1, 0, 0},
{"ol",0,1, 0, 0},
{"li",0,0, 0, "ul,ol"},
{"dl",0,1, 0, 0},
{"dt",0,0, 0, "dl"},
{"table",0,1, 0, 0},
{"td",0,0, 0, "table"},
{"th",0,0, 0, "table"},
{"tr",0,0, 0, "table"},
{"thead",0,0, 0, "table"},
{"tbody",0,0, 0, "table"},
{"tfoot",0,0, 0, "table"},
{"div",0,1, 0, 0},
{"font",0,1, 0, 0},
{"span",0,1, 0, 0},
{0, 0,0,0, 0},
};

static int isAutoclose(const char *name)
{
	const struct specialtag *y;
	for(y = specialtags; y->name; ++y)
		if(stringEqualCI(name, y->name))
			return y->autoclose;
	return false;
}

static int isInhead(const char *name)
{
	const struct specialtag *y;
	for(y = specialtags; y->name; ++y)
		if(stringEqualCI(name, y->name))
			return y->inhead;
	return false;
}

static int isNextclose(const char *name)
{
// there are so few of these; may as well just make a list
	static const char * const list[] = {"title", "option", 0};
	return stringInListCI(list, name) >= 0;
}

static int isCrossclose(const char *name)
{
	static const char * const list[] = {"h1","h2","h3","h4","h5","h6","p",0};
	return stringInListCI(list, name) >= 0;
}

static int isCrossclose2(const char *name)
{
	static const char * const list[] = {"h1","h2","h3","h4","h5","h6","p","table","ul","ol","dl","hr",0};
	return stringInListCI(list, name) >= 0;
}

// space after these tags isn't significant
static int isWall(const char *name)
{
	static const char * const list[] = {"body","h1","h2","h3","h4","h5","h6","p","table","tr","td","th","ul","ol","dl","li","dt","div","br","hr","iframe",0};
	if(!lastPush) return false;
	return stringInListCI(list, name) >= 0;
}


static int isNonest(const char *name, const struct opentag *k)
{
	const struct specialtag *y;
	const char *s, *t;
	char watch[MAXTAGNAME];
	const struct opentag *l;
	for(y = specialtags; y->name; ++y)
		if(stringEqualCI(name, y->name))
			break;
	if(!y->name) return true;
	if(y->nestable) return false;
// td can be inside td, if there is table in between,
// second indicates this in-between tag
	if(!(s = y->second)) return true;
	while(*s) {
		t = strchr(s, ',');
		if(!t) t = s + strlen(s);
		strncpy(watch, s, t-s);
		watch[t - s] = 0;
		for(l = stack; l != k; l = l->next)
			if(stringEqualCI(l->name, watch)) return false;
		s = t;
		if(*s == ',') ++s;
	}
	return true;
}

// generate a tag using newTag, which does most of the work
static void makeTag(const char *name, bool slash, const char *mark)
{
	Tag *t;
	struct opentag *k;

	if(slash) {
		if(!(k = balance(name))) {
// </foo> without <foo>, may as well just throw it away.
			if(dhs) puts("unbalanced");
			return;
		}
// now handle <i><b></i></b>
// the balancing tag is not at the top of the stack, where it should be!
// I think the best thing is to close out the other tags.
		while(stack != k) {
			if(dhs) printf("force closure of %s\n", stack->name);
			makeTag(stack->name, true, mark);
		}
	}

	working_t = t = newTag(cf, name);
	t->slash = slash;

	if(!slash) {
		k = allocMem(sizeof(struct opentag));
		strcpy(k->name, name);
		k->t = t;
		k->start = mark;
		k->next = stack, stack = k;
		if(stringEqualCI(name, "html")) {
			headbody = 1;
			if(dhs) puts("in html");
		}
		if(stringEqualCI(name, "head")) {
			headbody = 2;
			if(dhs) puts("in head");
		}
		if(stringEqualCI(name, "body")) {
			headbody = 4;
			if(dhs) puts("in body");
		}
		if(stringEqualCI(name, "pre")) {
			premode = true;
			if(dhs) puts("pre");
		}
	} else {
		if(stringEqualCI(name, "head")) {
			headbody = 3;
			if(dhs) puts("post head");
		}
		if(stringEqualCI(name, "body")) {
			headbody = 5;
			if(dhs) puts("post body");
		}
		if(stringEqualCI(name, "html")) {
			headbody = 6;
			if(dhs) puts("post html");
		}
		if(stringEqualCI(name, "pre")) {
			premode = false;
			if(dhs) puts("not pre");
		}
		stack = k->next;
// set up for innerHTML
		if(k->t->info->bits & TAG_INNERHTML && k->start && mark)
			k->t->innerHTML = pullString(k->start, mark - k->start);
		free(k);
	}
}

void html2tags(const char *htmltext, bool startpage)
{
	int i;
	const char *lt; // les than sign
	const char *gt; // greater than sign
	const char *seek, *s, *t, *u;
	char *w;
	bool slash; // </foo>
	bool ws; // all whitespace
	char qc; // quote character
	char tagname[MAXTAGNAME];
	const struct opentag *k;

	seek = s = htmltext, ln = 1, premode = false, headbody = 0;
	stack = 0;
	lastPush = 0;

// loop looking for tags
	while(*s) {
// the next literal < should begin the next tag
// text like x<y is invalid; you should be using &lt; in this case
		if(!(lt = strchr(s, '<')))
			break;
		slash = false, t = lt + 1;
		if(*t == '/') ++t, slash = true;

// bare < just passes through
		if((!slash && *t != '!' && !isalpha(*t) && !memEqualCI(t, "?xml", 4)) ||
		(slash && !isalpha(*t))) {
			s = t + 1;
			continue;
		}

// text fragment between tags
		if(lt > seek) {
// adjust line number
			for(ws = true, u = seek; u < lt; ++u) {
				if(*u == '\n') ++ln;
				if(!isspace(*u)) ws = false;
			}
// Ignore whitespace that is not in the head or the body.
// Ignore text after body
			if(headbody < 5 && (!ws || (headbody == 4 && !isWall(stack->name)))) {
				pushState(seek, true);
				w = pullAnd(seek, lt);
				if(!premode) compress(w);
				  if(dhs) printf("text{%s}\n", w);
				makeTag("text", false, 0);
				working_t->textval = w;
				makeTag("text", true, 0);
			}
		}

// special code here for html comment
		if(*t == '!') {
			int hyphens = 0;
			++t;
			while(t[0] == '-' && t[1] == '-')
				t += 2, hyphens += 2;
			u = t;
//   <!------> is not self contained
			if(*u == '>') ++u;
closecomment:
			u = strchr(u, '>');
			if(!u) goto opencomment;
			for(i = 1; i <= hyphens; ++i)
				if(u[-i] != '-') break;
			if(i < hyphens) { ++u; goto closecomment; }
// this is a valid comment
			if(dhs) puts("comment");
// adjust line number
			for(t = lt; t < u; ++t)
				if(*t == '\n') ++ln;
			seek = s = u + 1;
// see if this is doctype
			t = lt + 2 + hyphens;
			if(headbody == 0 && memEqualCI(t, "doctype", 7) &&
			!isalnum(t[7])) {
				if(dhs) puts("doctype");
				makeTag("doctype", false, 0);
				makeTag("doctype", true, 0);
			}
			continue;
opencomment:
			if(dhs) printf("open comment at line %d, html parsing stops here\n", ln);
			goto stop;
		}

// xml specifier - I don't really understand this.
		if(memEqualCI(t, "?xml", 4)) {
			if(!(gt = strstr(t, "?>"))) {
				if(dhs) puts("open xml");
				s = t;
				continue;
			}
			if(dhs) puts("xml");
			s = seek = gt + 2;
			continue;
		}

// at this point it is <tag or </tag
		i = 0;
		while(isalnum(*t)) {
			if(i < MAXTAGNAME - 1) tagname[i++] = *t;
			++t;
		}
		tagname[i] = 0;
// tidy converts all tags to lower case; I will do the same
		caseShift(tagname, 'l');

/*********************************************************************
the next > should close the tag,
		gt = strpbrk(t, "<>");
but watch out. Look at acid3.
Apparently this crap is legal   <area alt="<'>">
You should be using &gt; but I guess when quoted like this, it flies.
And tidy is ok with it, not even a warning.
So I have to step along and watch for quotes.
But then I get this from usps.gov:
<table role="presentation" width="100%"         style="padding-bottom: 30px; max-width: 820px!important;>
Unbalanced quotes, bad html, somehow tidy knows what to do with it.
I haven't given the tidy team enough credit.
So my compromise is to take the first literal > if the quoted
string that contains the > would run past end of line.
That makes acid3 and usps.gov happy.
But some web pages are all on a line to save bits,
so also break out at >< like a new tag is starting.
*********************************************************************/

		for(gt = t, qc = 0; *gt; ++gt) {
			if(qc) {
				if(qc == *gt) qc = 0; // unquote
				if(*gt == '>' && !u) u = gt;
				if(*gt == '\n' && u) { gt = u; break; }
				if(*gt == '>' && gt[1] == '<' && u) { gt = u; break; }
				continue;
			}
			if(*gt == '<' || *gt == '>') break;
			if(*gt == '"' || *gt == '\'') qc = *gt, u = 0;
		}

		if(!gt) {
			printf("open tag %s, html parsing stops here\n", tagname);
			goto stop;
		}
// adjust line number for this tag
		for(u = lt; u < gt; ++u)
			if(*u == '\n') ++ln;
		if(*(seek = gt) == '>') ++seek;
		s = seek; // ready to march on

		if(slash) {
// close the corresponging open tag. If none found then discard this one.
// create this tag in the edbrowse world.
			if(dhs) printf("</%s>\n", tagname);
			makeTag(tagname, true, lt);
			lastPush = false;
			continue;
		}

// create this tag in the edbrowse world.
		if(dhs) printf("<%s> line %d\n", tagname, ln);
// has to start and end with html
		if(stringEqualCI(tagname, "html")) {
			if(headbody > 0) { if(dhs) puts("sequence"); continue; }
			goto tag_ok;
		}
		if(headbody == 0)
			makeTag("html", false, lt);
		if(stringEqualCI(tagname, "head")) {
			if(headbody > 1) { if(dhs) puts("sequence"); continue; }
			goto tag_ok;
		}
		if(!isInhead(tagname)) {
			if(headbody == 1) {
				if(dhs) puts("initiate and terminate head");
				makeTag("head", false, lt);
				makeTag("head", true, lt);
			} else if(headbody == 2) {
				if(dhs) puts("terminate head");
				makeTag("head", true, lt);
			}
		} else pushState(lt, true);
		if(stringEqualCI(tagname, "body")) {
			if(headbody > 3) { if(dhs) puts("sequence"); continue; }
			goto tag_ok;
		}
		if(headbody == 3) pushState(lt, false);
tag_ok:
//  see if we need to close a prior instance of this tag
		k = balance(tagname);
		if(k && isNonest(tagname, k)) {
			if(dhs) puts("not nestable");
			makeTag(tagname, true, lt);
		}
		if(stack && isNextclose(stack->name)) {
			if(dhs) printf("prior close %s\n", stack->name);
			makeTag(stack->name, true, lt);
		}
		if(stack && isCrossclose2(tagname)) {
			for(k = stack; k; k = k->next) {
				if(isCrossclose(k->name)) {
					if(dhs) printf("cross close %s\n", k->name);
					makeTag(k->name, true, lt);
				}
			}
		}
		makeTag(tagname, false, seek);
		lastPush = true;
		findAttributes(t, gt);
		if(isAutoclose(tagname)) {
			if(dhs) puts("autoclose");
			makeTag(tagname, true, seek);
		}

		if(stringEqualCI(tagname, "script")) {
// remember the line number for error messages
			working_t->js_ln = ln;
/*********************************************************************
A script, javascript or otherwise, can contain virtually any string.
We can't expect an html scanner, like this one, to understand
the syntax of every scripting language that might ever embed.
To this end, constructs like this are invalid.
	var foo = "hello </script> world";
Such has to be written
	var foo = "hello </scr" + "ipt> world";
With this understanding, we can, and should, scan for </script
*********************************************************************/
			if(!(lt = strcasestr(seek, "</script"))) {
				if(dhs) printf("open script at line %d, html parsing stops here\n", ln);
				goto stop;
			}
			if(!(gt = strpbrk(lt + 1, "<>")) || *gt == '<') {
				if(dhs) printf("open script at line %d, html parsing stops here\n", ln);
				goto stop;
			}
// adjust line number
			for(u = seek; u < gt; ++u)
				if(*u == '\n') ++ln;
			while(isspace(*seek)) ++seek;
			   if(dhs) printf("script length %d\n", lt - seek);
			if(lt > seek) {
// pull out the script, do not andify or change in any way.
				w = pullString(seek, lt - seek);
				makeTag("text", false, 0);
				working_t->textval = w;
				makeTag("text", true, 0);
			}
			if(dhs) puts("</script>");
			makeTag(tagname, true, lt);
			seek = s = gt + 1;
			continue;
		}

		if(stringEqualCI(tagname, "style")) {
// this is like script; leave it alone!
			if(!(lt = strcasestr(seek, "</style"))) {
				if(dhs) printf("open style at line %d, html parsing stops here\n", ln);
				goto stop;
			}
			if(!(gt = strpbrk(lt + 1, "<>")) || *gt == '<') {
				if(dhs) printf("open style at line %d, html parsing stops here\n", ln);
				goto stop;
			}
// adjust line number
			for(u = seek; u < gt; ++u)
				if(*u == '\n') ++ln;
			while(isspace(*seek)) ++seek;
			   if(dhs) printf("style length %d\n", lt - seek);
			if(lt > seek) {
// pull out the style, do not andify or change in any way.
				w = pullString(seek, lt - seek);
				makeTag("text", false, 0);
				working_t->textval = w;
				makeTag("text", true, 0);
			}
			if(dhs) puts("</style>");
			makeTag(tagname, true, lt);
			seek = s = gt + 1;
			continue;
		}

		if(stringEqualCI(tagname, "textarea")) {
/*********************************************************************
A textarea can contain tags and these are not to be interpreted. In fact a
textarea is sometimes html code that you are suppose to embed in your web page.
With this understanding, we can, and should, scan for </textarea
*********************************************************************/
			if(!(lt = strcasestr(seek, "</textarea"))) {
				if(dhs) printf("open textarea at line %d, html parsing stops here\n", ln);
				goto stop;
			}
			if(!(gt = strpbrk(lt + 1, "<>")) || *gt == '<') {
				if(dhs) printf("open textarea at line %d, html parsing stops here\n", ln);
				goto stop;
			}
// adjust line number
			for(u = seek; u < gt; ++u)
				if(*u == '\n') ++ln;
			while(isspace(*seek)) ++seek; // should we be doing this?
			if(lt > seek) {
// pull out the text and andify.
				w = pullAnd(seek, lt);
				   if(dhs) printf("textarea length %d\n", strlen(w));
				makeTag("text", false, 0);
				working_t->textval = w;
				makeTag("text", true, 0);
			}
			if(dhs) puts("</textarea>");
			makeTag(tagname, true, lt);
			seek = s = gt + 1;
			continue;
		}
	}

// seek points to the last piece of the buffer, after the last tag
	if(*seek) {
		for(ws = true, u = seek; *u; ++u) {
			if(*u == '\n') ++ln;
			if(!isspace(*u)) ws = false;
		}
		if(headbody < 5 && (!ws || headbody == 4)) {
			pushState(seek, true);
			w = pullAnd(seek, seek + strlen(seek));
			if(!premode) compress(w);
			  if(dhs) printf("text{%s}\n", w);
			makeTag("text", false, 0);
			working_t->textval = w;
			makeTag("text", true, 0);
		}
	}

stop:
// we should have a head and a body
	pushState(s, false);

// has to start and end with html
if(headbody < 6)
		makeTag("html", true, s);

	if(stack)
		debugPrint(1, "stack not empty after html scan");
}

static void pushState(const char *start, bool head_ok)
{
	if(headbody == 0) {
		makeTag("html", false, start);
	}
	if(headbody == 1) {
		if(dhs) puts("initiate head");
		makeTag("head", false, start);
	}
	if(headbody == 2 && !head_ok) {
		if(dhs) puts("terminate head");
		makeTag("head", true, start);
	}
	if(headbody == 3) {
		if(dhs) puts("initiate body");
		makeTag("body", false, start);
	}
}

static void findAttributes(const char *start, const char *end)
{
	const char *s = start;
	char qc; // quote character
	const char *a1, *a2; // attribute name
	const char *v1, *v2; // attribute value

	while(s < end) {
// look for a C identifier, then whitespace, then =
		if(!isalpha(*s)) { ++s; continue; }
		a1 = s; // start of attribute
		a2 = a1 + 1;
		while(*a2 == '_' || *a2 == '-' || isalnum(*a2)) ++a2;
		for(s = a2; isspace(*s); ++s)  ;
		if(*s != '=' || s == end) {
// it could be an attribute with no value, but then we need whitespace
			if(s > a2 || s == end)
				setAttribute(a1, a2, a2, a2);
			continue;
		}
		for(v1 = s + 1; isspace(*v1); ++v1)  ;
		qc = 0;
		if(*v1 == '"' || *v1 == '\'') qc = *v1++;
		for(v2 = v1; v2 < end; ++v2)
			if((!qc && isspace(*v2)) || (qc && *v2 == qc)) break;
		setAttribute(a1, a2, v1, v2);
		if(*v2 == qc) ++v2;
		s = v2;
	}
}

static void setAttribute(const char *a1, const char *a2, const char *v1, const char *v2)
{
	char *w;
	char save_c;
	w = pullAnd(v1, v2);
// yeah this is tacky, write on top of a const, but I'll put it back.
	save_c = *a2, *(char*)a2 = 0;
	if(dhs && debugLevel >= 3) printf("%s=%s\n", a1, w);
	setTagAttr(working_t, a1, w);
	*(char*)a2 = save_c;
}

// make an allocated copy of the designated string,
// then decode the & fragments.

static char *pullAnd(const char *start, const char *end)
{
	char *s, *t;
	unsigned u; // unicode
	char *u8;
	char *entity;
	int l = end - start;
	char *w = pullString(start, l);

// the assumption here is that &stuff always encodes to something smaller
// when represented as utf8.
// Example: &pi; is pretty short, but the utf8 for pi is 3 bytes, so we're good.

	for(s = t = w; *s; ++s) {
		if(*s != '&') goto putc;
		if(s[1] == '#' && (isdigit(s[2]) || ((s[2] == 'x' || s[2] == 'X') && isxdigit(s[3])))) {
			if(isdigit(s[2])) u = strtol(s+2, &s, 10);
			else u = strtol(s+3, &s, 16);
putuni:
// tidy issues a warning if no ; but tolerates it
			if(*s != ';') --s;
// nulls are not allowed, nor the low bytes as edbrowse uses them internally.
			if(u <= 4) u = ' ';
			u8 = uni2utf8(u);
			strcpy(t, u8);
			t += strlen(t);
			continue;
		}
		if(!isalpha(s[1])) goto putc;
// there is an identifier after &
		entity = ++s;
		for(++s; isalnum(*s); ++s)  ;
		u = andLookup(entity, s);
		if(u) goto putuni;
// word not recognized, just skip it
		if(*s != ';') --s;
		continue;
putc:
		*t++ = *s;
	}
	*t = 0;

	return w;
}

// entity words and codes taken from
// https://www.w3schools.com/charsets/ref_html_entities_4.asp
// then sorted for binary search.
static const struct entity { unsigned int u; const char *word; } andlist[] = {
{198, "AElig"},
{193, "Aacute"},
{258, "Abreve"},
{194, "Acirc"},
{1040, "Acy"},
{120068, "Afr"},
{192, "Agrave"},
{913, "Alpha"},
{256, "Amacr"},
{10835, "And"},
{260, "Aogon"},
{120120, "Aopf"},
{8289, "ApplyFunction"},
{197, "Aring"},
{119964, "Ascr"},
{8788, "Assign"},
{195, "Atilde"},
{196, "Auml"},
{8726, "Backslash"},
{10983, "Barv"},
{1041, "Bcy"},
{8492, "Bernoullis"},
{914, "Beta"},
{120069, "Bfr"},
{120121, "Bopf"},
{728, "Breve"},
{8492, "Bscr"},
{8782, "Bumpeq"},
{1063, "CHcy"},
{262, "Cacute"},
{8914, "Cap"},
{8517, "CapitalDifferentialD"},
{8493, "Cayleys"},
{268, "Ccaron"},
{199, "Ccedil"},
{264, "Ccirc"},
{8752, "Cconint"},
{266, "Cdot"},
{184, "Cedilla"},
{183, "CenterDot"},
{8493, "Cfr"},
{935, "Chi"},
{8857, "CircleDot"},
{8854, "CircleMinus"},
{8853, "CirclePlus"},
{8855, "CircleTimes"},
{8754, "ClockwiseContourIntegral"},
{8221, "CloseCurlyDoubleQuote"},
{8217, "CloseCurlyQuote"},
{8759, "Colon"},
{10868, "Colone"},
{8801, "Congruent"},
{8751, "Conint"},
{8750, "ContourIntegral"},
{8450, "Copf"},
{8720, "Coproduct"},
{10799, "Cross"},
{119966, "Cscr"},
{8915, "Cup"},
{8781, "CupCap"},
{8517, "DD"},
{10513, "DDotrahd"},
{1026, "DJcy"},
{1029, "DScy"},
{1039, "DZcy"},
{8609, "Darr"},
{10980, "Dashv"},
{270, "Dcaron"},
{1044, "Dcy"},
{8711, "Del"},
{916, "Delta"},
{120071, "Dfr"},
{180, "DiacriticalAcute"},
{729, "DiacriticalDot"},
{733, "DiacriticalDoubleAcute"},
{96, "DiacriticalGrave"},
{732, "DiacriticalTilde"},
{8900, "Diamond"},
{8518, "DifferentialD"},
{120123, "Dopf"},
{168, "Dot"},
{8412, "DotDot"},
{8784, "DotEqual"},
{8751, "DoubleContourIntegral"},
{168, "DoubleDot"},
{8659, "DoubleDownArrow"},
{8656, "DoubleLeftArrow"},
{8660, "DoubleLeftRightArrow"},
{10980, "DoubleLeftTee"},
{10232, "DoubleLongLeftArrow"},
{10234, "DoubleLongLeftRightArrow"},
{10233, "DoubleLongRightArrow"},
{8658, "DoubleRightArrow"},
{8872, "DoubleRightTee"},
{8657, "DoubleUpArrow"},
{8661, "DoubleUpDownArrow"},
{8741, "DoubleVerticalBar"},
{8595, "DownArrow"},
{10515, "DownArrowBar"},
{8693, "DownArrowUpArrow"},
{785, "DownBreve"},
{10576, "DownLeftRightVector"},
{10590, "DownLeftTeeVector"},
{8637, "DownLeftVector"},
{10582, "DownLeftVectorBar"},
{10591, "DownRightTeeVector"},
{8641, "DownRightVector"},
{10583, "DownRightVectorBar"},
{8868, "DownTee"},
{8615, "DownTeeArrow"},
{8659, "Downarrow"},
{119967, "Dscr"},
{272, "Dstrok"},
{330, "ENG"},
{208, "ETH"},
{201, "Eacute"},
{282, "Ecaron"},
{202, "Ecirc"},
{1069, "Ecy"},
{278, "Edot"},
{120072, "Efr"},
{200, "Egrave"},
{8712, "Element"},
{274, "Emacr"},
{9723, "EmptySmallSquare"},
{9643, "EmptyVerySmallSquare"},
{280, "Eogon"},
{120124, "Eopf"},
{917, "Epsilon"},
{10869, "Equal"},
{8770, "EqualTilde"},
{8652, "Equilibrium"},
{8496, "Escr"},
{10867, "Esim"},
{919, "Eta"},
{203, "Euml"},
{8707, "Exists"},
{8519, "ExponentialE"},
{1060, "Fcy"},
{120073, "Ffr"},
{9724, "FilledSmallSquare"},
{9642, "FilledVerySmallSquare"},
{120125, "Fopf"},
{8704, "ForAll"},
{8497, "Fouriertrf"},
{8497, "Fscr"},
{1027, "GJcy"},
{62, "GT"},
{915, "Gamma"},
{988, "Gammad"},
{286, "Gbreve"},
{290, "Gcedil"},
{284, "Gcirc"},
{1043, "Gcy"},
{288, "Gdot"},
{120074, "Gfr"},
{8921, "Gg"},
{120126, "Gopf"},
{8805, "GreaterEqual"},
{8923, "GreaterEqualLess"},
{8807, "GreaterFullEqual"},
{10914, "GreaterGreater"},
{8823, "GreaterLess"},
{10878, "GreaterSlantEqual"},
{8819, "GreaterTilde"},
{119970, "Gscr"},
{8811, "Gt"},
{1066, "HARDcy"},
{711, "Hacek"},
{94, "Hat"},
{292, "Hcirc"},
{8460, "Hfr"},
{8459, "HilbertSpace"},
{8461, "Hopf"},
{9472, "HorizontalLine"},
{8459, "Hscr"},
{294, "Hstrok"},
{8782, "HumpDownHump"},
{8783, "HumpEqual"},
{1045, "IEcy"},
{306, "IJlig"},
{1025, "IOcy"},
{205, "Iacute"},
{206, "Icirc"},
{1048, "Icy"},
{304, "Idot"},
{8465, "Ifr"},
{204, "Igrave"},
{8465, "Im"},
{298, "Imacr"},
{8520, "ImaginaryI"},
{8658, "Implies"},
{8748, "Int"},
{8747, "Integral"},
{8898, "Intersection"},
{8291, "InvisibleComma"},
{8290, "InvisibleTimes"},
{302, "Iogon"},
{120128, "Iopf"},
{921, "Iota"},
{8464, "Iscr"},
{296, "Itilde"},
{1030, "Iukcy"},
{207, "Iuml"},
{308, "Jcirc"},
{1049, "Jcy"},
{120077, "Jfr"},
{120129, "Jopf"},
{119973, "Jscr"},
{1032, "Jsercy"},
{1028, "Jukcy"},
{1061, "KHcy"},
{1036, "KJcy"},
{922, "Kappa"},
{310, "Kcedil"},
{1050, "Kcy"},
{120078, "Kfr"},
{120130, "Kopf"},
{119974, "Kscr"},
{1033, "LJcy"},
{313, "Lacute"},
{923, "Lambda"},
{10218, "Lang"},
{8466, "Laplacetrf"},
{8606, "Larr"},
{317, "Lcaron"},
{315, "Lcedil"},
{1051, "Lcy"},
{10216, "LeftAngleBracket"},
{8592, "LeftArrow"},
{8676, "LeftArrowBar"},
{8646, "LeftArrowRightArrow"},
{8968, "LeftCeiling"},
{10214, "LeftDoubleBracket"},
{10593, "LeftDownTeeVector"},
{8643, "LeftDownVector"},
{10585, "LeftDownVectorBar"},
{8970, "LeftFloor"},
{8596, "LeftRightArrow"},
{10574, "LeftRightVector"},
{8867, "LeftTee"},
{8612, "LeftTeeArrow"},
{10586, "LeftTeeVector"},
{8882, "LeftTriangle"},
{10703, "LeftTriangleBar"},
{8884, "LeftTriangleEqual"},
{10577, "LeftUpDownVector"},
{10592, "LeftUpTeeVector"},
{8639, "LeftUpVector"},
{10584, "LeftUpVectorBar"},
{8636, "LeftVector"},
{10578, "LeftVectorBar"},
{8656, "Leftarrow"},
{8660, "Leftrightarrow"},
{8922, "LessEqualGreater"},
{8806, "LessFullEqual"},
{8822, "LessGreater"},
{10913, "LessLess"},
{10877, "LessSlantEqual"},
{8818, "LessTilde"},
{120079, "Lfr"},
{8920, "Ll"},
{8666, "Lleftarrow"},
{319, "Lmidot"},
{10229, "LongLeftArrow"},
{10231, "LongLeftRightArrow"},
{10230, "LongRightArrow"},
{10232, "Longleftarrow"},
{10234, "Longleftrightarrow"},
{10233, "Longrightarrow"},
{120131, "Lopf"},
{8601, "LowerLeftArrow"},
{8600, "LowerRightArrow"},
{8466, "Lscr"},
{8624, "Lsh"},
{321, "Lstrok"},
{8810, "Lt"},
{10501, "Map"},
{1052, "Mcy"},
{8287, "MediumSpace"},
{8499, "Mellintrf"},
{120080, "Mfr"},
{8723, "MinusPlus"},
{120132, "Mopf"},
{8499, "Mscr"},
{924, "Mu"},
{1034, "NJcy"},
{323, "Nacute"},
{327, "Ncaron"},
{325, "Ncedil"},
{1053, "Ncy"},
{8811, "NestedGreaterGreater"},
{8810, "NestedLessLess"},
{10, "NewLine"},
{120081, "Nfr"},
{8288, "NoBreak"},
{8469, "Nopf"},
{10988, "Not"},
{8802, "NotCongruent"},
{8813, "NotCupCap"},
{8742, "NotDoubleVerticalBar"},
{8713, "NotElement"},
{8800, "NotEqual"},
{8708, "NotExists"},
{8815, "NotGreater"},
{8817, "NotGreaterEqual"},
{8825, "NotGreaterLess"},
{8821, "NotGreaterTilde"},
{8938, "NotLeftTriangle"},
{8940, "NotLeftTriangleEqual"},
{8814, "NotLess"},
{8816, "NotLessEqual"},
{8824, "NotLessGreater"},
{8820, "NotLessTilde"},
{8832, "NotPrecedes"},
{8928, "NotPrecedesSlantEqual"},
{8716, "NotReverseElement"},
{8939, "NotRightTriangle"},
{8941, "NotRightTriangleEqual"},
{8930, "NotSquareSubsetEqual"},
{8931, "NotSquareSupersetEqual"},
{8840, "NotSubsetEqual"},
{8833, "NotSucceeds"},
{8929, "NotSucceedsSlantEqual"},
{8841, "NotSupersetEqual"},
{8769, "NotTilde"},
{8772, "NotTildeEqual"},
{8775, "NotTildeFullEqual"},
{8777, "NotTildeTilde"},
{8740, "NotVerticalBar"},
{119977, "Nscr"},
{209, "Ntilde"},
{925, "Nu"},
{338, "OElig"},
{211, "Oacute"},
{212, "Ocirc"},
{1054, "Ocy"},
{336, "Odblac"},
{120082, "Ofr"},
{210, "Ograve"},
{332, "Omacr"},
{937, "Omega"},
{927, "Omicron"},
{120134, "Oopf"},
{8220, "OpenCurlyDoubleQuote"},
{8216, "OpenCurlyQuote"},
{10836, "Or"},
{119978, "Oscr"},
{216, "Oslash"},
{213, "Otilde"},
{10807, "Otimes"},
{214, "Ouml"},
{8254, "OverBar"},
{9182, "OverBrace"},
{9140, "OverBracket"},
{9180, "OverParenthesis"},
{8706, "PartialD"},
{1055, "Pcy"},
{120083, "Pfr"},
{934, "Phi"},
{928, "Pi"},
{8460, "Poincareplane"},
{8473, "Popf"},
{10939, "Pr"},
{8826, "Precedes"},
{10927, "PrecedesEqual"},
{8828, "PrecedesSlantEqual"},
{8830, "PrecedesTilde"},
{8243, "Prime"},
{8719, "Product"},
{8759, "Proportion"},
{8733, "Proportional"},
{119979, "Pscr"},
{936, "Psi"},
{120084, "Qfr"},
{8474, "Qopf"},
{119980, "Qscr"},
{10512, "RBarr"},
{340, "Racute"},
{10219, "Rang"},
{8608, "Rarr"},
{10518, "Rarrtl"},
{344, "Rcaron"},
{342, "Rcedil"},
{1056, "Rcy"},
{8476, "Re"},
{8715, "ReverseElement"},
{8651, "ReverseEquilibrium"},
{10607, "ReverseUpEquilibrium"},
{8476, "Rfr"},
{929, "Rho"},
{10217, "RightAngleBracket"},
{8594, "RightArrow"},
{8677, "RightArrowBar"},
{8644, "RightArrowLeftArrow"},
{8969, "RightCeiling"},
{10215, "RightDoubleBracket"},
{10589, "RightDownTeeVector"},
{8642, "RightDownVector"},
{10581, "RightDownVectorBar"},
{8971, "RightFloor"},
{8866, "RightTee"},
{8614, "RightTeeArrow"},
{10587, "RightTeeVector"},
{8883, "RightTriangle"},
{10704, "RightTriangleBar"},
{8885, "RightTriangleEqual"},
{10575, "RightUpDownVector"},
{10588, "RightUpTeeVector"},
{8638, "RightUpVector"},
{10580, "RightUpVectorBar"},
{8640, "RightVector"},
{10579, "RightVectorBar"},
{8658, "Rightarrow"},
{8477, "Ropf"},
{10608, "RoundImplies"},
{8667, "Rrightarrow"},
{8475, "Rscr"},
{8625, "Rsh"},
{10740, "RuleDelayed"},
{1065, "SHCHcy"},
{1064, "SHcy"},
{1068, "SOFTcy"},
{346, "Sacute"},
{10940, "Sc"},
{352, "Scaron"},
{350, "Scedil"},
{348, "Scirc"},
{1057, "Scy"},
{120086, "Sfr"},
{8595, "ShortDownArrow"},
{8592, "ShortLeftArrow"},
{8594, "ShortRightArrow"},
{8593, "ShortUpArrow"},
{931, "Sigma"},
{8728, "SmallCircle"},
{120138, "Sopf"},
{8730, "Sqrt"},
{9633, "Square"},
{8851, "SquareIntersection"},
{8847, "SquareSubset"},
{8849, "SquareSubsetEqual"},
{8848, "SquareSuperset"},
{8850, "SquareSupersetEqual"},
{8852, "SquareUnion"},
{119982, "Sscr"},
{8902, "Star"},
{8912, "Sub"},
{8912, "Subset"},
{8838, "SubsetEqual"},
{8827, "Succeeds"},
{10928, "SucceedsEqual"},
{8829, "SucceedsSlantEqual"},
{8831, "SucceedsTilde"},
{8715, "SuchThat"},
{8721, "Sum"},
{8913, "Sup"},
{8835, "Superset"},
{8839, "SupersetEqual"},
{8913, "Supset"},
{222, "THORN"},
{8482, "TRADE"},
{1035, "TSHcy"},
{1062, "TScy"},
{9, "Tab"},
{932, "Tau"},
{356, "Tcaron"},
{354, "Tcedil"},
{1058, "Tcy"},
{120087, "Tfr"},
{8756, "Therefore"},
{920, "Theta"},
{8201, "ThinSpace"},
{8764, "Tilde"},
{8771, "TildeEqual"},
{8773, "TildeFullEqual"},
{8776, "TildeTilde"},
{120139, "Topf"},
{8411, "TripleDot"},
{119983, "Tscr"},
{358, "Tstrok"},
{218, "Uacute"},
{8607, "Uarr"},
{10569, "Uarrocir"},
{1038, "Ubrcy"},
{364, "Ubreve"},
{219, "Ucirc"},
{1059, "Ucy"},
{368, "Udblac"},
{120088, "Ufr"},
{217, "Ugrave"},
{362, "Umacr"},
{95, "UnderBar"},
{9183, "UnderBrace"},
{9141, "UnderBracket"},
{9181, "UnderParenthesis"},
{8899, "Union"},
{8846, "UnionPlus"},
{370, "Uogon"},
{120140, "Uopf"},
{8593, "UpArrow"},
{10514, "UpArrowBar"},
{8645, "UpArrowDownArrow"},
{8597, "UpDownArrow"},
{10606, "UpEquilibrium"},
{8869, "UpTee"},
{8613, "UpTeeArrow"},
{8657, "Uparrow"},
{8661, "Updownarrow"},
{8598, "UpperLeftArrow"},
{8599, "UpperRightArrow"},
{978, "Upsi"},
{933, "Upsilon"},
{366, "Uring"},
{119984, "Uscr"},
{360, "Utilde"},
{220, "Uuml"},
{8875, "VDash"},
{10987, "Vbar"},
{1042, "Vcy"},
{8873, "Vdash"},
{10982, "Vdashl"},
{8897, "Vee"},
{8214, "Verbar"},
{8214, "Vert"},
{8739, "VerticalBar"},
{10072, "VerticalSeparator"},
{8768, "VerticalTilde"},
{8202, "VeryThinSpace"},
{120089, "Vfr"},
{120141, "Vopf"},
{119985, "Vscr"},
{8874, "Vvdash"},
{372, "Wcirc"},
{8896, "Wedge"},
{120090, "Wfr"},
{120142, "Wopf"},
{119986, "Wscr"},
{120091, "Xfr"},
{926, "Xi"},
{120143, "Xopf"},
{119987, "Xscr"},
{1071, "YAcy"},
{1031, "YIcy"},
{1070, "YUcy"},
{221, "Yacute"},
{374, "Ycirc"},
{1067, "Ycy"},
{120092, "Yfr"},
{120144, "Yopf"},
{119988, "Yscr"},
{376, "Yuml"},
{1046, "ZHcy"},
{377, "Zacute"},
{381, "Zcaron"},
{1047, "Zcy"},
{379, "Zdot"},
{8203, "ZeroWidthSpace"},
{918, "Zeta"},
{8488, "Zfr"},
{8484, "Zopf"},
{119989, "Zscr"},
{225, "aacute"},
{259, "abreve"},
{8766, "ac"},
{8767, "acd"},
{226, "acirc"},
{180, "acute"},
{1072, "acy"},
{230, "aelig"},
{8289, "af"},
{120094, "afr"},
{224, "agrave"},
{8501, "alefsym"},
{8501, "aleph"},
{945, "alpha"},
{257, "amacr"},
{10815, "amalg"},
{38, "amp"},
{8743, "and"},
{10837, "andand"},
{10844, "andd"},
{10840, "andslope"},
{10842, "andv"},
{8736, "ang"},
{10660, "ange"},
{8736, "angle"},
{8737, "angmsd"},
{10664, "angmsdaa"},
{10665, "angmsdab"},
{10666, "angmsdac"},
{10667, "angmsdad"},
{10668, "angmsdae"},
{10669, "angmsdaf"},
{10670, "angmsdag"},
{10671, "angmsdah"},
{8735, "angrt"},
{8894, "angrtvb"},
{10653, "angrtvbd"},
{8738, "angsph"},
{197, "angst"},
{9084, "angzarr"},
{261, "aogon"},
{120146, "aopf"},
{8776, "ap"},
{10864, "apE"},
{10863, "apacir"},
{8778, "ape"},
{8779, "apid"},
{39, "apos"},
{8776, "approx"},
{8778, "approxeq"},
{229, "aring"},
{119990, "ascr"},
{42, "ast"},
{8776, "asymp"},
{8781, "asympeq"},
{227, "atilde"},
{228, "auml"},
{8755, "awconint"},
{10769, "awint"},
{10989, "bNot"},
{8780, "backcong"},
{1014, "backepsilon"},
{8245, "backprime"},
{8765, "backsim"},
{8909, "backsimeq"},
{8893, "barvee"},
{8965, "barwedge"},
{9141, "bbrk"},
{9142, "bbrktbrk"},
{8780, "bcong"},
{1073, "bcy"},
{8222, "bdquo"},
{8757, "because"},
{10672, "bemptyv"},
{1014, "bepsi"},
{8492, "bernou"},
{946, "beta"},
{8502, "beth"},
{8812, "between"},
{120095, "bfr"},
{8898, "bigcap"},
{9711, "bigcirc"},
{8899, "bigcup"},
{10752, "bigodot"},
{10753, "bigoplus"},
{10754, "bigotimes"},
{10758, "bigsqcup"},
{9733, "bigstar"},
{9661, "bigtriangledown"},
{9651, "bigtriangleup"},
{10756, "biguplus"},
{8897, "bigvee"},
{8896, "bigwedge"},
{10509, "bkarow"},
{10731, "blacklozenge"},
{9642, "blacksquare"},
{9652, "blacktriangle"},
{9662, "blacktriangledown"},
{9666, "blacktriangleleft"},
{9656, "blacktriangleright"},
{9251, "blank"},
{9618, "blk12"},
{9617, "blk14"},
{9619, "blk34"},
{9608, "block"},
{8976, "bnot"},
{120147, "bopf"},
{8869, "bot"},
{8869, "bottom"},
{8904, "bowtie"},
{9559, "boxDL"},
{9556, "boxDR"},
{9558, "boxDl"},
{9555, "boxDr"},
{9552, "boxH"},
{9574, "boxHD"},
{9577, "boxHU"},
{9572, "boxHd"},
{9575, "boxHu"},
{9565, "boxUL"},
{9562, "boxUR"},
{9564, "boxUl"},
{9561, "boxUr"},
{9553, "boxV"},
{9580, "boxVH"},
{9571, "boxVL"},
{9568, "boxVR"},
{9579, "boxVh"},
{9570, "boxVl"},
{9567, "boxVr"},
{10697, "boxbox"},
{9557, "boxdL"},
{9554, "boxdR"},
{9488, "boxdl"},
{9484, "boxdr"},
{9472, "boxh"},
{9573, "boxhD"},
{9576, "boxhU"},
{9516, "boxhd"},
{9524, "boxhu"},
{8863, "boxminus"},
{8862, "boxplus"},
{8864, "boxtimes"},
{9563, "boxuL"},
{9560, "boxuR"},
{9496, "boxul"},
{9492, "boxur"},
{9474, "boxv"},
{9578, "boxvH"},
{9569, "boxvL"},
{9566, "boxvR"},
{9532, "boxvh"},
{9508, "boxvl"},
{9500, "boxvr"},
{8245, "bprime"},
{728, "breve"},
{166, "brvbar"},
{119991, "bscr"},
{8271, "bsemi"},
{8765, "bsim"},
{8909, "bsime"},
{92, "bsol"},
{10693, "bsolb"},
{10184, "bsolhsub"},
{8226, "bull"},
{8226, "bullet"},
{8782, "bump"},
{10926, "bumpE"},
{8783, "bumpe"},
{8783, "bumpeq"},
{263, "cacute"},
{8745, "cap"},
{10820, "capand"},
{10825, "capbrcup"},
{10827, "capcap"},
{10823, "capcup"},
{10816, "capdot"},
{8257, "caret"},
{711, "caron"},
{10829, "ccaps"},
{269, "ccaron"},
{231, "ccedil"},
{265, "ccirc"},
{10828, "ccups"},
{10832, "ccupssm"},
{267, "cdot"},
{184, "cedil"},
{10674, "cemptyv"},
{162, "cent"},
{183, "centerdot"},
{120096, "cfr"},
{1095, "chcy"},
{10003, "check"},
{10003, "checkmark"},
{967, "chi"},
{9675, "cir"},
{10691, "cirE"},
{710, "circ"},
{8791, "circeq"},
{8634, "circlearrowleft"},
{8635, "circlearrowright"},
{174, "circledR"},
{9416, "circledS"},
{8859, "circledast"},
{8858, "circledcirc"},
{8861, "circleddash"},
{8791, "cire"},
{10768, "cirfnint"},
{10991, "cirmid"},
{10690, "cirscir"},
{9827, "clubs"},
{9827, "clubsuit"},
{58, "colon"},
{8788, "colone"},
{8788, "coloneq"},
{44, "comma"},
{64, "commat"},
{8705, "comp"},
{8728, "compfn"},
{8705, "complement"},
{8450, "complexes"},
{8773, "cong"},
{10861, "congdot"},
{8750, "conint"},
{120148, "copf"},
{8720, "coprod"},
{169, "copy"},
{8471, "copysr"},
{8629, "crarr"},
{10007, "cross"},
{119992, "cscr"},
{10959, "csub"},
{10961, "csube"},
{10960, "csup"},
{10962, "csupe"},
{8943, "ctdot"},
{10552, "cudarrl"},
{10549, "cudarrr"},
{8926, "cuepr"},
{8927, "cuesc"},
{8630, "cularr"},
{10557, "cularrp"},
{8746, "cup"},
{10824, "cupbrcap"},
{10822, "cupcap"},
{10826, "cupcup"},
{8845, "cupdot"},
{10821, "cupor"},
{8631, "curarr"},
{10556, "curarrm"},
{8926, "curlyeqprec"},
{8927, "curlyeqsucc"},
{8910, "curlyvee"},
{8911, "curlywedge"},
{164, "curren"},
{8630, "curvearrowleft"},
{8631, "curvearrowright"},
{8910, "cuvee"},
{8911, "cuwed"},
{8754, "cwconint"},
{8753, "cwint"},
{9005, "cylcty"},
{8659, "dArr"},
{10597, "dHar"},
{8224, "dagger"},
{8504, "daleth"},
{8595, "darr"},
{8208, "dash"},
{8867, "dashv"},
{10511, "dbkarow"},
{733, "dblac"},
{271, "dcaron"},
{1076, "dcy"},
{8518, "dd"},
{8225, "ddagger"},
{8650, "ddarr"},
{10871, "ddotseq"},
{176, "deg"},
{948, "delta"},
{10673, "demptyv"},
{10623, "dfisht"},
{120097, "dfr"},
{8643, "dharl"},
{8642, "dharr"},
{8900, "diam"},
{8900, "diamond"},
{9830, "diamondsuit"},
{9830, "diams"},
{168, "die"},
{989, "digamma"},
{8946, "disin"},
{247, "div"},
{247, "divide"},
{8903, "divideontimes"},
{8903, "divonx"},
{1106, "djcy"},
{8990, "dlcorn"},
{8973, "dlcrop"},
{36, "dollar"},
{120149, "dopf"},
{729, "dot"},
{8784, "doteq"},
{8785, "doteqdot"},
{8760, "dotminus"},
{8724, "dotplus"},
{8865, "dotsquare"},
{8966, "doublebarwedge"},
{8595, "downarrow"},
{8650, "downdownarrows"},
{8643, "downharpoonleft"},
{8642, "downharpoonright"},
{10512, "drbkarow"},
{8991, "drcorn"},
{8972, "drcrop"},
{119993, "dscr"},
{1109, "dscy"},
{10742, "dsol"},
{273, "dstrok"},
{8945, "dtdot"},
{9663, "dtri"},
{9662, "dtrif"},
{8693, "duarr"},
{10607, "duhar"},
{10662, "dwangle"},
{1119, "dzcy"},
{10239, "dzigrarr"},
{10871, "eDDot"},
{8785, "eDot"},
{233, "eacute"},
{10862, "easter"},
{283, "ecaron"},
{8790, "ecir"},
{234, "ecirc"},
{8789, "ecolon"},
{1101, "ecy"},
{279, "edot"},
{8519, "ee"},
{8786, "efDot"},
{120098, "efr"},
{10906, "eg"},
{232, "egrave"},
{10902, "egs"},
{10904, "egsdot"},
{10905, "el"},
{9191, "elinters"},
{8467, "ell"},
{10901, "els"},
{10903, "elsdot"},
{275, "emacr"},
{8709, "empty"},
{8709, "emptyset"},
{8709, "emptyv"},
{8195, "emsp"},
{8196, "emsp13"},
{8197, "emsp14"},
{331, "eng"},
{8194, "ensp"},
{281, "eogon"},
{120150, "eopf"},
{8917, "epar"},
{10723, "eparsl"},
{10865, "eplus"},
{949, "epsi"},
{949, "epsilon"},
{1013, "epsiv"},
{8790, "eqcirc"},
{8789, "eqcolon"},
{8770, "eqsim"},
{10902, "eqslantgtr"},
{10901, "eqslantless"},
{61, "equals"},
{8799, "equest"},
{8801, "equiv"},
{10872, "equivDD"},
{10725, "eqvparsl"},
{8787, "erDot"},
{10609, "erarr"},
{8495, "escr"},
{8784, "esdot"},
{8770, "esim"},
{951, "eta"},
{240, "eth"},
{235, "euml"},
{8364, "euro"},
{33, "excl"},
{8707, "exist"},
{8496, "expectation"},
{8519, "exponentiale"},
{8786, "fallingdotseq"},
{1092, "fcy"},
{9792, "female"},
{64259, "ffilig"},
{64256, "fflig"},
{64260, "ffllig"},
{120099, "ffr"},
{64257, "filig"},
{9837, "flat"},
{64258, "fllig"},
{9649, "fltns"},
{402, "fnof"},
{120151, "fopf"},
{8704, "forall"},
{8916, "fork"},
{10969, "forkv"},
{10765, "fpartint"},
{189, "frac12"},
{8531, "frac13"},
{188, "frac14"},
{8533, "frac15"},
{8537, "frac16"},
{8539, "frac18"},
{8532, "frac23"},
{8534, "frac25"},
{190, "frac34"},
{8535, "frac35"},
{8540, "frac38"},
{8536, "frac45"},
{8538, "frac56"},
{8541, "frac58"},
{8542, "frac78"},
{8260, "frasl"},
{8994, "frown"},
{119995, "fscr"},
{8807, "gE"},
{10892, "gEl"},
{501, "gacute"},
{947, "gamma"},
{989, "gammad"},
{10886, "gap"},
{287, "gbreve"},
{285, "gcirc"},
{1075, "gcy"},
{289, "gdot"},
{8805, "ge"},
{8923, "gel"},
{8805, "geq"},
{8807, "geqq"},
{10878, "geqslant"},
{10878, "ges"},
{10921, "gescc"},
{10880, "gesdot"},
{10882, "gesdoto"},
{10884, "gesdotol"},
{10900, "gesles"},
{120100, "gfr"},
{8811, "gg"},
{8921, "ggg"},
{8503, "gimel"},
{1107, "gjcy"},
{8823, "gl"},
{10898, "glE"},
{10917, "gla"},
{10916, "glj"},
{8809, "gnE"},
{10890, "gnap"},
{10890, "gnapprox"},
{10888, "gne"},
{10888, "gneq"},
{8809, "gneqq"},
{8935, "gnsim"},
{120152, "gopf"},
{96, "grave"},
{8458, "gscr"},
{8819, "gsim"},
{10894, "gsime"},
{10896, "gsiml"},
{62, "gt"},
{10919, "gtcc"},
{10874, "gtcir"},
{8919, "gtdot"},
{10645, "gtlPar"},
{10876, "gtquest"},
{10886, "gtrapprox"},
{10616, "gtrarr"},
{8919, "gtrdot"},
{8923, "gtreqless"},
{10892, "gtreqqless"},
{8823, "gtrless"},
{8819, "gtrsim"},
{8660, "hArr"},
{8202, "hairsp"},
{189, "half"},
{8459, "hamilt"},
{1098, "hardcy"},
{8596, "harr"},
{10568, "harrcir"},
{8621, "harrw"},
{8463, "hbar"},
{293, "hcirc"},
{9829, "hearts"},
{9829, "heartsuit"},
{8230, "hellip"},
{8889, "hercon"},
{120101, "hfr"},
{10533, "hksearow"},
{10534, "hkswarow"},
{8703, "hoarr"},
{8763, "homtht"},
{8617, "hookleftarrow"},
{8618, "hookrightarrow"},
{120153, "hopf"},
{8213, "horbar"},
{119997, "hscr"},
{8463, "hslash"},
{295, "hstrok"},
{8259, "hybull"},
{8208, "hyphen"},
{237, "iacute"},
{8291, "ic"},
{238, "icirc"},
{1080, "icy"},
{1077, "iecy"},
{161, "iexcl"},
{8660, "iff"},
{120102, "ifr"},
{236, "igrave"},
{8520, "ii"},
{10764, "iiiint"},
{8749, "iiint"},
{10716, "iinfin"},
{8489, "iiota"},
{307, "ijlig"},
{299, "imacr"},
{8465, "image"},
{8464, "imagline"},
{8465, "imagpart"},
{305, "imath"},
{8887, "imof"},
{437, "imped"},
{8712, "in"},
{8453, "incare"},
{8734, "infin"},
{10717, "infintie"},
{305, "inodot"},
{8747, "int"},
{8890, "intcal"},
{8484, "integers"},
{8890, "intercal"},
{10775, "intlarhk"},
{10812, "intprod"},
{1105, "iocy"},
{303, "iogon"},
{120154, "iopf"},
{953, "iota"},
{10812, "iprod"},
{191, "iquest"},
{119998, "iscr"},
{8712, "isin"},
{8953, "isinE"},
{8949, "isindot"},
{8948, "isins"},
{8947, "isinsv"},
{8712, "isinv"},
{8290, "it"},
{297, "itilde"},
{1110, "iukcy"},
{239, "iuml"},
{309, "jcirc"},
{1081, "jcy"},
{120103, "jfr"},
{567, "jmath"},
{120155, "jopf"},
{119999, "jscr"},
{1112, "jsercy"},
{1108, "jukcy"},
{954, "kappa"},
{1008, "kappav"},
{311, "kcedil"},
{1082, "kcy"},
{120104, "kfr"},
{312, "kgreen"},
{1093, "khcy"},
{1116, "kjcy"},
{120156, "kopf"},
{120000, "kscr"},
{8666, "lAarr"},
{8656, "lArr"},
{10523, "lAtail"},
{10510, "lBarr"},
{8806, "lE"},
{10891, "lEg"},
{10594, "lHar"},
{314, "lacute"},
{10676, "laemptyv"},
{8466, "lagran"},
{955, "lambda"},
{9001, "lang"},
{10641, "langd"},
{10216, "langle"},
{10885, "lap"},
{171, "laquo"},
{8592, "larr"},
{8676, "larrb"},
{10527, "larrbfs"},
{10525, "larrfs"},
{8617, "larrhk"},
{8619, "larrlp"},
{10553, "larrpl"},
{10611, "larrsim"},
{8610, "larrtl"},
{10923, "lat"},
{10521, "latail"},
{10925, "late"},
{10508, "lbarr"},
{10098, "lbbrk"},
{123, "lbrace"},
{91, "lbrack"},
{10635, "lbrke"},
{10639, "lbrksld"},
{10637, "lbrkslu"},
{318, "lcaron"},
{316, "lcedil"},
{8968, "lceil"},
{123, "lcub"},
{1083, "lcy"},
{10550, "ldca"},
{8220, "ldquo"},
{8222, "ldquor"},
{10599, "ldrdhar"},
{10571, "ldrushar"},
{8626, "ldsh"},
{8804, "le"},
{8592, "leftarrow"},
{8610, "leftarrowtail"},
{8637, "leftharpoondown"},
{8636, "leftharpoonup"},
{8647, "leftleftarrows"},
{8596, "leftrightarrow"},
{8646, "leftrightarrows"},
{8651, "leftrightharpoons"},
{8621, "leftrightsquigarrow"},
{8907, "leftthreetimes"},
{8922, "leg"},
{8804, "leq"},
{8806, "leqq"},
{10877, "leqslant"},
{10877, "les"},
{10920, "lescc"},
{10879, "lesdot"},
{10881, "lesdoto"},
{10883, "lesdotor"},
{10899, "lesges"},
{10885, "lessapprox"},
{8918, "lessdot"},
{8922, "lesseqgtr"},
{10891, "lesseqqgtr"},
{8822, "lessgtr"},
{8818, "lesssim"},
{10620, "lfisht"},
{8970, "lfloor"},
{120105, "lfr"},
{8822, "lg"},
{10897, "lgE"},
{8637, "lhard"},
{8636, "lharu"},
{10602, "lharul"},
{9604, "lhblk"},
{1113, "ljcy"},
{8810, "ll"},
{8647, "llarr"},
{8990, "llcorner"},
{10603, "llhard"},
{9722, "lltri"},
{320, "lmidot"},
{9136, "lmoust"},
{9136, "lmoustache"},
{8808, "lnE"},
{10889, "lnap"},
{10889, "lnapprox"},
{10887, "lne"},
{10887, "lneq"},
{8808, "lneqq"},
{8934, "lnsim"},
{10220, "loang"},
{8701, "loarr"},
{10214, "lobrk"},
{10229, "longleftarrow"},
{10231, "longleftrightarrow"},
{10236, "longmapsto"},
{10230, "longrightarrow"},
{8619, "looparrowleft"},
{8620, "looparrowright"},
{10629, "lopar"},
{120157, "lopf"},
{10797, "loplus"},
{10804, "lotimes"},
{8727, "lowast"},
{95, "lowbar"},
{9674, "loz"},
{9674, "lozenge"},
{10731, "lozf"},
{40, "lpar"},
{10643, "lparlt"},
{8646, "lrarr"},
{8991, "lrcorner"},
{8651, "lrhar"},
{10605, "lrhard"},
{8206, "lrm"},
{8895, "lrtri"},
{8249, "lsaquo"},
{120001, "lscr"},
{8624, "lsh"},
{8818, "lsim"},
{10893, "lsime"},
{10895, "lsimg"},
{91, "lsqb"},
{8216, "lsquo"},
{8218, "lsquor"},
{322, "lstrok"},
{60, "lt"},
{10918, "ltcc"},
{10873, "ltcir"},
{8918, "ltdot"},
{8907, "lthree"},
{8905, "ltimes"},
{10614, "ltlarr"},
{10875, "ltquest"},
{10646, "ltrPar"},
{9667, "ltri"},
{8884, "ltrie"},
{9666, "ltrif"},
{10570, "lurdshar"},
{10598, "luruhar"},
{8762, "mDDot"},
{175, "macr"},
{9794, "male"},
{10016, "malt"},
{10016, "maltese"},
{8614, "map"},
{8614, "mapsto"},
{8615, "mapstodown"},
{8612, "mapstoleft"},
{8613, "mapstoup"},
{9646, "marker"},
{10793, "mcomma"},
{1084, "mcy"},
{8212, "mdash"},
{8737, "measuredangle"},
{120106, "mfr"},
{8487, "mho"},
{181, "micro"},
{8739, "mid"},
{42, "midast"},
{10992, "midcir"},
{183, "middot"},
{8722, "minus"},
{8863, "minusb"},
{8760, "minusd"},
{10794, "minusdu"},
{10971, "mlcp"},
{8230, "mldr"},
{8723, "mnplus"},
{8871, "models"},
{120158, "mopf"},
{8723, "mp"},
{120002, "mscr"},
{8766, "mstpos"},
{956, "mu"},
{8888, "multimap"},
{8888, "mumap"},
{8653, "nLeftarrow"},
{8654, "nLeftrightarrow"},
{8655, "nRightarrow"},
{8879, "nVDash"},
{8878, "nVdash"},
{8711, "nabla"},
{324, "nacute"},
{8777, "nap"},
{329, "napos"},
{8777, "napprox"},
{9838, "natur"},
{9838, "natural"},
{8469, "naturals"},
{160, "nbsp"},
{10819, "ncap"},
{328, "ncaron"},
{326, "ncedil"},
{8775, "ncong"},
{10818, "ncup"},
{1085, "ncy"},
{8211, "ndash"},
{8800, "ne"},
{8663, "neArr"},
{10532, "nearhk"},
{8599, "nearr"},
{8599, "nearrow"},
{8802, "nequiv"},
{10536, "nesear"},
{8708, "nexist"},
{8708, "nexists"},
{120107, "nfr"},
{8817, "nge"},
{8817, "ngeq"},
{8821, "ngsim"},
{8815, "ngt"},
{8815, "ngtr"},
{8654, "nhArr"},
{8622, "nharr"},
{10994, "nhpar"},
{8715, "ni"},
{8956, "nis"},
{8954, "nisd"},
{8715, "niv"},
{1114, "njcy"},
{8653, "nlArr"},
{8602, "nlarr"},
{8229, "nldr"},
{8816, "nle"},
{8602, "nleftarrow"},
{8622, "nleftrightarrow"},
{8816, "nleq"},
{8814, "nless"},
{8820, "nlsim"},
{8814, "nlt"},
{8938, "nltri"},
{8940, "nltrie"},
{8740, "nmid"},
{120159, "nopf"},
{172, "not"},
{8713, "notin"},
{8713, "notinva"},
{8951, "notinvb"},
{8950, "notinvc"},
{8716, "notni"},
{8716, "notniva"},
{8958, "notnivb"},
{8957, "notnivc"},
{8742, "npar"},
{8742, "nparallel"},
{10772, "npolint"},
{8832, "npr"},
{8928, "nprcue"},
{8832, "nprec"},
{8655, "nrArr"},
{8603, "nrarr"},
{8603, "nrightarrow"},
{8939, "nrtri"},
{8941, "nrtrie"},
{8833, "nsc"},
{8929, "nsccue"},
{120003, "nscr"},
{8740, "nshortmid"},
{8742, "nshortparallel"},
{8769, "nsim"},
{8772, "nsime"},
{8772, "nsimeq"},
{8740, "nsmid"},
{8742, "nspar"},
{8930, "nsqsube"},
{8931, "nsqsupe"},
{8836, "nsub"},
{8840, "nsube"},
{8840, "nsubseteq"},
{8833, "nsucc"},
{8837, "nsup"},
{8841, "nsupe"},
{8841, "nsupseteq"},
{8825, "ntgl"},
{241, "ntilde"},
{8824, "ntlg"},
{8938, "ntriangleleft"},
{8940, "ntrianglelefteq"},
{8939, "ntriangleright"},
{8941, "ntrianglerighteq"},
{957, "nu"},
{35, "num"},
{8470, "numero"},
{8199, "numsp"},
{8877, "nvDash"},
{10500, "nvHarr"},
{8876, "nvdash"},
{10718, "nvinfin"},
{10498, "nvlArr"},
{10499, "nvrArr"},
{8662, "nwArr"},
{10531, "nwarhk"},
{8598, "nwarr"},
{8598, "nwarrow"},
{10535, "nwnear"},
{9416, "oS"},
{243, "oacute"},
{8859, "oast"},
{8858, "ocir"},
{244, "ocirc"},
{1086, "ocy"},
{8861, "odash"},
{337, "odblac"},
{10808, "odiv"},
{8857, "odot"},
{10684, "odsold"},
{339, "oelig"},
{10687, "ofcir"},
{120108, "ofr"},
{731, "ogon"},
{242, "ograve"},
{10689, "ogt"},
{10677, "ohbar"},
{937, "ohm"},
{8750, "oint"},
{8634, "olarr"},
{10686, "olcir"},
{10683, "olcross"},
{8254, "oline"},
{10688, "olt"},
{333, "omacr"},
{969, "omega"},
{959, "omicron"},
{10678, "omid"},
{8854, "ominus"},
{120160, "oopf"},
{10679, "opar"},
{10681, "operp"},
{8853, "oplus"},
{8744, "or"},
{8635, "orarr"},
{10845, "ord"},
{8500, "order"},
{8500, "orderof"},
{170, "ordf"},
{186, "ordm"},
{8886, "origof"},
{10838, "oror"},
{10839, "orslope"},
{10843, "orv"},
{8500, "oscr"},
{248, "oslash"},
{8856, "osol"},
{245, "otilde"},
{8855, "otimes"},
{10806, "otimesas"},
{246, "ouml"},
{9021, "ovbar"},
{8741, "par"},
{182, "para"},
{8741, "parallel"},
{10995, "parsim"},
{11005, "parsl"},
{8706, "part"},
{1087, "pcy"},
{37, "percnt"},
{46, "period"},
{8240, "permil"},
{8869, "perp"},
{8241, "pertenk"},
{120109, "pfr"},
{966, "phi"},
{981, "phiv"},
{8499, "phmmat"},
{9742, "phone"},
{960, "pi"},
{8916, "pitchfork"},
{982, "piv"},
{8463, "planck"},
{8462, "planckh"},
{8463, "plankv"},
{43, "plus"},
{10787, "plusacir"},
{8862, "plusb"},
{10786, "pluscir"},
{8724, "plusdo"},
{10789, "plusdu"},
{10866, "pluse"},
{177, "plusmn"},
{10790, "plussim"},
{10791, "plustwo"},
{177, "pm"},
{10773, "pointint"},
{120161, "popf"},
{163, "pound"},
{8826, "pr"},
{10931, "prE"},
{10935, "prap"},
{8828, "prcue"},
{10927, "pre"},
{8826, "prec"},
{10935, "precapprox"},
{8828, "preccurlyeq"},
{10927, "preceq"},
{10937, "precnapprox"},
{10933, "precneqq"},
{8936, "precnsim"},
{8830, "precsim"},
{8242, "prime"},
{8473, "primes"},
{10933, "prnE"},
{10937, "prnap"},
{8936, "prnsim"},
{8719, "prod"},
{9006, "profalar"},
{8978, "profline"},
{8979, "profsurf"},
{8733, "prop"},
{8733, "propto"},
{8830, "prsim"},
{8880, "prurel"},
{120005, "pscr"},
{968, "psi"},
{8200, "puncsp"},
{120110, "qfr"},
{10764, "qint"},
{120162, "qopf"},
{8279, "qprime"},
{120006, "qscr"},
{8461, "quaternions"},
{10774, "quatint"},
{63, "quest"},
{8799, "questeq"},
{34, "quot"},
{8667, "rAarr"},
{8658, "rArr"},
{10524, "rAtail"},
{10511, "rBarr"},
{10596, "rHar"},
{341, "racute"},
{8730, "radic"},
{10675, "raemptyv"},
{9002, "rang"},
{10642, "rangd"},
{10661, "range"},
{10217, "rangle"},
{187, "raquo"},
{8594, "rarr"},
{10613, "rarrap"},
{8677, "rarrb"},
{10528, "rarrbfs"},
{10547, "rarrc"},
{10526, "rarrfs"},
{8618, "rarrhk"},
{8620, "rarrlp"},
{10565, "rarrpl"},
{10612, "rarrsim"},
{8611, "rarrtl"},
{8605, "rarrw"},
{10522, "ratail"},
{8758, "ratio"},
{8474, "rationals"},
{10509, "rbarr"},
{10099, "rbbrk"},
{125, "rbrace"},
{93, "rbrack"},
{10636, "rbrke"},
{10638, "rbrksld"},
{10640, "rbrkslu"},
{345, "rcaron"},
{343, "rcedil"},
{8969, "rceil"},
{125, "rcub"},
{1088, "rcy"},
{10551, "rdca"},
{10601, "rdldhar"},
{8221, "rdquo"},
{8221, "rdquor"},
{8627, "rdsh"},
{8476, "real"},
{8475, "realine"},
{8476, "realpart"},
{8477, "reals"},
{9645, "rect"},
{174, "reg"},
{10621, "rfisht"},
{8971, "rfloor"},
{120111, "rfr"},
{8641, "rhard"},
{8640, "rharu"},
{10604, "rharul"},
{961, "rho"},
{1009, "rhov"},
{8594, "rightarrow"},
{8611, "rightarrowtail"},
{8641, "rightharpoondown"},
{8640, "rightharpoonup"},
{8644, "rightleftarrows"},
{8652, "rightleftharpoons"},
{8649, "rightrightarrows"},
{8605, "rightsquigarrow"},
{8908, "rightthreetimes"},
{730, "ring"},
{8787, "risingdotseq"},
{8644, "rlarr"},
{8652, "rlhar"},
{8207, "rlm"},
{9137, "rmoust"},
{9137, "rmoustache"},
{10990, "rnmid"},
{10221, "roang"},
{8702, "roarr"},
{10215, "robrk"},
{10630, "ropar"},
{120163, "ropf"},
{10798, "roplus"},
{10805, "rotimes"},
{41, "rpar"},
{10644, "rpargt"},
{10770, "rppolint"},
{8649, "rrarr"},
{8250, "rsaquo"},
{120007, "rscr"},
{8625, "rsh"},
{93, "rsqb"},
{8217, "rsquo"},
{8217, "rsquor"},
{8908, "rthree"},
{8906, "rtimes"},
{9657, "rtri"},
{8885, "rtrie"},
{9656, "rtrif"},
{10702, "rtriltri"},
{10600, "ruluhar"},
{8478, "rx"},
{347, "sacute"},
{8218, "sbquo"},
{8827, "sc"},
{10932, "scE"},
{10936, "scap"},
{353, "scaron"},
{8829, "sccue"},
{10928, "sce"},
{351, "scedil"},
{349, "scirc"},
{10934, "scnE"},
{10938, "scnap"},
{8937, "scnsim"},
{10771, "scpolint"},
{8831, "scsim"},
{1089, "scy"},
{8901, "sdot"},
{8865, "sdotb"},
{10854, "sdote"},
{8664, "seArr"},
{10533, "searhk"},
{8600, "searr"},
{8600, "searrow"},
{167, "sect"},
{59, "semi"},
{10537, "seswar"},
{8726, "setminus"},
{8726, "setmn"},
{10038, "sext"},
{120112, "sfr"},
{8994, "sfrown"},
{9839, "sharp"},
{1097, "shchcy"},
{1096, "shcy"},
{8739, "shortmid"},
{8741, "shortparallel"},
{371, "shy"},
{963, "sigma"},
{962, "sigmaf"},
{962, "sigmav"},
{8764, "sim"},
{10858, "simdot"},
{8771, "sime"},
{8771, "simeq"},
{10910, "simg"},
{10912, "simgE"},
{10909, "siml"},
{10911, "simlE"},
{8774, "simne"},
{10788, "simplus"},
{10610, "simrarr"},
{8592, "slarr"},
{8726, "smallsetminus"},
{10803, "smashp"},
{10724, "smeparsl"},
{8739, "smid"},
{8995, "smile"},
{10922, "smt"},
{10924, "smte"},
{1100, "softcy"},
{47, "sol"},
{10692, "solb"},
{9023, "solbar"},
{120164, "sopf"},
{9824, "spades"},
{9824, "spadesuit"},
{8741, "spar"},
{8851, "sqcap"},
{8852, "sqcup"},
{8847, "sqsub"},
{8849, "sqsube"},
{8847, "sqsubset"},
{8849, "sqsubseteq"},
{8848, "sqsup"},
{8850, "sqsupe"},
{8848, "sqsupset"},
{8850, "sqsupseteq"},
{9633, "squ"},
{9633, "square"},
{9642, "squarf"},
{9642, "squf"},
{8594, "srarr"},
{120008, "sscr"},
{8726, "ssetmn"},
{8995, "ssmile"},
{8902, "sstarf"},
{9734, "star"},
{9733, "starf"},
{1013, "straightepsilon"},
{981, "straightphi"},
{175, "strns"},
{8834, "sub"},
{10949, "subE"},
{10941, "subdot"},
{8838, "sube"},
{10947, "subedot"},
{10945, "submult"},
{10955, "subnE"},
{8842, "subne"},
{10943, "subplus"},
{10617, "subrarr"},
{8834, "subset"},
{8838, "subseteq"},
{10949, "subseteqq"},
{8842, "subsetneq"},
{10955, "subsetneqq"},
{10951, "subsim"},
{10965, "subsub"},
{10963, "subsup"},
{8827, "succ"},
{10936, "succapprox"},
{8829, "succcurlyeq"},
{10928, "succeq"},
{10938, "succnapprox"},
{10934, "succneqq"},
{8937, "succnsim"},
{8831, "succsim"},
{8721, "sum"},
{9834, "sung"},
{8835, "sup"},
{185, "sup1"},
{178, "sup2"},
{179, "sup3"},
{10950, "supE"},
{10942, "supdot"},
{10968, "supdsub"},
{8839, "supe"},
{10948, "supedot"},
{10185, "suphsol"},
{10967, "suphsub"},
{10619, "suplarr"},
{10946, "supmult"},
{10956, "supnE"},
{8843, "supne"},
{10944, "supplus"},
{8835, "supset"},
{8839, "supseteq"},
{10950, "supseteqq"},
{8843, "supsetneq"},
{10956, "supsetneqq"},
{10952, "supsim"},
{10964, "supsub"},
{10966, "supsup"},
{8665, "swArr"},
{10534, "swarhk"},
{8601, "swarr"},
{8601, "swarrow"},
{10538, "swnwar"},
{223, "szlig"},
{8982, "target"},
{964, "tau"},
{9140, "tbrk"},
{357, "tcaron"},
{355, "tcedil"},
{1090, "tcy"},
{8411, "tdot"},
{8981, "telrec"},
{120113, "tfr"},
{8756, "there4"},
{8756, "therefore"},
{952, "theta"},
{977, "thetasym"},
{977, "thetav"},
{8776, "thickapprox"},
{8764, "thicksim"},
{8201, "thinsp"},
{8776, "thkap"},
{8764, "thksim"},
{254, "thorn"},
{732, "tilde"},
{215, "times"},
{8864, "timesb"},
{10801, "timesbar"},
{10800, "timesd"},
{8749, "tint"},
{10536, "toea"},
{8868, "top"},
{9014, "topbot"},
{10993, "topcir"},
{120165, "topf"},
{10970, "topfork"},
{10537, "tosa"},
{8244, "tprime"},
{8482, "trade"},
{9653, "triangle"},
{9663, "triangledown"},
{9667, "triangleleft"},
{8884, "trianglelefteq"},
{8796, "triangleq"},
{9657, "triangleright"},
{8885, "trianglerighteq"},
{9708, "tridot"},
{8796, "trie"},
{10810, "triminus"},
{10809, "triplus"},
{10701, "trisb"},
{10811, "tritime"},
{9186, "trpezium"},
{120009, "tscr"},
{1094, "tscy"},
{1115, "tshcy"},
{359, "tstrok"},
{8812, "twixt"},
{8606, "twoheadleftarrow"},
{8608, "twoheadrightarrow"},
{8657, "uArr"},
{10595, "uHar"},
{250, "uacute"},
{8593, "uarr"},
{1118, "ubrcy"},
{365, "ubreve"},
{251, "ucirc"},
{1091, "ucy"},
{8645, "udarr"},
{369, "udblac"},
{10606, "udhar"},
{10622, "ufisht"},
{120114, "ufr"},
{249, "ugrave"},
{8639, "uharl"},
{8638, "uharr"},
{9600, "uhblk"},
{8988, "ulcorn"},
{8988, "ulcorner"},
{8975, "ulcrop"},
{9720, "ultri"},
{363, "umacr"},
{168, "uml"},
{371, "uogon"},
{120166, "uopf"},
{8593, "uparrow"},
{8597, "updownarrow"},
{8639, "upharpoonleft"},
{8638, "upharpoonright"},
{8846, "uplus"},
{965, "upsi"},
{978, "upsih"},
{965, "upsilon"},
{8648, "upuparrows"},
{8989, "urcorn"},
{8989, "urcorner"},
{8974, "urcrop"},
{367, "uring"},
{9721, "urtri"},
{120010, "uscr"},
{8944, "utdot"},
{361, "utilde"},
{9653, "utri"},
{9652, "utrif"},
{8648, "uuarr"},
{252, "uuml"},
{10663, "uwangle"},
{8661, "vArr"},
{10984, "vBar"},
{10985, "vBarv"},
{8872, "vDash"},
{10652, "vangrt"},
{1013, "varepsilon"},
{1008, "varkappa"},
{8709, "varnothing"},
{981, "varphi"},
{982, "varpi"},
{8733, "varpropto"},
{8597, "varr"},
{1009, "varrho"},
{962, "varsigma"},
{977, "vartheta"},
{8882, "vartriangleleft"},
{8883, "vartriangleright"},
{1074, "vcy"},
{8866, "vdash"},
{8744, "vee"},
{8891, "veebar"},
{8794, "veeeq"},
{8942, "vellip"},
{120115, "vfr"},
{8882, "vltri"},
{120167, "vopf"},
{8733, "vprop"},
{8883, "vrtri"},
{120011, "vscr"},
{10650, "vzigzag"},
{373, "wcirc"},
{10847, "wedbar"},
{8743, "wedge"},
{8793, "wedgeq"},
{8472, "weierp"},
{120116, "wfr"},
{120168, "wopf"},
{8472, "wp"},
{8768, "wr"},
{8768, "wreath"},
{120012, "wscr"},
{8898, "xcap"},
{9711, "xcirc"},
{8899, "xcup"},
{9661, "xdtri"},
{120117, "xfr"},
{10234, "xhArr"},
{10231, "xharr"},
{958, "xi"},
{10232, "xlArr"},
{10229, "xlarr"},
{10236, "xmap"},
{8955, "xnis"},
{10752, "xodot"},
{120169, "xopf"},
{10753, "xoplus"},
{10754, "xotime"},
{10233, "xrArr"},
{10230, "xrarr"},
{120013, "xscr"},
{10758, "xsqcup"},
{10756, "xuplus"},
{9651, "xutri"},
{8897, "xvee"},
{8896, "xwedge"},
{253, "yacute"},
{1103, "yacy"},
{375, "ycirc"},
{1099, "ycy"},
{165, "yen"},
{120118, "yfr"},
{1111, "yicy"},
{120170, "yopf"},
{120014, "yscr"},
{1102, "yucy"},
{255, "yuml"},
{378, "zacute"},
{382, "zcaron"},
{1079, "zcy"},
{380, "zdot"},
{8488, "zeetrf"},
{950, "zeta"},
{120119, "zfr"},
{1078, "zhcy"},
{8669, "zigrarr"},
{120171, "zopf"},
{120015, "zscr"},
{8205, "zwj"},
{8204, "zwnj"},
{0, 0},
};

static unsigned andLookup(char *entity, char *v)
{
	int i, l, r, d;
	int n = v - entity;
// left, right, binary search
	l = -1;
	r = sizeof(andlist) / sizeof(struct entity);
	while(r - l > 1) {
		i = (l + r) / 2;
		d = strncmp(entity, andlist[i].word, n);
		if(!d && andlist[i].word[n]) d = -1;
		if(!d) return andlist[i].u;
		if(d > 0) l = i; else r = i;
	}
	return 0; // not found
}

