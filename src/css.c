/*********************************************************************
Parse css files into css descriptors, and apply those descriptors to nodes.
All this was written in js but was too slow.
Some sites have thousands of descriptors and hundreds of nodes,
e.g. www.stackoverflow.com with 5,050 descriptors.
These functions are all called from running js,
so can use the native api around the js engine,
which is a tad faster.
*********************************************************************/

#include "eb.h"

#define cssDebugFile "/tmp/css"

#define CSS_ERROR_NONE 0
#define CSS_ERROR_NOSEL 1
#define CSS_ERROR_MANYNOT 2
#define CSS_ERROR_BRACES 3
#define CSS_ERROR_SEL0 4
#define CSS_ERROR_NORULE 5
#define CSS_ERROR_COLON 6
#define CSS_ERROR_RB 7
#define CSS_ERROR_TAG 8
#define CSS_ERROR_ATTR 9
#define CSS_ERROR_RATTR 10
#define CSS_ERROR_DYN 11
#define CSS_ERROR_TAGLINK 12
#define CSS_ERROR_INJECTHIGH 13
#define CSS_ERROR_PE 14
#define CSS_ERROR_UNSUP 15
#define CSS_ERROR_MULTIPLE 16
#define CSS_ERROR_ATEMPTY 17
#define CSS_ERROR_NOTMEDIA 18
#define CSS_ERROR_BADMEDIA 19
// DELIM is special not really an error, true errors come before
#define CSS_ERROR_DELIM 20
#define CSS_ERROR_ATPROC 21
#define CSS_ERROR_LAST 22

static const char *const errorMessage[] = {
	"ok",
	"no selectors",
	"many selectors under not",
	"nested braces",
	"empty selector",
	"no rules",
	"rule no :",
	"modifier no ]",
	"bad tag",
	"bad selector attribute",
	"bad rule attribute",
	"dynamic",
	"tag link",
	"inject high",
	"pseudo element",
	":unsupported",
	"multiple",
	"@ empty",
	"@ not media",
	"@ bad media",
	"==========",
	"@ processed",
	"eof",
};

static int errorBuckets[CSS_ERROR_LAST];
static int loadcount;

static void cssStats(void)
{
	bool first = true;
	int i, l;
	char *s = initString(&l);
	if (debugLevel < 3)
		return;
	stringAndNum(&s, &l, loadcount);
	stringAndString(&s, &l, " selectors + rules");
	for (i = 1; i < CSS_ERROR_LAST; ++i) {
		int n = errorBuckets[i];
		if (!n)
			continue;
		stringAndString(&s, &l, (first ? ": " : ", "));
		first = false;
		stringAndNum(&s, &l, n);
		stringAndChar(&s, &l, ' ');
		stringAndString(&s, &l, errorMessage[i]);
	}
	debugPrint(3, "%s", s);
	nzFree(s);
}

static int closeString(char *s, char delim)
{
	int i;
	char c, qc = 0;		// quote character
	int pc = 0;		// paren count
	if (delim == '"' || delim == '\'')
		qc = delim;
	for (i = 0; (c = s[i]); ++i) {
		if (c == qc) {	// close quote
			if (delim == qc)
				return i + 1;
			qc = 0;
			continue;
		}
		if (c == '\\') {
			if (!s[++i])
				return i;
			continue;
		}
// a css string should not contain an unescaped newline, so if you find one,
// something is wrong or misaligned. End the string here.
		if (c == '\n')
			return i + 1;
		if (qc)
			continue;
		if (c == ']' && delim == '[')
			return i + 1;
		if (c == '(' && delim == '(')
			++pc;
		if (c == ')' && delim == '(' && --pc < 0)
			return i + 1;
		if (c == '"' || c == '\'')
			qc = c;
	}
	return -1;
}

static char *cut20(char *s)
{
	static char buf[20 + 1];
	strncpy(buf, s, 20);
	return buf;
}

// Remove the comments from a css string.
// Watch out - url(http://stuf) or url(data base64 jklk33//ss88djdjj)
static void uncomment(char *s)
{
	char *w = s;
	int n;
	char c, urlmode = 0;
	while ((c = *s)) {
		if (urlmode) { // copy up to paren
			if (c == ')' || c == '\n') urlmode = 0;
			goto copy;
		}
		if (c == '"' || c == '\'') {
			n = closeString(s + 1, c);
			if (n < 0) {
				debugPrint(3, "unterminated string %s",
					   cut20(s));
				goto abandon;
			}
			++n;
			memmove(w, s, n);
			s += n;
			w += n;
			continue;
		}
		if (c == 'u' && !strncmp(s, "url(", 4))
			urlmode = 1;
		if (c == '@' && !strncmp(s, "@ebdelim", 8))
			urlmode = 1;
		if (c != '/')
			goto copy;
		if (s[1] == '/') {
			for (n = 2; s[n]; ++n)
				if (s[n] == '\n')
					break;
			if (s[n]) {
				s += n + 1;
				continue;
			}
			debugPrint(3, "unterminated comment %s", cut20(s));
			goto abandon;
		}
		if (s[1] == '*') {
			for (n = 2; s[n]; ++n)
				if (s[n] == '*' && s[n + 1] == '/')
					break;
			if (s[n]) {
				s += n + 2;
				continue;
			}
			debugPrint(3, "unterminated comment %s", cut20(s));
			goto abandon;
		}
copy:
		*w++ = c;
		++s;
	}
	*w = 0;
	return;
abandon:
	strmove(w, s);
}

static void trim(char *s)
{
int l = strlen(s);
	int n;
	for (n = 0; s[n]; ++n)
		if (!isspace(s[n]))
			break;
	if (n)
		strmove(s, s + n);
	n = strlen(s);
	while (n && isspace(s[n - 1]))
		--n;
/* allow for space at the end of a selector or rule. #foo\   */
	if (isspace(s[n]) && n && s[n - 1] == '\\' &&
	    (n == 1 || s[n - 2] != '\\'))
		++n;
	s[n] = 0;
// we may have to fill the trimmed area with spaces,
// so make sure they are all zeros.
	while(++n < l)
		s[n] = 0;
}

/*********************************************************************
Parse a css string, from a css file, into css descriptors.
Each descriptor points to the left hand side (lhs), the selectors,
and the right hand side (rhs), the rules.
Braces are replaced with '\0' so that lhs and rhs are strings.
thus this string is not freed until all these structures are freed.
Remember the start of this string with a base pointer so you can free it later.
In fact the base is the first lhs.
Each descriptor is a structure, no surprise there.
Along with lhs and rhs, we have selectors and rules.
Commas delimite multiple selectors.
Thus selectors is a chain of structures, one structure per selector.
p,div produces two selectors, for <p> and for <div>.
In fact each selector is more than a structure, it is itself a chain,
a chain of atomic selectors joined by combinators.
See https://www.w3.org/TR/CSS21/selector.html#grouping for the details.
So a > b c produces a chain of length 3.
Now each atomic selector is another chain, the tag name
and a conjuction of modifiers.
div[x="foo"][y="bar"]
So the result is a 4 dimensional structure: descriptors, selectors,
combined atomic selectors, and modifiers.
Beyond this, and parallel to selectors, are rules.
This is a chain of attribute=value pairs, as in color=green.
*********************************************************************/

// css descriptor
struct desc {
	struct desc *next;
	char *lhs, *rhs;
	short bc;		// brace count
	uchar error;
	struct sel *selectors;
	struct rule *rules;
	int highspec;		// specificity when this descriptor matches
	bool underat;
};

// selector
struct sel {
	struct sel *next;
	uchar error;
	bool before, after, hover;
	struct asel *chain;
	int spec;		// specificity
};

// atomic selector
struct asel {
	struct asel *next;
	char *tag;
	char *part;
	bool before, after, hover, link;
	uchar error;
	char combin;
	struct mod *modifiers;
};

// selector modifiers
struct mod {
	struct mod *next;
	char *part;
	bool isclass, isid, negate;
// notchain is used only if negate is true, not(ul.lister), recursive
	struct asel *notchain;
};

struct rule {
	struct rule *next;
	char *atname, *atval;
};

struct hashhead {
	char *key;
	Tag **body;
	int n;
	Tag *t;
};

static struct hashhead *hashtags, *hashids, *hashclasses;
static int hashtags_n, hashids_n, hashclasses_n;

struct shortcache {
	struct shortcache *next;
	char *url;
	char *data;
};

struct cssmaster {
	struct desc *descriptors;
	struct shortcache *cache;
};

static void cssPiecesFree(struct desc *d);
static void cssPiecesPrint(const struct desc *d);
static void cssAtomic(struct asel *a);
static void cssParseLeft(struct desc *d);
static void cssModify(struct asel *a, const char *m1, const char *m2);
static void chainFree(struct asel *asel);
static bool onematch, topmatch, skiproot, gcsmatch, bulkmatch, bulktotal;
static char matchtype;		// 0 plain 1 before 2 after
static bool matchhover;		// match on :hover selectors.
static jsobjtype rootobj;
static Tag **doclist;
static int doclist_a, doclist_n;
static void build_doclist(Tag *top);
static void hashBuild(void);
static void hashFree(void);
static void hashPrint(void);
static Tag **bestListAtomic(struct asel *a);
static void cssEverybody(void);

static char *fromShortCache(const char *url)
{
	struct shortcache *c;
	struct cssmaster *cm = cf->cssmaster;
	if (!cm)
		return 0;
	for (c = cm->cache; c; c = c->next)
		if (stringEqual(url, c->url))
			return c->data;
	return 0;
}

static void intoShortCache(const char *url, char *data)
{
	struct shortcache *c;
	struct cssmaster *cm = cf->cssmaster;
	if (!cm) {
// don't know how this could ever happen.
		cf->cssmaster = cm = allocZeroMem(sizeof(struct cssmaster));
	}
	c = allocMem(sizeof(struct shortcache));
	c->next = cm->cache;
	cm->cache = c;
	c->url = cloneString(url);
	c->data = data;
}

void writeShortCache(void)
{
	struct shortcache *c;
	FILE *f;
	int n = 0;
	struct cssmaster *cm = cf->cssmaster;
	if (!cm)
		return;
	f = fopen("implocal", "w");
	if (!f)
		return;
	for (c = cm->cache; c; c = c->next) {
		char filename[20];
		++n;
		sprintf(filename, "i%d.css", n);
		if (!memoryOutToFile(filename, c->data, strlen(c->data),
				     MSG_TempNoCreate2, MSG_NoWrite2)) {
/* oops, can't write the file */
			unlink(filename);
			showError();
			continue;
		}
		fprintf(f, "%s:%s\n", filename, c->url);
	}
	fclose(f);
}

static void readShortCache(struct cssmaster *cm)
{
	FILE *f = fopen("implocal", "r");
	struct shortcache *c;
	char *s;
	int length, n = 0;
	char line[400];
	if (!f)
		return;
	while (fgets(line, sizeof(line), f)) {
		s = strchr(line, '\n');
		if (!s) {
			fprintf(stderr, "implocal line too long\n");
			fclose(f);
			return;
		}
		*s = 0;
		if (s > line && s[-1] == '\r')
			*--s = 0;
		if (line[0] == '#' || line[0] == 0)
			continue;
		s = strchr(line, ':');
		if (!s) {
			fprintf(stderr, "jslocal line has no :\n");
			continue;
		}
		*s++ = 0;
		c = allocMem(sizeof(struct shortcache));
		c->url = cloneString(s);
		fileIntoMemory(line, &c->data, &length);
// fileIntoMemory puts a null byte on the end
		c->next = cm->cache;
		cm->cache = c;
		++n;
	}
	fclose(f);
	if (n)
		debugPrint(3, "%d import file replacements", n);
}

// Step back through a css string looking for the base url.
// The result is allocated.
static char *cssBase(const char *start, const char *end)
{
	char which;
	char *a, *t;
	int l, nest = 0;
	for (; end >= start; --end) {
		if (*end != '@')
			continue;
		if (strncmp(end, "@ebdelim", 8))
			continue;
		which = end[8];
		if (which == '2') {
			++nest;
			continue;
		}
		if (which == '1') {
			--nest;
			if (nest >= 0)
				continue;
extract:
			end += 9;
			t = strstr(end, "{}\n");
			l = t - end;
			a = allocMem(l + 1);
			memcpy(a, end, l);
			a[l] = 0;
			return a;
		}
		if (which == '0')
			goto extract;
	}
// we shouldn't be here
	return emptyString;
}

