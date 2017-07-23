#!/usr/bin/perl -w
#  Turn a file into a NUL-terminated char array containing the contents of that file.
# Windows has a limit of 16380 single-byte characters.
use strict;
use warnings;
use English;

sub prt($) { print shift; }

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
    print OUTF "const char ${stringname}[] = {\n";
    my ($line);
    foreach $line (@lines) {
        chomp $line;
#  in case \r is not removed on windows
        $line =~ s/\r*$//;
        $line =~ s/(.)/sprintf("0x%02x, ", ord($1))/ge;
	if (($OSNAME eq "MSWin32") || ($OSNAME eq "MSWin64")) {
		$line .= " 0x0d, 0x0a,";
	} else {
		$line .= " 0x0a,";
	}
        print OUTF "$line\n";
    }
    print OUTF "0};\n";
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

