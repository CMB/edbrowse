/* format.c
 * Format text, establish line breaks, manage whitespace.
 * Copyright (c) Karl Dahlke, 2006
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

void
prepareForBrowse(char *h, int h_len)
{
    int i, j;

    for(i = j = 0; i < h_len; ++i) {
	if(h[i] == 0)
	    h[i] = ' ';
	if(h[i] == '\b') {
	    if(i && !strchr("\n\b<>'\"&", h[i - 1]))
		--j;
	    continue;
	}
	if(h[i] == (char)0xe2 && i < h_len - 1 && h[i + 1] == (char)0x80) {
	    ++i;
	    continue;
	}
	if(h[i] == InternalCodeChar)
	    h[i] = InternalCodeCharAlternate;
	h[j++] = h[i];
    }
    h[j] = 0;			/* now it's a string */

/* undos the file */
    for(i = j = 0; h[i]; ++i) {
	if(h[i] == '\r' && h[i + 1] == '\n')
	    continue;
	h[j++] = h[i];
    }
    h[j] = 0;
}				/* prepareForBrowse */


/*********************************************************************
Skip past an html comment.
Parse an html tag <tag foo=bar>
*********************************************************************/

const char *
skipHtmlComment(const char *h, int *lines)
{
    int lns = 0;
    bool comm = h[2] == '-' && h[3] == '-';
    bool php = memEqualCI(h + 1, "?php", 4);

    h += comm ? 4 : 2;
    while(*h) {
	if(php) {		/* special type of comment */
	    if(*h == '?' && h[1] == '>') {
		h += 2;
		goto done;
	    }
	    ++h;
	    continue;
	}

	if(!comm && *h == '>') {
	    ++h;
	    goto done;
	}

	if(comm && h[0] == '-' && h[1] == '-') {
	    h += 2;
	    while(*h == '-')
		h++;
	    while(isspaceByte(*h)) {
		if(*h == '\n')
		    ++lns;
		h++;
	    }
	    if(!*h)
		goto done;
	    if(*h == '>') {
		++h;
		goto done;
	    }
	    continue;
	}

	if(*h == '\n')
	    ++lns;
	h++;
    }

  done:
    if(lines)
	*lines = lns;
    return h;
}				/* skipHtmlComment */

/* an attribute character */
static bool
atchr(char c)
{
    return (c > ' ' && c != '=' && c != '<' && c != '>');
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
    if(*e++ != '<')
	return false;
    if(name)
	*name = e;
    if(*e == '/')
	e++;
    if(!isA(*e))
	return false;
    while(isA(*e) || *e == '=')
	++e;
    if(!isspaceByte(*e) && *e != '>' && *e != '<' && *e != '/' && *e != ':')
	return false;
/* Note that name includes the leading / */
    if(name && namelen)
	*namelen = e - *name;
/* skip past space colon slash */
    while(isspaceByte(*e) || *e == '/' || *e == ':') {
	if(*e == '\n')
	    ++lns;
	++e;
    }
/* should be the start of the first attribute, or < or > */
    if(!atchr(*e) && *e != '>' && *e != '<')
	return false;
    if(attr)
	*attr = e;
  nextattr:
    if(*e == '>' || *e == '<')
	goto en;
    if(!atchr(*e))
	return false;
    while(atchr(*e))
	++e;
    while(isspaceByte(*e)) {
	if(*e == '\n')
	    ++lns;
	++e;
    }
    if(*e != '=')
	goto nextattr;
    ++e;
    while(isspaceByte(*e)) {
	if(*e == '\n')
	    ++lns;
	++e;
    }
    if(isquote(*e)) {
	unsigned char uu = *e;
      x3:
	++e;
	while(*e != uu && *e) {
	    if(*e == '\n')
		++lns;
	    ++e;
	}
	if(*e != uu)
	    return false;
	++e;
	if(*e == uu) {
/* lots of tags end with an extra quote */
	    if(e[1] == '>')
		*e = ' ';
	    else
		goto x3;
	}
    } else {
	while(!isspaceByte(*e) && *e != '>' && *e != '<' && *e)
	    ++e;
    }
    while(isspaceByte(*e)) {
	if(*e == '\n')
	    ++lns;
	++e;
    }
    goto nextattr;
  en:
/* could be < or > */
    if(end)
	*end = e + (*e == '>');
    if(lines)
	*lines = lns;
    return true;
}				/* parseTag */