static void unstring(char *s)
{
	char *w = s;
	char qc = 0;		// quote character
	char c;
	char hexin[12];
	const char *r;
	unsigned long uc;	// unicode character
	int i;
	while ((c = *s)) {
		if (c == qc) {	// in quotes
			qc = 0;
			++s;
			continue;
		}
		if (!qc && (c == '"' || c == '\'')) {
			qc = c;
			++s;
			continue;
		}
		if (c != '\\')
			goto copy;
		if (!s[1])
			goto copy;
		c = *++s;
		if (c == 'n')
			c = '\n';
		if (c == 't')
			c = '\t';
		if (!isxdigit(c))
			goto copy;
		for (i = 0; i < 4; ++i)
			if (isxdigit(s[i]))
				hexin[i] = s[i];
			else
				break;
		hexin[i] = 0;
		sscanf(hexin, "%lx", &uc);
// convert to utf8
		r = uni2utf8(uc);
		strcpy(w, r);
		w += strlen(r);
		s += i;
		continue;
copy:
		*w++ = c;
		++s;
	}
	*w = 0;
}

// This is a crude measure of specificity, contained in a 5 digit number.
static int specificity(const struct sel *sel, bool underat)
{
	const struct asel *a = sel->chain;
	const struct mod *mod;
	int n = 0;
	int level = 1000;
	int cnt;
	while (a && level) {
		cnt = 0;
		for (mod = a->modifiers; mod; mod = mod->next) {
			if (mod->negate)
				continue;
			if (!cnt)
				++cnt;
			++cnt;
			if (cnt == 8)
				break;
		}
		if (a->tag)
			++cnt;
		n += level * cnt;
		level /= 10;
		a = a->next;
	}

// need something positive here
	if (!n) {
// each atomic has no tag and only "not() modifiers.
// Set n to 1 + the number of not modifiers on the first atomic
		n = 1;
		for (mod = sel->chain->modifiers; mod; mod = mod->next)
			++n;
	}
// @ media tag has more specificity; I have no idea how much more.
	if (underat)
		n += 10;

	return n;
}

static bool mediaPiece(struct desc *d, char *t)
{
	char *a;
	int n, s_width, s_height;
	bool atnot = false, atsome = false;

	if (!*t)		// nothing there
		return false;

	if (!strncmp(t, "only", 4) && !isalnum(t[4])) {
// only is meaningless
		t += 4;
		skipWhite2(&t);
	}
	if (!strncmp(t, "not", 3) && !isalnum(t[3])) {
		atnot = true;
		t += 3;
		skipWhite2(&t);
	}
	if (!strncmp(t, "screen", 6) && !isalnum(t[6])) {
		atsome = true;
		t += 6;
		skipWhite2(&t);
	} else if (!strncmp(t, "all", 3) && !isalnum(t[3])) {
		atsome = true;
		t += 3;
		skipWhite2(&t);
	}

and:
	if (!*t) {
		if (atsome)
			return !atnot;
		d->error = CSS_ERROR_BADMEDIA;
		return false;
	}

	if (!strncmp(t, "and", 3) && !isalnum(t[3])) {
		t += 3;
		skipWhite2(&t);
	}
	if (*t != '(') {
		d->error = CSS_ERROR_BADMEDIA;
		return false;
	}
	++t;
	skipWhite2(&t);
// A well written website can skip the hover text, which edbrowse can't
// manage very well, so we should respond to this one.
	if (!strncmp(t, "hover)", 6)
	    || !strncmp(t, "any-hover)", 10)
	    || !strncmp(t, "pointer)", 8)
	    || !strncmp(t, "any-pointer)", 12)
	    || !strncmp(t, "inverted-colors)", 16))
		return atnot;
	if (!strncmp(t, "scripting)", 10)) {
		if (!isJSAlive)
			return atnot;
		t += 10;
		skipWhite2(&t);
		atsome = true;
		goto and;
	}
// I only handle min or max on certain parameters
	if ((strncmp(t, "max", 3) && strncmp(t, "min", 3)) ||
	    t[3] != '-' || (strncmp(t + 4, "height", 6)
			    && strncmp(t + 4, "width", 5)
			    && strncmp(t + 4, "color", 5)
			    && strncmp(t + 4, "monochrome", 10))) {
// not recognized
		d->error = CSS_ERROR_BADMEDIA;
		return false;
	}
	a = t + 9;
	if (t[4] == 'h')
		++a;
	if (t[4] == 'm')
		a += 5;
	if (*a != ':') {
		d->error = CSS_ERROR_BADMEDIA;
		return false;
	}
	++a;
	skipWhite2(&a);
	if (!isdigit(*a)) {
		d->error = CSS_ERROR_BADMEDIA;
		return false;
	}
	n = strtol(a, &a, 10);

// screen is always 1024 by 768
	s_width = 1024, s_height = 768;

// px or em
	if (a[0] == 'p' && a[1] == 'x')
		a += 2;
	if (a[0] == 'e' && a[1] == 'm') {
		a += 2;
		s_width /= 16, s_height /= 16;
	}
	if (*a != ')') {
		d->error = CSS_ERROR_BADMEDIA;
		return false;
	}

	if (t[1] == 'i' && t[4] == 'w' && n > s_width)
		return atnot;
	if (t[1] == 'a' && t[4] == 'w' && n < s_width)
		return atnot;
	if (t[1] == 'i' && t[4] == 'h' && n > s_height)
		return atnot;
	if (t[1] == 'a' && t[4] == 'h' && n < s_height)
		return atnot;
	if (t[1] == 'i' && t[4] == 'c' && n > 8)
		return atnot;
	if (t[1] == 'a' && t[4] == 'c' && n < 8)
		return atnot;
	if (t[1] == 'i' && t[4] == 'm' && n > 4)
		return atnot;
	if (t[1] == 'a' && t[4] == 'm' && n < 4)
		return atnot;

	t = a + 1;
	skipWhite2(&t);
	atsome = true;
	goto and;
}

static bool media(struct desc *d, char *t)
{
	char *p = t;
	char c;
	int n;

	while ((c = *t)) {
		if (strchr("'\"[(", c)) {
			n = closeString(t + 1, c);
			if (n < 0) {	// should not happen
				d->error = CSS_ERROR_BADMEDIA;
				return false;
			}
			t += n + 1;
			continue;
		}
		if (c == ',') {
			*t = 0;
			trim(p);
			if (mediaPiece(d, p))
				return true;
			p = t + 1;
		}
		++t;
	}

// last piece
	trim(p);
	if (mediaPiece(d, p))
		return true;

	if (!d->error)
		d->error = CSS_ERROR_ATPROC;
	return false;
}

// The above is used by window.matchMedia
bool matchMedia(char *t)
{
	struct desc d0;
	return media(&d0, t);
}

// The input string is assumed allocated, it could be reallocated.
static struct desc *cssPieces(char *s)
{
	int bc = 0;		// brace count
	struct desc *d1 = 0, *d2, *d = 0;
	struct sel *sel;
	struct asel *asel;
	int n;
	char c;
	char *lhs;
	char *a, *t, *at_end_marker = 0;
	char *iu1, *iu2, *iu3;	// for import url

	loadcount = 0;
	memset(errorBuckets, 0, sizeof(errorBuckets));

top:
	uncomment(s);

// look for import url
	iu1 = strstr(s, "@import");
	if (iu1 && isspace(iu1[7])) {
		iu2 = iu1 + 8;
		while (isspace(*iu2))
			++iu2;
		if (!strncmp(iu2, "url(", 4)) {
			iu2 += 4;
			t = iu2;
			while ((c = *t)) {
				struct i_get g;
				char *lasturl, *newurl;
				if (c == '"' || c == '\'') {
					n = closeString(t + 1, c);
					if (n < 0)	// should never happen
						break;
					t += n + 1;
					continue;
				}
				if (c == '\n')	// should never happen
					break;
				if (c != ')') {
					++t;
					continue;
				}
// end of url(blah)
				*t++ = 0;
				while (*t && *t != ';' && *t != '\n')
					++t;
				if (*t)
					++t;
				iu3 = t;
				lasturl = cssBase(s, iu2);
				unstring(iu2);
				newurl = resolveURL(lasturl, iu2);
				nzFree(lasturl);
				*iu1 = 0;
				if ((a = fromShortCache(newurl)))
					goto imported_data;
				debugPrint(3, "css source %s", newurl);
				memset(&g, 0, sizeof(g));
				g.thisfile = cf->fileName;
				g.uriEncoded = true;
				g.url = newurl;
				if (!intFlag && httpConnect(&g)) {
					if (g.code == 200) {
						a = force_utf8(g.buffer,
							       g.length);
						if (!a)
							a = g.buffer;
						else
							nzFree(g.buffer);
						if (g.content[0]
						    && !stringEqual(g.content,
								    "text/css")
						    && !stringEqual(g.content,
								    "text/plain"))
						{
							debugPrint(3,
								   "css suppressed because content type is %s",
								   g.content);
							cnzFree(a);
							a = NULL;
						}
					} else {
						nzFree(g.buffer);
						if (debugLevel >= 3)
							i_printf(MSG_GetCSS,
								 g.url, g.code);
					}
				} else {
					if (debugLevel >= 3)
						i_printf(MSG_GetCSS2);
				}
				if (!a)
					a = emptyString;
				intoShortCache(newurl, a);
imported_data:
				t = allocMem(strlen(s) + strlen(a) +
					     strlen(newurl) + strlen(iu3) + 27);
				sprintf(t,
					"%s\n@ebdelim1%s{}\n%s\n@ebdelim2{}\n%s",
					s, newurl, a, iu3);
				nzFree(newurl);
				nzFree(s);
				s = t;
				goto top;
			}
		}
	}
// remove @charset directives.
top2:
	iu1 = strstr(s, "@charset");
	if (iu1 && isspace(iu1[8])) {
		t = iu1 + 9;
		while ((c = *t)) {
			if (c == '"' || c == '\'') {
				n = closeString(t + 1, c);
				if (n < 0)
					break;
				t += n + 1;
				continue;
			}
			if (c != ';' && c != '\n') {
				++t;
				continue;
			}
			*t++ = 0;
			iu3 = t;
			strmove(iu1, iu3);
			goto top2;
		}
	}

	lhs = s;

	while ((c = *s)) {
		if (at_end_marker && s >= at_end_marker)
			at_end_marker = 0;
		if (c == '"' || c == '\'') {
			n = closeString(s + 1, c);
			if (n < 0) {
				debugPrint(3, "unterminated string %s",
					   cut20(s));
				break;
			}
			s += n + 1;
			continue;
		}
		if (c == '}') {
			if (--bc < 0) {
				debugPrint(3, "unexpected %s", cut20(s));
				break;
			}
			if (bc)
				goto copy;
// bc is 0, end of the descriptor
			*s++ = 0;
			lhs = s;	// next descriptor
			trim(d->lhs);
			trim(d->rhs);

// some special @ code here
			t = d->lhs;
			if (*t != '@')
				goto past_at;
// our special code to insert a delimiter into debugging output
			if (!strncmp(t, "@ebdelim", 8)) {
				d->error = CSS_ERROR_DELIM;
				goto past_at;
			}
			if (d->bc > 2) {
				d->error = CSS_ERROR_BRACES;
				goto past_at;
			}
			if (d->bc == 1) {
				d->error = CSS_ERROR_ATEMPTY;
				goto past_at;
			}
			d->bc = 1;
			++t;
			skipWhite2(&t);
			if (strncmp(t, "media", 5) || isalnum(t[5])) {
				d->error = CSS_ERROR_NOTMEDIA;
				goto past_at;
			}
			t += 5;
			skipWhite2(&t);
			if (!media(d, t))
				goto past_at;
			d->error = CSS_ERROR_ATPROC;

// Here comes some funky string manipulation.
// The descriptors are in rhs.
// I nulled out the trailing }, make it a space.
			while (s[-1] == 0)
				*--s = ' ';
			at_end_marker = s;
			lhs = s = d->rhs;
			d->rhs = "follow";

past_at:
			if (d->bc > 1)
				d->error = CSS_ERROR_BRACES;

			if (!d1)
				d1 = d2 = d;
			else
				d2->next = d, d2 = d;
			d = 0;
			continue;
		}
		if (c == '{') {
			if (bc) {
				++bc;
// remember the highest brace nesting.
				if (bc > d->bc)
					d->bc = bc;
				goto copy;
			}
// A new descripter is born
			d = allocZeroMem(sizeof(struct desc));
			d->lhs = lhs;
			*s++ = 0;
			d->rhs = s;
			d->bc = bc = 1;
			if (at_end_marker)
				d->underat = true;
			continue;
		}
copy:		++s;
	}

