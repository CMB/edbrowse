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
    "javascript disabled, no action taken",
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
    "proxy authentication for a non-proxy url",
    "session %d is not active",
    "session 0 is invalid",
    "session %d is out of bounds, limit %d",
    "you are already in session %d",
    "expecting `w'",
    "expecting `w' on session %d",
    "Your limit of 1 million lines has been reached.\nSave your files, then exit and restart this program.",
    "absolute path name too long, limit %d chars",
    "directories are readonly, type dw to enable directory writes",
    "could not create .recycle under your home directory, to hold the deleted files",
    "could not remove file %s",
    "Could not move %s to the recycle bin, set dx mode to actually remove the file",
    "destination lies inside the block to be moved or copied",
    "no change",
    "cannot join one line",
    "cannot read from the database into another file",
    "cannot read different tables into the same session",
    "missing file name",
    "empty domain in url",
    "cannot write to a url",
    "cannot write to a database table",
    "cannot write an empty file",
    "cannot create %s",
    "cannot write to %s",
    "session is not interactive",
    "line %c is out of range",
    "cannot embed nulls in a shell command",
    "label %c not set",
    "invalid delimiter",
    "no remembered search string",
    "no remembered replacement string",
    "regular expression too long",
    "line ends in backslash",
    "Unexpected closing )",
    "replacement string can only use $1 through $9",
    "%s modifier has no preceding character",
    "no closing ]",
    "no closing )",
    "empty buffer",
    "error in regular expression, %s",
    "unexpected error while evaluating the regular expression at line %d",
    "search string not found",
    "line number too large",
    "negative line number",
    "no regular expression after %c",
    "missing delimiter",
    "no lines match the g pattern",
    "all lines match the v pattern",
    "none of the marked lines were successfully modified",
    "operation interrupted",
    "no match",
    "no changes",
    "agent number %c is not defined",
    "could not print new directory",
    "could not establish current directory",
    "invalid directory",
    "cannot play audio from an empty buffer",
    "cannot play audio in browse mode",
    "cannot play audio in directory mode",
    "cannot play audio in database mode",
    "file has no suffix, use mt.xxx to specify your own suffix",
    "suffix is limited to 5 characters",
    "suffix .%s is not a recognized mime type, please check your config file.",
    "no file name or url to refresh",
    "not in database mode",
    "not in browse mode",
    "file name does not contain /",
    "file name ends in /",
    "invalid characters after the sm command",
    "you must specify exactly one of %s after the B command",
    "line does not contain an open %c",
    "both %c and %c are unbalanced on this line, try B%c or B%c",
    "line does not contain an unbalanced brace, parenthesis, or bracket",
    "cannot find the line that balances %c",
    "session %d is currently in browse mode",
    "session %d is currently in directory mode",
    "no file name",
    "end of buffer",
    "no more lines to join",
    "bad range",
    "cannot break lines in directory mode",
    "cannot break lines in database mode",
    "cannot break lines in browse mode",
    "unknown command %c",
    "%c not available in directory mode",
    "%c not available in database mode",
    "%c not available in browse mode",
    "zero line number",
    "no space after command",
    "the %c command cannot be applied globally",
    "invalid move/copy destination",
    "unexpected text after the %c command",
    "nothing to undo",
    "please enter k[a-z]",
    "cannot label an entire range",
    "backing up 0 is invalid",
    "spurious",
    "unexpected text after the q command",
    "cannot change the name of a directory",
    "cannot change the name of a table",
    "cannot append to another buffer",
    "no file specified",
    "cannot write to the directory; files are modified as you go",
    "cannot write to the database; rows are modified as you go",
    "unexpected text after the ^ command",
    "no previous text",
    "unexpected text after the M command",
    "destination session not specified",
    "no previous text, cannot back up",
    "cannot apply the g command to a range",
    "g not available in database mode",
    "cannot apply the i%c command to a range",
    "cannot use i< in a global command",
    "buffer %d is empty",
    "buffer %d contains more than one line",
    "cannot open %s",
    "cannot read from %s",
    "input text contains nulls",
    "line contains an embeded carriage return",
    "first line of %s is too long",
    "file is currently in buffer - please use the rf command to refresh",
    "cannot browse a binary file",
    "cannot browse an empty file",
    "this doesn't look like browsable text",
    "already browsing",
    "label %s is not found",
    "i not available in browse mode",
    "cannot run an insert command from an edbrowse function",
    "cannot read text into a database session",
    "command %c not yet implemented",
    "%d is out of range",
    "no options contain the string %s",
    "multiple options contain the string %s",
    "this is a button; use i* to push the button",
    "this is a submit button; use i* to push the button",
    "this is a reset button; use i* to push the button",
    "this is a textarea, you must edit it from session %d",
    "readonly field",
    "input field cannot contain a newline character",
    "input too long, limit %d characters",
    "field must be set to + or -",
    "you cannot clear a radio button; you must set another one",
    "%s is not an accessible file",
    "number expected",
    "session %d contains nulls",
    "sending a file requires method=post and enctype=multipart/form-data",
    "this is an input field, not a button",
    "this button is not part of a form",
    "no javascript associated with this button",
    "form does not include a destination url",
    "the action of the form is not a url",
    "javascript is disabled, cannot activate this form",
    "This form has changed from https to http, and is now insecure",
    "cannot submit using protocol %s",
    "%d is out of range, please use %c1 through %c%d",
    "%d is out of range, please use %c1, or simply %c",
    "cannot replace multiple instances of the empty string",
    "line exceeds %d bytes after substitution",
    "no regular expression after %c",
    "multiple numbers after the third delimiter",
    "unexpected substitution suffix after the third delimiter",
    "cannot use both a numeric suffix and the `g' suffix simultaneously",
    "sorry, cannot apply the bl command to lines longer than %d bytes",
    "replacement data contains newlines",
    "replacement data contains null characters",
    "cannot embed slash, newline, or null in a directory name",
    "destination file already exists",
    "cannot rename file to %s",
    "cannot embed nulls in an input field",
    "cannot embed newlines in an input field",
    "no input fields present",
    "no links present",
    "no buttons present",
    "multiple input fields present, please use %c1 through %c%d",
    "multiple links present, please use %c1 through %c%d",
    "multiple buttons present, please use %c1 through %c%d",
    "could not read the data from the server",
    "%s is not a url",
    "secure proxy not yet implemented",
    "cannot identify %s on the network",
    "the %s protocol is not supported by edbrowse, and is not included in the mime types in your config file",
    "spurious",
    "spurious",
    "cannot connect to %s",
    "cannot establish a secure connection to %s, error %d",
    "The certificate for host %s could not be verified - SSL connection aborted",
    "could not send the request to the web server",
    "authorization method %s not recognized",
    "web page requires authorization",
    "login aborted",
    "cannot create temp file %s to uncompress the web page",
    "cannot write to temp file %s to uncompress the web page",
    "zcat cannot uncompress the data to reconstruct the web page",
    "cannot access the uncompressed web page in %s",
    "cannot read the uncompressed web page in %s",
    "could not spawn subcommand %s, errno %d",
    "ftp could not connect to remote host",
    "ftp could not connect to remote host (timed out)",
    "ftp transfer failed",
    "ftp transfer failed (timed out)",
    "ftp no such directory",
    "ftp could not change directory (timed out)",
    "ftp malformed url",
    "ftp usage error",
    "ftp error in login config file",
    "ftp library initialization failed",
    "ftp session initialization failed",
    "ftp unexpected error %d",
    "edbrowse was not compiled with database access",
    "missing alias in address book line %d",
    "missing : in address book line %d",
    "alias in address book line %d is too long, limit 15 characters",
    "email in address book line %d is too long, limit 63 characters",
    " email in address book line %d does not have an @",
    "cannot handle whitespace in email, address book line %d",
    "unprintable characters in your alias or email, address book line %d",
    "last line of your address book is not terminated",
    "cannot send data to the mail server",
    "cannot read data from the mail server",
    "line from the mail server is too long, or unterminated",
    "cannot locate the mail server %s",
    "cannot connect to the mail server",
    "spurious",
    "file %s is empty",
    "your email should begin with subject:",
    "empty subject line",
    "subject line too long, limit %d characters",
    "invalid characters in the subject line, please use only spaces and printable ascii text",
    ".signature is not a regular file",
    "cannot access .signature file",
    "cannot mail the binary file %s, perhaps this should be an attachment?",
    "either none or all of the attachments must be declared \"alternative\"",
    "too many recipients, limit %d",
    "No address book specified, please check your .ebrc config file",
    "alias %s not found in your address book",
    "no recipients specified",
    "session %d is empty, cannot atach",
    "cannot access attachment %s",
    "file %s is not a regular file, cannot attach",
    "file %s is empty, cannot attach",
    "unexpected prompt \"%s\" at the start of the sendmail session",
    "mail server doesn't recognize %s",
    "mail server rejected %s",
    "mail server is not ready to receive data, %s",
    "could not send mail message, %s",
    "no mail accounts specified, plese check your config file",
    "invalid account number %d, please use 1 through %d",
    "cannot send mail while in browse mode",
    "cannot send mail while in database mode",
    "cannot send mail from directory mode",
    "cannot mail binary data, should this be an attachment?",
    "cannot mail an empty file",
    "no recipient at line %d",
    "cannot cc or bcc to the first recipient",
    "no attachment at line %d",
    "invalid account number at line %d",
    "empty subject",
    "line %d should begin with to: attach: alt: account: or subject:",
    "there is no subject line",
    "no recipients specified, place to: emailaddress at the top of youre file",
    "%s:// expected",
    "unrecognized protocol %s",
    "invalid :port specifier at the end of the domain",
    "domain name too long",
    "user name too long",
    "password too long",
    "too many fetches from the internet, you may want to disable `redirect html'",
    "web page indirectly fetches itself, an infinite loop",
    "no function specified",
    "function name should only contain letters and numbers",
    "no such function %s",
    "too many arguments",
    "~%d has no corresponding argument",
    "could not spawn subcommand %s, errno %d",
    "could not create temp file %s, errno %d",
    "too many sql tables in cache, limit %d",
    "%s is not a regular file",
    "file is too large, limit 40MB",
    "cannot read the contents of %s",
    "cannot access %s",
    "environement variable %s not set",
    "cannot expand * ? or [] prior to the last /",
    "%s is not an accessible directory",
    "shell pattern is too long",
    "sorry, I don't know how to expand filenames with \\ in them",
    "improperly formed [] pattern",
    "error compiling the shell pattern, %s",
    "unexpected error while evaluating the shell pattern",
    "shell pattern does not match any files",
    "shell pattern matches more than one file",
    "line becomes too long when shell variables are expanded",
    "the config file does not specify the database - cannot connect",
    "cannot connect to the database - error %d",
    "unexpected sql error %d",
    "no key column specified",
    "column name %s is too long, limit %d characters",
    "syntax error in where clause",
    "column %d is out of range",
    "multiple columns match %s",
    "no column matches %s",
    "no such table %s",
    "invalid column name",
    "cannot select more than one blob column",
    "the data contains pipes, which is my reserved delimiter",
    "the data contains newlines",
    "line contains too many fields, please do not introduce any pipes into the text",
    "line contains too few fields, please do not remove any pipes from the text",
    "key column not specified",
    "miscelaneous sql error %d",
    "cannot delete more than 100 rows at a time",
    "cannot change a key column",
    "cannot change a blob field",
    "cannot change a text field",
    "oops!  I deleted %d row(s), and %d database record(s) were affected.",
    "oops!  I inserted %d row(s), and %d database record(s) were affected.",
    "oops!  I updated %d row(s), and %d database record(s) were affected.",
    "some other row in the database depends on this row",
    "row or table is locked",
    "you do not have permission to modify the database in this way",
    "deadlock detected",
    "placing null into a not-null column",
    "check constraint violated",
    "database timeout",
    "cannot modify a view",
    "no closing %s",
    "warning, javascript cannot be invoked through keystroke events",
    "javascript cannot be invoked through focus or blur",
    "warning, onclick code is not associated with a hyperlink or button",
    "onchange handler is not accessible",
    "%s is not part of a fill-out form",
    "%s does not have a name",
    "unrecognized method, plese use GET or POST",
    "unrecognized enctype, plese use multipart/form-data or application/x-www-form-urlencoded",
    "form cannot submit using protocol %s",
    "unrecognized input type %s",
    "multiple radio buttons have been selected",
    "%s is closed inside %s",
    "%s begins in the middle of %s",
    "an unexpected closure of %s, which was never opened",
    "a text area begins in the middle of a text area",
    "%s appears inside an anchor",
    "%s contains html tags",
    "option cannot contain a comma when it is part of a multiple select",
    "empty option",
    "multiple titles",
    "%s is not inside a list",
    "%s is not inside a table",
    "%s is not inside a table row",
    "option appears outside a select statement",
    "multiple options are selected",
    "unprocessed tag action %d",
    "%s is not closed at eof",
    "java is opening a blank window",
    "unexpected characters after the encoded attachment",
    "invalid characters in the encoded attachment",
    "onchange handler does not work with textarea",
    "warning, javascript cannot be invoked by a double click",
    "error resolving option %s during sync or form submit",
    "tag cannot have onunload and onclik handlers simultaneously",
    "script is not closed at eof, suspending javascript",
    "could not fetch local javascript, %s",
    "could not fetch javascript from %s, code %d",
    "could not fetch javascript, %s",
    "spurious",
    "javascript disabled, skipping the onchange code",
    "javascript disabled, no action taken",
    "javascript disabled, skipping the onreset code",
    "javascript disabled, skipping the onsubmit code",
    "could not find the html tag associated with the javascript variable being modified",
    "javascript modified a textarea, and that isn't implemented yet",
};

