#!/usr/bin/perl -w
#  Turn a file into a NUL-terminated char array containing the contents of that file.
# Windows has a limit of 16380 single-byte characters.

use strict;
use warnings;
use English;

my $strip_comments = 1; # set this to strip out comments and certain whitespace
$strip_comments = 0 if $ENV{"NOCOMPRESSSOURCE"};
my $dohex = 0;
$dohex = 1 if $ENV{"SOURCEASHEX"};

my $in_cmt = 0; # in a block comment
my $last_semi = 0;

sub prt($) { print shift; }

my $nargs = $#ARGV;
if($nargs < 2) {
prt "Usage: buildsourcestring.pl file1 file2 ... outfile\n";
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
for(my $j = 0; $j < $nargs; $j += 1) {
my $infile = $ARGV[$j];
my $inbase = $infile;
$inbase =~ s,.*/,,;
my $stringname = $inbase;
$stringname =~ s/-/_/g;

if ( -f $infile ) {
    if (!open  INF, "<$infile") {
        prt("Error: Unable to open $infile file!\n");
        exit(1);
    }
    my @lines = <INF>;
    close INF;
    if ($inbase =~ /\.js$/) {
        if ($lines[0] =~ /stringname=([a-zA-Z_][a-zA-Z_0-9]*)/) {
            $stringname = "$1";
            shift @lines;
        } else {
            prt("Unable to determine string name.\n");
            exit(1);
        }
    }
    print OUTF "/* source file $inbase */\n";
    print OUTF "const char ${stringname}[] = {\n";
    my ($line);
    foreach $line (@lines) {
        chomp $line;
#  in case \r is not removed on windows
        $line =~ s/\r*$//;

if($strip_comments) {
# Strip out js comments.
# These regular expressions are replicated in uncomment.
# If you change something here you must also change it there. Sorry.
# Comments, which are desperately needed, can be found over there.
# If unsure, set $strip_comments to 0 and run again.
if($in_cmt) {
if($line =~ s:.*?\*/::) {
$in_cmt = 0;
} else {
$line = ""; # retain line numbers
}
}
$line =~ s:(?<![\\"'])/\*.*?\*/(?!["'])::g;
$line =~ s/^[\t ]*//;
$line =~ s:^//.*::;
$line =~ s:([;{}]) *//.*:$1:;
if($line =~ s:(?<![\\"'])/\*.*::) {
$in_cmt = 1;
}
$line =~ s/ *$//;
$line =~ s/ *([(){}\[\]]) */$1/g;
$line =~ s/ +([=<>+\-|&]+) +/$1/g;
$line =~ s/([,;:]) (\w)/$1$2/g;
$line =~ s/^([(\[])/;$1/ if $last_semi;
$last_semi = 0 if length $line;
$last_semi = 1 if $line =~ s/;$//;
$line =~ s/; *}/}/g;
}

if($dohex) {
# switch to hex bytes.
        $line =~ s/(.)/sprintf("0x%02x, ", ord($1))/ge;
		$line .= " 0x0a,";
} else {
$line =~ s/\\/\\\\/g;
        $line =~ s/([\001-\037])/sprintf("\\%03o", ord($1))/ge;
        $line =~ s/"/\\"/g;
$line = "\"" . $line . "\\n\"";
}
        print OUTF "$line\n";
    }
if($dohex) {
    print OUTF "0};\n";
} else {
    print OUTF "};\n";
}
    print OUTF "\n";
    prt("Content $infile written to $outfile\n");
} else {
    prt("Error: Unable to locate $infile file!\n");
    exit(1);
}
}

close OUTF;
exit(0);    