	nzFree(d);

	if (!d1) {
// no descriptors, nothing to do,
// except free the incoming string.
		nzFree(lhs);
		cssPiecesPrint(0);
		return NULL;
	}
// now the base string is at d1->lhs;

// Now let's try to understand the selectors.
	for (d = d1; d; d = d->next) {
		if (d->error)
			continue;
		cssParseLeft(d);
	}

// pull before and after up from atomic selector to selector
	for (d = d1; d; d = d->next) {
		if (d->error)
			continue;
		for (sel = d->selectors; sel; sel = sel->next) {
			if (sel->error)
				continue;
			for (asel = sel->chain; asel; asel = asel->next) {
				if (asel->hover)
					sel->hover = true;
				if (asel->before) {
// before and after should only be on the base node of the chain
					if (asel == sel->chain)
						sel->before = true;
					else
						sel->error =
						    CSS_ERROR_INJECTHIGH;
				}
				if (asel->after) {
					if (asel == sel->chain)
						sel->after = true;
					else
						sel->error =
						    CSS_ERROR_INJECTHIGH;
				}
				if (asel->link) {
					if (!asel->tag)
						asel->tag = cloneString("a");
					else if (!stringEqual(asel->tag, "a")) {
						sel->error = CSS_ERROR_TAGLINK;
					}
				}
			}
		}
	}

// if all the selectors under d are in error, then d is error
	for (d = d1; d; d = d->next) {
		bool across = true;
		uchar ec = CSS_ERROR_NONE;
		if (d->error)
			continue;
		if (!d->selectors)	// should never happen
			continue;
		for (sel = d->selectors; sel; sel = sel->next) {
			if (!sel->error) {
// as good a time as any to compute specificity
				sel->spec = specificity(sel, d->underat);
				across = false;
				continue;
			}
			if (!ec)
				ec = sel->error;
			else if (ec != sel->error)
				ec = CSS_ERROR_MULTIPLE;
		}
		if (across)
			d->error = ec;
	}

// now for the rules
	for (d = d1; d; d = d->next) {
		char *r1, *r2;	// rule delimiters
		struct rule *rule, *rule2;
		if (d->error)
			continue;
		++loadcount;
		s = d->rhs;
		if (!*s) {
			d->error = CSS_ERROR_NORULE;
			continue;
		}

		r1 = s;
		while ((c = *s)) {
			if (c == '"' || c == '\'') {
				n = closeString(s + 1, c);
				if (n < 0)	// should never happen
					break;
				s += n + 1;
				continue;
			}
			if (c != ';') {
				++s;
				continue;
			}
			r2 = s;
			for (++s; *s; ++s)
				if (!isspace(*s) && *s != ';')
					break;
			while (r2 > r1 && isspace(r2[-1]))
				--r2;
// has to start with an identifyer, letters and hyphens, but look out,
// I have to allow for a leading * or _
// https://stackoverflow.com/questions/4563651/what-does-an-asterisk-do-in-a-css-property-name
			if (r1 < r2 && (*r1 == '*' || *r1 == '_')) {
				r1 = s;
				continue;
			}
			if (r1 == r2) {
// perhaps an extra ;
				r1 = s;
				continue;
			}
lastrule:
			for (t = r1; t < r2; ++t) {
				if (*t == ':')
					break;
				if (isupper(*t))
					*t = tolower(*t);
				if ((isdigit(*t) && t > r1) ||
				    isalpha(*t) || *t == '-')
					continue;
				d->error = CSS_ERROR_RATTR;
				break;
			}
			if (d->error)
				break;
			if (!*t) {
				d->error = CSS_ERROR_COLON;
				break;
			}
			if (t == r1) {
				d->error = CSS_ERROR_RATTR;
				break;
			}
			rule = allocZeroMem(sizeof(struct rule));
			if (d->rules) {
				rule2 = d->rules;
				while (rule2->next)
					rule2 = rule2->next;
				rule2->next = rule;
			} else
				d->rules = rule;
			a = allocMem(t - r1 + 1);
			memcpy(a, r1, t - r1);
			a[t - r1] = 0;
			camelCase(a);
			rule->atname = a;
			++t;
			while (isspace(*t))
				++t;
			if (r2 > t) {
				a = allocMem(r2 - t + 1);
				memcpy(a, t, r2 - t);
				a[r2 - t] = 0;
				rule->atval = a;
				unstring(a);
			} else
				rule->atval = emptyString;
			r1 = s;
		}

		if (r1 < s && !d->error && *r1 != '*' && *r1 != '_') {
// There should have been a final ; but oops.
// process the last rule as above.
			r2 = s;
			goto lastrule;
		}

		if (!d->rules) {
			d->error = CSS_ERROR_NORULE;
			continue;
		}
	}

// gather error statistics
	for (d = d1; d; d = d->next) {
		bool across = true;
		uchar ec = CSS_ERROR_NONE;
		if (d->error) {
			if (d->error < CSS_ERROR_DELIM) {
				++loadcount;
				++errorBuckets[d->error];
			}
			continue;
		}
		if (!d->selectors)	// should never happen
			continue;
		for (sel = d->selectors; sel; sel = sel->next) {
			++loadcount;
			if (!sel->error) {
				across = false;
				continue;
			}
			if (!ec)
				ec = sel->error;
			else if (ec != sel->error)
				ec = CSS_ERROR_MULTIPLE;
			++errorBuckets[sel->error];
		}
		if (across)
			d->error = ec;
	}

	cssPiecesPrint(d1);

	return d1;
}

static void cssParseLeft(struct desc *d)
{
	char *s = d->lhs;
	char *a1 = s;		// start of the atomic selector
	char *a2, *t;
	char c, last_c = 0, combin;
	struct sel *sel = 0;	// the selector being built
	struct sel *sel2;
	struct asel *asel, *asel2;
	int n;

	if (!s[0]) {
		d->error = CSS_ERROR_NOSEL;
		return;
	}

	while ((c = *s)) {
		if (c == '"' || c == '\'' || c == '[') {
			n = closeString(s + 1, c);
			if (n < 0)	// should never happen
				break;
			s += n + 1;
			last_c = 0;
			continue;
		}
// Ambiguous, ~ is combinator or part of [foo~=bar].
// Simplistic check here for ~=
// I don't really need it any more, now that I treat [ stuf ] as a string.
// But I still need the next one.
// Ambiguous, + is combinator or part of "nth_child(n+3)
// Simplistic check here, next selector should not begin with a digit
		if ((c == '~' && s[1] == '=') || (c == '+' && isdigit(s[1]))) {
			last_c = c;
			++s;
			continue;
		}

		if (last_c == '\\') {
			last_c = 0;
			++s;
			continue;
		}
// :not( code, rather like closing a string.
		if (!strncmp(s, ":not(", 5)) {
			n = closeString(s + 5, '(');
			if (n < 0)	// should never happen
				break;
			s += n + 5;
			last_c = 0;
			continue;
		}

		combin = 0;	// look for combinator
		a2 = s;
		while (strchr(", \t\n\r>~+", c)) {
			if (isspace(c)) {
				if (!combin)
					combin = ' ';
			} else {
				if (combin && combin != ' ')
					break;
				combin = c;
				last_c = c;
			}
			c = *++s;
		}
		if (!combin) {
			last_c = c;
			++s;
			continue;
		}
// it's a combinator or separator
		last_c = c;
		if (a2 == a1) {	// empty piece
// I'll allow it if it's just an extra comma
			if (combin == ',' && !sel) {
				a1 = s;
				continue;
			}
// empty piece could be first in the selector, as in querySelector(">p")
			if (!sel) {
				sel = allocZeroMem(sizeof(struct sel));
				if (!d->selectors)
					d->selectors = sel2 = sel;
				else
					sel2->next = sel, sel2 = sel;
			}
			sel->error = CSS_ERROR_SEL0;
			break;
		}

		t = allocMem(a2 - a1 + 1);
		memcpy(t, a1, a2 - a1);
		t[a2 - a1] = 0;
		asel = allocZeroMem(sizeof(struct asel));
		asel->part = t;
		asel->combin = combin;
		if (!sel) {
			sel = allocZeroMem(sizeof(struct sel));
			if (!d->selectors)
				d->selectors = sel2 = sel;
			else
				sel2->next = sel, sel2 = sel;
		}
// I'm reversing the order of the atomic selectors here
		asel2 = sel->chain;
		sel->chain = asel;
		asel->next = asel2;

		cssAtomic(asel);
// pass characteristics of the atomic selector back up to the selector
		if (asel->error && !sel->error)
			sel->error = asel->error;
		if (combin == ',')
			sel = 0;
		a1 = s;
	}

// This is the last piece, pretend like combin is a comma
	combin = ',';
	a2 = s;
	if (a2 == a1) {		// empty piece
		if (!sel) {
			if (!d->selectors)
				d->error = CSS_ERROR_NOSEL;
			return;
		}
		sel->error = CSS_ERROR_SEL0;
		return;
	}
	t = allocMem(a2 - a1 + 1);
	memcpy(t, a1, a2 - a1);
	t[a2 - a1] = 0;
	asel = allocZeroMem(sizeof(struct asel));
	asel->part = t;
	asel->combin = combin;
	if (!sel) {
		sel = allocZeroMem(sizeof(struct sel));
		if (!d->selectors)
			d->selectors = sel;
		else
			sel2->next = sel;
	}
	asel2 = sel->chain;
	sel->chain = asel;
	asel->next = asel2;
	cssAtomic(asel);
	if (asel->error && !sel->error)
		sel->error = asel->error;
}

// determine the tag and build the chain of modifiers
static void cssAtomic(struct asel *a)
{
	char c, last_c;
	char *m1;		// demarkate the modifier
	int n;
	char *s = a->part;
	char *tag = cloneString(s);
	char *t = strpbrk(tag, ".[#:");
	if (t)
		*t = 0, s += (t - tag);
	else
		s = emptyString;
	if (!*tag || stringEqual(tag, "*"))
		nzFree(tag), tag = 0;
	if (tag) {
		for (t = tag; *t; ++t) {
			if (isupper(*t))
				*t = tolower(*t);
			if ((isdigit(*t) && t > tag) ||
			    isalpha(*t) || *t == '-')
				continue;
			a->error = CSS_ERROR_TAG;
			nzFree(tag);
			return;
		}
		a->tag = tag;
	}
// tag set, time for modifiers
	if (!*s)
		return;
	m1 = s;
	++s;
	last_c = 0;

	while ((c = *s)) {
		if (c == '"' || c == '\'') {
			last_c = 0;
			n = closeString(s + 1, c);
			if (n < 0)	// should never happen
				break;
			s += n + 1;
			continue;
		}

		if (s == m1 + 1 && !strncmp(m1, ":not(", 5)) {
			last_c = 0;
			n = closeString(s + 4, '(');
			if (n < 0)	// should never happen
				break;
			s += n + 4;
			continue;
		}
// I assume \ is an escape, though this could fail  foo\\:
		if (!strchr(".[#:", c) || last_c == '\\') {
			++s;
			last_c = c;
			continue;
		}

		cssModify(a, m1, s);
		m1 = s;
		++s;
	}

// last modifier
	cssModify(a, m1, s);
}

