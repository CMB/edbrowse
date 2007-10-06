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
    "no subject",
    "no config file present",
    ".ebrc config file does not contain a subjfilter{...} block",
    "interrupt, type qt to quit completely",
    "edbrowse ready",
    "entered command line contains nulls; you can use \\0 in a search/replace string to indicate null",
    "authorization requested - type x to abort.",
    "too many redirections",
    "success",
    "directory",
    "redirect to %s delayed by %d seconds",
    "Username: ",
    "user name too long, limit %d characters",
    "Password: ",
    "password too long, limit %d characters",
    "your config file does not specify a cooky jar",
    "warning, the directory is inaccessible - the listing will be empty",
    "disaster, error message length %d is too long\n",
    "line %d: ",
    "browse error: ",
    "The following has been sent to your browser, but there is no buffer to contain it.",
    "no options",
    "form has been reset",
    "submitting form",
    "A separate window has been created in buffer %d\n",
    " many",
    " recommended",
    "]",
    "no options contain the string %s\n",
    "sending mail to %s\n",
    "Your information has been sent; watch for a reply via return mail.",
    "junk",
    "quit",
    "next",
    "delete",
    "ip delete",
    "-p option disables this feature",
    "no blacklist file specified, feature disabled",
    "end of message",
    "not yet implemented",
    "filename: ",
    "Sorry, file %s already exists.\n",
    "abbreviated ",
    "empty attachment",
    "attachment ",
    "no buffers left, atachment discarded",
    "could not copy the attachment into buffer %d",
    "cannot create file %s, attachment is discarded\n",
    "cannot write to file %s, attachment is discarded\n",
    "no mail",
    "%d messages\n",
    "spam",
    " from ",
    "?\tprint this help message.\nq\tquit this program.\nx\texit without changing anything on the mail server.\nspace\tread more of this mail message.\nn\tmove on to the next mail message.\nd\tdelete this message.\nj\tjunk this subject for ten days.\nJ\tjunk this subject for a year.\nw\twrite this message to a file and delete it.\nk\tkeep this message in a file, but don't delete it.\nu\twrite this message unformatted to a file, and delete it.",
    "cannot create file %s\n",
    "cannot write to file %s\n",
    "mail save, %d bytes",
    " appended",
    "this page is finished, please use your back key or quit",
    "Sorry, innerText update not yet implemented.",
    "no errors",
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
    const char *realmsg = messageArray[msg];
    va_list p;
    va_start(p, msg);
    vprintf(realmsg, p);
    va_end(p);
}				/* i_printf */