static const char *frenchMessages[] = {
    "EOF",
};

/* English by default */
static const char **messageArray = englishMessages;
static int messageArrayLength = sizeof (englishMessages) / sizeof (char *);

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
	messageArrayLength = sizeof (frenchMessages) / sizeof (char *);
	return;
    }

    errorPrint("1Sorry, language %s is not emplemented", s);
}				/* selectLanguage */

static const char *
getString(int msg)
{
    const char **a = messageArray;
    const char *s;
    if(msg >= messageArrayLength)
	a = englishMessages;
    s = a[msg];
    if(!s)
	s = englishMessages[msg];
    if(!s)
	errorPrint("@printing null message %d", msg);
    return s;
}				/* getString */

/*********************************************************************
Internationalize the standard puts and printf.
These are for typical informational messages, where you don't need to
error out, or check the debug level, or store the error in a buffer.
The i_ prefix means international.
*********************************************************************/

void
i_puts(int msg)
{
    puts(getString(msg));
}				/* i_puts */

void
i_printf(int msg, ...)
{
    const char *realmsg = getString(msg);
    va_list p;
    va_start(p, msg);
    vprintf(realmsg, p);
    va_end(p);
}				/* i_printf */

/*********************************************************************
The following error display functions are specific to edbrowse,
rather than extended versions of the standard unix print functions.
Thus I don't need the i_ prefix.
*********************************************************************/

