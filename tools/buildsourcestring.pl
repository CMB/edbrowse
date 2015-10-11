#!/usr/bin/perl -w
#  Turn a file into a C string containing the contents of that file.
# Windows has a limit of 16380 single-byte characters.
use strict;
use warnings;

sub prt($) { print shift; }

my $max_chars = 16380;

my $nargs = $#ARGV;
if($nargs < 2) {
prt "Usage: buildsourcestring.pl file1 string1 file2 string2 ... outfile\n";
exit 1;
}

my $outfile = $ARGV[$nargs];
my $outbase = $outfile;
$outbase =~ s,.*/,,;

    if (! open OUTF, ">$outfile") {
        prt("Error: Unable to create $outfile file!\n");
        exit(1);
    }
    print OUTF "/* $outbase: this file is machine generated; */\n\n";

#  loop over input files
for(my $j = 0; $j < $nargs; $j += 2) {
my $infile = $ARGV[$j];
my $inbase = $infile;
$inbase =~ s,.*/,,;
my $stringname = $ARGV[$j+1];

if ( -f $infile ) {
    if (!open  INF, "<$infile") {
        prt("Error: Unable to open $infile file!\n");
        exit(1);
    }
    my @lines = <INF>;
    close INF;
    print OUTF "/* source file $inbase */\n";
    print OUTF "const char *$stringname = \"\\\n";
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
    prt("Content $infile written to $outfile\n");
} else {
    prt("Error: Unable to locate $infile file!\n");
    exit(1);
}
}

close OUTF;
exit(0);    

# eof

