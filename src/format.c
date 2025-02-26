// format.c, Format text, establish line breaks, manage whitespace,
// iso8859, utf8, utf16, utf32, base64, color codes,
// emojis, messages in your local language, edbrowse variables.

#include "eb.h"

// It's possible that console has to be utf8, or edbrowse won't work properly.
bool cons_utf8, iuConvert = true;
char type8859 = 1;
bool helpMessagesOn;

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
	bool ab = false;

	for (i = j = 0; i < h_len; ++i) {
		if (h[i] == 0)
			h[i] = ' ';
		if (h[i] == '\b') {
			if (i && !strchr("\n\b<>'\"&", h[i - 1]))
				--j;
			continue;
		}
		if (h[i] == InternalCodeChar) {
			h[i] = InternalCodeCharAlternate;
			ab = true;
		}
		h[j++] = h[i];
	}
	h[j] = 0;		/* now it's a string */
	if(ab) debugPrint(1, "changing ^b to ^a");

/* undos the file */
	for (i = j = 0; h[i]; ++i) {
		if (h[i] == '\r' && h[i + 1] == '\n')
			continue;
		h[j++] = h[i];
	}
	h[j] = 0;
}

/* An input field cannot contain cr, lf, null, or the InternalCodeChar */
void prepareForField(char *h)
{
	bool ab = false;
	while (*h) {
// this line is goofy cause null would terminate the loop
		if (*h == 0) *h = ' ';
		if (*h == '\n') *h = '\07'; // should never happen
		if (*h == InternalCodeChar) { *h = InternalCodeCharAlternate; ab = true; }
		++h;
	}
	if(ab) debugPrint(1, "changing ^b to ^a");
}

/*********************************************************************
The characters 03 and 04 delimite the cells of a table, or data, respectively.
We often don't know, so fall back to table.
And tables are often used to format a page, not a real table.  Ugh!
This routine turns either of these into |
However if there is one such | on a line, and it is a table cell marker,
we remove it. Most of the time the table is page layout,
and the | would only confuse things.
Remove whitespace before or after <td>, as tidy does.
*********************************************************************/

static void cellDelimiters(char *buf)
{
	char *lastcell = 0;
	int cellcount = 0;
	char *s, *t;

	for (s = t = buf; *s; ++s) {
		int n;
		char *u;
		if(*s != DataCellChar && *s != TableCellChar) {
			*t++ = *s;
			continue;
		}
// spaces behind
		while(t > buf && t[-1] == ' ') --t;
		*t++ = *s; // cell marker
// spaces ahead
respace:
		if(s[1] == ' ') { ++s; goto respace; }
// tidy turns <td> <i> hello </i> </td> into <td><i>hello</i></td>
// But not so with <p> or other strong tags.
// I try to do the same.
		if(s[1] != InternalCodeChar || !isdigitByte(s[2])) continue;
			n = strtol(s + 2, &u, 10);
// leave input fields alone
			if(*u != '*' && *u != '{') continue;
			if(tagList[n]->info->para & 3) continue;
// looks like a soft tag
			memcpy(t, s + 1, u - s);
			t += u - s;
			s = u;
			goto respace;
	}
	*t = 0;

	for (s = buf; *s; ++s) {
		if (*s == DataCellChar) {
			*s = '|';
			continue;
		}
		if (*s == TableCellChar) {
			*s = '|';
			lastcell = s;
			++cellcount;
			continue;
		}
		if (!strchr("\f\r\n", *s))
			continue;
// newline here, if just one cell delimiter then blank it out
		if (cellcount == 1)
			*lastcell = ' ';
		cellcount = 0;
	}
}

/*********************************************************************
this is transliteration, like /bin/tr
If the output char is x, it goes away, like tr -d
Some characters, like unicode A0, turn into space,
so we need to do this now, before swapping anchors and whitespace.
Watch out for utf8 - don't translate the a0 in c3a0.  That is a grave.
But a0 by itself is breakspace; turn it into space.
And c2a0 is utf8 breakspace.
99.9% of the time we are in utf8 mode.
I strip out the private utf8 codes.
Those should never be used on a public website.
If they are used by your private work website, and you need to retain them,
export EB_PUA=on
Don't do any of these transliterations in an input field.
Those must be exactly preserved, obviously.
*********************************************************************/

static void html_tr(char *buf)
{
	char *s, *ss, *w, c, d;
	int n;
	static const char from[] = "\x1b\x95\x99\x9c\x9d\x91\x92\x93\x94\xa0\xad\x96\x97\x85";
	static const char becomes[] = "_*'`'`'`' x---";
	bool inputmode = false;
	char *pua = getenv("EB_PUA");
	if(pua && !*pua) pua = 0;

	for (s = w = buf; (c = *s); ++s) {
		d = s[1];
		if (c == InternalCodeChar && isdigitByte(d)) {
			int tagno = strtol(s + 1, &ss, 10);
			if (*ss == '<' && !stringEqual(tagList[tagno]->info->name, "button"))
				inputmode = true;
			if (*ss == '>')
				inputmode = false;
			++ss;
			n = ss - s;
			memmove(w, s, n);
			w += n;
			s = ss - 1;
			continue;
		}

		if (inputmode)
			goto put1;

// utf8 test
		if ((c & 0xc0) == 0xc0 && (d & 0xc0) == 0x80) {
			unsigned int uni = 0, ubytes = 0;
			if ((c & 0x3c) == 0) {
/* fits in 8 bits */
				uni = ((uchar) c << 6) | (d & 0x3f);
				ss = strchr(from, (char)uni);
				if (ss) {
					c = becomes[ss - from];
					++s;
					if(c == 'x')
						continue;
					goto put1;
				}
			}
// copy the utf8 sequence as is
			uni = 0;
			*w++ = c;
			++s;
			c = (char) ((uchar)c << 1);
			while ((c & 0x80) && ((d = *s) & 0xc0) == 0x80) {
				*w++ = d, ++s, ++ubytes;
				uni = (uni << 6) | (d & 0x3f);
				c = (char) ((uchar)c << 1);
			}
			--s;
				c = (char) ((uchar)c >> (1+ubytes));
				uni |= ((unsigned int)c << (ubytes*6));
// We can do things with high unicodes here, if we wish.
// Suppresse private unicodes, which shouldn't appear on public websites,
// and if they do, there isn't a consistent way to read them.
			if(((uni >= 0xe000 && uni < 0xf8ff) ||
			(uni >= 0xf0000 && uni < 0xffffd) ||
			(uni >= 0x100000 && uni < 0x10fffd)) &&
			!pua)
				w -= ubytes+1;
			continue;
		}

// Now assuming iso8859-1, which is practically deprecated
		ss = strchr(from, c);
		if (ss) {
			c = becomes[ss - from];
			if(c == 'x')
				continue;
		}

put1:
		*w++ = c;
	}
	*w = 0;
}

// For <h3><div>hello</div></h3> which should not happen, but does
// And for <blockquote><p> which is prefectly fine
static void h3div(char *buf)
{
	char *s, *t;
	bool in_h = false;
	for(s = buf; *s; ++s) {
		if(*s == '\f' && s[1] == 'h' &&
		(uchar)s[2] >= '1' && (uchar)s[2] <= '6' && s[3] == ' ') {
			in_h = true, s += 3;
			continue;
		}
		if(isspaceByte(*s)) {
			if(in_h) *s = ' ';
		} else in_h = false;
	}
	in_h = false;
	for(s = t = buf; *s; ++s) {
		if(*s == '\f' && s[1] == '`' && s[2] == '`'
		&& isspaceByte(s[3])) {
			memcpy(t, s, 3);
			in_h = true, s += 3, t += 3;
			continue;
		}
		if(isspaceByte(*s)) {
			if(in_h) continue;
		} else in_h = false;
		*t++ = *s;
	}
	*t = 0;
}

/*********************************************************************
The primary goal of this routine is to turn
Hey,{ click here } for more information
into
Hey, {click here}  for more information
But of course we won't do that if the section is preformatted.
Nor can we muck with the whitespace that might be present in an input field <>.
Also swap 32* whitespace, pushing invisible anchors forward.
All this swapping preserves the length of the string.
If a change is made, the procedure is run again,
kinda like bubble sort.
It has the potential to be terribly inefficient,
but that doesn't seem to happen in practice.
Use cnt to count the iterations, for debugging purposes.
| is considered a whitespace character. Why is that?
Html tables are mostly used for visual layout, but sometimes not.
I use | to separate the cells of a table, but if there's nothing in them,
or at least no text, then I get rid of the pipes.
But every cell is going to have an invisible anchor from <td>, so that js can,
perhaps, set innerHTML inside this cell.
So there's something there, but nothing there.
I push these tags past the pipes, so I can clear it all away.
*********************************************************************/

static void anchorSwap(char *buf)
{
	char c, d, *s, *ss, *w, *a;
	bool pretag;		// <pre>
	bool premode;		// inside <pre> </pre>
	bool slash;		// closing tag
	bool change;		// made a swap somewhere
	bool strong;		// strong whitespace, newline or paragraph
	int n = 0, cnt, tagno = 0;
	char tag[20];

	cnt = 0;
	change = true;
	while (change) {
		change = false;
		++cnt;
		premode = false;
// w represents the state of whitespace
		w = NULL;
// a points to the prior anchor, which is swappable with following whitespace
		a = NULL;

		for (s = buf; (c = *s); ++s) {
			if (isspaceByte(c) || (c == '|' && !premode)) {
#if 0
				if (c == '\t' && !premode)
					*s = ' ';
#endif
				if (!w)
					w = s;
				continue;
			}

// end of white space, should we swap it with prior tag?
			if (w && a) {
				const Tag *t = tagList[tagno];
				const char *q;
// don't move td past newline; it screws things up.
				if(t->action != TAGACT_TD) {
tagforward:
					memmove(a, w, s - w);
					memmove(a + (s - w), tag, n);
					change = true;
					w = NULL;
					goto afterforward;
				}
// It's ok to move things around, unless it's a data table.
				if(tableType(t) != 1) goto tagforward;
				for(q = w; q < s; ++q)
					if(*q != ' ') goto afterforward;
				goto tagforward;
			}
afterforward:

// prior anchor has no significance
			a = NULL;

			if (c != InternalCodeChar)
				goto normalChar;
// some conditions that should never happen
			if (!isdigitByte(s[1]))
				goto normalChar;
			tagno = strtol(s + 1, &ss, 10);
			preFormatCheck(tagno, &pretag, &slash);
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
			if(d == '<' && stringEqual(tagList[tagno]->info->name, "button"))
				a = s;
			s = ss;

normalChar:
			w = 0;	/* no more whitespace */
		}
	}
	debugPrint(4, "anchorSwap %d", cnt);
}

/* Framing characters like [] around an anchor are unnecessary here,
 * because we already frame it in braces.
 * Get rid of these characters, even in premode. */
static void anchorUnframe(char *buf)
{
	char c, d, *s, *w, *a;
	int n;

	for (s = w = buf; (c = *s); ++s) {
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
				break;	// should never happen
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
	}			// loop over buffer
	*w = 0;
	debugPrint(4, "anchors unframed");
}