static void cssModify(struct asel *a, const char *m1, const char *m2)
{
	struct mod *mod;
	char *t, *w, *propname;
	char c, h;
	int n = m2 - m1;
	static const char *const okcolon[] = {
		"first-child", "last-child", "only-child", "checked",
		"first-of-type", "last-of-type", "only-of-type",
		"empty", "disabled", "enabled", "read-only", "read-write",
		"scope", "root",
		0
	};

	if (n == 1)		// empty
		return;
	t = allocMem(n + 1);
	memcpy(t, m1, n);
	t[n] = 0;

	mod = allocZeroMem(sizeof(struct mod));
	mod->part = t;
// add this to the end of the chain
	if (a->modifiers) {
		struct mod *mod2 = a->modifiers;
		while (mod2->next)
			mod2 = mod2->next;
		mod2->next = mod;
	} else
		a->modifiers = mod;

// Handle not() first because it's weird.
	if (!strncmp(t, ":not(", 5)) {
		struct desc d0;	// dummy descriptor for not()
		if (t[n - 1] != ')') {
			a->error = CSS_ERROR_UNSUP;
			return;
		}
		t[--n] = 0;
		strmove(t, t + 5);
		trim(t);
		memset(&d0, 0, sizeof(d0));
		d0.lhs = t;
		cssParseLeft(&d0);
		if (!d0.selectors)	//   :not()
			return;
		mod->negate = true;
		if (d0.selectors->next) {
// more than one chain beneath
			struct sel *sel, *sel2;
			sel = d0.selectors;
			while (sel) {
				chainFree(sel->chain);
				sel2 = sel->next;
				free(sel);
				sel = sel2;
			}
			a->error = CSS_ERROR_MANYNOT;
			return;
		}
// one chain, that's good
		mod->notchain = d0.selectors->chain;
		free(d0.selectors);
		a->error = d0.error;	// pass the error upward
// should we check for before after hover?
		return;
	}
// See if the modifier makes sense
	h = t[0];
	switch (h) {
	case ':':
		if (stringEqual(t, ":visited")
		    || stringEqual(t, ":active") || stringEqual(t, ":focus")) {
			a->error = CSS_ERROR_DYN;
			return;
		}
		if (stringEqual(t, ":hover")) {
			a->hover = true;
			return;
		}
		if (stringEqual(t, ":link")) {
			a->link = true;
			return;
		}
// :lang(it) becomes [lang|=it], but more
		if (!strncmp(t, ":lang(", 6) && t[n - 1] == ')') {
			t[n - 1] = 0;
			return;
		}
		if (t[n - 1] == ')' &&
		    (!strncmp(t, ":nth-child(", 11) ||
		     !strncmp(t, ":nth-last-child(", 16) ||
		     !strncmp(t, ":nth-of-type(", 13) ||
		     !strncmp(t, ":nth-last-of-type(", 18))) {
			t[n - 1] = 0;
			spaceCrunch(t, false, false);
			return;
		}
		if (stringEqual(t, ":before")) {
			a->before = true;
			return;
		}
		if (stringEqual(t, ":after")) {
			a->after = true;
			return;
		}
		if (stringEqual(t, ":first-line") ||
		    stringEqual(t, ":last-line") ||
		    stringEqual(t, ":first-letter") ||
		    stringEqual(t, ":last-letter")) {
			a->error = CSS_ERROR_PE;
			return;
		}
		if (stringInList(okcolon, t + 1) < 0) {
			a->error = CSS_ERROR_UNSUP;
			return;
		}
		break;

	case '#':
	case '.':
		if (h == '.')
			mod->isclass = true;
		else
			mod->isid = true;
		propname = (h == '.' ? "[class~=" : "[id=");
		w = allocMem(n + strlen(propname) + 1);
		sprintf(w, "%s%s]", propname, t + 1);
		nzFree(t);
		mod->part = t = w;
		n = strlen(t);
// fall through

	case '[':
		if (t[n - 1] != ']') {
			a->error = CSS_ERROR_RB;
			return;
		}
		t[--n] = 0;	// lop off ]
// remove whitespace from either side of =
		w = strchr(t, '=');
// I assume = is not in quotes, not part of the attribute name
// Thus I'm postponing the call to unstring.
		if (w) {
			char *y = w + 1;
			skipWhite2(&y);
			if (y > w + 1)
				strmove(w + 1, y);
			if (strchr("|~^$*", w[-1]))
				--w;
			for (y = w - 1; isspace(*y); --y) ;
			++y;
			if (w > y)
				strmove(y, w);
		}
// We might have had ["a "=b] now it's safe to undo the quotes
		unstring(t);
		for (w = t + 1; (c = *w); ++w) {
			if (c == '=')
				break;
			if (strchr("|~^$*", c) && w[1] == '=') {
				++w;
				break;
			}
			if (isupper(c))
				*w = tolower(c);
		}
		break;

	default:
// not sure how we would get here
		a->error = CSS_ERROR_UNSUP;
		return;
	}			// switch
}

static void frameFromWindow(jsobjtype thisobj)
{
	Frame *f;
	for (f = &(cw->f0); f; f = f->next)
		if (f->winobj == thisobj)
			break;
	if (f) {
		cf = f;
	} else {
		debugPrint(3, " can't find frame for window object %p",
			   thisobj);
	}
}

void cssDocLoad(jsobjtype thisobj, char *start, bool pageload)
{
	Frame *save_cf = cf;
	struct cssmaster *cm;
	bool recompile = false;
	frameFromWindow(thisobj);
	cm = cf->cssmaster;
	if (!cm) {
		cf->cssmaster = cm = allocZeroMem(sizeof(struct cssmaster));
		readShortCache(cm);
	}
// This could be run again and again, if the style nodes change.
	if (cm->descriptors) {
		debugPrint(3,
			   "free and recompile css descriptors due to dom changes");
		cssPiecesFree(cm->descriptors);
		recompile = true;
	}
	cm->descriptors = cssPieces(start);
	if (recompile)
		debugPrint(3, "css complete");
	if (!cm->descriptors)
		goto done;
	if (debugCSS) {
		FILE *f = fopen(cssDebugFile, "a");
		if (f) {
			fprintf(f, "%s end\n", errorMessage[CSS_ERROR_DELIM]);
			fclose(f);
		}
	}

	if (!pageload)
		goto done;

	cssStats();

	build_doclist(0);
	hashBuild();
	hashPrint();
	cssEverybody();
	debugPrint(3, "%d css assignments", bulktotal);
	hashFree();
	nzFree(doclist);

done:
	cf = save_cf;
}

static void chainFree(struct asel *asel)
{
	struct asel *asel2;
	struct mod *mod, *mod2;
	while (asel) {
		mod = asel->modifiers;
		while (mod) {
			mod2 = mod->next;
			nzFree(mod->part);
			if (mod->notchain)
				chainFree(mod->notchain);
			free(mod);
			mod = mod2;
		}
		nzFree(asel->part);
		nzFree(asel->tag);
		asel2 = asel->next;
		free(asel);
		asel = asel2;
	}
}

static void cssPiecesFree(struct desc *d)
{
	struct desc *d2;
	struct sel *sel, *sel2;
	struct rule *r, *r2;
	if (!d)
		return;		// nothing to do
	free(d->lhs);		// the base string
	while (d) {
		sel = d->selectors;
		while (sel) {
			chainFree(sel->chain);
			sel2 = sel->next;
			free(sel);
			sel = sel2;
		}
		r = d->rules;
		while (r) {
			free(r->atname);
			nzFree(r->atval);
			r2 = r->next;
			free(r);
			r = r2;
		}
		d2 = d->next;
		free(d);
		d = d2;
	}
}

void cssFree(Frame *f)
{
	struct shortcache *c;
	struct cssmaster *cm = f->cssmaster;
	if (!cm)
		return;
	if (cm->descriptors)
		cssPiecesFree(cm->descriptors);
	while ((c = cm->cache)) {
		cm->cache = c->next;
		nzFree(c->url);
		nzFree(c->data);
		free(c);
	}
	free(cm);
	f->cssmaster = 0;
}

// for debugging
static FILE *cssfile;
static void chainPrint(struct asel *asel)
{
	const struct mod *mod;
	while (asel) {
		char *tag = asel->tag;
		if (!tag)
			tag = (asel->modifiers ? emptyString : "*");
		if (asel->combin != ',')
			fprintf(cssfile, "%c",
				(asel->combin == ' ' ? '^' : asel->combin));
		fprintf(cssfile, "%s", tag);
		for (mod = asel->modifiers; mod; mod = mod->next) {
			if (mod->negate) {
				if (mod->notchain) {
					fprintf(cssfile, ":not(");
					chainPrint(mod->notchain);
					fprintf(cssfile, ")");
				} else
					fprintf(cssfile, ":not()");
				continue;
			}
			if (!strncmp(mod->part, "[class~=", 8))
				fprintf(cssfile, ".%s", mod->part + 8);
			else if (!strncmp(mod->part, "[id=", 4))
				fprintf(cssfile, "#%s", mod->part + 4);
			else
				fprintf(cssfile, "%s", mod->part);
		}
		asel = asel->next;
	}
}

static void cssPiecesPrint(const struct desc *d)
{
	const struct sel *sel;
	const struct rule *r;

	if (!debugCSS)
		return;
	cssfile = fopen(cssDebugFile, "a");
	if (!cssfile)
		return;
	if (!d) {
		fclose(cssfile);
		return;
	}

	for (; d; d = d->next) {
		if (d->error) {
			if (d->error == CSS_ERROR_DELIM) {
				char which = d->lhs[8];
				if (which == '0')
					fprintf(cssfile, "%s start %s\n",
						errorMessage[d->error],
						d->lhs + 9);
				if (which == '1')
					fprintf(cssfile, "%s open %s\n",
						errorMessage[d->error],
						d->lhs + 9);
				if (which == '2')
					fprintf(cssfile, "%s close\n",
						errorMessage[d->error]);
			} else
				fprintf(cssfile, "<%s>%s\n",
					errorMessage[d->error], d->lhs);
			continue;
		}
		for (sel = d->selectors; sel; sel = sel->next) {
			if (sel != d->selectors)
				fprintf(cssfile, ",");
			if (sel->error)
				fprintf(cssfile, "<%s|",
					errorMessage[sel->error]);
			chainPrint(sel->chain);
		}
		fprintf(cssfile, "{");
		for (r = d->rules; r; r = r->next)
			fprintf(cssfile, "%s:%s;", r->atname, r->atval);
		fprintf(cssfile, "}\n");
	}
	fclose(cssfile);
}

/*********************************************************************
This is a special routine for the selector :lang(foo).
You could almost replace it with :[lang=|foo], except, that same selector
is compared against all the ancestors. Ugh.
*********************************************************************/

static bool languageSpecial(Tag *t, jsobjtype obj, const char *lang)
{
	char *v;
	bool valloc;
	int rc, l = strlen(lang);
	jsobjtype cx = cf->cx;

top:
	v = 0;
	valloc = false;
	if (t)
		obj = t->jv, cx = t->f0->cx;
	if (bulkmatch && t)
		v = (char *)attribVal(t, "lang");
	else if (obj) {
		v = get_property_string_0(cx, obj, "lang");
		valloc = true;
	}
	if (!v)
		goto up;

	rc = (strncmp(v, lang, l) || !(v[l] == 0 || v[l] == '-'));
	if (valloc)
		nzFree(v);
	return !rc;

up:
	if (t) {
		if ((t = t->parent) && t->action != TAGACT_DOC)
			goto top;
		return false;
	}
	if ((obj = get_property_object_0(cx, obj, "parentNode")) &&
	    get_property_number_0(cx, obj, "nodeType") != 9)
		goto top;
	return false;
}

/*********************************************************************
A helpful spread routine to find the siblings of where you are.
Returns 0 if you are at the top and siblings are not meaningful.
Otherwise allocate an array, which you must free.
Return is the length of the aray.
*********************************************************************/

struct sibnode {
	char tag[MAXTAGNAME];
	int nodeType;
	int myself;
	union {
		Tag *t;
		jsobjtype j;
	} u;
};
static struct sibnode *sibs;

