#!/usr/bin/perl -w
# Build message strings for the languages that are supported.
# This is much simpler than buildsourcestring.pl.
# No interpretation or escaping, each line ia assumed to be a C string,
# put quotes around it and that's it!
# That means none of these lines can contain quotes, unless of course escaped,
# as in \"hello\"

use strict;
use warnings;

sub prt($) { print shift; }

my @files = glob "lang/msg-*";
my $infile;
my $outfile = "src/msg-strings.c";
my $outbase = $outfile;
$outbase =~ s,.*/,,;

    if (! open OUTF, ">$outfile") {
        prt("Error: Unable to create $outfile!\n");
        exit(1);
    }

    print OUTF "/* $outbase: this file is machine generated; */\n\n";

#  loop over input files
foreach $infile (@files) {
my $inbase = $infile;
$inbase =~ s,.*/,,;
my $stringname = $inbase;
$stringname =~ s/-/_/;

    if (!open  INF, "<$infile") {
        prt("Error: Unable to open $infile!\n");
        exit(1);
    }
    my @lines = <INF>;
my $line;
    close INF;
    print OUTF "/* source file $inbase */\n";
    print OUTF "const char *$stringname" . "[] = {\n";
    foreach $line (@lines) {
chomp $line;
#  in case \r is not removed on windows
$line =~ s/\r*$//;
if($line =~ /^[0,\s]*$/) {
print OUTF "\t0,\n";
} else {
print OUTF "\t\"$line\",\n";
}
}
print OUTF "};\n\n";
}

exit 0;
