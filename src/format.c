/* format.c
 * Format text, establish line breaks, manage whitespace.
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"

/*********************************************************************
Prepare html for text processing.
Change nulls to spaces.
Make sure it doesn't already contain my magic code,
The one I use to indicate a tag.
If it does, well, change them to something else.
I can only hope this doesn't screw up some embedded javascript.
*********************************************************************/

void prepareForBrowse(char *h, int h_len)
{
	int i, j;

	for (i = j = 0; i < h_len; ++i) {
		if (h[i] == 0)
			h[i] = ' ';
		if (h[i] == '\b') {
			if (i && !strchr("\n\b<>'\"&", h[i - 1]))
				--j;
			continue;
		}
		if (h[i] == InternalCodeChar)
			h[i] = InternalCodeCharAlternate;
		h[j++] = h[i];
	}
	h[j] = 0;		/* now it's a string */

/* undos the file */
	for (i = j = 0; h[i]; ++i) {
		if (h[i] == '\r' && h[i + 1] == '\n')
			continue;
		h[j++] = h[i];
	}
	h[j] = 0;
}				/* prepareForBrowse */

/* An input field cannot contain newline, null, or the InternalCodeChar */
void prepareForField(char *h)
{
	while (*h) {
		if (*h == 0 || *h == '\n')
			*h = ' ';
		if (*h == InternalCodeChar)
			*h = InternalCodeCharAlternate;
		++h;
	}
}				/* prepareForField */

/*********************************************************************
Skip past an html comment.
Parse an html tag <tag foo=bar>
*********************************************************************/

const char *skipHtmlComment(const char *h, int *lines)
{
	int lns = 0;
	bool comm = h[2] == '-' && h[3] == '-';
	bool php = memEqualCI(h + 1, "?php", 4);

	h += comm ? 4 : 2;
	while (*h) {
		if (php) {	/* special type of comment */
			if (*h == '?' && h[1] == '>') {
				h += 2;
				goto done;
			}
			++h;
			continue;
		}

		if (!comm && *h == '>') {
			++h;
			goto done;
		}

		if (comm && h[0] == '-' && h[1] == '-') {
			h += 2;
			while (*h == '-')
				h++;
			while (isspaceByte(*h)) {
				if (*h == '\n')
					++lns;
				h++;
			}
			if (!*h)
				goto done;
			if (*h == '>') {
				++h;
				goto done;
			}
			continue;
		}

		if (*h == '\n')
			++lns;
		h++;
	}

done:
	if (lines)
		*lines = lns;
	return h;
}				/* skipHtmlComment */

/* an attribute character */
static bool atchr(char c)
{
	return (c > ' ' && c <= 0x7f && c != '=' && c != '<' && c != '>');
}				/* atchr */

/*********************************************************************
Parse an html tag.
e is pointer to the begining of the element (*e must be '<').
eof is pointer to the end of the html page.
Result parameters:
parsed tag name is stored in name, it's length is namelen.
first attribute is stored in attr.
end points to first character past the html tag.
lines records the number of newlines consumed by the tag.
*********************************************************************/

bool htmlAttrVal_nl;		/* allow nl in attribute values */

bool
parseTag(char *e,
	 const char **name, int *namelen, const char **attr, const char **end,
	 int *lines)
{
	int lns = 0;
	if (*e++ != '<')
		return false;
	if (name)
		*name = e;
	if (*e == '/')
		e++;
	if (!isA(*e))
		return false;
	while (isA(*e) || *e == '=')
		++e;
	if (!isspaceByte(*e) && *e != '>' && *e != '<' && *e != '/'
	    && *e != ':')
		return false;
/* Note that name includes the leading / */
	if (name && namelen)
		*namelen = e - *name;
/* skip past space colon slash */
	while (isspaceByte(*e) || *e == '/' || *e == ':') {
		if (*e == '\n')
			++lns;
		++e;
	}
/* should be the start of the first attribute, or < or > */
	if (!atchr(*e) && *e != '>' && *e != '<')
		return false;
	if (attr)
		*attr = e;
nextattr:
	if (*e == '>' || *e == '<')
		goto en;
	if (!atchr(*e))
		return false;
	while (atchr(*e))
		++e;
	while (isspaceByte(*e)) {
		if (*e == '\n')
			++lns;
		++e;
	}
	if (*e != '=')
		goto nextattr;
	++e;
	while (isspaceByte(*e)) {
		if (*e == '\n')
			++lns;
		++e;
	}
	if (isquote(*e)) {
		unsigned char uu = *e;
x3:
		++e;
		while (*e != uu && *e) {
			if (*e == '\n')
				++lns;
			++e;
		}
		if (*e != uu)
			return false;
		++e;
		if (*e == uu) {
/* lots of tags end with an extra quote */
			if (e[1] == '>')
				*e = ' ';
			else
				goto x3;
		}
	} else {
		while (!isspaceByte(*e) && *e != '>' && *e != '<' && *e)
			++e;
	}
	while (isspaceByte(*e)) {
		if (*e == '\n')
			++lns;
		++e;
	}
	goto nextattr;
en:
/* could be < or > */
	if (end)
		*end = e + (*e == '>');
	if (lines)
		*lines = lns;
	return true;
}				/* parseTag */

/* Don't know why he didn't use the stringAndChar() functions, but he
 * invented something new here, so on we go. */
static void valChar(char **sp, int *lp, char c)
{
	char *s = *sp;
	int l = *lp;
	if (!(l % ALLOC_GR))
		*sp = s = reallocMem(s, l + ALLOC_GR);
	s[l++] = c;
	*lp = l;
}				/* valChar */

/*********************************************************************
Find an attribute in an html tag.
e is attr pointer previously gotten from parseTag, DON'T PASS HERE ANY OTHER VALUE!!!
name is the sought attribute.
returns allocated string containing the attribute, or NULL on unsuccess.
*********************************************************************/

char *htmlAttrVal(const char *e, const char *name)
{
	const char *n;
	char *a = EMPTYSTRING;	/* holds the value */
	char *b;
	int l = 0;		/* length */
	char f;

	if (!e) {
out:
		nzFree(a);
		return NULL;
	}

top:
	while (isspaceByte(*e))
		e++;
	if (!*e)
		goto out;
	if (*e == '>' || *e == '<')
		goto out;

/* case insensitive match on name */
	n = name;
	while (*n && !((*e ^ *n) & 0xdf))
		e++, n++;
	f = *n;
	while (atchr(*e))
		f = 'x', e++;
	while (isspaceByte(*e))
		e++;
	if (*e != '=')
		goto ea;
	e++;
	while (isspaceByte(*e))
		e++;
	if (!isquote(*e)) {
/* no quotes, the attribute value is just the word */
		while (*e && !isspaceByte(*e) && *e != '>' && *e != '<') {
			if (!f)
				valChar(&a, &l, *e);
			e++;
		}
	} else {
		char uu = *e;	/* holds " or ' */
a:
		e++;
		while (*e != uu) {
			if (!*e)
				goto out;
			if (!f && *e != '\r') {
				if (*e != '\t' && *e != '\n')
					valChar(&a, &l, *e);
				else if (!htmlAttrVal_nl)
					valChar(&a, &l, ' ');
			}
			e++;
		}
		e++;
		if (*e == uu) {
			if (!f)
				valChar(&a, &l, uu);
			goto a;
		}
	}
ea:
	if (f)
		goto top;	/* no match, next attribute */
	if (l)
		valChar(&a, &l, 0);	/* null terminate */
	if (strchr(a, '&')) {
		b = a;
		a = andTranslate(b, true);
		nzFree(b);
	}
/* strip leading and trailing spaces.
 * Are we really suppose to do this? */
	for (b = a; *b == ' '; b++) ;
	if (b > a)
		strmove(a, b);
	for (b = a + strlen(a) - 1; b >= a && *b == ' '; b--)
		*b = 0;
	return a;
}				/* htmlAttrVal */

