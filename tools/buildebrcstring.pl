#!/usr/bin/perl -w
# Wrapper around buildsourcestring.pl, for the ebrc files in various languages.
#  Also the quick reference guide from the manual.

use strict;
use warnings;

my $s = join ' ', glob "lang/ebrc-*";
$s =~ s/ebrc-\w+/$& $&/g;
# Some inconsistency has evolved here,
# file name is hyphen, but C string has to be an underscore.
$s =~ s/ ebrc-/ ebrc_/g;

#  Extract the quick reference guide.
my $in_qrg = 0;
open F1, "doc/usersguide.html";
open F2, ">src/qrg";
print F2 "<P>\n";
while(<F1>) {
$in_qrg = 1 if /\(toggle\),/;
next if ! $in_qrg;
$in_qrg = 0 if /H3/;
next if ! $in_qrg;
chomp;
#  in case \r is not removed on windows
s/\r$//;
print F2 "$_\n";
}
close F1;
close F2;

$s = "src/qrg qrg $s";

# Ready to go.
system "perl -w tools/buildsourcestring.pl $s src/ebrc.c";

unlink "src/qrg";

exit 0;
