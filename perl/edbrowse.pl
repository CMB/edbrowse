#!/usr/bin/perl

#  edbrowse: line editor/browser

use IO::Handle;
use IO::Socket;
use Time::Local;

=head1 Author

	Karl Dahlke
	eklhad@comcast.net
	248-524-1004 (during regular business hours)
	http://www.eklhad.net/linux/app

=head1 Copyright Notice

This program is copyright (C) (C) Karl Dahlke, 2000-2003.
It is made available, by the author, under the terms of the General Public License (GPL),
as articulated by the Free Software Foundation.
It may be used for any purpose, and redistributed,
provided this copyright notice is included.

=head1 Redirection

This program, and its associated documentation, are becoming quite large.
Therefore the documentation has been moved to a separate html file.
Please visit:

http://www.eklhad.net/linux/app/edbdoc.html

If you have lynx on hand, you can run:

lynx -dump http://www.eklhad.net/linux/app/edbdoc.html > edbdoc.txt

If you are using lynx to download the actual program, do this:

lynx -source www.eklhad.net/linux/app/edbrowse >edbrowse

=cut


$version = "1.5.17";
@agents = ("edbrowse/$version");
$agent = $agents[0];


#  It's tempting to let perl establish the global variables as you go.
#  Let's try not to do this.
#  That's where all the side effects are - that's where the bugs come in.
#  Below are the global variables, with some explanations.

$debug = 0;  # general debugging
$errorExit = 0;
$ismc = 0;  # is mail client
$zapmail = 0;  # just get rid of the mail
$maxfile = 40000000;  # Max size of an editable file.
$eol = "\r\n";  # end-of-line, as far as http is concerned
$doslike = 0;  # Is it a Dos-like OS?
$doslike = 1 if $^O =~ /^(dos|win|mswin)/i;
$errorMsg = "";  # Set this if the last operation produced an error.
$inglob = 0;  # Are we in global mode, under a g// operation?
$onloadSubmit = 0;
$inscript = 0;  # plowing through javascript
$filesize = 0;  # size of file just read or written
$global_lhs_rhs = 0;  # remember lhs and rhs across sessions
$caseInsensitive = 0;
#  Do we send crnl or nl after the lines in a text buffer?
#  What is the standard - I think it's DOS newlines.
$textAreaCR = 1;
$pdf_convert = 1;  # convert pdf to html
$fetchFrames = 1;  # fetch the frames into a web page
$allsub = 0;  # enclose all superscripts and subscripts
$allowCookies = 1; # allow all cookies.
%cookies = ();  # the in-memory cookie jar
%authHist = ();  # authorization strings by domain
$authAttempt = 0;  # count authorization attempts for this page
$ssl_verify = 1; # By default we verify all certs.
$ssl = undef;  # ssl connection
$ctx = undef;  # ssl certificate
$allowReferer = 1; # Allow referer header by default.
$referer = "";  # refering web page
$reroute = 1;  # follow http redirections to find the actual web page
$rerouteCount = 0;  # but prevent infinite loops
%didFrame = ();  # which frames have we fetched already
$passive = 1; # ftp passive mode on by default.
$nostack = 0;  # suppress stacking of edit sessions
$last_z = 1;  # line count for the z command
$endmarks = 0;  # do we print ^ $ at the start and end of lines?
$subprint = 0;  # print lines after substitutions?
$delprint = 0;  # print line after delete
$dw = 0;  # directory write enabled
$altattach = 0;  # attachments are really alternative presentations of the same email
$do_input = 0;  # waiting for the next input from the tty
$intFlag = 0;  # control c was hit
$intMsg = "operation interrupted";

#  Interrupt handler, for control C.
#  Close file handle if we were reading from disk or socket.
sub intHandler()
{
$intFlag = 1;
if($do_input) {
print "\ninterrupt, type qt to quit completely\n";
return;
}
#  Reading from an http server.
close FH if defined FH;
# Kill ftp data connection if open.
close FDFH if defined FDFH;
#  and mail connection or ftp control connection
close SERVER_FH if defined SERVER_FH;
# And listening ftp socket.
close FLFH if defined FLFH;
exit 1 if $ismc;
}  # intHandler

$SIG{INT} = \&intHandler;

#  A quieter form of die, without the edbrowse line number, which just confuses people.
sub dieq($)
{
my $msg = shift;
print "fatal: $msg\n";
exit 1;
}  # dieq

@weekDaysShort = ("Sun","Mon","Tue","Wed","Thu","Fri","Sat");
@monthsShort = ("Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec");
sub mailTimeString()
{
my ($ss, $nn, $hh, $dd, $mm, $yy, $wd) = localtime time;
my $wds = $weekDaysShort[$wd];
my $mths = $monthsShort[$mm];
return sprintf "%s, %02d %s %d %02d:%02d:%02d",
$wds, $dd, $mths, $yy+1900, $hh, $nn, $ss;
}  # mailTimeString

#  ubackup is set when the command has changed something.
#  The previous text, stored in the save_* variables,
#  is copied into the last* variables.  If you type u,
#  the last* variables and current variables are swapped.
$ubackup = 0;

#  Did we successfully read the edbrowse config file?
#  If so, set some variables.
$myname = $annoyFile = $junkFile = $addressFile = "";
%adbook = ();
$adbooktime = 0;
@inmailserver = ();  # list of pop3 servers
$mailDir = "";
$localMail = -1;
$whichMail = 0;  # which account to use
$smMail = "";
$naccounts = 0;  # number of pop accounts
$outmailserver = "";  # smtp
$smtplogin = "";  # smtp login
my $mailToSend = "";
@pop3login = ();
@pop3password = ();
@replyAddress = ();
@fromSource = ();
@fromDest = ();
$serverLine = "";  # line received from mail or ftp server

#  web express configuration variables and arrays.
%shortcut = ();
%commandList = ();
%commandCheck = ();
$currentShortcut = "";
$currentCommandList = "";

#  Specify the start and end of a range for an operation.
#  1,3m5 will set these variables to 1, 3, and 5.
$startRange = $endRange = $dest = 0;

#  The input command, but only the one-letter commands.
$icmd = "";
#  Now the command that is actually executed is in $cmd.
#  This is usually the same as $icmd, but not always.
#  8i becomes 7a, for instance.
$cmd = "";
#  The valid edbrowse commands.
$valid_cmd = "aAbBcdefghHiIjJklmnpqrsStuvwz=^@<";
#  Commands that can be done in browse mode.
$browse_cmd = "AbBdefghHIjJklmnpqsuvwz=^@<";
#  Commands for directory mode.
$dir_cmd = "AbdefghHklnpqsvwz=^@<";
#  Commands that work at line number 0, in an empty file.
$zero_cmd = "aAbefhHqruw=^@<";
#  Commands that expect a space afterward.
$spaceplus_cmd = "befrw";
#  Commands that should have no text after them.
$nofollow_cmd = "aAcdhHijlmnptu=";
#  Commands that can be done after a g// global directive.
$global_cmd = "dIjJlmnpst";
#  Show the error message, not just the question mark, after these commands.
$showerror_cmd = "Abefqrw^@";
$helpall = 0;  #  show the error message all the time

#  Remember that two successive q's will quit the session without changes.
#  here we must track which session, by number, you were trying to quit.
$lastq = $lastqq = -1;

#  For any variable x, there are usually multiple copies of x, one per session.
#  These are housed in an array @x.
#  In contrast, the variable $x holds $x[$context],
#  according to the current context.
#  I hope this isn't too confusing.
$context = 0;

#  dot and dol, current and last line numbers.
@dot = (0);
$dot = $dot[0];
@dol = (0);
$dol = $dol[0];
@factive = (1);  # which sessions are active
#  Retain file names, and whether the text has been modified.
@fname = ("");
$fname = $fname[0];
$baseref = "";  # usually the same as $fname
@fmode = (0);  # file modes
$fmode = $fmode[0];
$binmode = 1;  # binary file
$nlmode = 2;  # newline apended
$browsemode = 4;  # browsing html text
$changemode = 8;  # something has changed in this file
$dirmode = 16;  # directory mode
$firstopmode = 32;  # first operation issued - undo is possible
$nobrowse = "not in browse mode";  # common error message
$nixbrowse = "command not available in browse mode";
$nixdir = "command not available in directory mode";

sub dirBrowseCheck($)
{
my $cmd = shift;
$fmode&$browsemode and $errorMsg = "$cmd $nixbrowse", $inglob = 0, return 0;
$fmode&$dirmode and $errorMsg = "$cmd $nixdir", $inglob = 0, return 0;
return 1;
}  # dirBrowseCheck

#  retain base directory name when scanning a directory
@dirname = ("");
$dirname = $dirname[0];

#  Remember substitution strings.
@savelhs = ();  # save left hand side
$savelhs = $savelhs[0];
@saverhs = ();  # save right hand side
$saverhs = $saverhs[0];

#  month hash, to encode dates.
%monhash =
(jan => 1, feb => 2, mar => 3, apr => 4, may => 5, jun => 6,
jul => 7, aug => 8, sep => 9, oct => 10, nov => 11, dec => 12);

$home = $ENV{HOME};
defined $home and length $home or
dieq 'home directory not defined by $HOME.';
-d $home or
dieq "$home is not a directory.";

#  Establish the trash bin, for deleted files.
$rbin = "$home/.trash";
if(! -d $rbin) {
$rbin = "" unless mkdir $rbin, 0700;
}
#  Config file for this browser.
#  Sample file is available at http://www.eklhad.net/linux/app/sample.perl.ebrc
$rcFile = "$home/.ebrc";
#  Last http header, normally deleted before you read the web page.
$ebhttp = "$rbin/eb.http";
truncate $ebhttp, 0;
#  When we need a temp file.
$ebtmp = "$rbin/eb.tmp";
# A file containing SSL certificates in PEM format, concatinated together.
# This will be used for certificate verification.
$ebcerts = "$home/.ssl-certs";
#  file for persistant cookies.
$ebcooks = "$home/.cookies";
sub fillJar() ;
fillJar();  # fill up that cooky jar

#  Let's see if we can read the config file?
if(open FH, $rcFile) {
my $sort = 0;
while(<FH>) {
s/^\s+//;
s/^#.*$//;
next if /^$/;
s/\s+$//;
my ($server, $login, $passwd, $retpath, $key, $value);
if(/^([^:\s]+)\s*:\s*([^:\s]+)\s*:\s*([^:\s]+)\s*:\s*([^:\s]+)\s*:\s*([^:\s]*)/) {
($server, $login, $passwd, $retpath) = ($1, $2, $3, $4);
my $smtpbox = $5;
if($server =~ s/^\*\s*//) {
dieq "multiple accounts are marked as local, with a star." if $localMail >= 0;
$localMail = $naccounts;
$smtpbox = $server unless length $smtpbox;
$outmailserver = $smtpbox;
$smtplogin = $login;
}
$inmailserver[$naccounts] = $server;
$pop3login[$naccounts] = $login;
$pop3password[$naccounts] = $passwd;
$replyAddress[$naccounts] = $retpath;
++$naccounts;
next;
}  # describing a mail server

#  Now look form keyword = string.
#  Initial < is shorthand for cmd =
s/^\</cmd =/;
if(/^([^=]+)=\s*(.+)/) {
$key = $1;
$value = $2;
$key =~ s/\s+$//;
$myname = $value, next if $key eq "fullname";
$addressFile = $value, next if $key eq "addressbook";
$junkFile = $value, next if $key eq "junkfile";
$annoyFile = $value, next if $key eq "annoyfile";
$mailDir = $value, next if $key eq "cd";

if($key eq "from") {
if($value =~ /^\s*([^\s>]+)\s*>\s*(.+)$/) {
push @fromSource, lc $1;
push @fromDest, $2;
next;
}
dieq "from filter \"$value\" does not look like \"emailAddress > file\".";
}  # from

if($key eq "agent") {
push @agents, $value;
next;
}  # agent

#  web express keywords
if($key eq "shortcut") {
if(length $currentShortcut and ! defined $shortcut{$currentShortcut}{url}) {
dieq "shortcut $currentShortcut has not been assigned a url";
}
$value =~ /^[\w-]+$/ or dieq "the name of a shortcut must consist of letters digits or dashes, $value is invalid";
$currentShortcut = $value;
#  Start out with no post processing commands.
$shortcut{$value}{after} = [];
$shortcut{$value}{sort} = sprintf "%04d", $sort;
++$sort;
$currentCommandList = "";
next;
}  # shortcut
if($key eq "cmdlist") {
if(length $currentShortcut and ! defined $shortcut{$currentShortcut}{url}) {
dieq "shortcut $currentShortcut has not been assigned a url";
}
$currentShortcut = "";
my $check = 0;
$check = 1 if $value =~ s/^\+//;
$value =~ /^[\w-]+$/ or dieq "the name of a command list must consist of letters digits or dashes, $value is invalid.";
$currentCommandList = $value;
$commandList{$value} = [];
$commandCheck{$value} = $check;
next;
}  # cmdlist
if($key eq "cmd") {
length $currentShortcut or length $currentCommandList or
dieq "postprocessing command is not part of a command list or shortcut";
my $cref;  # command reference
$cref = $shortcut{$currentShortcut}{after} if length $currentShortcut;
$cref = $commandList{$currentCommandList} if length $currentCommandList;
#  is this a command list?
if($value =~ /^[a-zA-Z_-]+$/ and defined $commandList{$value}) {
my $cpush = $commandList{$value};
push @$cref, @$cpush;
} else {
push @$cref, $value;
}
next;
}  # cmd
if($key eq "url") {
length $currentShortcut or dieq "$key command without a current shortcut";
$shortcut{$currentShortcut}{url} = $value;
next;
}  # url
if($key eq "desc") {
length $currentShortcut or dieq "$key command without a current shortcut";
$shortcut{$currentShortcut}{desc} = $value;
next;
}  # desc

dieq "Unrecognized keyword <$key> in config file.";
}

dieq "garbled line <$_> in config file.";
}  # loop over lines in config file
close FH;

if(length $currentShortcut and ! defined $shortcut{$currentShortcut}{url}) {
dieq "shortcut $currentShortcut has not been assigned a url";
}

if($naccounts) {
$localMail = 0 if $naccounts == 1;
dieq "None of the pop3 accounts is marked as local." if $localMail < 0;
dieq "fullname not specified in the config file." if ! length $myname;
}  # mail accounts
}  # open succeeded

#  One array holds all the lines of text (without the newlines)
#  for all the files in all the sessions.
#  Within a given session, the actual file is represented by a list of numbers,
#  indexes into this large array.
#  Note that when text is copied, we actually copy the strings in the array.
#  I could just have different lines use the same index, thus pointing to the
#  same string, and there would be no need to copy that string,
#  but then I'd have to maintain reference counts on all these strings,
#  and that would make the program very messy!
@text = ();

#  If a file has 30 lines, it is represented by 30 numbers,
#  indexes into @text above.
#  Should we use an array of numbers, or a string of numbers
#  represented by decimal digits?
#  Both are painful, in different ways.
#  Consider inserting a block of text, a very common operation.
#  In a list, we would have to slide all the following numbers down.
#  Granted, that's better than copying all those lines of text down,
#  but it's still a pain to program, and somewhat inefficient.
#  If we use strings, we take the original string of numbers,
#  break it at the insert point, and make a new string
#  by concatenating these two pieces with the new block.
#  The same issues arise when deleting text near the top of a file.
#  This and other considerations push me towards strings.
#  I currently use 6 characters for a line number, and a seventh for the g// flag.
$lnwidth = 7;  # width of a line number field in $map
$lnwidth1 = $lnwidth - 1;
$lnformat = "%6d ";
$lnspace = ' ' x $lnwidth;
$lnmax = 999999;
#  Note that line 0 never maps to anything in @text.
@map = ($lnspace);
$map = $map[0];
#  The 26 labels, corresponding to the lower case letters.
#  These are stored in a packed string, like $map above.
#  labels also holds the filetype suffixes when in directory mode.
@labels = ($lnspace x 26);
$labels = $labels[0];
# offset into $labels, where directory suffixes begin.
$dirSufStart = 26 * $lnwidth;

#  The anchor/form/input tags, for browsing.
#  The browse tags are in an array of hashes.
#  Each hash has tag=tagname,
#  and attrib=value for each attrib=value in the tag.
#  Be advised that certain special tags, such as those defining
#  title and description and keywords, are placed in btag[0].
@btags = ();
$btags = $btags[0];

#  When we focus on an input field, for edit or manipulation,
#  we need its type, size, and list of options.
$inf = "";  # current text displayed by this input field.
$itype = "";  # Type of the input field.
$isize = 0;  # size of the input field.
$iopt = {};  # hash of input options in a discrete list.
$irows = $icols = 0;  # for a text area window.
$iwrap = "";  # Can we scroll beyond this window?
$itag = undef;  # the input tag from which the previous variables were derived.
$iline = 0;  # line where this input field was found.
$ifield = 0;  # field number, within the line, the nth input field on the line.
$itagnum = 0;  # tag number for this input field.
$inorange = "this input directive cannot be applied to a range of lines";
$inoglobal = "this input directive cannot be applied globally";

#  last* and save* variables mirror the variables that define your session.
#  This supports the undo command.
$lastdot = $savedot = $lastdol = $savedol = 0;
$lastmap = $savemap = $lastlabels = $savelabels = "";

#  Variables to format text, i.e. break lines at sentence/phrase boundaries.
$refbuf = "";  #  The new, reformatted buffer.
$lineno = $colno = 0;  # line/column number
$optimalLine = 80;  # optimal line length
$cutLineAfter = 36;  # cut sentence or phrase after this column
$paraLine = 120;  # longer lines are assumed to be self-contained paragraphs
$longcut = 0;  # last cut of a long line
$lspace = 3;  # last space value, 3 = paragraph
$lperiod = $lcomma = $lright = $lany = 0;  # columns for various punctuations
$idxperiod = $idxcomma = $idxright = $idxany = 0;

#  Push the entire edit session onto a stack, for the back key.
#  A hash will hold all the variables that make a session,
#  such as $map, $fname, $btags, etc.
@backup = ();
$backup = $backup[0];

$hexChars = "0123456789abcdefABCDEF";

#  Valid delimiters for search/substitute.
#  note that \ is conspicuously absent, not a valid delimiter.
#  I alsso avoid nestable delimiters such as parentheses.
#  And no alphanumerics please -- too confusing.
$valid_delim = "-_=!|#*;:`\"',./?+@";

#  $linePending holds a line of text that you accidentally typed in
#  while edbrowse was in command mode.
#  When you see the question mark, immediately type a+ to recover the line.
$linePending = undef;


#  That's it for the globals, here comes the code.
#  First a few support routines.
#  Strip white space from either side.
sub stripWhite($)
{
my $line = shift;
$$line =~ s/^\s+//;
$$line =~ s/\s+$//;
}  # stripWhite

#  Is a filename a URL?
#  If it is, return the transport protocol, e.g. http.
sub is_url($)
{
my $line = shift;
return 'http' if $line =~ m,^http://[^\s],i;
return 'https' if $line =~ m,^https://[^\s],i;
return 'gopher' if $line =~ m,^gopher://[^\s],i;
return 'telnet' if $line =~ m,^telnet://[^\s],i;
return 'ftp' if $line =~ m,^ftp://[^\s],i;
# I assume that the following will be regular http.
#  Strip off the ?this=that stuff
$line =~ s:\?.*::;
#  Strip off the file name and .browse suffix.
$line =~ s:/.*::;
$line =~ s/\.browse$//;
$line =~ s/:\d+$//;
return 0 if $line !~ /\w\.\w.*\w\.\w/;  # we need at least two internal dots
#  Look for an ip address, four numbers and three dots.
return 'http' if $line =~ /^\d+\.\d+\.\d+\.\d+$/;
$line =~ s/.*\.//;
return 'http' if index(".com.biz.info.net.org.gov.edu.us.uk.au.ca.de.jp.be.nz.sg.", ".$line.") >= 0;
}  # is_url