static int spread(Tag *t, jsobjtype obj)
{
	int ns = 0;		// number of siblings
	int i, ntype, me_index = -1;
	jsobjtype pobj, children, w;
	jsobjtype cx;

	sibs = NULL;

	if (t) {
		Tag *tp, *u;
		if (!(tp = t->parent) || tp->action == TAGACT_DOC)
			return 0;
		for ((u = tp->firstchild); u; u = u->sibling) {
			if (u == t)
				me_index = ns;
			++ns;
		}
		if (me_index < 0)	// should never happen
			return 0;
		sibs = allocMem(sizeof(struct sibnode) * ns);
		for (i = 0, (u = tp->firstchild); i < ns; ++i, u = u->sibling) {
			strcpy(sibs[i].tag, u->info->name);
			ntype = 1;
			if (u->action == TAGACT_TEXT)
				ntype = 3;
			if (u->action == TAGACT_DOC)
				ntype = 9;
			if (u->action == TAGACT_COMMENT)
				ntype = 8;
			sibs[i].nodeType = ntype;
			sibs[i].myself = (i == me_index);
		}
		return ns;
	}
// bummer, have to go by objects
	cx = cf->cx;
	pobj = get_property_object_0(cx, obj, "parentNode");
	if (!pobj || get_property_number_0(cx, pobj, "nodeType") == 9)
		return 0;
	children = get_property_object_0(cx, pobj, "childNodes");
	if (!children)
		return 0;
	ns = get_arraylength_0(cx, children);
	if (!ns)
		return 0;
	sibs = allocMem(sizeof(struct sibnode) * ns);
	for (i = 0; i < ns; ++i) {
		char *v;
		int l;
		w = get_array_element_object_0(cx, children, i);
		if (!w) {	// should never happen
			free(sibs);
			return 0;
		}
		sibs[i].nodeType = get_property_number_0(cx, w, "nodeType");
		v = get_property_string_0(cx, w, "nodeName");
		if (!v) {
			strcpy(sibs[i].tag, "@x");
		} else {
			l = strlen(v);
			if (l >= MAXTAGNAME)
				l = MAXTAGNAME - 1;
			strncpy(sibs[i].tag, v, l);
			sibs[i].tag[l] = 0;
			nzFree(v);
		}
		sibs[i].myself = 0;
		if (w == obj)
			sibs[i].myself = 1, me_index = i;
	}
	if (me_index >= 0)
		return ns;
	free(sibs);
	return 0;
}

// when we only need elements, do nothing if ns == 0
static int spreadElem(int ns)
{
	int i, j;
	if (!ns)
		return 0;
	for (i = j = 0; i < ns; ++i) {
		if (sibs[i].nodeType != 1)
			continue;
		if (i > j)
			sibs[j] = sibs[i];
		++j;
	}
	if (!j)
		free(sibs);
	return j;
}

// Restrict the list further to elements of the same type
static int spreadType(int ns)
{
	int i, j;
	char mytype[MAXTAGNAME];
	if (!ns)
		return 0;
// find myself
	for (i = 0; i < ns; ++i)
		if (sibs[i].myself)
			break;
	if (i == ns) {
		free(sibs);
		return 0;
	}
	strcpy(mytype, sibs[i].tag);
	for (i = j = 0; i < ns; ++i) {
		if (!stringEqual(sibs[i].tag, mytype))
			continue;
		if (i > j)
			sibs[j] = sibs[i];
		++j;
	}
	if (!j)
		free(sibs);
	return j;
}

// Like spread but for children, not siblings. Still I use the sibs array.
static int spreadKids(Tag *t, jsobjtype obj)
{
	int ns = 0;		// number of children
	int i, ntype;
	jsobjtype children, w;
	jsobjtype cx = cf->cx;

	sibs = NULL;

	if (t) {
		Tag *u;
		for ((u = t->firstchild); u; u = u->sibling)
			++ns;
		if (!ns)
			return 0;
		sibs = allocMem(sizeof(struct sibnode) * ns);
		for (i = 0, (u = t->firstchild); i < ns; ++i, u = u->sibling) {
			strcpy(sibs[i].tag, u->info->name);
			ntype = 1;
			if (u->action == TAGACT_TEXT)
				ntype = 3;
			if (u->action == TAGACT_DOC)
				ntype = 9;
			if (u->action == TAGACT_COMMENT)
				ntype = 8;
			sibs[i].nodeType = ntype;
			sibs[i].myself = 0;
			sibs[i].u.t = u;
		}
		return ns;
	}

	children = get_property_object_0(cx, obj, "childNodes");
	if (!children)
		return 0;
	ns = get_arraylength_0(cx, children);
	if (!ns)
		return 0;
	sibs = allocMem(sizeof(struct sibnode) * ns);
	for (i = 0; i < ns; ++i) {
		char *v;
		int l;
		w = get_array_element_object_0(cx, children, i);
		if (!w) {	// should never happen
			free(sibs);
			return 0;
		}
		sibs[i].nodeType = get_property_number_0(cx, w, "nodeType");
		v = get_property_string_0(cx, w, "nodeName");
		if (!v) {
			strcpy(sibs[i].tag, "@x");
		} else {
			l = strlen(v);
			if (l >= MAXTAGNAME)
				l = MAXTAGNAME - 1;
			strncpy(sibs[i].tag, v, l);
			sibs[i].tag[l] = 0;
			nzFree(v);
		}
		sibs[i].myself = 0;
		sibs[i].u.j = w;
	}
	return ns;
}

// Things like enabled, clik, read-only, only make sense for input fields;
// they are false for other tags.
static bool inputLike(Tag *t, jsobjtype obj, int flavor)
{
	jsobjtype cx = cf->cx;
	char *v;
	bool rc;
	int j, action;
	static const char *const inputtags[] = {
		"INPUT", "SELECT", "BUTTON", 0
	};
	static const char *const clicktypes[] = {
		"button", "submit", "reset", "checkbox", "radio", 0
	};
	static const char *const nwtypes[] = {
		"button", "submit", "reset", "hidden", 0
	};
	if (t) {
		action = t->action;
		rc = (action == TAGACT_INPUT || action == TAGACT_BUTTON ||
		      action == TAGACT_SELECT);
		if (!rc)
			return false;
	} else {
		v = get_property_string_0(cx, obj, "nodeName");
		if (!v || !*v)
			return false;
		j = stringInList(inputtags, v);
		nzFree(v);
		if (j < 0)
			return false;
		if (j == 0)
			action = TAGACT_INPUT;
		if (j == 1)
			action = TAGACT_SELECT;
		if (j == 2)
			action = TAGACT_BUTTON;
	}
	if (flavor == 1) {	// clickable
		v = get_property_string_0(cx, obj, "type");
		rc = (action == TAGACT_BUTTON || (action == TAGACT_INPUT &&
						  v
						  && stringInList(clicktypes,
								  v) >= 0));
		nzFree(v);
		return rc;
	}
	if (flavor == 2) {	// writable
		v = get_property_string_0(cx, obj, "type");
		rc = (action == TAGACT_SELECT || (action == TAGACT_INPUT &&
						  v
						  && stringInList(nwtypes,
								  v) < 0));
		nzFree(v);
		return rc;
	}
	return true;
}

/*********************************************************************
Match a node against an atomic selector.
One of t or obj should be nonzero. It's more efficient with t.
If bulkmatch is true, then the document has loaded and no js has run.
If t->jclass is not set, there's no point dipping into js
to see if it has been set dynamically by a script.
This is only called from qsaMatchChain, as part of a chain of atomic selectors.
That chain is considered, or not considered, based on before after hover
criteria in qsa2() and qsaMatchGroup(), so we need not test for those here.
*********************************************************************/

static bool qsaMatchChain(Tag *t, jsobjtype obj,
			  const struct asel *a);