/* Don't know why he didn't use the stringAndChar() functions, but he
 * invented something new here, so on we go. */
static void
valChar(char **sp, int *lp, char c)
{
    char *s = *sp;
    int l = *lp;
    if(!(l % ALLOC_GR))
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

char *
htmlAttrVal(const char *e, const char *name)
{
    const char *n;
    char *a = EMPTYSTRING;	/* holds the value */
    char *b;
    int l = 0;			/* length */
    char f;
    if(!e)
	return a;
  top:
    while(isspaceByte(*e))
	e++;
    if(!*e)
	return 0;
    if(*e == '>' || *e == '<')
	return 0;
    n = name;
    while(*n && !((*e ^ *n) & 0xdf))
	e++, n++;
    f = *n;
    while(atchr(*e))
	f = 'x', e++;
    while(isspaceByte(*e))
	e++;
    if(*e != '=')
	goto ea;
    e++;
    while(isspaceByte(*e))
	e++;
    if(!isquote(*e)) {
	while(*e && !isspaceByte(*e) && *e != '>' && *e != '<') {
	    if(!f)
		valChar(&a, &l, *e);
	    e++;
	}
    } else {
	char uu = *e;
      a:
	e++;
	while(*e != uu) {
	    if(!*e) {
		nzFree(a);
		return NULL;
	    }
	    if(!f && *e != '\r') {
		if(*e != '\t' && *e != '\n')
		    valChar(&a, &l, *e);
		else if(!htmlAttrVal_nl)
		    valChar(&a, &l, ' ');
	    }
	    e++;
	}
	e++;
	if(*e == uu) {
	    if(!f)
		valChar(&a, &l, uu);
	    goto a;
	}
    }
  ea:
    if(f)
	goto top;		/* no match, next attribute */
    if(l)
	valChar(&a, &l, 0);	/* null terminate */
    if(strchr(a, '&')) {
	b = a;
	a = andTranslate(b, true);
	nzFree(b);
    }
/* strip leading and trailing spaces.
 * Are we really suppose to do this? */
    for(b = a; *b == ' '; b++) ;
    if(b > a)
	strcpy(a, b);
    for(b = a + strlen(a) - 1; b >= a && *b == ' '; b--)
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
    int js_nl = 0;

    sprintf(look, "</%s>", tagname);

  retry:
    end = strstrCI(s, look);
    if(!end) {
	rc = false;
	browseError(MSG_CloseTag, look);
	end = (char *)h + strlen(h);
    } else if(is_js) {
/* Check for document.write("</script>");
 * This isn't legal javascript, but it happens all the time!
 * This is a really stupid check.
 * Scan forward 30 chars, on the same line, looking
 * for a quote, and ) ; or + */
	char c;
	int j;
	s = end + strlen(look);
	for(j = 0; j < 30; ++j, ++s) {
	    c = *s;
	    if(!c)
		break;
	    if(c == '\n')
		break;
	    if(c != '"' && c != '\'')
		continue;
	    while(s[1] == ' ')
		++s;
	    c = s[1];
	    if(!c)
		break;
	    if(strchr(";)+", c))
		goto retry;
	}
    }
    if(end_p)
	*end_p = end;
    if(new_p)
	*new_p = pullString1(h, end);
/* count the newlines */
    while(h < end) {
	if(*h == '\n')
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
State variables remember:
Whether we are in a preformatted section
Whether we have seen any visible text in the document
Whether we have seen any visible text in the current hyperlink,
	between the braces.
Whether we are stepping through a span of whitespace.
A tag and adjacent whitespace might be swapped, depending on state.
If a change is made, the procedure is run again,
kinda like bubble sort.
It has the potential to be terribly inefficient,
but that's not likely.
Use cnt to count the iterations, just for debugging.
*********************************************************************/

void
anchorSwap(char *buf)
{
    char c, d, *s, *ss, *w, *a;
    bool premode, pretag, state_braces, state_text, state_atext;
    bool strong, change, slash;
    int n, cnt;
    char tag[20];

/* Transliterate a few characters.  One of them is 0xa0 to space,
 * so we need to do this now, before the anchors swap with whitespace.
 * Also get rid of hyperlinks with absolutely nothing to click on. */
    for(s = w = buf; c = *s; ++s) {
	static const char from[] =
	   "\x1b\x95\x99\x9c\x9d\x91\x92\x93\x94\xa0\xad\x96\x97\x85\xa6\xc2";
	static const char becomes[] = "_*'`'`'`' ----- ";
	ss = strchr(from, c);
	if(ss)
	    c = becomes[ss - from];
	if(c != (char)InternalCodeChar)
	    goto put1;
	if(!isdigitByte(s[1]))
	    goto put1;
	for(a = s + 2; isdigitByte(*a); ++a) ;
	if(*a != '{')
	    goto put1;
	for(++a; *a == ' '; ++a) ;
	if(memcmp(a, "\2000}", 3))
	    goto put1;
	s = a + 2;
	continue;
      put1:
	*w++ = c;
    }
    *w = 0;

    cnt = 0;
    change = true;
    while(change) {
	change = false;
	++cnt;
	premode = state_text = state_atext = state_braces = false;
/* w represents the state of whitespace */
	w = 0;
/* a represents the state of being in an anchor */
	a = 0;

	for(s = buf; c = *s; ++s) {
	    if(isspaceByte(c)) {
		if(!w)
		    w = s;
		continue;
	    }

/* end of white space, should we swap it with prior tag? */
	    if(w && a && !premode &&
	       ((state_braces & !state_atext) ||
	       ((!state_braces) & !state_text))) {
		memcpy(a, w, s - w);
		memcpy(a + (s - w), tag, n);
		change = true;
		w = 0;
	    }

/* prior anchor has no significance */
	    a = 0;

	    if(c == (char)InternalCodeChar) {
		if(!isdigitByte(s[1]))
		    goto normalChar;
		n = strtol(s + 1, &ss, 10);
		preFormatCheck(n, &pretag, &slash);
		d = *ss;
/* the following should never happen */
		if(!strchr("{}<>*", d))
		    goto normalChar;
		n = ss + 1 - s;
		memcpy(tag, s, n);
		tag[n] = 0;

		if(pretag) {
		    w = 0;
		    premode = !slash;
		    s = ss;
		    continue;
		}

/* We have a tag, should we swap it with prior whitespace? */
		if(w && !premode &&
		   (d == '}' ||
		   d == '@' &&
		   ((state_braces & state_atext) ||
		   ((!state_braces) & state_text)))) {
		    memmove(w + n, w, s - w);
		    memcpy(w, tag, n);
		    change = true;
		    w += n;
		    if(d == '}')
			state_braces = false;
		    s = ss;
		    continue;
		}

/* prior whitespace doesn't matter any more */
		w = 0;

		if(d == '{') {
		    state_braces = state_text = true;
		    state_atext = false;
		    a = s;
		    s = ss;
		    continue;
		}

		if(d == '}') {
		    state_braces = false;
		    s = ss;
		    continue;
		}

		if(d == '*') {
		    if(state_braces)
			state_atext = true;
		    else
			state_text = true;
		    a = s;
		    s = ss;
		    continue;
		}

/* The remaining tags are <>, for an input field. */
		s = ss;
		c = d;
/* end of tag processing */
	    }

	  normalChar:
	    w = 0;		/* no more whitespace */
	    if(state_braces)
		state_atext = true;
	    else
		state_text = true;
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
    for(s = w = buf; c = *s; ++s) {
	char open, close, linkchar;
	if(!strchr("{[(<", c))
	    goto putc;
	if(s[1] != (char)InternalCodeChar)
	    goto putc;
	if(!isdigitByte(s[2]))
	    goto putc;
	for(a = s + 3; isdigitByte(*a); ++a) ;
	linkchar = 0;
	if(*a == '{')
	    linkchar = '}';
	if(*a == '<')
	    linkchar = '>';
	if(!linkchar)
	    goto putc;
	open = c;
	close = 0;
	if(open == '{')
	    close = '}';
	if(open == '[')
	    close = ']';
	if(open == '(')
	    close = ')';
	if(open == '<')
	    close = '>';
	n = 1;
	while(n < 120) {
	    d = a[n++];
	    if(!d)
		break;
	    if(d != (char)InternalCodeChar)
		continue;
	    while(isdigitByte(a[n]))
		++n;
	    d = a[n++];
	    if(!d)
		break;		/* should never happen */
	    if(strchr("{}<>", d))
		break;
	}
	if(n >= 120)
	    goto putc;
	if(d != linkchar)
	    goto putc;
	a += n;
	if(*a != close)
	    goto putc;
	++s;
	memcpy(w, s, a - s);
	w += a - s;
	s = a;
	ss = 0;
	continue;
      putc:
	if(c == '|' && !ss)
	    ss = w;
	if(strchr("\r\n\f", c) && ss)
	    w = ss, ss = 0;
	if(!isspaceByte(c) && c != '|')
	    ss = 0;
	*w++ = c;
    }				/* loop over buffer */
    *w = 0;
    debugPrint(3, "anchors unframed");

/* Now compress the implied linebreaks into one. */
    premode = false;
    for(s = buf; c = *s; ++s) {
	if(c == (char)InternalCodeChar && isdigitByte(s[1])) {
	    n = strtol(s + 1, &s, 10);
	    if(*s == '*') {
		preFormatCheck(n, &pretag, &slash);
		if(pretag)
		    premode = !slash;
	    }
	}
	if(!isspaceByte(c))
	    continue;
	strong = false;
	a = 0;
	for(w = s; isspaceByte(*w); ++w) {
	    if(*w == '\n' || *w == '\f')
		strong = true;
	    if(*w == '\r' && !a)
		a = w;
	}
	ss = s, s = w - 1;
	if(!a)
	    continue;
	if(premode)
	    continue;
	if(strong) {
	    for(w = ss; w <= s; ++w)
		if(*w == '\r')
		    *w = ' ';
	    continue;
	}
	for(w = ss; w <= s; ++w)
	    if(*w == '\r' && w != a)
		*w = ' ';
    }				/* loop over buffer */
    debugPrint(3, "whitespace combined");
}				/* anchorSwap */


/*********************************************************************
Format text, and break lines at sentence/phrase boundaries.
The prefix bl means breakline.
*********************************************************************/

static char *bl_start, *bl_cursor, *bl_end;
static bool bl_overflow;
static int colno;		/* column number */
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

static void
debugChunk(const char *chunk, int len)
{
    int i;
    if(debugLevel < 7)
	return;
    printf("chunk<");
    for(i = 0; i < len; ++i) {
	char c = chunk[i];
	if(c == '\t') {
	    printf("\\t");
	    continue;
	}
	if(c == '\n') {
	    printf("\\n");
	    continue;
	}
	if(c == '\f') {
	    printf("\\f");
	    continue;
	}
	if(c == '\r') {
	    printf("\\r");
	    continue;
	}
	if(c == '\0') {
	    printf("\\0");
	    continue;
	}
	printf("%c", c);
    }
    printf(">%d.%d\n", colno, lspace);
}				/* debugChunk */

static void
appendOneChar(char c)
{
    if(bl_cursor == bl_end)
	bl_overflow = true;
    else
	*bl_cursor++ = c;
}				/* appendOneChar */

static bool
spaceNotInInput(void)
{
    char *t = bl_cursor;
    char c;
    for(--t; t >= bl_start; --t) {
	c = *t;
	if(c == '\n' || c == '\r')
	    return true;
	if(c == '>' && t >= bl_start + 2 &&
	   t[-1] == '0' && t[-2] == (char)InternalCodeChar)
	    return true;
	if(c != '<')
	    continue;
	while(t > bl_start && isdigitByte(t[-1]))
	    --t;
	if(*t == '<')
	    continue;
	if(t > bl_start && t[-1] == (char)InternalCodeChar)
	    return false;
    }
    return true;
}				/* spaceNotInInput */

static void
appendSpaceChunk(const char *chunk, int len, bool premode)
{
    int nlc = pre_cr;		/* newline count */
    int spc = 0;		/* space count */
    int i, j;
    char c, d, e;

    if(!len)
	return;
    for(i = 0; i < len; ++i) {
	c = chunk[i];
	if(c == '\n' || c == '\r') {
	    ++nlc, spc = 0;
	    continue;
	}
	if(c == '\f') {
	    nlc += 2, spc = 0;
	    continue;
	}
	++spc;
    }

    if(!premode && spaceNotInInput()) {
	int l = bl_cursor - bl_start;
	c = d = ' ';
	if(l)
	    d = bl_cursor[-1];
	if(l > 1)
	    c = bl_cursor[-2];
	e = d;
	if(strchr(")\"|}", d))
	    e = c;
	if(strchr(".?!:", e)) {
	    bool ok = true;
/* Check for Mr. Mrs. and others. */
	    if(e == '.' && bl_cursor - bl_start > 10) {
		static const char *const prefix[] =
		   { "mr.", "mrs.", "sis.", "ms.", 0 };
		char trailing[12];
		for(i = 0; i < 6; ++i) {
		    c = bl_cursor[i - 6];
		    if(isupperByte(c))
			c = tolower(c);
		    trailing[i] = c;
		}
		trailing[i] = 0;
		for(i = 0; prefix[i]; ++i)
		    if(strstr(trailing, prefix[i]))
			ok = false;
/* Check for John C. Calhoon */
		if(isupperByte(bl_cursor[-2]) && isspaceByte(bl_cursor[-3]))
		    ok = false;
	    }
	    if(ok)
		lperiod = colno, idxperiod = l;
	}
	e = d;
	if(strchr(")\"|", d))
	    e = c;
	if(strchr("-,;", e))
	    lcomma = colno, idxcomma = l;
	if(strchr(")\"|", d))
	    lright = colno, idxright = l;
	lany = colno, idxany = l;
/* tack a short fragment onto the previous line. */
	if(longcut && colno <= 15 && (nlc || lperiod == colno)) {
	    bl_start[longcut] = ' ';
	    if(!nlc)
		len = spc = 0, nlc = 1;
	}			/* pasting small fragment onto previous line */
    }				/* allowing line breaks */
    if(lspace == 3)
	nlc = 0;
    if(nlc) {
	if(lspace == 2)
	    nlc = 1;
	appendOneChar('\n');
	if(nlc > 1)
	    appendOneChar('\n');
	colno = 1;
	longcut = lperiod = lcomma = lright = lany = 0;
	if(lspace >= 2 || nlc > 1)
	    lspace = 3;
	if(lspace < 2)
	    lspace = 2;
	if(!premode)
	    return;
    }
    if(!spc)
	return;
    if(!premode) {
/* if the first char of the text to be reformatted is space,
 * then we will wind up here, with lspace = 3. */
	if(lspace == 3)
	    return;
	appendOneChar(' ');
	++colno;
	lspace = 1;
	return;
    }
    j = -1;
    for(i = 0; i < len; ++i) {
	c = chunk[i];
	if(c == '\n' || c == '\r' || c == '\f')
	    j = i;
    }
    i = j + 1;
    if(i)
	colno = 1;
    for(; i < len; ++i) {
	c = chunk[i];
	if(c == 0)
	    c = ' ';
	appendOneChar(c);
	if(c == ' ')
	    ++colno;
	if(c == '\t')
	    colno += 4;
    }
    lspace = 1;
}				/* appendSpaceChunk */

static void
appendPrintableChunk(const char *chunk, int len, bool premode)
{
    int i, j;
    for(i = 0; i < len; ++i)
	appendOneChar(chunk[i]);
    colno += len;
    lspace = 0;
    if(premode)
	return;
    if(colno <= optimalLine)
	return;
/* Oops, line is getting long.  Let's see where we can cut it. */
    i = j = 0;
    if(lperiod > cutLineAfter)
	i = lperiod, j = idxperiod;
    else if(lcomma > cutLineAfter)
	i = lcomma, j = idxcomma;
    else if(lright > cutLineAfter)
	i = lright, j = idxright;
    else if(lany > cutLineAfter)
	i = lany, j = idxany;
    if(!j)
	return;			/* nothing we can do about it */
    longcut = 0;
    if(i != lperiod)
	longcut = j;
    bl_start[j] = '\n';
    colno -= i;
    lperiod -= i;
    lcomma -= i;
    lright -= i;
    lany -= i;
}				/* appendPrintableChunk */

/* Break up a line using the above routines.
 * The buffer for the new text must be supplied.
 * Return false (fail) if we ran out of room.
 * This function is called from bufsup.c, implementing the bl command,
 * and is only in this file because it shares the above routines and variables
 * with the html reformatting, which really has to be here. */
bool
breakLine(const char *line, int len, int *newlen)
{
    char c, state, newstate;
    int i, last;

    pre_cr = 0;
    if(len && line[len - 1] == '\r')
	--len;
    if(lspace == 4) {
/* special continuation code from the previous invokation */
	lspace = 2;
	if(line[0])
	    ++pre_cr;
    }
    if(len > paraLine)
	++pre_cr;
    if(lspace < 2)
	lspace = 2;		/* should never happen */
    if(!len + pre_cr)
	lspace == 3;
    bl_start = bl_cursor = replaceLine;
    bl_end = replaceLine + REPLACELINELEN - 8;
    bl_overflow = false;
    colno = 1;
    longcut = lperiod = lcomma = lright = lany = 0;
    last = 0;
    state = 0;
    if(pre_cr)
	state = 1;

    for(i = 0; i < len; ++i) {
	c = line[i];
	newstate = 2;
	if(!c || strchr(" \t\n\r\f", c))
	    newstate = 1;
	if(state == newstate)
	    continue;
	if(!state) {
	    state = newstate;
	    continue;
	}

/* state change here */
	debugChunk(line + last, i - last);
	if(state == 1)
	    appendSpaceChunk(line + last, i - last, false);
	else
	    appendPrintableChunk(line + last, i - last, false);
	last = i;
	state = newstate;
	pre_cr = 0;
    }

    if(state) {			/* last token */
	debugChunk(line + last, len - last);
	if(state == 1)
	    appendSpaceChunk(line + last, len - last, false);
	else
	    appendPrintableChunk(line + last, len - last, false);
    }

    if(lspace < 2) {		/* line didn't have a \r at the end */
	appendSpaceChunk("\n", 1, false);
    }
    if(bl_cursor - bl_start > paraLine)
	lspace = 4;
    debugPrint(7, "chunk<EOL>%d.%d", colno, lspace);
    *newlen = bl_cursor - bl_start;
    return !bl_overflow;
}				/* breakLine */

void
breakLineSetup(void)
{
    lspace = 3;
}

char *
htmlReformat(const char *buf)
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

    for(h = buf; (c = *h); h = nh) {
	if(isspaceByte(c)) {
	    for(s = h + 1; isspaceByte(*s); ++s) ;
	    nh = s;
	    appendSpaceChunk(h, nh - h, premode);
	    if(lspace == 3 || lspace == 2 &&
	       (bl_cursor - bl_start) >= (bl_end - bl_start) * 2 / 3) {
		if(bl_cursor > bl_start)
		    stringAndBytes(&new, &l, bl_start, bl_cursor - bl_start);
		bl_cursor = bl_start;
		lspace = 3;
		longcut = lperiod = lcomma = lright = lany = 0;
		colno = 1;
	    }
	    continue;
	}
	/* white space */
	if(c != (char)InternalCodeChar) {
	    for(s = h + 1; *s; ++s)
		if(isspaceByte(*s) || *s == (char)InternalCodeChar)
		    break;
	    nh = s;
	    appendPrintableChunk(h, nh - h, premode);
	    continue;
	}

	/* word */
	/* It's a tag */
	tagno = strtol(h + 1, (char **)&nh, 10);
	c = *nh++;
	if(!c || !strchr("{}<>*", c))
	    errorPrint("@tag code %d has bad character %c following", tagno, c);
	appendPrintableChunk(h, nh - h, premode);
	preFormatCheck(tagno, &pretag, &slash);
	if(pretag)
	    premode = !slash;

/* Insert newlines between adjacent hyperlinks. */
	if(c != '}' || premode)
	    continue;
	for(h = nh; c = *h; ++h)
	    if(!strchr(" \t,:-|;", c))
		break;
	if(!c || strchr("\r\n\f", c)) {
	    nh = h;
	    continue;
	}
	if(c != (char)InternalCodeChar)
	    continue;
/* Does this start a new hyperlink? */
	for(s = h + 1; isdigitByte(*s); ++s) ;
	if(*s != '{')
	    continue;
	appendSpaceChunk("\n", 1, false);
	nh = h;
    }				/* loop over text */

/* close off the last line */
    if(lspace < 2)
	appendSpaceChunk("\n", 1, true);
    if(bl_cursor > bl_start)
	stringAndBytes(&new, &l, bl_start, bl_cursor - bl_start);
/* Get rid of last space. */
    if(l >= 2 && new[l - 1] == '\n' && new[l - 2] == ' ')
	new[l - 2] = '\n', new[--l] = 0;
/* Don't need empty lines at the end. */
    while(l > 1 && new[l - 1] == '\n' && new[l - 2] == '\n')
	--l;
    new[l] = 0;
/* Don't allow an empty buffer */
    if(!l)
	stringAndChar(&new, &l, '\n');

    return new;
}				/* htmlReformat */


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

char *
andTranslate(const char *s, bool invisible)
{
    char *new;
    int l, n, j;
    uchar c, d;
    uchar alnum = 0;		/* was last char an alphanumeric */
    bool premode;
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
	"#913\0Alpha",
	"#914\0Beta",
	"#915\0Gamma",
	"#916\0Delta",
	"#917\0Epsilon",
	"#918\0Zeta",
	"#919\0Eta",
	"#920\0Theta",
	"#921\0Iota",
	"#922\0Kappa",
	"#923\0Lambda",
	"#924\0Mu",
	"#925\0Nu",
	"#926\0Xi",
	"#927\0Omicron",
	"#928\0Pi",
	"#929\0Rho",
	"#931\0Sigma",
	"#932\0Tau",
	"#933\0Upsilon",
	"#934\0Phi",
	"#935\0Chi",
	"#936\0Psi",
	"#937\0Omega",
	"#945\0alpha",
	"#946\0beta",
	"#947\0gamma",
	"#948\0delta",
	"#949\0epsilon",
	"#950\0zeta",
	"#951\0eta",
	"#952\0theta",
	"#953\0iota",
	"#954\0kappa",
	"#955\0lambda",
	"#956\0mu",
	"#957\0nu",
	"#958\0xi",
	"#959\0omicron",
	"#960\0pi",
	"#961\0rho",
	"#962\0sigmaf",
	"#963\0sigma",
	"#964\0tau",
	"#965\0upsilon",
	"#966\0phi",
	"#967\0chi",
	"#968\0psi",
	"#969\0omega",
	"#177\0+-",
	"#8211\0-",
	"#8212\0 - ",
	"#8216\0`",
	"#8217\0'",
	"#8220\0`",
	"#8221\0'",
	"bull\0*",
	"#8226\0*",
	"#8230\0...",
	"#8242\0prime",
	"#8501\0aleph",
	"#8592\0left arrow",
	"#8593\0up arrow",
	"#8594\0arrow",
	"#8595\0down arrow",
	"#8660\0double arrow",
	"#8704\0every",
	"#8706\0d",
	"#8707\0some",
	"#8709\0empty set",
	"#8711\0del",
	"#8712\0member of",
	"#8713\0not a member of",
	"#8717\0such that",
	"#8721\0sum",
	"#8734\0infinity",
	"#8736\0angle",
	"#8745\0intersect",
	"#8746\0union",
	"#8747\0integral",
	"#8773\0congruent to",
	"#8800\0not equal",
	"#8804\0<=",
	"#8805\0>=",
	"#8834\0proper subset of",
	"#8835\0proper superset of",
	"#8836\0not a subset of",
	"#8838\0subset of",
	"#8839\0superset of",
	"#9658\0*",
	0
    };

    if(!s)
	return 0;
    if(s == EMPTYSTRING)
	return EMPTYSTRING;
    new = initString(&l);

    while(c = *s) {
	if(c == (uchar) InternalCodeChar && !invisible) {
	    const char *t = s + 1;
	    while(isdigitByte(*t))
		++t;
	    if(t > s + 1 && *t && strchr("{}<>*", *t)) {	/* it's a tag */
		bool separate, pretag, slash;
		n = atoi(s + 1);
		preFormatCheck(n, &pretag, &slash);
		separate = (*t != '*');
		if(separate)
		    alnum = 0;
		debugPrint(7, "tag %d%c separate %d", n, *t, separate);
		if(pretag)
		    premode = !slash;
		++t;
		stringAndBytes(&new, &l, s, t - s);
		s = t;
		continue;
	    }			/* tag */
	}
	/* code */
	j = 1;
	if(c != '&')
	    goto putc;

	for(j = 0; j < sizeof (andbuf); ++j) {
	    d = s[j + 1];
	    if(d == '&' || d == ';' || d <= ' ')
		break;
	}
	if(j == sizeof (andbuf))
	    goto putc;		/* too long, no match */
	strncpy(andbuf, s + 1, j);
	andbuf[j] = 0;
	++j;
	if(s[j] == ';')
	    ++j;
/* remove leading zeros */
	if(andbuf[0] == '#')
	    while(andbuf[1] == '0')
		strcpy(andbuf + 1, andbuf + 2);

      lookup:
	debugPrint(6, "meta %s", andbuf);
	n = stringInList(andwords, andbuf);
	if(n >= 0) {		/* match */
	    const char *r = andwords[n] + strlen(andwords[n]) + 1;	/* replacement string */
	    s += j;
	    if(!r[1]) {		/* replace with a single character */
		c = *r;
		--s;
		goto putc;
	    }
	    if(invisible) {
		s -= j;
		goto putc;
	    }
/* We're replacing with a word */
	    if(!invisible && isalnumByte(*r)) {
/* insert spaces either side */
		if(alnum)
		    stringAndChar(&new, &l, ' ');
		alnum = 2;
	    } else
		alnum = 0;
	    stringAndString(&new, &l, r);
	    continue;
	}
	/* match */
	if(andbuf[0] != '#')
	    goto putc;
	n = stringIsNum(andbuf + 1);
	if(n < 0)
	    goto putc;
	if(n > 255)
	    goto putc;
	c = n;
/* don't allow nulls */
	if(c == 0)
	    c = ' ';
	if(strchr("\r\n\f", c) && !premode)
	    c = ' ';
	if(c == (uchar) InternalCodeChar)
	    c = ' ';
	s += j - 1;
	j = 1;

      putc:
	if(isalnumByte(c)) {
	    if(alnum == 2)
		stringAndChar(&new, &l, ' ');
	    alnum = 1;
	} else
	    alnum = 0;
	stringAndChar(&new, &l, c);
	++s;
    }				/* loop over input string */

    return new;
}				/* andTranslate */
