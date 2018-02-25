/*********************************************************************
Parse css files into css descriptors, and apply those descriptors to nodes.
All this was written in js but was too slow.
Some sites have thousands of descriptors and hundreds of nodes,
e.g. www.stackoverflow.com with 5,050 descriptors.
*********************************************************************/

#include "eb.h"

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
#define CSS_ERROR_CLASS 11
#define CSS_ERROR_ID 12
#define CSS_ERROR_DYN 13
#define CSS_ERROR_BEFORE 14
#define CSS_ERROR_AFTER 15
#define CSS_ERROR_UNSUP 16
#define CSS_ERROR_MULTIPLE 17
#define CSS_ERROR_DELIM1 18
#define CSS_ERROR_DELIM2 19
#define CSS_ERROR_LAST 20

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
	"bad class",
	"bad id",
	"dynamic",
	"before",
	"after",
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
	bool gentext;		// generates text
	struct sel *selectors;
	struct rule *rules;
};

// selector
struct sel {
	struct sel *next;
	uchar error;
	bool gentext;
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
	bool before, after;
};

struct rule {
	struct rule *next;
	char *atname, *atval;
};

struct cssmaster {
	struct desc *descriptors;
};

static void cssPiecesFree(struct desc *d);
static void cssPiecesPrint(const struct desc *d);
static void cssAtomic(struct asel *a);
static void cssModify(struct asel *a, const char *m1, const char *m2);

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
				unstring(iu2);
				debugPrint(3, "css source %s", iu2);
				*iu1 = 0;
				a = NULL;
				if (httpConnect
				    (iu2, false, false, true, 0, 0, 0)) {
					if (ht_code == 200) {
						a = force_utf8(serverData,
							       serverDataLen);
						if (!a)
							a = serverData;
						else
							nzFree(serverData);
					} else {
						nzFree(serverData);
						if (debugLevel >= 3)
							i_printf(MSG_GetCSS,
								 iu2, ht_code);
					}
					serverData = NULL;
					serverDataLen = 0;
				} else {
					if (debugLevel >= 3)
						i_printf(MSG_GetCSS2, errorMsg);
				}
				if (!a)
					a = emptyString;
				t = allocMem(strlen(s) + strlen(a) +
					     strlen(iu3) + 24 + 1);
				strcpy(t, s);
				strcat(t, "@ebdelim1{}\n");
				strcat(t, a);
				strcat(t, "@ebdelim2{}\n");
				strcat(t, iu3);
				nzFree(a);
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
		if (stringEqual(s, "@ebdelim1")) {
			d->error = CSS_ERROR_DELIM1;
			continue;
		}
		if (stringEqual(s, "@ebdelim2")) {
			d->error = CSS_ERROR_DELIM2;
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
// before and after should only be on the base node of the chain
			if (!sel->chain->next) {
				if (asel->before)
					sel->before = true;
				if (asel->after)
					sel->after = true;
				if (sel->before | sel->after)
					d->gentext = sel->gentext = true;
			}

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
		if (!sel->chain->next) {
			if (asel->before)
				sel->before = true;
			if (asel->after)
				sel->after = true;
			if (sel->before | sel->after)
				d->gentext = sel->gentext = true;
		}
	}