/*********************************************************************
Jump straight to the </script>, and don't look at anything in between.
Result parameters:
end of the script, the extracted script, and the number of newlines.
*********************************************************************/

bool
findEndScript(const char *h, const char *tagname,
	      bool is_js, char **end_p, char **new_p, int *lines)
{
	char *end;
	bool rc = true;
	const char *s = h;
	char look[12];
	int looklen;
	int js_nl = 0;

	sprintf(look, "</%s", tagname);
	looklen = strlen(look);

retry:
	end = strstrCI(s, look);
	if (!end) {
		rc = false;
		browseError(MSG_CloseTag, look);
		end = (char *)h + strlen(h);
	} else if (isA(end[looklen])) {
/* the tag is </scriptfoobar> or some such, skip past it */
		s = end + looklen;
		goto retry;
	} else if (is_js) {
/* Check for document.write("</script>");
 * This isn't legal javascript, but it happens all the time!
 * This is a really stupid check.
 * Scan forward 30 chars, on the same line, looking
 * for a quote, and ) ; or + */
		char c;
		int j;
		s = end + looklen;
		for (j = 0; j < 30; ++j, ++s) {
			c = *s;
			if (!c)
				break;
			if (c == '\n')
				break;
			if (c != '"' && c != '\'')
				continue;
			while (s[1] == ' ' || s[1] == '\t')
				++s;
			c = s[1];
			if (!c)
				break;
			if (strchr(";)+", c))
				goto retry;
		}
	}

	if (end_p)
		*end_p = end;
	if (new_p)
		*new_p = pullString1(h, end);
/* count the newlines */
	while (h < end) {
		if (*h == '\n')
			++js_nl;
		++h;
	}

	*lines = js_nl;
	return rc;
}				/* findEndScript */

/*********************************************************************
The primary goal of this routine is to turn
Hey,{ click here } for more information
into
Hey, {click here}  for more information
But of course we won't do that if the section is preformatted.
Nor can we muck with the whitespace that might be present in an input field <>.
Also swap 32* whitespace, pushing invisible anchors forward.
If a change is made, the procedure is run again,
kinda like bubble sort.
It has the potential to be terribly inefficient,
but that doesn't seem to happen in practice.
Use cnt to count the iterations, just for debugging.
| is considered a whitespace character. Why is that?
Html tables are mostly used for visual layout, but sometimes not.
I use | to separate the cells of a table, but if there's nothing in them,
or at least no text, then I get rid of the pipes.
But every cell is going to have an invisible anchor from <td>, so that js can,
perhaps, set innerHTML inside this cell.
So there's something there, but nothing there.
I push these tags past pipes, so I can clear it all away.
One web page in ten thousand will actually set html inside a cell,
after the fact, and when that happens the text won't be in the right place,
it won't have the pipes around it that it should.
I'm willing to accept that for now.
*********************************************************************/