char errorMsg[4000];
/* Show the error message, not just the question mark, after these commands. */
static const char showerror_cmd[] = "AbefMqrw^";

/* Set the error message.  Type h to see the message. */
void
setError(int msg, ...)
{
    va_list p;

    if(msg < 0) {
	errorMsg[0] = 0;
	return;
    }

    va_start(p, msg);
    vsprintf(errorMsg, getString(msg), p);
    va_end(p);

/* sanity check */
    if(strlen(errorMsg) >= sizeof (errorMsg)) {
	i_printf(63, strlen(errorMsg));
	puts(errorMsg);
	exit(1);
    }
}				/* setError */

void
showError(void)
{
    if(errorMsg[0])
	puts(errorMsg);
    else
	i_puts(106);
}				/* showError */

void
showErrorConditional(char cmd)
{
    if(helpMessagesOn || strchr(showerror_cmd, cmd))
	showError();
    else
	printf("?\n");
}				/* showErrorConditional */

void
showErrorAbort(void)
{
    errorPrint("1%s", errorMsg);
}				/* showErrorAbort */

void
browseError(int msg, ...)
{
    va_list p;
    if(ismc)
	return;
    if(browseLocal != 1)
	return;
    if(browseLine) {
	i_printf(64, browseLine);
	cw->labels[4] = browseLine;
    } else
	i_printf(65);
    va_start(p, msg);
    vprintf(getString(msg), p);
    va_end(p);
    nl();
    browseLocal = 2;
}				/* browseError */