// Now compress the implied linebreaks into one.
static void html_ws(char *buf)
{
	char c, d, *s, *w, *w2, *a;
	int n;
	bool premode = false, pretag, strong, slash;
	char *ss = 0;

	for (s = buf; (c = *s); ++s) {

// look for <pre>
		if (c == InternalCodeChar && isdigitByte(s[1])) {
			n = strtol(s + 1, &s, 10);
			if (*s == '*') {
				preFormatCheck(n, &pretag, &slash);
				if (pretag) premode = !slash;
			}
		}

		if (!isspaceByte(c)) continue;

// whitespace region starts here.
// strong is anything harder than space, like <br> or <p> or <div>
		strong = false;
// watch for pipes inside whitespace, these are usually cell delimiters.
// pipe ends a whitespace region if only spaces come before
		for (w = s; isspaceByte(*w) || (*w == '|' && !premode); ++w) {
			if(*w == '|') {
				if(strong) continue;
				break;
			}
			if(*w != ' ') strong = true;
		}

// whitespace region from s up to w
// pipes have to be internal to whitespace to be moved;
// pipes at the end are left alone.
// Look ahead past anchors
		w2 = w;
		while(*w2 == InternalCodeChar) {
			n = strtol(w2 + 1, &w2, 10);
			++w2;
		}
		if(*w2)
			while(w[-1] == '|') --w;

// move internal pipes to the front
		for(ss = --w; w >= s; --w)
			if(*w != '|') *ss-- = *w;
		while(ss >= s) *ss-- = '|';
		while(*s == '|') ++s;

		strong = false, a = 0;
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
	}			// loop over buffer
	debugPrint(4, "whitespace combined");

/* Due to the anchor swap, the buffer could end in whitespace
 * followed by several anchors. Trim these off. */
	s = buf + strlen(buf);
	while (s > buf + 1 && s[-1] == '*' && isdigitByte(s[-2])) {
		for (w = s - 3; w >= buf && isdigitByte(*w); --w) ;
		if (w < buf || *w != InternalCodeChar)
			break;
		s = w;
	}
	*s = 0;

/*********************************************************************
Some hyperlinks are multiline, due to some html inside, and our interpretation
of said html. This is just annoying, so pull it back down to one line.
Same goes for <button>, but other input fields must remain as they are.
Even submit, as shown by jsrt, if you submit the form it says b1=Send%20Message
hence it would send a newline if there was one.
*********************************************************************/

	for (s = buf; (c = *s); ++s) {
		if (c != InternalCodeChar)
			continue;
		n = strtol(s + 1, &s, 10);
		if (*s == '<') {
			if (!stringEqual(tagList[n]->info->name, "button"))
				continue;
		} else if (*s != '{')
			continue;
		for (a = s + 1; (c = *a); ++a) {
			if (c == InternalCodeChar && a[1] == '0')
				break;
			if (c == '\n' || c == '\f')
				*a = ' ';
		}
		s = a;
	}
}

/*********************************************************************
Format text, and break lines at sentence/phrase boundaries.
The prefix bl means breakline.
*********************************************************************/

static char *bl_start, *bl_cursor, *bl_end;
static bool bl_overflow;
/* This is a virtual column number, extra spaces for tab,
 * one space for emoji, and skipping over invisible anchors. */
static int colno;
int formatLineLength = 80;	// for html formatting or the bl command
bool formatOverflow;
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
	FILE *f;
	if (debugLevel < 7)
		return;
	f = debugFile ? debugFile : stdout;
	fprintf(f, "chunk<");
	for (i = 0; i < len; ++i) {
		char c = chunk[i];
		if (c == '\t') {
			fprintf(f, "\\t");
			continue;
		}
		if (c == '\n') {
			fprintf(f, "\\n");
			continue;
		}
		if (c == '\f') {
			fprintf(f, "\\f");
			continue;
		}
		if (c == '\r') {
			fprintf(f, "\\r");
			continue;
		}
		if (c == '\0') {
			fprintf(f, "\\0");
			continue;
		}
		fprintf(f, "%c", c);
	}
	fprintf(f, ">%d.%d\n", colno, lspace);
}

static void appendOneChar(char c)
{
	if (bl_cursor == bl_end)
		bl_overflow = true;
	else
		*bl_cursor++ = c;
}

static bool spacesInInput(void)
{
	char *t = bl_cursor;
	char c;
	for (--t; t >= bl_start; --t) {
		c = *t;
		if (c == '\n') return false;
		if (c == '>' && t >= bl_start + 2 &&
		    t[-1] == '0' && t[-2] == InternalCodeChar)
			return false;
		if (c != '<')
			continue;
		while (t > bl_start && isdigitByte(t[-1]))
			--t;
		if (*t == '<')
			continue;
		if (t > bl_start && t[-1] == InternalCodeChar)
			return true;
	}
	return false;
}

static void appendSpaceChunk(const char *chunk, int len, bool premode)
{
	int nlc = pre_cr;	// newline count
	int spc = 0;		// space count
	int i, j;
	char c, d, e;

	if (!len)
		return; // nothing to add

// spaces in input field are literal
	if(spacesInInput()) {
		for (i = 0; i < len; ++i) {
			c = chunk[i];
			if (c == '\n') // should never happen
				c = '\r';
			if (c == 0) // should never happen
				c = ' ';
			colno += (c == '\t' ? 4 : 1);
			appendOneChar(c);
		}
		return;
	}

// gather stats on the whitespace chunk
	for (i = 0; i < len; ++i) {
		c = chunk[i];
// a newline or an isolated return
		if (c == '\n' || (c == '\r' && (i == len || chunk[i+1] != '\n'))) {
			++nlc, spc = 0;
			continue;
		}
		if (c == '\f') {
			nlc += 2, spc = 0;
			continue;
		}
		++spc;
	}

	if (!premode) {
// was there a period or such just before this whitespace?
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
// Check for Mr. Mrs. and others
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
// Check for John C. Calhoon
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
		if (formatOverflow) {
// tack a short fragment onto the previous line
			if (longcut && colno <= 15 && (nlc || lperiod == colno)) {
				bl_start[longcut] = ' ';
				if (!nlc)
					len = spc = 0, nlc = 1;
			}	// pasting small fragment onto previous line
		}
	}

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
}

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
// each foreign char or emoji counts as one.
// Ignore all but the first byte of a utf8.
			if (!(c&0x80)	// ascii
			    || (c & 0x40) == 0x40)
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
	if (colno <= formatLineLength)
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
}

/* Break up a line using the above routines.
 * The new lines are put in a fixed array.
 * Return false (fail) if we ran out of room.
 * This function is called from buffers.c, implementing the bl command,
 * and is only in this file because it shares the above routines and variables
 * with the html reformatting, which really has to be here. */

char *breakLineResult;
#define REFORMAT_EXTRA 400

/* Count the formfeeds in a string. Each of these expands to \n\n,
 * making the string longer. */
static int formfeedCount(const char *buf, int len)
{
	int i, ff = 0;
	for (i = 0; i < len; ++i)
		if (buf[i] == '\f')
			++ff;
	return ff;
}

bool breakLine(const char *line, int len, int *newlen)
{
	char c, state, newstate;
	int i, last, extra;

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
	extra = REFORMAT_EXTRA + formfeedCount(line, len);
	breakLineResult = allocMem(len + extra);
	bl_start = bl_cursor = breakLineResult;
	bl_end = breakLineResult + len + extra - 8;
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
}

void breakLineSetup(void)
{
	lspace = 3;
}

// Return the number of unbalanced punctuation marks.
// This is used by the next routine.
static void unbalanced(char c, char d, int ln, int *back_p, int *for_p)
{
	char *t, *open;
	char *p = (char *)fetchLine(ln, 1);
	bool change;
	char qc;
	int backward, forward;

// line is allocated, so blank out anything that looks like a string.
	for(qc = 0, t = p; *t != '\n'; ++t) {
		if(qc) { // in a string
			if(*t == '\\' && t[1] != '\n') t[1] = ' ';
			if(*t == qc) qc = 0;
			*t = ' ';
			continue;
		}
		if(*t == '"' || *t == '\'') {
			qc = *t;
// If we don't have a closing quote then forget it.
			const char *u = t + 1;
			for(; *u != '\n'; ++u) {
				if(*u == qc) break; // found closing quote
				if(*u == '\\' && u[1] != '\n') ++u;
			}
			if(*u != qc) qc = 0;
		}
	}

	change = true;
	while (change) {
		change = false;
		open = 0;
		for (t = p; *t != '\n'; ++t) {
			if (*t == c)
				open = t;
			if (*t == d && open) {
				*open = 0;
				*t = 0;
				change = true;
				open = 0;
			}
		}
	}

	backward = forward = 0;
	for (t = p; *t != '\n'; ++t) {
		if (*t == c)
			++forward;
		if (*t == d)
			++backward;
	}

	free(p);
	*back_p = backward;
	*for_p = forward;
}

// Find the line that balances the unbalanced punctuation.
bool balanceLine(const char *line, int mark)
{
	char c, d;		/* open and close */
	char selected;
	static char openlist[] = "{([<";
	static char closelist[] = "})]>";
	static const char alllist[] = "{}()[]<>`'";
	char *t;
	int level = 0;
	int i, direction, forward, backward;

	if ((c = *line)) {
		const int cx = stringIsNum(line + 1);
		if (!strchr(alllist, c) || (line[1] && cx == -1)) {
			setError(MSG_BalanceChar, alllist);
			return false;
		}
		if (!cx)
			return true;
		if ((t = strchr(openlist, c))) {
			d = closelist[t - openlist];
			direction = 1;
		} else {
			d = c;
			t = strchr(closelist, d);
			c = openlist[t - closelist];
			direction = -1;
		}
		if (line[1])
			level = cx;
		else {
			unbalanced(c, d, mark, &backward, &forward);
			level = direction > 0 ? forward : backward;
			if (!level)
				level = 1;
		}
	} else {

// Look for anything unbalanced, probably a brace.
		for (i = 0; i <= 2; ++i) {
			c = openlist[i];
			d = closelist[i];
			unbalanced(c, d, mark, &backward, &forward);
			if (backward && forward) {
				setError(MSG_BalanceAmbig, c, d, c, d);
				return false;
			}
			level = backward + forward;
			if (!level)
				continue;
			direction = backward ? -1 : 1;
			break;
		}
		if (!level) {
			setError(MSG_BalanceNothing);
			return false;
		}
	}			/* explicit character passed in, or look for one */

	selected = (direction > 0 ? c : d);

/* search for the balancing line */
	i = mark;
	while ((i += direction) > 0 && i <= cw->dol) {
		unbalanced(c, d, i, &backward, &forward);
		if ((direction > 0 && backward >= level) ||
		    (direction < 0 && forward >= level)) {
			cw->dot = i;
			printDot();
			return true;
		}
		level += (forward - backward) * direction;
	}			/* loop over lines */

	setError(MSG_Unbalanced, selected);
	return false;
}