static bool qsaMatch(Tag *t, jsobjtype obj, const struct asel *a)
{
	bool rc;
	struct mod *mod;
	jsobjtype cx = cf->cx;

	if (a->tag) {
		const char *nn;
		if (t)
			nn = t->nodeName;
		else
			nn = get_property_string_0(cx, obj, "nodeName");
		if (!nn)	// should never happen
			return false;
		rc = stringEqualCI(nn, a->tag);
		if (!t)
			cnzFree(nn);
		if (!rc)
			return false;
	}
// now step through the modifyers
	for (mod = a->modifiers; mod; mod = mod->next) {
		char *p = mod->part;
		bool negate = mod->negate;
		char c = p[0];
		int i, ntype, ns;

		if (!c)		// empty modifier
			continue;

		if (negate) {
			if (mod->notchain) {
				if (qsaMatchChain(t, obj, mod->notchain))
					return false;
// the notchain fails, which is what we want, so on we go.
				continue;
			}
// empty not()
			continue;
		}

		if (mod->isclass && t
		    && (bulkmatch || (gcsmatch && a->combin == ','))) {
			char *v = t->jclass;
			char *u = p + 8;
			int l = strlen(u);
			char *q;
			if (!v)
				v = emptyString;
			while ((q = strstr(v, u))) {
				v += l;
				if (q > t->jclass && !isspace(q[-1]))
					continue;
				if (q[l] && !isspace(q[l]))
					continue;
				goto next_mod;
			}
			return false;
		}

		if (mod->isid && t
		    && (bulkmatch || (gcsmatch && a->combin == ','))) {
			char *v = t->id;
			if (!v)
				v = emptyString;
			if (stringEqual(v, p + 4))
				goto next_mod;
			return false;
		}

		if (t)
			obj = t->jv, cx = t->f0->cx;

// for bulkmatch we use the attributes on t,
// not js, t is faster.

		if (c == '[') {
			bool valloc = false;
			int l;
			char cutc = 0;
			char *value, *v, *v0, *q;
			char *cut = strchr(p, '=');
			if (cut) {
				value = cut + 1;
				skipWhite2(&value);
				l = strlen(value);
				if (strchr("|~^$*", cut[-1]))
					--cut;
				cutc = *cut;
				*cut = 0;	// I'll put it back
			}
			v = 0;
			if (bulkmatch && t)
				v = (char *)attribVal(t, p + 1);
			else if (obj) {
				if (!strncmp(p + 1, "data-", 5)) {
					jsobjtype ds =
					    get_property_object_0(cx, obj,
								    "dataset");
					if (ds) {
						char *k = cloneString(p + 6);
						camelCase(k);
						v = get_property_string_0(cx, ds,
									    k);
						nzFree(k);
					}
				} else
					v = get_property_string_0(cx, obj, p + 1);
				valloc = true;
			}
			if (cut)
				*cut = cutc;
			if (!v)
				return false;
			if (!cutc) {
				if (valloc)
					nzFree(v);
				goto next_mod;
			}
			if (cutc == '=') {	// easy case
				rc = (stringEqual(v, value));
				if (valloc)
					nzFree(v);
				if (rc)
					goto next_mod;
				return false;
			}
			if (cutc == '|') {
				rc = (!strncmp(v, value, l)
				      && (v[l] == 0 || v[l] == '-'));
				if (valloc)
					nzFree(v);
				if (rc)
					goto next_mod;
				return false;
			}
			if (cutc == '^') {
				rc = !strncmp(v, value, l);
				if (valloc)
					nzFree(v);
				if (rc)
					goto next_mod;
				return false;
			}
			if (cutc == '$') {
				int l1 = strlen(v);
				int l2 = strlen(value);
				rc = false;
				if (l1 >= l2)
					rc = !strncmp(v + l1 - l2, value, l);
				if (valloc)
					nzFree(v);
				if (rc)
					goto next_mod;
				return false;
			}
			if (cutc == '*') {
				rc = (! !strstr(v, value));
				if (valloc)
					nzFree(v);
				if (rc)
					goto next_mod;
				return false;
			}
// now value is a word inside v
			v0 = v;
			while ((q = strstr(v, value))) {
				v += l;
				if (q > v0 && !isspace(q[-1]))
					continue;
				if (q[l] && !isspace(q[l]))
					continue;
				if (valloc)
					nzFree(v0);
				goto next_mod;
			}
			if (valloc)
				nzFree(v0);
			return false;
		}
// At this point c should be a colon.
		if (c != ':')
			return false;

		if (stringEqual(p, ":link") || stringEqual(p, ":hover") ||
		    stringEqual(p, ":before") || stringEqual(p, ":after"))
			continue;

		if (!strncmp(p, ":lang(", 6)) {
			if (languageSpecial(t, obj, p + 6))
				goto next_mod;
			return false;
		}

		if (!strncmp(p, ":nth-child(", 11) ||
		    !strncmp(p, ":nth-last-child(", 16) ||
		    !strncmp(p, ":nth-of-type(", 13) ||
		    !strncmp(p, ":nth-last-of-type(", 18)) {
			int coef, constant, d;
			bool n_present = false, d_present = false, last =
			    false, oftype = false;
			char *s;

			if (p[5] == 'l')
				last = true;
			if (strstr(p, "of-type"))
				oftype = true;
			p = strchr(p, '(') + 1;
			if (stringEqual(p, "even"))
				p = "2n";
			if (stringEqual(p, "odd"))
				p = "2n+1";

// parse the formula
			s = p;
			if (*s == '-')
				++s;
			if (!*s)
				goto nth_bad;
			if (isdigit(*s))
				d = strtol(s, &s, 10), d_present = true;
			if (!*s) {
				constant = (*p == '-' ? -d : d);
				goto nth_good;
			}
			if (*s != 'n')
				goto nth_bad;
			n_present = true;
			if (d_present)
				coef = (*p == '-' ? -d : d);
			else
				coef = (*p == '-' ? -1 : 1);
			++s;
			constant = 0;
			if (!*s)
				goto nth_good;
			if (*s != '+' && *s != '-')
				goto nth_bad;
			if (*s == '+')
				++s;
			constant = 1;
			if (*s == '-')
				constant = -1, ++s;
			if (!isdigit(*s))
				goto nth_bad;
			d = strtol(s, &s, 10);
			if (*s)
				goto nth_bad;
			constant *= d;

nth_good:
// prevent divide by 0   :nth_child(0n+3)
			if (n_present && coef == 0)
				n_present = false;

			ns = spread(t, obj);
			ns = spreadElem(ns);
			if (oftype)
				ns = spreadType(ns);
			if (!ns)
				return false;
// find myself
			for (i = 0; i < ns; ++i)
				if (sibs[i].myself)
					break;
			rc = false;
			if (i < ns) {
				if (last)
					i = (ns - 1) - i;
				++i;	// numbers start at 1
				if (n_present) {
					i -= constant;
					if (i % coef)
						rc = false;
					else
						rc = (i / coef) >= 0;
				} else {
					rc = (i == constant);
				}
			}
			free(sibs);
			return rc;

nth_bad:
			debugPrint(3,
				   "unrecognized nth_child(%s), treating as false",
				   p);
			return false;
		}

		if (stringEqual(p, ":first-child") ||
		    stringEqual(p, ":last-child") ||
		    stringEqual(p, ":only-child") ||
		    stringEqual(p, ":first-of-type") ||
		    stringEqual(p, ":last-of-type") ||
		    stringEqual(p, ":only-of-type")) {
			ns = spread(t, obj);
			ns = spreadElem(ns);
			if (strstr(p, "of-type"))
				ns = spreadType(ns);
			if (!ns)
				return false;
			if (p[1] == 'f')
				rc = sibs[0].myself;
			if (p[1] == 'l')
				rc = sibs[ns - 1].myself;
			if (p[1] == 'o')
				rc = (ns == 1 && sibs[0].myself);
			free(sibs);
			if (rc)
				goto next_mod;
			return false;
		}

		if (stringEqual(p, ":root") || stringEqual(p, ":scope")) {
/*********************************************************************
You know what's missing from :root? The stand alone selector :root,
which some say should match the current node.
Thing is, all selectors are suppose to match the nodes below,
not including the current node. So * matches everything below.
I thus make a list of the nodes below and use that to start the chain.
Therefore :root will never match.
Should I make an exception for :root? If so, how best to implement it?
Meantime, this code manages :root up the chain, as in :root>div,
all the div sections just below the current node.
*********************************************************************/
			if (t) {
				if (!rootobj) {
					if ((t->action == TAGACT_HTML))
						goto next_mod;
					return false;
				}
				if ((t->jv == rootobj))
					goto next_mod;
				return false;
			}
			if (!rootobj) {
				const char *a =
				    get_property_string_0(cx, obj, "nodeName");
				rc = (a && stringEqualCI(a, "document"));
				cnzFree(a);
				if (rc)
					goto next_mod;
				return false;
			}
			if ((obj == rootobj))
				goto next_mod;
			return false;
		}

		if (stringEqual(p, ":empty")) {
			ns = spreadKids(t, obj);
			rc = true;	// empty
			for (i = 0; i < ns; ++i) {
				jsobjtype w;
				char *v;
				ntype = sibs[i].nodeType;
				if (ntype == 8)	// comment
					continue;
				if (ntype != 3) {	// not text node
					rc = false;
					break;
				}
// text node has to be empty.
				if (t) {
					Tag *u = sibs[i].u.t;
					if (bulkmatch) {
						if (u->textval && *u->textval) {
							rc = false;
							break;
						}
						continue;
					}
					w = u->jv;
				} else {
					w = sibs[i].u.j;
				}
				if (!w) {	// should not happen
					rc = false;
					break;
				}
				v = get_property_string_0(cx, w, "data");
				rc = (!v || !*v);
				nzFree(v);
				if (!rc)
					break;
			}
			nzFree(sibs);
			if (rc)
				goto next_mod;
			return false;
		}

		if (stringEqual(p, ":enabled") || stringEqual(p, ":disabled")) {
			rc = false;
			if (inputLike(t, obj, 0)) {
				if (t && bulkmatch)
					rc = t->disabled;
				else
					rc = get_property_bool_0(cx, obj,
								   "disabled");
				if (p[1] == 'e')
					rc ^= 1;
			}
			if (rc)
				goto next_mod;
			return false;
		}

		if (stringEqual(p, ":read-only")
		    || stringEqual(p, ":read-write")) {
			rc = false;
			if (inputLike(t, obj, 2)) {
				if (t && bulkmatch)
					rc = t->rdonly;
				else
					rc = get_property_bool_0(cx, obj,
								   "readonly");
				if (p[6] == 'w')
					rc ^= 1;
			}
			if (rc)
				goto next_mod;
			return false;
		}

		if (stringEqual(p, ":checked")) {
			rc = false;
			if (inputLike(t, obj, 1)) {
				if (t && bulkmatch)
					rc = t->checked;
				else
					rc = get_property_bool_0(cx, obj,
								   "checked");
			}
			if (rc)
				goto next_mod;
			return false;
		}

		return false;	// unrecognized

next_mod:	;
	}

	return true;		// all modifiers pass
}

/*********************************************************************
There's an important optimization we don't do.
qsa1 picks the best list based on the atomic selector it is looking at,
but if you look at the entire chain you can sometimes do better.
This really happens.
.foobar * {color:green}
Everything below the node of class foobar is green.
Right now * matches every node, then we march up the tree to see if
.foobar is abov it, and if yes then the chain matches and it's green.
It would be better to build a new list dynamically, everything below .foobar,
and apply the rules to those nodes, which we were going to do anyways.
Well these under * chains are somewhat rare,
and the routine runs fast enough, even on stackoverflow.com, the worst site,
so I'm not going to implement chain level optimization today.
Called from qsaMatchGroup and from qsa1.
This is recursive, since we must explore every path.
*********************************************************************/

static bool qsaMatchChain(Tag *t, jsobjtype obj,
			  const struct asel *a)
{
	Tag *u;
	jsobjtype cx = cf->cx;

	if (!a)			// should never happen
		return false;

	if(t)
		cx = t->f0->cx;

	switch (a->combin) {
	case ',':
// this is the base node; it has to match.
		if (!qsaMatch(t, obj, a))
			break;
// now look down the rest of the chain
onetime:
		if (!a->next)
			return true;
		return qsaMatchChain(t, obj, a->next);

	case '+':
		if (t) {
			if (!t->parent)
				break;
			u = t->parent->firstchild;
			if (!u || u == t)
				break;
			for (; u; u = u->sibling)
				if (u->sibling == t)
					break;
			if (!u)
				break;
			if (!qsaMatch(u, obj, a))
				break;
			t = u;
			goto onetime;
		}
// now by objects
		obj = get_property_object_0(cx, obj, "previousSibling");
		if (!obj)
			break;
		if (!qsaMatch(t, obj, a))
			break;
		goto onetime;

	case '~':
		if (t) {
			if (!t->parent)
				break;
			u = t->parent->firstchild;
			for (; u; u = u->sibling) {
				if (u == t)
					return false;
				if (!qsaMatch(u, obj, a))
					continue;
				if (!a->next)
					return true;
				if (qsaMatchChain(u, obj, a->next))
					return true;
			}
			break;
		}
		while ((obj = get_property_object_0(cx, obj, "previousSibling"))) {
			if (!qsaMatch(t, obj, a))
				continue;
			if (!a->next)
				return true;
			if (qsaMatchChain(t, obj, a->next))
				return true;
		}
		break;

	case '>':
		if (t) {
			t = t->parent;
			if (!t || t->action == TAGACT_DOC)
				break;
			if (!qsaMatch(t, obj, a))
				break;
			goto onetime;
		}
		obj = get_property_object_0(cx, obj, "parentNode");
		if (!obj)
			break;
		if (get_property_number_0(cx, obj, "numType") == 9)
			break;
		if (!qsaMatch(t, obj, a))
			break;
		goto onetime;

	case ' ':
		if (t) {
			while ((t = t->parent) && t->action != TAGACT_DOC) {
				if (!qsaMatch(t, obj, a))
					continue;
				if (!a->next)
					return true;
				if (qsaMatchChain(t, obj, a->next))
					return true;
			}
			break;
		}
		while ((obj = get_property_object_0(cx, obj, "parentNode")) &&
		       get_property_number_0(cx, obj, "nodeType") != 9) {
			if (!qsaMatch(t, obj, a))
				continue;
			if (!a->next)
				return true;
			if (qsaMatchChain(t, obj, a->next))
				return true;
		}
		break;

	}			// switch

	return false;
}

// Only called from cssApply, as part of getComputedStyle.
static bool qsaMatchGroup(Tag *t, jsobjtype obj, struct desc *d)
{
	struct sel *sel;
	FILE *f = 0;
	if (d->error)
		return false;
	d->highspec = 0;
	if (debugCSS)
		f = fopen(cssDebugFile, "a");
	for (sel = d->selectors; sel; sel = sel->next) {
		if (sel->error)
			continue;
// Only the plain descriptors for getComputedStyle().
		if (sel->before | sel->after | sel->hover)
			continue;
		if (qsaMatchChain(t, obj, sel->chain)) {
// debug getComputedStyle, which can also be a directed debug
// of the whole css system, focusing on a particular node.
			if (f)
				fprintf(f, "gcs|%s|%s|%d\n", d->lhs, d->rhs,
					sel->spec);
			if (sel->spec > d->highspec)
				d->highspec = sel->spec;
		}
	}
	if (f)
		fclose(f);
	return (d->highspec > 0);
}

/*********************************************************************
Match a selector against a list of nodes.
If list is not given then doclist is used - all nodes in the document.
the result is an allocated array of nodes from the list that match.
Only called from qsa2.
*********************************************************************/

static Tag **qsa1(const struct sel *sel)
{
	int i, n = 1;
	Tag *t;
	Tag **a, **list;

	list = bestListAtomic(sel->chain);
	if (!onematch && list) {
// allocate room for all, in case they all match.
		for (n = 0; list[n]; ++n) ;
	}
	a = allocMem((n + 1) * sizeof(Tag *));
	if (!list) {
		a[0] = 0;
		return a;
	}
	n = 0;
// querySelectorAll does not match the root, only everything below.
	i = 0;
	if (skiproot && list[i])
		++i;
	for (; (t = list[i]); ++i) {
		if (qsaMatchChain(t, 0, sel->chain)) {
			a[n++] = t;
			if (sel->spec > t->highspec)
				t->highspec = sel->spec;
			if (onematch)
				break;
		}
	}
	a[n] = 0;
	if (!onematch)
		a = reallocMem(a, (n + 1) * sizeof(Tag *));
	return a;
}

// merge two lists of nodes, giving a third.
static Tag **qsaMerge(Tag **list1, Tag **list2)
{
	int n1, n2, i1, i2, v1, v2, n;
	Tag **a;
	bool g1 = true, g2 = true;
// make sure there's room for both lists
	for (n1 = 0; list1[n1]; ++n1) ;
	for (n2 = 0; list2[n2]; ++n2) ;
	n = n1 + n2;
	a = allocMem((n + 1) * sizeof(Tag *));
	n = i1 = i2 = 0;
	if (!n1)
		g1 = false;
	if (!n2)
		g2 = false;
	while (g1 | g2) {
		if (!g1) {
			a[n++] = list2[i2++];
			if (i2 == n2)
				g2 = false;
			continue;
		}
		if (!g2) {
			a[n++] = list1[i1++];
			if (i1 == n1)
				g1 = false;
			continue;
		}
		v1 = list1[i1]->seqno;
		v2 = list2[i2]->seqno;
		if (v1 < v2) {
			a[n++] = list1[i1++];
			if (i1 == n1)
				g1 = false;
		} else if (v2 < v1) {
			a[n++] = list2[i2++];
			if (i2 == n2)
				g2 = false;
		} else {
			i1++;
			a[n++] = list2[i2++];
			if (i1 == n1)
				g1 = false;
			if (i2 == n2)
				g2 = false;
		}
	}
	a[n] = 0;
	return a;
}