/* Javascript errors, we need to see these no matter what. */
void
runningError(int msg, ...)
{
    va_list p;
    if(ismc)
	return;
    if(browseLine) {
	i_printf(64, browseLine);
	cw->labels[4] = browseLine;
    }
    va_start(p, msg);
    vprintf(getString(msg), p);
    va_end(p);
    nl();
    browseLocal = 2;
}				/* runningError */


/*********************************************************************
Now for the international version of caseShift.
This converts anything that might reasonably be a letter, such as » and Ë.
This routine is used by default; even if the language is English.
After all, you might be a native English speaker, using edbrowse
in English, but you are writing a document in French.
This (unfortunately) does not affect \w in regular expressions,
which is still restricted to English letters.
Even more annoying, \b uses English as boundary,
so that \bbar\b will indeed match on the line foo»bar.
There may be a way to set locale in libpcre; I don't know.
Anyways, here are the nonascii letters, upper and lower.
*********************************************************************/

static const char upperMore[] = "©¿¡¬√ƒ≈∆«»… ÀÃÕŒœ–—“”‘’÷ÿŸ⁄€‹›ﬁﬂˇ";

static const char lowerMore[] = "©‡·‚„‰ÂÊÁËÈÍÎÏÌÓÔÒÚÛÙıˆ¯˘˙˚¸˝˛ﬂˇ";