void anchorSwap(char *buf)
{
	char c, d, *s, *ss, *w, *a;
	bool pretag;		// <pre>
	bool premode;		// inside <pre> </pre>
	bool slash;		// closing tag
	bool change;		// made a swap somewhere
	bool strong;		// strong whitespace, newline or paragraph
	int n, cnt;
	char tag[20];

	static const char from[] =
	    "\x1b\x95\x99\x9c\x9d\x91\x92\x93\x94\xa0\xad\x96\x97\x85";
	static const char becomes[] = "_*'`'`'`' ----";
/* I use to convert a6 and c2 to hyphen space, not sure why */

/* Transliterate a few characters.  One of them is 0xa0 to space,
 * so we need to do this now, before the anchors swap with whitespace.
 * Watch out for utf8 - don't translate the a0 in c3a0.  That is a grave.
 * But a0 by itself is breakspace; turn it into space.
 * And c2a0 is a0 is breakspace.
 * Then get rid of hyperlinks with absolutely nothing to click on. */

	for (s = w = buf; c = *s; ++s) {
		d = s[1];
/* utf8 test */
		if ((c & 0xc0) == 0xc0 && (d & 0xc0) == 0x80) {
			unsigned int uni = 0;
			if ((c & 0x3c) == 0) {
/* fits in 8 bits */
				uni = ((uchar) c << 6) | (d & 0x3f);
				ss = strchr(from, (char)uni);
				if (ss) {
					c = becomes[ss - from];
					++s;
					goto put1;
				}
			}
/* copy the utf8 sequence as is */
			*w++ = c;
			++s;
			c <<= 1;
			while ((c & 0x80) && ((d = *s) & 0xc0) == 0x80) {
				*w++ = d;
				++s;
			}
			--s;
			continue;
		}

/* Now assuming iso8859-1, which is practically depricated */
		ss = strchr(from, c);
		if (ss)
			c = becomes[ss - from];

		if (c != InternalCodeChar)
			goto put1;
		if (!isdigitByte(s[1]))
			goto put1;
		for (a = s + 2; isdigitByte(*a); ++a) ;
		if (*a != '{')
			goto put1;
		for (++a; *a == ' '; ++a) ;
		if (a[0] != InternalCodeChar || a[1] != '0' || a[2] != '}')
			goto put1;
/* skip past empty {} */
		s = a + 2;
		continue;

put1:
		*w++ = c;
	}
	*w = 0;

/* anchor whitespace swap preserves the length of the string */
	cnt = 0;
	change = true;
	while (change) {
		change = false;
		++cnt;
		premode = false;
/* w represents the state of whitespace */
		w = NULL;
/* a points to the prior anchor, which is swappable with following whitespace */
		a = NULL;

		for (s = buf; c = *s; ++s) {
			if (isspaceByte(c) || c == '|') {
				if (c == '\t' && !premode)
					*s = ' ';
				if (!w)
					w = s;
				continue;
			}

/* end of white space, should we swap it with prior tag? */
			if (w && a) {
				memmove(a, w, s - w);
				memmove(a + (s - w), tag, n);
				change = true;
				w = NULL;
			}

/* prior anchor has no significance */
			a = NULL;

			if (c != InternalCodeChar)
				goto normalChar;
/* some conditions that should never happen */
			if (!isdigitByte(s[1]))
				goto normalChar;
			n = strtol(s + 1, &ss, 10);
			preFormatCheck(n, &pretag, &slash);
			d = *ss;
			if (!strchr("{}<>*", d))
				goto normalChar;
			n = ss + 1 - s;
			memcpy(tag, s, n);
			tag[n] = 0;

			if (pretag) {
				w = 0;
				premode = !slash;
				s = ss;
				continue;
			}

/* We have a tag, should we swap it with prior whitespace? */
			if (w && !premode && d == '}') {
				memmove(w + n, w, s - w);
				memcpy(w, tag, n);
				change = true;
				w += n;
				s = ss;
				continue;
			}

			if ((d == '*' || d == '{') && !premode)
				a = s;
			s = ss;

normalChar:
			w = 0;	/* no more whitespace */
/* end of loop over the chars in the buffer */
		}
/* end of loop making changes */
	}
	debugPrint(3, "anchorSwap %d", cnt);

/* Framing characters like [] around an anchor are unnecessary here,
 * because we already frame it in braces.
 * Get rid of these characters, even in premode.
 * Also, remove trailing pipes on a line. */
	ss = 0;			/* remember location of first pipe */
	for (s = w = buf; c = *s; ++s) {
		char open, close, linkchar;
		if (!strchr("{[(<", c))
			goto putc;
		if (s[1] != InternalCodeChar)
			goto putc;
		if (!isdigitByte(s[2]))
			goto putc;
		for (a = s + 3; isdigitByte(*a); ++a) ;
		linkchar = 0;
		if (*a == '{')
			linkchar = '}';
		if (*a == '<')
			linkchar = '>';
		if (!linkchar)
			goto putc;
		open = c;
		close = 0;
		if (open == '{')
			close = '}';
		if (open == '[')
			close = ']';
		if (open == '(')
			close = ')';
		if (open == '<')
			close = '>';
		n = 1;
		while (n < 120) {
			d = a[n++];
			if (!d)
				break;
			if (d != InternalCodeChar)
				continue;
			while (isdigitByte(a[n]))
				++n;
			d = a[n++];
			if (!d)
				break;	/* should never happen */
			if (strchr("{}<>", d))
				break;
		}
		if (n >= 120)
			goto putc;
		if (d != linkchar)
			goto putc;
		a += n;
		if (*a != close)
			goto putc;
		++s;
		memmove(w, s, a - s);
		w += a - s;
		s = a;
		ss = 0;
		continue;
putc:
		if (c == '|' && !ss)
			ss = w;
		if (strchr("\r\n\f", c) && ss)
			w = ss, ss = 0;
		if (!isspaceByte(c) && c != '|')
			ss = 0;
		*w++ = c;
	}			/* loop over buffer */
	*w = 0;
	debugPrint(3, "anchors unframed");

/* Now compress the implied linebreaks into one. */
	premode = false;
	for (s = buf; c = *s; ++s) {
		if (c == InternalCodeChar && isdigitByte(s[1])) {
			n = strtol(s + 1, &s, 10);
			if (*s == '*') {
				preFormatCheck(n, &pretag, &slash);
				if (pretag)
					premode = !slash;
			}
		}
		if (!isspaceByte(c))
			continue;
		strong = false;
		a = 0;
		for (w = s; isspaceByte(*w); ++w) {
			if (*w == '\n' || *w == '\f')
				strong = true;
			if (*w == '\r' && !a)
				a = w;
		}
		ss = s, s = w - 1;
		if (!a)
			continue;
		if (premode)
			continue;
		if (strong) {
			for (w = ss; w <= s; ++w)
				if (*w == '\r')
					*w = ' ';
			continue;
		}
		for (w = ss; w <= s; ++w)
			if (*w == '\r' && w != a)
				*w = ' ';
	}			/* loop over buffer */
	debugPrint(3, "whitespace combined");
}				/* anchorSwap */

/*********************************************************************
Format text, and break lines at sentence/phrase boundaries.
The prefix bl means breakline.
*********************************************************************/

static char *bl_start, *bl_cursor, *bl_end;
static bool bl_overflow;
/* This is a virtual column number, extra spaces for tab,
 * and skipping over invisible anchors. */
static int colno;
static const int optimalLine = 80;	/* optimal line length */
static const int cutLineAfter = 36;	/* cut sentence after this column */
static const int paraLine = 120;	/* paragraph in a line */
static int longcut, pre_cr;
static int lspace;		/* last space value, 3 = paragraph */
/* Location of period comma rightparen or any word.
 * Question mark is equivalent to period etc.
 * Other things being equal, we break at period, rather than comma, etc.
 * First the column numbers, then the index into the string. */
static int lperiod, lcomma, lright, lany;
static int idxperiod, idxcomma, idxright, idxany;

static void debugChunk(const char *chunk, int len)
{
	int i;
	if (debugLevel < 7)
		return;
	printf("chunk<");
	for (i = 0; i < len; ++i) {
		char c = chunk[i];
		if (c == '\t') {
			printf("\\t");
			continue;
		}
		if (c == '\n') {
			printf("\\n");
			continue;
		}
		if (c == '\f') {
			printf("\\f");
			continue;
		}
		if (c == '\r') {
			printf("\\r");
			continue;
		}
		if (c == '\0') {
			printf("\\0");
			continue;
		}
		printf("%c", c);
	}
	printf(">%d.%d\n", colno, lspace);
}				/* debugChunk */

static void appendOneChar(char c)
{
	if (bl_cursor == bl_end)
		bl_overflow = true;
	else
		*bl_cursor++ = c;
}				/* appendOneChar */

static bool spaceNotInInput(void)
{
	char *t = bl_cursor;
	char c;
	for (--t; t >= bl_start; --t) {
		c = *t;
		if (c == '\n' || c == '\r')
			return true;
		if (c == '>' && t >= bl_start + 2 &&
		    t[-1] == '0' && t[-2] == InternalCodeChar)
			return true;
		if (c != '<')
			continue;
		while (t > bl_start && isdigitByte(t[-1]))
			--t;
		if (*t == '<')
			continue;
		if (t > bl_start && t[-1] == InternalCodeChar)
			return false;
	}
	return true;
}				/* spaceNotInInput */

