/*********************************************************************
Parse css files into css descriptors, and apply those descriptors to nodes.
All this was written in js but was too slow.
Some sites have thousands of descriptors and hundreds of nodes,
e.g. www.stackoverflow.com with 5,050 descriptors.
*********************************************************************/

#include "eb.h"

#define cssDebugFile "/tmp/css"

#define CSS_ERROR_NONE 0
#define CSS_ERROR_NOSEL 1
#define CSS_ERROR_AT 2
#define CSS_ERROR_BRACES 3
#define CSS_ERROR_SEL0 4
#define CSS_ERROR_NORULE 5
#define CSS_ERROR_COLON 6
#define CSS_ERROR_RB 7
#define CSS_ERROR_TAG 8
#define CSS_ERROR_ATTR 9
#define CSS_ERROR_RATTR 10
#define CSS_ERROR_DYN 11
#define CSS_ERROR_INJECTNULL 12
#define CSS_ERROR_INJECTHIGH 13
#define CSS_ERROR_PE 14
#define CSS_ERROR_UNSUP 15
#define CSS_ERROR_MULTIPLE 16
#define CSS_ERROR_DELIM 17
#define CSS_ERROR_LAST 18

static const char *const errorMessage[] = {
	"ok",
	"no selectors",
	"@",
	"nested braces",
	"empty selector",
	"no rules",
	"rule no :",
	"modifier no ]",
	"bad tag",
	"bad selector attribute",
	"bad rule attribute",
	"dynamic",
	"inject null",
	"inject high",
	"pseudo element",
	": unsupported",
	"multiple",
	"==========",
	"==========",
	0
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
	char c;
	for (i = 0; (c = s[i]); ++i) {
		if (c == delim)
			return i + 1;
		if (c == '\\') {
			if (!s[++i])
				return i;
		}
// a css string should not contain an unescaped newline, so if you find one,
// something is wrong or misaligned. End the string here.
		if (c == '\n')
			return i + 1;
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
static void uncomment(char *s)
{
	char *s0 = s, *w = s;
	int n;
	char c;
	while ((c = *s)) {
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
		if (c != '/')
			goto copy;
		if (s[1] == '/') {
// Look out! People actually write @import url(http://blah) with no quotes,
// and the slashes trip the comment syntax, so check for this,
// in a rather crude way.
			if (s - s0 >= 3 && s[-1] == ':' &&
			    islower(s[-2]) && islower(s[-3]))
				goto copy;
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
	int n;
	for (n = 0; s[n]; ++n)
		if (!isspace(s[n]))
			break;
	if (n)
		strmove(s, s + n);
	n = strlen(s);
	while (n && isspace(s[n - 1]))
		--n;
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
	bool content;		// rhs includes content=blah
	struct sel *selectors;
	struct rule *rules;
};

// selector
struct sel {
	struct sel *next;
	uchar error;
	bool before, after;
	struct asel *chain;
};

// atomic selector
struct asel {
	struct asel *next;
	char *tag;
	char *part;
	bool before, after;
	uchar error;
	char combin;
	struct mod *modifiers;
};

// selector modifiers
struct mod {
	struct mod *next;
	char *part;
	bool isclass, isid, negate;
};

struct rule {
	struct rule *next;
	char *atname, *atval;
};

struct hashhead {
	char *key;
	struct htmlTag **body;
	int n;
	struct htmlTag *t;
};

static struct hashhead *hashtags, *hashids, *hashclasses;
static int hashtags_n, hashids_n, hashclasses_n;

struct cssmaster {
	struct desc *descriptors;
};

static void cssPiecesFree(struct desc *d);
static void cssPiecesPrint(const struct desc *d);
static void cssAtomic(struct asel *a);
static void cssModify(struct asel *a, const char *m1, const char *m2);
static bool onematch, bulkmatch, bulktotal;
static char matchtype;		// 0 plain 1 before 2 after
static struct htmlTag **doclist;
static int doclist_a, doclist_n;
static struct ebFrame *doclist_f;
static void build_doclist(struct htmlTag *top);
static void hashBuild(void);
static void hashFree(void);
static void hashPrint(void);
static struct htmlTag **bestListAtomic(struct asel *a);
static void cssEverybody(void);

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

/*********************************************************************
This is a really simple version of unstring. I don't decode \37 or \u1f.
Just \t and \n, and \ anything else is just the next character.
Improve this some day.
*********************************************************************/

static void unstring(char *s)
{
	char *w = s;
	char qc = 0;		// quote character
	char c;
	while ((c = *s)) {
		if (qc) {	// in quotes
			if (c == qc) {
				qc = 0;
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
			goto copy;
		}
		if (c == '"' || c == '\'') {
			qc = c;
			++s;
			continue;
		}
copy:
		*w++ = c;
		++s;
	}
	*w = 0;
}

// The input string is assumed allocated, it could be reallocated.
static struct desc *cssPieces(char *s)
{
	int bc = 0;		// brace count
	struct desc *d1 = 0, *d2, *d = 0;
	struct sel *sel, *sel2;
	struct asel *asel, *asel2;
	int n;
	char c;
	char *lhs;
	char *a, *t;
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
				debugPrint(3, "css source %s", newurl);
				*iu1 = 0;
				memset(&g, 0, sizeof(g));
				g.thisfile = cf->fileName;
				g.uriEncoded = true;
				g.url = newurl;
				a = NULL;
				if (httpConnect(&g)) {
					if (g.code == 200) {
						a = force_utf8(g.buffer,
							       g.length);
						if (!a)
							a = g.buffer;
						else
							nzFree(g.buffer);
					} else {
						nzFree(g.buffer);
						if (debugLevel >= 3)
							i_printf(MSG_GetCSS,
								 g.url, g.code);
					}
				} else {
					if (debugLevel >= 3)
						i_printf(MSG_GetCSS2, errorMsg);
				}
				if (!a)
					a = emptyString;
				t = allocMem(strlen(s) + strlen(a) +
					     strlen(newurl) + strlen(iu3) + 26 +
					     1);
				sprintf(t,
					"%s\n@ebdelim1%s{}\n%s\n@ebdelim2{}\n%s",
					s, newurl, a, iu3);
				nzFree(a);
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
			lhs = s;
			trim(d->lhs);
			trim(d->rhs);
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
		char combin;
		char *a1, *a2;	// bracket the atomic selector

		if (d->error)
			continue;
		s = d->lhs;
		if (!s[0]) {
			d->error = CSS_ERROR_NOSEL;
			continue;
		}
// our special code to insert a delimiter into debugging output
		if (!strncmp(s, "@ebdelim", 8)) {
			d->error = CSS_ERROR_DELIM;
			continue;
		}
// leading @ doesn't apply to edbrowse.
		if (s[0] == '@') {
			d->error = CSS_ERROR_AT;
			continue;
		}
		if (d->bc > 1) {
			d->error = CSS_ERROR_BRACES;
			continue;
		}

		a1 = s;		// start of the atomic selector
		sel = 0;	// the selector being built

		while ((c = *s)) {
			if (c == '"' || c == '\'') {
				n = closeString(s + 1, c);
				if (n < 0)	// should never happen
					break;
				s += n + 1;
				continue;
			}
			combin = 0;	// look for combinator
			a2 = s;
			while (strchr(", \t\n\r>+", c)) {
				if (isspace(c)) {
					if (!combin)
						combin = ' ';
				} else {
					if (combin && combin != ' ')
						break;
					combin = c;
				}
				c = *++s;
			}
			if (!combin) {
				++s;
				continue;
			}
// it's a combinator or separator
			if (a2 == a1) {	// empty piece
// I'll allow it if it's just an extra comma
				if (combin == ',' && !sel) {
					a1 = s;
					continue;
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
		if (a2 == a1) {	// empty piece
			if (!sel) {
				if (!d->selectors)
					d->error = CSS_ERROR_NOSEL;
				continue;
			}
			sel->error = CSS_ERROR_SEL0;
			continue;
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

// pull before and after up from atomic selector to selector
	for (d = d1; d; d = d->next) {
		if (d->error)
			continue;
		for (sel = d->selectors; sel; sel = sel->next) {
			if (sel->error)
				continue;
			for (asel = sel->chain; asel; asel = asel->next) {
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
			cssAttributeCrunch(a);
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

/*********************************************************************
In theory, :before and :after can inject text, but they rarely do.
They usually inject whitespace with a contrastihng background color
or an interesting image or decoration, before or after a button or whatever.
This means nothing to edbrowse.
Look for content other than whitespace.
Also, oranges.com uses content=none, I assume none is a reserved word for "".
If content is real then set a flag.
*********************************************************************/

				if (stringEqual(rule->atname, "content") &&
				    !stringEqual(a, "none")) {
					while (*a) {
						if (!isspace(*a)) {
							d->content = true;
							break;
						}
						++a;
					}
				}

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
// check for before after with no content
		if (d->content)
			continue;
		for (sel = d->selectors; sel; sel = sel->next)
			if (sel->before | sel->after)
				sel->error = CSS_ERROR_INJECTNULL;
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

	cssStats();
	cssPiecesPrint(d1);

	return d1;
}

// determine the tag, and build the chain of modifiers
static void cssAtomic(struct asel *a)
{
	char c;
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
// special code for :not(:whatever)
	if (!strncmp(s, "not(", 4) && s[4])
		s += 5;

	while ((c = *s)) {
		if (c == '"' || c == '\'') {
			n = closeString(s + 1, c);
			if (n < 0)	// should never happen
				break;
			s += n + 1;
			continue;
		}
		if (!strchr(".[#:", c)) {
			++s;
			continue;
		}
		cssModify(a, m1, s);
		m1 = s;
		++s;
// special code for :not(:whatever)
		if (!strncmp(s, "not(", 4) && s[4])
			s += 5;
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

// :not(foo) is jquery css syntax, whatever that is,
// and can have any kind of complex selector inside,
// but I'll do at least the simplest case, :not(modifier)
	if (!strncmp(t, ":not(", 5) && t[n - 1] == ')') {
		t[--n] = 0;
		strmove(t, t + 5);
		n -= 5;
		mod->negate = true;
	}
// See if the modifier makes sense
	h = t[0];
	switch (h) {
	case ':':
		if (stringEqual(t, ":hover") || stringEqual(t, ":visited")
		    || stringEqual(t, ":active") || stringEqual(t, ":focus")) {
			a->error = CSS_ERROR_DYN;
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
		if (!stringEqual(t, ":link") && !stringEqual(t, ":first-child")
		    && !stringEqual(t, ":last-child")
		    && !stringEqual(t, ":checked")) {
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
		for (w = t + 1; (c = *w); ++w) {
			if (c == '=')
				break;
			if (strchr("|~^*", c) && w[1] == '=') {
				++w;
				break;
			}
			if (isupper(c))
				*w = tolower(c);
			if ((isdigit(c) && w > t + 1) || isalpha(c) || c == '-')
				continue;
			a->error = CSS_ERROR_ATTR;
			return;
		}
		if (!*w)
			break;
		unstring(w);
// [foo=] isn't well defined. I'm going to call it [foo]
		if (w[1])
			break;
		*w-- = 0;
		if (strchr("|~^*", *w))
			*w = 0;
		break;

	default:
// we could get here via :not(crap)
		a->error = CSS_ERROR_UNSUP;
		return;
	}			// switch
}

void cssDocLoad(char *start)
{
	struct cssmaster *cm = cf->cssmaster;
	if (!cm)
		cf->cssmaster = cm = allocZeroMem(sizeof(struct cssmaster));
// This shouldn't be run twice for a given frame,
// but sometimes we do anyways fore debugging.
	if (cm->descriptors)
		cssPiecesFree(cm->descriptors);
	cm->descriptors = cssPieces(start);
	if (!cm->descriptors)
		return;
	if (debugCSS) {
		FILE *f = fopen(cssDebugFile, "a");
		if (f) {
			fprintf(f, "%s end\n", errorMessage[CSS_ERROR_DELIM]);
			fclose(f);
		}
	}
	build_doclist(0);
	hashBuild();
	hashPrint();
	cssEverybody();
	debugPrint(3, "%d css assignments", bulktotal);
	hashFree();
	nzFree(doclist);
}

static void cssPiecesFree(struct desc *d)
{
	struct desc *d2;
	struct sel *sel, *sel2;
	struct asel *asel, *asel2;
	struct mod *mod, *mod2;
	struct rule *r, *r2;
	if (!d)
		return;		// nothing to do
	free(d->lhs);		// the base string
	while (d) {
		sel = d->selectors;
		while (sel) {
			asel = sel->chain;
			while (asel) {
				mod = asel->modifiers;
				while (mod) {
					mod2 = mod->next;
					nzFree(mod->part);
					free(mod);
					mod = mod2;
				}
				nzFree(asel->part);
				nzFree(asel->tag);
				asel2 = asel->next;
				free(asel);
				asel = asel2;
			}
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

void cssFree(struct ebFrame *f)
{
	struct cssmaster *cm = f->cssmaster;
	if (!cm)
		return;
	if (cm->descriptors)
		cssPiecesFree(cm->descriptors);
	free(cm);
	f->cssmaster = 0;
}

// for debugging
static void cssPiecesPrint(const struct desc *d)
{
	FILE *f;
	const struct sel *sel;
	const struct asel *asel;
	const struct mod *mod;
	const struct rule *r;

	if (!debugCSS)
		return;
	f = fopen(cssDebugFile, "a");
	if (!f)
		return;
	if (!d) {
		fclose(f);
		return;
	}

	for (; d; d = d->next) {
		if (d->error) {
			if (d->error == CSS_ERROR_DELIM) {
				char which = d->lhs[8];
				if (which == '0')
					fprintf(f, "%s start %s\n",
						errorMessage[d->error],
						d->lhs + 9);
				if (which == '1')
					fprintf(f, "%s open %s\n",
						errorMessage[d->error],
						d->lhs + 9);
				if (which == '2')
					fprintf(f, "%s close\n",
						errorMessage[d->error]);
			} else
				fprintf(f, "<%s>%s\n", errorMessage[d->error],
					d->lhs);
			continue;
		}
		for (sel = d->selectors; sel; sel = sel->next) {
			if (sel != d->selectors)
				fprintf(f, ",");
			if (sel->error)
				fprintf(f, "<%s|", errorMessage[sel->error]);
			for (asel = sel->chain; asel; asel = asel->next) {
				char *tag = asel->tag;
				if (!tag)
					tag =
					    (asel->modifiers ? emptyString :
					     "*");
				if (asel->combin != ',')
					fprintf(f, "%c",
						(asel->combin ==
						 ' ' ? '^' : asel->combin));
				fprintf(f, "%s", tag);
				for (mod = asel->modifiers; mod;
				     mod = mod->next) {
					if (mod->negate)
						fprintf(f, "~");
					if (!strncmp(mod->part, "[class~=", 8))
						fprintf(f, ".%s",
							mod->part + 8);
					else if (!strncmp(mod->part, "[id=", 4))
						fprintf(f, "#%s",
							mod->part + 4);
					else
						fprintf(f, "%s", mod->part);
				}
			}
		}
		fprintf(f, "{");
		for (r = d->rules; r; r = r->next)
			fprintf(f, "%s:%s;", r->atname, r->atval);
		fprintf(f, "}\n");
	}
	fclose(f);
}

// Match a node against an atomic selector.
// One of t or obj should be nonzero.
// It's more efficient with t.
// If bulkmatch is true, then the document has loaded and no js has run.
// If t->class is not set, there's no point dipping into js
// to see if it has been set dynamically by a script.
static bool qsaMatch(struct htmlTag *t, jsobjtype obj, const struct asel *a)
{
	bool rc;
	struct mod *mod;
	jsobjtype pobj;		// parent object

	if ((a->before && matchtype != 1) ||
	    (a->after && matchtype != 2) ||
	    (!(a->before | a->after) && matchtype))
		return false;

	if (a->tag) {
		const char *nn;
		if (t)
			nn = t->nodeName;
		else
			nn = get_property_string_nat(obj, "nodeName");
		if (!nn)	// should never happen
			return false;
		rc = stringEqual(nn, a->tag);
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

// I'll assume that class and id, once set, are unchanging, like nodeName.
		if (mod->isclass && t) {
			char *v = t->class;
			char *u = p + 8;
			int l = strlen(u);
			char *q;
			if ((!v || !*v) && !bulkmatch && t->jv)	// dip into js
				t->class = v =
				    get_property_string_nat(t->jv, "class");
			if (!v)
				v = emptyString;
			while ((q = strstr(v, u))) {
				v += l;
				if (q > t->class && !isspace(q[-1]))
					continue;
				if (q[l] && !isspace(q[l]))
					continue;
				if (negate)
					return false;
				goto next_mod;
			}
			if (negate)
				goto next_mod;
			return false;
		}

		if (mod->isid && t) {
			char *v = t->id;
			if ((!v || !*v) && !bulkmatch && t->jv)	// dip into js
				t->id = v =
				    get_property_string_nat(t->jv, "id");
			if (!v)
				v = emptyString;
			if (stringEqual(v, p + 4) ^ negate)
				goto next_mod;
			return false;
		}

		if (t)
			obj = t->jv;

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
				l = strlen(value);
				if (strchr("|~^*", cut[-1]))
					--cut;
				cutc = *cut;
				*cut = 0;	// I'll put it back
			}
			v = 0;
			if (bulkmatch && t)
				v = (char *)attribVal(t, p + 1);
			else if (obj) {
				v = get_property_string_nat(obj, p + 1);
				valloc = true;
			}
			if (cut)
				*cut = cutc;
			if (!v || !*v) {
				if (negate)
					goto next_mod;
				return false;
			}
			if (!cutc) {
				if (valloc)
					nzFree(v);
				if (negate)
					return false;
				goto next_mod;
			}
			if (cutc == '=') {	// easy case
				rc = (stringEqual(v, value) ^ negate);
				if (valloc)
					nzFree(v);
				if (rc)
					goto next_mod;
				return false;
			}
			if (cutc == '|') {
				rc = (!strncmp(v, value, l)
				      && (v[l] == 0 || v[l] == '-'));
				rc ^= negate;
				if (valloc)
					nzFree(v);
				if (rc)
					goto next_mod;
				return false;
			}
			if (cutc == '^') {
				rc = (!strncmp(v, value, l) ^ negate);
				if (valloc)
					nzFree(v);
				if (rc)
					goto next_mod;
				return false;
			}
			if (cutc == '*') {
				rc = ((! !strstr(v, value)) ^ negate);
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
				if (negate)
					return false;
				goto next_mod;
			}
			if (valloc)
				nzFree(v0);
			if (negate)
				goto next_mod;
			return false;
		}
// At this point c should be a colon.
		if (c != ':')
			return false;

		if (stringEqual(p, ":link") ||
		    stringEqual(p, ":before") || stringEqual(p, ":after"))
			continue;

		if (stringEqual(p, ":checked")) {
			if (bulkmatch) {
				if (t->checked ^ negate)
					goto next_mod;
				return false;
			}
// This is very dynamic, better go to the js world.
			if (obj)
				rc = get_property_bool_nat(obj, "checked");
			else
				rc = t->checked;
			if (rc ^ negate)
				goto next_mod;
			return false;
		}

		if (stringEqual(p, ":first-child")) {
// t is more efficient, but this doesn't work for body and head,
// which have no parent in our representation.
			if (t && t->parent) {
				if ((t == t->parent->firstchild) ^ negate)
					goto next_mod;
				return false;
			}
// now by object
			pobj = get_property_object_nat(obj, "parentNode");
			if (!pobj)
				return false;
			if ((get_property_object_nat(pobj, "firstChild") ==
			     obj) ^ negate)
				goto next_mod;
			return false;
		}

		if (stringEqual(p, ":last-child")) {
			if (t && t->parent) {
				if (!t->sibling ^ negate)
					goto next_mod;
				return false;
			}
			pobj = get_property_object_nat(obj, "parentNode");
			if (!pobj)
				return false;
			if ((get_property_object_nat(pobj, "lastChild") ==
			     obj) ^ negate)
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
*********************************************************************/

static bool qsaMatchChain(struct htmlTag *t, jsobjtype obj, const struct sel *s)
{
	struct htmlTag *u;
	char combin;
	const struct asel *a = s->chain;
	if (!a)			// should never happen
		return false;
// base node matches the first selector
	if (!qsaMatch(t, obj, a))
		return false;
// now look down the rest of the chain
	while ((a = a->next)) {
		combin = a->combin;

		if (combin == '+') {
			if (t) {
				if (!t->parent)
					return false;
				u = t->parent->firstchild;
				if (!u || u == t)
					return false;
				for (; u; u = u->sibling)
					if (u->sibling == t)
						break;
				if (!u)
					return false;
				t = u;
				if (qsaMatch(t, obj, a))
					continue;
				return false;
			}
// now by objects
			obj = get_property_object_nat(obj, "previousSibling");
			if (!obj)
				return false;
			if (qsaMatch(t, obj, a))
				continue;
			return false;
		}

		if (combin == '>') {
			if (t) {
				t = t->parent;
				if (!t)
					return false;
				if (qsaMatch(t, obj, a))
					continue;
				return false;
			}
			obj = get_property_object_nat(obj, "parentNode");
			if (!obj)
				return false;
			if (qsaMatch(t, obj, a))
				continue;
			return false;
		}
// any ancestor
		if (t) {
			while ((t = t->parent))
				if (qsaMatch(t, obj, a))
					goto next_a;
			return false;
		}
		while ((obj = get_property_object_nat(obj, "parentNode")))
			if (qsaMatch(t, obj, a))
				goto next_a;
		return false;

next_a:	;
	}

	return true;
}

static bool qsaMatchGroup(struct htmlTag *t, jsobjtype obj,
			  const struct desc *d)
{
	struct sel *sel;
	if (d->error)
		return false;
	for (sel = d->selectors; sel; sel = sel->next) {
		if (sel->error)
			continue;
		if (qsaMatchChain(t, obj, sel))
			return true;
	}
	return false;
}

// Match a selector against a list of nodes.
// If list is not given then doclist is used - all nodes in the document.
// the result is an allocated array of nodes from the list that match.
static struct htmlTag **qsa1(const struct sel *sel)
{
	int i, n = 1;
	struct htmlTag *t;
	struct htmlTag **a, **list;

	if ((sel->before && matchtype != 1) ||
	    (sel->after && matchtype != 2) ||
	    (!(sel->before | sel->after) && matchtype)) {
		a = allocMem((n + 1) * sizeof(struct htmlTag *));
		a[0] = 0;
		return a;
	}

	list = bestListAtomic(sel->chain);
	if (!onematch && list) {
// allocate room for all, in case they all match.
		for (n = 0; list[n]; ++n) ;
	}
	a = allocMem((n + 1) * sizeof(struct htmlTag *));
	if (!list) {
		a[0] = 0;
		return a;
	}
	n = 0;
	for (i = 0; (t = list[i]); ++i) {
		if (qsaMatchChain(t, 0, sel)) {
			a[n++] = t;
			if (onematch)
				break;
		}
	}
	a[n] = 0;
	if (!onematch)
		a = reallocMem(a, (n + 1) * sizeof(struct htmlTag *));
	return a;
}

// merge two lists of nodes, giving a third.
static struct htmlTag **qsaMerge(struct htmlTag **list1, struct htmlTag **list2)
{
	int n1, n2, i1, i2, v1, v2, n;
	struct htmlTag **a;
	bool g1 = true, g2 = true;
// make sure there's room for both lists
	for (n1 = 0; list1[n1]; ++n1) ;
	for (n2 = 0; list2[n2]; ++n2) ;
	n = n1 + n2;
	a = allocMem((n + 1) * sizeof(struct htmlTag *));
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

// querySelectorAll on a group, uses merge above
static struct htmlTag **qsa2(struct desc *d)
{
	struct htmlTag **a = 0, **a2, **a_new;
	struct sel *sel;
	for (sel = d->selectors; sel; sel = sel->next) {
		if (sel->error)
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
static void build1_doclist(struct htmlTag *t);
static int doclist_cmp(const void *v1, const void *v2);
static void build_doclist(struct htmlTag *top)
{
	doclist_n = 0;
	doclist_a = 500;
	doclist = allocMem((doclist_a + 1) * sizeof(struct htmlTag *));
	if (top) {
		doclist_f = top->f0;
		build1_doclist(top);
	} else {
		doclist_f = cf;
		if (cf->headtag)
			build1_doclist(cf->headtag);
		if (cf->bodytag)
			build1_doclist(cf->bodytag);
	}
	doclist[doclist_n] = 0;
	qsort(doclist, doclist_n, sizeof(struct htmlTag *), doclist_cmp);
}

// recursive
static void build1_doclist(struct htmlTag *t)
{
// can't descend into another frame
	if (t->f0 != doclist_f)
		return;
	if (doclist_n == doclist_a) {
		doclist_a += 500;
		doclist =
		    reallocMem(doclist,
			       (doclist_a + 1) * sizeof(struct htmlTag *));
	}
	doclist[doclist_n++] = t;
	for (t = t->firstchild; t; t = t->sibling)
		build1_doclist(t);
}

static int doclist_cmp(const void *v1, const void *v2)
{
	const struct htmlTag *const *p1 = v1;
	const struct htmlTag *const *p2 = v2;
	int d = (*p1)->seqno - (*p2)->seqno;
	return d;
}

static struct htmlTag **qsaInternal(const char *selstring, struct htmlTag *top)
{
	struct desc *d0;
	struct htmlTag **a;
// Compile the selector. The string has to be allocated.
	char *s = allocMem(strlen(selstring) + 20);
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
	a = qsa2(d0);
	nzFree(doclist);
	cssPiecesFree(d0);
	return a;
}

// turn an array of html tags into an array of objects
static jsobjtype objectize(struct htmlTag **list)
{
	int i, j;
	const struct htmlTag *t;
	delete_property_nat(cf->winobj, "qsagc");
	jsobjtype ao = instantiate_array_nat(cf->winobj, "qsagc");
	if (!ao || !list)
		return ao;
	for (i = j = 0; (t = list[i]); ++i) {
		if (!t->jv)	// should never happen
			continue;
		set_array_element_object_nat(ao, j++, t->jv);
	}
	return ao;
}

jsobjtype querySelectorAll(const char *selstring, jsobjtype topobj)
{
	struct htmlTag *top = 0, **a;
	jsobjtype ao;
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
	struct htmlTag *top = 0, **a;
	jsobjtype node = 0;
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

// foo-bar has to become fooBar
void cssAttributeCrunch(char *s)
{
	char *t, *w;
	for (t = w = s; *t; ++t)
		if (*t == '-' && isalpha(t[1]))
			t[1] = toupper(t[1]);
		else
			*w++ = *t;
	*w = 0;
}

static void do_rules(jsobjtype obj, struct rule *r, bool force)
{
	char *s;
	int sl;
	jsobjtype textobj;
	static const char jl[] = "text 0x0, 0x0";

	if (!obj)
		return;

	if (!matchtype) {
		for (; r; r = r->next) {
// if it appears to be part of the prototype, and not the object,
// I won't write it, even if force is true.
			bool has = has_property_nat(obj, r->atname);
			enum ej_proptype what =
			    typeof_property_nat(obj, r->atname);
			if (has && !what)
				continue;
			if (what && !force)
				continue;
			++bulktotal;
			set_property_string_nat(obj, r->atname, r->atval);
		}
		return;
	}
// this is before and after, injecting text
	s = initString(&sl);
	for (; r; r = r->next) {
		if (!stringEqual(r->atname, "content"))
			continue;
		if (stringEqual(r->atval, "none"))
			continue;
		if (sl)
			stringAndChar(&s, &sl, ' ');
		stringAndString(&s, &sl, r->atval);
	}
	if (!sl)		// should never happen
		return;
// put a space between the injected text and the original text
	stringAndChar(&s, &sl, ' ');
	if (matchtype == 2) {
// oops, space belongs on the other side
		memmove(s + 1, s, sl - 1);
		s[0] = ' ';
	}

	textobj = instantiate_nat(cf->winobj, "eb$inject", "TextNode");
	if (!textobj)		// should never happen
		return;
	set_property_string_nat(textobj, "data", s);
	nzFree(s);
	javaSetsLinkage(false, 'c', textobj, jl);
	if (matchtype == 1)
		run_function_onearg_nat(obj, "prependChild", textobj);
	else
		run_function_onearg_nat(obj, "appendChild", textobj);
// It is now linked in and safe from gc
	delete_property_nat(cf->winobj, "eb$inject");
}

void cssApply(jsobjtype node, jsobjtype destination)
{
	struct htmlTag *t = tagFromJavaVar(node);
	struct cssmaster *cm = cf->cssmaster;
	struct desc *d;
	if (!cm)
		return;
	for (d = cm->descriptors; d; d = d->next) {
		if (qsaMatchGroup(t, node, d))
			do_rules(destination, d->rules, false);
	}
}

void cssText(jsobjtype node, const char *rulestring)
{
	struct desc *d0;
// Compile the selector. The string has to be allocated.
	char *s = allocMem(strlen(rulestring) + 20);
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
	do_rules(node, d0->rules, true);
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
		mark->body = allocMem((mark->n + 1) * sizeof(struct htmlTag *));
		mark->body[0] = mark->t;
		mark->t = 0;
		j = 1;
	}

	if (mark) {
		mark->body[j] = 0;
		++mark;
		if (mark - h != distinct)
			printf("css hash mismatch %d versus %d\n", mark - h,
			       distinct);
		distinct = mark - h;
	}
// make sure there's something, even if distinct = 0
	h = reallocMem(h, (distinct + 1) * sizeof(struct hashhead));
	*hp = h, *np = distinct;
}

static void hashBuild(void)
{
	struct htmlTag *t;
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
		if (!(t->class && t->class[0]))
			continue;
		if (j == a) {
			a += 500;
			h = reallocMem(h, a * sizeof(struct hashhead));
		}
		classcopy = cloneString(t->class);
		h[j].key = classcopy;
		h[j].t = t;
		++j;

		if (!strpbrk(t->class, ws))
			continue;	// no spaces

		classcopy = cloneString(t->class);
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
static struct htmlTag **bestListAtomic(struct asel *a)
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
	struct htmlTag **a, **u;
	jsobjtype style;
	struct htmlTag *t;

	bulkmatch = true;
	bulktotal = 0;
	for (matchtype = 0; matchtype <= 2; ++matchtype) {
		for (d = d0; d; d = d->next) {
			if (d->error)
				continue;
			a = qsa2(d);
			if (!a)
				continue;
			for (u = a; (t = *u); ++u) {
				if (!t->jv)
					continue;
				if (matchtype)
					style = t->jv;
				else {
					style =
					    get_property_object_nat(t->jv,
								    "style");
					if (!style)
						continue;
				}
				do_rules(style, d->rules, false);
			}
			nzFree(a);
		}
	}
	bulkmatch = false;
	matchtype = 0;
}
