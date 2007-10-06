/* messages.c
 * Error, warning, and info messages, in your host language,
 * as determined by the variable $LANG.
 * Copyright (c) Karl Dahlke, 2007
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"

/* Arrays of messages, one array for each language. */
static const char *englishMessages[] = {
    "EOF",
    "no file",
    "none",
    "substitutions global",
    "substitutions local",
    "case insensitive",
    "case sensitive",
    "directories readonly",
    "directories writable",
    "directories writable with delete",
    "no http redirection",
    "http redirection",
    "do not send referrer",
    "send referrer",
    "javascript disabled",
    "javascript enabled",
    "treating binary like text",
    "watching for binary files",
    "passive mode",
    "active mode",
    "passive/active mode",
    "don't verify ssl connections (less secure)",
    "verify ssl connections",
    "don't show hidden files in directory mode",
    "show hidden files in directory mode",
    "text areas use unix newlines",
    "text areas use dos newlines",
    "end markers off",
    "end markers on listed lines",
    "end markers on",
    "javascript is disabled, no action taken",
    "javascript is disabled, going straight to the url",
    "redirection interrupted by user",
    "empty",
    "new session",
    "no trailing newline",
    "directory mode",
    "binary data",
    "ok",
    " [binary]",
    "no title",
    "no description",
    "no keywords",
    "session %d\n",
    "SendMail link.  Compose your mail, type sm to send, then ^ to get back.",
    "line %d has been updated\n",
};

static const char *frenchMessages[] = {
};

/* English by default */
static const char **messageArray = englishMessages;

void
selectLanguage(void)
{
    char *s = getenv("LANG");
    if(!s)
	return;
    if(!*s)
	return;
    if(stringEqual(s, "en"))
	return;			/* english is default */
    if(stringEqual(s, "eng"))
	return;			/* english is default */
    if(stringEqual(s, "fr")) {
	messageArray = frenchMessages;
	return;
    }
    errorPrint("1Sorry, language %s is not emplemented", s);
}				/* selectLanguage */

/*********************************************************************
Internationalize the standard puts and printf.
These are for typical informational messages, where you don't need to
error out, or check the debug level, or store the error in a buffer.
The i_ prefix means international.
*********************************************************************/

void
i_puts(int msg)
{
    puts(messageArray[msg]);
}				/* i_puts */

void
i_printf(int msg, ...)
{
    va_list p;
    long a[5];
    const char *realmsg = messageArray[msg];
    va_start(p, msg);
    varargLocals(p, realmsg, a);
    va_end(p);

    printf(realmsg, a[0], a[1], a[2], a[3], a[4]);
}				/* i_printf */