char *htmlReformat(char *buf)
{
	const char *h, *nh, *s;
	char c;
	bool premode = false;
	bool pretag, slash;
	char *new;
	int l, tagno, extra;
	char *fmark;		/* mark the start of a frame */

	if(debugLayout) printf("rendered<%s>\n", buf);
	cellDelimiters(buf);
	if(debugLayout) printf("cells<%s>\n", buf);
	html_tr(buf);
	if(debugLayout) printf("translate<%s>\n", buf);
	h3div(buf);
	if(debugLayout) printf("h3div<%s>\n", buf);
	anchorSwap(buf);
	if(debugLayout) printf("swap<%s>\n", buf);
	anchorUnframe(buf);
	if(debugLayout) printf("unframe<%s>\n", buf);
	html_ws(buf);
	if(debugLayout) printf("whitespace<%s>\n", buf);

	longcut = lperiod = lcomma = lright = lany = 0;
	colno = 1;
	pre_cr = 0;
	lspace = 3;

	l = strlen(buf);
/* Only a pathological web page gets longer after reformatting.
 * Those with paragraphs and nothing else to compress or remove.
 * Thus I allocate for the formfeeds, which correspond to paragraphs,
 * and are replaced with \n\n.
 * Plus some extra bytes for slop.
 * If you still overflow, even beyond the EXTRA,
 * it won't seg fault, you'll just lose some text. */
	extra = REFORMAT_EXTRA + formfeedCount(buf, l);
	new = allocMem(l + extra);
	bl_start = bl_cursor = new;
	bl_end = new + l + extra - 20;
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
		if (!c || !strchr("{}<>*", c)) {
// this should never happen!
			i_printf(MSG_BadTagCode, tagno, c);
			appendOneChar('@');
			nh = h + 1;
			continue;
		}
		appendPrintableChunk(h, nh - h, premode);
		preFormatCheck(tagno, &pretag, &slash);
		if (pretag) {
			premode = !slash;
			if (!premode) {
/* This forces a new paragraph, so it last char was nl, erase it. */
				char *w = bl_cursor - 1;
				while (*w != InternalCodeChar)
					--w;
				if (w > bl_start && w[-1] == '\n') {
					memmove(w - 1, w, bl_cursor - w);
					--bl_cursor;
				}
			}
		}
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

/* It's a little thing really, but the blank line at the top of each frame annoys me */
	fmark = new;
	while ((fmark = strstr(fmark + 1, "*`--\n\n"))) {
		if (isdigitByte(fmark[-1]))
			strmove(fmark + 5, fmark + 6);
	}

	return new;
}

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

	for (s = t = mark = line; (c = *s); ++s) {
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
	for (s = line; (c = *s); ++s)
		if (c == ' ')
			*s = ',';
	if (*line)
		strcat(line, ",");
}

static void cutDuplicateEmail(char *line, const char *dup, int duplen)
{
	char *s;
	while (*line) {
		s = strchr(line, ',');
		if (!s)
			return;	// should never happen
		if (duplen == s - line && memEqualCI(line, dup, duplen)) {
			++s;
			strmove(line, s);
			continue;
		}
		line = s + 1;
	}
}

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
			break;	// should never happen
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
			break;	// should never happen
		len = t - s;
		++t;
		cutDuplicateEmail(t, s, len);
		s = t;
	}

// If your email address is on the to or cc list, drop it.
// But retain it if it is the reply, in case you sent mail to yourself.
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
}

bool isEmailAddress(const char *s)
{
	bool atfound = false, dotfound = false;
	if (!s || !*s)
		return false;
	for (; *s; ++s) {
		char c = *s;
		if (c&0x80)	// nonascii
			return false;
		if (atfound) {
			if (!isalnumByte(c) && c != '.' && c != '-')
				return false;
			if (c == '.') {
				if (s[1] == '.' || s[1] == 0 || s[-1] == '.'
				    || s[-1] == '@')
					return false;
				dotfound = true;
			}
			continue;
		}
// I think anything is ok before the @, except space.
		if (c <= ' ')
			return false;
		if (c == '@')
			atfound = true;
	}
	return atfound & dotfound;
}

// Check for a list of email addresses separated by commas.
bool isEmailAddressList(char *s)
{
	if (!s || !*s)
		return false;
	char *e = strchr(s, ',');
	if (!e) return isEmailAddress(s);
	if (isEmailAddressList(e + 1)) {
		*e = 0;
		bool starts_well = isEmailAddress(s);
		*e = ',';
		return starts_well;
	}
	return false;
}

// return 1 for utf16, 2 for utf32, ored with 4 for big endian
int byteOrderMark(const uchar * buf, int buflen)
{
	if (buflen < 2)
		return 0;
	if (buf[0] == 0xfe && buf[1] == 0xff)
		return 5;
	if (buf[0] == 0xff && buf[1] == 0xfe) {
		if (buflen >= 4 && buf[2] == 0 && buf[3] == 0)
			return 2;
		return 1;
	}
	if (buflen >= 4 && !memcmp(buf, "\x0\x0\xfe\xff", 4))
		return 6;
	return 0;
}

/*********************************************************************
We got some data from a file or from the internet.
Count the binary characters and decide if this is, on the whole,
binary or text.  I allow some nonascii chars,
like you might see in Spanish or German, and still call it text,
but if there's too many such chars, I call it binary.
It's not an exact science.
utf8 sequences are considered text characters.
If there is a leading byte order mark as per the previous routine, it's text.
*********************************************************************/

bool looksBinary(const uchar * buf, int buflen)
{
	int i, j, bincount = 0, charcount = 0, nullcount = 0;
	uchar c;
	uchar seed;

	if (byteOrderMark(buf, buflen))
		return false;

	for (i = 0; i < buflen; ++i, ++charcount) {
		c = buf[i];
// 0 is ascii, but not really text, and very common in binary files.
		if (c == 0) {
			if (++nullcount >= 10)
				return true;
		}
		if (c < 0x80)
			continue;
// could represent a utf8 character
		seed = c;
		if ((seed & 0xfe) == 0xfe || (seed & 0xc0) == 0x80) {
binchar:
			++bincount;
			continue;
		}
		seed <<= 1;
		j = 1;
		while (seed & 0x80 && i + j < buflen
		       && (buf[i + j] & 0xc0) == 0x80)
			seed <<= 1, ++j;
		if (seed & 0x80)
			goto binchar;
// this is valid utf8 char, don't treat it as binary.
		i += j - 1;
	}

	return (bincount * 8 - 16 >= charcount);
}

