/* messages.c
 * Error, warning, and info messages in your host language,
 * as determined by the variable $LANG.
 * Messages can be generated in iso-8859-1, but utf8 is recommended.
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"

#include <locale.h>

/* English by default */
static const char **messageArray = msg_en;
int eb_lang = 1;
/* startup .ebrc files in various languages */
const char *ebrc_string;
bool cons_utf8, iuConvert = true;
char type8859 = 1;
bool helpMessagesOn;
bool errorExit;

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
		*outbuf_p = emptyString;
		*outbuflen_p = 0;
		return;
	}

/* count chars, so we can allocate */
	for (i = 0; i < inbuflen; ++i) {
		c = inbuf[i];
		if (c < 0)
			++nacount;
	}

	outbuf = allocString(inbuflen + nacount + 1);
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
		*outbuf_p = emptyString;
		*outbuflen_p = 0;
		return;
	}

	outbuf = allocString(inbuflen + 1);
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

/*********************************************************************
Convert the current line in buffer, which is either iso8859-1 or utf8,
into utf16 or utf32, big or little endian.
The returned string is allocated, though not really a string,
since it will contain nulls, plenty of them in the case of utf32.
*********************************************************************/

void utfHigh(const char *inbuf, int inbuflen, char **outbuf_p, int *outbuflen_p)
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

	outbuf = allocMem(inbuflen * 4);	// worst case

	i = j = 0;
	while (i < inbuflen) {
		c = (uchar) inbuf[i];
		if (!cons_utf8 || (c & 0xc0) != 0xc0 && (c & 0xfe) != 0xfe) {
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

		if (cw->utf32Mode) {
			if (cw->bigMode) {
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
		if (unicode <= 0xd7ff || unicode >= 0xe000 && unicode <= 0xffff) {
			if (cw->bigMode) {
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
			if (cw->bigMode) {
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
}				/* utfHigh */

void selectLanguage(void)
{
	char buf[8];
	char *s = getenv("LANG");	// This is likely to fail in windows
	ebrc_string = ebrc_en;

#ifndef DOSLIKE
	if (!s)
		return;
	if (!*s)
		return;

	if (strstrCI(s, "utf8") || strstrCI(s, "utf-8"))
		cons_utf8 = true;

/* We roll our own international messages in this file, so you wouldn't think
 * we need setlocale, but pcre needs the locale for expressions like \w,
 * and for ranges like [A-z],
 * and to convert to upper or lower case etc.
 * So I set LC_ALL, which covers both LC_CTYPE and LC_COLLATE.
 * By calling strcoll, the directory scan is in the same order as ls.
 * See dircmp() in stringfile.c */

	setlocale(LC_ALL, "");

#else // DOSLIKE

/* I'm going to assume Windows runs utf8 */
	cons_utf8 = true;

	if (!s)
		s = setlocale(LC_ALL, "");
	if (!s)
		return;
	if (!*s)
		return;
#endif // DOSLIKE y/n

	strncpy(buf, s, 7);
	buf[7] = 0;
	caseShift(buf, 'l');

	if (!strncmp(buf, "en", 2))
		return;		/* english is default */

	if (!strncmp(buf, "fr", 2)) {
		eb_lang = 2;
		messageArray = msg_fr;
		ebrc_string = ebrc_fr;
		return;
	}

	if (!strncmp(buf, "pt_br", 5)) {
		eb_lang = 3;
		messageArray = msg_pt_br;
		ebrc_string = ebrc_pt_br;
		return;
	}

	if (!strncmp(buf, "pl", 2)) {
		eb_lang = 4;
		messageArray = msg_pl;
		type8859 = 2;
		return;
	}

	if (!strncmp(buf, "de", 2)) {
		eb_lang = 5;
		messageArray = msg_de;
		ebrc_string = ebrc_de;
		type8859 = 1;
		return;
	}

	if (!strncmp(buf, "ru", 2)) {
		eb_lang = 6;
		messageArray = msg_ru;
		type8859 = 5;
		return;
	}

/* This error is really annoying if it pops up every time you invoke edbrowse.
	fprintf(stderr, "Sorry, language %s is not implemented\n", buf);
*/
}				/* selectLanguage */

const char *i_getString(int msg)
{
	const char **a = messageArray;
	const char *s;
	char *t;
	int t_len;
	static char utfbuf[1000];

	if (msg >= EdbrowseMessageCount)
		s = emptyString;
	else
		s = a[msg];
	if (!s)
		s = msg_en[msg];
	if (!s)
		s = "spurious message";

	if (cons_utf8)
		return s;

/* We have to convert it. */
	utf2iso(s, strlen(s), &t, &t_len);
	strcpy(utfbuf, t);
	nzFree(t);
	return utfbuf;
}				/* i_getString */

/*********************************************************************
Internationalize the standard puts and printf.
These are simple informational messages, where you don't need to error out,
or check the debug level, or store the error in a buffer.
The i_ prefix means international.
*********************************************************************/

void i_puts(int msg)
{
	eb_puts(i_getString(msg));
}				/* i_puts */

void i_printf(int msg, ...)
{
	const char *realmsg = i_getString(msg);
	va_list p;
	va_start(p, msg);
	eb_vprintf(realmsg, p);
	va_end(p);
}				/* i_printf */

/* Print and exit.  This puts newline on, like puts. */
void i_printfExit(int msg, ...)
{
	const char *realmsg = i_getString(msg);
	va_list p;
	va_start(p, msg);
	eb_vprintf(realmsg, p);
	nl();
	va_end(p);
	ebClose(99);
}				/* i_printfExit */

/* i_stringAndMessage: concatenate a message to an existing string. */
void i_stringAndMessage(char **s, int *l, int messageNum)
{
	const char *messageText = i_getString(messageNum);
	stringAndString(s, l, messageText);
}				/* i_stringAndMessage */

/*********************************************************************
The following error display functions are specific to edbrowse,
rather than extended versions of the standard unix print functions.
Thus I don't need the i_ prefix.
*********************************************************************/

char errorMsg[4000];

/* Show the error message, not just the question mark, after these commands. */
static const char showerror_cmd[] = "AbefMqrw^";

/* Set the error message.  Type h to see the message. */
void setError(int msg, ...)
{
	va_list p;

	if (msg < 0) {
		errorMsg[0] = 0;
		return;
	}

	va_start(p, msg);
	vsprintf(errorMsg, i_getString(msg), p);
	va_end(p);

/* sanity check */
	if (strlen(errorMsg) >= sizeof(errorMsg)) {
		i_printf(MSG_ErrorMessageLong, strlen(errorMsg));
		puts(errorMsg);
		exit(1);
	}
}				/* setError */

void showError(void)
{
	if (errorMsg[0])
		eb_puts(errorMsg);
	else
		i_puts(MSG_NoErrors);
}				/* showError */

void showErrorConditional(char cmd)
{
	if (helpMessagesOn || strchr(showerror_cmd, cmd))
		showError();
	else
		eb_puts("?");
}				/* showErrorConditional */

void showErrorAbort(void)
{
	showError();
	ebClose(99);
}				/* showErrorAbort */

/* error exit check function */
void eeCheck(void)
{
	if (errorExit)
		ebClose(1);
}

/*********************************************************************
Now for the international version of caseShift.
This converts anything that might reasonably be a letter in your locale.
But it isn't ready for prime time.
I'd have to handle utf8 or not,
and then understand upper and lower case letters per language.
So this is commented out.
It was just a preliminary effort anyways, based on iso8859-1.
*********************************************************************/

#if 0

static const char upperMore[] = "";

static const char lowerMore[] = "";

static const char letterMore[] = "";

static bool i_isalphaByte(unsigned char c)
{
	if (isalphaByte(c))
		return true;
	if (c == false)
		return 0;
	if (strchr(letterMore, c))
		return true;
	return false;
}				/* i_isalphaByte */

/* assumes the arg is a letter */
static unsigned char i_tolower(unsigned char c)
{
	char *s;
	if (isalphaByte(c))
		return tolower(c);
	s = strchr(upperMore, c);
	if (s)
		c = lowerMore[s - upperMore];
	return c;
}				/* i_tolower */

static unsigned char i_toupper(unsigned char c)
{
	char *s;
	if (isalphaByte(c))
		return toupper(c);
	s = strchr(lowerMore, c);
	if (s)
		c = upperMore[s - lowerMore];
	return c;
}				/* i_toupper */

/* This is a variation on the original routine, found in stringfile.c */
void i_caseShift(unsigned char *s, char action)
{
	unsigned char c;
/* The McDonalds conversion is very English - should we do it in all languages? */
	int mc = 0;
	bool ws = true;

	for (; c = *s; ++s) {
		if (action == 'u') {
			if (i_isalphaByte(c))
				*s = i_toupper(c);
			continue;
		}

		if (action == 'l') {
			if (i_isalphaByte(c))
				*s = i_tolower(c);
			continue;
		}

/* mixed case left */
		if (i_isalphaByte(c)) {
			if (ws)
				c = i_toupper(c);
			else
				c = i_tolower(c);
			if (ws && c == 'M')
				mc = 1;
			else if (mc == 1 && c == 'c')
				mc = 2;
			else if (mc == 2) {
				c = i_toupper(c);
				mc = 0;
			} else
				mc = 0;
			*s = c;
			ws = false;
			continue;
		}

		ws = true, mc = 0;
	}			/* loop */
}				/* caseShift */

#endif

void eb_puts(const char *s)
{
#ifdef DOSLIKE
	wchar_t *chars = NULL;
	DWORD written, mode;
	HANDLE output_handle;
	int needed;
	output_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	if (GetConsoleMode(output_handle, &mode) == 0) {
		puts(s);
		return;
	}
	needed = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
	if (needed == 0) {
		return;
	}
	//Add space for the newline
	chars = (wchar_t *) allocMem(sizeof(wchar_t) * (needed + 2));
	MultiByteToWideChar(CP_UTF8, 0, s, -1, chars, needed);
	chars[needed - 1] = L'\r';
	chars[needed] = L'\n';
	chars[needed + 1] = L'\0';
	WriteConsoleW(output_handle, (void *)chars, needed + 1, &written, NULL);
	free(chars);
#else
	puts(s);
#endif

	if (debugFile)
		fprintf(debugFile, "%s\n", s);
}				/* eb_puts */

void eb_vprintf(const char *fmt, va_list args)
{
#ifdef DOSLIKE
	wchar_t *chars = NULL;
	DWORD written, mode;
	HANDLE output_handle;
	int needed;
	char *a;		// result of vasprintf
	output_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	if (GetConsoleMode(output_handle, &mode) == 0) {
		vprintf(fmt, args);
		return;
	}
	if (vasprintf(&a, fmt, args) < 0)
		return;
	needed = MultiByteToWideChar(CP_UTF8, 0, a, -1, NULL, 0);
	if (needed == 0) {
		free(a);
		return;
	}
	chars = (wchar_t *) allocMem(sizeof(wchar_t) * needed);
	MultiByteToWideChar(CP_UTF8, 0, a, -1, chars, needed);
	WriteConsoleW(output_handle, (void *)chars, needed - 1, &written, NULL);
	free(chars);
	free(a);
#else
	vprintf(fmt, args);
#endif

	if (debugFile)
		vfprintf(debugFile, fmt, args);
}				/* eb_printf */