static void appendSpaceChunk(const char *chunk, int len, bool premode)
{
	int nlc = pre_cr;	/* newline count */
	int spc = 0;		/* space count */
	int i, j;
	char c, d, e;

	if (!len)
		return;
	for (i = 0; i < len; ++i) {
		c = chunk[i];
		if (c == '\n' || c == '\r') {
			++nlc, spc = 0;
			continue;
		}
		if (c == '\f') {
			nlc += 2, spc = 0;
			continue;
		}
		++spc;
	}

	if (!premode && spaceNotInInput()) {
		int l = bl_cursor - bl_start;
		c = d = ' ';
		if (l)
			d = bl_cursor[-1];
		if (l > 1)
			c = bl_cursor[-2];
		e = d;
		if (strchr(")\"|}", d))
			e = c;
		if (strchr(".?!:", e)) {
			bool ok = true;
/* Check for Mr. Mrs. and others. */
			if (e == '.' && bl_cursor - bl_start > 10) {
				static const char *const prefix[] =
				    { "mr.", "mrs.", "sis.", "ms.", 0 };
				char trailing[12];
				for (i = 0; i < 6; ++i) {
					c = bl_cursor[i - 6];
					if (isupperByte(c))
						c = tolower(c);
					trailing[i] = c;
				}
				trailing[i] = 0;
				for (i = 0; prefix[i]; ++i)
					if (strstr(trailing, prefix[i]))
						ok = false;
/* Check for John C. Calhoon */
				if (isupperByte(bl_cursor[-2])
				    && isspaceByte(bl_cursor[-3]))
					ok = false;
			}
			if (ok)
				lperiod = colno, idxperiod = l;
		}
		e = d;
		if (strchr(")\"|", d))
			e = c;
		if (strchr("-,;", e))
			lcomma = colno, idxcomma = l;
		if (strchr(")\"|", d))
			lright = colno, idxright = l;
		lany = colno, idxany = l;
/* tack a short fragment onto the previous line. */
		if (longcut && colno <= 15 && (nlc || lperiod == colno)) {
			bl_start[longcut] = ' ';
			if (!nlc)
				len = spc = 0, nlc = 1;
		}		/* pasting small fragment onto previous line */
	}			/* allowing line breaks */
	if (lspace == 3)
		nlc = 0;
	if (nlc) {
		if (lspace == 2)
			nlc = 1;
		appendOneChar('\n');
		if (nlc > 1)
			appendOneChar('\n');
		colno = 1;
		longcut = lperiod = lcomma = lright = lany = 0;
		if (lspace >= 2 || nlc > 1)
			lspace = 3;
		if (lspace < 2)
			lspace = 2;
		if (!premode)
			return;
	}
	if (!spc)
		return;
	if (!premode) {
/* if the first char of the text to be reformatted is space,
 * then we will wind up here, with lspace = 3. */
		if (lspace == 3)
			return;
		appendOneChar(' ');
		++colno;
		lspace = 1;
		return;
	}
	j = -1;
	for (i = 0; i < len; ++i) {
		c = chunk[i];
		if (c == '\n' || c == '\r' || c == '\f')
			j = i;
	}
	i = j + 1;
	if (i)
		colno = 1;
	for (; i < len; ++i) {
		c = chunk[i];
		if (c == 0)
			c = ' ';
		appendOneChar(c);
		if (c == ' ')
			++colno;
		if (c == '\t')
			colno += 4;
	}
	lspace = 1;
}				/* appendSpaceChunk */

static void appendPrintableChunk(const char *chunk, int len, bool premode)
{
	int i, j;
	bool visible = true;

	for (i = 0; i < len; ++i) {
		char c = chunk[i];
		appendOneChar(c);
		if (c == InternalCodeChar) {
			visible = false;
			continue;
		}
		if (visible) {
			++colno;
			continue;
		}
		if (isdigitByte(c))
			continue;
/* end of the tag */
		visible = true;
		if (c != '*')
			++colno;
	}

	lspace = 0;
	if (premode)
		return;
	if (colno <= optimalLine)
		return;
/* Oops, line is getting long.  Let's see where we can cut it. */
	i = j = 0;
	if (lperiod > cutLineAfter)
		i = lperiod, j = idxperiod;
	else if (lcomma > cutLineAfter)
		i = lcomma, j = idxcomma;
	else if (lright > cutLineAfter)
		i = lright, j = idxright;
	else if (lany > cutLineAfter)
		i = lany, j = idxany;
	if (!j)
		return;		/* nothing we can do about it */
	longcut = 0;
	if (i != lperiod)
		longcut = j;
	bl_start[j] = '\n';
	colno -= i;
	lperiod -= i;
	lcomma -= i;
	lright -= i;
	lany -= i;
}				/* appendPrintableChunk */

/* Break up a line using the above routines.
 * The new lines are put in a fixed array.
 * Return false (fail) if we ran out of room.
 * This function is called from buffers.c, implementing the bl command,
 * and is only in this file because it shares the above routines and variables
 * with the html reformatting, which really has to be here. */

char replaceLine[REPLACELINELEN];

bool breakLine(const char *line, int len, int *newlen)
{
	char c, state, newstate;
	int i, last;

	pre_cr = 0;
	if (len && line[len - 1] == '\r')
		--len;
	if (lspace == 4) {
/* special continuation code from the previous invokation */
		lspace = 2;
		if (line[0])
			++pre_cr;
	}
	if (len > paraLine)
		++pre_cr;
	if (lspace < 2)
		lspace = 2;	/* should never happen */
	if (!len + pre_cr)
		lspace = 3;
	bl_start = bl_cursor = replaceLine;
	bl_end = replaceLine + REPLACELINELEN - 8;
	bl_overflow = false;
	colno = 1;
	longcut = lperiod = lcomma = lright = lany = 0;
	last = 0;
	state = 0;
	if (pre_cr)
		state = 1;

	for (i = 0; i < len; ++i) {
		c = line[i];
		newstate = 2;
		if (!c || strchr(" \t\n\r\f", c))
			newstate = 1;
		if (state == newstate)
			continue;
		if (!state) {
			state = newstate;
			continue;
		}

/* state change here */
		debugChunk(line + last, i - last);
		if (state == 1)
			appendSpaceChunk(line + last, i - last, false);
		else
			appendPrintableChunk(line + last, i - last, false);
		last = i;
		state = newstate;
		pre_cr = 0;
	}

	if (state) {		/* last token */
		debugChunk(line + last, len - last);
		if (state == 1)
			appendSpaceChunk(line + last, len - last, false);
		else
			appendPrintableChunk(line + last, len - last, false);
	}

	if (lspace < 2) {	/* line didn't have a \r at the end */
		appendSpaceChunk("\n", 1, false);
	}
	if (bl_cursor - bl_start > paraLine)
		lspace = 4;
	debugPrint(7, "chunk<EOL>%d.%d", colno, lspace);
	*newlen = bl_cursor - bl_start;
	return !bl_overflow;
}				/* breakLine */

void breakLineSetup(void)
{
	lspace = 3;
}

