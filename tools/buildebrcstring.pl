#!/usr/bin/perl -w
# Wrapper around buildsourcestring.pl, for the ebrc files in various languages.

use strict;
use warnings;

my $s = join ' ', glob "lang/ebrc-*";
$s =~ s/ebrc-\w+/$& $&/g;
# Some inconsistency has evolved here,
# file name is hyphen, but C string has to be an underscore.
$s =~ s/ ebrc-/ ebrc_/g;

# Ready to go.
system "perl -w tools/buildsourcestring.pl $s src/ebrc.c";

exit 0;
