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
    "warning: no buffers available to handle the ancillary window",
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
    "warning: could not preload <buffer %d> with its initial text\n",
    "warning: fields without names will not be transmitted",
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
    "warning: form url specifies a section %s, which will be ignored\n",
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
    "javascript disabled, skipping the onclick code",
    "javascript disabled, skipping the onchange code",
    "warning: the url already specifies some data, which will be overwritten by the data in this form",
    "javascript disabled, skipping the onreset code",
    "javascript disabled, skipping the onsubmit code",
    "could not find the html tag associated with the javascript variable being modified",
    "javascript modified a textarea, and that isn't implemented yet",
    "warning: garbled refresh directive, %s\n",
    "warning: unrecognized http compression method %s\n",
    "warning: http redirection %d, but a new url is not specified\n",
    "warning: http error %d, %s\n",
    "warning: page does not have a recognizable http header",
    "warning: cannot update config file",
    "help messages on",
    "ftp download",
    "no ssl certificate file specified; secure connections cannot be verified",
};

static const char *frenchMessages[] = {
    "EOF - fin de fichier",
    "pas de fichier",
    "rien",
    "substitutions globales",
    "substitutions locales",
    "insensible à la casse",
    "sensible à la casse",
    "répertoires en lecture seule",
    "répertoires en lecture écriture",
    "répertoires en lecture écriture avec effacement",
    "pas de redirection http",
    "redirection http",
    "ne pas envoyer le référent",
    "envoyer le référent",
    "javascript dédactivé",
    "javascript activé",
    "binaire traité comme texte",
    "détection des fichiers binaires",
    "mode passif",
    "mode actif",
    "mode passif/actif",
    "pas de vérification des connexions ssl (moins sûr)",
    "vérification des connexions ssl",
    "ne pas afficher les fichiers cachés en mode répertoire",
    "afficher les fichiers cachés en mode répertoire",
    "fins de ligne unix dans les zones de texte",
    "fins de ligne dos dans les zones de texte",
    "marqueurs de fin de ligne désactivés",
    "marqueurs de fin de ligne activés pour les lignes listées",
    "marqueurs de fin de ligne activés",
    "javascript désactivé, aucune action lancée",
    "javascript désactivé, on va directement à l'url",
    "redirection interrompue par l'utilisateur",
    "vide",
    "nouvelle session",
    "pas de saut de ligne à la fin",
    "mode répertoire",
    "données binaires",
    "ok",
    " [binaire]",
    "pas de titre",
    "pas de description",
    "pas de mots-clés",
    "session %d\n",
    "Envoi de courrier. Tapez votre courrier, tapez sm pour l'envoyer, puis ^ pour revenir.",
    "la ligne %d a été modifiée\n",
    "pas de sujet",
    "pas de fichier de configuration",
    "pas de bloc subjfilter{...} dans le fichier de configuration .ebrc",
    "interruption, tapez qt pour quitter complètement",
    "edbrowse prêt",
    "la ligne de commande contient des caractères nuls; vous pouvez utiliser \\0 dans une chaîne de recherche/remplacement pour indiquer le caractère nul",
    "autorisation nécessaire - tapez x pour annuler.",
    "trop de redirections",
    "succès",
    "répertoire",
    "redirection vers %s retardée de %d secondes",
    "Nom d'utilisateur: ",
    "nom d'utilisateur trop long, la limite est de %d caractères",
    "Mot de passe: ",
    "nom de passe trop long, la limite est de %d caractères",
    "pas de récipient pour les cookies dans votre fichier de configuration",
    "attention, répertoire inaccessible - la liste des fichiers sera vide",
    "désastre, message d'erreur %d trop long\n",
    "ligne %d: ",
    "erreur de navigation: ",
    "Le navigateur a reçu la suite, mais il n'y a pas de tampon pour la recevoir",
    "pas d'options",
    "formulaire réinitialisé",
    "formulaire en cours de soumission",
    "Une nouvelle fenêtre a été créée dans le tampon %d\n",
    " beaucoup",
    " recommendé",
    "]",
    "aucune option ne contient la chaîne %s\n",
    "envoi du courrier à %s\n",
    "Envoi effectué; attendez une réponse par retour de courrier.",
    "indésirable",
    "quitter",
    "suivant",
    "effacement",
    "ip effacé",
    "l'option -p désactive cette fonctionnalité",
    "pas de fichier liste noire, fonctionnalité désactivée",
    "fin du message",
    "pas encore implémenté",
    "nom de fichier: ",
    "Désolé, le fichier %s existe déjà.\n",
    "abrégé ",
    "pièce jointe vide",
    "pièce jointe ",
    "plus de tampon disponible, pièce jointe ignorée",
    "impossible de copier la pièce jointe dans le tampon %d",
    "impossible de créer le fichier %s, pièce jointe ignorée\n",
    "impossible de sauvegarder le fichier %s, pièce jointe ignorée\n",
    "pas de courrier",
    "%d messages\n",
    "spam",
    " from ",
    "?\taffiche ce message d'aide.\nq\tquitte ce programme.\nx\tquitte sans rien modifier sur le serveur de courrier.\nespace\tcontinuer la lecture de ce message.\nn\taller au message suivant.\nd\teffacer ce message.\nj\ttraiter ce sujet comme indésirable pendant dix jours.\nJ\ttraiter ce sujet comme indésirable pendant un an.\nw\tsauvegarder ce message et l'effacer.\nk\tsauvegarder ce message sans l'effacer.\nu\tsauvegader ce message non formaté, puis l'effacer.",
    "création du fichier %s impossible\n",
    "sauvegarde du fichier %s impossible\n",
    "courrier sauvegardé, %d octets",
    " ajouté",
    "page terminée, utilisez la touche retour arrière ou quittez",
    "Désolé, innerText n'est pas encore implémenté",
    "pas d'erreurs",
    "authentification proxy pour une url non-proxy",
    "session %d non active",
    "session 0 invalide",
    "débordement de la session %d, la limite est de %d",
    "vous êtes déjà dans la session %d",
    "`w' attendu",
    "`w' attendu en session %d",
    "Vous avez atteint la limite d'un million de lignes.\nSauvegardez vos fichiers, puis sortez et redémarrez le programme.",
    "chemin du répertoire trop long, la limite est de %d caractères",
    "répertoires en lecture seule, tapez dw pour autoriser l'écriture",
    "impossible de créer le fichier corbeille .recycle, pour contenir vos fichiers effacés",
    "effacement du fichier %s impossible",
    "impossible de mettre %s à la corbeille, passez en mode dx pour l'effacer",
    "destination à l'intérieur du bloc à déplacer ou à copier",
    "pas de modification",
    "ne peut fusionner une seule ligne",
    "impossible de lire la base de données dans un autre fichier",
    "impossible de lire plusieurs tables dans la même session",
    "nom de fichier absent",
    "nom de domaine vide dans l'url",
    "impossible d'écrire sur une url",
    "impossible d'écrire dans une table de base de données",
    "impossible de sauvegarder un fichier vide",
    "impossible de créer %s",
    "ne peut écrire dans %s",
    "session non interactive",
    "ligne %c trop grande",
    "une commande shell ne peut contenir des caractères nuls",
    "label %c non défini",
    "délimiteur invalide",
    "chaîne de recherche non conservée",
    "chaîne de remplacement non conservée",
    "expression régulière trop grande",
    "ligne terminée par un anti-slash",
    "parenthèse fermante inattendue",
    "on ne peut remplacer que de $1 à $9",
    "le modificateur %s n'est précédé d'aucun caractère",
    "pas de crochet fermant",
    "pas de parenthèse fermante",
    "tampon vide",
    "erreur dans l'expression régulière, %s",
    "erreur inattendue en évaluant l'expression régulière %d",
    "chaîne de recherche non trouvée",
    "numéro de ligne trop grand",
    "numéro de ligne négatif",
    "pas d'expression régulière après %c",
    "délimiteur absent",
    "aucune ligne ne correspond au modèle g",
    "toutes les lignes correspondent au modèle v",
    "aucune des lignes marquées n'a pu être modifiée avec succès",
    "opération interrompue",
    "pas de correspondance",
    "pas de changements",
    "l'agent numéro %c n'est pas défini",
    "impossible d'afficher le nouveau répertoire",
    "impossible d'établir le répertoire courant",
    "répertoire invalide",
    "lecture audio impossible à partir d'un tampon vide audio",
    "lecture audio impossible en mode navigation",
    "lecture audio impossible en mode répertoire",
    "lecture audio impossible en mode base de données",
    "le fichier n'a pas d'extension, utilisez mt.xxx pour définir votre propre extension",
    "extension limitée à 5 caractères",
    "l'extension .%s n'est pas un type mime connu, vérifiez votre fichier de configuration.",
    "pas de nom de fichier ou d'url à réafficher",
    "pas en mode base de données",
    "pas en mode navigation",
    "le nom de fichier ne contient pas le caractère /",
    "le nom de fichier se termine par le caractère /",
    "commande sm suivie de caractères invalides",
    "vous devez spécifier exactement un %s après la commande B",
    "la ligne ne contient pas de %c ouvrant(e)",
    "%c et %c non équilibrés sur cette ligne, essayez B%c ou B%c",
    "la ligne ne contient pas de parenthèses, de crochets ou d'accolades non équilibrés",
    "ne peux trouver de ligne équilibrant %c",
    "session %d actuellement en mode navigation",
    "session %d actuellement en mode répertoire",
    "pas de nom de fichier",
    "fin du tampon",
    "plus de lignes à fusionner",
    "mauvais intervalle",
    "impossible de découper les lignes en mode répertoire",
    "impossible de découper les lignes en mode base de données",
    "impossible de découper les lignes en mode navigation",
    "commande %c inconnue",
    "%c impossible en mode répertoire",
    "%c impossible en mode base de données",
    "%c impossible en mode navigation",
    "ligne numéro zéro",
    "pas d'espace après la commande",
    "la commande %c ne peut s'appliquer globalement",
    "destination du déplacement ou de la copie invalide",
    "texte inattendu après la commande %c",
    "rien à défaire",
    "tapez svp k[a-z]",
    "impossible d'étiqueter un intervalle entier",
    "ne peux sauvegarder 0",
    "attention : pas de tampon pour g�rer la fen�tre auxiliaire",
    "texte inattendu après la commande q",
    "impossible de renommer le répertoire",
    "impossible de renommer la table",
    "impossible d'ajouter à un autre tampon",
    "pas de fichier spécifié",
    "ne peut mettre à jour le répertoire; les fichiers seront modifiés quand vous quitterez",
    "ne peut mettre à jour la base de données; les lignes seront modifiées quand vous quitterez",
    "texte inattendu après la commande ^",
    "pas de texte antérieur",
    "texte inattendu après la commande M",
    "destination de la session non spécifiée",
    "pas de texte antérieur, sauvegarde impossible",
    "impossible d'appliquer la commande g à un intervalle",
    "g impossible en mode base de données",
    "impossible d'appliquer la commande i%c à un intervalle",
    "impossible d'utiliser i< dans une commande globale",
    "le tampon %d est vide",
    "le tampon %d contient plus d'une ligne",
    "impossible d'ouvrir %s",
    "impossible de lire %s",
    "le texte entré contient des caractères nuls",
    "il y a un retour-chariot dans la ligne",
    "le première ligne de %s est trop longue",
    "le fichier est actuellement dans le tampon - utilisez la commande rf pour le réafficher",
    "navigation impossible dans un fichier binaire",
    "navigation impossible dans un fichier vide",
    "ce contenu ne semble pas navigable",
    "en cours de navigation",
    "label %s non trouvé",
    "i impossible en mode navigation",
    "impossible de lancer une commande d'insertion à partir d'une fonction d'edbrowse",
    "impossible de lire du texte dans une session base de données",
    "commande %c non implémentée",
    "%d hors limites",
    "aucune option ne contient la chaîne %s",
    "de multiples options contiennent la chaîne %s",
    "c'est un bouton; tapez i* pour le déclencher",
    "c'est un bouton de validation; tapez i* pour le déclencher",
    "c'est un bouton d'annulation; tapez i* pour le déclencher",
    "c'est une zone de texte, vous devez l'éditer dans la session %d",
    "champ en lecture seule",
    "une zone de saisie ne peut contenir un saut de ligne",
    "saisie trop long, la limite est de %d caractères",
    "le champ doit être défini + or -",
    "vous ne pouvez réinitialiser un bouton radio; vous devez en activer un autre",
    "%s n'est pas un fichier accessible",
    "nombre attendu",
    "la session %d contient des caractères nuls",
    "l'envoi d'un fichier exige 'method=post and enctype=multipart/form-data'",
    "c'est une zone de saisie, pas un bouton",
    "ce bouton ne fait pas partie d'un formulaire",
    "pas de javascript associé à ce bouton",
    "le formulaire n'inclut aucune URL de destination",
    "l'action de ce formulaire ne renvoie pas à une destination URL",
    "javascript désactivé, impossible d'activer ce formulaire",
    "Le formulaire est passé du mode https au mode http, il n'est plus sécurisé",
    "impossible de soumettre avec le protocole %s",
    "%d hors limites, utilisez s'il vous plait %c1 à %c%d",
    "%d hors limites, utilisez s'il vous plait  %c1, ou simplement %c",
    "impossible de remplacer plusieurs fois la chaîne vide",
    "la ligne dépasse %d caractères après la substitution",
    "pas d'expression régulière après %c",
    "nombres multiples après le troisième délimiteur",
    "suffixe de substitution inattendu après le troisième délimiteur",
    "impossible d'utiliser simultanément un suffixe numérique et le suffixe `g'",
    "désolé, impossible d'exécuter la commande bl sur les lignes excédant %d caractères",
    "les données de remplacement contiennent des sauts de ligne",
    "les données de remplacement contiennent des caractères nuls",
    "un nom de répertoire ne peut contenir un slash, un saut de ligne ou un caractère nul",
    "le fichier destination existe déjà",
    "impossible de renommer le fichier à %s",
    "une zone de saisie ne peut contenir des caractères nuls",
    "une zone de saisie ne peut contenir des sauts de ligne",
    "aucune zone de saisie présente",
    "aucun lien présent",
    "aucun bouton présent",
    "il y a plusieurs zones de saisie, utilisez svp %c1 à %c%d",
    "il y a plusieurs liens, utilisez svp %c1 à %c%d",
    "il y a plusieurs boutons, utilisez svp %c1 à %c%d",
    "impossible de lire les données sur le serveur",
    "%s n'est pas une url",
    "proxy sécurisé non implémenté",
    "impossible d'identifier %s sur le réseau",
    "le protocole %s n'est pas supporté par edbrowse, et n'est pas défini dans les types mime de votre fichier de configuration",
    "attention : impossible de pr�charger le texte initial dans le <tampon %d>\n",
    "attention : les champs d�pourvus de nom ne seront pas transmis",
    "connexion impossible à %s",
    "connexion sécurisée impossible à %s, erreur %d",
    "Le certificat de sécurité de l'hôte %s ne peut être vérifié - connexion SSL abandonnée",
    "impossible d'envoyer la requête au serveur web",
    "méthode d'autorisation %s inconnue",
    "la page web requiert une autorisation",
    "authentification abandonnée",
    "impossible de créer le fichier temporaire %s pour décompresser la page web",
    "impossible d'écrire dans le fichier temporaire %s pour décompresser la page web",
    "zcat ne peut décompresser les données pour reconstruire la page web",
    "impossible d'accéder à la page web décompressée dans %s",
    "impossible de lire la page web décompressée dans %s",
    "impossible de créer la sous-commande %s, erreur numéro %d",
    "connexion ftp impossible à l'hôte distant",
    "connexion ftp impossible à l'hôte distant (délai dépassé)",
    "échec du transfert ftp",
    "échec du transfert ftp (délai dépassé)",
    "ftp : répertoire inexistant",
    "ftp : ne peut changer de répertoire (délai dépassé)",
    "ftp : url mal formée",
    "ftp : erreur d'utilisation",
    "ftp : erreur d'authentification dans le fichier de configuration",
    "ftp : échec de l'initialisation de la librairie",
    "ftp : échec de la session d'initialisation",
    "ftp : erreur inattendue %d",
    "edbrowse n'a pas été compilé avec l'accès aux bases de données",
    "alias manquant dans le carnet d'adresses à la ligne %d",
    "manquant dans le carnet d'adresses à la ligne %d",
    "alias trop long dans le carnet d'adresses, limite 15 caractères",
    "email trop long dans le carnet d'adresses, limite 63 caractères",
    " pas de @ dans l'email à la ligne %d du carnet d'adresses",
    "espaces interdits dans un email, à la ligne %d du carnet d'adresses",
    "caractères non imprimables dans un alias ou un email, à la ligne %d du carnet d'adresses",
    "la dernière ligne de votre carnet d'adresses n'est pas terminée",
    "impossible d'envoyer les données au serveur de courrier",
    "données du serveur de courrier impossibles à lire",
    "la ligne envoyée par le serveur de courrier est trop longue, ou n'est pas close",
    "impossible de localiser le serveur de courrier %s",
    "impossible de se connecter au serveur de courrier",
    "attention : le formulaire url sp�cifie une section %s, qui sera ignor�e\n",
    "le fichier %s est vide",
    "votre mail devrait commencer par 'subject:':",
    "ligne sujet vide",
    "ligne sujet trop longue, la limite est de %d caractères",
    "caractères invalides dans la ligne sujet, n'utilisez svp que des espaces ou des caractères imprimables",
    ".signature n'est pas un fichier régulier",
    "impossible d'accéder au fichier .signature",
    "impossible d'envoyer le fichier binaire %s, serait-ce une pièce jointe ?",
    "aucune ou bien toutes les pièces jointes doivent être spécifiées \"alternative\"",
    "trop de destinataires, la limite est de %d",
    "Pas de carnet d'adresses défini, contrôlez votre fichier de configuration .ebrc",
    "impossible de trouver l'alias %s dans votre carnet d'adresses",
    "aucun destinataire spécifié",
    "la session %d est vide, impossible de joindre quelque chose",
    "impossible d'accéder à la pièce jointe %s",
    "impossible de joindre le fichier %s, ce n'est pas un fichier régulier",
    "le fichier %s est vide, impossible de le joindre",
    "invite inattendue \"%s\" au départ de la session d'envoi",
    "le serveur de courrier ne reconnait pas %s",
    "le serveur de courrier a rejeté %s",
    "le serveur de courrier n'est pas prêt à recevoir les données, %s",
    "impossible d'envoyer le courrier, %s",
    "pas de comptes de courrier définis, contrôlez votre fichier de configuration",
    "numéro de compte de courrier invalide %d, utilisez 1 à %d",
    "impossible d'envoyer du courrier en mode navigation",
    "impossible d'envoyer du courrier en mode base de données",
    "impossible d'envoyer du courrier en mode répertoire",
    "impossible d'envoyer des données binaires, serait-ce une pièce jointe ?",
    "impossible d'envoyer un fichier vide",
    "pas de destinataire à la ligne %d",
    "impossible de mettre le premier destinataire en cc ou bcc",
    "pas de pièce jointe à la ligne %d",
    "numéro de compte de courrier invalide à la ligne %d",
    "pas de sujet",
    "la ligne %d devrait débuter par to: attach: alt: account: ou subject:",
    "pas de ligne subject",
    "pas de destinataires spécifiés, placez to: adresse email au début de votre fichier",
    "%s:// attendu",
    "protocole %s non reconnu",
    ":port invalide à la fin du nom de domaine",
    "nom de domaine trop long",
    "nom d'utilisateur trop long",
    "mot de passe trop long",
    "trop de données provenant de l'internet, peut-être faudrait-il désactiver `redirect html'",
    "la page web s'appelle elle-même indirectement, c'est une boucle infinie",
    "pas de fonction spécifiée",
    "les noms de fonction ne peuvent contenir que des lettres et des chiffres",
    "fonction %s non trouvée",
    "trop d'arguments",
    "~%d n'a pas d'argument correspondant",
    "impossible de créer la sous-commande %s, erreur numéro %d",
    "impossible de créer le fichier temporaire %s, erreur numéro %d",
    "trop de tables sql dans le cache, la limite est de %d",
    "%s n'est pas un fichier régulier",
    "fichier trop grand, la limite est de 40MB",
    "impossible de lire le contenu de %s",
    "impossible d'accéder à %s",
    "la variable d'environnement %s n'est pas définie",
    "impossible de développer * ? or [] antérieur au dernier /",
    "%s n'est pas un répertoire accessible",
    "modèle shell trop long",
    "désolé, j'ignore comment développer les noms de fichiers contenant \\",
    "modèle [] mal formé",
    "erreur de compilation du modèle shell, %s",
    "erreur inattendue d'évaluation du modèle shell",
    "le modèle shell ne correspond à aucun nom de fichier",
    "le modèle shell correspond à plusieurs noms de fichier",
    "ligne trop longue après expansion des variables shell",
    "le fichier de config ne définit aucune base de données - connexion impossible",
    "connexion à la base de données impossible - erreur %d",
    "erreur sql inattendue %d",
    "pas de colonne index spécifiée",
    "nom de colonne %s trop long, la limite est de %d caractères",
    "erreur de syntaxe dans la clause where",
    "débordement dans la colonne %d",
    "plusieurs colonnes correspondent à %s",
    "aucune colonne ne correspond à %s",
    "la table %s n'existe pas",
    "nom de colonne invalide",
    "impossible de sélectionner plus d'une colonne contenant des données binaires",
    "les données contiennent le caractère pipe, réservé comme délimiteur de champ",
    "les données contiennent des sauts de ligne",
    "trop de champs dans la ligne, s'il vous plait, ne mettez pas de caractères pipe dans le texte",
    "pas assez de champs dans la ligne, s'il vous plait, ne supprimez pas de caractères pipe dans le texte",
    "colonne index non spécifiée",
    "erreur sql %d",
    "impossible de supprimer plus de 100 lignes à la fois",
    "impossible de modifier une colonne index",
    "impossible de modifier un champ de données binaires",
    "impossible de modifier un champ de texte",
    "oups!  j'ai supprimé %d ligne(s), et %d enregistrement(s) de la base de données ont été affectés.",
    "oups!  j'ai supprimé %d ligne(s), et %d enregistrement(s) de la base de données ont été affectés.",
    "oups!  j'ai supprimé %d ligne(s), et %d enregistrement(s) de la base de données ont été affectés.",
    "d'autres lignes de la base de données dépendent de cette ligne",
    "la ligne ou la table sont verrouillées",
    "vous n'avez pas la permission de modifier la base de données par ce moyen",
    "deadlock (verrou mortel) détecté",
    "placement d'un null dans une colonne non null",
    "violation de contrainte",
    "dépassement de délai de la base de données",
    "impossible de modifier une vue",
    "pas de fermeture de %s",
    "attention, javascript ne peut être invoqué par un événement clavier",
    "javascript ne peut être invoqué par un focus ou un blur",
    "attention, ce code onclick n'est pas associé à un hyperlien ou à un bouton",
    "gestionnaire onchange inaccessible",
    "%s n'appartient pas à un formulaire",
    "%s n'a pas de nom",
    "méthode inconnue, utilisez s'il vous plaît GET ou POST",
    "codage enctype non reconnu, utilisez s'il vous plaît multipart/form-data or application/x-www-form-urlencoded",
    "le formulaire ne peut être soumis en utilisant le protocole %s",
    "saisie de type inconnu %s",
    "plusieurs boutons radio ont été activés",
    "%s est inclus dans %s",
    "%s débute au milieu de %s",
    "fermeture inattendue de %s, qui n'a jamais été ouvert",
    "une zone de texte débute au milieu d'une zone de texte",
    "%s apparaît dans une ancre",
    "%s contient des balises html",
    "l'option ne peut contenir une virgule lorsqu'elle fait partie d'une sélection multiple",
    "option vide",
    "titres multiples",
    "%s n'est pas dans une liste",
    "%s n'est pas dans une table",
    "%s n'est pas dans une ligne de la table",
    "option apparaît à l'extérieur d'un ordre select",
    "de multiples options sont sélectionnées",
    "commande %d du tag non traitée",
    "%s non fermé à la fin du fichier",
    "java ouvre une fenêtre vide",
    "caractères inattendus après la pièce jointe encodée",
    "caractères invalides dans la pièce jointe encodée",
    "le gestionnaire handler ne fonctionne pas avec les zones de texte",
    "attention, javascript ne peut être invoqué par un double clic",
    "erreur de détermination de l'option %s pendant la synchronisation ou la soumission du formulaire",
    "une balise ne peut simultanément gérer onunload and onclik",
    "le script n'est pas clos en fin de fichier, javascript est suspendu",
    "impossible de charger localement javascript, %s",
    "impossible de charger javascript depuis %s, code %d",
    "impossible de charger javascript, %s",
    "javascript désactivé, code onclick ignoré",
    "javascript désactivé, code onchange ignoré",
    "attention : l'url contient d�j� des donn�es, qui seront �cras�es par les donn�es de ce formulaire",
    "javascript désactivé, code onreset ignoré",
    "javascript désactivé, code onsubmit ignoré",
    "impossible de trouver la balise html associée à la variable javascript modifiée",
    "javascript a modifié une zone de texte, et ce n'est pas encore implémenté",
    "attention : directive de rafra�chissement alt�r�e, %s\n",
    "attention : m�thode %s de compression http inconnue\n",
    "attention: redirection http %d, mais la nouvelle url n'a pas �t� sp�cifi�e\n",
    "attention : erreur html %d, %s\n",
    "attention : la page n'a pas d'en-t�te http reconnaissable",
    "attention : impossible de modifier le fichier de configuration",
    "messages d'aide activ�s",
    "t�l�chargement ftp",
    "pas de fichier de certificats ssl; les connexions s�curis�es ne pourront �tre v�rifi�es",
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
	s = "spurious message";
    return s;
}				/* getString */

/*********************************************************************
Internationalize the standard puts and printf.
These are simple informational messages, where you don't need to error out,
or check the debug level, or store the error in a buffer.
The i_ prefix means international.
*********************************************************************/

void
i_puts(int msg)
{
    puts(getString(msg));
} /* i_puts */ void
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
This converts anything that might reasonably be a letter, such as � and �.
This routine is used by default; even if the language is English.
After all, you might be a native English speaker, using edbrowse
in English, but you are writing a document in French.
This (unfortunately) does not affect \w in regular expressions,
which is still restricted to English letters.
Even more annoying, \b uses English as boundary,
so that \bbar\b will indeed match on the line foo�bar.
There may be a way to set locale in libpcre; I don't know.
Anyways, here are the nonascii letters, upper and lower.
*********************************************************************/

static const char upperMore[] = "���������������������������������";

static const char lowerMore[] = "���������������������������������";

static const char letterMore[] =
   "������������������������������ީ��������������������������������";
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