char *htmlReformat(const char *buf)
{
	const char *h, *nh, *s;
	char c;
	bool premode = false;
	bool pretag, slash;
	char *new;
	int l, tagno;

	longcut = lperiod = lcomma = lright = lany = 0;
	colno = 1;
	pre_cr = 0;
	lspace = 3;
	bl_start = bl_cursor = replaceLine;
	bl_end = replaceLine + REPLACELINELEN - 8;
	bl_overflow = false;
	new = initString(&l);

	for (h = buf; (c = *h); h = nh) {
		if (isspaceByte(c)) {
			for (s = h + 1; isspaceByte(*s); ++s) ;
			nh = s;
			appendSpaceChunk(h, nh - h, premode);
			if (lspace == 3 || (lspace == 2 || premode) &&
			    (bl_cursor - bl_start) >=
			    (bl_end - bl_start) * 2 / 3) {
				if (bl_cursor > bl_start)
					stringAndBytes(&new, &l, bl_start,
						       bl_cursor - bl_start);
				bl_cursor = bl_start;
				lspace = 3;
				longcut = lperiod = lcomma = lright = lany = 0;
				colno = 1;
			}
			continue;
		}

		if (c != InternalCodeChar) {
			for (s = h + 1; *s; ++s)
				if (isspaceByte(*s) || *s == InternalCodeChar)
					break;
			nh = s;
			appendPrintableChunk(h, nh - h, premode);
			continue;
		}

		/* It's a tag */
		tagno = strtol(h + 1, (char **)&nh, 10);
		c = *nh++;
		if (!c || !strchr("{}<>*", c))
			i_printfExit(MSG_BadTagCode, tagno, c);
		appendPrintableChunk(h, nh - h, premode);
		preFormatCheck(tagno, &pretag, &slash);
		if (pretag)
			premode = !slash;

/* Insert newlines between adjacent hyperlinks. */
		if (c != '}' || premode)
			continue;
		for (h = nh; c = *h; ++h)
			if (!strchr(" \t,:-|;", c))
				break;
		if (!c || strchr("\r\n\f", c)) {
			nh = h;
			continue;
		}
		if (c != InternalCodeChar)
			continue;
/* Does this start a new hyperlink? */
		for (s = h + 1; isdigitByte(*s); ++s) ;
		if (*s != '{')
			continue;
		appendSpaceChunk("\n", 1, false);
		nh = h;
	}			/* loop over text */

/* close off the last line */
	if (lspace < 2)
		appendSpaceChunk("\n", 1, true);
	if (bl_cursor > bl_start)
		stringAndBytes(&new, &l, bl_start, bl_cursor - bl_start);
/* Get rid of last space. */
	if (l >= 2 && new[l - 1] == '\n' && new[l - 2] == ' ')
		new[l - 2] = '\n', new[--l] = 0;
/* Don't need empty lines at the end. */
	while (l > 1 && new[l - 1] == '\n' && new[l - 2] == '\n')
		--l;
	new[l] = 0;
/* Don't allow an empty buffer */
	if (!l)
		stringAndChar(&new, &l, '\n');

	return new;
}				/* htmlReformat */

/*********************************************************************
Convert a 31 bit unicode character into utf8.
*********************************************************************/

static void uni2utf8(unsigned int unichar, unsigned char *outbuf)
{
	int n = 0;

	if (unichar <= 0x7f) {
		outbuf[n++] = unichar;
	} else if (unichar <= 0x7ff) {
		outbuf[n++] = 0xc0 | ((unichar >> 6) & 0x1f);
		outbuf[n++] = 0x80 | (unichar & 0x3f);
	} else if (unichar <= 0xffff) {
		outbuf[n++] = 0xe0 | ((unichar >> 12) & 0xf);
		outbuf[n++] = 0x80 | ((unichar >> 6) & 0x3f);
		outbuf[n++] = 0x80 | (unichar & 0x3f);
	} else if (unichar <= 0x1fffff) {
		outbuf[n++] = 0xf0 | ((unichar >> 18) & 7);
		outbuf[n++] = 0x80 | ((unichar >> 12) & 0x3f);
		outbuf[n++] = 0x80 | ((unichar >> 6) & 0x3f);
		outbuf[n++] = 0x80 | (unichar & 0x3f);
	} else if (unichar <= 0x3ffffff) {
		outbuf[n++] = 0xf8 | ((unichar >> 24) & 3);
		outbuf[n++] = 0x80 | ((unichar >> 18) & 0x3f);
		outbuf[n++] = 0x80 | ((unichar >> 12) & 0x3f);
		outbuf[n++] = 0x80 | ((unichar >> 6) & 0x3f);
		outbuf[n++] = 0x80 | (unichar & 0x3f);
	} else if (unichar <= 0x7fffffff) {
		outbuf[n++] = 0xfc | ((unichar >> 30) & 1);
		outbuf[n++] = 0x80 | ((unichar >> 24) & 0x3f);
		outbuf[n++] = 0x80 | ((unichar >> 18) & 0x3f);
		outbuf[n++] = 0x80 | ((unichar >> 12) & 0x3f);
		outbuf[n++] = 0x80 | ((unichar >> 6) & 0x3f);
		outbuf[n++] = 0x80 | (unichar & 0x3f);
	}

	outbuf[n] = 0;
}				/* uni2utf8 */

/*********************************************************************
And-convert the string; you know, &nbsp; &lt; etc.
This is the routine that makes it possible for me to read, and write,
my math site.  http://www.mathreference.com/accessible.html
In the invisible mode, graphics characters are not rendered at all.
This is used when translating attributes inside tags,
such as HREF, in an anchor.
The original string is not disturbed.
The new string is allocated.
*********************************************************************/