void looks_8859_utf8(const uchar * buf, int buflen, bool * iso_p, bool * utf8_p)
{
	int utfcount = 0, isocount = 0;
	int i, j, bothcount;

	for (i = 0; i < buflen; ++i) {
		uchar c = buf[i];
		if (c < 0x80)
			continue;
/* This is the start of the nonascii sequence. */
/* No second bit, it has to be iso. */
		if (!(c & 0x40)) {
isogo:
			++isocount;
			continue;
		}
/* Next byte has to start with 10 to be utf8, else it's iso */
		if ((buf[i + 1] & 0xc0) != 0x80)
			goto isogo;
		c <<= 2;
		for (j = i + 2; c&0x80; ++j, c <<= 1)
			if ((buf[j] & 0xc0) != 0x80)
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
// a sequence of ascii, like times +- 7, can look like utf8.
// If 2/3 of the nonascii chars look like iso, I'll call it iso.
	if (isocount * 9 >= bothcount)
		*iso_p = true;
}

/*********************************************************************
Convert a string from iso 8859 to utf8, or vice versa.
In each case a new string is allocated.
Don't forget to free it when you're done.
*********************************************************************/

/* only 8859-1 and 8859-2 so far */
static const unsigned int iso_unicodes[2][128] = {
	{
/*********************************************************************
The first 32 nonascii chars in iso8859-1 are symbols,
and almost never used.
Much more common are the cp1252 characters, introduced by Microsoft.
I'm gonna go with those, and hope I'm right more often than wrong.
There should be a switch for this.
*********************************************************************/
#define CP1252 1
#if CP1252
	 0x20AC, 0x81, 0x201A, 0x192, 0x201E, 0x2026, 0x2020, 0x2021,
	 0x2C6, 0x2030, 0x160, 0x2039, 0x152, 0x8d, 0x17D, 0x8f,
	 0x90, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
	 0x2DC, 0x2122, 0x161, 0x203A, 0x153, 0x9d, 0x17E, 0x178,
#else
	 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b,
	 0x8c, 0x8d, 0x8e, 0x8f,
	 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a,
	 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
#endif
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

void iso2utf(const uchar * inbuf, int inbuflen, uchar ** outbuf_p,
	     int *outbuflen_p)
{
	int i, j;
	int nacount = 0;
	uchar c;
	uchar *outbuf;
	const unsigned int *isoarray = iso_unicodes[type8859 - 1];
	int ucode;
	char *s;

	if (!inbuflen) {
		*outbuf_p = (uchar *) emptyString;
		*outbuflen_p = 0;
		return;
	}

/* count chars, so we can allocate */
	for (i = 0; i < inbuflen; ++i) {
		c = inbuf[i];
		if (c >= 0x80) {
			ucode = isoarray[c & 0x7f];
			s = uni2utf8(ucode);
			nacount += strlen(s) - 1;
		}
	}

	outbuf = allocMem(inbuflen + nacount + 1);

	for (i = j = 0; i < inbuflen; ++i) {
		c = inbuf[i];
		if (c < 0x80) {
			outbuf[j++] = c;
			continue;
		}
		ucode = isoarray[c & 0x7f];
		s = uni2utf8(ucode);
		strcpy((char *)outbuf + j, s);
		j += strlen(s);
	}
	outbuf[j] = 0;

	*outbuf_p = outbuf;
	*outbuflen_p = j;
}

void utf2iso(const uchar * inbuf, int inbuflen, uchar ** outbuf_p,
	     int *outbuflen_p)
{
	int i, j, k;
	uchar c;
	uchar *outbuf;
	const unsigned int *isoarray = iso_unicodes[type8859 - 1];
	unsigned int ucode;

	if (!inbuflen) {
		*outbuf_p = (uchar *) emptyString;
		*outbuflen_p = 0;
		return;
	}

	outbuf = allocMem(inbuflen + 1);
	for (i = j = 0; i < inbuflen; ++i) {
		c = inbuf[i];

/* regular chars and nonascii chars that aren't utf8 pass through. */
/* There shouldn't be any of the latter */
		if ((c & 0xc0) != 0xc0) {
			outbuf[j++] = c;
			continue;
		}

/* Convertable into 11 bit */
		if ((c & 0xe0) == 0xc0 && (inbuf[i + 1] & 0xc0) == 0x80) {
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

/* Convertable into 16 bit */
		if ((c & 0xf0) == 0xe0 &&
		    (inbuf[i + 1] & 0xc0) == 0x80 &&
		    (inbuf[i + 2] & 0xc0) == 0x80) {
			ucode = c & 0xf;
			ucode <<= 6;
			ucode |= (inbuf[i + 1] & 0x3f);
			ucode <<= 6;
			ucode |= (inbuf[i + 2] & 0x3f);
			for (k = 0; k < 128; ++k)
				if (isoarray[k] == ucode)
					break;
			if (k < 128) {
				outbuf[j++] = k | 0x80;
				i += 2;
				continue;
			}
		}

/* unicodes not found in our iso class are converted into stars */
		c <<= 1;
		++i;
		for (++i; c&0x80; ++i, c <<= 1) {
			if ((outbuf[i] & 0xc0) != 0x80)
				break;
		}
		outbuf[j++] = '*';
		--i;
	}
	outbuf[j] = 0;

	*outbuf_p = outbuf;
	*outbuflen_p = j;
}

// like the above, but always iso8859-1, and the change is made inline
void utf2iso1(char *s0, size_t *lenp)
{
	char *s = s0;
	char *t = s0;
// null pointer means we don't specify a length
	while((!lenp && *s) || (lenp && (unsigned)(s - s0) < *lenp)) {
		uchar c = *s;
		uchar d = s[1];
		if((c&0xe0) == 0xe0 || (c&0xe0) == 0x80 ||
		((c&0x80) && (d&0xc0) != 0x80)) {
			debugPrint(1, "improper utf8 format in payload");
			break;
		}
		if(c >= 0xc4) {
			debugPrint(1, "null or high character in payload");
			break;
		}
		if(c < 0x80) { *t++ = c; ++s; continue; }
		*t++ = ((d&0x3f) | (c<<6));
		s += 2;
	}
	*t = 0;
	if(lenp) *lenp = t - s0;
}

// reverse the above. This time the new string is allocated, as it might be longer
char *iso12utf(const char *t1, const char *t2, int *lenp)
{
	int l;
	char *s = initString(&l);
	while(t1 < t2) {
		uchar c = (uchar)*t1;
		if(!(c&0x80)) { // ascii
			stringAndChar(&s, &l, c);
		} else {
			stringAndChar(&s, &l, (0xc2 | ((c>>6)&1)));
			stringAndChar(&s, &l, (0x80|(c&0x3f)));
		}
		++t1;
	}
	*lenp = l;
	return s;
}

/*********************************************************************
Convert the current line in buffer, which is either iso8859-1 or utf8,
into utf16 or utf32, big or little endian.
The returned string is allocated, though not really a string,
since it will contain nulls, plenty of them in the case of utf32.
*********************************************************************/

void utfHigh(const char *inbuf, int inbuflen, char **outbuf_p, int *outbuflen_p,
	     bool inutf8, bool out32, bool outbig)
{
	uchar *outbuf;
	unsigned int unicode;
	uchar c;
	int i, j;

	if (!inbuflen) {
		*outbuf_p = emptyString;
		*outbuflen_p = 0;
		return;
	}

// worst case is utf32
	outbuf = allocMem(inbuflen * 4);

	i = j = 0;
	while (i < inbuflen) {
		c = (uchar) inbuf[i];
		if (!inutf8 || ((c & 0xc0) != 0xc0 && (c & 0xfe) != 0xfe)) {
			unicode = c;	// that was easy
			++i;
		} else {
			uchar mask = 0x20;
			int k = 1;
			++i;
			while (c & mask)
				++k, mask >>= 1;
			c &= (mask - 1);
			unicode = ((unsigned int)c) << (6 * k);
			while (i < inbuflen && k) {
				c = (uchar) inbuf[i];
				if ((c & 0xc0) != 0x80)
					break;
				++i, --k;
				c &= 0x3f;
				unicode |= (((unsigned int)c) << (6 * k));
			}
		}

		if (out32) {
			if (outbig) {
				outbuf[j++] = ((unicode >> 24) & 0xff);
				outbuf[j++] = ((unicode >> 16) & 0xff);
				outbuf[j++] = ((unicode >> 8) & 0xff);
				outbuf[j++] = (unicode & 0xff);
			} else {
				outbuf[j++] = (unicode & 0xff);
				outbuf[j++] = ((unicode >> 8) & 0xff);
				outbuf[j++] = ((unicode >> 16) & 0xff);
				outbuf[j++] = ((unicode >> 24) & 0xff);
			}
			continue;
		}

// utf16, a bit trickier but not too bad.
		if (unicode <= 0xd7ff
		    || (unicode >= 0xe000 && unicode <= 0xffff)) {
			if (outbig) {
				outbuf[j++] = ((unicode >> 8) & 0xff);
				outbuf[j++] = (unicode & 0xff);
			} else {
				outbuf[j++] = (unicode & 0xff);
				outbuf[j++] = ((unicode >> 8) & 0xff);
			}
			continue;
		}

		if (unicode >= 0x10000 && unicode <= 0x10ffff) {
// surrogate pairs
			unsigned int pair1, pair2;
			unicode -= 0x10000;
			pair1 = 0xd800 + ((unicode >> 10) & 0x3ff);
			pair2 = 0xdc00 + (unicode & 0x3ff);
			if (outbig) {
				outbuf[j++] = ((pair1 >> 8) & 0xff);
				outbuf[j++] = (pair1 & 0xff);
				outbuf[j++] = ((pair2 >> 8) & 0xff);
				outbuf[j++] = (pair2 & 0xff);
			} else {
				outbuf[j++] = (pair1 & 0xff);
				outbuf[j++] = ((pair1 >> 8) & 0xff);
				outbuf[j++] = (pair2 & 0xff);
				outbuf[j++] = ((pair2 >> 8) & 0xff);
			}
			continue;
		}

	}

	*outbuf_p = (char *)outbuf;
	*outbuflen_p = j;
}

// convert a 32 bit unicode character into utf8
char *uni2utf8(unsigned int unichar)
{
	static uchar outbuf[12];
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
	return (char *)outbuf;
}

void utfLow(const char *inbuf, int inbuflen, char **outbuf_p, int *outbuflen_p,
	    int bom)
{
	char *obuf;
	int obuf_l;
	unsigned int unicode;
	int isbig;
	int k, l;
	const unsigned int *isoarray = iso_unicodes[type8859 - 1];

	if (!inbuflen) {
		*outbuf_p = emptyString;
		*outbuflen_p = 0;
		return;
	}

	obuf = initString(&obuf_l);
	isbig = (bom & 4);
	bom &= 3;
	l = bom * 2;		// skip past byte order mark

	while (l < inbuflen) {
		if (bom == 2) {
			if (l + 4 > inbuflen) {
				unicode = '?';
				l = inbuflen;
			} else if (isbig) {
				unicode = (uchar) inbuf[l];
				unicode <<= 8;
				unicode |= (uchar) inbuf[l + 1];
				unicode <<= 8;
				unicode |= (uchar) inbuf[l + 2];
				unicode <<= 8;
				unicode |= (uchar) inbuf[l + 3];
				l += 4;
			} else {
				unicode = (uchar) inbuf[l + 3];
				unicode <<= 8;
				unicode |= (uchar) inbuf[l + 2];
				unicode <<= 8;
				unicode |= (uchar) inbuf[l + 1];
				unicode <<= 8;
				unicode |= (uchar) inbuf[l];
				l += 4;
			}
		} else {
			if (l + 2 > inbuflen) {
				unicode = '?';
				l = inbuflen;
			} else if (isbig) {
				unicode = (uchar) inbuf[l];
				unicode <<= 8;
				unicode |= (uchar) inbuf[l + 1];
				l += 2;
			} else {
				unicode = (uchar) inbuf[l + 1];
				unicode <<= 8;
				unicode |= (uchar) inbuf[l];
				l += 2;
			}
			if (unicode >= 0xd800 && unicode <= 0xdbff
			    && l + 2 <= inbuflen) {
				unsigned int pair1, pair2;
				pair1 = unicode - 0xd800;
				if (isbig) {
					pair2 = (uchar) inbuf[l];
					pair2 <<= 8;
					pair2 |= (uchar) inbuf[l + 1];
				} else {
					pair2 = (uchar) inbuf[l + 1];
					pair2 <<= 8;
					pair2 |= (uchar) inbuf[l];
				}
				if (pair2 >= 0xdc00 && pair2 <= 0xdfff) {
					pair2 -= 0xdc00;
					l += 2;
					unicode = pair1;
					unicode <<= 10;
					unicode |= pair2;
				}
			}
		}

// ok we got the unicode.
// It now becomes utf8 or iso8859-x
		if (cons_utf8) {
			stringAndString(&obuf, &obuf_l, uni2utf8(unicode));
			continue;
		}
// iso8859-x here, practically deprecated
		if (unicode <= 127) {	// ascii
			stringAndChar(&obuf, &obuf_l, (char)unicode);
			continue;
		}

		for (k = 0; k < 128; ++k)
			if (isoarray[k] == unicode)
				break;
		if (k < 128)
			unicode = k | 0x80;
		else
			unicode = '?';
		stringAndChar(&obuf, &obuf_l, (char)unicode);
	}

// The input string is a file or url and has 2 extra bytes after it.
// After reformatting it should still have two extra bytes after it.
	stringAndString(&obuf, &obuf_l, "  ");

	*outbuf_p = obuf;
	*outbuflen_p = obuf_l - 2;
}

// Determine type of file, utf 8 or 16 or 32, or dos mode, then convert to utf8,
// or perhaps iso8859 for as long as we choose to support that obsolete format.
// Show messages if appropriate.
// Convert text and display messages if appropriate.
// Size is passed by reference so that the new size is available
// to the calling routine.
// rbuf is passed by reference because some of the conversion routines
// create a new allocated array which must be passed back.
// This is called by readFile in buffers.c, so has some
void diagnoseAndConvert (char **rbuf_p, bool *isAllocated_p, int *partSize_p, const bool firstPart, const bool showMessage)
{
	if (!iuConvert) return;

	char *rbuf = *rbuf_p;
	char *tbuf;
	int i, j;
	bool crlf_yes = false, crlf_no = false, dosmode = false;
	bool is8859 = false, isutf8 = false;
	int bom = 0;
// Classify this incoming text as ascii or 8859 or utf-x
	if(firstPart) bom = byteOrderMark((uchar *) rbuf, (int)fileSize);

	if (bom) {
// bom implies not reading by parts, so don't worry about that any more.
		debugPrint(3, "text type is %s%s",
			   ((bom & 4) ? "big " : ""),
			   ((bom & 2) ? "utf32" : "utf16"));
		if (debugLevel >= 2 || (debugLevel == 1 && showMessage))
			i_puts(cons_utf8 ? MSG_ConvUtf8 :        MSG_Conv8859);
		utfLow(rbuf, *partSize_p, &tbuf, &*partSize_p, bom);
		if(*isAllocated_p) nzFree(rbuf);
		*isAllocated_p = true;
		*rbuf_p = rbuf = tbuf;
		fileSize = *partSize_p;
	} else {
		int oldSize = *partSize_p;
		looks_8859_utf8((uchar *) rbuf, *partSize_p,
				&is8859, &isutf8);
		if(firstPart)
		debugPrint(3, "text type is %s",
			   (isutf8 ? "utf8"
			    : (is8859 ? "8859" : "ascii")));
		if (cons_utf8 && is8859) {
			if ((debugLevel >= 2 || (debugLevel == 1 && showMessage))
			&& firstPart)
				i_puts(MSG_ConvUtf8);
			iso2utf((uchar *) rbuf, *partSize_p,
				(uchar **) & tbuf, &*partSize_p);
			if(*isAllocated_p) nzFree(rbuf);
			*isAllocated_p = true;
			*rbuf_p = rbuf = tbuf;
			fileSize += (*partSize_p - oldSize);
		}
		if (!cons_utf8 && isutf8) {
			if ((debugLevel >= 2 || (debugLevel == 1 && showMessage))
			&& firstPart)
				i_puts(MSG_Conv8859);
			utf2iso((uchar *) rbuf, *partSize_p,
				(uchar **) & tbuf, &*partSize_p);
			if(*isAllocated_p) nzFree(rbuf);
			*isAllocated_p = true;
			*rbuf_p = rbuf = tbuf;
			fileSize += (*partSize_p - oldSize);
		}
		if (cons_utf8 && isutf8 && firstPart) {
// Strip off the leading bom, if any.
			if (fileSize >= 3 &&
			    !memcmp(rbuf, "\xef\xbb\xbf", 3)) {
				if ((debugLevel >= 2 || (debugLevel == 1 && showMessage)))
					i_puts(MSG_RemovingBOM);
				fileSize -= 3, *partSize_p -= 3;
				memmove(rbuf, rbuf + 3, *partSize_p);
				if (!cw->dol)
					cw->utf8Mark = true;
			}
		}
	}

// undos, only if each \n has \r preceeding.
	for (i = 0; i < *partSize_p; ++i) {
		if (rbuf[i] != '\n') continue;
		if (i && rbuf[i - 1] == '\r') crlf_yes = true;
		else crlf_no = true;
	}
	if (crlf_yes && !crlf_no) dosmode = true;

	if (dosmode) {
		if ((debugLevel >= 2 || (debugLevel == 1 && showMessage))
		&& firstPart)
			i_puts(MSG_ConvUnix);
		for (i = j = 0; i < *partSize_p; ++i) {
			char c = rbuf[i];
			if (c == '\r' && rbuf[i + 1] == '\n')
				continue;
			rbuf[j++] = c;
		}
		rbuf[j] = 0;
		fileSize -= (*partSize_p - j);
		*partSize_p = j;
	}

// if reading into an empty buffer, set the mode and print message
	if (!cw->dol) {
		if (bom & 2) {
			cw->utf32Mode = true;
			debugPrint(3, "setting utf32 mode");
		}
		if (bom & 1) {
			cw->utf16Mode = true;
			debugPrint(3, "setting utf16 mode");
		}
		if (bom & 4)
			cw->bigMode = true;
		if (isutf8) {
			cw->utf8Mode = true;
			debugPrint(3, "setting utf8 mode");
		}
		if (is8859) {
			cw->iso8859Mode = true;
			debugPrint(3, "setting 8859 mode");
		}
		if (dosmode) {
			cw->dosMode = true;
			debugPrint(3, "setting dos mode");
		}
	}
}

// Convert from whatever it is to utf8, for javascript and css.
// Result parameter is the new string, or null if no conversion.
// But, if the original string is utf8, I remove the bom.
// Also turn \0 into spaces.
char *force_utf8(char *buf, int buflen)
{
	char *tbuf, *s;
	int bom = byteOrderMark((const uchar *)buf, buflen);
	if (bom) {
		debugPrint(3, "text type is %s%s",
			   ((bom & 4) ? "big " : ""),
			   ((bom & 2) ? "utf32" : "utf16"));
		if (debugLevel >= 3)
			i_puts(MSG_ConvUtf8);
		utfLow(buf, buflen, &tbuf, &buflen, bom);
// get rid of \0
		for (s = tbuf; s < tbuf + buflen; ++s)
			if (!*s)
				*s = ' ';
		*s = 0;
		return tbuf;
	}
// Strip off the leading bom, if any, and no we're not going to put it back.
	if (buflen >= 3 && !memcmp(buf, "\xef\xbb\xbf", 3)) {
		buflen -= 3;
		memmove(buf, buf + 3, buflen);
		buf[buflen] = 0;
	}
	for (s = buf; s < buf + buflen; ++s)
		if (!*s)
			*s = ' ';
	return NULL;
}

static const char base64_chars[] =
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
		outlen += ((inlen / 54) + 1) * 2;
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
		*out++ = '\r', *out++ = '\n';
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
		*out++ = '\r', *out++ = '\n';
	*out = 0;
	return outstr;
}

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
}

/*********************************************************************
Decode some data in base64.
This function operates on the data in-line.  It does not allocate a fresh
string to hold the decoded data.  Since the data will be smaller than
the base64 encoded representation, this cannot overflow.
If you need to preserve the input, copy it first.
start points to the start of the input
*end initially points to the byte just after the end of the input
Returns: GOOD_BASE64_DECODE on success, BAD_BASE64_DECODE or
EXTRA_CHARS_BASE64_DECODE on error.
When the function returns success, *end points to the end of the decoded
data.  On failure, end points to the byte just past the end of
what was successfully decoded.
*********************************************************************/

int base64Decode(char *start, char **end)
{
	char *b64_end = *end;
	uchar val, leftover = 0, mod;
	bool equals;
	int ret = GOOD_BASE64_DECODE;
	char c, *q, *r;
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
}

void
iuReformat(const char *inbuf, int inbuflen, char **outbuf_p, int *outbuflen_p)
{
	bool is8859, isutf8;

	*outbuf_p = 0;
	*outbuflen_p = 0;
	if (!iuConvert)
		return;

	looks_8859_utf8((uchar *) inbuf, inbuflen, &is8859, &isutf8);
	if (cons_utf8 && is8859) {
		debugPrint(3, "converting to utf8");
		iso2utf((uchar *) inbuf, inbuflen, (uchar **) outbuf_p,
			outbuflen_p);
	}
	if (!cons_utf8 && isutf8) {
		debugPrint(3, "converting to iso8859");
		utf2iso((uchar *) inbuf, inbuflen, (uchar **) outbuf_p,
			outbuflen_p);
	}
}

// Lines have to be doslike when sending multipart/form-data.
// Question: is this also true when sending text attachments by email?
// Free the input string and return the output string allocated.
char *makeDosNewlines(char *p)
{
	int linecount = 0;
	char *s, *t, *p2;
	for(s = p; *s; ++s)
		if(*s == '\n' && s > p && s[-1] != '\r')
			++linecount;
	if(!linecount)
		return p;
	t = p2 = allocMem(strlen(p) + linecount + 1);
	for(s = p; *s; ++s) {
		if(*s == '\n' && s > p && s[-1] != '\r')
			*t++ = '\r';
		*t++ = *s;
	}
*t = 0;
	nzFree(p);
	return p2;
}

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
}

uchar fromHex(char d, char e)
{
	d |= 0x20, e |= 0x20;
	if (d >= 'a')
		d -= ('a' - '9' - 1);
	if (e >= 'a')
		e -= ('a' - '9' - 1);
	d -= '0', e -= '0';
	return ((((uchar) d) << 4) | (uchar) e);
}

// find the color closest to the rgb value.
// Input string is allocated; return is either the einput string
// or another allocated string.
char *closeColor(const char *s)
{
// indent formats an array of structures really weird; not like I would.
	const struct reserved {
		const char *name;
		uchar r, g, b;
	} colorlist[] = {
		{
		"aliceblue", 0xf0, 0xf8, 0xff}, {
		"antiquewhite", 0xfa, 0xeb, 0xd7}, {
		"aqua", 0x00, 0xff, 0xff}, {
		"aquamarine", 0x7f, 0xff, 0xd4}, {
		"azure", 0xf0, 0xff, 0xff}, {
		"beige", 0xf5, 0xf5, 0xdc}, {
		"bisque", 0xff, 0xe4, 0xc4}, {
		"black", 0x00, 0x00, 0x00}, {
		"blanchedalmond", 0xff, 0xeb, 0xcd}, {
		"blue", 0x00, 0x00, 0xff}, {
		"blueviolet", 0x8a, 0x2b, 0xe2}, {
		"brown", 0xa5, 0x2a, 0x2a}, {
		"burlywood", 0xde, 0xb8, 0x87}, {
		"cadetblue", 0x5f, 0x9e, 0xa0}, {
		"chartreuse", 0x7f, 0xff, 0x00}, {
		"chocolate", 0xd2, 0x69, 0x1e}, {
		"coral", 0xff, 0x7f, 0x50}, {
		"cornflowerblue", 0x64, 0x95, 0xed}, {
		"cornsilk", 0xff, 0xf8, 0xdc}, {
		"crimson", 0xdc, 0x14, 0x3c}, {
		"cyan", 0x00, 0xff, 0xff}, {
		"darkblue", 0x00, 0x00, 0x8b}, {
		"darkcyan", 0x00, 0x8b, 0x8b}, {
		"darkgoldenrod", 0xb8, 0x86, 0x0b}, {
		"darkgray", 0xa9, 0xa9, 0xa9}, {
		"darkgreen", 0x00, 0x64, 0x00}, {
		"darkkhaki", 0xbd, 0xb7, 0x6b}, {
		"darkmagenta", 0x8b, 0x00, 0x8b}, {
		"darkolivegreen", 0x55, 0x6b, 0x2f}, {
		"darkorange", 0xff, 0x8c, 0x00}, {
		"darkorchid", 0x99, 0x32, 0xcc}, {
		"darkred", 0x8b, 0x00, 0x00}, {
		"darksalmon", 0xe9, 0x96, 0x7a}, {
		"darkseagreen", 0x8f, 0xbc, 0x8f}, {
		"darkslateblue", 0x48, 0x3d, 0x8b}, {
		"darkslategray", 0x2f, 0x4f, 0x4f}, {
		"darkturquoise", 0x00, 0xce, 0xd1}, {
		"darkviolet", 0x94, 0x00, 0xd3}, {
		"deeppink", 0xff, 0x14, 0x93}, {
		"deepskyblue", 0x00, 0xbf, 0xff}, {
		"dimgray", 0x69, 0x69, 0x69}, {
		"dodgerblue", 0x1e, 0x90, 0xff}, {
		"feldspar", 0xd1, 0x92, 0x75}, {
		"firebrick", 0xb2, 0x22, 0x22}, {
		"floralwhite", 0xff, 0xfa, 0xf0}, {
		"forestgreen", 0x22, 0x8b, 0x22}, {
		"fuchsia", 0xff, 0x00, 0xff}, {
		"gainsboro", 0xdc, 0xdc, 0xdc}, {
		"ghostwhite", 0xf8, 0xf8, 0xff}, {
		"gold", 0xff, 0xd7, 0x00}, {
		"goldenrod", 0xda, 0xa5, 0x20}, {
		"gray", 0x80, 0x80, 0x80}, {
		"green", 0x00, 0x80, 0x00}, {
		"greenyellow", 0xad, 0xff, 0x2f}, {
		"honeydew", 0xf0, 0xff, 0xf0}, {
		"hotpink", 0xff, 0x69, 0xb4}, {
		"indianred", 0xcd, 0x5c, 0x5c}, {
		"indigo", 0x4b, 0x00, 0x82}, {
		"ivory", 0xff, 0xff, 0xf0}, {
		"khaki", 0xf0, 0xe6, 0x8c}, {
		"lavender", 0xe6, 0xe6, 0xfa}, {
		"lavenderblush", 0xff, 0xf0, 0xf5}, {
		"lawngreen", 0x7c, 0xfc, 0x00}, {
		"lemonchiffon", 0xff, 0xfa, 0xcd}, {
		"lightblue", 0xad, 0xd8, 0xe6}, {
		"lightcoral", 0xf0, 0x80, 0x80}, {
		"lightcyan", 0xe0, 0xff, 0xff}, {
		"lightgoldenrodyellow", 0xfa, 0xfa, 0xd2}, {
		"lightgrey", 0xd3, 0xd3, 0xd3}, {
		"lightgreen", 0x90, 0xee, 0x90}, {
		"lightpink", 0xff, 0xb6, 0xc1}, {
		"lightsalmon", 0xff, 0xa0, 0x7a}, {
		"lightseagreen", 0x20, 0xb2, 0xaa}, {
		"lightskyblue", 0x87, 0xce, 0xfa}, {
		"lightslateblue", 0x84, 0x70, 0xff}, {
		"lightslategray", 0x77, 0x88, 0x99}, {
		"lightsteelblue", 0xb0, 0xc4, 0xde}, {
		"lightyellow", 0xff, 0xff, 0xe0}, {
		"lime", 0x00, 0xff, 0x00}, {
		"limegreen", 0x32, 0xcd, 0x32}, {
		"linen", 0xfa, 0xf0, 0xe6}, {
		"magenta", 0xff, 0x00, 0xff}, {
		"maroon", 0x80, 0x00, 0x00}, {
		"mediumaquamarine", 0x66, 0xcd, 0xaa}, {
		"mediumblue", 0x00, 0x00, 0xcd}, {
		"mediumorchid", 0xba, 0x55, 0xd3}, {
		"mediumpurple", 0x93, 0x70, 0xd8}, {
		"mediumseagreen", 0x3c, 0xb3, 0x71}, {
		"mediumslateblue", 0x7b, 0x68, 0xee}, {
		"mediumspringgreen", 0x00, 0xfa, 0x9a}, {
		"mediumturquoise", 0x48, 0xd1, 0xcc}, {
		"mediumvioletred", 0xc7, 0x15, 0x85}, {
		"midnightblue", 0x19, 0x19, 0x70}, {
		"mintcream", 0xf5, 0xff, 0xfa}, {
		"mistyrose", 0xff, 0xe4, 0xe1}, {
		"moccasin", 0xff, 0xe4, 0xb5}, {
		"navajowhite", 0xff, 0xde, 0xad}, {
		"navy", 0x00, 0x00, 0x80}, {
		"oldlace", 0xfd, 0xf5, 0xe6}, {
		"olive", 0x80, 0x80, 0x00}, {
		"olivedrab", 0x6b, 0x8e, 0x23}, {
		"orange", 0xff, 0xa5, 0x00}, {
		"orangered", 0xff, 0x45, 0x00}, {
		"orchid", 0xda, 0x70, 0xd6}, {
		"palegoldenrod", 0xee, 0xe8, 0xaa}, {
		"palegreen", 0x98, 0xfb, 0x98}, {
		"paleturquoise", 0xaf, 0xee, 0xee}, {
		"palevioletred", 0xd8, 0x70, 0x93}, {
		"papayawhip", 0xff, 0xef, 0xd5}, {
		"peachpuff", 0xff, 0xda, 0xb9}, {
		"peru", 0xcd, 0x85, 0x3f}, {
		"pink", 0xff, 0xc0, 0xcb}, {
		"plum", 0xdd, 0xa0, 0xdd}, {
		"powderblue", 0xb0, 0xe0, 0xe6}, {
		"purple", 0x80, 0x00, 0x80}, {
		"red", 0xff, 0x00, 0x00}, {
		"rosybrown", 0xbc, 0x8f, 0x8f}, {
		"royalblue", 0x41, 0x69, 0xe1}, {
		"saddlebrown", 0x8b, 0x45, 0x13}, {
		"salmon", 0xfa, 0x80, 0x72}, {
		"sandybrown", 0xf4, 0xa4, 0x60}, {
		"seagreen", 0x2e, 0x8b, 0x57}, {
		"seashell", 0xff, 0xf5, 0xee}, {
		"sienna", 0xa0, 0x52, 0x2d}, {
		"silver", 0xc0, 0xc0, 0xc0}, {
		"skyblue", 0x87, 0xce, 0xeb}, {
		"slateblue", 0x6a, 0x5a, 0xcd}, {
		"slategray", 0x70, 0x80, 0x90}, {
		"snow", 0xff, 0xfa, 0xfa}, {
		"springgreen", 0x00, 0xff, 0x7f}, {
		"steelblue", 0x46, 0x82, 0xb4}, {
		"tan", 0xd2, 0xb4, 0x8c}, {
		"teal", 0x00, 0x80, 0x80}, {
		"thistle", 0xd8, 0xbf, 0xd8}, {
		"tomato", 0xff, 0x63, 0x47}, {
		"turquoise", 0x40, 0xe0, 0xd0}, {
		"violet", 0xee, 0x82, 0xee}, {
		"violetred", 0xd0, 0x20, 0x90}, {
		"wheat", 0xf5, 0xde, 0xb3}, {
		"white", 0xff, 0xff, 0xff}, {
		"whitesmoke", 0xf5, 0xf5, 0xf5}, {
		"yellow", 0xff, 0xff, 0x00}, {
		"yellowgreen", 0x9a, 0xcd, 0x32}, {
		0}
	};
	const struct reserved *c, *best_c = 0;
	int best_val;
	int r1, g1, b1;
	const char *t;

	if (!strncmp(s, "rgb(", 4)) {
		t = s + 4;
		if (!isdigitByte(*t))
			goto fail;
		r1 = strtol(t, (char **)&t, 10);
		if (*t == ',')
			++t;
		while (*t == ' ')
			++t;
		if (!isdigitByte(*t))
			goto fail;
		g1 = strtol(t, (char **)&t, 10);
		if (*t == ',')
			++t;
		while (*t == ' ')
			++t;
		if (!isdigitByte(*t))
			goto fail;
		b1 = strtol(t, (char **)&t, 10);
		if (*t == ',')
			++t;
		while (*t == ' ')
			++t;
		if (*t != ')')
			goto fail;
	} else if (*s == '#' && isxdigit(s[1])) {
		if (!isxdigit(s[2]) || !isxdigit(s[3]))
			goto fail;
		if (isxdigit(s[4]) && isxdigit(s[5]) && isxdigit(s[6])) {
			r1 = fromHex(s[1], s[2]);
			g1 = fromHex(s[3], s[4]);
			b1 = fromHex(s[5], s[6]);
		} else {
// #xyz is short for #xxyyzz
			r1 = fromHex(s[1], s[1]);
			g1 = fromHex(s[2], s[2]);
			b1 = fromHex(s[3], s[3]);
		}
	} else {
// not an rgb format we recognize; should be just a word.
		for (t = s; *t; ++t)
			if (!isalphaByte(*t))
				goto fail;
		return (char *)s;
	}

	if (r1 < 0 || g1 < 0 || b1 < 0)
		goto fail;
	if (r1 > 255 || g1 > 255 || b1 > 255)
		goto fail;

// closest by rms; just check them all; kind of inefficient.
	best_val = 255 * 255 * 3 + 1;
	for (c = colorlist; c->name; ++c) {
		int rms = (r1 - (int)c->r) * (r1 - (int)c->r) +
		    (g1 - (int)c->g) * (g1 - (int)c->g) +
		    (b1 - (int)c->b) * (b1 - (int)c->b);
		if (rms < best_val)
			best_val = rms, best_c = c;
	}
	return cloneString(best_c->name);

fail:
	return 0;
}

/*********************************************************************
Read in emojis from a library.
These are hierarchical - a group, then the emojis in that group.
heart.green  heart.blue  etc.  Like Carl Linnaeus.
No more than 100 emojis per group.
This seems arbitrary - but if you just wanna review the emojis in a group,
cause you don't remember them, you don't want a menu with hundreds of choices.
Maybe 100 could be larger, but for practical considerations,
there should be a limit.
*********************************************************************/

#define EJGROUPSIZE 100

static struct EJGROUP {
	struct EJGROUP *next;
	int n;
	const char *name;
	const char *desc[EJGROUPSIZE];
	long code[EJGROUPSIZE];
	long code2[EJGROUPSIZE];
	char join[EJGROUPSIZE];
} *ejgroup0;
// the emojis file pulled into memory
static char *ejbase;

void clearEmojis(void)
{
	struct EJGROUP *g, *g1;
	nzFree(ejbase);
	ejbase = 0;
	g = ejgroup0;
	while(g) {
		g1 = g->next;
		free(g);
		g = g1;
	}
ejgroup0 = 0;
}

void loadEmojis(void)
{
	int i, j, len, lineno;
	char c;
	uchar state;
	struct EJGROUP *g = 0;
	char *s, *t, *u;

	if(!emojiFile) // should never happen
		return;
	if(ejgroup0) // should never happen
		clearEmojis();
	if(!fileIntoMemory(emojiFile, &ejbase, &len, 0)) {
		showError();
		setError(-1);
		return;
	}

// look for bad characters, and compress whitespace
	state = 2, lineno = 1;
	for(i = j = 0; i < len; ++i) {
		c = ejbase[i];
		if(state == 3 && c != '\n') continue;
		if(c == '\t' || c == '\r' || c == '\f')
			c = ' ';
		if(c == ' ') {
			if(!state) ejbase[j++] = c, state = 1;
			continue;
		}
		if(c == '\n') {
			if(state == 1) --j;
			ejbase[j++] = c;
			state = 2, ++lineno;
			continue;
		}
		if(c == '#' && state == 2) {
			state = 3; continue;
		}
// all the whitespace has been dealt with
// I use to disallow nonascii, but other languages might
// employ accented letters, as utf8.
#if 0
		if((signed char)c <= ' ') {
			i_printf(MSG_EmojiNonascii, lineno);
			goto fail;
		}
#endif
		if((uchar) c < ' ') {
			i_printf(MSG_EmojiBadControl, c, lineno);
			goto fail;
		}
		if((signed char)c > ' ' && !isalnumByte(c) &&  !strchr("{}+^", c)) {
			i_printf(MSG_EmojiBadChar, c, lineno);
			goto fail;
		}
		ejbase[j++] = c;
		state = 0;
	}
	ejbase[j] = 0; // null terminate

	s = ejbase, lineno = 0;
	while(++lineno, *s) {
		bool limitprint = false;
		long uc, uc2;
		char joinchar;
		t = strchr(s, '\n');
		if(t) *t++ = 0; else t = s + strlen(s);
		if(!*s) { // empty line
			s = t; continue;
		}
		if(!g) {
// need to start a new group
			for(u = s; isalnumByte(*u); ++u)  ;
			if(u == s || !stringEqual(u, " {")) {
				i_printf(MSG_EmojiSyntax, lineno);
				s = t; continue;
			}
			g = allocMem(sizeof(struct EJGROUP));
			g->name = s, *u = 0;
			g->n = 0;
			g->next = 0;
			if(!ejgroup0) {
				ejgroup0 = g;
			} else {
				struct EJGROUP *g1 = ejgroup0;
				while(g1->next) g1 = g1->next;
				g1->next = g;
			}
			limitprint = false;
			s = t; continue;
		}
// now in a group
		if(stringEqual(s, "}")) {
			g = 0, s = t; continue;
		}
		uc = strtol(s, &u, 16);
		if(uc <= 0) {
			i_printf(MSG_EmojiUnicode, lineno);
			s = t; continue;
		}
		joinchar = 0;
		if(*u == '^' || *u == '+') {
			joinchar = *u;
			uc2 = strtol(u + 1, &u, 16);
		}
		if(!*u) { // no description
			s = t; continue;
		}
		if(*u != ' ' || strchr(u, '{') || strchr(u, '}')) {
			i_printf(MSG_EmojiSyntax, lineno);
			s = t; continue;
		}
		if(g->n == EJGROUPSIZE) {
			if(!limitprint)
				i_printf(MSG_EmojiOverGroup, g->name, EJGROUPSIZE);
			limitprint = true;
		} else {
			g->desc[g->n] = u+1;
			g->code[g->n] = uc;
			g->join[g->n] = joinchar;
			if(joinchar)
				g->code2[g->n] = uc2;
			++g->n;
		}
		s = t;
	}

	return;

fail:
	free(ejbase);
	ejbase = 0;
}

static const struct EJGROUP *ejGroupSearch(const char *s, int len)
{
	const struct EJGROUP *g, *g1 = 0;
	for(g = ejgroup0; g; g = g->next) {
		if(memcmp(g->name, s, len))
			continue;
		if(!g->name[len]) // exact match
			return g;
		if(g1) // ambiguous
			goto fail;
		g1 = g;
	}
	if(g1)
		return g1;
fail:
	i_printf(g1 ? MSG_Multiple : MSG_NoMatch);
	printf(": ");
	while(len--)
		printf("%c", *s++);
	nl();
	for(g = ejgroup0; g; g = g->next)
		  printf("%s%c", g->name, (g->next ? ' ' : '\n'));
	return 0;
}

// Gather an input line to select an emoji. Rather like the routine
// in buffers.c, but I can't call that, or I'm infinitely recursive.
static char *ejline; // alocated
static int ejcommas; // selecting multiple emojis
static void ejGetLine(void)
{
	char c, line[MAXTTYLINE];
	int i, j, len, state;

	ejline = 0;
top:
	intFlag = false;

	inInput = true;
	while (fgets(line, sizeof(line), stdin)) {
// A bug in my keyboard causes nulls to be entered from time to time.
		c = 0;
		i = 0;
		while ((unsigned)i < sizeof(line) - 1 && (c = line[i]) != '\n') {
			if (c == 0)
				line[i] = ' ';
			++i;
		}
		if (ejline) {
			len = strlen(ejline);
// with nulls transliterated, strlen() returns the right answer
			i = strlen(line);
			ejline = reallocMem(ejline, len + i + 1);
			strcpy(ejline + len, line);
		} else
			ejline = cloneString(line);
		if (c == '\n')
			goto tty_complete;
	}
	if (intFlag)
		goto top;
	i_puts(MSG_EndFile);
		ebClose(1);

tty_complete:
	inInput = false;
	state = 2, ejcommas = 0;
	for(i = j = 0; (c = ejline[i]); ++i) {
		if(isspaceByte(c)) c = ' ';
		if(c == ' ') {
			if(!state) ejline[j++] = c, state = 1;
			continue;
		}
		if(c == ',') {
			if(state == 1) --j;
			ejline[j++] = c;
			state = 2, ++ejcommas;
			continue;
		}
// all the whitespace has been dealt with
		if((signed char)c <= ' ')
			continue;
		ejline[j++] = c;
		state = 0;
	}
	if(state == 1) --j;
ejline[j] = 0; // null terminate

// take out empty fields
	state = 2;
	for(i = j = 0; (c = ejline[i]); ++i) {
		if(c == ',') {
			if(state == 2) { --ejcommas; continue; }
			state = 2;
		} else state = 1;
		ejline[j++] = c;
	}
	if(j && line[j - 1] == ',') --j, --ejcommas;
	ejline[j] = 0;
}

static bool ejmatches[EJGROUPSIZE];
static int ejcount;

/*********************************************************************
Does the string you typed match one and only one emoji in the group?
If it is from input, then you can't include spaces, so I skate
past spaces in the matching algorithm.
If you are selecting from a menu, then you can and should include spaces.
Note the frommenu parameter below.
I use to search for any substring in the string, as we do with an
html dropdown menu, but in this case I think we want the initial substring.
anim.b gives bear, bat, boar, etc.
So I match what you typed against the beginning of the emoji text.
*********************************************************************/

static int ejSelect(const struct EJGROUP *g, const char *s, int len, bool frommenu)
{
	int partial = -1;
	int n, i, j;
	const char *t;

// select an option by number
	if(frommenu &&
	(n = strtol(s, (char**)&t, 10)) >= 0 &&
	t - s == len) {
		if(!n || n > g->n || !ejmatches[n - 1]) {
			i_printf(MSG_XOutOfRange, n), nl();
			return -2;
		}
		return n - 1;
	}

	if(!frommenu)
		memset(ejmatches, false, EJGROUPSIZE);
	ejcount = 0;

// search by string or substring
	for(n = 0; n < g->n; ++n) {
		if(frommenu && !ejmatches[n])
			continue;
		t = g->desc[n]; // shorthand
		if(*t != *s)
			continue;
		for(i = j = 0; i < len; ++i, ++j) {
// from the inputline we don't have spaces
			if(t[j] == ' ' && !frommenu) {
			--i;
				continue;
			}
			if(s[i] != t[j])
				break;
		}
		if(i < len) // no match
			continue;
		if(t == g->desc[n] && !t[j]) // exact match
			return n;
		if(!ejcount++) partial = n;
		ejmatches[n] = true;
	}

	return (ejcount == 1 ? partial : -1);
}

char *selectEmoji(const char *p, int len)
{
	int dot; // index where dot is found
	const struct EJGROUP *g;
	int n;
	char *s, *t, *ejresponse;

	for(dot = 0; dot < len; ++dot)
		if(p[dot] == '.') break;
	if(dot == len)
		dot = 0;
	if(dot && dot == len - 1)
		dot = 0, --len;

	g = ejGroupSearch(p, (dot ? dot : len));
	if(!g || g->n == 0) // no group selected or no options
		return 0;

	if(dot) { // foo.bar
		n = ejSelect(g, p + dot + 1, len - dot - 1, false);
		if(n >= 0) {
			ejresponse = allocMem(15 + 1);
			strcpy(ejresponse, uni2utf8(g->code[n]));
			if(g->join[n] == '^')
				strcat(ejresponse, uni2utf8(8205));
			if(g->join[n])
				strcat(ejresponse, uni2utf8(g->code2[n]));
			return ejresponse;
		}
	}

// present the menu.
// If there were no matches on the string you provided, then present all options.
	if(!dot || ejcount == 0)
		memset(ejmatches, true, EJGROUPSIZE);
	for(n = 0; n < g->n; ++n) {
		if(ejmatches[n])
			printf("%d: %s\n", n + 1, g->desc[n]);
	}

top:
	ejGetLine();
	if(!ejline[0] || stringEqual(ejline, "qt")) { // abort
		free(ejline);
		return 0;
	}
// picking one or more emojis
	ejresponse = allocMem(15 * ejcommas + 15 + 1);
	ejresponse[0] = 0;
	s = ejline;
	while(*s) {
		t = strchr(s, ',');
		if(t) *t++ = 0; else t = s + strlen(s);
		n = ejSelect(g, s, strlen(s), true);
		if(n < 0) {
			if(n == -1)
				i_printf(MSG_OptStartNone + (ejcount > 0), s), nl();
			free(ejline);
			free(ejresponse);
			goto top;
		}
		strcat(ejresponse, uni2utf8(g->code[n]));
			if(g->join[n] == '^')
				strcat(ejresponse, uni2utf8(8205));
			if(g->join[n])
				strcat(ejresponse, uni2utf8(g->code2[n]));
		s = t;
	}
// all good
	free(ejline);
	return ejresponse;
}

// Messages in your host language.
static const char **messageArray;
char eb_language[8];
int eb_lang;
const char *const supported_languages[] = { 0,
	"english", "french", "portuguese", "polish",
	"german", "russian", "italian", "spanish",
};
// don't forget allMonths in stringfile.c

// startup .ebrc files in various languages
const char *ebrc_string;
static const char *qrg_string;

#include <locale.h>

void selectLanguage(void)
{
	char *s = getenv("LANG");
	char *dot;

// default is English
	strcpy(eb_language, "en");
	eb_lang = 1;
	messageArray = msg_en;
	ebrc_string = ebrc_en;
	qrg_string = qrg_en;

	if (!s || !*s) return;

	if (strcasestr(s, "utf8") || strcasestr(s, "utf-8"))
		cons_utf8 = true;

/*********************************************************************
We roll our own international messages in this file, so you wouldn't think
we need setlocale, but pcre needs the locale for expressions like \w,
and for ranges like [A-z],
and to convert to upper or lower case etc.
I think we want a standard behavior here, so I'm going with C.

By calling strcoll, the directory scan is in the same order as ls.
But only if we get our collation information from the environment, as ls does.

quickjs parses numbers according to locale, and that has to be
standard C numbers, or all the javascript on the internet won't run.

LC_TIME controls time/date formatting, I.E., strftime.  The place we do that,
sendmail.c, we need standard day/month abbreviations, not  localized ones.
According to the rfc standards. So LC_TIME needs to be C.
*********************************************************************/

	setlocale(LC_CTYPE, "C");
	setlocale(LC_COLLATE, "");
	setlocale(LC_NUMERIC, "C");
	setlocale(LC_TIME, "C");

	strncpy(eb_language, s, 7);
	eb_language[7] = 0;
	caseShift(eb_language, 'l');
	dot = strchr(eb_language, '.');
	if (dot) *dot = 0;
	for(s = eb_language; *s; ++s)
		if(*s == '_') *s = '-';

	if (!strncmp(eb_language, "en", 2))
		return;		// english is already default

	if (!strncmp(eb_language, "fr", 2)) {
		eb_lang = 2;
		messageArray = msg_fr;
		ebrc_string = ebrc_fr;
		qrg_string = qrg_fr;
		type8859 = 1;
		return;
	}

	if (!strncmp(eb_language, "pt", 2)) {
// This is Brazillian Portuguese. We use to key on pt_br.
// It is close to pt_pt but not identical. I assume it is close enough.
// Someone from Portugal can write msg_pt_pt if they wish.
		eb_lang = 3;
		messageArray = msg_pt_br;
		ebrc_string = ebrc_pt_br;
		qrg_string = qrg_pt_br;
		type8859 = 1;
		return;
	}

	if (!strncmp(eb_language, "pl", 2)) {
		eb_lang = 4;
		messageArray = msg_pl;
		ebrc_string = ebrc_pl;
		type8859 = 2;
		return;
	}

	if (!strncmp(eb_language, "de", 2)) {
		eb_lang = 5;
		messageArray = msg_de;
		ebrc_string = ebrc_de;
		type8859 = 1;
		return;
	}

	if (!strncmp(eb_language, "ru", 2)) {
		eb_lang = 6;
		messageArray = msg_ru;
		ebrc_string = ebrc_ru;
		type8859 = 5;
		return;
	}

	if (!strncmp(eb_language, "it", 2)) {
		eb_lang = 7;
		messageArray = msg_it;
		type8859 = 1;
		return;
	}

	if (!strncmp(eb_language, "es", 2)) {
		eb_lang = 8;
		messageArray = msg_es;
		type8859 = 1;
		ebrc_string = ebrc_es;
		qrg_string = qrg_es;
		return;
	}

/* This error is really annoying if it pops up every time you invoke edbrowse.
	fprintf(stderr, "Sorry, language %s is not implemented\n", buf);
*/
}

/*********************************************************************
WARNING: this routine, which is at the heart of the international prints
i_puts i_printf, is not threadsafe in iso8859 mode.
Well utf8 has been the default console standard since er um 2000,
and I'm almost ready to chuck iso8859 console support altogether.
*********************************************************************/

const char *i_message(int msg)
{
	const char **a = messageArray;
	const char *s;
	char *t;
	int t_len;
// large enough for any edbrowse message
	static char utfbuf[1000];
	if (msg >= EdbrowseMessageCount) s = emptyString;
	else s = a[msg];
	if (!s) s = msg_en[msg];
	if (!s) s = "spurious message";
	if (cons_utf8) return s;

// Oops, we have to convert
	utf2iso((uchar *) s, strlen(s), (uchar **) & t, &t_len);
	strcpy(utfbuf, t);
	nzFree(t);
	return utfbuf;
}

/*********************************************************************
Internationalize the standard puts and printf.
These are simple informational messages, where you don't need to error out,
or check the debug level, or store the error in a buffer.
The i_ prefix means international.
*********************************************************************/

void i_puts(int msg)
{
	eb_puts(i_message(msg));
}

void i_printf(int msg, ...)
{
	const char *realmsg = i_message(msg);
	va_list p;
	va_start(p, msg);
	vprintf(realmsg, p);
	va_end(p);
	if (debugFile) {
		va_start(p, msg);
		vfprintf(debugFile, realmsg, p);
		va_end(p);
	}
}

// Print and exit.  This puts newline on, like puts.
void i_printfExit(int msg, ...)
{
	const char *realmsg = i_message(msg);
	va_list p;
	va_start(p, msg);
	vprintf(realmsg, p);
	nl();
	va_end(p);
	if (debugFile) {
		va_start(p, msg);
		vfprintf(debugFile, realmsg, p);
		fprintf(debugFile, "\n");
		va_end(p);
	}
	ebClose(99);
}

/*********************************************************************
The following error display functions are specific to edbrowse,
rather than extended versions of the standard unix print functions.
Thus I don't need the i_ prefix.
*********************************************************************/

char errorMsg[1024];

// Show the error message, not just the question mark, after these commands.
static const char showerror_cmd[] = "AbefMqrw^&";

// Set the error message.  Type h to see the message.
void setError(int msg, ...)
{
	va_list p;
	char *a;		// result of vasprintf
	int l;

	if (msg < 0) {
		errorMsg[0] = 0;
		return;
	}

	va_start(p, msg);
	if (vasprintf(&a, i_message(msg), p) < 0)
		i_printfExit(MSG_MemAllocError, 4096);
	va_end(p);
// If the error message is crazy long, truncate it.
	l = sizeof(errorMsg) - 1;
	strncpy(errorMsg, a, l);
	nzFree(a);
}

void showError(void)
{
	if (errorMsg[0])
		eb_puts(errorMsg);
	else
		i_puts(MSG_NoErrors);
}

void showErrorConditional(char cmd)
{
	if (helpMessagesOn || strchr(showerror_cmd, cmd))
		showError();
	else
		eb_puts("?");
}

void showErrorAbort(void)
{
	showError();
	ebClose(99);
}

void eb_printf(const char *fmt, ...)
{
	va_list p;
	va_start(p, fmt);
	vprintf(fmt, p);
	va_end(p);
	if (debugFile) {
		va_start(p, fmt);
		vfprintf(debugFile, fmt, p);
		va_end(p);
	}
}

void eb_puts(const char *s)
{
	puts(s);
	if (debugFile)
		fprintf(debugFile, "%s\n", s);
}

// the help command
bool helpUtility(void)
{
	int cx;

	if (!cxQuit(context, 0))
		return false;

	undoSpecialClear();

// maybe we already have a buffer with the help guide in it
	for (cx = 1; cx <= maxSession; ++cx) {
		Window *w = sessionList[cx].lw;
		if (!w)
			continue;
		if (!w->f0.fileName)
			continue;
		if (!stringEqual(w->f0.fileName, "qrg.browse"))
			continue;
		cxSwitch(cx, false);
		i_printf(MSG_MovedSession, cx);
		return true;
	}

	cx = sideBuffer(0, qrg_string, -1, "qrg");
	if (cx == 0)
		return false;
	cxSwitch(cx, false);
	i_printf(MSG_MovedSession, cx);
	browseCurrentBuffer(NULL, false);
	cw->dot = 1;
	return true;
}

// An edbrowse variable, primarily for scripting
static struct EBVAR {
	struct EBVAR *next;
	const char *name, *value;
	} *varhead;

static const char *varLookup(const char *name)
{
	const struct EBVAR *v;
	for(v = varhead; v; v = v->next)
		if(stringEqual(name, v->name)) return v->value;
	return NULL;
}

static void varSet(const char *name, const char *value)
{
	struct EBVAR *v, *v1 = 0;
	bool isclear = stringEqual(value, "clear");
	for(v = varhead; v; v1 = v, v = v->next)
		if(stringEqual(name, v->name)) break;
	if(v) {
		cnzFree(v->value);
		if(!isclear) {
			v->value = cloneString(value);
			return;
		}
// clear this variable
		cnzFree(v->name);
		if(v1) v1->next = v->next;
		else varhead = v->next;
		nzFree(v);
		return;
	}
	if(isclear) return;
	v = allocMem(sizeof(struct EBVAR));
	v->next = varhead, varhead = v;
	v->name = cloneString(name);
	v->value = cloneString(value);
}

bool varCommand(const char *line)
{
	const char *v;
	char cut;
// I will cast to char * to blank out a character; but I'll put it back.
	char *t;
	skipWhite(&line);
	t = (char*)line;
	if(!isalpha(*t)) goto fail;
	while(isalpha(*t) || isdigit(*t)) ++t;
	cut = *t;
	if(cut != '=' && cut != ':') goto fail;
	v = t + 1;
	if(cut == ':') {
// set environment variable
		if(*v != '=') goto fail;
		++v;
		*t = 0;
		if(stringEqual(v, "clear"))
			unsetenv(line);
		else
			setenv(line, v, 1);
		*t = cut;
		return true;
	}
	*t = 0;
	varSet(line, v);
	*t = cut;
	return true;

fail:
	setError(MSG_VarSyntax);
	return false;
}

// expand variables, result is allocated as it could be considerably longer
bool varExpand(const char *line, char **newline)
{
	*newline = 0;
// high runner case first
	if(!strstr(line, "$(")) return true;

// ok folks, there is something to look at
	char *ns; // new string
	int ns_l; // length of new string
	ns = initString(&ns_l);
	const char *l1 = line, *l2, *l3, *lv;
	char *t; // I may cast to char*, to blank out something, but I'll put it back
	char cut, op1, op2;
	const char *value;
	int total, side, n = 0, sign;

top:
	l2 = strstr(l1, "$(");
	if(!l2) { // done
		stringAndString(&ns, &ns_l, l1);
		if(stringEqual(ns, line)) {
			nzFree(ns);
			return true;
		}
		*newline = ns;
		return true;
	}

	if(l2 > l1) stringAndBytes(&ns, &ns_l, l1, l2 - l1);
	l1 = l2;
	l3 = (l2 += 2);
	if(*l3 == ':') ++l3;
	lv = l3;
	if(!isalpha(*l3)) {
syntax:
// doesn't look right, just step ahead.
		stringAndString(&ns, &ns_l, "$(");
		l1 += 2;
		goto top;
	}
	while(isalnum(*l3) || *l3 == '_') ++l3;
	cut = *l3;
	if(!cut) goto syntax;
	if(!strchr(")+-*/", cut)) goto syntax;
	t = (char*)l3, *t = 0;
	if(*l2 == ':') eb_variables(), value = getenv(lv);
	else value = varLookup(lv);
	if(!value) {
reference:
#if 0
		nzFree(ns);
		setError(MSG_NoEbVar, lv);
		*t = cut;
		return false;
#else
		*t = cut;
		goto syntax;
#endif
	}

	if(cut == ')') { // simple variable reference
		*t = cut;
		stringAndString(&ns, &ns_l, value);
		l1 = l3 + 1;
		goto top;
	}

// an arithmetic expression begins here, every value henceforth
// has to be a number
	sign = 1;
	if(*value == '+') side = stringIsNum(value + 1);
	else if(*value == '-') sign = -1, side = stringIsNum(value + 1);
	else side = stringIsNum(value);
	if(side < 0) {
notnumber:
		nzFree(ns);
		setError(MSG_VarNumber, lv);
		*t = cut;
		return false;
	}
	side *= sign;
	total = 0, op1 = '+', op2 = cut;
	*t = cut;

nextarg:
	l2 = ++l3;
	if(*l3 == ':') ++l3;
	lv = l3;
	if(isdigit(*l3)) {
		n = strtol(l3, (char**)&l3, 10);
	} else {
		if(!isalpha(*l3)) goto syntax;
		while(isalnum(*l3)) ++l3;
	}
	cut = *l3;
	if(!cut) goto syntax;
	if(!strchr(")+-*/", cut)) goto syntax;
	t = (char*)l3, *t = 0;
	if(isalpha(*lv)) {
		if(*l2 == ':') eb_variables(), value = getenv(lv);
		else value = varLookup(lv);
		if(!value) goto reference;
		sign = 1;
		if(*value == '+') n = stringIsNum(value + 1);
		else if(*value == '-') sign = -1, n = stringIsNum(value + 1);
		else n = stringIsNum(value);
		if(n < 0) goto notnumber;
		n *= sign;
	}

// total op1 side op2 n cut
// op1 is always + or -
	if(op2 == '*') {
		side *= n;
	}

	if(op2 == '/') {
		if(!n) {
			nzFree(ns);
			setError(MSG_Div0);
			*t = cut;
			return false;
		}
// simulate $((x/y)) in the shell
// there's no floating point here
		int side1 = side, n1 = n;
		if(side1 < 0) side1 = -side1;
		if(n1 < 0) n1 = -n1;
		side1 /= n1;
		if(side < 0) side1 = -side1;
		if(n < 0) side1 = -side1;
		side = side1;
	}

	if(op2 != '*' && op2 != '/') {
		if(op1 == '+') total += side; else total -= side;
		side = n;
		op1 = op2;
	}

	*t = cut;
	if(cut == ')') { // end of expression
		if(op1 == '+') total += side; else total -= side;
		stringAndNum(&ns, &ns_l, total);
		l1 = l3 + 1;
		goto top;
	}
	op2 = cut;
	goto nextarg;
}

