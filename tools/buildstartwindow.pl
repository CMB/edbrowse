#!/usr/bin/perl -w
#  turn startwindow.js into startwindow.c
# Windows has a limit of 16380 single-byte characters.
use strict;
use warnings;

my $infile = "src/startwindow.js";
my $outfile = "src/startwindow.c";

my $max_chars = 16380;

sub prt($) { print shift; }

if ( -f $infile ) {
    if (!open  INF, "<$infile") {
        prt("Error: Unable to open $infile file!\n");
        exit(1);
    }
    my @lines = <INF>;
    close INF;
    if (! open OUTF, ">$outfile") {
        prt("Error: Unable to create $outfile file!\n");
        exit(1);
    }
    print OUTF "/* startwindow.c: this file is machine generated; */\n";
    print OUTF "/* please edit startwindow.js instead. */\n";
    print OUTF "\n";
    print OUTF "const char startWindowJS[] = \"\\\n";
    my ($line,$len,$total);
    $total = 0;
    foreach $line (@lines) {
        chomp $line;
        $line =~ s/\\/\\\\/g;
        $line =~ s/"/\\"/g;
        $len = length($line) + 4;
        if (($total + $len) > $max_chars) {
            print OUTF "\"\n\"";
            $total = 0;
        }
        print OUTF "$line\\n\\\n";
        $total += $len + 4;
    }
    print OUTF "\";\n";
    print OUTF "\n";
    close OUTF;
    prt("Content $infile written to $outfile\n");
    exit(0);    
} else {
    prt("Error: Unable to locate $infile file!\n");
    exit(1);
}

# eof