char *andTranslate(const char *s, bool invisible)
{
	char *new;
	int l, n, j;
	uchar c, d;
	uchar alnum = 0;	/* was last char an alphanumeric */
	bool premode = false;
	char andbuf[16];

	static const char *const andwords[] = {
		"gt\0>",
		"lt\0<",
		"quot\0\"",
		"raquo\0-",
		"ldquo\0\"",
		"rdquo\0\"",
		"lsquo\0'",
		"rsquo\0'",
		"plus\0+",
		"minus\0-",
		"mdash\0 - ",
		"ndash\0 - ",
		"colon\0:",
		"apos\0`",
		"star\0*",
		"comma\0,",
		"period\0.",
		"dot\0.",
		"dollar\0$",
		"percnt\0%",
		"amp\0&",
		"iexcl\0!",
		"cent\0\xa2",
		"pound\0\xa3",
		"yen\0\xa5",
		"brvbar\0\xa6",
		"copy\0\xa9",
		"reg\0\xae",
		"deg\0\xb0",
		"plusmn\0\xb1",
		"para\0\xb6",
		"sdot\0\xb7",
		"middot\0\xb7",
		"frac14\0\xbc",
		"half\0\xbd",
		"frac34\0\xbe",
		"iquest\0\xbf",
		"rsaquo\0*",
		"Agrave\0\xc0",
		"Aacute\0\xc1",
		"Acirc\0\xc2",
		"Atilde\0\xc3",
		"Auml\0\xc4",
		"Aring\0\xc5",
		"AElig\0\xc6",
		"Ccedil\0\xc7",
		"Egrave\0\xc8",
		"Eacute\0\xc9",
		"Ecirc\0\xca",
		"Euml\0\xcb",
		"Igrave\0\xcc",
		"Iacute\0\xcd",
		"Icirc\0\xce",
		"Iuml\0\xcf",
		"ETH\0\xd0",
		"Ntilde\0\xd1",
		"Ograve\0\xd2",
		"Oacute\0\xd3",
		"Ocirc\0\xd4",
		"Otilde\0\xd5",
		"Ouml\0\xd6",
		"times\0\xd7",
		"Oslash\0\xd8",
		"Ugrave\0\xd9",
		"Uacute\0\xda",
		"Ucirc\0\xdb",
		"Uuml\0\xdc",
		"Yacute\0\xdd",
		"THORN\0\xde",
		"szlig\0\xdf",
		"agrave\0\xe0",
		"aacute\0\xe1",
		"acirc\0\xe2",
		"atilde\0\xe3",
		"auml\0\xe4",
		"aring\0\xe5",
		"aelig\0\xe6",
		"ccedil\0\xe7",
		"egrave\0\xe8",
		"eacute\0\xe9",
		"ecirc\0\xea",
		"euml\0\xeb",
		"igrave\0\xec",
		"iacute\0\xed",
		"icirc\0\xee",
		"iuml\0\xef",
		"eth\0\xf0",
		"ntilde\0\xf1",
		"ograve\0\xf2",
		"oacute\0\xf3",
		"ocirc\0\xf4",
		"otilde\0\xf5",
		"ouml\0\xf6",
		"divide\0\xf7",
		"oslash\0\xf8",
		"ugrave\0\xf9",
		"uacute\0\xfa",
		"ucirc\0\xfb",
		"uuml\0\xfc",
		"yacute\0\xfd",
		"thorn\0\xfe",
		"yuml\0\xff",
		"Yuml\0Y",
		"itilde\0i",
		"Itilde\0I",
		"utilde\0u",
		"Utilde\0U",
		"edot\0e",
		"nbsp\0 ",
		"shy\0-",
		"frac13\01/3",
		"frac23\02/3",
		"plusmn\0+-",
		"laquo\0left arrow",
		"#171\0left arrow",
		"raquo\0arrow",
		"#187\0arrow",
		"micro\0micro",
		"trade\0(TM)",
		"hellip\0...",
		"#275\0`",
		"#773\0overbar",
		"#177\0+-",
		"#8211\0-",
		"#8212\0 - ",
		"#8216\0`",
		"#8217\0'",
		"#8220\0`",
		"#8221\0'",
		"bull\0*",
		0
	};

	if (!s)
		return 0;
	if (s == EMPTYSTRING)
		return EMPTYSTRING;
	new = initString(&l);

	while (c = *s) {
		if (c == InternalCodeChar && !invisible) {
			const char *t = s + 1;
			while (isdigitByte(*t))
				++t;
			if (t > s + 1 && *t && strchr("{}<>*", *t)) {	/* it's a tag */
				bool separate, pretag, slash;
				n = atoi(s + 1);
				preFormatCheck(n, &pretag, &slash);
				separate = (*t != '*');
				if (separate)
					alnum = 0;
				debugPrint(7, "tag %d%c separate %d", n, *t,
					   separate);
				if (pretag)
					premode = !slash;
				++t;
				stringAndBytes(&new, &l, s, t - s);
				s = t;
				continue;
			}	/* tag */
		}

		if (c != '&')
			goto putc;

		for (j = 0; j < sizeof(andbuf); ++j) {
			d = s[j + 1];
			if (d == '&' || d == ';' || d <= ' ')
				break;
		}
		if (j == sizeof(andbuf))
			goto putc;	/* too long, no match */
		strncpy(andbuf, s + 1, j);
		andbuf[j] = 0;
		++j;
		if (s[j] == ';')
			++j;
/* remove leading zeros */
		if (andbuf[0] == '#') {
			while (andbuf[1] == '0')
				strmove(andbuf + 1, andbuf + 2);
			if (andbuf[1] == 'x' || andbuf[1] == 'X') {
				unsigned int uc;
				uc = strtol(andbuf + 2, 0, 16);
				if (uc <= 0x7fffffff)
					sprintf(andbuf + 1, "%d", uc);
			}
		}

lookup:
		debugPrint(6, "meta %s", andbuf);
		n = stringInList(andwords, andbuf);
		if (n >= 0) {	/* match */
			const char *r = andwords[n] + strlen(andwords[n]) + 1;	/* replacement string */
			s += j;
			if (!r[1]) {	/* replace with a single character */
				c = *r;
				if (c & 0x80 && cons_utf8) {
					static char utfbuf[8];
					n = (uchar) c;
n_utf8:
					uni2utf8(n, (uchar *) utfbuf);
					r = utfbuf;
					goto putw;
				}
				--s;
				goto putc;
			}
			if (invisible) {
				s -= j;
				goto putc;
			}
/* We're replacing with a word */
			if (!invisible && isalnumByte(*r)) {
/* insert spaces either side */
				if (alnum)
					stringAndChar(&new, &l, ' ');
				alnum = 2;
			} else
				alnum = 0;
putw:
			stringAndString(&new, &l, r);
			continue;
		}

		if (andbuf[0] != '#')
			goto putc;
		if (andbuf[1] == 'x' || andbuf[1] == 'X') {
			n = strtol(andbuf + 2, 0, 16);
		} else
			n = stringIsNum(andbuf + 1);
		if (n < 0)
			goto putc;
		if (n > 0x7f && cons_utf8) {
			s += j;
			goto n_utf8;
		}
		if (n > 0xff)
			goto putc;
/* This line assumes iso8859-1; if you're anything else, you're screwed! */
/* I need to fix this some day, but most everybody is utf8 anyways. */
		c = n;
/* don't allow nulls */
		if (c == 0)
			c = ' ';
		if (strchr("\r\n\f", c) && !premode)
			c = ' ';
		if (c == InternalCodeChar)
			c = ' ';
		s += j - 1;

putc:
		if (isalnumByte(c)) {
			if (alnum == 2)
				stringAndChar(&new, &l, ' ');
			alnum = 1;
		} else
			alnum = 0;
		stringAndChar(&new, &l, c);
		++s;
	}			/* loop over input string */

	return new;
}				/* andTranslate */

/*********************************************************************
Crunch a to-list or a copy-to-list down to its email addresses.
Delimit them with newlines.
"Smith, John" <jsmith@whatever.com>
becomes
jsmith@whatever.com
*********************************************************************/

void extractEmailAddresses(char *line)
{
	char *s, *t;
	char *mark;		/* start of current entry */
	char quote = 0, c;

	for (s = t = mark = line; c = *s; ++s) {
		if (c == ',' && !quote) {
			mark = t + 1;
			c = ' ';
			goto append;
		}

		if (c == '"') {
			if (!quote)
				quote = c;
			else if (quote == c)
				quote = 0;
/* don't think you can quote in an email address */
			continue;
		}

		if (c == '<') {
			if (!quote) {
				quote = c;
				t = mark;
			}
			continue;
		}

		if (c == '>') {
			if (quote == '<')
				quote = 0;
			continue;
		}

		if (quote == '"')
			continue;

		if (c < ' ')
			c = ' ';
		if (c == ' ' && quote == '<')
			c = '_';

append:
		*t++ = c;
	}

	*t = 0;
	spaceCrunch(line, true, false);
	for (s = line; c = *s; ++s)
		if (c == ' ')
			*s = ',';
	if (*line)
		strcat(line, ",");
}				/* extractEmailAddresses */

