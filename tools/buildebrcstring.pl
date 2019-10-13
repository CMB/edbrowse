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

#  Extract the quick reference guides.
my @guides = glob "doc/usersguide*.html";
for(my $j = 0; $j <= $#guides; ++$j) {
my $p1 = $guides[$j];
my $p2 = $p1;
$p2 =~ s/doc\/usersguide/qrg/;
$p2 =~ s/.html$//;
$p2 .= "_en" if $p2 eq "qrg";
my $in_qrg = 0;
open F1, "$p1";
open F2, ">src/$p2";
print F2 "<P>\n";
while(<F1>) {
$in_qrg = 1 if /qrg start/;
next if ! $in_qrg;
$in_qrg = 0 if /<[hH]3/;
next if ! $in_qrg;
chomp;
#  in case \r is not removed on windows
s/\r$//;
print F2 "$_\n";
}
close F1;
close F2;
$s = "src/$p2 $p2 $s";
}

# Ready to go.
system "perl -w tools/buildsourcestring.pl $s src/ebrc.c";

for(my $j = 0; $j <= $#guides; ++$j) {
my $p2 = $guides[$j];
$p2 =~ s/doc\/usersguide/qrg/;
$p2 =~ s/.html$//;
$p2 .= "_en" if $p2 eq "qrg";
unlink "src/$p2";
}

exit 0;
