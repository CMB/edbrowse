/* messages.c
 * Error, warning, and info messages in your host language,
 * as determined by the variable $LANG.
 * Messages can be generated in iso-8859-1, but utf8 is recommended.
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"

#include <locale.h>

/* English by default */
static const char **messageArray;
char eb_language[8];
int eb_lang;
/* startup .ebrc files in various languages */
const char *ebrc_string;
static const char *qrg_string;
bool cons_utf8, iuConvert = true;
char type8859 = 1;
bool helpMessagesOn;
bool errorExit;

void selectLanguage(void)
{
	char *s = getenv("LANG");	// This is likely to fail in windows
	char *dot;

// default English
	strcpy(eb_language, "en");
	eb_lang = 1;
	messageArray = msg_en;
	ebrc_string = ebrc_en;
	qrg_string = qrg_en;

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

/* But LC_TIME controls time/date formatting, I.E., strftime.  The one
 * place we do that, we need standard day/month abbreviations, not
 * localized ones.  So LC_TIME needs to be C. */
	setlocale(LC_TIME, "C");
#else // DOSLIKE

/* I'm going to assume Windows runs utf8 */
	cons_utf8 = true;

	if (!s)
		s = setlocale(LC_ALL, "");
	if (!s)
		return;
	if (!*s)
		return;
	setlocale(LC_TIME, "C");
#endif // DOSLIKE y/n

	strncpy(eb_language, s, 7);
	eb_language[7] = 0;
	caseShift(eb_language, 'l');
	dot = strchr(eb_language, '.');
	if (dot)
		*dot = 0;

	if (!strncmp(eb_language, "en", 2))
		return;		/* english is already the default */

	if (!strncmp(eb_language, "fr", 2)) {
		eb_lang = 2;
		messageArray = msg_fr;
		ebrc_string = ebrc_fr;
		qrg_string = qrg_fr;
		type8859 = 1;
		return;
	}

	if (!strncmp(eb_language, "pt_br", 5)) {
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
		type8859 = 5;
		return;
	}

	if (!strncmp(eb_language, "it", 2)) {
		eb_lang = 7;
		messageArray = msg_it;
		type8859 = 1;
		return;
	}

/* This error is really annoying if it pops up every time you invoke edbrowse.
	fprintf(stderr, "Sorry, language %s is not implemented\n", buf);
*/
}				/* selectLanguage */

/*********************************************************************
WARNING: this routine, which is at the heart of the international prints
i_puts i_printf, is not threadsafe in iso8859 mode.
Well utf8 has been the default console standard for 15 years now,
and I'm almost ready to chuck iso8859 altogether, so for now,
let's just say you can't use threading in 8859 mode.
If you try to turn it on via the bg (background) command, I won't let you.
I really don't think this will come up, everybody is utf8 by now.
*********************************************************************/

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
	utf2iso((uchar *) s, strlen(s), (uchar **) & t, &t_len);
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

static void eb_vprintf(const char *fmt, va_list args);

void i_printf(int msg, ...)
{
	const char *realmsg = i_getString(msg);
	va_list p;
	va_start(p, msg);
	eb_vprintf(realmsg, p);
	va_end(p);
	if (debugFile) {
		va_start(p, msg);
		vfprintf(debugFile, realmsg, p);
		va_end(p);
	}
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
	if (debugFile) {
		va_start(p, msg);
		vfprintf(debugFile, realmsg, p);
		fprintf(debugFile, "\n");
		va_end(p);
	}
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

char errorMsg[1024];

/* Show the error message, not just the question mark, after these commands. */
static const char showerror_cmd[] = "AbefMqrw^&";

/* Set the error message.  Type h to see the message. */
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
	if (vasprintf(&a, i_getString(msg), p) < 0)
		i_printfExit(MSG_MemAllocError, 4096);
	va_end(p);
// If the error message is crazy long, truncate it.
	l = sizeof(errorMsg) - 1;
	strncpy(errorMsg, a, l);
	nzFree(a);
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

static void eb_vprintf(const char *fmt, va_list args)
{
#ifdef DOSLIKE
	wchar_t *chars = NULL;
	DWORD written, mode;
	HANDLE output_handle;
	int needed;
	char *a;		// result of vasprintf
	output_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	if (GetConsoleMode(output_handle, &mode) == 0) {
// this is better than doing nothing.
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
}				/* eb_vprintf */

bool helpUtility(void)
{
	int cx;

	if (!cxQuit(context, 0))
		return false;

// maybe we already have a buffer with the help guide in it
	for (cx = 1; cx < MAXSESSION; ++cx) {
		struct ebWindow *w = sessionList[cx].lw;
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
	browseCurrentBuffer();
	cw->dot = 1;
	return true;
}