static void cutDuplicateEmail(char *line, const char *dup, int duplen)
{
	char *s;
	while (*line) {
		s = strchr(line, ',');
		if (!s)
			return;	/* should never happen */
		if (duplen == s - line && memEqualCI(line, dup, duplen)) {
			++s;
			strmove(line, s);
			continue;
		}
		line = s + 1;
	}
}				/* cutDuplicateEmail */

void cutDuplicateEmails(char *tolist, char *cclist, const char *reply)
{
	int len;
	char *s, *t;

	len = strlen(reply);
	if (len) {
		cutDuplicateEmail(tolist, reply, len);
		cutDuplicateEmail(cclist, reply, len);
	}

	s = tolist;
	while (*s) {
		t = strchr(s, ',');
		if (!t)
			break;	/* should never happen */
		len = t - s;
		++t;
		cutDuplicateEmail(t, s, len);
		cutDuplicateEmail(cclist, s, len);
		s = t;
	}

	s = cclist;
	while (*s) {
		t = strchr(s, ',');
		if (!t)
			break;	/* should never happen */
		len = t - s;
		++t;
		cutDuplicateEmail(t, s, len);
		s = t;
	}

/* If your email address is on the to or cc list, drop it.
 * But retain it if it is the reply, in case you sent mail to yourself. */
	if (reply[0]) {
		struct MACCOUNT *m = accounts;
		int i;
		for (i = 0; i < maxAccount; ++i, ++m) {
			const char *r = m->reply;
			if (!r)
				continue;
			len = strlen(r);
			cutDuplicateEmail(tolist, r, len);
			cutDuplicateEmail(cclist, r, len);
		}
	}
}				/* cutDuplicateEmails */

/*********************************************************************
We got some data, from a file or from the internet.
Count the binary characters and decide if this is, on the whole,
binary or text.  I allow some nonascii chars,
like you might see in Spanish or German, and still call it text,
but if there's too many such chars, I call it binary.
It's not an exact science.
*********************************************************************/

bool looksBinary(const char *buf, int buflen)
{
	int i, bincount = 0;
	for (i = 0; i < buflen; ++i) {
		char c = buf[i];
		if (c <= 0)
			++bincount;
	}
	return (bincount * 4 - 10 >= buflen);
}				/* looksBinary */

void looks_8859_utf8(const char *buf, int buflen, bool * iso_p, bool * utf8_p)
{
	int utfcount = 0, isocount = 0;
	int i, j, bothcount;

	for (i = 0; i < buflen; ++i) {
		char c = buf[i];
		if (c >= 0)
			continue;
/* This is the start of the nonascii sequence. */
/* No second bit, it has to be iso. */
		if (!(c & 0x40)) {
isogo:
			++isocount;
			continue;
		}
/* Next byte has to start with 10 to be utf8, else it's iso */
		if (((uchar) buf[i + 1] & 0xc0) != 0x80)
			goto isogo;
		c <<= 2;
		for (j = i + 2; c < 0; ++j, c <<= 1)
			if (((uchar) buf[j] & 0xc0) != 0x80)
				goto isogo;
		++utfcount;
		i = j - 1;
	}

	*iso_p = *utf8_p = false;

	bothcount = isocount + utfcount;
	if (!bothcount)
		return;		/* ascii */
	bothcount *= 6;
	if (utfcount * 7 >= bothcount)
		*utf8_p = true;
	if (isocount * 7 >= bothcount)
		*iso_p = true;
}				/* looks_8859_utf8 */

/*********************************************************************
Convert a string from iso 8859 to utf8, or vice versa.
In each case a new string is allocated.
Don't forget to free it when you're done.
*********************************************************************/

/* only 8859-1 and 8859-2 so far */
static const int iso_unicodes[2][128] = {
	{0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b,
	 0x8c, 0x8d, 0x8e, 0x8f,
	 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a,
	 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa,
	 0xab, 0xac, 0xad, 0xae, 0xaf,
	 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba,
	 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca,
	 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
	 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
	 0xeb, 0xec, 0xed, 0xee, 0xef,
	 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa,
	 0xfb, 0xfc, 0xfd, 0xfe, 0xff},
	{0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b,
	 0x8c, 0x8d, 0x8e, 0x8f,
	 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a,
	 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	 0xa0, 0x104, 0x2d8, 0x141, 0xa4, 0x13d, 0x15a, 0xa7, 0xa8, 0x160,
	 0x15e, 0x164, 0x179, 0xad, 0x17d, 0x17b,
	 0xb0, 0x105, 0x2db, 0x142, 0xb4, 0x13e, 0x15b, 0x2c7, 0xb8, 0x161,
	 0x15f, 0x165, 0x17a, 0x2dd, 0x17e, 0x17c,
	 0x154, 0xc1, 0xc2, 0x102, 0xc4, 0x139, 0x106, 0xc7, 0x10c, 0xc9,
	 0x118, 0xcb, 0x11a, 0xcd, 0xce, 0x10e,
	 0x110, 0x143, 0x147, 0xd3, 0xd4, 0x150, 0xd6, 0xd7, 0x158, 0x16e,
	 0xda, 0x170, 0xdc, 0xdd, 0x162, 0xdf,
	 0x155, 0xe1, 0xe2, 0x103, 0xe4, 0x13a, 0x107, 0xe7, 0x10d, 0xe9,
	 0x119, 0xeb, 0x11b, 0xed, 0xee, 0x10f,
	 0x111, 0x144, 0x148, 0xf3, 0xf4, 0x151, 0xf6, 0xf7, 0x159, 0x16f,
	 0xfa, 0x171, 0xfc, 0xfd, 0x163, 0x2d9},
};

void iso2utf(const char *inbuf, int inbuflen, char **outbuf_p, int *outbuflen_p)
{
	int i, j;
	int nacount = 0;
	char c;
	char *outbuf;
	const int *isoarray = iso_unicodes[type8859 - 1];
	int ucode;

	if (!inbuflen) {
		*outbuf_p = EMPTYSTRING;
		*outbuflen_p = 0;
		return;
	}

/* count chars, so we can allocate */
	for (i = 0; i < inbuflen; ++i) {
		c = inbuf[i];
		if (c < 0)
			++nacount;
	}

	outbuf = allocMem(inbuflen + nacount + 1);
	for (i = j = 0; i < inbuflen; ++i) {
		c = inbuf[i];
		if (c >= 0) {
			outbuf[j++] = c;
			continue;
		}
		ucode = isoarray[c & 0x7f];
		outbuf[j++] = (ucode >> 6) | 0xc0;
		outbuf[j++] = (ucode & 0x3f) | 0x80;
	}
	outbuf[j] = 0;		/* just for fun */

	*outbuf_p = outbuf;
	*outbuflen_p = j;
}				/* iso2utf */