// querySelectorAll on a group, uses merge above.
// Called from javascript querySelectorAll and from cssEverybody.

static Tag **qsa2(struct desc *d)
{
	Tag **a = 0, **a2, **a_new;
	struct sel *sel;
	for (sel = d->selectors; sel; sel = sel->next) {
		if (sel->error)
			continue;
		if ((sel->before && matchtype != 1) ||
		    (sel->after && matchtype != 2) ||
		    (!(sel->before | sel->after) && matchtype))
			continue;
		if (sel->hover ^ matchhover)
			continue;
		a2 = qsa1(sel);
		if (a) {
			a_new = qsaMerge(a, a2);
			nzFree(a);
			nzFree(a2);
			a = a_new;
			if (onematch && a[0] && a[1])
				a[1] = 0;
		} else
			a = a2;
	}
	return a;
}

// Build the list of nodes in the document.
// Gee, this use to be one line of javascript, via getElementsByTagName().
static void build1_doclist(Tag *t);
static int doclist_cmp(const void *v1, const void *v2);
static void build_doclist(Tag *top)
{
	doclist_n = 0;
	doclist_a = 500;
	doclist = allocMem((doclist_a + 1) * sizeof(Tag *));
	if (top) {
		build1_doclist(top);
	} else {
// the html tag should always be there
		if (cf->htmltag) {
			build1_doclist(cf->htmltag);
		} else {
			if (cf->headtag)
				build1_doclist(cf->headtag);
			if (cf->bodytag)
				build1_doclist(cf->bodytag);
		}
	}
	doclist[doclist_n] = 0;
	qsort(doclist, doclist_n, sizeof(Tag *), doclist_cmp);
}

// recursive
static void build1_doclist(Tag *t)
{
	if (doclist_n == doclist_a) {
		doclist_a += 500;
		doclist =
		    reallocMem(doclist,
			       (doclist_a + 1) * sizeof(Tag *));
	}
	doclist[doclist_n++] = t;
	t->highspec = 0;
	if (topmatch)		// top only
		return;
// can't descend into another frame
	if (t->action == TAGACT_FRAME)
		return;
	for (t = t->firstchild; t; t = t->sibling)
		build1_doclist(t);
}

static int doclist_cmp(const void *v1, const void *v2)
{
	const Tag *const *p1 = v1;
	const Tag *const *p2 = v2;
	int d = (*p1)->seqno - (*p2)->seqno;
	return d;
}

static Tag **qsaInternal(const char *selstring, Tag *top)
{
	struct desc *d0;
	Tag **a;
	char *s;
// Compile the selector. The string has to be allocated.
	if (!selstring)
		selstring = emptyString;
	s = allocMem(strlen(selstring) + 20);
	sprintf(s, "%s{c:g}", selstring);
	d0 = cssPieces(s);
	if (!d0) {
		debugPrint(3, "querySelectorAll(%s) yields no descriptors",
			   selstring);
		return 0;
	}
	if (d0->next) {
		debugPrint(3,
			   "querySelectorAll(%s) yields multiple descriptors",
			   selstring);
		cssPiecesFree(d0);
		return 0;
	}
	if (d0->error) {
		debugPrint(3, "querySelectorAll(%s): %s", selstring,
			   errorMessage[d0->error]);
		cssPiecesFree(d0);
		return 0;
	}
	build_doclist(top);
	skiproot = ! !top;
	if (topmatch)
		skiproot = false;
	a = qsa2(d0);
	nzFree(doclist);
	cssPiecesFree(d0);
	return a;
}

// turn an array of html tags into an array of objects
static jsobjtype objectize(Tag **list)
{
	jsobjtype cx = cf->cx;
	int i, j;
	const Tag *t;
	jsobjtype ao;
	delete_property_0(cx, cf->winobj, "qsagc");
	ao = instantiate_array_0(cx, cf->winobj, "qsagc");
	if (!ao || !list)
		return ao;
	for (i = j = 0; (t = list[i]); ++i) {
		if (!t->jv)	// should never happen
			continue;
		set_array_element_object_0(cx, ao, j++, t->jv);
	}
	return ao;
}

jsobjtype querySelectorAll(const char *selstring, jsobjtype topobj)
{
	Tag *top = 0, **a;
	jsobjtype ao;
	rootobj = topobj;
	if (topobj)
		top = tagFromJavaVar(topobj);
	a = qsaInternal(selstring, top);
	ao = objectize(a);
	nzFree(a);
	return ao;
}

// this one just returns the node
jsobjtype querySelector(const char *selstring, jsobjtype topobj)
{
	Tag *top = 0, **a;
	jsobjtype node = 0;
	rootobj = topobj;
	if (topobj)
		top = tagFromJavaVar(topobj);
	onematch = true;
	a = qsaInternal(selstring, top);
	onematch = false;
	if (!a)
		return 0;
	if (a[0])
		node = a[0]->jv;
	nzFree(a);
	return node;
}

bool querySelector0(const char *selstring, jsobjtype topobj)
{
	Tag *top = 0, **a;
	jsobjtype node = 0;
	rootobj = topobj;
	if (topobj)
		top = tagFromJavaVar(topobj);
	onematch = topmatch = true;
	a = qsaInternal(selstring, top);
	onematch = topmatch = false;
	if (!a)
		return false;
	if (a[0])
		node = a[0]->jv;
	nzFree(a);
	return (node ? true : false);
}

// replace each attr(foo) with the value of attribute foo
static char *attrify(jsobjtype obj, char *line)
{
	jsobjtype cx = cf->cx;
	char *s;
	int sl;
	char *t, *t1, *t2, *v;
	s = initString(&sl);
	for (t = line; *t; t = t2) {
		t1 = strstr(t, "attr(");
		if (!t1)
			break;
		t2 = strchr(t1, ')');
		if (!t2)
			break;
		*t2++ = 0;
		stringAndBytes(&s, &sl, t, t1 - t);
		t1 += 5;
		v = get_property_string_0(cx, obj, t1);
		if (v) {
			stringAndString(&s, &sl, v);
			nzFree(v);
		}
	}
	stringAndString(&s, &sl, t);
	return s;
}

/*********************************************************************
do_rules is called from 3 different places, under 3 very different contexts.
1. getComputedStyle(node), creates a new style object s,
loops through all the extant css descriptors, matches them against node,
and then applies the rules to s.
The obj argument is the newly created style object s.
Because matchtype is 0, the before and after selectors don't match,
and they shouldn't, because before after attributes
wouldn't apply to node.style but rather node.textnode.style.
matchhover is false so hover selectors won't match either, nor should they.
2. The cssText setter on a style object.
There are no selectors here, just rules, as supplied by the calling function.
This is the easy case.
obj is the style object, also supplied by the calling function.
3. Apply all css descriptors to all nodes at javascript startup.
This is indicated by bulkmatch = true.
3a. matchtype is 0 for the plain selectors. obj = node.style.
3b. matchtype is 1 for the before selectors. obj = node.
3c. matchtype is 2 for the after selectors. obj = node.
Then repeat 3a 3b 3c with matchhover = true.
This is done only to see if the node becomes visible on hover.
Only looking for display=something or visibility=visible.
Set a flag if that is found.
*********************************************************************/

static void do_rules(jsobjtype obj, struct rule *r0, int highspec)
{
	jsobjtype cx = cf->cx;
	struct rule *r, *r1;
	char *s, *s_attr;
	int sl;
	jsobjtype textobj, original = obj;
	static const char jl[] = "text 0x0, 0x0";
	char *a;
	int spec;

	if (!obj)
		return;

/*********************************************************************
before and after can't act on a select node,as that is an array,
the way I have implemented it, so sneaking in a text node
is sometimes treated as an option, and that is a disaster!
Corner case? Not really, because some people write *:before, hitting every node.
And if you do it twice the second call could add a text node
to the text node you just added in the first call, and so on,
so we don't want to apply before or after to text nodes.
Or options, or html (screwing up the head body structure),
or iframe (which should only have document below);
in fact it's easier to list the tags that allow it.
*********************************************************************/

	if (matchtype) {
		bool forbidden = true;
		static const char *const ok2inject[] = {
			"A", "ADDRESS", "Q", "BLOCKQUOTE", "BODY", "BUTTON",
			"CAPTION", "CITE",
			"DIV", "FOOTER", "H1", "H2", "H3", "H4", "H5", "H6",
			"HEADER", "LABEL", "LI", "MENU",
			"P", "SPAN", "TD", "TH", "XMP",
			"LISTING", "STRONG", "EM", "S", "STRIKE", "I", "U", "B",
			0
		};
		s = get_property_string_0(cx, obj, "nodeName");
		if (s && stringInList(ok2inject, s) >= 0)
			forbidden = false;
		nzFree(s);
		if (forbidden)
			return;
	}

	if (matchtype == 1) {	// before
		if (get_property_bool_0(cx, obj, "inj$before")) {
			textobj = get_property_object_0(cx, obj, "firstChild");
		} else {
			textobj =
			    instantiate_0(cx, cf->winobj, "eb$inject",
					    "TextNode");
			if (!textobj)	// should never happen
				return;
			javaSetsLinkage(false, 'c', textobj, jl);
			run_function_onearg_0(cx, obj, "prependChild", textobj);
// It is now linked in and safe from gc
			delete_property_0(cx, cf->winobj, "eb$inject");
			set_property_bool_0(cx, obj, "inj$before", true);
		}
	}

	if (matchtype == 2) {	// after
		if (get_property_bool_0(cx, obj, "inj$after")) {
			textobj = get_property_object_0(cx, obj, "lastChild");
		} else {
			textobj =
			    instantiate_0(cx, cf->winobj, "eb$inject",
					    "TextNode");
			if (!textobj)	// should never happen
				return;
			javaSetsLinkage(false, 'c', textobj, jl);
			run_function_onearg_0(cx, obj, "appendChild", textobj);
// It is now linked in and safe from gc
			delete_property_0(cx, cf->winobj, "eb$inject");
			set_property_bool_0(cx, obj, "inj$after", true);
		}
	}

	if (matchtype)
		obj = get_property_object_0(cx, textobj, "style");
// obj is now the style object, ready for attributes

	s = initString(&sl);
	for (r = r0; r; r = r->next) {
		bool has;
		enum ej_proptype what;

// hover only looks for display visible
		if (matchhover) {
			if ((stringEqual(r->atname, "display") &&
			     strlen(r->atval) && !stringEqual(r->atval, "none"))
			    ||
			    (stringEqual(r->atname, "visibility") &&
			     strlen(r->atval)
			     && !stringEqual(r->atval, "hidden")))
				set_property_bool_0(cx, obj, "hov$vis", true);
// what about color anything other than transparent?
// If invisible because color = transparent, then color = red unhides it.
			if (stringEqual(r->atname, "color") && strlen(r->atval)
			    && !stringEqual(r->atval, "transparent"))
				set_property_bool_0(cx, obj, "hov$col", true);
			continue;
		}
// special code for before after content
		if (matchtype && stringEqual(r->atname, "content")) {
			if (stringEqual(r->atval, "none"))
				continue;
			if (sl)
				stringAndChar(&s, &sl, ' ');
			stringAndString(&s, &sl, r->atval);
			continue;
		}
// if it appears to be part of the prototype, and not the object,
// I won't write it.
		has = has_property_0(cx, obj, r->atname);
		what = typeof_property_0(cx, obj, r->atname);
		if (has && !what)
			continue;

// don't repeat an attribute. Hardly ever happens except for acid test 0.
		for (r1 = r0; r1 != r; r1 = r1->next)
			if (stringEqual(r1->atname, r->atname))
				break;
		if (r1 != r)
			continue;

// Don't write if the specificity is less
		a = allocMem(strlen(r->atname) + 6);
		sprintf(a, "%s$$scy", r->atname);
		spec = get_property_number_0(cx, obj, a);
		if (spec > highspec) {
			free(a);
			continue;
		}

		if (bulkmatch)
			++bulktotal;
		set_property_string_0(cx, obj, r->atname, r->atval);
		set_property_number_0(cx, obj, a, highspec);
		free(a);
	}

	if (!sl)
		return;

// put a space between the injected text and the original text
	stringAndChar(&s, &sl, ' ');
	if (matchtype == 2) {
// oops, space belongs on the other side
		memmove(s + 1, s, sl - 1);
		s[0] = ' ';
	}
// turn attr(foo) into obj[foo]
	s_attr = attrify(original, s);
	nzFree(s);
	s = s_attr;
	set_property_string_0(cx, textobj, "data", s);
	nzFree(s);
	set_property_bool_0(cx, textobj, "inj$css", true);
}