#  Apply a (possibly) relative path to a preexisting url.
#  The new url is returned.
#  resolveUrl("http://www.eklhad.net/linux/index.html", "app") returns
#  "http://www.eklhad.net/linux/app"
sub resolveUrl($$)
{
my ($line, $href) = @_;
my $scheme;
$line = "" unless defined $line;
$line =~ s/\.browse$//;
#  debug print - this is a very subtle routine.
print "resolve($line, $href)\n" if $debug >= 2;
#  Some people, or generators, actually write http://../whatever.html
$href =~ s/^http:(\.+)/$1/i;
$href =~ s,^http://(\.*/),$1,i;
return $href unless length $href and length $line and ! is_url($href);
if(substr($href, 0, 1) ne '/') {
$line =~ s/\?.*//;  # hope this is right
if(substr($href, 0, 1) ne '?') {
if($line =~ s,^/[^/]*$,, or
$line =~ s,([^/])/[^/]*$,$1,) {
#  We stripped off the last directory
$line .= '/';
} else {
if($scheme = is_url $line) {
$line .= '/';
} else {
$line = "";
}
}  # stripping off last directory
}  # doesn't start with ?
} elsif($scheme = is_url $line) {
#  Keep the scheme and server, lose the filename
$line =~ s/\?.*//;  # hope this is right
$line =~ s,^($scheme://[^/]*)/.*,$1,i;
} else {
$line = "";
}
return $line.$href;
}  # resolveUrl

#  Prepare a string for http transmition.
#  No, I really don't know which characters to encode.
#  I'm probably encoding more than I need to -- hope that's ok.
sub urlEncode($)
{
$_ = shift;
s/([^-\w .@])/sprintf('%%%02X',ord($1))/ge;
y/ /+/;
return $_;
}  # urlEncode

sub urlDecode($)
{
$_ = shift;
y/+/ /;
s/%([0-9a-fA-F]{2})/chr hex "$1"/ge;
return $_;
}  # urlDecode

#  The javascript unescape function, sort of
sub unescape($)
{
$_ = shift;
s/(%|\\u00)([0-9a-fA-F]{2})/chr hex "$2"/ge;
s/&#(\d+);/chr "$1"/ge;
return $_;
}  # unescape

#  Pull the subject out of a sendmail url.
sub urlSubject($)
{
my $href = shift;
if($$href =~ s/\?(.*)$//) {
my @pieces = split '&', $1;
foreach my $j (@pieces) {
next unless $j =~ s/^subject=//i;
my $subj = urlDecode $j;
stripWhite \$subj;
return $subj;
}  # loop
}  # attributes after the email
return "";
}  # urlSubject

#  Get raw text ready for html display.
sub textUnmeta($)
{
my $tbuf = shift;
return unless length $$tbuf;
$$tbuf =~ s/&/&amp;/g;
$$tbuf =~ s/</&lt;/g;
$$tbuf =~ s/>/&gt;/g;
$$tbuf =~ s/^/<P><PRE>/;
$$tbuf =~ s/$/<\/PRE><P>\n/;
}  # textUnmeta

#  Derive the alt description for an image or hyperlink.
sub deriveAlt($$)
{
my $h = shift;
my $href = shift;
my $alt = $$h{alt};
$alt = "" unless defined $alt;
stripWhite \$alt;
#  Some alt descriptions are flat-out useless.
$alt =~ s/^[^\w]+$//;
return $alt if length $alt;
if(!length $href) {
$href = $$h{href};
$href = "" unless defined $href;
}
$alt = $href;
$alt =~ s/^javascript.*$//i;
$alt =~ s/^\?//;
$alt =~ s:\?.*::s;
$alt =~ s:.*/::;
$alt =~ s/\.[^.]*$//;
$alt =~ s:/$::;
return $alt;
}  # deriveAlt

#  Pull the reference out of a javascript openWindow() call.
$foundFunc = "";
sub javaWindow($)
{
my $jc = shift;  # java call
my $page = "";
$foundFunc = "";
$page = $1 if $jc =~ /(?:open|location|window)[\w.]* *[(=] *["']([\w._\/:,=@&?+-]+)["']/i;
return $page if length $page;
return "submit" if $jc =~ /\bsubmit *\(/i;
while($jc =~ /(\w+) *\(/g) {
my $f = $1;
my $href = $$btags[0]{fw}{$f};
if($href) {
$href =~ s/^\*//;
$foundFunc = $f;
$page = $href;
}
}
return $page;
}  # javaWindow

#  Try to find the Java functions
sub javaFunctions($)
{
my $tbuf = shift;
my $flc = 0;  # function line count
my $f;  # java function
while($$tbuf =~ /(.+)/g) {
my $line = $1;
if($line =~ /function *(\w+)\(/) {
$f = $1;
print "java function $f\n" if $debug >= 6;
$flc = 1;
}
my $win = javaWindow $line;
if(length $win) {
if($flc) {
if(not defined $$btags[0]{fw}{$f}) {
$$btags[0]{fw}{$f} = "*$win";
print "$f: $win\n" if $debug >= 3;
}
} elsif($win ne "submit") {
my $h = {};
push @$btags, $h;
$attrhidden = hideNumber($#$btags);
$$h{ofs1} = length $refbuf;
my $alt = deriveAlt($h, $win);
$alt = "relocate" unless length $alt;
createHyperLink($h, $win, $alt);
}
}
next unless $flc;
++$flc;
$flc = 0 if $flc == 12;
}  # loop over lines
}  # javaFunctions

#  Mixed case.
sub mixCase($)
{
my $w = lc shift;
$w =~ s/\b([a-z])/uc $1/ge;
#  special McDonald code
$w =~ s/Mc([a-z])/"Mc".uc $1/ge;
return $w;
}  # mixCase

#  Create a hyperlink where there was none before.
sub createHyperLink($$$)
{
my ($h, $href, $desc) = @_;
$$h{tag} = "a";
$$h{bref} = $baseref;
$$h{href} = $href;
$refbuf .= "\x80$attrhidden" . "{$desc}";
$colno += 2 + length $desc;
$$h{ofs2} = length $refbuf;
$lspace = 0;
}  # createHyperLink

#  meta html characters.
#  There's lots more -- this is just a starter.
%charmap = (
#  Normal ascii symbols
gt => '>', lt => '<', quot => '"',
plus => '+', minus => '-', colon => ':',
apos => '`', star => '*', comma => ',',
period => '.', dot => ".",
dollar => '$', percnt => '%', amp => '&',
#  International letters
ntilde => "\xf1", Ntilde => "\xd1",
agrave => "\xe0", Agrave => "\xc0",
egrave => "\xe8", Egrave => "\xc8",
igrave => "\xec", Igrave => "\xcc",
ograve => "\xf2", Ograve => "\xd2",
ugrave => "\xf9", Ugrave => "\xd9",
auml => "\xe4", Auml => "\xc4",
euml => "\xeb", Euml => "\xcb",
iuml => "\xef", Iuml => "\xcf",
ouml => "\xf6", Ouml => "\xd6",
uuml => "\xfc", Uuml => "\xdc",
yuml => "\xff", Yuml => 'Y',
aacute => "\xe1", Aacute => "\xc1",
eacute => "\xe9", Eacute => "\xc9",
iacute => "\xed", Iacute => "\xcd",
oacute => "\xf3", Oacute => "\xd3",
uacute => "\xfa", Uacute => "\xda",
yacute => "\xfd", Yacute => "\xdd",
atilde => "\xe3", Atilde => "\xc3",
itilde => 'i', Itilde => 'I',
otilde => "\xf5", Otilde => "\xd5",
utilde => 'u', Utilde => 'U',
acirc => "\xe2", Acirc => "\xc2",
ecirc => "\xea", Ecirc => "\xca",
icirc => "\xee", Icirc => "\xce",
ocirc => "\xf4", Ocirc => "\xd4",
ucirc => "\xfb", Ucirc => "\xdb",
#  Other 8-bit symbols.
#  I turn these into their 8 bit equivalents,
#  then a follow-on routine turns them into words for easy reading.
#  Some speech adapters do this as well, saying "cents" for the cents sign,
#  but yours may not, so I do some of these translations for you.
#  But not here, because some people put the 8-bit cents sign in directly,
#  rather then &cent;, so I've got to do that translation later.
pound => "\xa3", cent => "\xa2",
sdot => "\xb7",
middot => "\xb7",
edot => 'e',
nbsp => ' ',
times => "\xd7",
divide => "\xf7",
deg => "\xb0",
frac14 => "\xbc",
half => "\xbd",
frac34 => "\xbe",
frac13 => "1/3",
frac23 => "2/3",
copy => "\xa9",
reg => "\xae",
trade => "(TM)",
);

%symbolmap = (
a => "945",
b => "946",
g => "947",
d => "948",
e => "949",
z => "950",
h => "951",
q => "952",
i => "953",
k => "954",
l => "955",
m => "956",
n => "957",
x => "958",
o => "959",
p => "960",
r => "961",
s => "963",
t => "964",
u => "965",
f => "966",
c => "967",
y => "968",
w => "969",
177 => "8177",  # kludge!!  I made up 8177
198 => "8709",
219 => "8660",
209 => "8711",
229 => "8721",
206 => "8712",
207 => "8713",
242 => "8747",
192 => "8501",
172 => "8592",
174 => "8594",
165 => "8734",
199 => "8745",
200 => "8746",
64 => "8773",
182 => "8706",
185 => "8800",
162 => "8242",
163 => "8804",
179 => "8805",
204 => "8834",
205 => "8838",
201 => "8835",
203 => "8836",
202 => "8839",
208 => "8736",
);

#  map certain font=symbol characters to words
%symbolWord = (
176 => "degrees",
188 => "1fourth",
189 => "1half",
190 => "3fourths",
215 => "times",
247 => "divided by",
913 => "Alpha",
914 => "Beta",
915 => "Gamma",
916 => "Delta",
917 => "Epsilon",
918 => "Zeta",
919 => "Eta",
920 => "Theta",
921 => "Iota",
922 => "Kappa",
923 => "Lambda",
924 => "Mu",
925 => "Nu",
926 => "Xi",
927 => "Omicron",
928 => "Pi",
929 => "Rho",
931 => "Sigma",
932 => "Tau",
933 => "Upsilon",
934 => "Phi",
935 => "Chi",
936 => "Psi",
937 => "Omega",
945 => "alpha",
946 => "beta",
947 => "gamma",
948 => "delta",
949 => "epsilon",
950 => "zeta",
951 => "eta",
952 => "theta",
953 => "iota",
954 => "kappa",
955 => "lambda",
956 => "mu",
957 => "nu",
958 => "xi",
959 => "omicron",
960 => "pi",
961 => "rho",
962 => "sigmaf",
963 => "sigma",
964 => "tau",
965 => "upsilon",
966 => "phi",
967 => "chi",
968 => "psi",
969 => "omega",
8177 => "+-",  # kludge!!  I made up 8177
8242 => "prime",
8501 => "aleph",
8592 => "left arrow",
8594 => "arrow",
8660 => "double arrow",
8706 => "d",
8709 => "empty set",
8711 => "del",
8712 => "member of",
8713 => "not a member of",
8721 => "sum",
8734 => "infinity",
8736 => "angle",
8745 => "intersect",
8746 => "union",
8747 => "integral",
8773 => "congruent to",
8800 => "not equal",
8804 => "less equal",
8805 => "greater equal",
8834 => "proper subset of",
8835 => "proper superset of",
8836 => "not a subset of",
8838 => "subset of",
8839 => "superset of",
);

#  Map an html meta character using the above hashes.
#  Usually run from within a global substitute.
sub metaChar($)
{
my $meta = shift;
if($meta =~ /^#(\d+)$/) {
return chr $1 if $1 <= 255;
return "'" if $1 == 8217;
return "\x82$1#" if $symbolWord{$1};
return "?";
}
my $real = $charmap{$meta};
defined $real or $real = "?";
return $real;
}  # metaChar

#  Translate <font face=symbol>number</font>.
#  This is highly specific to my web pages - doesn't work in general!
sub metaSymbol($)
{
my $meta = shift;
$meta =~ s/^&#//;
$meta =~ s/;$//;
my $real = $symbolmap{$meta};
return "?" unless $real;
return "&#$real;";
}  # metaSymbol

#  replace VAR with $VAR, as defined by the environment.
sub envVar($)
{
my $var = shift;
my $newvar = $ENV{$var};
if(defined $newvar) {
#  There shouldn't be any whitespace at the front or back.
stripWhite \$newvar;
return $newvar if length $newvar;
}
length $errorMsg or
$errorMsg = "environment variable $var not set";
return "";
}  # envVar

#  Replace the variables in a line, using the above.
sub envLine($)
{
my $line = shift;
$errorMsg = "";
#  $errorMsg will be set if something goes wrong.
$line =~ s,^~/,\$HOME/,;
$line =~ s/\$([a-zA-Z]\w*)/envVar($1)/ge;
return $line;
}  # envLine

#  The filename can be specified using environment variables,
#  and shell meta characters such as *.
#  But not if it's a url.
sub envFile($)
{
my $filename = shift;
$errorMsg = "";
if(! is_url($filename)) {
$filename = envLine($filename);
return if length $errorMsg;
my @filelist;
#  This is real kludgy - I just don't understand how glob works.
if($filename =~ / / and $filename !~ /"/) {
@filelist = glob '"'.$filename.'"';
} else {
@filelist = glob $filename;
}
$filelist[0] = $filename if $#filelist < 0;
$errorMsg = "wild card expansion produces multiple files" if $#filelist;
$filename = $filelist[0];
}
return $filename;
}  # envFile

#  Drop any active edit sessions that have no text, and no associated file.
#  This housecleaning routine is run on every quit or backup command.
sub dropEmptyBuffers()
{
foreach my $cx (0..$#factive) {
next if $cx == $context;
next unless $factive[$cx];
next if length $fname[$cx];
next if $dol[$cx];
$factive[$cx] = undef;
}
}  # dropEmptyBuffers

#  Several small functions to switch between contexts, i.e. editing sessions.
#  In all these functions, we have to map between our context numbers,
#  that start with 0, and the user's session numbers, that start with 1.
#  C and fortran programmers will be use to this problem.
#  Is a context different from the currently running context?
sub cxCompare($)
{
my $cx = shift;
$errorMsg = "session 0 is invalid", return 0 if $cx < 0;
return 1 if $cx != $context;  # ok
++$cx;
$errorMsg = "you are already in session $cx";
return 0;
}  # cxCompare

#  Is a context active?
sub cxActive($)
{
my $cx = shift;
return 1 if $factive[$cx];
++$cx;
$errorMsg = "session $cx is not active";
return 0;
}  # cxActive

#  Switch to another editing session.
#  This assumes cxCompare has succeeded - we're moving to a different context.
#  Pass the context number and an interactive flag.
sub cxSwitch($$)
{
my ($cx, $ia) = @_;
#  Put the variables in a known start state if this is a virgin session.
cxReset($cx, 0) if ! defined $factive[$cx];
$dot[$context] = $dot, $dot = $dot[$cx];
$dol[$context] = $dol, $dol = $dol[$cx];
$fname[$context] = $fname, $fname = $fname[$cx];
$dirname[$context] = $dirname, $dirname = $dirname[$cx];
$map[$context] = $map, $map = $map[$cx];
$labels[$context] = $labels, $labels = $labels[$cx];
$btags = $btags[$cx];
$backup[$context] = $backup, $backup = $backup[$cx];
if(!$global_lhs_rhs) {
$savelhs[$context] = $savelhs, $savelhs = $savelhs[$cx];
$saverhs[$context] = $saverhs, $saverhs = $saverhs[$cx];
}
$fmode[$context] = $fmode, $fmode = $fmode[$cx];
#  But we don't replicate the last* variables per context,
#  so your ability to undo is destroyed if you switch contexts.
$fmode &= ~$firstopmode;
if($ia) {
if(defined $factive[$cx]) {
print ((length($fname[$cx]) ? $fname[$cx] : "no file")."\n");
} else {
print "new session\n";
}
}
$factive[$cx] = 1;
$context = $cx;
return 1;
}  # cxSwitch

#  Can we trash the data in a context?
#  If so, trash it, and reset all the variables.
#  The second parameter is a close directive.
#  If nonzero, we clear out empty buffers associated with
#  text areas in the fill-out forms (browse mode).
#  A value of 1, as opposed to 2, means close down the entire session.
sub cxReset($$)
{
my ($cx, $close) = @_;

if(defined $factive[$cx]) {
#  We might be trashing data, make sure that's ok.
$fname[$cx] = $fname, $fmode[$cx] = $fmode if $cx == $context;
if($fmode[$cx]&$changemode and
!( $fmode[$cx]&$dirmode) and
$lastq != $cx and
length $fname[$cx] and
! is_url($fname[$cx])) {
$errorMsg = "expecting `w'";
$lastqq = $cx;
if($cx != $context) {
++$cx;
$errorMsg .= " on session $cx";
}
return 0;
}  # warning message

if($close) {
dropEmptyBuffers();
if($close&1) {
#  And we're closing this session.
$factive[$cx] = undef;
$backup[$cx] = undef;
}
}
}  # session was active

#  reset the variables
$dot[$cx] = $dol[$cx] = 0;
$map[$cx] = $lnspace;
$fname[$cx] = "";
$dirname[$cx] = "";
$labels[$cx] = $lnspace x 26;
$btags[$cx] = [];
$savelhs[$cx] = $saverhs[$cx] = undef;
$fmode[$cx] = 0;
if($cx == $context) {
$dot = $dol = 0;
$map = $map[$cx];
$fname = "";
$labels = $labels[$cx];
$btags = $btags[$cx];
$global_lhs_rhs or $savelhs = $saverhs = undef;
$fmode = 0;
}  # current context

return 1;
}  # cxReset

#  Pack all the information about the current context into a hash.
#  This will be pushed onto a virtual stack.
#  When you enter the back key, it all gets unpacked again,
#  to restore your session.
sub cxPack()
{
my $h = {
dot =>$dot, dol => $dol, map => $map, labels => $labels,
lastdot =>$lastdot, lastdol => $lastdol, lastmap => $lastmap, lastlabels => $lastlabels,
fname => $fname, dirname => $dirname,
fmode => $fmode&~$changemode,
savelhs => $savelhs, saverhs => $saverhs,
btags => $btags,
};
return $h;
}  # cxPack

sub cxUnpack($)
{
my $h = shift;
return if ! defined $h;
$dot = $$h{dot};
$lastdot = $$h{lastdot};
$dol = $$h{dol};
$lastdol = $$h{lastdol};
$map = $$h{map};
$lastmap = $$h{lastmap};
$labels = $$h{labels};
$lastlabels = $$h{lastlabels};
$fmode = $$h{fmode};
$fname = $$h{fname};
$dirname = $$h{dirname};
if(!$global_lhs_rhs) {
$savelhs = $$h{savelhs};
$saverhs = $$h{saverhs};
}
$btags[$context] = $btags = $$h{btags};
}  # cxUnpack

#  find an available session and load it with some initial data.
#  Returns the context number.
sub cxCreate($$)
{
my ($text_ptr, $filename) = @_;
#  Look for an unused buffer
my ($cx, $j);
for($cx=0; $cx<=$#factive; ++$cx) {
last unless defined $factive[$cx];
}
cxReset($cx, 0);
$factive[$cx] = 1;
$fname[$cx] = $filename;
my $bincount = $$text_ptr =~ y/\0\x80-\xff/\0\x80-\xff/;
if($bincount*4 - 10 < length $$text_ptr) {
#  A text file - remove crlf in the dos world.
$$text_ptr =~ s/\r\n/\n/g if $doslike;
} else {
$fmode[$cx] |= $binmode;
}
$fmode[$cx] |= $nlmode unless $$text_ptr =~ s/\n$//;
$j = $#text;
if(length $$text_ptr) {
push @text, split "\n", $$text_ptr, -1;
}
if(!lineLimit(0)) {
my $newpiece = $lnspace;
++$dol[$cx], $newpiece .= sprintf($lnformat, $j) while ++$j <= $#text;
$map[$cx] = $newpiece;
$dot[$cx] = $dol[$cx];
} else {
warn $errorMsg;
}
return $cx;
}  # cxCreate

#  See if @text is too big.
#  Pass the number of lines we will be adding.
sub lineLimit($)
{
my $more = shift;
return 0 if $#text + $more <= $lnmax;
$errorMsg = "Your limit of 1 million lines has been reached.\nSave your files, then exit and restart this program.";
return 1;
}  # lineLimit

#  Hide and reveal numbers that are internal to the line.
#  These numbers indicate links and input fields, and are not displayed by the next routine.
sub hideNumber($)
{
my $n = shift;
$n =~ y/0-9/\x85-\x8f/;
return $n;
} # hideNumber

sub revealNumber($)
{
my $n = shift;
$n =~ y/\x85-\x8f/0-9/;
return $n;
} # revealNumber

sub removeHiddenNumbers($)
{
my $t = shift;
$$t =~ s/\x80[\x85-\x8f]+([<>{])/$1/g;
$$t =~ s/\x80[\x85-\x8f]+\*//g;
}  # removeHiddenNumbers

#  Small helper function to retrieve the text for line number n.
#  If the second parameter is set, hidden numbers are left in place;
#  otherwise they are stripped out via removeHiddenNumbers().
sub fetchLine($$)
{
my $n = shift;
my $show = shift;
return "" unless $n;  # should never happen
my $t = $text[substr($map, $n*$lnwidth, $lnwidth1)];
removeHiddenNumbers(\$t) if $show and $fmode&$browsemode;
return $t;
}  # fetchLine

#  Here's the same function, but for another context.
sub fetchLineContext($$$)
{
my $n = shift;
my $show = shift;
my $cx = shift;
$t = $text[substr($map[$cx], $n*$lnwidth, $lnwidth1)];
removeHiddenNumbers(\$t) if $show and $fmode[$cx]&$browsemode;
return $t;
}  # fetchLineContext

#  Print size of the text in buffer.
sub apparentSize()
{
my $j = 0;
$j += length(fetchLine($_, 1)) + 1 foreach (1..$dol);
--$j if $fmode&$nlmode;
print "$j\n";
}  # apparentSize

#  Read a line from stdin.
#  Could be a command, could be text going into the buffer.
sub readLine()
{
my ($i, $j, $c, $d, $line);
getline: {
$intFlag = 0;
$do_input = 1;
$line = <STDIN>;
$do_input = 0;
redo getline if $intFlag and ! defined $line;  # interrupt
$intFlag = 0;
}
exit 0 unless defined $line;  # EOF
$line =~ s/\n$//;
#  A bug in my keyboard causes nulls to be entered from time to time.
$line =~ s/\0/ /g;
return $line if $line !~ /~/;  # shortcut
#  We have to process it, character by character.
my $line2 = "";
for($i=0; $i<length($line); $line2 .= $c, ++$i) {
$c = substr $line, $i, 1;
next if $c ne '~';
next if $i == length($line) - 1;
$d = substr $line, $i+1, 1;
++$i, next if $d eq '~';
next if $i == length($line) - 2;
$j = index $hexChars, $d;
next if $j < 0;
$j -= 6 if $j >= 16;
my $val = $j*16;
$d = substr $line, $i+2, 1;
$j = index $hexChars, $d;
next if $j < 0;
$j -= 6 if $j >= 16;
$val += $j;
#  We don't use this mechanism to enter normal ascii characters.
next if $val >= 32 and $val < 127;
#  And don't stick a newline in the middle of an entered line.
next if $val == 10;
$c = chr $val;
$i += 2;
}  # loop over input chars
return $line2;
}  # readLine

#  Read a block of lines into the buffer.
sub readLines()
{
my $tbuf = "";
#  Put the pending line in first, if it's there.
my $line = $linePending;
$line = readLine() unless defined $line;
while($line ne ".") {
$tbuf .= "$line\n";
$line = readLine();
}  # loop gathering input lines
return addTextToSession(\$tbuf) if length $tbuf;
$dot = $endRange;
$dot = 1 if $dot == 0 and $dol;
return 1;
}  # readLines

#  Display a line.  Show line number if $cmd is n.
#  Expand binary characters if $cmd is l.
#  Pass the line number.
sub dispLine($)
{
my $ln = shift;
print "$ln " if $cmd eq 'n';
my $line = fetchLine($ln, 1);
#  Truncate, if the line is pathologically long.
$line = substr($line, 0, 500) . "..." if length($line) > 500;
print '^' if $endmarks and ($endmarks == 2 or $cmd eq 'l');
if($cmd eq 'l') {
$line =~ y/\10\11/<>/;
$line =~ s/([\0-\x1f\x80-\xff])/sprintf("~%02x",ord($1))/ge;
} else {
#  But we always remap return, null, and escape
$line =~ s/(\00|\r|\x1b)/sprintf("~%02x",ord($1))/ge;
}
print $line;
print dirSuffix($ln);
print '$' if $endmarks and ($endmarks == 2 or $cmd eq 'l');
print "\n";
}  # dispLine

#  If we've printed a line in directory mode, and the entry isn't
#  a regular file, we've got to find and print the special character at the end.
#  / means directory, for example.
#  This is used by the previous routine, among others.
sub dirSuffix($)
{
my $ln = shift;
my $suf = "";
if($fmode&$dirmode) {
$suf = substr($labels, $dirSufStart + 2*$ln, 2);
$suf =~ s/ +$//;
}
return $suf;
}  # dirSuffix

#  Routines to help format a string, i.e. cut at sentence boundaries.
#  This isn't real smart; it will happily split Mr. Flintstone.
sub appendWhiteSpace($$)
{
my($chunk, $breakable) = @_;
my $nlc = $chunk =~ y/\n//d;  # newline count
if($breakable) {
#  Don't interrogate the last few characters of a huge string -- that's inefficient.
my $short = substr $refbuf, -2;
my $l = length $refbuf;
$lperiod = $colno, $idxperiod = $l if $short =~ /[.!?:][)"|}]?$/;
$lcomma = $colno, $idxcomma = $l if $short =~ /[-,;][)"|]?$/;
$lright = $colno, $idxright = $l if $short =~ /[)"|]$/;
$lany = $colno, $idxany = $l;
#  Tack short fragment onto previous long line.
if($longcut and ($nlc or $lperiod == $colno) and $colno <= 14) {
substr($refbuf, $longcut, 1) = " ";
$chunk = "", $nlc = 1 unless $nlc;
}  # pasting small fragment onto previous line
}  # allowing line breaks
$nlc = 0 if $lspace == 3;
if($nlc) {
$nlc = 1 if $lspace == 2;
$refbuf .= "\n";
$refbuf .= "\n" if $nlc > 1;
$colno = 1;
$longcut = $lperiod = $lcomma = $lright = $lany = 0;
$lspace = 3 if $lspace >= 2 or $nlc > 1;
$lspace = 2 if $lspace < 2;
}
$refbuf .= $chunk;
$lspace = 1 if length $chunk;
$colno += $chunk =~ y/ / /;
$colno += 4 * ($chunk =~ y/\t/\t/);
}  # appendWhiteSpace

sub appendPrintable($)
{
my $chunk = shift;
$refbuf .= $chunk;
$colno += length $chunk;
$lspace = 0;
return if $colno <= $optimalLine;
#  Oops, line is getting long.  Let's see where we can cut it.
my ($i, $j) = (0, 0);
if($lperiod > $cutLineAfter) { $i = $lperiod, $j = $idxperiod;
} elsif($lcomma > $cutLineAfter) { $i = $lcomma, $j = $idxcomma;
} elsif($lright > $cutLineAfter) { $i = $lright, $j = $idxright;
} elsif($lany > $cutLineAfter) { $i = $lany, $j = $idxany;
}
return unless $j;  # nothing we can do about it
$longcut = 0;
$longcut = $j if $i != $lperiod;
substr($refbuf, $j, 1) = "\n";
$colno -= $i;
$lperiod -= $i;
$lcomma -= $i;
$lright -= $i;
$lany -= $i;
}  # appendPrintable

#  Break up a line using the above routines.
sub breakLine($)
{
my $t = shift;
my $ud = $$t =~ s/\r$//;
if($lspace eq "2l") {
$$t =~ s/^/\r/ if length $$t;
$lspace = 2;
}
$$t =~ s/^/\r/ if length $$t > $paraLine;
my $rc = $$t =~ y/\r/\n/;
$ud |= $$t =~ s/[ \t]+$//gm;
$ud |= $$t =~ s/([^ \t\n])[ \t]{2,}/$1 /g;
$ud |= $$t =~ s/([^ \t\n])\t/$1 /g;
$ud |= $$t =~ s/ +\t/\t/g;
$lspace = 2 if $lspace < 2;  # should never happen
$lspace = 3 unless length $$t;
return $ud if ! $rc and length $$t < $optimalLine;
$rc |= $ud;
#  The following 120 comes from $paraLine.
$$t =~ s/(\n.{120})/\n$1/g;
$$t =~ s/(.{120,}\n)/$1\n/g;
$refbuf = "";
$colno = 1;
$longcut = $lperiod =  $lcomma =  $lright =  $lany = 0;
while($$t =~ /(\s+|[^\s]+)/g) {
my $chunk = $1;
if($chunk =~ /\s/) { appendWhiteSpace($chunk, 1); } else { appendPrintable($chunk); }
}
if($lspace < 2) {  # line didn't have a \r at the end
#  We might want to paste the last word back on.
appendWhiteSpace("\n", 1);
chop $refbuf;
}
$rc = 1 if $refbuf =~ /\n/;
return 0 unless $rc;
$$t = $refbuf;
$lspace = "2l" if length $refbuf > $paraLine;
return 1;
}  # breakLine

#  Check the syntax of a regular expression, before we pass it to perl.
#  If perl doesn't like it, it dies, and you've lost your edits.
#  The first char is the delimiter -- we stop at the next delimiter.
#  The regexp, up to the second delimiter, is returned,
#  along with the remainder of the string in the second return variable.
#  return (regexp, remainder), or return () if there is a problem.
#  As usual, $errorMsg will be set.
#  Pass the line containing the regexp, and a flag indicating
#  left or right side of a substitute.
sub regexpCheck($$)
{
my ($line, $isleft) = @_;
my ($c, $d);
#  We wouldn't be here if the line was empty.
my $delim = substr $line, 0, 1;
index($valid_delim, $delim) >= 0 or
$errorMsg = "invalid delimiter $delim", return ();
$line = substr $line, 1;  # remove lead delimiter
#  Remember whether a character is "on deck", ready to be modified by * etc.
my $ondeck = 0;
my $offdeck = ' ';
my $exp = "";
my $cc = 0;  # in character class
my $paren = 0;  # nested parentheses

while(length $line) {
$c = substr $line, 0, 1;
if($c eq '\\') {
$errorMsg = "line ends in backslash", return () if length($line) == 1;
$d = substr $line, 1, 1;
$ondeck = 1;
$offdeck = ' ';
#  I can't think of any reason to remove the escape \ from any character,
#  except ()|, where we reverse the sense of escape,
#  and \& on the right, which becomes &.
if(index("()|", $d) >= 0 and ! $cc and $isleft) {
$ondeck = 0, ++$paren if $c eq '(';
--$paren if $c eq ')';
$errorMsg = "Unexpected closing )", return () if $paren < 0;
$c = '';
}
$c = '' if $d eq '&' and ! $isleft;
$exp .= "$c$d";
$line = substr $line, 2;
next;
}  # escape character

#  Break out if you've hit the delimiter
$paren or $c ne $delim or last;

#  Not the delimiter, I'll assume I can copy it over to $exp.
#  But I have to watch out for slash, which is *my* delimiter.
$exp .= '\\' if $c eq '/';
#  Then there's ()|, which I am reversing the sense of escape.
$exp .= '\\'  if index("()|", $c) >= 0 and $isleft;
#  Sometimes $ is interpolated when I don't want it to be.
#  Even if there is no alphanumeric following, a bare $ seems to cause trouble.
#  Escape it, unless followed by delimiter, or digit (rhs).
if($c eq '$') {
$exp .= '\\' if $isleft and
length($line) > 1 and substr($line, 1, 1) ne $delim;
$exp .= '\\' if ! $isleft and
$line !~ /^\$\d/;
}
if($c eq '^') {
$exp .= '\\' if $isleft and $cc != length $exp;
}
#  And we have to escape every @, to avoid interpolation.
#  Good thing we don't have to escape %,
#  or it might mess up our % remembered rhs logic.
$exp .= '\\' if $c eq '@';
#  Turn & into $&
$exp .= '$' if $c eq '&' and ! $isleft;
#  Finally push the character.
$exp .= $c;
$line = substr $line, 1;

#  Are there any syntax checks I need to make on the rhs?
#  I don't think so.
next if ! $isleft;

if($cc) {  # character class
#  All that matters here is the ]
$cc = 0 if $c eq ']';
next;
}

#  Modifiers must have a preceding character.
#  Except ? which can reduce the greediness of the others.
if($c eq '?' and $offdeck ne '?') {
$ondeck = 0;
$offdeck = '?';
next;
}

if(index("?+*", $c) >= 0 or
$c eq '{' and $line =~ s/^(\d+,?\d*})//) {
my $mod = ( $c eq '{' ? "{$1" : $c);
$errorMsg = "$mod modifier has no preceding character", return () if ! $ondeck;
$ondeck = 0;
$offdeck = $c;
$exp .= "$1" if $c eq '{';
next;
}  # modifier

$ondeck = 1;
$offdeck = ' ';
$cc = length $exp if $c eq '[';
}  # loop over chars in the pattern

$cc == 0 or
$errorMsg = "no closing ]", return ();
$paren == 0 or
$errorMsg = "no closing )", return ();
if(! length $exp and $isleft) {
$exp = $savelhs;
$errorMsg = "no remembered search string", return () if ! defined $exp;
}
$savelhs = $exp if $isleft;
if(! $isleft) {
if($exp eq '%') {
$exp = $saverhs;
$errorMsg = "no remembered replacement string", return () if ! defined $exp;
} elsif($exp eq '\\%') {
$exp = '%';
}
$saverhs = $exp;
}  # rhs

return ($exp, $line);
}  # regexpCheck

#  Get the start or end of a range.
#  Pass the line containing the address.
sub getRangePart($)
{
my $line = shift;
my $ln = $dot;
if($line =~ s/^(\d+)//) {
$ln = $1;
} elsif($line =~ s/^\.//) {
#  $ln is already set to dot
} elsif($line =~ s/^\$//) {
$ln = $dol;
} elsif($line =~ s/^'([a-z])//) {
$ln = substr $labels, (ord($1) - ord('a'))*$lnwidth, $lnwidth;
$errorMsg = "label $1 not set", return () if $ln eq $lnspace;
} elsif($line =~ m:^([/?]):) {
$errorMsg = "search string not found", return () if $dot == 0;
my $delim = $1;
my @pieces = regexpCheck($line, 1);
return () if $#pieces < 0;
my $exp = $pieces[0];
$line = $pieces[1];
my $icase = "";  # case independent
$icase = "i" if $caseInsensitive;
if($delim eq substr $line, 0, 1) {
$line = substr $line, 1;
if('i' eq substr $line, 0, 1) {
$line = substr $line, 1;
$icase = 'i';
}
}
my $incr = ($delim eq '/' ? 1 : -1);
#  Recompile the regexp after each command, but don't compile it on every line.
#  Is there a better way to do this, besides using eval?
my $notfound = 0;
eval '
while(1) {
$ln += $incr;
$ln = 1 if $ln > $dol;
$ln = $dol if $ln == 0;
last if fetchLine($ln, 1) =~ ' .
"/$exp/o$icase; " .
'$notfound = 1, last if $ln == $dot;
}  # looking for match
';   # end evaluated string
$errorMsg = "search string not found", return () if $notfound;
}  # search pattern
#  Now add or subtract from this base line number
while($line =~ s/^([+-])(\d*)//) {
my $add = ($2 eq "" ? 1 : $2);
$ln += ($1 eq '+' ? $add : -$add);
}
$errorMsg = "line number too large", return ()
if $ln > $dol;
$errorMsg = "negative line number", return ()
if $ln < 0;
return ($ln, $line);
}  # getRangePart

#  Read the data as a string from a url.
#  Data is retrieved using http, https, or ftp.
#  Parameters: url, post data, result buffer.
#  You can return 0 (failure) and leave text and the buffer,
#  and I'll report the error, and still assimilate the buffer.
sub readUrl($$$)
{
my ($filename, $post, $tbuf) = @_;
my $rc = 1;  # return code, success
$lfsz = 0;  # local file size
my $rsize = 0;  # size read
my $weburl;
my $scheme;
my $encoding = "";
my $pagetype = "";
my %url_desc = (); # Description of the current URL

#  I don't know if we need a full url encode or what??
#  This is a major kludge!  I just don't understand this.
$filename =~ s/ /%20/g;
$filename =~ s/[\t\r\n]//g;
#  I don't know what http://foo@this.that.com/file.htm means,
#  but I see it all the time.
$filename =~ s,^http://[^/]*@,http://,i;

$$tbuf = "";  # start with a clear buffer
$errorMsg = "too many nested frames", return 0 unless $rerouteCount;
--$rerouteCount;

#  split into machine, file, and post parameters
separate: {
my $oldname = $filename;  # remember where we started
my $authinfo = "";  # login password for web sites that return error 401

$scheme = is_url $filename;  # scheme could have changed
$weburl = 0;
$weburl = 1 if $scheme =~ /^https?$/;
if(!length $post and $filename =~ s/^(.*?)(\?.*)$/$1/ ) {
$post = $2;
}
#  $post should be url encoded, but sometimes it's not, and I don't know why.
$post =~ y/ /+/;
my $postfilename = "";
#  We assume $post starts with ? or *, if it is present at all.
my $meth = "GET";
my $postapplic = "";
if(substr($post, 0, 1) eq '*') {
$meth = "POST";
} else {
$postfilename = $post;
}
print "$meth: $post\n" if $debug >= 2;

$filename =~ s,^$scheme://,,i;
my $serverPort = 80;
$serverPort = 443 if $scheme eq 'https';
$serverPort = 21 if $scheme eq 'ftp';
$serverPort = 23 if $scheme eq 'telnet';
my $serverPortString = "";
my $server = $filename;
$server =~ s,/.*,,;
#  Sometimes we need to do this -- got me hanging!
$server =~ s/%([0-9a-fA-F]{2})/chr hex "$1"/ge;
if($server =~ s/:(\d+)$//) {
$serverPort = $1;
}
# If a server is on port 443, assume it speaks SSL.
#  This is a real bastardization of the html standard,
#  but it's the explorer standard.  Need I say more?
$scheme = 'https' if$serverPort == 443;
$serverPortString = ":$serverPort" if $serverPort != 80;
$filename =~ s,^[^/]*,,;

#  Lots of http servers can't handle /./ or /../ or //
$filename =~ s:/{2,}:/:g;
#  Oops, put internal http:// back the way it was.
#  The bug is caused by a line like this.
#  <form method=post action=server/file?this=that&return=http://someOtherServer/blah>
#  Because it's post, the get parameters after the ? are still here.
#  And I just turned http:// into http:/
#  This is very rare, but it happened to me, so I'm trying to fix it.
$filename =~ s,http:/,http://,gi;
$filename =~ s,ftp:/,ftp://,gi;
$filename =~ s:^/(\.{1,2}/)+:/:;
$filename =~ s:/(\./)+:/:g;
1 while $filename =~ s:/[^/]+/\.\./:/:;
$filename =~ s:^/(\.\./)+:/:;

#  Ok, create some more variables so we either fetch this file
#  or convert it if it's pdf.
#  Too bad I did all this work, and the pdf converter doesn't work for crap.
#  Probably because pdf is irreparably inaccessible.
#  Thanks a lot adobe!
my $go_server = $server;
my $go_port = $serverPort;
my $go_portString = $serverPortString;
my $go_file = $filename;
my $go_post = $post;
my $go_postfilename = $postfilename;
my $go_meth = $meth;

if($filename =~ /\.pdf$/ and $pdf_convert) {
($meth eq "GET" and $scheme eq "http") or
$errorMsg = "online conversion from pdf to html only works when the pdf file is accessed via the http get method\ntype pr to download pdf in raw mode", return 0;
$go_server="access.adobe.com";
$go_port = 80;
$go_portString = "";
$go_file = "/perl/convertPDF.pl";
#  It would be simpler if this bloody form wer get, but it's post.
$go_meth = "POST";
$go_post = "http://$server$serverPortString$filename$postfilename";
$go_post = "*submit=submit&url=" . urlEncode($go_post);
$go_postfilename = "";
}  # redirecting to adobe to convert pdf

if($go_meth eq "POST") {
$postapplic =
"Pragma: no-cache$eol" .
"Cache-Control: no-cache$eol" .
"Content-Type: application/x-www-form-urlencoded$eol" .
"Content-Length: " . (length($go_post)-1) . $eol;
}

my $newname = "";

$authAttempt = 0;
makeconnect: {
my $chunk;
$lfsz = 0;
$$tbuf = "";
$go_file = "/" if ! length $go_file;
%url_desc = (SCHEME => $scheme, SERVER => $go_server, PORT => $go_port, PATH => $go_file, method => $go_meth);
$url_desc{content} = substr($go_post, 1) if length $go_post; # Kinda silly.
# If you're using digest authentication with the POST method, 
# the content needs to be digestified.
# This is for message integrity checking, when that option is used.
# Consider completely replacing $go_x variables with elements of the %url_desc
# hash?  There is massive redundancy here.
my $domainCookies = "";
$domainCookies = fetchCookies(\%url_desc) if $allowCookies; # Grab the cookies.
my $send_server  = # Send this to the http server - maybe via SSL
"$go_meth $go_file$go_postfilename HTTP/1.0$eol" .
#  Do we need $go_portString here???
#  If I put it in, paypal doesn't work.
"Host: $go_server$eol" .
(length $referer ? "Referer: $referer$eol" : "") .
$domainCookies .
$authinfo .
"Accept: text/*, audio/*, image/*, application/*, message/*$eol" .
"Accept: audio-file, postscript-file, mail-file, default, */*;q=0.01$eol" .
"Accept-Encoding: gzip, compress$eol" .
"Accept-Language: en$eol" .
 "User-Agent: $agent$eol" .
$postapplic .
$eol;  # blank line at the end

#  send data after if post method
$send_server .= substr($go_post, 1) if $go_meth eq "POST";

if($debug >= 4) {
my $temp_server = $send_server;
$temp_server =~ y/\r//d;
print $temp_server;
}

if($scheme eq 'http') {
#  Connect to the http server.
my $iaddr   = inet_aton($go_server)               or
$errorMsg = "cannot identify $go_server on the network", return 0;
my $paddr   = sockaddr_in($go_port, $iaddr);
my $proto   = getprotobyname('tcp');
socket(FH, PF_INET, SOCK_STREAM, $proto)  or
$errorMsg = "cannot allocate a socket", return 0;
connect(FH, $paddr)    or
$errorMsg = "cannot connect to $go_server", return 0;
FH->autoflush(1);

print FH $send_server; # Send the HTTP request message

#  Now retrieve the page and update the user after every 100K of data.
my $last_fk = 0;
STDOUT->autoflush(1) if ! $doslike;
while(defined($rsize = sysread FH, $chunk, 100000)) {
print "sockread $rsize\n" if $debug >= 5;
$$tbuf .= $chunk;
$lfsz += $rsize;
last if $rsize == 0;
my $fk = int($lfsz/100000);
if($fk > $last_fk) {
print ".";
$last_fk = $fk;
}
last if $lfsz >= $maxfile;
}
               close FH;
print "\n" if $last_fk;
STDOUT->autoflush(0) if ! $doslike;
$lfsz <= $maxfile or
$errorMsg = "file is too large, limit 40MB", return 0;
defined $rsize or
$$tbuf = "", $errorMsg = "error reading data from the socket", return 0;

} elsif ($scheme eq 'https') {
$lfsz = do_ssl($go_server, $go_port, $send_server, $tbuf);
Net::SSLeay::free($ssl) if defined $ssl;
Net::SSLeay::CTX_free($ctx) if defined $ctx;
return 0 unless $lfsz;

} elsif ($scheme eq 'ftp') {
$lfsz = ftp_connect($go_server, $go_port, $go_file, $tbuf);
return 0 unless $lfsz;

} elsif ($scheme eq "telnet") {
if($go_server =~ s/^([^:@]*):([^:@]*)@//) {
print "This URL gives a suggested username of $1 and password of $2\n" .
"to be used with the telnet connection you are about to establish.\n";
# See RFC 1738, section 3.8.  The username and password in a telnet URL
# are advisory.  There is no standard method of logging into telnet services.
# I guess this is especially useful for public services, which offer guest accounts and such.
}
print "Starting telnet.\n";
system("telnet $go_server $go_port");
return 1;

} else {
$errorMsg = "this browser cannot access $scheme URLs.", return 0;
}

#  We got the web page.
#  But it might be a redirection to another url.
if($weburl and $$tbuf =~  /^http\/[\d.]+ 30[12]/i) {
if($$tbuf =~ /\nlocation:[ \t]+(.*[^\s])[ \t\r]*\n/i) {
$newname = $1;
print "relocate $newname\n" if $debug >= 2;
}}

if($rc and
! length $newname and
#  Some web sites serve up pages with no headers at all!
#  aspace.whizy.com/forum/ultimate.cgi
$$tbuf =~ /^http/i and
$$tbuf =~  /^http\/[\d.]+ 404 /i) {
$errorMsg = "file not found on the remote server";
$rc = 0;
}  # not found

#  there is yet another way to redirect to a url
if($rc and $$tbuf =~ /<meta +http-equiv=["']?refresh[^<>]*(url=|\d+;)['"]?([^'">\s]+)/i) {
$newname = $2;
print "refresh $newname\n" if $debug >= 2;
#  This is almost always an absolute url, even without the http prefix,
#  but sometimes it's relative.  Got me hanging!
#  Here's a looser criterion for web url.
if($newname =~ /^[\w,-]+\.[\w,-]+\.[\w,-]/) {
$newname = "http://$newname";
}
}

#  Extract information from the http header - primarily cookies.
$encoding = $pagetype = "";
if($$tbuf =~ s/^(http\/\d.*?\r?\n\r?\n)//si) {
my $header = $1;
my @lines = split "\n", $header;
open BFH, ">>$ebhttp";
if(defined BFH) {
print BFH $header;
close BFH;
}
$authinfo = "";
while(my $hline = shift @lines) {
$hline =~ s/\r$//;
print "$hline\n" if $debug >= 4;
setCookies($hline, \%url_desc) if $hline =~ /^Set-Cookie:/i and $allowCookies;
$authinfo = parseWWWAuth($hline, \%url_desc) if $hline =~ /^WWW-Authenticate/i;
return 0 if $authinfo eq "x";
#  I shouldn't really discard things like charset=blablabla,
#  but I don't really know what to do with it anyways.
$hline =~s/;.*//;
$encoding = lc $1 if $hline =~ /^content-encoding:\s+['"]?(\w+)['"]?\s*$/i;
$pagetype = lc $1 if $hline =~ /^content-type:\s+['"]?([^\s'"]+)['"]?\s*$/i;
}  # loop over lines
++$authAttempt, redo makeconnect if length $authinfo;
} else {  # http header extracted
if($scheme =~ /^https?$/) {
$errorMsg = "http response doesn't have a head-body structure";
$rc = 0;
} else {
#  For now, this means ftp.
#  We could have retrieved an html page via ftp, but probably not.
#  Turn off browse command.
$cmd = 'e' unless $$tbuf =~ /^<[hH!]/;
}
}
} # makeconnect

#  cookies that are set via http-equiv
#  The content of the cookie must be quoted.
while($$tbuf =~ /<meta +http-equiv=["']?set-cookie['" ]+content="([^"]*)"/gi) {
setCookies($1, \%url_desc);
}
while($$tbuf =~ /<meta +http-equiv=["']?set-cookie['" ]+content='([^']*)'/gi) {
setCookies($1, \%url_desc);
}

if($rc and $reroute and length $newname) {
$newname = resolveUrl("$scheme://$server$serverPortString$filename", $newname);
print "becomes $newname\n" if $debug >= 2;
if($newname ne $oldname) {
#  It's not really diferent if one has :80 and the other doesn't.
#  I wouldn't code this up if it didn't really happen.  See www.claritin.com
$oldname =~ s,^HTTP://,http://,;
$oldname =~ s,^(http://)?([^/]*):80/,$1$2/,;
$oldname =~ s,^(http://)?([^/]*):80$,$1$2,;
$newname =~ s,^HTTP://,http://,;
$newname =~ s,^(http://)?([^/]*):80/,$1$2/,;
$newname =~ s,^(http://)?([^/]*):80$,$1$2,;
if($oldname ne $newname) {
if(--$rerouteCount) {
print "$newname\n" if $debug >= 1;
#  Post method becomes get after redirection, I think.
#  $post = "" if length $post and $newname =~ /\?[^\/]*$/;
$post = "";
$filename = $newname;
redo separate;
}
$errorMsg = "too many url redirections";
$rc = 0;
}}}  # automatic url redirection

$changeFname = "$scheme://$server$serverPortString$filename$postfilename";
}  # separate

#  Check for complressed data.
if($rc and $lfsz and length $encoding and $pagetype =~ /^text/i) {
print "$lfsz\ndecoding $encoding\n" if $debug >= 2;
my $program = "";
my $csuf = "";  # compression suffix
$program = "zcat", $csuf = "gz" if $encoding eq "gzip";
$program = "zcat", $csuf = "Z" if $encoding eq "compress";
length $program or
$errorMsg = "unrecognized compression method", return 0;
$cfn = "$ebtmp.$csuf";  # compressed file name
open FH, ">$cfn" or
$errorMsg = "cannot create temp file $cfn", return 0;
binmode FH, ':raw' if $doslike;
print FH $$tbuf or
$errorMsg = "cannot write to temp file $cfn", return 0;
close FH;
unlink $ebtmp;
if(! system "$program $ebtmp.$csuf >$ebtmp 2>/dev/null") {
#  There are web pages out there that are improperly compressed.
#  We'll call it good if we got any data at all.
$errorMsg = "could not uncompress the data", return 0 unless (stat($ebtmp))[7];
}

#  Read in the uncompressed data.
$$tbuf = "";
open FH, $ebtmp or
$errorMsg = "cannot open the uncompressed file $ebtmp", return 0;
$lfsz = (stat(FH))[7];
$lfsz <= $maxfile or
$errorMsg = "uncompressed file is too large, limit 40MB", close FH, return 0;
binmode FH, ':raw' if $doslike;
$rsize = sysread FH, $$tbuf, $lfsz;
close FH;
$rsize and $rsize == $lfsz or
$errorMsg = "cannot read the uncompressed data from $ebtmp", return 0;
unlink $ebtmp;
}  # compressed data

if($rc and $fetchFrames) {
$errorMsg = "";
#  This really isn't right - to do this here I mean.
#  If a line of javascript happens to contain a frame tag
#  I'm going to fetch that frame and put it in right here.
#  Hopefully that won't happen.
#  Note that the entire frame tag must be on one line.
$$tbuf =~ s/(<i?frame\b[^<>\0\x80-\xff]+>)/readFrame($1)/gei;
$rc = 0 if length $errorMsg;
}  # looking for frames

return $rc;
}  # readUrl

#  Read a frame.
sub readFrame($)
{
my $tag = shift;
my $saveFname = $changeFname;
my($tc, $fbuf, $src, $name);

$tag =~ s/\bsrc *= */src=/gi;
$tag =~ s/\bname *= */name=/gi;
$tc = $tag;
if($tc =~ s/^.*\bsrc=//s) {
$src = $tc;
$src =~ s/ .*//s;
$src =~ s/^['"]//;
$src =~ s/['"]?>?$//;
if(length $src) {
print "fetch frame $src\n" if $debug >= 1;
$src = resolveUrl($saveFname, $src);
if($didFrame{$src}) {
print "already fetched\n" if $debug >= 2;
$changeFname = $saveFname;
return "";
}
$didFrame{$src} = 1;
print "* $src\n" if $debug >= 1;

$name = "";
$tc = $tag;
if($tc =~ s/^.*\bname=//s) {
$tc =~ s/ .*//s;
$tc =~ s/^['"]//;
$tc =~ s/['"]?>?$//;
$name = urlDecode $tc if length $tc;
}  # name attribute

if(readUrl($src, "", \$fbuf)) {
#  Replace the tag with the data, and some stuff prepended.
$name = " $name" if length $name;
$tag = "<H2> Frame$name: </H2>\n<base href=" .
urlEncode($changeFname) . ">\n";
$changeFname = $saveFname;
return $tag.$fbuf;
}  # frame read successfully
}}  # src attribute present

$changeFname = $saveFname;
return $tag;
}  # readFrame

#  Adjust the map of line numbers -- we have inserted text.
#  Also shift the downstream labels.
#  Pass the string containing the new line numbers, and the dest line number.
sub addToMap($$)
{
my ($newpiece, $dln) = @_;
my $offset = length($newpiece)/$lnwidth;
$offset > 0 or
die "negative offset in addToMap";
my ($i, $j);
foreach $i (0..25) {
my $ln = substr($labels, $i*$lnwidth, $lnwidth);  # line number
next if $ln eq $lnspace or $ln <= $dln;
substr($labels, $i*$lnwidth, $lnwidth) =
sprintf($lnformat, $ln + $offset);
}  # loop over 26 labels
$j = ($dln+1) * $lnwidth;
substr($map, $j, 0) = $newpiece;
$dot = $dln + $offset;
$dol += $offset;
$fmode |= $changemode|$firstopmode;
$ubackup = 1;
}  # addToMap

#  Fold in the text buffer (parameter) at $endRange (global variable).
#  Assumes the text has the last newline on it.
sub addTextToSession($)
{
my $tbuf = shift;  # text buffer
return 1 unless length $$tbuf;
$fmode &= ~$nlmode if $endRange == $dol;
if(not $$tbuf =~ s/\n$// and
$endRange == $dol) {
$fmode |= $nlmode;
print "no trailing newline\n" if ! ($fmode&$binmode) and $cmd ne 'b';
}  # missing newline
my $j = $#text;
my $newpiece = "";
#  At this point $tbuf could be empty, whence split doesn't work properly.
#  This only happens when reading a file containing one blank line.
if(length $$tbuf) {
push @text, split "\n", $$tbuf, -1;
} else {
push @text, "";
}
$#text = $j, return 0 if lineLimit 0;
$newpiece .= sprintf($lnformat, $j) while ++$j <= $#text;
addToMap($newpiece, $endRange);
return 1;
}  # addTextToSession

#  Read a file into memory.
#  As described earlier, the lines are appended to @text.
#  Then the indexes for those lines are pasted into $map,
#  using addToMap().
#  Check to see if the data is binary, and set $fmode accordingly.
#  Parameters are the filename or URL, and the post data (for URLs).
sub readFile($$)
{
my ($filename, $post) = @_;
my $tbuf;  # text buffer
my $rc = 1;  # return code, success
$filesize = 0;
my $rsize = 0;  # size read
my $j;

if(is_url $filename) {
$rerouteCount = 24;
%didFrame = ();
$rc = readUrl($filename, $post, \$tbuf);
$filesize = length $tbuf;
return 0 unless $rc + $filesize;
} else {  # url or file

open FH, "<$filename" or
$errorMsg = "cannot open $filename, $!", return 0;

#  Check for directory here
if(-d FH) {
close FH;
$j = $filename;
$j =~ s,/$,,;
$j .= "/*";
my @dirlist;
if($j =~ / /) {
@dirlist = glob '"'.$j.'"';
} else {
@dirlist = glob $j;
}
if($#dirlist < 0) {
$dot = $endRange;
$filesize = 0;
return $rc;
}  # empty directory
$dirname = $j;
$dirname =~ s/..$//;  # get rid of /*
return 0 if lineLimit($#dirlist + 1);
$filesize = 0;
$tbuf = "";
$j = $dirSufStart;
substr($labels, $j, 2) = "  ";
foreach (@dirlist) {
my $entry = $_;
$entry =~ s,.*/,,;  # leave only the file
$entry =~ s/\n/\t/g;
my $suf = "";
$suf .= '@' if -l;
if(! -f) {
$suf .= '/' if -d;
$suf .= '|' if -p;
$suf .= '*' if -b;
$suf .= '<' if -c;
$suf .= '^' if -S;
}  # not a regular file
$filesize += length($entry) + length($suf) + 1;
if($dol) {
$entry .= $suf;
} else {
$suf .= "  ";
$j += 2;
substr($labels, $j, 2) = substr($suf, 0, 2);
}
$tbuf .= "$entry\n";
}
$dol or $fmode = $dirmode, print "directory mode\n";
return addTextToSession(\$tbuf);
}  # directory

-f FH or $errorMsg = "$filename is not a regular file", close FH, return 0;
$filesize = (stat(FH))[7];
if(! $filesize) {
close FH;
$dot = $endRange;
$filesize = 0;
return $rc;
}  # empty file
$filesize <= $maxfile or
$errorMsg = "file is too large, limit 40MB", close FH, return 0;
binmode FH, ':raw' if $doslike;
$rsize = sysread(FH, $tbuf, $filesize) if $filesize;
close FH;
$rsize == $filesize or
$errorMsg = "cannot read the contents of $filename,$!", return 0;
}  # reading url or regular file

my $bincount = $tbuf =~ y/\0\x80-\xff/\0\x80-\xff/;
if($bincount*4 - 10 < $filesize) {
#  A text file - remove crlf in the dos world.
$tbuf =~ s/\r\n/\n/g if $doslike;
} elsif(! ($fmode&$binmode)) {
#  If it wasn't before, it is now a binary file.
print "binary data\n";
$fmode |= $binmode;
}

$rc &= addTextToSession(\$tbuf);
return $rc;
}  # readFile

#  Write a range into a file.
#  Pass the mode and filename.
sub writeFile($$)
{
my ($mode, $filename) = @_;
$errorMsg = "cannot write to a url", return 0 if is_url($filename);
$dol or $errorMsg = "writing an empty file", return 0;
open FH, "$mode$filename" or
$errorMsg = "cannot create $filename, $!", return 0;
$filesize = 0;
binmode FH, ':raw' if $doslike and $fmode&$binmode;
if($startRange) {
foreach my $i ($startRange..$endRange) {
my $nl = ($fmode&$nlmode && $i == $dol ? "" : "\n");
my $suf = dirSuffix($i);
my $outline = fetchLine($i, 1).$suf.$nl;
print FH $outline or
$errorMsg = "cannot write to $filename, $!", close FH, return 0;
$filesize += length $outline;
}  # loop over range
}  # nonempty file
close FH;
#  This is not an undoable operation, nor does it change data.
#  In fact the data is "no longer modified" if we have written all of it.
$fmode &= ~$changemode if $dol == 0 or $startRange == 1 and $endRange == $dol;
return 1;
}  # writeFile

#  Read from another context.
#  Pass the context number.
sub readContext($)
{
my $cx = shift;
cxCompare($cx) and cxActive($cx) or return 0;
my $dolcx = $dol[$cx];
$filesize = 0;
if($dolcx) {
return 0 if lineLimit $dolcx;
$fmode &= ~$nlmode if $endRange == $dol;
my $newpiece = "";
foreach my $i (1..$dolcx) {
my $inline = fetchLineContext($i, 1, $cx);
my $suf = "";
if($fmode[$cx] & $dirmode) {
$suf = substr($labels[$cx], $dirSufStart + 2*$i, 2);
$suf =~ s/ +$//;
}
$inline .= $suf;
push @text, $inline;
$newpiece .= sprintf $lnformat, $#text;
$filesize += length($inline) + 1;
}  # end loop copying lines
addToMap($newpiece, $endRange);
if($fmode[$cx]&$nlmode) {
--$filesize;
$fmode |= $nlmode if $endRange == $dol;
}
$fmode |= $binmode, print "binary data\n"
if $fmode[$cx]&$binmode and ! ($fmode&$binmode);
}  # nonempty buffer
return 1;
}  # readContext

#  Write to another context.
#  Pass the context number.
sub writeContext($)
{
my $cx = shift;
my $dolcx = $endRange - $startRange + 1;
$dolcx = 0 if ! $startRange;
return 0 if ! cxCompare($cx) or !cxReset($cx, 1) or lineLimit $dolcx;
my $mapcx = $lnspace;
$filesize = 0;
if($startRange) {
foreach my $i ($startRange..$endRange) {
$outline = fetchLine($i, 0);
$outline .= dirSuffix($i);
push @text, $outline;
$mapcx .= sprintf $lnformat, $#text;
$filesize += length($outline) + 1;
}  # end loop copying lines
$fmode[$cx] = $fmode & ($binmode|$browsemode);
$fmode[$cx] |= $nlmode, --$filesize
if $fmode&$nlmode and $endRange == $dol;
}  # nonempty file
$map[$cx] = $mapcx;
$dot[$cx] = $dol[$cx] = $dolcx;
$factive[$cx] = 1;
$fname[$cx] = "";
$btags[$cx] = $btags;
return 1;
}  # writeContext

#  Move or copy a block of text.
sub moveCopy()
{
$dest++;  # more convenient
$endr1 = $endRange+1;  # more convenient
$dest <= $startRange or
$dest >= $endr1 or
$errorMsg = "destination lies inside the block to be moved or copied", return 0;
if($cmd eq 'm' and
($dest == $endr1 or $dest == $startRange)) {
$errorMsg = "no change" if ! $inglob;
return 0;
}
my $starti = $startRange*$lnwidth;
my $endi = $endr1*$lnwidth;
my $desti = $dest * $lnwidth;
my $offset = $endr1 - $startRange;
my ($i, $j);
#  The section of the map that represents the range.
my $piece_r = substr $map, $starti, $endi-$starti;
my $piece_n = "";  # the new line numbers, if the text is copied.
if($cmd eq 't') {
return 0 if lineLimit $offset;
for($j=0; $j<length($piece_r); $j+=$lnwidth) {
push @text,
$text[substr($piece_r, $j, $lnwidth1)];
$piece_n .= sprintf $lnformat, $#text;
}
substr($map, $desti, 0) = $piece_n;
} elsif($dest < $startRange)  {
substr($map, $starti, $endi-$starti) = "";
substr($map, $desti, 0) = $piece_r;
} else {
substr($map, $desti, 0) = $piece_r;
substr($map, $starti, $endi-$starti) = "";
}
if($fmode&$nlmode) {
$fmode &= ~$nlmode if $dest > $dol;
$fmode &= ~$nlmode if $endRange == $dol and $cmd eq 'm';
}
#  Now for the labels
my ($lowcut, $highcut, $p2len);
if($dest <= $startRange) {
$lowcut = $dest;
$highcut = $endr1;
$p2len = $startRange - $dest;
} else {
$lowcut = $startRange;
$highcut = $dest;
$p2len = $dest - $endr1;
}
foreach $i (0..25) {
my $ln = substr($labels, $i*$lnwidth, $lnwidth);  # line number
next if $ln eq $lnspace or $ln < $lowcut;
if($ln >= $highcut) {
$ln += $offset if $cmd eq 't';
} elsif($ln >= $startRange and $ln <= $endRange) {
$ln += ($dest < $startRange ? -$p2len : $p2len) if $cmd eq 'm';
$ln += $offset if $cmd eq 't' and $dest < $startRange;
} elsif($dest < $startRange) {
$ln += $offset;
} else {
$ln -= $offset if $cmd eq 'm';
}
substr($labels, $i*$lnwidth, $lnwidth) = sprintf $lnformat, $ln;
}  # loop over labels
$dol += $offset if $cmd eq 't';
$dot = $endRange;
$dot +=  ($dest < $startRange ? -$p2len : $p2len) if $cmd eq 'm';
$dot = $dest + $offset - 1 if $cmd eq 't';
$fmode |= $changemode|$firstopmode;
$ubackup = 1;
return 1;
}  # moveCopy

#  Delete a block of text.
#  Pass the range to delete.
sub delText($$)
{
my ($sr, $er) = @_;  # local start and end range
my ($i, $j);
$fmode &= ~$nlmode if $er == $dol;
$j = $er - $sr + 1;
substr($map, $sr*$lnwidth, $j*$lnwidth) = "";
#  Move the labels.
foreach $i (0..25) {
my $ln = substr($labels, $i*$lnwidth, $lnwidth);  # line number
next if $ln eq $lnspace or $ln < $sr;
substr($labels, $i*$lnwidth, $lnwidth) =
($ln <= $er ? $lnspace : (sprintf $lnformat, $ln - $j));
}  # loop over labels
$dol -= $j;
$dot = $sr;
--$dot if $dot > $dol;
$fmode |= $changemode|$firstopmode;
$ubackup = 1;
return 1;
}  # delText

#  Delete files from a directory as you delete lines.
#  It actually moves them to your trash bin.
sub delFiles()
{
$dw or $errorMsg = "directories are readonly, type dw to enable directory writes", return 0;
$dw == 2 or length $rbin or
$errorMsg = "could not create .trash under your home directory, to hold the deleted files", return 0;
my $ln = $startRange;
my $cnt = $endRange - $startRange + 1;
while($cnt--) {
my $f = fetchLine($ln, 0);
if($dw == 2 or dirSuffix($ln) =~ /^@/) {
unlink "$dirname/$f" or
$errorMsg = "could not remove $f, $!", return 0;
} else {
rename "$dirname/$f", "$rbin/$f" or
$errorMsg = "Could not move $f to the trash bin, $!, set dx mode to actually remove the file", return 0;
}
delText($ln, $ln);
substr($labels, $dirSufStart + 2*$ln, 2) = "";
}
return 1;
}  # delFiles

#  Join lines from startRange to endRange.
sub joinText()
{
$errorMsg = "cannot join one line", return 0 if $startRange == $endRange;
return 0 if lineLimit 1;
my ($i, $line);
$line = "";
foreach $i ($startRange..$endRange) {
$line .= ' ' if $cmd eq 'J' and $i > $startRange;
$line .= fetchLine($i, 0);
}
push @text, $line;
substr($map, $startRange*$lnwidth, $lnwidth) = sprintf $lnformat, $#text;
delText($startRange+1, $endRange);
$dot = $startRange;
return 1;
}  # joinText

#  Substitute text on the lines in $startRange through $endRange.
#  We could be changing the text in an input field.
#  If so, we'll call infReplace().
#  Also, we might be indirectory mode, whence we must rename the file.
sub substituteText($)
{
my $line = shift;
my $whichlink = "";
$whichlink = $1 if $line =~ s/^(\d+)//;
length $line or
$errorMsg = "no regular expression after $icmd", return -1;
if($fmode&$dirmode) {
$dw or $errorMsg = "directories are readonly, type dw to enable directory writes", return -1;
}
my ($i, $j, $exp, $rhs, $qrhs, $lastSubst, @pieces, $blmode);

if($line ne "bl") {
$blmode = 0;
@pieces = regexpCheck($line, 1);
return -1 if $#pieces < 0;
$exp = $pieces[0];
$line = $pieces[1];
length $line or $errorMsg = "missing delimiter", return -1;
@pieces = regexpCheck($line, 0);
return -1 if $#pieces < 0;
$rhs = $pieces[0];
$line = $pieces[1];
} else { $blmode = 1, $lspace = 3; }

my $gflag = "";
my $nflag = 0;
my $iflag = "";
$iflag = "i" if $caseInsensitive;
$subprint = 1;  # default is to print the last line substituted
$lastSubst = 0;

if(! $blmode) {
if(length $line) {
$subprint = 0;
#  necessarily starts with the delimiter
substr($line, 0, 1) = "";
while(length $line) {
$gflag = 'g', next if $line =~ s/^g//;
$subprint = 2, next if $line =~ s/^p//;
$iflag = 'i', next if $line =~ s/^i//;
if($line =~ s/^(\d+)//) {
! $nflag or $errorMsg = "multiple numbers after the third delimiter", return -1;
$nflag = $1;
$nflag > 0 and $nflag <= 999 or
$errorMsg = "numeric suffix out of range, please use [1-999]", return -1;
next;
}  # number
$errorMsg = "unexpected substitution suffix after the third delimiter";
return -1;
}  # loop gathering suffix flags
! $gflag or ! $nflag or
$errorMsg = "cannot use both a numeric suffix and the `g' suffix simultaneously", return -1;
#  s/x/y/1 is very inefficient.
$nflag = 0 if $nflag == 1;
}  # closing delimiter

$qrhs = $rhs;  # quote-fixed right hand side
if($rhs =~ /^[ul]c$/) {
$qrhs = "$qrhs \$&";
$iflag .= 'e' if !$nflag;
} elsif($rhs eq "ue") {
$qrhs = "unescape \$&";
$iflag .= 'e' if !$nflag;
} elsif($rhs eq "mc") {
$qrhs = "mixCase \$&";
$iflag .= 'e' if !$nflag;
} else {
if($nflag) {
$qrhs =~ s/"/\\"/g;
$qrhs = '"'.$qrhs.'"';
}
}

#  I don't understand it, but $&[x] means something to perl.
#  So when I replace j with &[x], becomeing $&[x], it blows up.
#  Thus I escape open brackets and braces in the rhs.
#  Hopefully you won't escape them on the command line - you have no reason to.
#  If you do they'll be doubly escaped, and that's bad.
$qrhs =~ s/([\[{])/\\$1/g;  #  }
} else {
$subprint = 0;
}  # blmode or not

#  Substitute the input fields first.
if($cmd eq 'I') {
my $yesdot = 0;
my $foundFields = 0;
foreach $i ($startRange..$endRange) {
my $rc = infIndex($i, $whichlink);
next unless $rc;
$foundFields = 1;
$rc > 0 or $dot = $i, $inglob = 0, return -1;
my $newinf = $inf;
if(!$nflag) {
eval '$rc = $newinf =~ ' .
"s/$exp/$qrhs/$iflag$gflag; ";
} else {
$j = 0;
eval '$newinf =~ ' .
"s/$exp/++\$j == $nflag ? $qrhs : \$&/ge$iflag; ";
$rc = ($j >= $nflag);
}
next unless $rc;
$dot = $i;
infReplace($newinf) or return -1;
$yesdot = $dot;
}  # loop over lines
if(! $yesdot) {
if(!$inglob) {
$errorMsg = "no match" if $foundFields;
}
return 0;
}
dispLine($yesdot) if $subprint == 2 or ! $inglob and $subprint == 1;
return 1;
}  # input fields

#  Not an input field, just text, so replace it.
#  Once again, use the eval construct.
#  This time we might be substituting across an entire range.
@pieces = ();
$errorMsg = "";
eval '
for($i=$startRange; $i<=$endRange; ++$i) {
my $temp = fetchLine($i, 0);' .
($blmode ? 'my $subst = breakLine(\$temp);' :
(!$nflag ?
'my $subst = $temp =~ ' .
"s/$exp/$qrhs/o$iflag$gflag; "
:
'my $subst = 0;
my $k = 0;
$temp =~ ' .
"s/$exp/++\$k == $nflag ? $qrhs : \$&/oge$iflag; " .
'$subst = ($k >= $nflag); '
)) .
'next unless $subst;
if($fmode&$dirmode) {
if($temp =~ m,[/\n],) {
$errorMsg = "cannot embed slash or newline in a directory name";
$inglob = 0;
last;
}
my $dest = "$dirname/$temp";
my $src = fetchLine($i, 0);
$src = "$dirname/$src";
if($src ne $dest) {
if(-e $dest or -l $dest) {
$errorMsg = "destination file already exists";
$inglob = 0;
last;
}
rename $src, $dest or
$errorMsg = "cannot move file to $temp", $inglob = 0, last;
}  # source and dest are different
}  # directory
@pieces = split "\n", $temp, -1;
@pieces = ("") if $temp eq "";
last if lineLimit $#pieces+1;
$j = $#text;
push @text, @pieces;
@pieces = ();
substr($map, $i*$lnwidth, $lnwidth) = sprintf $lnformat, ++$j;
if($j < $#text) {
my $newpiece = "";
$newpiece .= sprintf $lnformat, $j while ++$j <= $#text;
addToMap($newpiece, $i);
$j = length($newpiece) / $lnwidth;
$endRange += $j;
$i += $j;
}
dispLine($i) if $subprint == 2;
$lastSubst = $i;
$fmode |= $changemode|$firstopmode;
$ubackup = 1;
last if $intFlag;
}
';  # eval string
return 0 if length $errorMsg;
if(! $lastSubst) {
$errorMsg = ($blmode ? "no change" : "no match") if ! $inglob;
return 0;
}
$dot = $lastSubst;
dispLine($dot) if $subprint == 1 and ! $inglob;
if($intFlag and ! $inglob) {
$errorMsg = $intMsg, return 0;
}
return 1;
}  # substituteText

#  Follow a hyperlink to another web page.
sub hyperlink($)
{
my $whichlink = shift;
$cmd = 'b';
$errorMsg = "cannot use the g$whichlink command in directory mode", return 0 if $fmode&$dirmode;
$startRange == $endRange or
$errorMsg = "go command does not expect a range", return 0;

my $h;  # hyperlink tag
my @links = ();  # links on this line
my @bref = ();  # baseref values
my ($j, $line, $href);

if($fmode&$browsemode) {
$line = fetchLine $endRange, 0;
while($line =~ /\x80([\x85-\x8f]+){/g) {
$j = revealNumber $1;
$h = $$btags[$j];
$href = $$h{href};
$errorMsg = "hyperlink found without a url?? internal error", return 0 unless defined $href;
push @links, $href;
push @bref, $$h{bref};
}  # loop
}  # browse mode

if($#links < 0) {
$line = fetchLine $endRange, 1;
stripWhite \$line;
$line =~ s/[\s"']+/ /g;
if(length $line) {
while($line =~ /([^ ]+)/g) {
$href = $1;
$href =~ s/^[^\w]+//;
$href =~ s/[^\w]+$//;
if(is_url $href) {
push @links, $href;
} else {
$href =~ s/^mailto://i;
push @links, "mailto:$href" if $href =~ /^[\w.,-]+@[\w,-]+\.[\w,.-]+$/;
}
}
}  # loop over words
}  # looking for url in text mode

$j = $#links + 1;
$j or $errorMsg = "no links present", return 0;
length $whichlink or $j == 1 or
$errorMsg = "multiple links, please use g [1,$j]", return 0;
$whichlink = 1 if ! length $whichlink;
if($whichlink == 0 or $whichlink > $j) {
$errorMsg = $j > 1 ?
"invalid link, please use g [1,$j]" :
"this line only has one link";
return 0;
}
--$whichlink;
$href = $links[$whichlink];
if($href =~ s/^mailto://i) {
$cmd = 'e';
return 1, "\x80mail\x80$href";
}  # mailto
$href =~ /^javascript:/i and
$errorMsg = "sorry, this link calls a javascript function", return 0;
return 1, $href if $href =~ /^#/;
$line = resolveUrl(($#bref >= 0 ? $bref[$whichlink] : ""), $href);
print "* $line\n";
return 1, $line;
}  # hyperlink

#  Follow an internal link to a section of the document.
sub findSection($)
{
my $section = shift;
foreach my $i (1..$dol) {
my $t = fetchLine $i, 0;
while($t =~ /\x80([\x85-\x8f]+)\*/g) {
my $j = revealNumber $1;
my $h = $$btags[$j];
return $i if $$h{name} eq $section;
}
}
return 0;
}  # findSection

#  Return the number of unbalanced punctuation marks at the start and end of the line.
sub unbalanced($$$)
{
my ($c, $d, $ln) = @_;
my $curline = fetchLine($ln, 1);
#  Escape these characters, so we know they are literal.
$c = "\\$c";
$d = "\\$d";
while($curline =~ s/$c[^$c$d]*$d//) { ; }
my $forward = $curline =~ s/$c//g;
$forward = 0 if $forward eq "";
my $backward = $curline =~ s/$d//g;
$backward = 0 if $backward eq "";
return $backward, $forward;
}  # unbalanced

#  Find the line that balances the unbalanced punctuation.
sub balanceLine($)
{
my $line = shift;
my ($c, $d);  # balancing characters
my $openlist = "{([<`";
my $closelist = "})]>'";
my $alllist = "{}()[]<>`'";
my $level = 0;
my ($i, $direction, $forward, $backward);

if(length $line) {
$line =~ /^[\{\}\(\)\[\]<>`']$/ or
$errorMsg = "you must specify exactly one of $alllist after the B command", return 0;
$c = $line;
if(index($openlist, $c) >= 0) {
$d = substr $closelist, index($openlist, $c), 1;
$direction = 1;
} else {
$d = $c;
$c = substr $openlist, index($closelist, $d), 1;
$direction = -1;
}
($backward, $forward) = unbalanced($c, $d, $endRange);
if($direction > 0) {
($level = $forward) or
$errorMsg = "line does not contain an open $c", return 0;
} else {
($level = $backward) or
$errorMsg = "line does not contain an open $d", return 0;
}
} else {  # character specified by the user or not?
#  Look for anything unbalanced, probably a brace.
foreach $i (0..2) {
$c = substr $openlist, $i, 1;
$d = substr $closelist, $i, 1;
($backward, $forward) = unbalanced($c, $d, $endRange);
! $backward or ! $forward or
$errorMsg = "both $c and $d are unbalanced on this line, try B$c or B$d", return 0;
($level = $backward + $forward) or next;
$direction = 1;
$direction = -1 if $backward;
last;
}
$level or
$errorMsg = "line does not contain an unbalanced brace, parenthesis, or bracket", return 0;
}  # explicit character passed in, or look for one

my $selected = ($direction > 0) ? $c : $d;

#  Now search for the balancing line.
$i = $endRange;
while(($i += $direction) > 0 and $i <= $dol) {
($backward, $forward) = unbalanced($c, $d, $i);
if($direction > 0 and $backward >= $level or
$direction < 0 and $forward >= $level) {
$dot = $i;
dispLine($dot);
return 1;
}
$level += ($forward-$backward) * $direction;
}  # loop over lines

$errorMsg = "cannot find the line that balances $selected";
return 0;
}  # balanceLine

#  Apply a regular expression to each line, and then execute
#  a command for each matching, or nonmatching, line.
#  This is the global feature, g/re/p, which gives us the word grep.
sub doGlobal($)
{
my $line = shift;
my ($i, $j, $exp, @pieces);

length $line or
$errorMsg = "no regular expression after $icmd", return 0;
@pieces = regexpCheck($line, 1);
return 0 if $#pieces < 0;
$exp = $pieces[0];
$line = $pieces[1];
length $line or
$errorMsg = "missing delimiter", return 0;
$line =~ s/^.(i?)\s*//;
my $iflag = $1;
$iflag = "i" if $caseInsensitive;

#  Clean up any previous stars.
substr($map, $_*$lnwidth+$lnwidth1, 1) = ' ' foreach (1.. $dol);

#  Find the lines that match the pattern.
my $gcnt = 0;  # global count
eval '
for($i=$startRange, $j=$i*$lnwidth+$lnwidth1; $i<=$endRange; ++$i, $j+=$lnwidth) {
substr($map, $j, 1) = "*", ++$gcnt if
fetchLine($i, 1)' .
($cmd eq 'g' ? ' =~ ' : ' !~ ') .
"/$exp/o$iflag; }";
$gcnt or $errorMsg = 
($cmd eq 'g' ? "no lines match the g pattern" : "all lines match the v pattern"),
return 0;

#  Now apply $line to every line with a *
$inglob = 1;
$errorMsg = "";
$line = 'p' if ! length $line;
my $origdot = $dot;
my $yesdot = 0;
my $nodot = 0;
my $stars = 1;
global:while($gcnt and $stars) {
$stars = 0;
for($i=1; $i<=$dol; ++$i) {
last global if $intFlag;
next unless substr($map, $i*$lnwidth+$lnwidth1, 1) eq '*';
$stars = 1,--$gcnt;
substr($map, $i*$lnwidth+$lnwidth1, 1) = ' ';
$dot = $i;  # ready to run the command
if(evaluate($line)) {
$yesdot = $dot;
--$i if $ubackup;  # try this line again, in case we deleted or moved it
} else {
#  Subcommand might turn global flag off.
$nodot = $dot, $yesdot = 0, last global if ! $inglob;
}
}
}
$inglob = 0;
#  yesdot could be 0, even upon success, if all lines are deleted via g/re/d
if($yesdot or ! $dol) {
$dot = $yesdot;
dispLine($dot) if ($cmd eq 's' or $cmd eq 'I') and $subprint == 1;
} elsif($nodot) {
$dot = $nodot;
} else {
$dot = $origdot;
$errorMsg = "none of the marked lines were successfully modified" if $errorMsg eq "";
}
$errorMsg = $intMsg if $errorMsg eq "" and $intFlag;
return ! length $errorMsg;
}  # doGlobal

#  Reveal the links to other web pages, or the email links.
sub showLinks()
{
my ($i, $j, $h, $href, $line);
my $addrtext = "";
if($fmode&$browsemode) {
$line = fetchLine $endRange, 0;
while($line =~ /\x80([\x85-\x8f]+){(.*?)}/g) {
$j = revealNumber $1;
$i = $2;
$h = $$btags[$j];
$href = $$h{href};
$href = "" unless defined $href;
if($href =~ s/^mailto://i) {
$addrtext .= "$i:$href\n";
} else {
$href = resolveUrl($$h{bref}, $href);
$addrtext .= "<A HREF=$href>\n$i\n</A>\n";
}
}  # loop
}  # browse mode
if(! length $addrtext) {
length $fname or $errorMsg = "no file name", return 0;
if(is_url($fname)) {
$href = $fname;
$href =~ s/\.browse$//;
$j = $href;
$j =~ s,^https?://,,i;
$j =~ s,.*/,,;
$addrtext = "<A HREF=$href>\n$j\n</A>\n";
} else {
$addrtext = $fname."\n";
}
}
$addrtext =~ s/\n$//;
$j = $#text;
push @text, split "\n", $addrtext, -1;
$#text = $j, return 0 if lineLimit 0;
$h = cxPack();
cxReset($context, 0) or return 0;
$$h{backup} = $backup if defined $backup;
$backup = $h;
print((length($addrtext)+1)."\n");
$dot = $dol = $#text - $j;
my $newpiece = $lnspace;
$newpiece .= sprintf($lnformat, $j) while ++$j <= $#text;
$map = $newpiece;
return 1;
}  # showLinks

#  All other editors let you stack and undo hundreds of operations.
#  If I'm writing a new editor, why don't I do that?
#  I don't know; guess I don't have the time.
#  And in my 20 years experience, I have rarely felt the need
#  to undo several operations.
#  I'm usually undoing the last typo, and that's it.
#  So I allow you to undo the last operation, only.
#  Get ready for a possible undo command.
sub readyUndo()
{
return if $fmode & $dirmode;
$savedot = $dot, $savedol = $dol;
$savemap = $map, $savelabels = $labels;
}  # readyUndo

sub goUndo()
{
#  swap, so we can undo our undo.  I do this alot.
my $temp;
$temp = $ dot, $dot = $lastdot, $lastdot = $temp;
$temp = $ dol, $dol = $lastdol, $lastdol = $temp;
$temp = $ map, $map = $lastmap, $lastmap = $temp;
$temp = $ labels, $labels = $lastlabels, $lastlabels = $temp;
}  # goUndo

#  Replace labels with their lines in shell escapes.
sub expandLabeledLine($)
{
my$x = shift;
my $n = ord($x) - ord('a');
my $ln = substr $labels, $n*$lnwidth, $lnwidth;
$ln ne $lnspace or
$errorMsg = "label $x not set", return "";
return fetchLine($ln, 1);
}  # expandLabeledLine

#  Run a shell escape
sub shellEscape($)
{
my $line = shift;
#  Expand 'a through 'z labels
$errorMsg = "";
$line =~ s/\B'([a-z])\b/expandLabeledLine($1)/ge;
return 0 if length $errorMsg;
$line =~ s/'_/$fname/g;
$line =~ s/'\./fetchLine($dot,1)/ge;
if($doslike) {
#  Just run system and hope for the best.
system $line;
} else {
#  Unix has a concept of shells.
my $shell = $ENV{SHELL};
$shell = "/bin/sh" if ! defined $shell;
if(length $line) {
system $shell, "-c", $line;
} else {
system $shell;
}
}  # dos or unix
print "ok\n";
return 1;
}  # shellEscape

#  Implement various two letter commands.
#  Most of these set and clear modes.
sub twoLetter($)
{
my $line = shift;
my ($i, $j);

if($line eq "qt") { exit 0; }

if($line =~ s/^cd\s+// or $line =~ s/^cd$//) {
$cmd = 'e';  # so error messages are printed
if(length $line) {
my $temppath = `pwd`;
chomp $temppath;
if($line eq "-") {
$errorMsg = "you have no previous directory", return 0 unless defined $oldpath;
chdir $oldpath or $errorMsg = "cannot change to previous directory $oldpath", return 0;
} else {
$line = envFile($line);
return 0 if length $errorMsg;
chdir $line or $errorMsg = "invalid directory", return 0;
}
$oldpath = $temppath;
}
print `pwd`;
return 1;
}

if($line eq "rf") {
$cmd = 'e';
if($fmode & $browsemode) {
$cmd = 'b';
$fname =~ s/.browse$//;
}
length $fname or $errorMsg = "no file name", return 0;
$nostack = 1;
return -1, "$cmd $fname";
}

if($line eq "et") {
$cmd = 'e';
$fmode&$browsemode or
$errorMsg = $nobrowse, return 0;
foreach $i (1..$dol) {
$text[substr($map, $i*$lnwidth, $lnwidth1)] = fetchLine($i,1);
}
$fmode &= ~($browsemode|$firstopmode|$changemode);
$btags = [];  # don't need those any more.
print "editing as pure text\n" if $helpall;
return 1;
}

if($line eq "ub") {
$fmode&$browsemode or
$errorMsg = $nobrowse, return 0;
dropEmptyBuffers();
#  Backing out.
$map = $$btags[0]{map1};
$fname = $$btags[0]{fname};
$fmode = $$btags[0]{fmode};
$labels = $$btags[0]{labels};
$dot = $$btags[0]{dot};
$dol = $$btags[0]{dol1};
apparentSize();
return 1;
}  # reverse browse

if($line eq "f/" or $line eq "w/") {
$i = $fname;
$i =~ s,.*/,, or
$errorMsg = "filename does not contain a slash", return 0;
print "$i\n" if $helpall;
substr($line, 1, 1) = " $i";
return -1, $line;
}

if($line =~ /^f[dkt]$/) {
$fmode&$browsemode or
$errorMsg = $nobrowse, return 0;
my $key = "title";
$key = "keywords" if $line eq "fk";
$key = "description" if $line eq "fd";
my $val = $$btags[0]{$key};
if(defined $val) {
print "$val\n";
} else {
print "no $key\n";
}
return 1;
}

if($line =~ /^sm(\d*)$/) {
 $cmd = 'e';
$smMail = $1;
$altattach = 0;
$j = sendMailCurrent();
$j and print "ok\n";
return $j;
}

#  simple commands
if($line eq "sg") { $global_lhs_rhs = 1; print "substitutions global\n" if $helpall; return 1; }
if($line eq "sl") { $global_lhs_rhs = 0; print "substitutions local\n" if $helpall; return 1; }
if($line eq "ci") { $caseInsensitive = 1; print "case insensitive\n" if $helpall; return 1; }
if($line eq "cs") { $caseInsensitive = 0; print "case sensitive\n" if $helpall; return 1; }
if($line eq "dr") { $dw = 0; print "directories readonly\n" if $helpall; return 1; }
if($line eq "dw") { $dw = 1; print "directories writable\n" if $helpall; return 1; }
if($line eq "dx") { $dw = 2; print "directories writable with delete\n" if $helpall; return 1; }
if($line eq "dp") { $delprint ^= 1; print ($delprint ? "delete print\n" : "delete quiet\n"); return 1; }
if($line eq "rh") { $reroute ^= 1; print ($reroute ? "redirect html\n" : "do not redirect html\n"); return 1; }
if($line eq "pm") { $passive ^= 1; print ($passive ? "passive ftp\n" : "active ftp\n"); return 1; }
if($line eq "ph") { $pdf_convert ^= 1; print ($pdf_convert ? "pdf to html conversion\n" : "pdf raw\n"); return 1; }
if($line eq "vs") { $ssl_verify ^= 1; print ($ssl_verify ? "verify ssl connections\n" : "do not verify ssl connections (less secure)\n"); return 1; }
if($line eq "ac") { $allowCookies ^= 1; print ($allowCookies ? "accept cookies\n" : "reject cookies\n"); return 1; }
if($line eq "sr") { $allowReferer ^= 1; print ($allowReferer ? "send refering web page\n" : "don't send refering web page\n"); return 1; }
if($line =~ s/^db *//) {
if($line =~ /^\d$/) {
$debug = $line, return 1;
} else {
$errorMsg = "please set debug level, 0 through 7", return 0;
}
}
if($line =~ s/^ua *//) {
if($line =~ /^\d+$/) {
$errorMsg = "Agent number $line is not defined", return 0 if ! defined$agents[$line];
$agent = $agents[$line], return 1;
} else {
$errorMsg = "please set user agent, 0 through ".$#agents, return 0;
}
}  # ua number
if($line eq "ff") { $fetchFrames ^= 1; print ($fetchFrames ? "fetch frames\n" : "do not fetch frames\n"); return 1; }
if($line eq "tn") { $textAreaCR ^= 1; print ($textAreaCR ? "dos newlines on text areas\n" : "unix newlines on text areas\n"); return 1; }
if($line eq "eo") { $endmarks = 0; print "end markers off\n" if $helpall; return 1; }
if($line eq "el") { $endmarks = 1; print "end markers list\n" if $helpall; return 1; }
if($line eq "ep") { $endmarks = 2; print "end markers on\n" if $helpall; return 1; }
return -1,"^".length($1) if $line =~ /^(\^+)$/;
return stripChild() if $line eq "ws";
return unstripChild() if $line eq "us";

return -1, $line;  # no change
}  # twoLetter

#  Evaluate the entered command.
#  This is indirectly recursive, as in g/z/ s/x/y/
#  Pass the command line, and return success or failure.
sub evaluate($)
{
my $line = shift;
my ($i, $j, @pieces, $h, $href);
my $postspace = 0;
my $postBrowse;
my $nsuf = -1;  # numeric suffix
my $cx;  # context specified -- always $nsuf - 1
my $section = "";  # section within a document
my $post = "";  # for post cgi method
$nostack = 0;  # suppress stacking of edit sessions

$referer = "";
$referer = $fname if $allowReferer;
$referer =~ s/\.browse$//;

$cmd = "";
#  We'll allow whitespace at the start of an entered command.
$line =~ s/^\s*//;
#  Watch for successive q commands.
$lastq = $lastqq, $lastqq = -1;

if(!$inglob) {
#  We'll allow comments in an edbrowse script
return 1 if $line =~ /^#/;

return shellEscape $line if $line =~ s/^!\s*//;

#  Web express shortcuts
if($line =~ s/^@ *//) {
if(! length $line) {
my @shortList = ();
foreach $i (sort keys %shortcut) {
$j = $i;
my ($desc, $sort);
defined ($desc = $shortcut{$i}{desc}) and
$j .= " = $desc";
$j = "|$j";
defined ($sort = $shortcut{$i}{sort}) and
$j = "$sort$j";
$j .= "\n";
push @shortList, $j;
}  # loop over shortcuts
foreach (sort @shortList) {
s/^.*?\|//;
print $_;
}
return 1;
}
$cmd = '@';
($j, $line, $postBrowse) = webExpress($line);
return 0 unless $j;
$line =~ s%^%b http://%;
if($line =~ /\*/) {
$post = $line;
$post =~ s/.*\*/*/;
$line =~ s/\*.*//;
}
}

#  Predefined command sets.
if($line =~ s/^< *//) {
if(!length $line) {
foreach $i (sort keys %commandList) {
print "$i\n";
}
return 1;
}
$i = $commandList{$line};
defined $i or $errorMsg = "command set $line is not recognized", return 0;
return evaluateSequence($i, $commandCheck{$line});
}  # command set

#  Two letter commands.
($j, $line) = twoLetter($line);
return $j if $j >= 0;
}  # not in global

$startRange = $endRange = $dot;  # default, if no range given
$line = '+' if ! length $line;
$line = ($dol ? 1 : 0) . $line if substr($line, 0, 1) eq ',';
if($line =~ /^j/i) {
$endRange = $dot + 1;
$errorMsg = "line number too large", return "" if $endRange > $dol;
} elsif(substr($line, 0, 1) eq '=') {
$startRange = $endRange = $dol;
} elsif($line =~ /^[wgv]/ and $line !~ /^g\s*\d*$/) {
$startRange = 1, $endRange = $dol;
$startRange = 0 if ! $dol;
} elsif($line =~ s/^;//) {
$endRange = $dol;
} else {
@pieces = getRangePart($line);
$inglob = 0, return 0 if $#pieces < 0;
$startRange = $endRange = $pieces[0];
$line = $pieces[1];
if($line =~ s/^,//) {
$endRange = $dol;  # new default
if($line =~ /^[-'.\$+\d\/?]/) {
@pieces = getRangePart($line);
$inglob = 0, return 0 if $#pieces < 0;
$endRange = $pieces[0];
$line = $pieces[1];
}  # second address
}  # comma present
}  # end standard range processing

#  lc lower case, uc upper case
$line =~ s:^([lmu]c|ue)$:s/.*/$1/:;
if($line eq "bl") {  # break the line
dirBrowseCheck("break line") or return 0;
$line = "sbl";
}

$cmd = substr($line, 0, 1);
if(length $cmd) { $line = substr($line, 1); } else { $cmd = 'p'; }
$icmd = $cmd;
$startRange <= $endRange or
$errorMsg = "bad range", return 0;
index($valid_cmd, $cmd) >= 0 or
$errorMsg = "unknown command $cmd", $inglob = 0, return 0;

#  Change some of the command codes, depending on context
$cmd = 'I' if $cmd eq 'i' and $line =~ /^[$valid_delim\d<*]/o;
$cmd = 'I' if $cmd eq 's' and $fmode&$browsemode;
$cmd = 's' if $cmd eq 'S';
my $writeMode = ">";
if($cmd eq "w" and substr($line, 0, 1) eq "+") {
$writeMode = ">>";
$line =~ s/^.//;
}

!($fmode&$dirmode) or index($dir_cmd, $cmd) >= 0 or
$errorMsg = "$icmd $nixdir", $inglob = 0, return 0;
!($fmode&$browsemode) or index($browse_cmd, $cmd) >= 0 or
$errorMsg = "$icmd $nixbrowse", $inglob = 0, return 0;
$startRange > 0 or index($zero_cmd, $cmd) >= 0 or
$errorMsg = "zero line number", return 0;
$postspace = 1 if  $line =~ s/^\s+//;
if(index($spaceplus_cmd, $cmd) >= 0 and
! $postspace and length $line and
$line !~ /^\d+$/) {
$errorMsg = "no space after command";
return 0;
}

#  env variable and wild card expansion
if(index("brewf", $cmd) >= 0 and length $line) {
$line = envFile($line);
return 0 if length $errorMsg;
}

if($cmd eq 'B') {
return balanceLine($line);
}

if($cmd eq 'z') {
$startRange = $endRange + 1;
$endRange = $startRange;
$startRange <= $dol or
$errorMsg = "line number too large", return 0;
$cmd = 'p';
$line = $last_z if ! length $line;
if($line =~ /^(\d+)\s*$/) {
$last_z = $1;
$last_z = 1 if $last_z == 0;
$endRange += $last_z - 1;
$endRange = $dol if $endRange > $dol;
} else {
$errorMsg = "z command should be followed by a number", return 0;
}
$line = "";
}

#  move/copy destination, the third address
if($cmd eq 'm' or $cmd eq 't') {
length $line or
$errorMsg = "no move/copy destination", $inglob = 0, return 0;
$line =~ /^[-'.\$+\d\/?]/ or
$errorMsg = "invalid move/copy destination", $inglob = 0, return 0;
@pieces = getRangePart($line);
$inglob = 0, return 0 if $#pieces < 0;
$dest = $pieces[0];
$line = $pieces[1];
$line =~ s/^\s*//;
}  # move copy destination
if($cmd eq 'a') {
($line eq "+") ? ($line = "") : ($linePending = undef);
} else {
$linePending = undef;
}
! length $line or index($nofollow_cmd, $cmd) < 0 or
$errorMsg = "unexpected text after the $icmd command", $inglob = 0, return 0;

#  We don't need trailing whitespace, except for substitute or global substitute.
index("sgvI", $cmd) >= 0 or
$line =~ s/\s*$//;

! $inglob or
index($global_cmd, $cmd) >= 0 or
$errorMsg = "the $icmd command cannot be applied globally", $inglob = 0, return 0;

if($cmd eq 'h') {
$errorMsg = "no errors" if ! length $errorMsg;
print $errorMsg,"\n";
return 1;
}

if($cmd eq 'H') {
$helpall ^= 1;
print "help messages on\n" if $helpall;
return 1;
}  # H

if(index("lpn", $cmd) >= 0) {
foreach $i ($startRange..$endRange) {
dispLine($i);
$dot = $i;
last if $intFlag;
}
return 1;
}

if($cmd eq '=') {
print $endRange,"\n";
return 1;
}

if($cmd eq 'u') {
$fmode&$firstopmode or
$errorMsg = "nothing to undo", return 0;
goUndo();
return 1;
}  # u

if($cmd eq 'k') {
$line =~ /^[a-z]$/ or
$errorMsg = "please enter k[a-z]", return 0;
$startRange == $endRange or
$errorMsg = "cannot label an entire range", return 0;
substr($labels, (ord($line) - ord('a'))*$lnwidth, $lnwidth) =
sprintf $lnformat, $endRange;
return 1;
}

$nsuf = $line if $line =~ /^\d+$/ and ! $postspace;
$cx = $nsuf - 1;

if($cmd eq 'f') {
if($nsuf >= 0) {
(cxCompare($cx) and cxActive($cx)) or return 0;
$j = $fname[$cx];
print(length($j) ? $j : "no file");
print " [binary]" if $fmode[$cx]&$binmode;
print "\n";
return 1;
}
if(length $line) {
$errorMsg = "cannot change the name of a directory", return 0 if $fmode&$dirmode;
$fname = $line;
} else {
print(length($fname) ? $fname : "no file");
print " [binary]" if $fmode&$binmode;
print "\n";
}
return 1;
}  # f

if($cmd eq 'q') {
$nsuf < 0 or (cxCompare($cx) and cxActive($cx)) or return 0;
if($nsuf < 0) {
$cx = $context;
$errorMsg = "unexpected text after the $icmd command", return 0 if length $line;
}
cxReset($cx, 1) or return 0;
return 1 if $cx != $context;
#  look around for another active session
while(1) {
$cx = 0 if ++$cx > $#factive;
exit 0 if $cx == $context;
next if ! defined $factive[$cx];
cxSwitch($cx, 1);
return 1;
}
}  # q

if($cmd eq 'w') {
if($nsuf >= 0) {
$writeMode eq ">" or
$errorMsg = "sorry, append to buffer not yet implemented", return 0;
return writeContext($cx)
}
$line = $fname if ! length $line;
if($fmode&$dirmode and $line eq $fname) {
$errorMsg = "cannot write to the directory; files are modified as you go";
return 0;
}
return writeFile($writeMode, $line) if length $line;
$errorMsg = "no file specified";
return 0;
}  # w

#  goto a file in a directory
if($fmode&$dirmode and $cmd eq 'g' and ! length $line) {
$cmd = 'e';
$line = $dirname . '/' . fetchLine($endRange, 0);
}

if($cmd eq 'e') {
return (cxCompare($cx) and cxSwitch($cx, 1)) if $nsuf >= 0;
if(!length $line) {
$j = $context + 1;
print "session $j\n";
return 1;
}
}  # e

if($cmd eq 'g' and $line =~ /^\d*$/) {
($j, $line) = hyperlink($line);
return 0 unless $j;
#  Go on to browse the file.
}  # goto link

if($cmd eq '^') {
! length $line or $nsuf >= 0 or
$errorMsg = "unexpected text after the ^ command", return 0;
$nsuf = 1 if $nsuf < 0;
while($nsuf) {
$errorMsg = "no previous text", return 0 if ! defined $backup;
cxReset($context, 2) or return 0;
$h = $backup;
$backup = $$h{backup};
cxUnpack($h);
--$nsuf;
}
#  Should this print be inside or outside the loop?
if($dot) { dispLine($dot); } else { print "empty file\n"; }
return 1;
}  # ^

if($cmd eq 'A') {
return showLinks();
}  # A

if($icmd eq 's' or $icmd eq 'S') {
#  A few shorthand notations.
if($line =~ /^([,.;:!?)"-])(\d?)$/) {
my $suffix = $2;
$line = "$1 +";
#  We have to escape the question mark and period
$line =~ s/^([?.])/\\$1/;
$line = "/$line/$1\\n";
$line .= "/$suffix" if length $suffix;
}
}  # original command was s

readyUndo if ! $inglob;

if($cmd eq 'g' or $cmd eq 'v') {
return doGlobal($line);
}  # global

if($cmd eq 'I') {
$fmode&$browsemode or $errorMsg = $nobrowse, $inglob = 0, return 0;

if($line =~ /^\d*\?/) {  # status
$inglob and $errorMsg = $inoglobal, $inglob = 0, return 0;
$startRange == $endRange or $errorMsg = $inorange, return 0;
infIndex($endRange, $line) > 0 or return 0;
infStatus($line);
return 1;
}  # get info on input field

if($line =~ /^\d*([=<])/) {
my $asg = $1;
$subprint = 1;
my $yesdot = 0;
my $t = $line;
$t =~ s/^\d*[=<]//;
if($asg eq '<') {
if($t =~ /^\d+$/) {
my $cx = $t-1;
cxCompare($cx) and cxActive($cx) or $inglob = 0, return 0;
my $dolcx = $dol[$cx];
$dolcx == 1 or $errorMsg = "session $t should contain exactly one line", $inglob = 0, return 0;
$t = fetchLineContext(1, 1, $cx);
} else {
$errorMsg = "";
$t = envFile $t;
length($errorMsg) and $inglob = 0, return 0;
open FH, $t or $errorMsg = "cannot open $t, $!", $inglob = 0, return 0;
$t = <FH>;
defined $t or $errorMsg = "empty file", $inglob = 0, return 0;
if(defined <FH>) {
close FH;
$errorMsg = "file contains more than one line";
$inglob = 0;
return 0;
}
close FH;
$t =~ s/[\r\n]+$//;
}
}  # I<file
foreach $i ($startRange..$endRange) {
my $rc = infIndex($i, $line);
next unless $rc;
$dot = $i;
$rc > 0 and infReplace($t) or $inglob = 0, return 0;
$yesdot = $dot;
}  # loop over lines
if($yesdot) {
dispLine($yesdot) if ! $inglob;
return 1;
}
$errorMsg = "no input fields present" if ! $inglob;
return 0;
}  # i=

if($line =~ /^\d*\*$/) {
$inglob and $errorMsg = $inoglobal, $inglob = 0, return 0;
$startRange == $endRange or $errorMsg = $inorange, return 0;
infIndex($endRange, $line) > 0 or return 0;
($j, $line, $post) = infPush();
#  return code of -1 means there's more to do.
return $j unless $j < 0;
} elsif( $line !~ m&^\d*[$valid_delim]&o) {
$errorMsg = "unknown input field directive, please use I? or I= or I/text/replacement/";
return 0;
}
}  # input field

#  Pull section indicator off of a url.
$section = $1 if $cmd eq 'b' and $line =~ s/(#.*)//;

if(($cmd eq 'b' or $cmd eq 'e') and length $line) {
$h = undef;
$h = cxPack() if $dol and ! $nostack;
cxReset($context, 0) or return 0;
$startRange = $endRange = 0;
$changeFname = "";
if($line =~ /^\x80mail\x80(.*)$/) {  # special code for sendmail link
$href = $1;
my $subj = urlSubject(\$href);
$subj = "Comments" unless length $subj;
if(lineLimit 2) {
$i = 0;
} else {
$i = 1;
push @text, "To: $href";
$map .= sprintf($lnformat, $#text);
push @text, "Subject: $subj";
$map .= sprintf($lnformat, $#text);
$dot = $dol = 2;
print "SendMail link.  Compose your mail, type sm to send, then ^ to get back.\n";
apparentSize();
}
} else {
$fname = $line;
$i = readFile($fname, $post);
$fmode &= ~($changemode|$firstopmode);
}
$filesize = -1, cxUnpack($h), return 0 if !$i and ! $dol and is_url($fname);
if(defined $h) {
$$h{backup} = $backup if defined $backup;
$backup = $h;
}
return 0 if ! $i;
$fname = $changeFname if length $changeFname;
$cmd = 'e' if $fmode&$binmode or ! $dol;
return 1 if $cmd eq 'e';
}

if($cmd eq 'b') {
if(! ($fmode&$browsemode)) {
readyUndo();
print("$filesize\n"), $filesize = -1 if $filesize >= 0;
render() or return 0;
if(defined $postBrowse) {
$$btags[0]{pb} = $postBrowse;
evaluateSequence($postBrowse, 0);
if($$btags[0]{dol2} > $dol) {
$fmode &= ~($changemode|$firstopmode);
apparentSize();
}
}
} else {
$errorMsg = "already browsing", return 0 if ! length $section;
}
return 1 if ! length $section;
$section =~ s/^#//;
$j = findSection($section);
$errorMsg = "cannot locate section #$section", return 0 unless $j;
$dot = $j;
dispLine($dot);
return 1;
}  # b

if($cmd eq 'm' or $cmd eq 't') {
return moveCopy();
}

if($cmd eq 'i') {
$cmd = 'a';
--$startRange, --$endRange;
}

if($cmd eq 'c') {
delText($startRange, $endRange) or return 0;
$endRange = --$startRange;
$cmd = 'a';
}

if($cmd eq 'a') {
return readLines();
}

if($cmd eq 'd') {
$i = ($endRange == $dol);
if($fmode & $dirmode) {
$j = delFiles();
} else {
$j = delText($startRange, $endRange);
}
$inglob = 0 if ! $j;
if($j and $delprint and ! $inglob) {
$i ? print "end of file\n" : dispLine($dot);
}
return $j;
}  # d

if($cmd eq 'j' or $cmd eq 'J') {
return joinText();
}  # j

if($cmd eq 'r') {
return readContext($cx) if $nsuf >= 0;
return readFile($line, "") if length $line;
$errorMsg = "no file specified";
return 0;
}  #  r

if($cmd eq 's' or $cmd eq 'I') {
$j = substituteText($line);
$inglob = $j = 0 if $j < 0;
return $j;
}  # substitute

$errorMsg = "command $icmd not yet implemented";
$inglob = 0;
return 0;
}  # evaluate

sub evaluateSequence($$)
{
my $commands = shift;
my $check = shift;
foreach my $go (@$commands) {
$inglob = 0;
$intFlag = 0;
$filesize = -1;
my $rc = evaluate($go);
print "$filesize\n" if $filesize >= 0;
$rc or ! $check or
return 0;
}
return 1;
}  # evaluateSequence

#  Hash to map html tags onto their English descriptions.
#  For instance, P maps to "paragraph".
#  Most of the tags, such as FONT, map to nothing,
#  whence they are thrown away.
#  The first two characters are not part of the description.
#  It forms a number that describes the nestability of the tag.
#  Bit 1 means the tag should be nested, like parentheses.
#  In fact all the bit1 tags should nest amongst eachother, unlike
#  <UL> <TABLE> </UL> </TABLE> (nesting error).
#  Bit 2 means a tag may appear inside itself, like nested lists.
#  Bit 4 means the tag implies a paragraph break.
#  Bit 8 means we retain attributes on the positive tag.
#  bit 16 means to close an open anchor *before* applying this tag
%tagdesc = (
sub => "11a subscript",
font => " 3a font",
center => " 3centered text",
sup => "11a superscript",
title => "17the title",
head => "17the html header information",
body => "27the html body",
bgsound => "24background music",
meta => " 8a meta tag",
base => " 8base reference for relative URLs",
img => " 8an image",
br => " 0a line break",
p => "20a paragraph",
blockquote => "20a quoted paragraph",
div => "20a divided section",
h => "21a header",
dt => "20a term",
dd => "20a definition",
hr => "16a horizontal line",
ul => "23a bullet list",
ol => "23a numbered list",
dl => "23a definition list",
li => "16a list item",
form => "25a form",
input => "24an input item",
a => "25an anchor",
frame => "28a frame",
map => "28An image map",
area => "24an image map area",
#  I've seen tables nested inside tables -- I don't know why!
table => "31a table",
tr => "19a table row",
td => "19a table entry",
th => "19a table heading",
pre => " 5a preformatted section",
xmp => " 5a preformatted section",
address => " 5a preformatted section",
script => " 1a script",
style => " 1a style block",
noframes => " 1noframe section",
select => "25an option list",
textarea => "25an input text area",
option => "24a select option",
#  The following tags convey formatting information that is eventually
#  discarded, but I'll track them for a while,
#  just to verify nestability.
em => " 1a block of emphasized text",
strong => " 1a block of emphasized text",
b => " 1a block of bold text",
i => " 1a block of italicized text",
code => " 1a block of sample code",
samp => " 1a block of sample code",
);

#  We encode tags in a @tag attribute=value attribute=value ...@ format,
#  though of course we don't use the @ sign.
#  We use \x80, which should not appear in international text.
#  I simply hard code it - it makes things simpler.

#  Support routine, to encode a tag.
#  Run from within a global substitute.
#  Pas the name of the tag, slash, and tag arguments
sub processTag($$$)
{
my ($tag, $slash, $attributes) = @_;
my $nlcount = $attributes =~ y/\n/\n/;  # newline count
my $doat = 0;  # do attributes
$tag = lc $tag;
my $desc = $tagdesc{$tag};
if(defined $desc) {
$doat = (substr($desc, 0, 2) & 8);
} else {
$tag = "z";
}
#  Do we need to gather up the attributes?
if(!$doat or $slash eq "/") {
#  Guess not, just return the tag.
return "" if $tag eq "z" and ! $nlcount;
return "\x80$tag$slash$nlcount\x80";
}
#  Process each whitespace separated chunk, taking quotes into account.
#  note that name="foo"size="1" is suppose to be two separate tags;
#  God help us!
#  Borrow a global variable, even though this may not be an input tag.
$itag = {tag => $tag};
push @$btags, $itag;
$attributes =~ s/(  # replace the entire matched text
\w+  # attribute name
(?>\s*=\s*  # as in name=value
(?>  # a sequence of choices
[^\s"']+  # regular printable characters
|
"[^"]*"  # double quoted string
|
'[^']*'  # single quoted string
)  # one of three formats
)?  # =value
)/processAttr($1)/xsge;
#  Capture description and keywords.
if($tag eq "meta") {
my $val = $$itag{name};
if(defined $val) {
$val = lc $val;
if($val eq "description" or $val eq "keywords") {
my $content = $$itag{content};
if(defined $content) {
stripWhite \$content;
$$btags[0]{$val} = $content if length $content;
}  # content
}  # description or keywords
}  # name=
pop @$btags;
return "" unless $nlcount;
return "\x80z$nlcount\x80";
}  # meta tag
my $tagnum = $#$btags;
return "\x80$tag$nlcount,$tagnum\x80";
}  # processTag

#  Support routine, to crack attribute=value.
sub processAttr($)
{
my $line = shift;
#  Get rid of spaces around first equals.
$line =~ s/^([^=\s]*)\s*=\s*/$1=/;
#  Get rid of the quotes.
$line =~ s/("[^"]*"|'[^']*')/substr($1,1,-1)/sge;
my $attr = lc $line;
$attr =~ s/\s*=.*//s;
return "" unless $attr =~ /^\w+$/;
$line =~ s/^[^=]*=//s
or $line = "";
$line =~ s/&([a-zA-Z]+|#\d+);/metaChar($1)/ge;
$$itag{$attr} = $line;
return "";
}  # processAttr

#  Support routine, to encode a bang tag.
#  Run from within a global substitute.
sub processBangtag($)
{
my $item = shift;
if($item eq "'" or $item eq '"') {
return (length $bangtag ? " " : $item);
}
if(substr($item, 0, 1) eq '<') {
return "" if length $bangtag;
return $item if $item eq "<";
$bangtag = substr $item, 1;
return "<z ";
}
return $item unless length $bangtag;
#  dashes at the front require dashes at the end.
#  But (apparently) they don't have to be the same number of dashes.
#  I really don't understand this syntax at all!
#  It is suppose to follow the rules in
#  http://www.htmlhelp.com/reference/wilbur/misc/comment.html
#  but real web pages hardly ever follow these rules!
substr($item, -1) = "";  # don't need that last >
my $l = length($bangtag) - 1;
$l &= ~1;  # back down to an even number
return " " if $l and ! length $item;  # lone > inside a comment
$bangtag = "";
return ">";
}  # processBangtag

#  Turn <>'" in javascript into spaces, as we did above.
sub processScript($)
{
my $item = shift;
if(length($item) < 5) {
return ($inscript ? " " : $item);
}
#  now $item is <script or </script
#  Try to guard against Java code that looks like
#  document_write("<script bla bla bla>\n";
#  There's a lot of this going around.
$prequote = 0;
$prequote = 1 if $item =~ s/^\( *['"]//;
return ' ' if $inscript and $prequote;
if(substr($item, 1, 1) eq '/') {
--$inscript if $inscript;
} else {
++$inscript;
}
return $item;
}  # processScript

sub backOverSpaces($)
{
my $trunc = shift;
my $j = length($refbuf) - 1;
--$j while $j >= 0 and substr($refbuf, $j, 1) =~ /[ \t]/;
++$j;
substr($refbuf, $j) = "" if $trunc;
return $j;
}  # backOverSpaces

#  Recompute space value, after the buffer has been cropped.
#  0 = word, 1 = spaces, 2 = newline, 3 = paragraph.
sub computeSpace()
{
return 3 if ! length $refbuf;
my $last = substr $refbuf, -1;
return 0 if $last !~ /\s/;
return 1 if $last ne "\n";
return 2 if substr($refbuf, -2) ne "\n\n";
return 3;
}  # computeSpace

#  Here are the common keywords for mail header lines.
#  These are in alphabetical order, so you can stick more in as you find them.
#  The more words we have, the more accurate the test.
#  Value = 1 means it might be just a "NextPart" mime header,
#  rather than a full-blown email header.
#  Value = 2 means it could be part of an English form.
#  Value = 4 means it's almost certainly a line in a mail header.
%mhWords = (
"action" => 2,
"arrival-date" => 4,
"content-transfer-encoding" => 1,
"content-type" => 1,
"date" => 2,
"delivered-to" => 4,
"errors-to" => 4,
"final-recipient" => 4,
"from" => 2,
"importance" => 4,
"last-attempt-date" => 4,
"list-id" => 4,
"mailing-list" => 4,
"message-id" => 4,
"mime-version" => 4,
"precedence" => 4,
"received" => 4,
"remote-mta" => 4,
"reply-to" => 4,
"reporting-mta" => 4,
"return-path" => 4,
"sender" => 4,
"status" => 2,
"subject" => 4,
"to" => 2,
"x-beenthere" => 4,
"x-loop" => 4,
"x-mailer" => 4,
"x-mailman-version" => 4,
"x-mimeole" => 4,
"x-ms-tnef-correlator" => 4,
"x-msmail-priority" => 4,
"x-priority" => 4,
"x-uidl" => 4,
);

#  Get a filename from the user.
sub getFileName($$)
{
my $startName = shift;
my $isnew = shift;
input: {
print "Filename: ";
print "[$startName] " if defined $startName;
my $line = <STDIN>;
exit 0 unless defined $line;
stripWhite \$line;
if($line eq "") {
redo input if ! defined $startName;
$line = $startName;
} else {
$startName = undef;
$line = envLine $line;
print("$errorMsg\n"), redo input if length $errorMsg;
}  # blank line
if($isnew and -e $line) {
print "Sorry, file $line already exists.\n";
$startName = undef;
redo input;
}
return $line;
}
}  # getFileName

#  Get a character from the tty, raw mode.
#  For some reason hitting ^c in this routine doesn't leave the tty
#  screwed up.  I don't know why not.
sub userChar
{
my $choices = shift;
input: {
#  Too bad there isn't a perl in-built for this.
#  I don't know how to do this in Windows.  Help anybody?
system "stty", "-icanon", "-echo";
my $c = getc;
system "stty", "icanon", "echo";
if(defined $choices and index($choices, $c) < 0) {
STDOUT->autoflush(1);
print "\a\b";
STDOUT->autoflush(0);
redo input;
}
return $c;
}
}  # userChar

#  Encode html page or mail message.
#  No args, the html is stored in @text, as indicated by $map.
sub render()
{
$dol or $errorMsg = "empty file", return 0;
$errorMsg = "binary file", return 0 if $fmode&$binmode;
$errorMsg = "cannot render a directory", return 0 if $fmode&$dirmode;

my ($i, $j, $k, $rc);
my $type = "";
$btags[$context] = $btags = [];
$$btags[0] = {tag => "special", fw => {} };

#  If it starts with html, head, or comment, we'll call it html.
my $tbuf = fetchLine 1, 0;
if($tbuf =~ /^\s*<(?:!-|html|head|meta)/i) {
$type = "html";
}

if(! length $type) {
#  Check for mail header.
#  There might be html tags inside the mail message, so we need to
#  look for mail headers first.
#  This is a very simple test - hopefully not too simple.
#  The first 20 non-indented lines have to look like mail header lines,
#  with at least half the keywords recognized.
$j = $k = 0;
for $i (1..$dol) {
my $line = fetchLine $i, 0;
last unless length $line;
next if $line =~ /^[ \t]/;  # indented
++$j;
next unless $line =~ /^([\w-]+):/;
my $word = lc $1;
my $v = $mhWords{$word};
++$k if $v;
if($k >= 4 and $k*2 >= $j) {
$type = "mail";
last;
}
last if $j > 20;
}
}

if($type ne "mail") {
#  Put the lines together into one long string.
#  This is necessary to check for, and render, html.
$tbuf .= "\n";
$tbuf .= fetchLine($_, 0) . "\n" foreach (2..$dol);
}

if(! length $type) {
#  Count the simple html tags, we need at least two per kilabyte.
$i = length $tbuf;
$j = $tbuf =~ s/(<\/?[a-zA-Z]{1,7}\d?[>\s])/$1/g;
$j = 0 if $j eq "";
$type = "html" if $j * 500 >= $i;
}

if(! length $type) {
$errorMsg = "this doesn't look like browsable text";
return 0;
}

$badHtml = 0;
$badHtml = 1 if is_url($fname);
$rc = renderMail(\$tbuf) if $type eq "mail";
$rc = renderHtml(\$tbuf) if $type eq "html";
return 0 unless $rc;

pushRenderedText(\$tbuf) or return 0;
if($type eq "mail") {
$fmode &= ~$browsemode;  # so I can run the next command
evaluate(",bl");
$errorMsg = "";
$dot = $dol;
$fmode &= ~$changemode;
$fmode |= $browsemode;
}
apparentSize();
$tbuf = undef;

if($type eq "mail" and $nat) {
print "$nat attachments.\n";
$j = 0;
foreach $curPart (@mimeParts) {
next unless $$curPart{isattach};
++$j;
print "Attachment $j\n";
my $filename = getFileName($$curPart{filename}, 1);
next if $filename eq "x";
if($filename eq "e") {
print "session " . (cxCreate(\$$curPart{data}, $$curPart{filename})+1) . "\n";
next;
}
if(open FH, ">$filename") {
binmode FH, ':raw' if $doslike;
print FH $$curPart{data}
or dieq "Cannot write to attachment file $filename, $!.";
close FH;
} else {
print "Cannot create attachment file $filename.\n";
}
}  # loop over attachments
print "attachments complete.\n";
}  # attachments present

return 1;
}  # render

#  Pass the reformatted text, without its last newline.
sub pushRenderedText($)
{
my $tbuf = shift;

#  Replace common nonascii symbols
#  I don't know what this pair of bytes is for!
$$tbuf =~ s/\xe2\x81//g;

#  Transliterate alternate forms of quote, apostrophe, etc.
#  We replace escape too, cuz it shouldn't be there anyways, and it messes up
#  some terminals, and some adapters.
#  Warning!!  Don't change anything in the range \x80-\x8f.
#  These codes are for internal use, and mus carry through.
$$tbuf =~ y/\x1b\x95\x99\x9c\x9d\x92\x93\x94\xa0\xad\x96\x97/_*'`''`' \55\55\55/;

#  Sometimes the bullet list indicator is falsely separated from the subsequent text.
$$tbuf =~ s/\n\n\*\n\n/\n\n* /g;

#  Turn nonascii math symbols into our encoded versions of math symbols,
#  to be handled like Greek letters etc, in a consistent manner,
#  by the next block of code.
$$tbuf =~ s/\xb0/\x82176#/;  # degrees
$$tbuf =~ s/\xbc/\x82188#/;  # 1 fourth
$$tbuf =~ s/\xbd/\x82189#/;  # 1 half
$$tbuf =~ s/\xbe/\x82190#/;  # 3 fourths
$$tbuf =~ s/\xd7/\x82215#/;  # times
$$tbuf =~ s/\xf7/\x82247#/;  # divided by

if($$tbuf =~ /\x82\d+#/) {  # we have codes to expand.
#  These symbols are going to become words -
#  put spaces on either side, if the neighbors are also words.
$$tbuf =~ s/#\x82/# \x82/g;
$$tbuf =~ s/([a-zA-Z\d])(\x82\d+#)/$1 $2/g;
$$tbuf =~ s/(\x82\d+#)([a-zA-Z\d])/$1 $2/g;
$$tbuf =~ s/\x82(\d+)#/$symbolWord{$1}/ge;
}

#  Now push into lines, for the editor.
my $j = $#text;
if(length $$tbuf) {
push @text, split "\n", $$tbuf, -1;
} else {
push @text, "";
}
$#text = $j, return 0 if lineLimit 0;

$$btags[0]{map1} = $map;
$$btags[0]{dot} = $dot;
$$btags[0]{dol1} = $dol;
$dot = $dol = $#text - $j;
$$btags[0]{dol2} = $dol;
$map = $lnspace;
$map .= sprintf($lnformat, $j) while ++$j <= $#text;
$$btags[0]{map2} = $map;
$fmode &= ~$firstopmode;
$$btags[0]{fname} = $fname;
$$btags[0]{fmode} = $fmode;
$$btags[0]{labels} = $labels;
$fmode &= $changemode;  #  only the change bit retains its significance
$fmode |= $browsemode;
$labels = $lnspace x 26;
$fname .= ".browse" if length $fname;
return 1;
}  # pushRenderedText

#  Pass in the text to be rendered, by reference.
#  The text is *replaced* with the rendered text.
sub renderHtml($)
{
my $tbuf = shift;
my ($i, $j, $ofs1, $ofs2, $h);  # variables

$baseref = $fname;

#  Ok, here's a real kludge.
#  The utility that converts pdf to html,
#  access.adobe.com/simple_form.html, has a few quirks.
#  One of the common problems in the translation is
#  the following meaningless string, that appears over and over again.
#  I'm removing it here.
$$tbuf =~ s/Had\strouble\sresolving\sdest\snear\sword\s(<[\w_;:\/'"().,-]+>\s)?action\stype\sis\sGoToR?//g;

#  I don't expect any overstrikes, but just in case ...
$$tbuf =~ s/[^<>"'&\n]\10//g;
#  Get rid of any other backspaces.
$$tbuf =~ y/\10/ /d;

#  Make sure there aren't any \x80 characters to begin with.
$$tbuf =~ y/\x80/\x81/;

#  As far as I can tell, href=// means href=http://
#  Is this documented anywhere??
$$tbuf =~ s,\bhref=(["']?)//\b,HREF=$1http://,ig;

#  Find the simple window javascript functions
$refbuf = "";
$lineno = $colno = 1;
$lspace = 3;
javaFunctions($tbuf);

#  Before we do the standard tags, get rid of the <!-- .. --> tags.
#  I turn them into <z ... > tags,
#  which will be disposed of later, along with all the
#  other unrecognized tags.
#  This is not a perfect implementation.
#  It will glom onto the <! inside <A HREF="xyz<!stuff">,
#  and it shouldn't; but niehter should you be writing such a perverse string!
$$tbuf =~ s/<!-*>//g;
$bangtag = "";
$$tbuf =~ s/(['"]|<(!-*)?|-*>)/processBangtag($1)/ge;
print "comments stripped\n" if $debug >= 6;

$errorMsg = $intMsg, return 0 if $intFlag;

#  A good web page encloses its javascript in comments <!-- ... -->,
#  But some don't, and the (sometimes quoted) < > characters
#  really mess us up.  Let's try to strip the javascript,
#  or any other script for that matter.
$inscript = 0;
$$tbuf =~ s/((?>(\( *['"])?<(\/?script[^>]*>)?|[>"']))/processScript($1)/gei;
print "javascript stripped\n" if $debug >= 6;

$errorMsg = $intMsg, return 0 if $intFlag;

#  I'm about to crack html tags with one regexp,
#  and that would be entirely doable, if people and web tools didn't
#  generate crappy html.
#  The biggest problem is unbalanced quotes, whence the open quote
#  swallows the rest of the document in one tag.
#  I'm goint to *try*, emphasis on try, to develop a few heuristics
#  that will detect some of the common misquotings.
#  This stuff should be written in C, a complex procedural algorithm.
#  But I don't have the time or inclination to translate this mess into C,
#  and perl is not the write language to write an algorithm like that.
#  I've seen examples of all of these syntactical nightmares on the web,
#  and others that I can't possibly code around.
#  Only one quote in the tag; get rid of it.  Tag is on one line.
$$tbuf =~ s/<(\/?[a-zA-Z][^<>'"]*)['"]([^<>'"]*)>/<$1$2>/g;
#  Two quotes before the last >, but not ="">, which would be ok.
$$tbuf =~ s/([^= <>])"">/$1">/g;
$$tbuf =~ s/([^= <>])''>/$1'>/g;
#  Missing quote before the last >   "word>
#  It's usually the last > where things screw up.
$$tbuf =~ s/["'](\w+)>/$1>/g;
#  &nbsp is suppose to have a semi after it - it often doesn't.
$$tbuf =~ s/&nbsp$/&nbsp;/gi;
$$tbuf =~ s/&nbsp([^;])/&nbsp;$1/gi;
#  Well that's all I can manage right now.

#  Encode <font face=symbol> number characters.
#  This is kludgy as hell, but I want to be able to read my own math pages.
$$tbuf =~ s/<font +face=['"]?symbol['"]?> *([a-zA-Z]|&#\d+;) *<\/font>/metaSymbol($1)/gei;

#  Now let's encode the tags.
#  Thanks to perl, we can do it in one regexp.
$$tbuf =~ s/<  # start the tag
(\/?)  # leading slash turns off the tag
([a-zA-Z]+)  # name of the tag
(  # remember the attributes
(?>  # fix each subexpression as you find it
[^>"']+  # unquoted stuff inside the tag
|
"[^"]*"  # stuff in double quotes
|
'[^']*'  # stuff in single quotes
)*  # as many of these chunks as you need
)  # return the set in $3
>  # close the html tag
/processTag($2, $1, $3)/xsge;
print "tags encoded\n" if $debug >= 6;

$errorMsg = $intMsg, return 0 if $intFlag;

#  Now we can crunch the meta chars without fear.
$$tbuf =~ s/&([a-zA-Z]+|#\d+);/metaChar($1)/ge;
print "meta chars translated\n" if $debug >= 6;

$onloadSubmit = 0;
$longcut = $lperiod =  $lcomma =  $lright =  $lany = 0;

my @olcount = ();  #  Where are we in each nested number list?
my @dlcount = ();  # definition lists
my $tagnest = ".";  #  Stack the nestable html tags, such as <LI> </LI>
my $tagLock = 0;  # other tags are locked out, semantically, until this one is done
my $tagStart;  # location of the tag currently under lock
#  Locking tags are currently: title, select, textarea
my $inhref = 0;  # in anchor reference
my $intitle = 0;
my $inselect = 0;
my $inta = 0;  # text area
my $optStart;  # start location of option
my $opt;  # hash of options
my $optCount;  # count of options
my $optSel;  # options selected
my $optSize;  # size of longest option
my $lastopt;  # last option, waiting for next <option>
my $premode = 0;  # preformatted mode
my $hrefTag;
my $hrefFile = "";
$inscript = 0;
my $intable = 0;  # in table
my $intabhead = 0;  # in table header
my $intabrow = 0;  # in table row
my $inform = 0;  # in form

#  Global substitute is mighty powerful, but at this point
#  we really need to proceed token by token.
#  Going by chunks is better than shifting each character.
#  Extract a contiguous sequence of non-whitespace characters,
#  or whitespace, or a tag.
reformat:
while($$tbuf =~ /(\s+|\x80[\w\/,]+\x80|[^\s\x80]+)/gs) {
$errorMsg = $intMsg, return 0 if $intFlag;
my $chunk = $1;

#  Should we ignore line breaks in table headers?
$chunk = ' ' if ($intabhead|$inhref) and $chunk =~ /^\x80br\/?0/;

if($chunk =~ /^\s/) {  # whitespace
$j = $chunk =~ y/\n/\n/;  # count newlines
$lineno += $j;
next reformat if $inscript;
if(!$premode or $tagLock) {
next reformat if $lspace;
$chunk = " ";
$chunk = "\n" if $j and substr($refbuf, -4) =~ / [a-zA-Z]\.$/;
appendWhiteSpace($chunk, !($inhref + $tagLock));
#  Switch { and space, it looks prettier.
#  Hopefully this will never happen accept at the beginning of a link.
$inhref and $lspace == 1 and $refbuf =~ s/(\x80[\x85-\x8f]+{) $/ $1/;
next reformat;
}  # not preformatted

#  Formfeed is a paragraph break.
$j = 2 if $chunk =~ s/\f/\n\n/g;
$colno = 1 if $j;
#  Keep the whitespace after nl, it's preformatted.
$chunk =~ s/.*\n//s;
#  Note that we make no effort to track colno or lperiod etc in preformat mode.
if($lspace == 3) {
backOverSpaces(1);
$j = 0;
}
if($j == 2) {
backOverSpaces(1);
$chunk = "\n\n".$chunk if $lspace < 2;
$chunk = "\n".$chunk if $lspace == 2;
$lspace = 3;
$j = 0;
}
if(!$j) {
$refbuf .= $chunk;
next reformat;
}
#  Now j = 1 and lspace < 3
backOverSpaces(1);
$refbuf .= "\n$chunk";
$lspace = 1 if ! $lspace;
++$lspace;
next reformat;
}  # whitespace

if(substr($chunk, 0, 1) ne "\x80") {
next reformat if $inscript;
$chunk =~ y/{}/[]/ if $inhref;
$inhref = 2 if $inhref;
appendPrintable($chunk);
next reformat;
}  # token

#  It's a tag
my ($tag, $slash, $nlcount, $attrnum) =
$chunk =~ /^.([a-z]+)(\/?)(\d+)(?:,(\d+))?.$/;
#  Unless we hear otherwise, the tag is assumed to contribute no visible
#  characters to the finished document.
$chunk = "";

my $desc = $tagdesc{$tag};
$desc = " 0an unknown construct" if ! defined $desc;
my $nest = substr $desc, 0, 2;
$chunk = "\n\n" if $nest & 4;
substr($desc, 0, 2) = "";
#  Equivalent tags, as far as we're concerned
$tag = "script" if $tag eq "style";
$tag = "script" if $tag eq "noframes";
$tag = "pre" if $tag eq "xmp";
$tag = "pre" if $tag eq "address";
$tag = "frame" if $tag eq "iframe";
my $tag1 = ".$tag.";
my $tagplus = "$tag$slash";
$lineno += $nlcount, next reformat if $inscript and $tagplus ne "script/";
$attrnum = 0 if ! defined $attrnum;
#  A hidden version of the attribute number, to embed in the text.
$attrhidden = hideNumber $attrnum;
$h = undef;
if($attrnum) {
$h = $$btags[$attrnum];
$$h{lineno} = $lineno;  # source line number
}
my $openattr = 0;
my $openattrhidden;
my $openTag;
my $closeAnchor = 0;
$closeAnchor = 1 if $inhref and ($tag eq "a" or $tag eq "area" or $tag eq "frame" or $tag eq "input");
$closeAnchor = 1 if $inhref == 2 and $nest&16;

#  Make sure we open and close things in order.
if($nest&1) {
if(!$slash) {
errorConvert("$desc begins in the middle of $desc")
if index($tagnest, $tag1) >= 0 and !($nest&2);
$tagnest = ".$tag.$attrnum" . $tagnest;
push @olcount, 0 if $tag eq "ol";
push @dlcount, 0 if $tag eq "dl";
} else {
$j = index $tagnest, $tag1;
if($j < 0) {
errorConvert("an unexpected closure of $desc, which was never opened");
} else {
if($j > 0) {
my $opendesc = substr $tagnest, 1;
$opendesc =~ s/\..*//;
$opendesc = $tagdesc{$opendesc};
substr($opendesc, 0, 2) = "";
errorConvert("$desc is closed inside $opendesc");
}  # bad nesting
++$j;
substr($tagnest, $j, length($tag)+1) = "";
$openattr = substr $tagnest, $j;
$openattr =~ s/\..*//;
substr($tagnest, $j, length($openattr)+1) = "";
$openattrhidden = hideNumber $openattr;
if($openattr) {  # record the offset of </tag>
$ofs2 = backOverSpaces(0);
$openTag = $$btags[$openattr];
#  Tweak offset for the } on the anchor
++$ofs2 if $closeAnchor and $inhref == 2;
$ofs2 = $tagStart if $tagLock;
$$openTag{ofs2} = $ofs2;
}
pop @olcount if $tag eq "ol";
pop @dlcount if $tag eq "dl";
}  # was this construct open or not
}  # /tag
}  # nestable tag

#  retain the start and end of any tag worthy of attributes
if($attrnum) {
$ofs1 = backOverSpaces(0);
$ofs1 = $tagStart if $tagLock;
$$h{ofs1} = $ofs1;
}

switch: {

if($closeAnchor) {
if($inhref == 1) {  # no text in the hyperlink
if($refbuf =~ s/( *\x80[\x85-\x8f]+{[\s|]*)$//s) {
$j = $1;
$colno -= $j =~ y/ {/ {/;
} else {
warn "couldn't strip off the open anchor at line $lineno <" .
substr($refbuf, -10) . ">.";
}
$$hrefTag{tag} = "z";  # trash the anchor
} else {
$refbuf .= "}";
$refbuf =~ s/([ \n])}$/}$1/;
++$colno;
$j = $$hrefTag{href};
my $onc = $$hrefTag{onclick};
if($j =~ /^javascript/i or $onc) {
#  Let the onclick take precedence.
$j = $onc if defined $onc and length $onc;
#  See if this is a javascript function we can recognize and circumvent.
$i = javaWindow $j;
if($$hrefTag{form} and $i eq "submit") {
#  I'll assume this is a check and submit function.
#  If it only validates fields, we're ok.
#  If it reformats the data, we're screwed!
$i = $$hrefTag{ofs1};
$inf = substr $refbuf, $i;
$inf =~ s/{/</;
$inf =~ s/ +$//;
$inf =~ s/}$/ js\x80\x8f>/;
$$hrefTag{$ofs2} += 5;
if($$inform{action}) {
my $actscheme = is_url $$inform{action};
$actscheme = is_url $baseref unless $actscheme;
if($actscheme eq "https") {
$inf =~ s/ js/& secure/;
$$hrefTag{$ofs2} += 7;
}
if($$inform{action} =~ /^mailto:/i) {
$inf =~ s/ js/& mailform/;
$$hrefTag{$ofs2} += 9;
}
}
substr($refbuf, $i) = $inf;
# change it to an input field
$$hrefTag{tag} = "input";
$$hrefTag{type} = "submit";
$$hrefTag{value} = "submit";
$$inform{nnh}++;  # another non hidden field
$$inform{nif}++;
$$inform{lnh} = $h;  # last non hidden field
}
#  Is this just opening a new window, then calling the link?
elsif(length $i and $i ne "submit") {
#  Ok, I'll assume it's a new window with hyperlink
$$hrefTag{href} = $i;
} else {
print "unknown javascript ref $j\n" if $debug >= 3;
}
}
}
$lspace = computeSpace();
$inhref = 0;
last switch if $tagplus eq "a/";
}  # close the open anchor

if($tagplus eq "sup") {
$refbuf .= '^';
last switch;
}  # sup

if($tagplus eq "sup/" and defined $openTag) {
$ofs1 = $$openTag{ofs1};
++$ofs1;  # skip past ^
$j = substr $refbuf, $ofs1;
stripWhite \$j;
last switch unless length $j;
if($j =~ /^th|st|rd|nd$/i and
substr($refbuf, $ofs1-2) =~ /\d/) {
--$ofs1;
substr($refbuf, $ofs1, 1) = "";
last switch;
}
last switch if $j =~ /^(\d+|\*)$/;
if(not $allsub) {
last switch if $j =~ /^[a-zA-Z](?:\d{1,2})?$/;
}
(substr $refbuf, $ofs1) = "($j)";
last switch;
}  # sup/

if($tagplus eq "sub/" and defined $openTag) {
$ofs1 = $$openTag{ofs1};
$j = substr $refbuf, $ofs1;
stripWhite \$j;
last switch unless length $j;
if(not $allsub) {
last switch if $j =~ /^\d{1,2}$/;
}
(substr $refbuf, $ofs1) = "[$j]";
last switch;
}  # sub/

if($tagplus eq "title" and ! $tagLock and ! $intitle) {
$tagStart = length $refbuf;
$tagLock = $intitle = 1;
last switch;
}  # title

if($tagplus eq "title/" and $intitle) {
$i = substr $refbuf, $tagStart;
substr($refbuf, $tagStart) = "";
$lspace = computeSpace();
$longcut = 0;
$colno = 1;
if(! defined $$btags[0]{title}) {
stripWhite \$i;
$$btags[0]{title} = $i if length $i;
}
$tagLock = 0;
$intitle = 0;
last switch;
}  # title/

if($tagplus eq "li") {
$i = index $tagnest, ".ol.";
$j = index $tagnest, ".ul.";
if($i >= 0) {
if($j >= 0 and $j < $i) {
$chunk = "\n* ";
} else {
$j = ++$olcount[$#olcount];
$chunk = "\n$j. ";
}
} elsif($j >= 0) {
$chunk = "\n* ";
} else {
$chunk = "\n";
errorConvert("$desc appears outside of a list context");
}
last switch;
}  # li

if($tagplus eq "dt" or $tagplus eq "dd") {
if(($i = $#dlcount) >= 0) {
$j = ($tag eq "dd" ? 1 : 0);
errorConvert("improper term->definition sequence") if $j != $dlcount[$i];
$dlcount[$i] ^= 1;
} else {
errorConvert("$desc is not contained in a definition list");
}
last switch;
}  # dt or dd

#  The only thing good about an image is its alt description.
if($tagplus eq "img") {
$hrefFile = "" unless $inhref;
$j = deriveAlt($h, $hrefFile);
$j = "?" if $inhref and length($j) == 0;
if(length $j) {
$refbuf .= $j;
$inhref = 2 if $inhref;
$lspace = 0;
}
last switch;
}  # image

if($tagplus eq "body") {
my $onl = $$h{onload};  # popup
$onl = $$h{onunload} unless $onl;  # popunder
next unless $onl;
if($onl =~ /submit[.\w]* *\(/i) {
$onloadSubmit = 1;
last switch;
}
$j = javaWindow $onl;
if(length $j and $j ne "submit") {
createHyperLink($h, $j, "onload");
$chunk = "";
last switch;
}  #  open another window
}  # body

if($tagplus eq "bgsound") {
my $j = $$h{src};
if(defined $j and length $j) {
#  Someday we'll let you play this right from edbrowse, spawning playmidi
#  or mpg123 or whatever.  For now I'll let you grab the file yourself.
#  Maybe that's better anyways.
createHyperLink($h, $j, "Background music");
$chunk = "\n";
}
last switch;
}  # background music

if($tag eq "base") {
$href = $$h{href};
$baseref = urlDecode $href if $href;
next reformat;
}  # base tag

if($tagplus eq "a") {
if(defined $$h{name}) {
$refbuf .= "\x80$attrhidden*";
}  # name=
if(defined($hrefFile = $$h{href})) {
$$h{form} = $inform;
$inhref = 1;
$hrefTag = $h;
$$h{bref} = $baseref;
#  We preserve $lspace, despite pushing visible characters.
$refbuf .= "\x80$attrhidden".'{';
++$colno;
}  # href=
last switch;
}  # a

if($tagplus eq "area") {
my ($alt, $href);
if(defined($href = $$h{href})) {
$j = javaWindow($href);
$href = $j if length $j and $j ne "submit";
$alt = deriveAlt($h, "");
$alt = $foundFunc if length $foundFunc and not defined $$h{alt};
$alt = "area" unless length $alt;
createHyperLink($h, $href, $alt);
}  # hyperlink
} # area

if($tagplus eq "frame") {
my $name = $$h{name};
my $src = $$h{src};
if(defined $src) {
$name = "" if ! defined $name;
stripWhite \$name;
stripWhite \$src;
if(length $src) {
$$h{ofs1} = backOverSpaces(1);
$name = "???" if ! length $name;
$name =~ y/{}\n/[] / if $inhref;
$refbuf .= "frame ";
$colno += 6;
createHyperLink($h, $src, $name);
}}  # frame becomes hyperlink
last switch;
}  # frame

$premode = 1, last switch if $tagplus eq "pre";
$premode = 0, last switch if $tagplus eq "pre/";
$inscript = 1, last switch if $tagplus eq "script";
$inscript = 0, last switch if $tagplus eq "script/";
$intable++, last switch if $tagplus eq "table";
$intable--, last switch if $tagplus eq "table/" and $intable;
if($tag eq "br") {
$chunk = "\n";
$chunk = "\n\n" if $lspace >= 2;
last switch;
}
$chunk = "\n--------------------------------------------------------------------------------\n\n", last switch if $tagplus eq "hr";

if($tag eq "tr") {
errorConvert("$desc not inside a table") if ! $intable;
$slash ? do { --$intabrow if $intabrow } : ++$intabrow;
$chunk = "\n";
$intabhead = 0;
}  # tr

if($tag eq "td" or $tag eq "th") {
errorConvert("$desc not inside a table row") if ! $intabrow;
$intabhead = 0;
$intabhead = 1 - length $slash if $tag eq "th";
if($slash) {
substr($refbuf, -1) = "" if $lspace == 1;
$refbuf .= "|";
$lspace = 1;
}
last switch;
}  # td or th

if($tagplus eq "form" and ! ($inform + $tagLock)) {
$inform = $h;
$$h{bref} = $baseref;
$j = lc $$h{method};
$j = "get" if ! defined $j or ! length $j;  # default
if($j ne "post" and $j ne "get") {
errorConvert("form method $j not supported");
$j = "get";
}
$$h{method} = $j;
$$h{nnh} = 0;  # number of non hidden fields
$$h{nif} = 0;  # number of input fields
last switch;
}  # form

if($tagplus eq "form/" and $inform) {
#  Handle the case with only one visible input field.
if($onloadSubmit or
($$inform{action} or $$inform{onchange}) and
$$inform{nnh} <= 1 and $$inform{nif} and (
$$inform{nnh} == 0 or
($h = $$inform{lnh}) and
$$h{type} ne "submit")) {
$refbuf .= " " if $lspace == 0;
$itag = {tag => "input",
type => "submit", form => $inform,
size => 2, value => "Go"};
push @$btags, $itag;
$j = hideNumber $#$btags;
$refbuf .= "\x80$j<Go\x80\x8f>";
$lspace = 0;
$onloadSubmit = 0;
}  # submit button created out of thin air
$inform = 0;
last switch;
}  # form/

my $noform = "$desc is not inside a form";
if($tagplus eq "select" and ! $tagLock) {
errorConvert($noform) if ! $inform;
$inselect = $h;
$$inform{onchange} = 1 if $inform and $$h{onchange};
$tagLock = 1;
$tagStart = length $refbuf;
$optCount = $optSel = $optSize = 0;
$lastopt = undef;
$$h{opt} = $opt = {};
last switch;
}  # select

if(($tagplus eq "select/" or $tagplus eq "option") and $inselect) {
if(defined $lastopt) {
$j = substr $refbuf, $optStart;
stripWhite \$j;
if(length $j) {
$lastopt =~ s/NoOptValue$/$j/;
$$opt{$j} = $lastopt;
if($optCount < 999) {
++$optCount;
} else {
errorConvert("too many options, limit 999");
}
++$optSel if substr($lastopt, 3, 1) eq '+';
$j = length $j;
$optSize = $j if $j > $optSize;
}}

if($tagplus eq "select/") {
$inselect = 0;
$tagLock = 0;
substr($refbuf, $tagStart) = "";
$lspace = computeSpace();
$colno = 1;
$optCount or errorConvert("no options in the select statement");
my $mult = 0;  # multiple select
$mult = 1 if defined $$openTag{multiple};
my $mse = 0;  # multiple select error
$optSel <= 1 or $mult or
$mse = 1, errorConvert("multiple options preselected");
$$inform{nnh}++;  # another non hidden field
$$inform{nif}++;
$$inform{lnh} = $openTag;  # last non hidden field
#  Display selected item(s)
$refbuf .= "\x80$openattrhidden<";
my $buflen = length $refbuf;
$i = 0;
foreach (%{$opt}) {
$i ^= 1;
$j = $_, next if $i;
if($mult and $j =~ /,/) {
errorConvert("sorry, option string cannot contain a comma");
$$opt{$j} = "";  # can't delete from hash
next;
}
substr($_, 3, 1) = '-' if $mse;
next unless substr($_, 3, 1) eq '+';
$refbuf .= ',' unless substr($refbuf, -1) eq '<';
$refbuf .= $j;
}
#  This is really an input tag.
$$openTag{tag} = "input";
$$openTag{type} = "select";
$$openTag{size} =  ($mult ? 0 : $optSize);
$$openTag{value} = substr $refbuf, $buflen;
$$openTag{form} = $inform if $inform;
$refbuf .= "\x80\x8f>";
$lspace = 0;
$$openTag{ofs2} = length $refbuf;
last switch;
}}  # select/

if($tagplus eq "option") {
if(! $inselect) {
errorConvert("$desc is not inside a select statement")
} else {
$lastopt = $$h{value};
$lastopt = "NoOptValue" unless defined $lastopt;
$lastopt = (defined $$h{selected} ? "+" : "-") . $lastopt;
$lastopt = sprintf("%03d", $optCount) . $lastopt;
$optStart = length $refbuf;
$$h{tag} = "z";
}  # in select or not
last switch;
}  # option

if($tagplus eq "textarea" and ! $tagLock) {
errorConvert($noform) if ! $inform;
$inta = $h;
$tagLock = 1;
$tagStart = length $refbuf;
last switch;
}  # textarea

if($tagplus eq "textarea/" and $inta) {
#  Gather up the original, unformatted text.
$i = "";
foreach $j ($$inta{lineno}..$lineno) {
  $i .= fetchLine($j, 0);
$i .= "\n";
}
#  Strip off textarea tags.
#  I'm not using the s suffix, textarea tags should not cover multiple lines.
$i =~ s/^.*<textarea[^<>]*>\n?//i;
$i =~ s/<\/textarea.*\n$//i;
$i .= "\n" if length $i and substr($i, -1) ne "\n";
$inta = 0;
$tagLock = 0;
substr($refbuf, $tagStart) = "";
my $cx = cxCreate(\$i, "");
$colno = 1;
$$openTag{cx} = $cx;
++$cx;
$$inform{nnh}++;  # another non hidden field
$$inform{nif}++;
$$inform{lnh} = $openTag;  # last non hidden field
$refbuf .= "\x80$openattrhidden<buffer $cx\x80\x8f>";
$lspace = 0;
#  This is really an input tag.
$$openTag{tag} = "input";
$$openTag{ofs1} = $tagStart;
$$openTag{ofs2} = length($refbuf);
$$openTag{type} = "area";
$$openTag{form} = $inform if $inform;
$j = $$openTag{rows};
$j = 0 if ! defined $j;
$j = 0 unless $j =~ /^\d+$/;
$$openTag{rows} = $j;
$j = $$openTag{cols};
$j = 0 if ! defined $j;
$j = 0 unless $j =~ /^\d+$/;
$$openTag{cols} = $j;
last switch;
}  # textarea/

if($tagplus eq "input") {
errorConvert($noform) if ! $inform;
$i = lc $$h{type};
$i = "text" unless defined $i and length $i;
$i = "text" if $i eq "password";
#  I should verify that the input is a number,
#  but I'm too busy right now to implement that.
$i = "text" if $i eq "number";
#  Be on the lookout for new, advanced types.
index(".text.checkbox.radio.submit.image.button.reset.hidden.", ".$i.") >= 0 or
errorConvert("unknown input type $i");
$j = $$h{value};
$j = "" unless defined $j;
$$h{saveval} = $j;
if($i eq "radio" or $i eq "checkbox") {
$j = (defined $$h{checked} ? '+' : '-');
}
if($i eq "image") {
$i = "submit";
$$h{image} = 1;
length $j or $j = deriveAlt($h, "submit");
}  # submit button is represented by an icon
if($i ne "hidden") {
#  I don't think there should be newlines in the value field.
$j =~ y/\n/ /;
}
if($i eq "button") {
#  Hopefully we can turn this into a hyperlink.
#  If not, it's no use to us.
my $onc = $$h{onclick};
my $page = javaWindow $onc;
$j = "button" unless length $j;  # alt=button
if(not length $page and
$onc =~ /self\.location *= *['"]([\w._\/:,=@&?+-]+)["'] *(\+?)/i) {
$page = "$1$2";
}
$i = $page, $page = "" if $page eq "submit";
if(length $page) {
createHyperLink($h, $page, $j);
last switch;
}
}  # button
$$h{type} = $i;
$$h{value} = $j;
$$inform{nif}++;
if($i ne "hidden") {
$$inform{nnh}++;  # another non hidden field
$$inform{lnh} = $h;  # last non hidden field
$refbuf .= "\x80$attrhidden<$j";
if($i eq "submit") {
$refbuf .= " js" if $$inform{onsubmit} or $$h{onclick};
if($$inform{action}) {
my $actscheme = is_url $$inform{action};
$actscheme = is_url $baseref unless $actscheme;
$refbuf .= " secure" if $actscheme eq "https";
$refbuf .= " mailform" if $$inform{action} =~ /^mailto:/i;
}
}
$refbuf .= "\x80\x8f>";
$lspace = 0;
$j = $$h{maxlength};
$j = 0 unless defined $j and $j =~ /^\d+$/;
$j = 1 if $i eq "checkbox" or $i eq "radio";
$$h{size} = $j;
}
if($inform and ! $tagLock) {
$$h{form} = $inform;
$$h{ofs2} = length $refbuf;
}
last switch;
}  # input

}  # switch on $tag

$lineno += $nlcount;
next reformat unless length $chunk;

#  Apparently the tag has forced a line break or paragraph break.
#  I've decided to honor this, even in preformat mode,
#  because that's what lynx does.
$colno = 1;
$longcut = $lperiod = $lcomma = $lright = $lany = 0;
backOverSpaces(1);

#  Get rid of a previous line of pipes.
#  This is usually a table or table row of images -- of no use to me.
if($intable and $lspace < 2) {
$j = length($refbuf) - 1;
while($j >= 0 and substr($refbuf, $j, 1) =~ /[|\s]/) {
last if $j > 0 and substr($refbuf, $j-1, 2) eq "\n\n";
--$j;
}
++$j;
if($j < length $refbuf) {
substr($refbuf, $j) = "";
$lspace = computeSpace();
$colno = 1;
}
}  # end of line tag inside a table

if($chunk eq "\n\n") {
next reformat if $lspace == 3;
$chunk = "\n" if $lspace == 2;
$lspace = 3;
$refbuf .= $chunk;
next reformat;
}  # tag paragraph

#  It's a line break.
substr($chunk, 0, 1) = "" if $lspace > 1;
$lspace = 2 if $lspace < 2;
next reformat unless length $chunk;
$refbuf .= $chunk;
$chunk =~ s/^\n//;
next reformat unless length $chunk;
#  It's either a list item indicator or a horizontal line
$inhref = 2 if $inhref;
if($chunk =~ /^--/) {
#  Again I'm following the lynx convention.
#  hr implies a line break before, and aparagraph break after.
$lspace = 3;
} else {
$colno += length $chunk;
$lspace = 1;
}
}  # loop over tokens in the buffer

$$tbuf = undef;
print "tags rendered\n" if $debug >= 6;

if(length($tagnest) > 1 and $tagnest ne ".body.0.") {
my $opendesc = substr $tagnest, 1;
$opendesc =~ s/\..*//;
$opendesc = $tagdesc{$opendesc};
substr($opendesc, 0, 2) = "";
--$lineno;
errorConvert("$opendesc is not closed at EOF");
}

$errorMsg = $intMsg, return 0 if $intFlag;

$refbuf =~ s/\s+$//;  # don't need trailing blank lines

#  In order to fit all the links on one screen, many web sites place
#  several links on a line.  Sometimes they are separated
#  by whitespace, sometimes commas, sometimes hyphens.
#  Sometimes they are arranged in a table, and thanks to the
#  table rendering software in this program, they will be pipe separated.
#  In any case, there is no advantage in having multiple
#  links on a line, and it's downright inconvenient when you want to use
#  the g or A command.  We introduce line breaks between links.
#  We use alphanum [punctuation] right brace to locate the end of a link.
#  We use { optional '" alphanum for the start of a link.
#  These aren't guaranteed to be right, but they probably are most of the time.
#  Let's start with link space link or link separater link
$refbuf =~ s/} ?[-,|]? ?(\x80[\x85-\x8f]+{['"]?\w)/}\n$1/g;
#  Separating punctuation at the end of the line.
$refbuf =~ s/^({[^{}]+} ?),$/$1/mg;
#  Delimiter at the start of line, before the first link.
$refbuf =~ s/\n[-,|] ?(\x80[\x85-\x8f]+{['"]?\w)/\n$1/g;
#  word delimiting punctuation space link.
$refbuf =~ s/([a-zA-Z]{2,}[-:|]) (\x80[\x85-\x8f]+{['"]?\w)/$1\n$2/g;
#  Link terminating punctuation words
$refbuf =~ s/(\w['"!]?}) ?[-|:] ?(\w\w)/$1\n$2/g;
print "links rearranged\n" if $debug >= 6;

if(! $badHtml) {
#  Verify internal links.
intlink:
foreach $h (@$btags) {
$tag = $$h{tag};
next unless $tag eq "a";
$j = $$h{href};
next if ! defined $j;
next unless $j =~ s/^#//;
$refbuf .= "";  # reset match position.  Isn't there a better way??
while($refbuf =~ /\x80([\x85-\x8f]+)\*/g) {
$i = revealNumber $1;
next intlink if $$btags[$i]{name} eq $j;
}
$lineno = $$h{lineno};
errorConvert("internal link #$j not found");
last;
}  # loop
print "internal links verified\n" if $debug >= 6;
}

#  Find the uncalled javascript functions.
my $fw = $$btags[0]{fw};  # pointer to function window hash
my $orphans = 0;
foreach $i (keys %$fw) {
$j = $$fw{$i};
next unless $j =~ s/^\*//;
$orphans = 1, $refbuf .= "\n" unless $orphans;
print "orphan java function $i\n" if $debug >= 3;
$itag = {tag => "a", href => $j, bref => $baseref};
push @$btags, $itag;
my $hn = hideNumber $#$btags;
$refbuf .= "\n jf: $i\x80$hn" . "{$j}";
#  I don't think we need to mess with ofs1 and ofs2?
$$itag{ofs2} = length $refbuf;
}

$$tbuf = $refbuf;  # replace
return 1;
}  # renderHtml

#  Report the first html syntax error.
#  $lineno tracks the line number, where text is being processed.
sub errorConvert($)
{
$badHtml and return;
my $msg = shift;
#  Look at the following print statement, and you'll see the little things
#  I try to anticipate when I write software for the blind.
#  The first physical line of output is for the sighted user, or the
#  new blind user -- but the experienced blind user doesn't need to read it.
#  He can read the last line of output, one keystroke in my adaptive software,
#  and hear exactly what he want to know.
print "The html text contains syntax errors.  The first one is at line\n$lineno: $msg.\n";
#  Put the bad line number in label e.
substr($labels, 4*$lnwidth, $lnwidth) =
sprintf $lnformat, $lineno;
$badHtml = 1;
}  # errorConvert

#  Strip redundent stuff off the start and end of a web page,
#  relative to its parent.
sub stripChild()
{
$fmode&$browsemode or $errorMsg = $nobrowse, return 0;
defined $backup or $errorMsg = "no previous web page", return 0;
my $p_fmode = $$backup{fmode};
$p_fmode&$browsemode or $errorMsg = "no previous web page", return 0;
#  Parent and child file names should come from the same server.
my $p_fname = $$backup{fname};
my $c_fname = $fname;
is_url($p_fname) and is_url($c_fname) or $errorMsg = "web pages do not come from web servers", return 0;
$p_fname =~ s,^https?://,,i;
$c_fname =~ s,^https?://,,i;
$p_fname =~ s,/.*,,;
$c_fname =~ s,/.*,,;
$p_fname =~ s/\.browse$//;
$c_fname =~ s/\.browse$//;
$p_fname eq $c_fname or $errorMsg = "parent web page comes from a different server", return 0;
$$btags[0]{dol2} == $dol or $errorMsg = "web page already stripped or modified", return 0;
my $p_dol = $$backup{btags}[0]{dol2};
my $c_dol = $dol;
if($p_dol > 10 and $c_dol > 10) {
my $pb = $$backup{btags}[0]{pb};
if(defined $pb) {
evaluateSequence($pb, 0);
if($$btags[0]{dol2} > $dol) {
$fmode &= ~($changemode|$firstopmode);
apparentSize();
$$btags[0]{pb} = $pb;
return 1;
}  # successful post browse from the parent page
}  # attempting post browse from the parent page
my $p_map = $$backup{btags}[0]{map2};
my $c_map = $map;
my $start = 1;
my $oneout = 0;
while($start <= $p_dol and $start <= $c_dol) {
if(!sameChildLine(\$p_map, $start, \$c_map, $start)) {
last if $oneout;
$oneout = $start;
}
++$start;
}
$start = $oneout if $oneout and $start < $oneout + 5;
my $delcount = --$start;
my $p_end = $p_dol;
my $c_end = $c_dol;
while($p_end > $start and $c_end > $start) {
last unless sameChildLine(\$p_map, $p_end, \$c_map, $c_end);
++$delcount;
--$p_end, --$c_end;
}
if($delcount == $dol) {
my $ln = substr($map, $lnwidth, $lnwidth1);
$text[$ln] = "This web page contains no new information - you've seen it all before.";
print "71\n";
$dol = $dot = 1;
$labels = $lnspace x 26;
$fmode &= ~$firstopmode;
return 1;
}
if($delcount > 5) {
++$c_end;
delText($c_end, $dol)  if $c_end <= $dol;
delText(1, $start) if $start;
$labels = $lnspace x 26;
$fmode &= ~($changemode|$firstopmode);
apparentSize();
return 1;
}
}
$errorMsg = "nothing to strip";
return 0;
}  # stripChild

sub sameChildLine($$$$)
{
my ($m1, $l1, $m2, $l2) = @_;
my $t1 = $text[substr($$m1, $l1*$lnwidth, $lnwidth1)];
my $t2 = $text[substr($$m2, $l2*$lnwidth, $lnwidth1)];
removeHiddenNumbers \$t1;
removeHiddenNumbers \$t2;
$t1 =~ y/a-zA-Z0-9//cd;
$t2 =~ y/a-zA-Z0-9//cd;
return ($t1 eq $t2);
}  # sameChildLine

sub unstripChild()
{
$fmode&$browsemode or
$errorMsg = $nobrowse, return 0;
my $dol2 = $$btags[0]{dol2};
$dol2 > $dol or $errorMsg = "nothing stripped from this web page", return 0;
#  Backing out.
$map = $$btags[0]{map2};
$fmode &= ~$firstopmode;
$labels = $lnspace x 26;
$dot = 1;
$dol = $dol2;
apparentSize();
return 1;
}  # unstripChild

#  Returns the index of the input field to be modified.
#  Sets $inf to the text of that field.
#  Sets $itag, $isize, and the other globals that establish an input field.
#  Returns 0 for no input fields on the line, -1 for some other error.
sub infIndex($$)
{
my ($ln, $line) = @_;
my ($i, $j, $idx);
my @fields = ();
my @fieldtext = ();
#  Here's some machinery to remember the index if there's only one
#  input field of the desired type.
my $holdInput = 0;
my $t = fetchLine $ln, 0;
#  Bug in perl mandates the use of the no-op (?=) below.
#  You'll see this other places in the code too.
#  This bug was fixed in September 2001, patch 12120.
while($t =~ /\x80([\x85-\x8f]+)<(.*?)(?=)\x80\x8f>/g) {
$j = revealNumber $1;
$i = $2;
push @fields, $j;
push @fieldtext, $i;
$itag = $$btags[$j];
$itype = $$itag{type};
next if $itype eq "area";
if($line =~ /^\d*\*/) {
if($itype eq "submit" or $itype eq "reset") {
$holdInput = -1 if $holdInput > 0;
$holdInput = $#fields+1 if $holdInput == 0;
}
} else {
if($itype ne "submit" and $itype ne "reset") {
$holdInput = -1 if $holdInput > 0;
$holdInput = $#fields+1 if $holdInput == 0;
}
}
}
$j = $#fields + 1;
if(!$j) {
$errorMsg = "no input fields present" if ! $inglob;
return 0;
}
$idx = -1;
$idx = $1 if $line =~ /^(\d+)/;
$idx = $holdInput if $holdInput > 0 and $idx < 0;
$idx >= 0 or $j == 1 or
$errorMsg = "multiple input fields, please use $icmd [1,$j]", return -1;
$idx = 1 if $idx < 0;
if($idx == 0 or $idx > $j) {
$errorMsg = $j > 1 ?
"invalid field, please use $icmd [1,$j]" :
"line only has one input field";
return -1;
}
$j = $fields[$idx-1];
$inf = $fieldtext[$idx-1];
$ifield = $idx;
$itagnum = $j;
$itag = $$btags[$j];
$iline = $ln;
$itype = $$itag{type};
$isize = $$itag{size};
$irows = $$itag{rows};
$icols = $$itag{cols};
$iwrap = $$itag{wrap};
$iwrap = "" if ! defined $iwrap;
$iwrap = lc $iwrap;
$iopt = $$itag{opt};
return $idx;
}  # infIndex

#  Get status on an input field, including its options.
sub infStatus($)
{
my $line = shift;
$line =~ s/^\d*\?//;
$line = lc $line;
print $itype;
print "[$isize]" if $isize;
if($itype eq "area" and $irows and $icols) {
print "[${irows}x$icols";
print " recommended" if $iwrap eq "virtual";
print "]";
}
print " many" if defined $$itag{multiple};
print " <$inf>";
my $name = $$itag{name};
print " [$name]" if defined $name and length $name;
print "\n";
return unless $itype eq "select";

#  Display the options in a pick list.
#  If a string is given, display only those options containing the string.
my $i = 0;
my @pieces = ();
my $j;
foreach my $v (%{$iopt}) {
$i ^= 1;
$j = $v, next if $i;
$_ = $v;
next unless s/^(...)[-+]//;
next if length $line and index(lc $j, $line) < 0;
push @pieces,  "$1$j\n";
}
if($#pieces < 0) {
print(length($line) ? "No options contain the string \"$line\"\n" :
"No options found\n");
return;
}
foreach (sort @pieces) {
print((substr($_, 0, 3) + 1) . ": " . substr($_, 3));
last if $intFlag;
}
}  # infStatus

#  Replace an input field with new text.
sub infReplace($)
{
my $newtext = shift;
my ($i, $j, $k, $t);

#  Sanity checks on the input.
$itype ne "submit" and $itype ne "reset" or
$errorMsg = "field is a $itype button, use * to push the button", return 0;
$itype ne "area" or
$errorMsg = "field is a text area, you must edit it from another session", return 0;
not defined $$itag{readonly} or
$errorMsg = "readonly field", return 0;
$newtext =~ /\n/ and
$errorMsg = "input field cannot contain a newline character", return 0;
return 0 if lineLimit 2;

if($ifield) {
my $newlen = length $newtext;
! $isize or $newlen <= $isize or
$errorMsg = "input field too long, limit $isize", return 0;

if($itype eq "checkbox" or $itype eq "radio") {
$newtext eq "+" or $newtext eq "-" or
$errorMsg = "field requires + (active) or - (inactive)", return 0;
$itype eq "checkbox" or $newtext eq '+' or $inf eq '-' or
$errorMsg = "at least one radio button must be set", return 0;
}
}  # not from reset button

if($itype eq "select") {
my @opts = $newtext;
@opts = split(',', $newtext) if defined $$itag{multiple};
$newtext = "";
option:
foreach my $newopt (@opts) {
$newtext .= "," if length $newtext;
$j = $$iopt{$newopt};
#  If you type in the option exactly, that's grand.
$newtext .= $newopt, next if defined $j and length $j;
#  Maybe it's a menu number.
if($newopt =~ /^\d+$/) {
$j = sprintf("%03d", $newopt-1);
#  reverse hash lookup.
my $revcnt = 0;
my $revkey;
foreach (%{$iopt}) {
$revcnt ^= 1;
$revkey = $_, next if $revcnt;
next unless substr($_, 0, 3) eq $j;
$newtext .= $revkey;
next option;
}
} else {  # menu number conversion
#  See if this text is a piece of one and only one option.
#  Or if it is exactly one and only one option.
$j = lc $newopt;
my $matchCount = 0;
my $matchLevel = 0;
my $bestopt = "";
foreach $k (keys %{$iopt}) {
my $klow = lc $k;  # k lower case
next unless index($klow, $j) >= 0;
if($j eq $klow) {
$matchCount = 0, $matchLevel = 2 if $matchLevel < 2;
++$matchCount;
$bestopt = $k;
} else {
next if $matchLevel == 2;
$matchCount = 0, $matchLevel = 1 unless $matchLevel;
++$matchCount;
$bestopt = $k;
}
}
$newtext .= $bestopt, next option if $matchCount == 1;
$errorMsg = "$j matches more than one entry in the list", return 0 if $matchCount > 1;
}
$errorMsg = "$newopt is not an option, type i$ifield? for the list";
return 0;
}  # loop over options in the new list
}  # select

#  Definitely making a change.
$fmode |= $firstopmode;
$ubackup = 1;
$dot = $iline;

return 1 if $newtext eq $inf;  # no change

#  Find and replace the text.
$t = fetchLine $iline, 0;
my $itaghidden = hideNumber $itagnum;
$t =~ s/\x80$itaghidden<.*?(?=)\x80\x8f>/\x80$itaghidden<$newtext\x80\x8f>/;
push @text, $t;
substr($map, $iline*$lnwidth, $lnwidth) =
sprintf $lnformat, $#text;

if($itype eq "radio") {  # find and undo the other radio button
my $radioname = $$itag{name};
if(defined $radioname and length $radioname) {
my $form = $$itag{form};
lineloop:
foreach $k (1..$dol) {
$t = fetchLine $k, 0;
while($t =~ /\x80([\x85-\x8f]+)<\+\x80\x8f>/g) {
$jh = $1;
$j = revealNumber $1;
next if $j == $itagnum;  # already changed this one
my $h = $$btags[$j];
next unless $$h{form} eq $form;
#  Input field is part of our form.
next unless $$h{type} eq "radio";
my $name = $$h{name};
next unless defined $name and $name eq $radioname;
#  It's another radio button in our set.
$t =~ s/\x80$jh<\+\x80\x8f>/\x80$jh<-\x80\x8f>/;
push @text, $t;
substr($map, $k*$lnwidth, $lnwidth) =
sprintf $lnformat, $#text;
last lineloop;
}  # loop over input fields on this line
}  # loop over lines
}  # radio button has a name
}  # radio

return 1;
}  # infReplace

#  Push the submit or reset button.
sub infPush()
{
my $button = $itag;
my $buttontype = $itype;
$buttontype eq "submit" or $buttontype eq "reset" or
$errorMsg = "this is not a submit or reset button", return 0;
$cmd = 'b';  # this has become a browse command
my $formh = $$itag{form};
defined $formh or
$errorMsg = "field is not part of a form", return 0;
my $buttonvalue = $inf;
$buttonvalue =~ s/ secure$//;
$buttonvalue =~ s/ mailform$//;
$buttonvalue =~ s/ js$//;
my $domail = 0;  # sendmail link

my $bref = $$formh{bref};
my $action = $$formh{action};
if(! defined $action or ! length $action) {
#  If no form program is specified, the default is the current url.
$action = $bref;
$action =~ s/\?.*//;
}
$domail = 1 if $action =~ s/^mailto://i;
#  We should check for $form{encoding}.

my ($name, $val, $i, $j, $cx, $h, @pieces);
my $post = "";
my $origdot = $dot;

#  Loop over all tags, keeping those in the input form.
$itagnum = -1;
foreach $h (@$btags) {
++$itagnum;
next unless $$h{tag} eq "input";
#  Overwrite the global input variables, so infReplace will work properly.
#  $itagnum is already set.
$itag = $h;
$itype = $$h{type};
$j = $$h{form};
next unless defined $j and $j eq $formh;
#  Input field is part of our form.
$iopt = $$h{opt};
$isize = $$h{size};

if($itag eq $button and $itype eq "submit") {
$name = $$button{name};
if(defined $name and length $name) {
if($domail) {
$post .= "\n" if length $post;
} else {
$post .= '&' if length $post;
$name = urlEncode $name;
}
if($$button{image}) {
$post .= $domail ?
"$name.x=\n0\n\n$name.y=\n0\n" :
"$name.x=0&$name.y=0";
} else {
if(defined $buttonvalue and length $buttonvalue) {
if($domail) {
$post .= "$name=\n$buttonvalue\n";
} else {
$buttonvalue = urlEncode $buttonvalue;
$post .= "$name=$buttonvalue";
}
} else {
$post .= $domail ?
"$name=\nSubmit\n" :
"$name=Submit";
}
}
}
}  # submit button

next if $itype eq "reset" or $itype eq "submit";

if($itype eq "hidden") {
$inf = $$h{value};
$iline = $ifield = 0;
} else {
#  Establish the line number, field number, and field value.
#  This is crude and inefficient, but it doesn't happen very often.
findField:
for($iline=1; $iline<=$dol; ++$iline) {
$j = fetchLine $iline, 0;
$ifield = 0;
while($j =~ /\x80([\x85-\x8f]+)<(.*?)(?=)\x80\x8f>/g) {
$i = revealNumber $1;
$inf = $2;
++$ifield;
last findField if $i == $itagnum;
}
}
$iline <= $dol or $errorMsg = "input field $itagnum is lost", return 0;
}

if($buttontype eq "submit") {
if($itype eq "area") {
$cx = $$h{cx};
$val = "";
if(defined $factive[$cx] and $dol[$cx]) {
#  Send all the lines of text in the secondary buffer.
for(my $ln=1; $ln<=$dol[$cx]; ++$ln) {
$val .= fetchLineContext($ln, 1, $cx);
next if $ln == $dol[$cx];
$val .= ($textAreaCR ? $eol : "\n");
}
}
} else {  # text area or field
$val = $inf;
if($itype eq "radio" or $itype eq "checkbox") {
next if $val eq '-';
$val = $$h{saveval};
#  I thought it had to say "on"; now I'm not sure.
$val = "on" if $itype eq "checkbox" and  ! length $val;
}  # radio
}  # text area or input field
#  Turn option descriptions into option codes for transmission
if($itype eq "select") {
@pieces = $val;
@pieces = split ',', $val if defined $$h{multiple};
$val = "";
foreach (@pieces) {
$val .= "," if length $val;
my $code = $$iopt{$_};
if(defined $code) {
$code = substr($code, 4);
} else {
$code = $_;
}
$val .= $code;
}  # loop over options
}  # select

$name = $$h{name};
defined $name or $name = "";
if(! $domail) {
#  Encode punctuation marks for http transmition
$name = urlEncode($name);
$name .= '=';
$val = urlEncode($val);
}
if($itype eq "select" and defined $$h{multiple}) {
#  This is kludgy as hell.
#  comma has been turned into %2C
@pieces = split '%2C', $val;
foreach $val (@pieces) {
$post .= ($domail ? "\n" : '&') if length $post;
$post .= $name;
$post .= "\n" if $domail and length $name;
$post .= $val;
$post .= "\n" if $domail and length $val;
}
} else {
$post .= ($domail ? "\n" : '&') if length $post;
$post .= $name;
$post .= "\n" if $domail and length $name;
$post .= $val;
$post .= "\n" if $domail and length $val;
}

} else {  # submit or reset

next if $itype eq "hidden";
if($itype eq "area") {
$cx = $$h{cx};
cxReset($cx, 2);
$factive[$cx] = 1;
} else {
$ifield = 0;  # zero skips some of the field checks in infReplace
$val = $$h{value};
infReplace($val);
}  # field or text area

}  # submit or reset
}  # loop over tags

$dot = $origdot, return 1 if $buttontype eq "reset";
print "submit: $post\n" if $debug >= 2;

length $action or
$errorMsg = "form does not specify a program to run", return 0;

if($domail) {
my $subj = urlSubject(\$action);
$subj = "html form" unless length $subj;
$post = "Subject: $subj\n\n$post";
print "$action\n";
my @tolist = ($action);
my @atlist = ();
$mailToSend = "form";
$altattach = 0;
$whichMail = $localMail;
sendMail(\@tolist, \$post, \@atlist) or return 0;
print "Form has been mailed, watch for a reply.\n";
return 1;
}  # sendmail

$line = resolveUrl($bref, $action);
print "* $line\n";
$post = ($$formh{method} eq "get" ? '?' : '*') . $post;
return -1, $line, $post;
}  # infPush

sub renderMail($)
{
my $tbuf = shift;
$badenc = $bad64 = 0;
$fhLevel = 0;
$nat = 0;  # number of attachments
@mimeParts = ();

#  Copy lines into @msg.
#  The original cleanMail routine was built upon @msg,
#  And when I folded it into edbrowse, I was too lazy to change it.
@msg = ();
push @msg, fetchLine($_, 0) foreach (1..$dol);

findHeaders(0, $#msg);
deGreater();
nullForwarding();
--$#msg while $#msg >= 0 and
$msg[$#msg] !~ /[a-zA-Z0-9]/;

#  Last chance to interrupt a browse operation
$errorMsg = $intMsg, return 0 if $intFlag;

$$tbuf = "";
$$tbuf .= "$_\n" foreach (@msg);
chomp $$tbuf if length $$tbuf;
$$tbuf =~ y/\x92\x93\x94\xa0\xad/'`' -/;
return 1;
}  # renderMail

#  Insert this text line between mail headers.
$mailBreak = "{NextMailHeader}";
#  Insert this text line between mime headers.
$mimeBreak = "{NextMimeSection}, Level";
#  Max lines in a "to unsubscribe" trailer?
$unsHorizon = 7;

#  Hash the annoying commercials.
%annoy = ();
if(length $annoyFile) {
open FH, $annoyFile
or dieq "Cannot open file of annoying commercials $annoyFile.";
while(<FH>) {
stripWhite \$_;
$annoy{lc $_} = "" if length $_;
}
close FH;
}  # annoy

#  Today timestamp, so old "junk" subjects can expire.
$junkToday = int time / (60*60*24);
$junkHorizon = 14;
$oldSubjects = 0;
%junkSubjects = ();

#  Now load the junk subjects, which we aren't interested in reading.
if(length $junkFile) {
open FH, $junkFile
or dieq "Cannot open file of junk subjects $junkFile.";
while(<FH>) {
s/\n$//;  # don't need nl
($jtime = $_) =~ s/:.*//;
($jsubject = $_) =~ s/^\d+:\s*(.*)\s*$/$1/;
if($jsubject =~ /^`/) {
$junkSubjects{$jsubject} = $junkToday;
} else {
$oldSubjects = 1, next if $jtime < $junkToday - $junkHorizon;
$junkSubjects{$jsubject} = $jtime;
}
}
close FH;
}  # junkFile

#  Add a subject to the junk list.
#  This updates the junk file.
sub markSubject ($)
{
my $s = shift;
die "No subject to junk." if $s eq "";
$junkSubjects{$s} = $junkToday;
if($oldSubjects) {
open FH, ">$junkFile"
or dieq "Cannot rewrite file of junk subjects $junkFile.";
$iskey = 0;
foreach (%junkSubjects) {
($iskey ^= 1) ?
($savekey = $_) :
print FH "$_:$savekey\n";
}
$oldSubjects = 0;
} else {
open FH, ">>$junkFile"
or dieq "Cannot add to file of junk subjects $junkFile.";
print FH "$junkToday:$s\n";
}
close FH;
}  # markSubject

#  Build an array for base64 decoding.
{
my ($j, $c);
$c = 'A', $j = 0;
$b64_map[ord $c] = $j, ++$c, ++$j until $j == 26;
$c = 'a';
$b64_map[ord $c] = $j, ++$c, ++$j until $j == 52;
$c = '0';
$b64_map[ord $c] = $j, ++$c, ++$j until $j == 62;
$b64_map[ord '+'] = $j++;
$b64_map[ord '/'] = $j++;
}

#  The following routine decodes the Quoted-Printable mime standard.
#  If one line ends in an equals sign, it must be joined to the next.
#  All other =xx sequences become the 8-bit value defined by hex xx.
#  Pass the start and end offsets -- what do you want to dequote?
#  Also pass the boundary, if any, and stop there.
sub qp_lowlevel($$$)
{
my ($start, $end, $boundary) = @_;
print "qp $start-$end<$boundary\n" if $debug >= 6;
return if $end < $start;

my $tbuf = "";
foreach my $i ($start..$end) {
if(length $boundary) {
my $line = $msg[$i];
$line =~ s/^[ \t>]*-*//;
$line =~ s/-*$//;
$end = $i-1, last if $line eq $boundary;
}
$msg[$i] =~ s/[ \t]+$//;
$tbuf .= $msg[$i]."\n";
}
chomp $tbuf;
print "qp ends at $end, length " . length($tbuf) . "\n" if $debug >= 6;

#  Now undo quoted-printable encoding.
#  Use global substitutions on the concatenated texts, it's faster.
my $join = 
$tbuf =~ s/=\n//g;
$join = 0 unless $join;
print "qp joins $join lines\n" if $debug >= 6;
$tbuf =~ s/=([0-9a-fA-F]{2})/chr hex "$1"/ge;
#  Split the text back into lines, and back into @msg.
$end -= $join;
if(length $tbuf) {
@msg[$start.. $end] = split("\n", $tbuf, $end-$start+1);
} else {  # split problem
$msg[$start] = "";  # probably was "" already
}
#  Fill the empty spaces with blank lines
$msg[++$end] = '' while $join--;
}  # qp_lowlevel

#  If the message includes any lines with leading > signs,
#  break paragraphs whenever the nesting level changes.
#  A change in the number of > symbols indicates a different speaker,
#  hence a new paragraph.
#  But watch out!
#  Some mail intermediaries cut long lines,
#  leaving a dangling fragment without a > nesting level.
#  This fragment does not represent a new paragraph;
#  it is part of the previous sentence.
#  But watch watch out!
#  Some people deliberately interject short comments, as in:
#
#  > I really think the Tigers are the hotest baseball team ever, really great,
#  Horse feathers!
#  > a team to look up to.
#
#  There is virtually no way to distinguish between the two cases.
#  Most of the time a short fragment in the midst of indented text
#  actually belongs to the previous line,
#  so I treat it as such and hope for the best.
sub deGreater()
{
my (@nestlev, @newmsg, $i, $j, $state, $temp);
my $lastsubject = "";

#  Push some blank lines, to avoid eof conditions.
push @msg, '', '', '';

#  Establish the nest level of each line.
foreach (@msg) {
$temp = $_;
#  Count > signs.
$temp =~ s/^([ \t>]*).*/$1/;
$j = $temp =~ y/>/>/;
push @nestlev, $j;
}

my $lastlev = 0;
my $newlev;
for($i=0; $i<=$#msg; ++$i) {
$newlev = $nestlev[$i];

#  Let's get right at the tricky part, a drop in level.
#  It's a fragment if the next line, or line after,
#  has the same nest level as the previous line.
if($newlev < $lastlev and
(($nextlev = $nestlev[$i+1]) == $lastlev or
$nextlev < $lastlev && $nestlev[$i+2] == $lastlev)) {
$temp = $msg[$i];
$temp =~ s/^[ \t>]*//;
if($j = length $temp) {
if($nextlev == $lastlev) {
$newmsg[$#newmsg] .= " $temp";
next;
}
#  It must be that the line after next has the previous nest level.
my $temp2 = $msg[$i+1];
$temp2 =~ s/^[ \t>]*//;
if($j = length $temp2) {
$newmsg[$#newmsg] .= " $temp $temp2";
++$i;
next;
}  # next line is nonempty
}  # this line is nonempty

$newlev = $lastlev if $j == 0 and $nextlev == $lastlev;
}  # bracketed between larger nest levels

if($msg[$i] =~ /^$mailBreak/o) {
$newlev = $ lastlev = 0;
push @newmsg, $mailBreak;
my ($subject, $from, $date, $reply);
$temp = $msg[$i];
($subject = $temp) =~ s/\n.*//s if $temp =~ s/.*\nSubject: //s;
($from = $temp) =~ s/\n.*//s if $temp =~ s/.*\nFrom: //s;
($date = $temp) =~ s/\n.*//s if $temp =~ s/.*\nDate: //s;
($reply = $temp) =~ s/\n.*//s if $temp =~ s/.*\nReply-to: //s;
if(defined $subject or defined $from or defined $date or defined $reply) {
if($#newmsg == 0) {  # Read the first header differently.
push @newmsg, "Subject: $subject" if defined $subject;
push @newmsg, "From $from" if defined $from;
} else {
$temp = "Message";
if(defined $from) {
$temp .= " from $from,";
if(defined $subject) {
$temp .= (
$subject eq $lastsubject ?
" same subject." :
" with subject, $subject.");
} else {
$temp .= " with no subject.";
}  # subject or not
} else {
if(defined $subject) {
$temp .= (
$subject eq $lastsubject ?
" with the same subject." :
" with subject, $subject.");
} else {
$temp .= " with no subject.";
}  # subject or not
}  # from line or not
push @newmsg, $temp;
}  # top header or internal
push @newmsg, "Mail sent $date" if defined $date;
push @newmsg, "Reply to $reply" if defined $reply;
push @newmsg, "" if $#newmsg;
}
$subject = "" if ! defined $subject;
$lastsubject = $subject;
next;
}  # mail header

if($newlev != $lastlev) {
push @newmsg, "", "Indent $newlev.";
}  # change in level

#  Strip off leading >
$temp = $msg[$i];
$temp =~ s/^[ \t]*>[ \t>]*//;
push @newmsg, $temp;
$lastlev = $newlev;
}  # loop over lines

#  Push a mime separater on, to make the unsubscribe test work.
push @newmsg, "$mimeBreak 1";

#  Now put the lines back into @msg, compressing blank lines.
#  Also, Try to remove any "unsubscribe" trailers.
$#msg = -1;
my $unslast = -1;
my $unscount = 0;
my $unstest;
$state = 1;
$j = 0;

foreach my $line (@newmsg) {
#  Check for "to unsubscribe"
if($line =~ /^ *to unsubscribe/i) {
$unslast = $j if $unslast < 0 or $unscount > $unsHorizon;
$unscount = 0;
}  # unsubscribe line

#  Check for mime/mail separater.
$unstest = 0;
$temp = lc $line;
$temp =~ s/\s+$//;
if($line =~ /^$mimeBreak \d/o or
$line eq $mailBreak or
defined $annoy{$temp} or
$temp =~ /^-+\s*original message\s*-+\s*$/) {
$line = "";  # no need to read that
$unstest = 1;
}

$unstest = 1 if $line =~ /^$mailBreak/o;

if($line =~ /^Indent \d/) {
$unstest = 1;
if($j > 0 and $msg[$j-1] =~ /^Indent \d/) {
--$j, --$#msg;
$unstest = 0;
}  # sequential indents
}  # indent line

if($unstest and $unslast >= 0 and $unscount <= $unsHorizon) {
#  Remove unsubscribe section
$j = $unslast - 1;
$unslast = -1;
--$j while $j >= 0 and
$msg[$j] !~ /[a-zA-Z0-9]/;
$#msg = $j;
++$j;
$state = ($j == 0);
next;
}  # crunching unsubscribe

if(length $line) {
++$unscount if $line =~ /[a-zA-Z0-9]/;
$msg[$j++] = $line;
$state = 0;
$state = 1 if $line =~ /^Indent \d/;
} elsif(! $state) {
$msg[$j++] = $line;
$state = 1;
}
}  # loop over lines

--$j;
--$j if $j >= 0 and $state;
$#msg = $j;
}  # deGreater

#  No need to read vacuous forwardings.
sub nullForwarding()
{
my $lf = -1;  # last forwarding
my $j = 0;
foreach my $line (@msg) {
if($line =~ /^Message/) {
$j = $lf if $lf >= 0 and $j - $lf <= 4;
$lf = $j;
}
$msg[$j++] = $line;
}  # loop over lines
--$j;
$#msg = $j;
}  # nullForwarding

#  Decide whether a line, and eventually a paragraph, is an email header.
#  Realize that these headers might be pasted in almost anywhere.
#  They don't always appear at the top of the message,
#  or even the top of a mime section.
#  They may even be indented, or prepended with leading greater than signs,
#  if a mail message is manually forwarded, or pasted inside a larger
#  mail message.
#  We willhowever assume that a header block, once begun,
#  continues until we reach a blank line.
#  If you've manually pasted a header and body together, sorry,
#  but the body is going to get thrown away.
#  This routine is recursive, so make sure the appropriate variables are auto.
#  Pass the start and end offsets -- a sub-message inside the entire message.
sub findHeaders($$) ;
sub findHeaders($$)
{
my ($start, $end) = @_;
++$fhLevel;
my $startLine = -1;
my $boundaryCut = "";
my ($i, $j, $temp, $line, $state);
my ($reply, $from, $subject, $date);
my ($boundary, $content, $encoding, $encfile);
my $expand64 = 0;

$line = $msg[$start];
if($line =~ s/^$mailBreak.*\nboundary=//so) {
$line =~ s/\n$//;
$boundaryCut = $line;
}

print "findheaders$fhLevel $start-$end<$boundaryCut\n" if $debug >= 6;

foreach $i ($start..$end) {
$line = $msg[$i];

#  Strip away whitespace and leading greater than signs.
$line =~ s/^[ \t>]+//;
$line =~ s/\s+$//;

#  Are we expanding binary data?
if($expand64) {
$expand64 = 0 if $line eq "";
if(length $boundaryCut and $expand64) {
$temp = $line;
$temp =~ s/^-+//;
$temp =~ s/-+$//;
$expand64 = 0 if $temp eq $boundaryCut;
}
if($expand64) {
my ($c, $leftover, $rem);
#  We don't really need the padding equals to run the algorithm properly.
#  Sometimes it ends in =9  I don't know what that means!
$line =~ s/=+9*$//;
if($line =~ y;+/a-zA-Z0-9;;cd && !$bad64) {
warn "Invalid base64 encoding at line $i";
$bad64 = 1;
}
for($j=0; $j < length $line; ++$j) {
$c = $b64_map[ord substr($line,$j,1)];
$rem = $j & 3;
if($rem == 0) {
$leftover = $c<<2;
} elsif($rem == 1) {
$$curPart{data} .= chr($leftover | ($c>>4));
$leftover = ($c & 0xf) <<4;
} elsif($rem == 2) {
$$curPart{data} .= chr($leftover | ($c>>2));
$leftover = ($c & 3) <<6;
} else {
$$curPart{data} .= chr($leftover | $c);
}
}
$msg[$i] = "";
next;
}
}

#  Look for mailKeyWord:
#  We check for a header until we have established a boundary,
#  and then we only crack the header at the start of each section.
if(($startLine >= 0 or  # inside a header
! length $boundaryCut or  # no boundary yet
$msg[$i-1] eq "$mimeBreak $fhLevel")  # top of the mime section
and
$line =~ /^\$?[a-zA-Z][\w-]*:/) {  # keyword:
if($startLine < 0) {
$startLine = $i;
$state = 0;
$reply = $from = $subject = $date = "";
$boundary = $content = $encoding = $encfile = "";
}
($headKey = $line) =~ s/:.*//;
$headKey = lc $headKey;
($headVal = $line) =~ s/^[^:]+:\s*//;
my $headKeyType = $mhWords{$headKey};
$state |= $headKeyType if defined $headKeyType;
if($headVal ne "") {
$from = $headVal if $headKey eq "from";
$reply = $headVal if $headKey eq "reply-to";
$subject = $headVal if $headKey eq "subject";
$date = $headVal if $headKey eq "date";
$date = $headVal if $headKey eq "sent";
if($headKey eq "content-transfer-encoding") {
$encoding = lc $headVal;
}
if($headKey eq "content-type") {
$content = lc $headVal;
$content =~ s/;.*//;
}
}  # something after keyword:
}  # keyword: mail/mime header line

if($startLine >= 0) {
#  boundary= is a special attribute within a mail header
$temp = $line;
if($temp =~ s/.*boundary *= *//i) {
($temp =~ s/^"//) ?
($temp =~ s/".*//) :
($temp =~ s/,.*//);
$boundary = $temp;
$boundary =~ s/^-+//;
$boundary =~ s/-+$//;
$boundaryCut = $boundary if length $boundary and ! length $boundaryCut;
}  # boundary keyword detected
#  filename is similarly set.
$temp = $line;
if($temp =~ s/.*(?:file)?name *= *//i) {
($temp =~ s/^"//) ?
($temp =~ s/".*//) :
($temp =~ s/,.*//);
$encfile = $temp;
}

} else {

next if ! length $boundaryCut;
#  Strip away leading and trailing hyphens -- helps us look for boundary
$line =~ s/^-+//;
$line =~ s/-+$//;
next if $line ne $boundaryCut;
$msg[$i] = "$mimeBreak $fhLevel";
next;
}  # body

#  Now we know we're inside a mail header.
next if length $line;

#  We've got a blank line -- that ends the header.
#  But it's not really a header if we've just got English keywords.
if($state&5) {

if(length $boundary) {
#  Skip the preamble.
foreach $j ($i+1..$#msg) {
$temp = $msg[$j];
$temp =~ s/^-+//;
$temp =~ s/-+$//;
last if $temp eq $boundary;
$msg[$j] = "";
}
}

#  Handle the various encodings.
$encoding = "" if length $boundary and $startLine == $start;
$encoding = "" if $encoding eq "8bit" or $encoding eq "7bit" or $encoding eq "binary";
if($encoding eq "quoted-printable") {
qp_lowlevel($i+1, $end, $boundaryCut);
$encoding = "";
}
if($encoding eq "base64") {  # binary attachment
$expand64 = 1;
$curPart = { data => "", filename => $encfile, isattach => 1};
push @mimeParts, $curPart;
++$nat;
$encoding = "";
}
if($encoding and !$badenc) {
warn "Unknown encoding at line $i $encoding";
$badenc = 1;
}

$j = $startLine;
if($state & 4 or length $boundary) {
#  Process from/reply lines.
$reply = $from if ! length $reply;
$from = $reply if ! length $from;
$from =~ s/".*// if $from =~ s/^"//;
$from =~ s/\s*<.*>.*$//;
$reply =~ s/^.*<(.*)>.*$/$1/;
$reply = ""
if length $reply and (
$reply =~ /[\s<>]/ or $reply !~ /\w@\w/ or $reply !~ /\w\.\w/);
#  Strip away  re:  and  fwd:
while($subject =~ s/^(re|fd|fwd)[,:]\s*//i) { }

$mailSubject = $subject,
$mailFrom = $from,
$mailReply = $reply,
$mailDate = $date
if $startLine == 0;  # top of the message

#  Consolodate the header.
$line = "$mailBreak\n";
$line .= "Subject: $subject\n" if length $subject;
$line .= "From: $from\n" if length $from;
$line .= "Date: $date\n" if length $date;
$line .= "Reply-to: $reply\n" if length $reply;
$line .= "boundary=$boundary\n" if length $boundary;
$msg[$j++] = $line;
}  # mail header
$msg[$j++] = "" while $j <= $i;

#  Decode html, if specified in the header.
#  Or turn it into an attachment, if anything other than plain text.
if(length $content and ! $expand64) {
if($content eq "text/html" or length $encfile) {
mailHtml($i+1, $end, $startLine-1, $boundaryCut, $encfile);
$content = "text/plain";
}
}

}  # mail or mime header

$startLine = -1;
}  # loop over lines in the message

if(length $boundaryCut) {
#  Still more work to do.
#  Reprocess each section.
$boundary = "$mimeBreak $fhLevel";
$j = -1;
foreach $i ($start..$end) {
next unless $msg[$i] eq $boundary;
findHeaders($j+1, $i-1) if $j >= 0;
$j = $i;
}  # loop over lines
}  # bounhdary encountered

--$fhLevel;
}  # findHeaders

#  process an html mime section within a mail message.
sub mailHtml($$$$$)
{
my ($start, $end, $breakLine, $boundary, $filename) = @_;
return if $end < $start;  # should never happen
my ($i, $line);

my $tbuf = "";

foreach $i ($start..$end) {
$line = $msg[$i];
$line =~ s/^[ \t>]*//;

#  boundary may end this section.
if(length $boundary) {
my $temp = $line;
$temp =~ s/^-+//;
$temp =~ s/-+$//;
$end = $i-1, last if $temp eq $boundary;
}

$tbuf .= "$line\n";
$msg[$i] = "";
}  # loop over lines

if(length $filename) {  # present as attachment
$curPart = { data => $tbuf, filename => $filename, isattach => 1};
push @mimeParts, $curPart;
++$nat;
return;
}

my $cx = cxCreate(\$tbuf, $filename);
my $precx = $context;
cxSwitch $cx, 0;
readyUndo();
#  $tbuf still holds the html attachment
$badHtml = 1;
renderHtml(\$tbuf) and
pushRenderedText(\$tbuf);
cxSwitch $precx, 0;

++$cx;
print "switch to session $cx for the html version of this mail\n" unless $ismc;
}  # mailHtml

#  Connect to the mail server.
sub pop3connect($$)
{
my $remote = shift;
my $port = shift;
my $iaddr   = inet_aton($remote)               or
$errorMsg = "cannot locate the mail server $remote", return 0;
my $paddr   = sockaddr_in($port, $iaddr);
my $proto   = getprotobyname('tcp');
socket(SERVER_FH, PF_INET, SOCK_STREAM, $proto)  or
$errorMsg = "Cannot establish TCP socket", return 0;
connect(SERVER_FH, $paddr)    or
$errorMsg = "Cannot connect to mail server $remote", return 0;
SERVER_FH->autoflush(1);
return 1;
}  # pop3connect

#  Put and get lines from the mail server.
sub serverPutLine ($)
{
my $line = shift;
if($debug >= 7) {
my $t = $line;
$t =~ s/\r\n/\n/g;
print "$t\n";
}
print SERVER_FH $line.$eol or
$errorMsg = "Could not write to the mail socket", return 0;
return 1;
}  # serverPutLine

sub serverGetLine()
{
defined($serverLine = <SERVER_FH>)
or $errorMsg = "could not read from the mail socket", return 0;
#  strip trailing newline indicator(s)
$serverLine =~ s/[\r\n]+$//;
print "< $serverLine\n" if $debug >= 7;
return 1;
}  # serverGetLine

sub serverClose($)
{
my $scheme = shift;
#  Should we make $scheme global instead of passing it around?
if($scheme =~ /(smtp|pop3)/i) {
serverPutLine("quit");
} elsif ($scheme =~ /ftp/i) {
serverPutLine "abor${eol}quit";
# Nope, abor is not a typo.
my @disposeOf = <SERVER_FH>;
close FDFH if defined FDFH;
close FLFH if defined FLFH;
}
sleep 2;
close SERVER_FH;
}  # serverClose

# This subroutine was taken from MIME::Base64 by Gisle Aas.
sub encodeBase64($$$)
{
my($in, $eol, $out) = @_;
my $inl = length $$in;
#  uuencode is pretty close
$$out = pack 'u', $$in;
#  get rid of first and last char
$$out =~ s/^.//;
chop $$out;
#  Get rid of newlines inside
$$out =~ s/\n.//g;
#  Over to base 64 char set
    $$out =~ tr|` -_|AA-Za-z0-9+/|;
    # fix padding at the end
    my $padding = (3 - $inl%3) % 3;
    $$out =~ s/.{$padding}$/'=' x $padding/e if $padding;
    # break encoded string into lines of no more than 76 characters each
    if (length $eol) {
	$$out =~ s/(.{1,72})/$1$eol/g;
    }
}  # encodeBase64

#  Read the file into memory, mime encode it,
#  and return the type of encoding and the encoded data.
#  Last three parameters are result parameters.
sub encodeAttachment($$$$$)
{
my ($atfile, $isMail, $res_enc, $res_type, $res_data) = @_;
my ($subline, $buffer, $fsize, $rsize);

if(!$isMail) {
if($atfile =~ /^\d+$/) {  # edbrowse session
my $cx = $atfile - 1;
$buffer = "";
for(my $ln=1; $ln<=$dol[$cx]; ++$ln) {
$buffer .= fetchLineContext($ln, 1, $cx);
$buffer .= "\n" if $ln < $dol[$cx];
}
$fsize = $rsize = length $buffer;
} else {
open FH, $atfile or
$errorMsg = "cannot open attachment file $atfile,$!", return 0;
binmode FH, ':raw' if $doslike;
$fsize = (stat(FH))[7];
$rsize = 0;
$buffer = "";
$rsize = sysread(FH, $buffer, $fsize) if $fsize;
close FH;
$rsize == $fsize or
$errorMsg = "cannot read the contents of $atfile,$!", return 0;
}
} else {
$buffer = $$atfile;
#  We just made a copy of the mail to send; hope it wasn't too big.
$atfile = $mailToSend;
$fsize = $rsize = length $buffer;
$buffer =~ s/^\s*subject\s*:\s*/Subject: /i or
$errorMsg = "$atfile does not begin with a line `Subject: subject of your mail'", return 0;
$buffer =~ s/\r\n/\n/g;
$buffer .= "\n" if substr($buffer, -1) ne "\n";
$buffer .= ':';  # temporary
#  Extra blank line after subject.
$buffer =~ s/^(.*\n)(.)/$1\n$2/;
$buffer =~ /^(.*)\n/;
$subline = $1;
substr($buffer, -1) = "";  # get rid of :
length $subline < 90 or
$errorMsg = "subject line too long, limit 80 characters", return 0;
}  # primary mail message

my $newbuf = "";
my ($c, $col, $j, $ctype, $enc);

#  Count nonascii characters.
my $nacount = $buffer =~ y/\x80-\xff/\x80-\xff/;
#  Count null characters.
my $nullcount = $buffer =~ y/\0/\0/;
$nacount += $nullcount;

if($nacount*5 > $fsize and $fsize > 20) {
! $isMail or
$errorMsg = "cannot mail the binary file $atfile - perhaps this should be an attachment?", return 0;

encodeBase64(\$buffer, "\n", \$newbuf);

$ctype = "application/octet-stream";
$ctype = "application/PostScript" if $atfile =~ /\.ps$/i;
$ctype = "image/jpeg" if $atfile =~ /\.jpeg$/i;
$ctype = "image/gif" if $atfile =~ /\.gif$/i;
$ctype = "audio/basic" if $atfile =~ /\.wav$/i;
$ctype = "video/mpeg" if $atfile =~ /\.mpeg$/i;
$enc = "base64";
$$res_type = $ctype;
$$res_enc = $enc;
$$res_data = $newbuf;
return 1;
}  # base 64 encode

#  Use the filename of the edbrowse session to determine type.
if($atfile =~ /^\d+$/) {
$atfile = $fname[$atfile-1];
}
$ctype = "text/plain";
$ctype = "text/html" if $atfile =~ /\.(htm|html|shtml|asp)$/i;
$ctype = "text/richtext" if $atfile =~ /\.rtf$/i;

#  Switch to unix newlines - we'll switch back to dos later.
$buffer =~ s/\r\n/\n/g;
$fsize = length $buffer;

if($nacount*20 < $fsize) {
#  Looks like it's almost all ascii, but we still have to switch to qp
#  if the lines are too long.
$col = 0;
for($j =0; $j < $fsize; ++$j) {
$c = substr $buffer, $j, 1;
$col = 0, next if $c eq "\n";
++$col;
$nacount = $fsize, last if $col > 500 or $col > 120 and ! $isMail;
}
}

if($nullcount or $nacount*20 >= $fsize) {
$buffer =~ s/([^\t\n-<>-~])/sprintf("=%02X", ord $1)/ge;
$buffer =~ s/ $/=20/m;
$buffer =~ s/\t$/=09/m;
#  Cut long lines, preferably after a space, but wherever we can.
$fsize = length $buffer;
my $spaceCol = 0;
$col = 0;
for($j =0; $j < $fsize; ++$j) {
$c = substr $buffer, $j, 1;
$newbuf .= $c;
if($c eq "\n") {  # new line, column 0
$spaceCol = $col = 0;
next;
}
++$col;
if($c eq " " || $c eq "\t") {
$spaceCol = length $newbuf;
}
next if $col < 72;
#  Don't break an = triplet.
next if $c eq '=';
next if substr($newbuf, -2, 1) eq '=';
#  If we're near the end, don't worry about it.
next if $j == $fsize - 1;
#  If newline's coming up anyways, don't force another one.
$c = substr $buffer, $j+1, 1;
next if $c eq "\n";
#  Ok, it's a long line, we need to cut it.
$spaceCol = length $newbuf if ! $spaceCol;
substr($newbuf, $spaceCol, 0) = "=\n";
$spaceCol += 2;
$col = length($newbuf) - $spaceCol;
$spaceCol = 0;
}

if($isMail) {
#  Don't qp the subject.
$newbuf =~ s/^.*/$subline/;
}

$enc = "quoted-printable";
$$res_type = $ctype;
$$res_enc = $enc;
$$res_data = $newbuf;
return 1;
}

#  Almost all ascii, short lines, no problems.
$enc = ($nacount ? "8bit" : "7bit");
$$res_type = $ctype;
$$res_enc = $enc;
$$res_data = $buffer;
return 1;
}  # encodeAttachment
#  Don't forget to turn lf into crlf before you send this on to smtp.

#  Send mail to the smtp server.
#  sendMail(recipients, mailtext, attachments)
#  Everything passed by reference.
sub sendMail($$$)
{
my ($tolist, $main, $atlist) = @_;
length $outmailserver or
$errorMsg = "No mail server specified - check your $home/.ebrc file", return 0;

my $proto = 'smtp';
my $reply = $replyAddress[$whichMail];
$altattach == 0 or $altattach == $#$atlist+1 or
$errorMsg = 'either none or all of the attachments must be declared "alternative"', return 0;

#  Read and/or refresh the address book.
if(length $addressFile and -e $addressFile) {
my $newtime = (stat($addressFile))[9];
if($newtime  > $adbooktime) {
%adbook = ();
$adbooktime = $newtime;
my ($alias, $email);
open FH, $addressFile or
$errorMsg = "Cannot open address book $addressFile.", return 0;
while(<FH>) {
s/\n$//;  # don't need nl
next if /^\s*#/;  # comment line
next if /^\s*$/;  # blank line
($alias = $_) =~ s/:.*//;
($email = $_) =~ s/^[^:]*:([^:]*).*/$1/;
$adbook{$alias} = $email;
}
close FH;
}
}

#  Resolve recipients against address book.
foreach my $who (@$tolist) {
next if $who =~ /@/;
my $real = $adbook{$who};
if(defined $real and length $real) {
#  Remember that $who is a by reference variable, being in the for loop.
$who = $real;
next;
}
length $addressFile or
$errorMsg = "No address book specified - check your $home/.ebrc file", return 0;
$errorMsg = "alias $who not found in your address book";
return 0;
}

#  Verify attachments are readable.
foreach my $f (@$atlist) {
if($f =~ /^\d+$/) {
my $cx = $f - 1;
cxCompare($cx) or return 0;
defined $factive[$cx] and $dol[$cx] or
$errorMsg = "session $f is empty - cannot atach", return 0;
} else {
-r $f or
$errorMsg = "cannot access attachment $f", return 0;
}
}

my $mustmime = $#$atlist + 1;
my ($sendEnc, $sendType, $sendData);
encodeAttachment($main, 1, \$sendEnc, \$sendType, \$sendData) or return 0;
$mustmime = 1 if $sendEnc =~ /^q/;

#  Boundary, for sending attachments.
my $sendBound = rand;
$sendBound =~ s/^0./nextpart-domail/;

#  Looks good - let's get going.
pop3connect($outmailserver, 25) or return 0;

normal: {
serverGetLine() or last normal;
while($serverLine =~ /^220-/) {
serverGetLine() or last normal;
}
$serverLine =~ /^220 / or
$errorMsg = "Unexpected prompt <$serverLine> at the start of the sendmail session", last normal;

serverPutLine "helo $smtplogin" or last normal;
serverGetLine() or last normal;
$serverLine =~ /^250 / or
$errorMsg = "The mail server doesn't recognize $smtplogin", last normal;

serverPutLine "mail from: $reply" or last normal;
serverGetLine() or last normal;
$serverLine =~ /^250 / or
$errorMsg = "mail server rejected $reply <$serverLine>", last normal;

my $reclist = "";  # list of recipients
my $reccount = 0;  # count recipients
foreach my $f (@$tolist) {
$f = "\"$f\"" if $f =~ /[^\w,.@=_-]/;
$reclist .= ", " if $reccount;
++$reccount;
$reclist .= $f;
serverPutLine "rcpt to: $f" or last normal;
serverGetLine() or last normal;
$serverLine =~ /^250 / or
$errorMsg = "mail server rejected $f <$serverLine>", last normal;
}  # loop over recipients

serverPutLine "data" or last normal;
serverGetLine() or last normal;
$serverLine =~ /^354 / or
$errorMsg = "The mail server is not ready to accept email data <$serverLine>", last normal;
serverPutLine "To: $reclist$eol" .
"From: $myname <$reply>$eol" .
"Reply-To: $myname <$reply>$eol" .
"Date: " . mailTimeString() . $eol .
"Mime-Version: 1.0" or last normal;

#  dot alone tells smtp we're done.
#  Make sure there isn't a dot line in the middle of the mail.
$sendData =~ s/^\.$/ ./gm;
#  serverPutLine() routine already adds the last newline.
substr($sendData, -1) = "" if substr($sendData, -1) eq "\n";
#  smtp requires crlf.
$sendData =~ s/\n/\r\n/g;

if(! $mustmime) {
serverPutLine "Content-Type: $sendType$eol" .
"Content-Transfer-Encoding: $sendEnc" or last normal;
} else {
$sendData =~ s/^(.*\r\n)// or
$errorMsg = "could not pull subject line out of sendData", last normal;
my $subline = $1;
serverPutLine $subline .
"Content-Type: multipart/" .
($altattach ? "alternative" : "mixed") .
"; boundary=$sendBound$eol" .
"Content-Transfer-Encoding: 7bit$eol" .
$eol .
"This message is in MIME format. Since your mail reader does not understand$eol" .
"this format, some or all of this message may not be legible.$eol" .
$eol .
"--$sendBound$eol" .
"Content-Type: $sendType$eol" .
"Content-Transfer-Encoding: $sendEnc" or last normal;
}
serverPutLine $sendData or last normal;

if($mustmime) {
foreach my $f (@$atlist) {
encodeAttachment($f, 0, \$sendEnc, \$sendType, \$sendData) or last normal;
serverPutLine "$eol--$sendBound$eol" .
"Content-Type: $sendType" .
#  If the filename has a quote in it, forget it.
#  Also, suppress filename if this is an alternative presentation.
#  Also, suppress filename if you pulled it out of an edbrowse session.
(($altattach or $f =~ /"/ or $f =~ /^\d+$/) ?
"" : "; name=\"$f\"") . $eol .
"Content-Transfer-Encoding: $sendEnc$eol" or last normal;

$sendData =~ s/^\.$/ ./gm;
substr($sendData, -1) = "" if substr($sendData, -1) eq "\n";
$sendData =~ s/\n/\r\n/g;
serverPutLine $sendData or last normal;
}  # loop over attachments
#  Last boundary.
serverPutLine "$eol--$sendBound--" or last normal;
}  # mime parts

serverPutLine "." or last normal;
serverGetLine() or last normal;
$serverLine =~ /message (accepted|received)/i or
$serverLine =~ /^250/ or
$errorMsg = "Could not send mail message <$serverLine>", last normal;
serverClose($proto);
return 1;
}  # normal processing

close SERVER_FH;
return 0;  # failed
}  # sendMail

#  Send the current session as outgoing mail.
sub sendMailCurrent()
{
dirBrowseCheck("send mail") or return 0;
$fmode&$binmode and $errorMsg = "cannot mail a binary file - should this be an attachment?", return 0;
$dol or $errorMsg = "cannot mail an empty file", return 0;
$whichMail = $localMail;

#  Gather recipients and attachments, until we reach subject:
my @tolist = ();
my @atlist = ();
my ($ln, $t);
my $subject = 0;
for($ln=1; $ln<=$dol; ++$ln) {
$t = fetchLine $ln, 0;
$t =~ s/^reply[ -]to:* /to:/i;
$t =~ s/^mailto:/to:/i;
push(@tolist, $1), next if $t =~ /^to\s*:\s*(.*?)[ \t]*$/i;
if($t =~ /^(attach|alt)\s*:\s*(.*?)[ \t]*$/i) {
$altattach++ if lc($1) eq "alt";
push(@atlist, $2);
next;
}
$whichMail = $1, next if $t =~ /^account\s*:\s*(\d+)[ \t]*$/i;
$subject = 1 if $t =~ /^subject\s*:/i;
last;
}
$whichMail = $smMail if length $smMail;
$subject or $errorMsg = "line $ln, should begin with to: attach: or subject:", return 0;
$#tolist >= 0 or $errorMsg = "no recipients specified - place `To: emailAddress' at the top of your file", return 0;
$whichMail <= $#inmailserver or $errorMsg = "account $whichMail is out of range", return 0;

my $tbuf = "";
$tbuf .= fetchLine($_, 0) . "\n" foreach ($ln..$dol);
$mailToSend = "buffer";
return sendMail(\@tolist, \$tbuf, \@atlist);
}  # sendMailCurrent


#  runtime code starts here.
#  Think of this code as being inside main(){}

if($doslike) {
#  Buffered I/O messes me up when this runs on NT, over telnet.
STDOUT->autoflush(1);
#  The shell doesn't expand wild cards, let's do it here.
my @arglist = ();
push @arglist, glob($_) foreach (@ARGV);
@ARGV=@arglist;
}

if($#ARGV >= 0 and $ARGV[0] eq "-v") {
print "$version\n";
exit 0;
}

#  debug option
if($#ARGV >= 0 and $ARGV[0] =~ /^-d(\d*)$/) {
$debug = (length $1 ? $1 : 4);
shift @ARGV;
}

#  error exit option
if($#ARGV >= 0 and $ARGV[0] eq '-e') {
$errorExit = 1;
shift @ARGV;
}

#  -m is a special flag; run as a mail client.
if($#ARGV >= 0 and $ARGV[0] =~ /^-(u?)m(\d+)$/) {
$ismc = 1;  # running as a mail client
my $unformat = length $1;
my $account = $2;
shift @ARGV;
$#inmailserver >= 0 or
dieq "there are no mail accounts in your .ebrc config file.";
$account <= $#inmailserver or
dieq "account designator $account is out of range.";
$whichMail = $account;
my @atfiles = ();
my $mailBuf = "";

if($#ARGV == 0 and $ARGV[0] eq "-Zap") {
$zapmail = 1;
shift @ARGV;
}

while($#ARGV>= 0) {
my $arg = pop @ARGV;
if($arg =~ s/^([-+])//) {
++$altattach if $1 eq '-';
open FH, $arg or
dieq "cannot access attachment $arg.";
close FH;
unshift @atfiles, $arg;
} else {
$mailToSend = $arg;
open FH, $mailToSend
or dieq "Cannot access send file $mailToSend.";
dieq "Send file $mailToSend has zero size." if -z FH;
binmode FH, ':raw' if $doslike;
my $fsize = (stat(FH))[7];
my $rsize = sysread(FH, $mailBuf, $fsize);
close FH;
$rsize == $fsize or
dieq "cannot read the contents of $mailToSend,$!";
last;
}
}  # loop looking for files to transmit

if(length $mailToSend or $#atfiles >= 0) {
#  Mail client is in send mode.
length $mailToSend or
dieq "all arguments are attachments - you must include a plain send file.";
$#ARGV >= 0 or dieq "No recipients specified.";
sendMail(\@ARGV, \$mailBuf, \@atfiles) or dieq $errorMsg;
exit 0;
}  # send mail

#  Move to the mail directory.
length $mailDir or dieq "mailbox directory not specified in your .ebrc file.";
chdir $mailDir or dieq "Cannot change directory to $mailDir.";

#  Now fetch the mail and process it,
#  and ask the user what to do with it.
#  Begin with the pop3 login/password sequence.
my $proto = "pop3";
pop3connect($inmailserver[$whichMail], 110) or dieq $errorMsg;
serverGetLine();
$serverLine =~ /^\+OK /
or dieq "Unexpected pop3 introduction <$serverLine>.";
my $login = $pop3login[$whichMail];
my $password = $pop3password[$whichMail];
serverPutLine("user $login");
serverGetLine();
#  perhaps we require a password?
if($password) {
serverPutLine("pass $password");
serverGetLine();
}  # sending password
$serverLine =~ /^\+OK/
or dieq "Could not complete the pop3 login/password sequence <$serverLine>.";

#  determine number of messages
serverPutLine("stat");
serverGetLine();
$serverLine =~ /^\+OK /
or dieq "Could not obtain status information on your mailbox <$serverLine>.";
my $nmsgs = substr($serverLine, 4);
$nmsgs =~ s/ .*//;

if(!$nmsgs) {
print "No mail\n";
serverClose($proto);
exit 0;
}

my $mailHuge = "Mail message consumes more than a million lines; you won't be able to use this client.";
print "$nmsgs messages\n";
if($zapmail) {
$nmsgs = 300 if $nmsgs > 300;
}

#  Iterate over messages.
foreach my $m (1..$nmsgs) {
my ($filename, $j, $curpart, $rendered);
#  Is this mail automatically going somewhere else?
my $redirect = "";
my $delFlag = 0;

if($zapmail) {
$delFlag = 1;
} else {

#  Clear out the editor before we read in the next message.
foreach $j (0..$#factive) {
cxReset $j, 1;
}
$context = 0;  # probably not necessary
$factive[0] = 1;  # mail goes into session 0
$#text = 1;
$text[0] = "";
$text[1] = "--------------------------------------------------------------------------------";

#  retrieve the entire mth message from the server.
serverPutLine("retr $m");
my $exact_msg = "";  # an exact copy of the email
# Throw first line away, it's from the pop3 server, not part of the mail.
serverGetLine();
$j = 1;
serverGetLine();
while($serverLine ne ".") {
$exact_msg .= "$serverLine\n";
lineLimit 1 and dieq $mailHuge;
push @text, $serverLine;
++$j;
$map .= sprintf($lnformat, $j);
serverGetLine();
}
$dot = $dol = $j-1;

if(not $unformat) {
#  Browse the mail message for display.
$btags[0] = $btags = [];
$$btags[0] = {tag => "special", fw => {} };
$badHtml = 1;
$mailSubject = $mailFrom = $mailReply = $mailDate = "";
renderMail(\$rendered) and pushRenderedText(\$rendered) or
dieq $errorMsg;
$rendered = undef;  # don't need it any more

#  Break the lines in the buffer.
$fmode &= ~$browsemode;  # so I can run the next command
evaluate(",bl");
$errorMsg = "";
$dot = $dol;
$fmode |= $browsemode;

#  Let user know about attachments.
my $unat = 0;  # unnamed attachments
my $exat = 0;  # attachment already exists
if($nat) {
print "$nat attachments.\n";
$j = 0;
foreach $curPart (@mimeParts) {
next unless $$curPart{isattach};
++$j;
$filename = $$curPart{filename};
++$unat, next unless length $filename;
print "$j = $filename";
if(-e $filename) {
print " exists";
$exat = 1;
}
print "\n";
}
}

#  Paste on the html segments.
foreach $j (1..$#factive) {
next unless $factive[$j];
next unless $dol[$j];
$map .= sprintf($lnformat, 0) if $dol;
if($dol > 4) {
$map .= sprintf($lnformat, 1);
$map .= sprintf($lnformat, 0);
}
$map .= substr($map[$j], $lnwidth);
$dot = $dol = length($map)/$lnwidth - 1;
}
foreach my $t (@text) {
removeHiddenNumbers \$t;
}

#  See if the mail is redirected.
if(length $mailReply and $#fromSource >= 0) {
my $lowReply = lc $mailReply;
foreach my $j (0..$#fromSource) {
next unless index($lowReply, $fromSource[$j]) >= 0;
$redirect = $fromDest[$j];
last;
}
}

#  I'm not going to redirect mail if there are unamed or existing attachments.
$redirect = "" if $redirect ne "x" and $unat + $exat;
}  # formatting the mail message

my $dispLine = 1;
if(length $redirect) {
$delFlag = 1;
#  Replace % date/time fields.
if($redirect =~ /%[ymdhns]{2,}/) {
my ($ss, $nn, $hh, $dd, $mm, $yy) = localtime time;
$mm++;
$yy += 1900;
$redirect =~ s/%yyyy/sprintf "%4d", $yy/ge;
$redirect =~ s/%yy/sprintf "%02d", $yy%100/ge;
$redirect =~ s/%mm/sprintf "%02d", $mm/ge;
$redirect =~ s/%dd/sprintf "%02d", $dd/ge;
$redirect =~ s/%hh/sprintf "%02d", $hh/ge;
$redirect =~ s/%nn/sprintf "%02d", $nn/ge;
$redirect =~ s/%ss/sprintf "%02d", $ss/ge;
}
print "$mailReply > $redirect\n";
}

#  display the next page of mail and get an input character.
dispInput: {
if(! $delFlag) {
print("skipped\n"), $delFlag = 1, last if  ! $unformat and length $mailSubject and defined $junkSubjects{$mailSubject};
foreach $j (keys %junkSubjects) {
next unless $j =~ /^`/;
my $trash = $j;
$trash =~ s/^`//;
next unless index($exact_msg, $trash) >= 0;
print("trash\n"), $delFlag = 1, last dispInput;
}
if($dispLine <= $dol) {
foreach $j (1..20) {
last if $dispLine > $dol;
my $line = fetchLine $dispLine, 0;
#  Don't print date and return address, but they will be recorded,
#  if you save the file.
next if $line =~ /^Mail sent /;
next if $line =~ /^Reply to /;
print "$line\n";
} continue { ++$dispLine; }
}  # display next page
}  # not being deleted

getkey: {
my $key;
if($delFlag) {
last if $redirect eq "x";
$key = 'w';
} else {
#  Interactive prompt depends on whether there is more text or not.
STDOUT->autoflush(1);
print ($dispLine > $dol ? "? " : "* ");
STDOUT->autoflush(0);

$key = userChar("qx? nwkuJdA");
print "\b\b";

exit 0 if $key eq 'x';
print("quit\n"), serverClose($proto), exit 0 if $key eq 'q';
print("next\n"), last dispInput if $key eq 'n';
print("delete\n"), $delFlag = 1, last dispInput if $key eq 'd';

if($key eq ' ') {
print "End of message\n" if $dispLine > $dol;
redo dispInput;
}

if($key eq '?') {
print "?\tprint this help message.
q\tquit this program.
x\texit without changing anything on the mail server.
space\tread more of this mail message.
n\tmove on to the next mail message.
A\tadd the sender to your address book.
d\tdelete this message.
J\tjunk this subject, and delete any mail with this subject.
w\twrite this message to a file and delete it.
k\tkeep this message in a file, but don't delete it.
u\twrite this message unformatted to a file, and delete it.\n";
redo;
}

if($key eq 'J') {
print "No subject to junk\n", redo if $mailSubject eq "";
print "No junkfile specified in your .ebrc file\n", redo unless length $junkFile;
print "junk\n";
markSubject($mailSubject);
$delFlag = 1;
last dispInput;
}  # J

if($key eq 'A') {
print "No addressbook specified in your .ebrc file\n", redo unless length $addressFile;
print "Cannot establish sender's name and/or email address.", redo unless length $mailFrom and length $mailReply;
open FH, ">>$addressFile"
or dieq "Cannot append to $addressFile.";
$_ = lc $mailFrom;
s/\s/./g;
print "$_:$mailReply\n";
print FH "$_:$mailReply\n";
close FH;
redo;
}  # A
}  # delFlag or not

#  At this point we're saving the mail somewhere.
$delFlag = 1 if $key ne 'k';

if(length $redirect) {
$filename = $redirect;
} else {
$filename = getFileName(undef, 0);
}
if($filename ne "x") {
my $append = (-e $filename);
open FH, ">>$filename"
or dieq "Cannot create mail file $filename.";  # should not happen
my $fsize = 0;
if($key eq 'u'or $unformat) {
print FH $exact_msg
or dieq "Cannot write to mail file $filename.";
$fsize = length $exact_msg;
} else {
foreach $j (1..$dol) {
my $line = fetchLine $j, 0;
print FH "$line\n"
or dieq "Cannot write to mail file $filename.";
$fsize += length($line) + 1;
}
}
close FH;
print "mail saved, $fsize bytes";
print " appended" if $append;
print "\n";
}

if($key ne 'u' and $redirect ne 'x') {
#  Ask the user about any attachments.
$j = 0;
foreach $curPart (@mimeParts) {
next unless $$curPart{isattach};
++$j;
$filename = $$curPart{filename};
if(length $redirect) {
print "attach $filename\n";
} else {
print "Attachment $j ";
$filename = getFileName($filename, 1);
next if $filename eq "x";
}
open FH, ">$filename"
or dieq "Cannot create attachment file $filename.";
binmode FH, ':raw' if $doslike;
print FH $$curPart{data}
or dieq "Cannot write to attachment file $filename.";
close FH;
}  # loop over attachments
}  # key other than 'u'

}  # input key
}  # display and input
}  # interactive or zap

if($delFlag) {  #  Delete the message.
#  Remember, it isn't really gone until you quit the session.
#  So if you didn't want to delete, type x to exit abruptly,
#  then fetch your mail again.
serverPutLine("dele $m");
serverGetLine() or
dieq "Sorry, you took too long; mail server hung up.";
$serverLine =~ /^\+OK/
or dieq "Unable to delete message <$serverLine>.";
}  # Del

}  # loop over messages

print "$nmsgs\n" if $zapmail;

serverClose($proto);  # that's all folks!
exit 0;
}  # end mail client

#  Initial set of commands.
if($commandList{init}) {
evaluateSequence($commandList{init}, $commandCheck{init});
}

#  Process the command line arguments.
foreach my $cx (0..$#ARGV) {
my $file = $ARGV[$cx];
cxSwitch($cx, 0) if $cx;
$changeFname = "";
my $rc = readFile($file, "");
print "$filesize\n";
$rc or print $errorMsg,"\n";
$fname = $file;
$fname = $changeFname if length $changeFname;
$fmode &= ~($changemode|$firstopmode);
if($rc and $filesize and is_url($fname)) {
#  Go ahead and browse it.
$inglob = $intFlag = 0;
$filesize = -1;
$rc = evaluate("b");
print "$filesize\n" if $filesize >= 0;
$rc or print "$errorMsg\n";
}  # open of url
}  # loop over args on the command line
cxSwitch(0, 0) if $context;
print "edbrowse ready\n" if ! length $fname;

#  get user commands.
while(1) {
my $line = readLine();
my $saveLine = $line;
$inglob = 0;
$intFlag = 0;
$filesize = -1;
my $rc = evaluate($line);
print "$filesize\n" if $filesize >= 0;
if(!$rc) {
print ((($helpall or $cmd =~ /[$showerror_cmd]/o) ? $errorMsg : "?"), "\n");
exit 1 if $errorExit;
}
$linePending = $saveLine;
if($ubackup) {
$lastdot = $savedot, $lastdol = $savedol;
$lastmap = $savemap, $lastlabels = $savelabels;
$ubackup = 0;
}
}   # infinite loop

#*********************************************************************
#  The following code is written and maintained by Chris Brannon,
#  cbrannon@wilnet1.com
#  It manages secure http and ftp connections.
#*********************************************************************

sub do_ssl($$$$)
{
# Do the SSL thing.  This takes four arguments: server, port, message,
# and buffer reference.
# <message> is a scalar containing http headers.  <buffer reference> is
# a reference to a scalar.  We tack each chunk of received data onto that
# scalar.  Thusly, we don't have to return a variable containing twenty
# MB of data.
# I borrow heavily from Karl's plain http connection code.
unless(eval { require Net::SSLeay }) {
$errorMsg = "you must have the Net::SSLeay module and OpenSSL toolkit to speak https", return 0;
}
# Should I error-check these values?  I don't know.  Probably.
my $server = shift;
my $port = shift;
my $message = shift;
my $bufref = shift;
my $iaddr = inet_aton($server) or
$errorMsg = "Cannot identify $server on the network", return 0;
my $paddr = sockaddr_in($port, $iaddr);
my $proto = getprotobyname('tcp');
socket(FH, PF_INET, SOCK_STREAM, $proto) or
$errorMsg = "cannot allocate a socket", return 0;
connect(FH, $paddr) or
$errorMsg = "cannot connect to $server on $port: $!", return 0;
Net::SSLeay::load_error_strings();
Net::SSLeay::SSLeay_add_ssl_algorithms();
Net::SSLeay::randomize();
$ctx = Net::SSLeay::CTX_new();
Net::SSLeay::CTX_set_options($ctx, &Net::SSLeay::OP_ALL);
if($ssl_verify) {
Net::SSLeay::CTX_load_verify_locations($ctx, $ebcerts, '') or
$errorMsg = "Error opening certificate file $ebcerts: $!", return 0;
Net::SSLeay::CTX_set_verify($ctx, &Net::SSLeay::VERIFY_PEER, 0);
}
# Should the user be warned somehow when SSL certificate verification has
# been turned off?  Accepting unverifiable certificates can be a security
# risk.  But some servers, like https://listman.redhat.com can't be verified
# with my certificate bundle.  So I make verification the default, but optional.
$ssl = Net::SSLeay::new($ctx);
Net::SSLeay::set_fd($ssl, fileno(FH)) or
$errorMsg = Net::SSLeay::ERR_error_string(Net::SSLeay::ERR_get_error()), return 0;
if(Net::SSLeay::connect($ssl) == -1) {
$errorMsg = Net::SSLeay::ERR_error_string(Net::SSLeay::ERR_get_error());
return 0;
}
Net::SSLeay::ssl_write_all($ssl, $message) or
$errorMsg = &Net::SSLeay::ERR_error_string(&Net::SSLeay::ERR_get_error()), return 0;
my ($chunk, $filesize, $rsize, $last_fk, $fk);
$fk = $last_fk = 0;
STDOUT->autoflush(1) if ! $doslike;
while($chunk = Net::SSLeay::ssl_read_all($ssl, 100000)) {
$$bufref .= $chunk; # how cute!
$rsize = length($chunk); # Is this computationally expensive??
$filesize += $rsize;
last if $rsize == 0;
$fk = int($filesize / 100000);
if($fk > $last_fk) {
print ".";
$last_fk = $fk;
}
last if($filesize >= $maxfile);
}
close(FH);
print "\n" if $last_fk > $fk;
STDOUT->autoflush(0) if ! $doslike;
$filesize <= $maxfile or
$errorMsg = "file is too large, limit 40MB", return 0;
defined $rsize or
$errorMsg = "error reading data from the socket", return 0;
#  There's no way to distinguish between a read error and reading a zero
#  length file.  I guess that's ok.
if(defined($filesize)) {
return $filesize;
} else {
return 0;
}
}   # do_ssl

sub ftp_connect($$$$)
{
my($host, $port, $path, $bufref) = @_;
my $proto = 'ftp';
my ($tempbuf, @disposeOf);
my $filesize = 0;
my $login = "anonymous";
my $password = 'some-user@edbrowse.net';
my $dataOpen = (
$passive ? \&pasvOpen : \&ftpListen);
if($host =~ s/^([^:@]*):([^:@]*)@//) {
$login = $1, $password = $2;
}
# Do an ftp connect, prompting for username & password.
my $iaddr = inet_aton($host) or
$errorMsg = "cannot identify $host on the network", return 0;
my $paddr = sockaddr_in($port, $iaddr);
socket(SERVER_FH, PF_INET, SOCK_STREAM, getprotobyname('tcp')) or
$errorMsg = "cannot allocate a socket", return 0;
connect(SERVER_FH, $paddr) or
$errorMsg = "cannot connect to $host", return 0;
SERVER_FH->autoflush(1);
STDOUT->autoflush(1) if !$doslike;
serverGetLine or serverClose($proto), return 0;
serverClose($proto), return 0 if ftpError($serverLine, 220, "server sent \"$serverLine\" while attempting login");
serverPutLine "user $login";
do {
serverGetLine or serverClose($proto), return 0;
}
while($serverLine =~ /^220/);
serverClose($proto), return 0 if ftpError($serverLine, 331, "invalid username: server sent $serverLine");
serverPutLine "pass $password";
serverGetLine or serverClose($proto), return 0;
serverClose($proto), return 0 if ftpError($serverLine, 230, "bad password: server sent $serverLine");
my $wmsg = "";  # welcome message
if($serverLine =~ s/^\s*230\s*?-\s*//) {
# We got a welcome message.
$wmsg = "$serverLine\n";
while(serverGetLine) {
last if $serverLine =~ /^\s*230\s*?[^-]/;
$serverLine =~ s/^\s*230\s*?-\s*//;
$wmsg .= "$serverLine\n";
}
}
$wmsg = "" unless $path eq "/";  # trash the welcome message, we're going somewhere else
serverPutLine "CWD $path";
serverGetLine or serverClose($proto), return 0;
if($serverLine =~ /^\s*250\s*/) {
if($serverLine =~ s/^\s*250\s*?-\s*//) {
# Its a directory-specific greeting.
$wmsg = "$serverLine\n";
while(serverGetLine) {
last if $serverLine =~ /^\s*250\s*?[^-]/;
$serverLine =~ s/^\s*250\s*?-\s*//;
$wmsg .= "$serverLine\n";
}
}
serverPutLine "type a";
serverGetLine or serverClose($proto), return 0;
serverClose($proto), return 0 if ftpError($serverLine, 200, "ASCII transfers not supported by server: received \"$serverLine\"");
&$dataOpen or
serverClose($proto), return 0;
serverPutLine "list";
serverGetLine or serverClose($proto), return 0;
serverClose($proto), return 0 if ftpError($serverLine, "150", "error retrieving directory listing");
$tempbuf = "";
ftpRead(\$tempbuf) or
serverClose($proto), return 0;
ParseList: {
serverPutLine "syst";
serverGetLine or serverClose($proto), return 0;
if($serverLine =~ /unix/i) {
# Good.  Let's try to htmlize this file listing.
my $base_filename = "ftp://$host";
$base_filename .= ":$port" if $port != 21;
$base_filename .= $path;
$base_filename .= '/' if $base_filename !~ m,/$,;
# Yah, I know.  That looks disgusting.
textUnmeta(\$wmsg);
$$bufref = "http/1.0 200 ok$eol$eol<html><head><title>Directory Listing</title>\n</head>\n<body>\n$wmsg<ul>\n";
my @lines = split("$eol", $tempbuf);
shift(@lines); # Ditch the "total: xxx" line from Unix ls
foreach $line (@lines) {
# Extract the filename and length from ls -l format
my @listItems = split /\s+/, $line;
my $mode = $listItems[0];
my $extracted = $listItems[$#listItems];
my $extlen = $listItems[$#listItems-4];
$extlen = "/" if $mode =~ /^d/;
$$bufref .= "<li><a href=\"$base_filename$extracted\">$extracted</a> $extlen\n";
}
$$bufref .= "</ul>\n</body>\n</html>\n";
$$bufref =~ s/<ul>\n<\/ul>/This ftp directory is empty./;
$filesize = length($$bufref);
} else {
$$bufref = $tempbuf; # Oh well...
}
serverPutLine "quit";
@disposeOf = <SERVER_FH>;
close SERVER_FH;
return 0 if !$filesize;
return $filesize;
} # ParseList
} else {
# Try to retr.  If unable, the path was bogus.
serverPutLine "type i";
serverGetLine or serverClose($proto), return 0;
serverClose($proto), return 0 if ftpError($serverLine, 200, "binary transfers unsupported by server: received \"$serverLine\"");
&$dataOpen or
serverClose($proto), return 0;
serverPutLine "retr $path";
serverGetLine or serverClose($proto), return 0;
serverClose($proto), return 0 if ftpError($serverLine, "150", 
"the path you specified in this URL is neither a filename nor a directory");
# Let's read our data.
$filesize = ftpRead($bufref);
serverClose($proto);
# The problem is, the ftp server will get an extraneous abor command when
# we close connection.  I only want these sent after an error condition, to
# abort a transfer.
return 0 if !$filesize;
return $filesize;
}
} # ftp_connect

sub ftpRead($)
{
# I don't like the fact that this subroutine returns 0 on error.  Seems wrong.
my $bufref = shift;
my $rsize = 0;
my $filesize = 0;
my $last_fk = 0;
my $chunk;
my $proto = 'ftp';
if(!$passive) {
my $check = '';
vec($check, fileno(FLFH), 1) = 1;
select($check, undef, undef, 10) or
$errorMsg = "ftp data connection timed out", $filesize =  0, goto Cleanup;
socket(FDFH, PF_INET, SOCK_STREAM, getprotobyname('TCP')) or
$errorMsg = "unable to allocate a socket", $filesize =  0, goto Cleanup;
accept(FDFH, FLFH);
shutdown(FDFH, 1);
}
while(defined($rsize = sysread(FDFH, $chunk, 100000))) {
print "sockread $rsize\n" if $debug >= 5;
$$bufref .= $chunk;
$filesize += $rsize;
last if $rsize == 0;
my $fk = int($filesize / 100000);
if($fk > $last_fk) {
print ".";
$last_fk = $fk;
}
last if $filesize >= $maxfile;
}
my $line;
serverGetLine or return 0;
close FDFH;
close FLFH;
# ignore it; it should read 226 transfer complete
print "\n" if $last_fk;
defined($rsize) or
$errorMsg = "error reading data from the socket", $filesize =  0, goto Cleanup;
$filesize <= $maxfile or
$errorMsg = "file to large: 4-1M limit", $filesize =  0, goto Cleanup;
$filesize > 0 or
$errorMsg = "empty file", $filesize = 0, goto Cleanup;
Cleanup: {
close FDFH if defined FDFH;
close FLFH if defined FLFH;
return $filesize;
}
} # ftpRead

sub ftpError($$$)
{
# This subroutine matches an ftp response against an status code.  The code can
# be specified as a regexp.  So, 25[0-9] as the status code will let us match
# any of the 25X status codes.
# It returns 1 on error. This subroutine used to do  cleanup, but
# I'm leaving this job to the main ftp subroutine.
my ($input, $statcode, $errmsg) = @_;
$errorMsg = $errmsg, return 1 if($input !~ /^\s*$statcode/);
return 0;
} # ftpError

sub pasvOpen()
{
my ($line, $packed_ftpaddr, $ipaddr, $port);
serverPutLine "pasv";
serverGetLine or return 0;
return 0 if ftpError($serverLine, '227', "server doesn't support passive mode: received \"$serverLine\"");
if($serverLine =~ /([0-9]+,[0-9]+,[0-9]+,[0-9]+,[0-9]+,[0-9]+)/) {
$packed_ftpaddr = pack("C6", split(',', $1));
} else {
$errorMsg = "cannot make ftp data connection: server sent \"$serverLine\"";
return 0;
}
$ipaddr = substr($packed_ftpaddr, 0, 4);
$port = unpack("n", substr($packed_ftpaddr, 4, 2));
# The address for ftp data connections is written this way:
# 127,0,0,1,100,100
# We turn those decimal notations into a packed string of unsigned chars..
# The first four characters are the IP address in network byte order.  They
# are fed directly to sockaddr_in.  The last two are unpacked as an
# unsigned short in NBO.  The new decimal representation is fed to sockaddr_in.
my $saddr = sockaddr_in($port, $ipaddr);
socket(FDFH, PF_INET, SOCK_STREAM, getprotobyname('tcp')) or
$errorMsg = "cannot allocate a socket", return 0;
connect(FDFH, $saddr) or
$errorMsg = "cannot open ftp data connection", return 0;
shutdown(FDFH, 1); # Hmm.  My server hangs if this doesn't happen...
return 1;
} # pasvOpen

sub ftpListen {
my $ctladdr = (sockaddr_in(getsockname(SERVER_FH)))[1];
$errorMsg = "unable to obtain address of control connection; cannot initiate data connection",
return 0 if !$ctladdr;
my $port = int(rand(64510) + 1025);
socket(FLFH, PF_INET, SOCK_STREAM, getprotobyname('tcp')) or
$errorMsg = "unable to allocate a socket", return 0;
my $saddr = sockaddr_in($port, $ctladdr);
bind(FLFH, $saddr) or
$errorMsg = "unable to bind socket: port $port $!", return 0;
listen(FLFH, 1) or
$errorMsg = "unable to listen on ftp data socket", return 0;
serverPutLine sprintf("port %d,%d,%d,%d,%d,%d", unpack('C4', $ctladdr), $port >> 8, $port & 255);
serverGetLine or return 0;
return 0 if ftpError($serverLine, '200', "ftp server does not support port command, received \"$serverLine\"");
shutdown(FLFH, 1);
return 1;
} # ftpListen



# Cookie support
sub setCookies($$)
{
# We only support Netscape-style cookies presently.  The newer style will
# be supported eventually.  It offers some functionality that Netscape's
# doesn't.
my $cookie = shift;
print "incoming cookie: $cookie\n" if $debug >= 4;
$cookie =~ s/^Set-Cookie:\s+//i;
stripWhite \$cookie;
return unless length $cookie;
my $url_desc = shift;
my @cook_array = split(';', $cookie);
# We should have the cookie into its component parts.
my($name, $value, $path, $domain, $expires, $secure);
($name, $value) = split('=', shift(@cook_array), 2);
$value = "" unless defined $value;
my $crumb;
while($crumb = shift(@cook_array)) {
stripWhite \$crumb;
$crumb = "secure=" if $crumb =~ /^secure$/i;
if($crumb =~ s/^domain=//i) {
# Do some work on $crumb to protect us from general maliciousness/idiocy.
my $workingserver = $$url_desc{SERVER};
next unless $$url_desc{SERVER} =~ /\Q$crumb\E$/i;
my $l = length $crumb;
next if length($workingserver) > $l and substr($crumb, 0, 1) ne '.' and substr($workingserver, -$l-1, 1) ne '.';
# We simply won't use a bogus domain attribute.  We ignore it, and the domain
# eventually is set to the default.
#  In other words, we don't want somebody sending us a cookie for all of .com.
my $numfields = $crumb =~ y/././;
++$numfields unless substr($crumb, 0, 1) eq '.';
if($crumb =~ /\.(net|com|gov|edu|mil|org|int|tv|bus)$/i) {
# One nasty regexp, oh well.  Domain attributes from these domains may
# have a minimum of two fields.
next if $numfields < 2;
} else {
# Everyone else needs three fields.
next if $numfields < 3;
}
$domain = $crumb;
} elsif($crumb =~ s/^path=//i) {
$path = $crumb;
} elsif($crumb =~ s/^expires=?\s*//i) {
# Squeeze a time_t out of the date string, hopefully!  If not, then "-1"
# is used as the date, so the cookie will expire on quit.
$expires = cookieDate($crumb);
} elsif($crumb =~ s/^max-age=?\s*//i) {
if($crumb =~ /^\d+$/ and not defined $expires) {
$expires = time() + $crumb;
}
} elsif($crumb =~ s/^secure=//i) {
# SSL-only cookie.
$secure = 1;
} else {
print STDERR "Error processing cookie with element $crumb\n"; # debugging statement
}
}
$domain = $$url_desc{SERVER} if !defined $domain;
#  Here's what it should be, according to the standard.
#  $path = $$url_desc{PATH} if !defined $path;
#  Here's what some sites require, such as http://tdzk.net
#  This is apparently what Explorer does.
#  Oh well, who the hell needs standards;
#  when you're a monopoly you set the standards.
$path = "/" if !defined $path;
$expires = -1 if !defined $expires;
$secure = 0 if !defined $secure; # For secure cookies, it will have been set to 1
# Put the cookie into the master cookie jar.
print "into jar: $domain $path $expires $name $value\n" if $debug >= 4;
$cookies{$domain}{$path}{$name} =
{value => $value, expires => $expires, secure => $secure};
# If a server sends two cookies of the same path and name, with different values,
# the former will be quashed by the latter.  This is proper behavior.
if($expires != -1) { # Persistent cookie.
my $chmodFlag = 0;
$chmodFlag = 1 unless -f $ebcooks;
# Now, append to the cookie file.
# I learned the format for Netscape's cookie file from lynx's source.  Thank you, lynx team.
if(!open(COOKFILE, ">>$ebcooks")) {
warn "unable to open cookie jar for append: $!";
} else {
chmod 0600, $ebcooks if $chmodFlag;
print COOKFILE join("\t", $domain, 'FALSE', $path,
$secure ? 'TRUE' : 'FALSE', $expires, $name, $value) . "\n";
# A note.  Lynx defines a field, "what".  I don't know what its used
# for.  But all the Netscape cookie files I've seen have it set to "FALSE".
# so will we.
# Maybe its proprietary to Netscape's browser.
close COOKFILE;
}
}
}  # setCookies

sub fetchCookies($)
{
my $url_desc = shift;
my $cur_scheme = $$url_desc{SCHEME};
my $cur_domain = $$url_desc{SERVER};
my $cur_path = $$url_desc{PATH};
my ($domainm, $pathm, $cookiem); # The 'm' at the end stands for 'match'
my @sendable = (); # Sendable cookie strings.
foreach $domainm (keys(%cookies)) {
next unless $cur_domain =~ /\Q$domainm\E$/i;
my $l = length $domainm;
next if length($cur_domain) > $l and substr($domainm, 0, 1) ne '.' and substr($cur_domain, -$l-1, 1) ne '.';
foreach $pathm (keys(%{$cookies{$domainm}})) {
next unless $cur_path =~ /^\Q$pathm\E/;
foreach $cookiem (keys(%{$cookies{$domainm}{$pathm}})) {
my $deref = $cookies{$domainm}{$pathm}{$cookiem};
# $deref is a simple hash reference, containing the description of one cookie.
# We can do the rest of our matching painlessly, without dereferencing
# the whole nasty data structure every time.
next if $$deref{secure} and  ($cur_scheme !~ /https/);
my $j = join('=', $cookiem, $$deref{value});
$j =~ s/=$//;
push @sendable, $j;
print "outgoing cookie: $domainm $pathm $j\n" if $debug >= 4;
}
}
}
return "" if $#sendable < 0;  # no cookies
my $outgoing = 'Cookie: ' . join("; ", @sendable);
#  Lynx prepends a cookie2: directive.
#  I don't know what it means or what it's for.  Here it is.
return "Cookie2: \$Version=1$eol$outgoing$eol";
}  # fetchCookies

sub cookieDate($)
{
# This might become a general http date decoder, if we ever find
# places where dates are useful.
my $datestring = shift;
stripWhite \$datestring;
if($datestring =~ /^[a-z]{3,9},\s+(\d\d)[- ]([a-z]{3})[- ](\d\d(?:\d\d)?)\s+(\d\d):(\d\d):(\d\d)\s+GMT/i) {
my ($day, $mon, $year, $hour, $min, $sec) = ($1, $2, $3, $4, $5, $6);
if(($year < 100) and ($year > 0)) {
# two digit.
if($year >= 70) {
$year += 1900;
} else { $year += 2000; }
}
$mon = $monhash{lc($mon)} - 1;
#  We should probably range-check all the fields,
#  but year is definitely necessary.
$year = 2035 if $year > 2035;
$year = 1970 if $year < 1970;
my $time = timegm($sec, $min, $hour, $day, $mon, $year);
return $time;
} else {
return -1; 
}
}  # cookyDate

sub fillJar()
{
# Initialize the cookie jar.
my $writeFlag = 0; # Write revised cookie file?
open(COOKFILE, "+<$ebcooks") or return;
my $inline;
my $nowtime = time;
while($inline = <COOKFILE>) {
chomp $inline;
my ($domain, $what, $path, $secure, $expires, $name, $value) = split("\t", $inline);
$writeFlag = 1 if exists $cookies{$domain}{$path}{$name};
if($expires > $nowtime) {
$cookies{$domain}{$path}{$name} =
{value => $value, secure => $secure eq "TRUE" ? 1 : 0, expires => $expires}
} else {
$writeFlag = 1;
} # cookies expired.
}  # loop reading
if($writeFlag) {
seek COOKFILE, 0, 0;
truncate COOKFILE, 0;
my ($odomain, $opath, $ocook); # o for out
foreach $odomain (keys(%cookies)) {
foreach $opath (keys(%{$cookies{$odomain}})) {
foreach $ocook (keys(%{$cookies{$odomain}{$opath}})) {
my %deref = %{$cookies{$odomain}{$opath}{$ocook}};
print COOKFILE join("\t", $odomain, 'FALSE', $opath,
$deref{secure} ? "TRUE" : "FALSE", $deref{expires}, $ocook,
$deref{value}), "\n" if $deref{expires} > $nowtime;
}
}
}
}  # rewrite file
close COOKFILE;
}  # fillJar

#*********************************************************************
#  Web Express features.  For more on Web Express, visit
#  http://www.webexpresstech.com/WebXP/WebExpressTutorial.html
#*********************************************************************

sub webExpress($)
{
my $line = shift;
stripWhite \$line;
$line =~ s/\s+/ /g;
my $code = $line;
$code =~ s/ .*//;
$line =~ s/.*? //;
defined $shortcut{$code} or
$errorMsg = "shortcut $code is not recognized", return 0;
my $newurl = $shortcut{$code}{url};

#  Step through $line and extract options, indicated by -
#  This isn't implemented yet.

#  Done with options, what remains is the search argument.
my $arg = urlEncode $line;
length $arg or
$errorMsg = "shortcut is given no search argument", return 0;

#  Put the argument into the url.
$newurl =~ s/\$1/$arg/;

return 1, $newurl, $shortcut{$code}{after};
}  # webExpress


#  return "x" if an error is encountered
sub parseWWWAuth($$)
{
my ($authline, $url_desc) = @_;
my ($qop_auth, $qop_authint) = (0, 1); # this would be an enum in C
my ($username, $pass);

#  parse the authorization request line
my @challenges = ();
my ($attribname, $value);
stripWhite(\$authline);
$authline =~ s/^WWW-Authenticate:\s*//i;
while($authline =~ s/^\s*([^\s]+)\s+//) {
my %challenge = (authscheme => $1);
while($authline =~ s/^([^=]+)=//) {
$attribname = lc($1);
if($authline =~ s/^"//) {
# value of attribute is a quoted string.
$authline =~ s/^([^"]+)"((,\s*)|$)//;
$value = $1;
} else {
$authline =~ s/^([^,]+)((,\s*)|$)//;
$value = $1;
}
$challenge{$attribname} = $value;
}
if($challenge{authscheme} =~ /^digest/i && defined($challenge{qop})) {
my ($q, $newq) = undef;
my @qop = split(/\s*,\s*/, $challenge{qop});
foreach $q (@qop) {
$newq = $qop_authint, last if $q =~ /^auth-int$/i;
}
if(!defined($newq)) {
foreach $q (@qop) {
$newq = $qop_auth, last if $q =~ /^auth$/i;
}
}
$errorMsg = "Server sent a bad qop value in digest authentication", return "x" unless defined $newq;
$challenge{qop} = $newq;
}
push(@challenges, {%challenge});
}
my ($c, $used_challenge) = undef;
# Server may have sent multiple challenges with multiple auth schemes.
# Spec says that we use the strongest scheme supported by the server.
foreach $c (@challenges) {
$used_challenge = $c, last if $$c{authscheme} =~ /^Digest$/i;
}
if(!defined($used_challenge)) {
foreach $c (@challenges) {
$used_challenge = $c if($$c{authscheme} =~ /Basic/);
}
}
$errorMsg = "no usable challenges were found", return "x" unless defined $used_challenge;
if($$used_challenge{authscheme} =~ /Basic/i) {
($username, $pass) = getUserPass($$url_desc{SERVER} . "\x01" . $$url_desc{PORT} . "\x01" . $$used_challenge{realm});
return "x" if $username eq "x";
my $do64x = "$username:$pass";
my $do64y = "";
encodeBase64(\$do64x, "", \$do64y);
return "Authorization: Basic $do64y$eol";
}
else { # Not Basic, must be Digest.
unless(eval { require Digest::MD5 }) {
$errorMsg = "You need to download the Digest::MD5 module from CPAN to do digest authentication.", return "x";
}
$errorMsg = "Unsupported algorithm for digest authentication", return "x" if(defined($$used_challenge{algorithm}) && $$used_challenge{algorithm} !~ /^md5$/i);
$errorMsg = "unable to perform digest authentication", return "x" if(!defined($$used_challenge{realm})
|| !defined($$used_challenge{nonce}));
($username, $pass) = getUserPass($$url_desc{SERVER} . "\x01" . $$url_desc{PORT} . "\x01" . $$used_challenge{realm});
return "x" if $username eq "x";
srand(time());
my $nc = "00000001";
my $cnonce = sprintf("%08x%08x", int(rand(0xffffffff)), int(rand(0xffffffff)));
# pseudorandoms are fine here.  The cnonce is used to thwart chosen plaintext
# attacks when checking integrity of message content.  Probably not much
# of a threat for MD5.  Maybe it will be someday, and when it is, I'll
# dream up a better way to create a random cnonce.
my ($a1, $a2);
$a1 = "$username:$$used_challenge{realm}:$pass";
if($$used_challenge{qop} == $qop_authint) {
$a2 = $$url_desc{method} . ':' . $$url_desc{PATH} . Digest::MD5::md5_hex($$url_desc{content});
} else {
$a2 = $$url_desc{method} . ':' . $$url_desc{PATH};
}
my $response;
if(defined($$used_challenge{qop})) {
$response = Digest::MD5::md5_hex(Digest::MD5::md5_hex($a1) . ':' . $$used_challenge{nonce} . ':' .
 $nc . ':' . $cnonce . ':' .
($$used_challenge{qop} == $qop_auth ? "auth" : "auth-int") . ':' . Digest::MD5::md5_hex($a2)) ;
} else {
$response = Digest::MD5::md5_hex(Digest::MD5::md5_hex($a1) . ':' . $$used_challenge{nonce} . ':' . Digest::MD5::md5_hex($a2)) ;
}
my $out = "Authorization: Digest username=\"$username\", realm=\"$$used_challenge{realm}\", " .
"nonce=\"$$used_challenge{nonce}\", uri=\"$$url_desc{PATH}\", response=\"$response\"";
$out .= ", opaque=\"$$used_challenge{opaque}\"" if defined($$used_challenge{opaque});
$out .= ", algorithm=\"$$used_challenge{algorithm}\"" if defined($$used_challenge{algorithm});
if(defined($$used_challenge{qop})) {
$out .= ", qop=";
$out .= "\"auth\"" if $$used_challenge{qop} == $qop_auth;
$out .= "\"auth-int\"" if $$used_challenge{qop} == $qop_authint;
$out .= ", nc=$nc, cnonce=\"$cnonce\"";
}
$out .= "$eol";
return $out;
}
}  # parseWWWAuth

sub getUserPass($)
{
my $request = shift;
my $abort = "login password sequence aborted";
if(! $authAttempt and defined $authHist{$request}) {
return split ":", $authHist{$request};
}
my ($server, $port, $realm) = split(":", $request);
print "Server $server requests authentication for $realm.  (type x to abort)\n";
print "Username: ";
my $username = <STDIN>;
chomp $username;
$errorMsg = $abort, return ("x","x") if $username eq "x";
print "Password: ";
my $pass = <STDIN>;
chomp $pass;
$errorMsg = $abort, return ("x","x") if $pass eq "x";
$authHist{$request} = "$username:$pass";
return ($username, $pass);
}  # getUserPass


