/*********************************************************************
Parse css files into css descriptors, and apply those descriptors to nodes.
All this was written in js but was too slow.
Some sites have thousands of descriptors and hundreds of nodes,
e.g. www.stackoverflow.com with 5,050 descriptors.
*********************************************************************/

#include "eb.h"

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
	bool nyi;		// not yet implemented
	bool gentext;		// generates text
	char *explain;
	struct sel *selectors;
	struct rule *rules;
};

// selector
struct sel {
	struct sel *next;
	bool nyi, gentext;
	bool before, after;
	char *explain;
	struct asel *chain;
};

// atomic selector
struct asel {
	struct asel *next;
	char *tag;
	char *part;
	char *explain;
	bool before, after;
	bool nyi;
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
void cssPieces(char *s)
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
	struct cssmaster *cm;

	cm = cf->cssmaster;
	if (!cm)
		cf->cssmaster = cm = allocZeroMem(sizeof(struct cssmaster));
	if (cm->descriptors) {
		cssPiecesFree(cm->descriptors);
		cm->descriptors = 0;
	}

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
					     strlen(iu3) + 1);
				strcpy(t, s);
				strcat(t, a);
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
		return;
	}
// now the base string is at d1->lhs;

	cm->descriptors = d1;

// Now let's try to understand the selectors.
	for (d = d1; d; d = d->next) {
		char combin;
		char *a1, *a2;	// bracket the atomic selector

		if (d->nyi)
			continue;
		s = d->lhs;
		if (!s[0]) {
			d->nyi = true, d->explain = "no selectors";
			continue;
		}
// leading @ doesn't apply to edbrowse.
		if (s[0] == '@') {
			d->nyi = true, d->explain = "@";
			continue;
		}
		if (d->bc > 1) {
			d->nyi = true, d->explain = "nested braces";
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
				sel->nyi = true, sel->explain =
				    "empty selector";
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
			if (asel->nyi && !sel->nyi)
				sel->nyi = true, sel->explain = asel->explain;
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
					d->nyi = true, d->explain =
					    "no selectors";
				continue;
			}
			sel->nyi = true, sel->explain = "empty selector";
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
		if (asel->nyi && !sel->nyi)
			sel->nyi = true, sel->explain = asel->explain;
		if (!sel->chain->next) {
			if (asel->before)
				sel->before = true;
			if (asel->after)
				sel->after = true;
			if (sel->before | sel->after)
				d->gentext = sel->gentext = true;
		}
	}

// if all the selectors under d are nyi, then d is nyi
	for (d = d1; d; d = d->next) {
		bool across = true;
		char *explain = 0;
		if (d->nyi)
			continue;
		if (!d->selectors)	// should never happen
			continue;
		for (sel = d->selectors; sel; sel = sel->next) {
			if (!sel->nyi) {
				across = false;
				break;
			}
			if (!explain)
				explain = sel->explain;
			else if (explain != sel->explain)
				explain = "multiple";
		}
		if (across)
			d->nyi = true, d->explain = explain;
	}

// now for the rules
	for (d = d1; d; d = d->next) {
		char *r1, *r2;	// rule delimiters
		struct rule *rule, *rule2;
		if (d->nyi)
			continue;
		s = d->rhs;
		if (!*s) {
			d->nyi = true, d->explain = "no rules";
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
				d->nyi = true, d->explain =
				    "bad rule attribute";
				break;
			}
			if (d->nyi)
				break;
			if (!*t) {
				d->nyi = true, d->explain = "rule no :";
				break;
			}
			if (t == r1) {
				d->nyi = true, d->explain =
				    "rule empty attribute";
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
		if (r1 < s && !d->nyi && *r1 != '*' && *r1 != '_') {
// There should have been a final ; but oops.
// process the last rule as above.
			r2 = s;
			goto lastrule;
		}
		if (!d->rules)
			d->nyi = true, d->explain = "no rules";
	}

/* to debug:
*/
	cssPiecesPrint(d1);
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
			a->nyi = true, a->explain = "bad tag";
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
			a->nyi = true, a->explain = "dynamic";
			return;
		}
		if (stringEqual(t, ":before")) {
			a->before = a->nyi = true;
			a->explain = "before";
			return;
		}
		if (stringEqual(t, ":after")) {
			a->after = a->nyi = true;
			a->explain = "after";
			return;
		}
		if (!stringEqual(t, ":link") && !stringEqual(t, ":first-child")
		    && !stringEqual(t, ":last-child")) {
			a->nyi = true, a->explain = ": unsupported";
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
			a->nyi = true, a->explain = "[ no ]";
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
			propname = "bad attribute";
			if (h == '.')
				propname = "bad class";
			if (h == '#')
				propname = "bad id";
			a->nyi = true, a->explain = propname;
			return;
		}
		if (*w)
			unstring(w);
	}			// switch
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
	FILE *f = fopen("/tmp/css", "w");
	const struct sel *sel;
	const struct asel *asel;
	const struct mod *mod;
	const struct rule *r;
	for (; d; d = d->next) {
		if (d->nyi) {
			fprintf(f, "<%s>%s\n", d->explain, d->lhs);
			continue;
		}
		for (sel = d->selectors; sel; sel = sel->next) {
			if (sel != d->selectors)
				fprintf(f, ",");
			if (sel->nyi)
				fprintf(f, "<%s|", sel->explain);
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