void utf2iso(const char *inbuf, int inbuflen, char **outbuf_p, int *outbuflen_p)
{
	int i, j, k;
	char c;
	char *outbuf;
	const int *isoarray = iso_unicodes[type8859 - 1];
	int ucode;

	if (!inbuflen) {
		*outbuf_p = EMPTYSTRING;
		*outbuflen_p = 0;
		return;
	}

	outbuf = allocMem(inbuflen + 1);
	for (i = j = 0; i < inbuflen; ++i) {
		c = inbuf[i];

/* regular chars and nonascii chars that aren't utf8 pass through. */
/* There shouldn't be any of the latter */
		if (((uchar) c & 0xc0) != 0xc0) {
			outbuf[j++] = c;
			continue;
		}

/* Convertable into 11 bit */
		if (((uchar) c & 0xe0) == 0xc0
		    && ((uchar) inbuf[i + 1] & 0xc0) == 0x80) {
			ucode = c & 0x1f;
			ucode <<= 6;
			ucode |= (inbuf[i + 1] & 0x3f);
			for (k = 0; k < 128; ++k)
				if (isoarray[k] == ucode)
					break;
			if (k < 128) {
				outbuf[j++] = k | 0x80;
				++i;
				continue;
			}
		}

/* unicodes not found in our iso class are converted into stars */
		c <<= 1;
		++i;
		for (++i; c < 0; ++i, c <<= 1) {
			if (((uchar) outbuf[i] & 0xc0) != 0x80)
				break;
		}
		outbuf[j++] = '*';
		--i;
	}
	outbuf[j] = 0;		/* just for fun */

	*outbuf_p = outbuf;
	*outbuflen_p = j;
}				/* utf2iso */

static char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
 * Encode some data in base64.
 * inbuf points to the data
 * inlen is the length of the data
 * lines is a boolean, indicating whether to add newlines to the output.
 * If true, newlines will be added after each group of 72 output bytes.
 * Returns: A freshly-allocated NUL-terminated string, containing the
 * base64 representation of the data. */
char *base64Encode(const char *inbuf, int inlen, bool lines)
{
	char *out, *outstr;
	uchar *in = (uchar *) inbuf;
	int colno;
	int outlen = ((inlen / 3) + 1) * 4;
	++outlen;		/* zero on the end */
	if (lines)
		outlen += (inlen / 54) + 1;
	outstr = out = allocMem(outlen);
	colno = 0;
	while (inlen >= 3) {
		*out++ = base64_chars[(int)(*in >> 2)];
		*out++ = base64_chars[(int)((*in << 4 | *(in + 1) >> 4) & 63)];
		*out++ =
		    base64_chars[(int)((*(in + 1) << 2 | *(in + 2) >> 6) & 63)];
		*out++ = base64_chars[(int)(*(in + 2) & 63)];
		inlen -= 3;
		in += 3;
		if (!lines)
			continue;
		colno += 4;
		if (colno < 72)
			continue;
		*out++ = '\n';
		colno = 0;
	}
	if (inlen == 1) {
		*out++ = base64_chars[(int)(*in >> 2)];
		*out++ = base64_chars[(int)(*in << 4 & 63)];
		*out++ = '=';
		*out++ = '=';
		colno += 4;
	}
	if (inlen == 2) {
		*out++ = base64_chars[(int)(*in >> 2)];
		*out++ = base64_chars[(int)((*in << 4 | *(in + 1) >> 4) & 63)];
		*out++ = base64_chars[(int)((*(in + 1) << 2) & 63)];
		*out++ = '=';
		colno += 4;
	}
/* finish the last line */
	if (lines && colno)
		*out++ = '\n';
	*out = 0;
	return outstr;
}				/* base64Encode */

uchar base64Bits(char c)
{
	if (isupperByte(c))
		return c - 'A';
	if (islowerByte(c))
		return c - ('a' - 26);
	if (isdigitByte(c))
		return c - ('0' - 52);
	if (c == '+')
		return 62;
	if (c == '/')
		return 63;
	return 64;		/* error */
}				/* base64Bits */

/*
 * Decode some data in base64.
 * This function operates on the data in-line.  It does not allocate a fresh
 * string to hold the decoded data.  Since the data will be smaller than
 * the base64 encoded representation, this cannot overflow buffers.
 * If you need to preserve the input, copy it first.
 *
 * start points to the start of the input
 * *end initially points to the byte just after the end of the input
 * Returns: GOOD_BASE64_DECODE on success, BAD_BASE64_DECODE or
 * EXTRA_CHARS_BASE64_DECODE on error.
 * When the function returns success, *end points to the end of the decoded
 * data.  On failure, end points to the just past the end of
 * what was successfully decoded. */
int base64Decode(char *start, char **end)
{
	char *b64_end = *end;
	uchar val, leftover, mod;
	bool equals;
	int ret = GOOD_BASE64_DECODE;
	char c, *q, *r;
/* Since this is a copy, and the unpacked version is always
 * smaller, just unpack it inline. */
	mod = 0;
	equals = false;
	for (q = r = start; q < b64_end; ++q) {
		c = *q;
		if (isspaceByte(c))
			continue;
		if (equals) {
			if (c == '=')
				continue;
			ret = EXTRA_CHARS_BASE64_DECODE;
			break;
		}
		if (c == '=') {
			equals = true;
			continue;
		}
		val = base64Bits(c);
		if (val & 64) {
			ret = BAD_BASE64_DECODE;
			break;
		}
		if (mod == 0) {
			leftover = val << 2;
		} else if (mod == 1) {
			*r++ = (leftover | (val >> 4));
			leftover = val << 4;
		} else if (mod == 2) {
			*r++ = (leftover | (val >> 2));
			leftover = val << 6;
		} else {
			*r++ = (leftover | val);
		}
		++mod;
		mod &= 3;
	}
	*end = r;
	return ret;
}				/* base64Decode */

void
iuReformat(const char *inbuf, int inbuflen, char **outbuf_p, int *outbuflen_p)
{
	bool is8859, isutf8;

	*outbuf_p = 0;
	*outbuflen_p = 0;
	if (!iuConvert)
		return;

	looks_8859_utf8(inbuf, inbuflen, &is8859, &isutf8);
	if (cons_utf8 && is8859) {
		debugPrint(3, "converting to utf8");
		iso2utf(inbuf, inbuflen, outbuf_p, outbuflen_p);
	}
	if (!cons_utf8 && isutf8) {
		debugPrint(3, "converting to iso8859");
		utf2iso(inbuf, inbuflen, outbuf_p, outbuflen_p);
	}
}				/* iuReformat */