/*********************************************************************
This is the native function for getComputedStyle().
If you call frames[i].contentWindow.getComputedStyle(), it is very important
that we apply the css for that frame, not the frame we are currently in.
For that reason, the first argument is "this", which I assume is a global
window object. I march down the frames and find it, and that is the root for
the css rules. If there's ever a document.head.getComputedStyle or some such,
where "this" is not the window object, then we have to make some changes.
*********************************************************************/

void cssApply(jsobjtype thisobj, jsobjtype node, jsobjtype destination)
{
	Frame *save_cf = cf;
	jsobjtype cx;
	Tag *t = tagFromJavaVar(node);
	struct cssmaster *cm;
	struct desc *d;

	if (!t)			// should never happen
		return;

	frameFromWindow(thisobj);
	cx = cf->cx;

// I think the root is document, not the current node, but that is not clear.
	rootobj = 0;
	cm = cf->cssmaster;
	if (!cm)
		goto done;

// it's a getComputedStyle match
	gcsmatch = true;
	nzFree(t->jclass);
	t->jclass = get_property_string_0(cx, node, "class");
	nzFree(t->id);
	t->id = get_property_string_0(cx, node, "id");

	for (d = cm->descriptors; d; d = d->next) {
		if (qsaMatchGroup(t, node, d))
			do_rules(destination, d->rules, d->highspec);
	}

done:
	cf = save_cf;
	gcsmatch = false;
}

void cssText(jsobjtype node, const char *rulestring)
{
	struct desc *d0;
	char *s;
// check arguments.
	if (!node || !rulestring)
		return;
// Is this suppose to replace the existing properties, or add to them?
// I have no idea; for now it just adds to them.
	if (!*rulestring)
		return;		// empty
// Compile the selector. The string has to be allocated.
	s = allocMem(strlen(rulestring) + 20);
	sprintf(s, "*{%s}", rulestring);
	d0 = cssPieces(s);
	if (!d0) {
		debugPrint(3, "cssText(%s) yields no descriptors", rulestring);
		return;
	}
	if (d0->next) {
		debugPrint(3,
			   "cssText(%s) yields multiple descriptors",
			   rulestring);
		cssPiecesFree(d0);
		return;
	}
	if (d0->error) {
		debugPrint(3, "cssText(%s): %s", rulestring,
			   errorMessage[d0->error]);
		cssPiecesFree(d0);
		return;
	}
	do_rules(node, d0->rules, 100000);
	cssPiecesFree(d0);
}

static int key_cmp(const void *s, const void *t)
{
// there shouldn't be any null or empty keys here.
	return strcmp(((struct hashhead *)s)->key, ((struct hashhead *)t)->key);
}

// First two parameters are pass by address, so we can pass them back.
// Third is true if keys are allocated.
// Empty list is possible, corner case.
static void hashSortCrunch(struct hashhead **hp, int *np, bool keyalloc)
{
	struct hashhead *h = *hp;
	int n = *np;
	struct hashhead *mark = 0, *v;
	int i, j, distinct = 0;

	qsort(h, n, sizeof(struct hashhead), key_cmp);

// /bin/uniq -c
	for (i = 0; i < n; ++i) {
		v = h + i;
		if (!mark)
			mark = v, v->n = 1, distinct = 1;
		else if (stringEqual(mark->key, v->key))
			++(mark->n), v->n = 0;
		else
			mark = v, v->n = 1, ++distinct;
	}

// now crunch
	mark = 0;
	for (i = 0; i < n; ++i) {
		v = h + i;
		if (!v->n) {	// same key
			mark->body[j++] = v->t;
			if (keyalloc)
				nzFree(v->key);
			continue;
		}
// it's a new key
		if (!mark)
			mark = h;
		else
			mark->body[j] = 0, ++mark;
		if (mark < v)
			(*mark) = (*v);
		mark->body = allocMem((mark->n + 1) * sizeof(Tag *));
		mark->body[0] = mark->t;
		mark->t = 0;
		j = 1;
	}

	if (mark) {
		mark->body[j] = 0;
		++mark;
		if (mark - h != distinct)
			printf("css hash mismatch %zu versus %d\n",
			       mark - h, distinct);
		distinct = mark - h;
	}
// make sure there's something, even if distinct = 0
	h = reallocMem(h, (distinct + 1) * sizeof(struct hashhead));
	*hp = h, *np = distinct;
}

static void hashBuild(void)
{
	Tag *t;
	int i, j, a;
	struct hashhead *h;
	static const char ws[] = " \t\r\n\f";	// white space
	char *classcopy, *s, *u;

	build_doclist(0);

// tags first, every node should have a tag.
	h = allocZeroMem(doclist_n * sizeof(struct hashhead));
	for (i = j = 0; i < doclist_n; ++i) {
		t = doclist[i];
		if (!(t->nodeName && t->nodeName[0]))
			continue;
		h[j].key = t->nodeName;
		h[j].t = t;
		++j;
	}
	hashSortCrunch(&h, &j, false);
	hashtags = h, hashtags_n = j;

// a lot of nodes won't have id, so this alloc is overkill, but oh well.
	h = allocZeroMem(doclist_n * sizeof(struct hashhead));
	for (i = j = 0; i < doclist_n; ++i) {
		t = doclist[i];
		if (!(t->id && t->id[0]))
			continue;
		h[j].key = t->id;
		h[j].t = t;
		++j;
	}
	hashSortCrunch(&h, &j, false);
	hashids = h, hashids_n = j;

/*********************************************************************
Last one is class but it's tricky.
If class is "foo bar", t must be hashed under foo and under bar.
A combinatorial explosion is possible here.
If class is "a b c d e" then we should hash under: "a" "b" "c" "d" "e"
"a b" "b c" "c d" "d e" "a b c" "b c d" "c d e"
"a b c d" "b c d e" and "a b c d e".
I'm not going to worry about that.
Just the indifidual words and the whole thing.
*********************************************************************/

	a = 500;
	h = allocMem(a * sizeof(struct hashhead));
	for (i = j = 0; i < doclist_n; ++i) {
		t = doclist[i];
		if (!(t->jclass && t->jclass[0]))
			continue;
		if (j == a) {
			a += 500;
			h = reallocMem(h, a * sizeof(struct hashhead));
		}
		classcopy = cloneString(t->jclass);
		h[j].key = classcopy;
		h[j].t = t;
		++j;

		if (!strpbrk(t->jclass, ws))
			continue;	// no spaces

		classcopy = cloneString(t->jclass);
		s = classcopy;
		while (isspace(*s))
			++s;
		while (*s) {
			char cutc = 0;	// cut character
			u = strpbrk(s, ws);
			if (u)
				cutc = *u, *u = 0;
// s is the individual word
			if (j == a) {
				a += 500;
				h = reallocMem(h, a * sizeof(struct hashhead));
			}
			h[j].key = cloneString(s);
			h[j].t = t;
			++j;
			if (!cutc)
				break;
			s = u + 1;
			while (isspace(*s))
				++s;
		}

		nzFree(classcopy);
	}
	hashSortCrunch(&h, &j, true);
	hashclasses = h, hashclasses_n = j;
}

static void hashFree(void)
{
	struct hashhead *h;
	int i;
	for (i = 0; i < hashtags_n; ++i) {
		h = hashtags + i;
		free(h->body);
	}
	free(hashtags);
	hashtags = 0, hashtags_n = 0;
	for (i = 0; i < hashids_n; ++i) {
		h = hashids + i;
		free(h->body);
	}
	free(hashids);
	hashids = 0, hashids_n = 0;
	for (i = 0; i < hashclasses_n; ++i) {
		h = hashclasses + i;
		free(h->body);
		free(h->key);
	}
	free(hashclasses);
	hashclasses = 0, hashclasses_n = 0;
	nzFree(doclist);
	doclist = 0, doclist_n = 0;
}

static void hashPrint(void)
{
	FILE *f;
	struct hashhead *h;
	int i;
	if (!debugCSS)
		return;
	f = fopen(cssDebugFile, "a");
	if (!f)
		return;
	fprintf(f, "nodes %d\n", doclist_n);
	fprintf(f, "tags:\n");
	for (i = 0; i < hashtags_n; ++i) {
		h = hashtags + i;
		fprintf(f, "%s %d\n", h->key, h->n);
	}
	fprintf(f, "ids:\n");
	for (i = 0; i < hashids_n; ++i) {
		h = hashids + i;
		fprintf(f, "%s %d\n", h->key, h->n);
	}
	fprintf(f, "classes:\n");
	for (i = 0; i < hashclasses_n; ++i) {
		h = hashclasses + i;
		fprintf(f, "%s %d\n", h->key, h->n);
	}
	fprintf(f, "nodes end\n");
	fclose(f);
}

// use binary search to find the key in a hashlist
static struct hashhead *findKey(struct hashhead *list, int n, const char *key)
{
	struct hashhead *h;
	int rc, i, l = -1, r = n;
	while (r - l > 1) {
		i = (l + r) / 2;
		h = list + i;
		rc = strcmp(h->key, key);
		if (!rc)
			return h;
		if (rc > 0)
			r = i;
		else
			l = i;
	}
	return 0;		// not found
}

// Return the best list to scan for a given atomic selector.
// This could be no list at all, if the selector includes .foo,
// and there is no node of class foo.
// Or it could be doclist if there is no tag and no class or id modifiers.
static Tag **bestListAtomic(struct asel *a)
{
	struct mod *mod;
	struct hashhead *h, *best_h;
	int n, best_n = 0;

	if (!bulkmatch)
		return doclist;

	if (a->tag) {
		h = findKey(hashtags, hashtags_n, a->tag);
		if (!h)
			return 0;
		best_n = h->n, best_h = h;
	}

	for (mod = a->modifiers; mod; mod = mod->next) {
		if (mod->negate)
			continue;
		if (mod->isid) {
			h = findKey(hashids, hashids_n, mod->part + 4);
			if (!h)
				return 0;
			n = h->n;
			if (!best_n || n < best_n)
				best_n = n, best_h = h;
		}
		if (mod->isclass) {
			h = findKey(hashclasses, hashclasses_n, mod->part + 8);
			if (!h)
				return 0;
			n = h->n;
			if (!best_n || n < best_n)
				best_n = n, best_h = h;
		}
	}

	return (best_n ? best_h->body : doclist);
}

// Cross all selectors and all nodes at document load time.
// Assumes the hash tables have been built.
static void cssEverybody(void)
{
	struct cssmaster *cm = cf->cssmaster;
	struct desc *d0 = cm->descriptors, *d;
	Tag **a, **u;
	jsobjtype style;
	jsobjtype cx;
	Tag *t;
	int l;

	bulkmatch = true;
	bulktotal = 0;
	skiproot = false;
	rootobj = NULL;

	for (l = 0; l < 6; ++l) {
		matchhover = (l >= 3);
		matchtype = l % 3;
		for (d = d0; d; d = d->next) {
			if (d->error)
				continue;
			a = qsa2(d);
			if (!a)
				continue;
			for (u = a; (t = *u); ++u) {
				if (!t->jv)
					continue;
		cx = t->f0->cx;
				if (matchtype)
					style = t->jv;
				else {
					style =
					    get_property_object_0(cx, t->jv,
								    "style");
					if (!style)
						continue;
				}
				do_rules(style, d->rules, t->highspec);
			}
			nzFree(a);
		}
	}
	bulkmatch = false;
	matchtype = 0;
	matchhover = false;
}
