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

static void cellDelimiters(char *buf)
{
	char *lastcell = 0;
	int cellcount = 0;
	char *s;

	for (s = buf; *s; ++s) {
		if (*s == TableCellChar) {
			*s = '|';
			lastcell = s;
			++cellcount;
			continue;
		}
		if (!strchr("\f\r\n", *s))
			continue;
/* newline here, if just one cell delimiter then blank it out */
		if (cellcount == 1)
			*lastcell = ' ';
		cellcount = 0;
	}
}				/* cellDelimiters */

static void anchorSwap(char *buf)
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
	debugPrint(4, "anchorSwap %d", cnt);

/* Framing characters like [] around an anchor are unnecessary here,
 * because we already frame it in braces.
 * Get rid of these characters, even in premode. */
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
		continue;
putc:
		*w++ = c;
	}			/* loop over buffer */
	*w = 0;
	debugPrint(4, "anchors unframed");

/* Now compress the implied linebreaks into one. */
	premode = false;
	ss = 0;
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
	debugPrint(4, "whitespace combined");
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

char *breakLineResult;
#define REFORMAT_EXTRA 4000

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

	nzFree(breakLineResult);
	breakLineResult = allocMem(len + REFORMAT_EXTRA);
	bl_start = bl_cursor = breakLineResult;
	bl_end = breakLineResult + len + REFORMAT_EXTRA - 8;
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

char *htmlReformat(char *buf)
{
	const char *h, *nh, *s;
	char c;
	bool premode = false;
	bool pretag, slash;
	char *new;
	int l, tagno;

	cellDelimiters(buf);

	anchorSwap(buf);

	longcut = lperiod = lcomma = lright = lany = 0;
	colno = 1;
	pre_cr = 0;
	lspace = 3;

	l = strlen(buf);
/* Only a pathological web page gets longer after reformatting.
 * Even then it isn't by much. This is a bit of a kludge.
 * If you still overflow, even beyond the EXTRA,
 * it won't seg fault, you'll just lose some text. */
	new = allocMem(l + REFORMAT_EXTRA);
	bl_start = bl_cursor = new;
	bl_end = new + l + REFORMAT_EXTRA - 20;
	bl_overflow = false;

	for (h = buf; (c = *h); h = nh) {
		if (isspaceByte(c)) {
			for (s = h + 1; isspaceByte(*s); ++s) ;
			nh = s;
			appendSpaceChunk(h, nh - h, premode);
			if (lspace == 3) {
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
	}			/* loop over text */

/* close off the last line */
	if (lspace < 2)
		appendSpaceChunk("\n", 1, true);
	*bl_cursor = 0;
	l = bl_cursor - bl_start;
/* Get rid of last space. */
	if (l >= 2 && new[l - 1] == '\n' && new[l - 2] == ' ')
		new[l - 2] = '\n', new[--l] = 0;
/* Don't need empty lines at the end. */
	while (l > 1 && new[l - 1] == '\n' && new[l - 2] == '\n')
		--l;
	new[l] = 0;
/* Don't allow an empty buffer */
	if (!l)
		new[0] = '\n', new[1] = 0, l = 1;

	if (bl_overflow) {
/* we should print a more helpful error message here */
		strcpy(new + l, "\n???");
		l += 4;
	}

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

bool parseDataURI(const char *uri, char **mediatype, char **data, int *data_l)
{
	bool base64 = false;
	const char *mediatype_start;
	const char *data_sep;
	const char *cp;
	size_t encoded_len;

	*data = *mediatype = emptyString;
	*data_l = 0;

	if (!isDataURI(uri))
		return false;

	mediatype_start = uri + 5;
	data_sep = strchr(mediatype_start, ',');

	if (!data_sep)
		return false;

	for (cp = data_sep - 1; (cp >= mediatype_start && *cp != ';'); cp--) ;

	if (cp >= mediatype_start && memEqualCI(cp, ";base64,", 8)) {
		base64 = true;
		*mediatype = pullString1(mediatype_start, cp);
	} else {
		*mediatype = pullString1(mediatype_start, data_sep);
	}

	encoded_len = strlen(data_sep + 1);
	*data = pullString(data_sep + 1, encoded_len);
	unpercentString(*data);

	if (!base64) {
		*data_l = strlen(*data);
	} else {
		char *data_end = *data + strlen(*data);
		int unpack_ret = base64Decode(*data, &data_end);
		if (unpack_ret != GOOD_BASE64_DECODE) {
			nzFree(*mediatype);
			*mediatype = emptyString;
			nzFree(*data);
			*data = emptyString;
			return false;
		}
		*data_end = '\0';
		*data_l = data_end - *data;
	}

	return true;
}				/* parseDataURI */