static const char letterMore[] =
   "©¿¡¬√ƒ≈∆«»… ÀÃÕŒœ–—“”‘’÷ÿŸ⁄€‹›ﬁ©‡·‚„‰ÂÊÁËÈÍÎÏÌÓÔÒÚÛÙıˆ¯˘˙˚¸˝˛ﬂˇ";
static bool
i_isalphaByte(unsigned char c)
{
    if(isalphaByte(c))
	return true;
    if(c == false)
	return 0;
    if(strchr(letterMore, c))
	return true;
    return false;
}				/* i_isalphaByte */

/* assumes the arg is a letter */
static unsigned char
i_tolower(unsigned char c)
{
    char *s;
    if(isalphaByte(c))
	return tolower(c);
    s = strchr(upperMore, c);
    if(s)
	c = lowerMore[s - upperMore];
    return c;
}				/* i_tolower */

static unsigned char
i_toupper(unsigned char c)
{
    char *s;
    if(isalphaByte(c))
	return toupper(c);
    s = strchr(lowerMore, c);
    if(s)
	c = upperMore[s - lowerMore];
    return c;
}				/* i_toupper */

/* This is a variation on the original routine, found in stringfile.c */
void
i_caseShift(unsigned char *s, char action)
{
    unsigned char c;
/* The McDonalds conversion is very English - should we do it in all languages? */
    int mc = 0;
    bool ws = true;

    for(; c = *s; ++s) {
	if(action == 'u') {
	    if(i_isalphaByte(c))
		*s = i_toupper(c);
	    continue;
	}

	if(action == 'l') {
	    if(i_isalphaByte(c))
		*s = i_tolower(c);
	    continue;
	}

/* mixed case left */
	if(i_isalphaByte(c)) {
	    if(ws)
		c = i_toupper(c);
	    else
		c = i_tolower(c);
	    if(ws && c == 'M')
		mc = 1;
	    else if(mc == 1 && c == 'c')
		mc = 2;
	    else if(mc == 2) {
		c = i_toupper(c);
		mc = 0;
	    } else
		mc = 0;
	    *s = c;
	    ws = false;
	    continue;
	}

	ws = true, mc = 0;
    }				/* loop */
}				/* caseShift */