// if all the selectors under d are in error, then d is error
	for (d = d1; d; d = d->next) {
		bool across = true;
		uchar ec = CSS_ERROR_NONE;
		if (d->error) {
			if (d->error < CSS_ERROR_DELIM1) {
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
			++errorBuckets[d->error];
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
				++errorBuckets[d->error];
				break;
			}
			if (d->error)
				break;
			if (!*t) {
				d->error = CSS_ERROR_COLON;
				++errorBuckets[d->error];
				break;
			}
			if (t == r1) {
				d->error = CSS_ERROR_RATTR;
				++errorBuckets[d->error];
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
			++errorBuckets[d->error];
		}
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
			a->error = CSS_ERROR_BEFORE;
			return;
		}
		if (stringEqual(t, ":after")) {
			a->after = true;
			a->error = CSS_ERROR_AFTER;
			return;
		}
		if (!stringEqual(t, ":link") && !stringEqual(t, ":first-child")
		    && !stringEqual(t, ":last-child")) {
			a->error = CSS_ERROR_UNSUP;
			return;
		}
		break;

	case '#':
	case '.':
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
			if ((c == '|' || c == '~') && w[1] == '=') {
				++w;
				break;
			}
			if (isupper(c))
				*w = tolower(c);
			if ((isdigit(c) && w > t + 1) || isalpha(c) || c == '-')
				continue;
			a->error = CSS_ERROR_ATTR;
			if (h == '.')
				a->error = CSS_ERROR_CLASS;
			if (h == '#')
				a->error = CSS_ERROR_ID;
			return;
		}
		if (*w)
			unstring(w);
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
	f = fopen("/tmp/css", "a");
	if (!f)
		return;
	fprintf(f, "%s start\n", errorMessage[CSS_ERROR_DELIM1]);
	if (!d) {
		fclose(f);
		return;
	}

	for (; d; d = d->next) {
		if (d->error) {
			if (d->error == CSS_ERROR_DELIM1)
				fprintf(f, "%s open\n",
					errorMessage[CSS_ERROR_DELIM1]);
			else if (d->error == CSS_ERROR_DELIM2)
				fprintf(f, "%s close\n",
					errorMessage[CSS_ERROR_DELIM2]);
			else
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
				     mod = mod->next)
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
static bool qsaMatch(struct htmlTag *t, jsobjtype obj, const struct asel *a)
{
	bool rc;
	struct mod *mod;
	jsobjtype pobj;		// parent object

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
		char c = p[0];
		bool isclass = !strncmp(p, "[class~=", 8);
		bool isid = !strncmp(p, "[id=", 4);

// I'll assume that class and id, once set, are unchanging, like nodeName.
		if (isclass && t) {
			char *v = t->class;
			char *u = p + 8;
			int l = strlen(u);
			char *q;
			if ((!v || !*v) && t->jv)	// dip into js
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
				goto next_mod;
			}
			return false;
		}

		if (isid && t) {
			char *v = t->id;
			if ((!v || !*v) && t->jv)	// dip into js
				t->id = v =
				    get_property_string_nat(t->jv, "id");
			if (!v)
				v = emptyString;
			if (stringEqual(v, p + 4))
				goto next_mod;
			return false;
		}

		if (t)
			obj = t->jv;

		if (c == '[') {
			int l;
			char cutc = 0;
			char *value, *v, *v0, *q;
			char *cut = strchr(p, '=');
			if (cut) {
				value = cut + 1;
				l = strlen(value);
				if (cut[-1] == '|' || cut[-1] == '~')
					--cut;
				cutc = *cut;
				*cut = 0;	// I'll put it back
			}
			v = 0;
			if (obj)
				v = get_property_string_nat(obj, p + 1);
			if (cut)
				*cut = cutc;
			if (!v || !*v)
				return false;
			if (!cutc) {
				nzFree(v);
				goto next_mod;
			}
			if (cutc == '=') {	// easy case
				rc = stringEqual(v, value);
				nzFree(v);
				if (rc)
					goto next_mod;
				return false;
			}
			if (cutc == '|') {
				rc = (!strncmp(v, value, l)
				      && (v[l] == 0 || v[l] == '-'));
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
				nzFree(v0);
				goto next_mod;
			}
			nzFree(v0);
			return false;
		}
// At this point c should be a colon.
		if (c != ':')
			return false;

		if (stringEqual(p, ":link"))
			continue;

		if (stringEqual(p, ":first-child")) {
// t is more efficient, but this doesn't work for body and head,
// which have no parent in our representation.
			if (t && t->parent) {
				if (t == t->parent->firstchild)
					goto next_mod;
				return false;
			}
// now by object
			pobj = get_property_object_nat(obj, "parentNode");
			if (!pobj)
				return false;
			if (get_property_object_nat(pobj, "firstChild") == obj)
				goto next_mod;
			return false;
		}

		if (stringEqual(p, ":last-child")) {
			if (t && t->parent) {
				if (!t->sibling)
					goto next_mod;
				return false;
			}
			pobj = get_property_object_nat(obj, "parentNode");
			if (!pobj)
				return false;
			if (get_property_object_nat(pobj, "lastChild") == obj)
				goto next_mod;
			return false;
		}

		return false;	// unrecognized

next_mod:	;
	}

	return true;		// all modifiers pass
}

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

static bool matchfirst;
static struct htmlTag **doclist;
static int doclist_a, doclist_n;
static struct ebFrame *doclist_f;

// Match a selector against a list of nodes.
// If list is not given then doclist is used - all nodes in the document.
// the result is an allocated array of nodes from the list that match.
static struct htmlTag **qsa1(struct htmlTag **list, const struct sel *sel)
{
	int i, n;
	struct htmlTag *t;
	struct htmlTag **a;
	if (!list)
		list = doclist;
	if (matchfirst)
		n = 1;
	else {
// allocate room for all, in case they all match.
		for (n = 0; list[n]; ++n) ;
	}
	a = allocMem((n + 1) * sizeof(struct htmlTag *));
	n = 0;
	for (i = 0; (t = list[i]); ++i) {
		if (qsaMatchChain(t, 0, sel)) {
			a[n++] = t;
			if (matchfirst)
				break;
		}
	}
	a[n] = 0;
	if (!matchfirst)
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
		a2 = qsa1(0, sel);
		if (a) {
			a_new = qsaMerge(a, a2);
			nzFree(a);
			nzFree(a2);
			a = a_new;
			if (matchfirst && a[0] && a[1])
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
	doclist_a = 200;
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
		doclist_a += 200;
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
	matchfirst = true;
	a = qsaInternal(selstring, top);
	matchfirst = false;
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
	if (!obj)
		return;
	for (; r; r = r->next) {
// if it appears to be part of the prototype, and not the object,
// I won't write it, even if force is true.
		bool has = has_property_nat(obj, r->atname);
		enum ej_proptype what = typeof_property_nat(obj, r->atname);
		if (has && !what)
			continue;
		if (what && !force)
			continue;
		set_property_string_nat(obj, r->atname, r->atval);
	}
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
