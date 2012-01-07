#!/usr/bin/perl

use ronscripts;
use Getopt::Std;
use strict;

if( $#ARGV < 2 ) { die "missing <host file> <proc name> <BT:1 else 0>\n"; }

my $plab_file = $ARGV[0];
my $proc = $ARGV[1];

my $cmd = "wc -l $plab_file";
print "$cmd\n";
open(FILE, "$cmd | ") || die "Cannot open $cmd";
my $line = <FILE>;
chomp $line;
my $plab_count;
if ($line =~ /^(\d+)\s+.*$/) {
    $plab_count = $1;
}
close(FILE);

print "Num plab nodes : $plab_count\n";

my $done = 0;
my $sleep_count = 0;

while ($done == 0) {
    
    if ($ARGV[2] == 0) {
	$cmd = "./do-perhost.pl -t 120 -c 200 -f $plab_file './check.pl \@HOST\@ $proc' >& temp.crap";
    }
    else {
	$cmd = "./do-perhost.pl -t 120 -c 200 -f $plab_file './check-bt.pl \@HOST\@' >& temp.crap";
    }
    print "$cmd\n";
    system($cmd);

    $cmd = "grep 'BAD HOST NAME' temp.crap | wc -l";
    print "$cmd\n";
    open(FILE, "$cmd | ") || die "Cannot open $cmd";
    my $temp_count = <FILE>;
    chomp $temp_count;
    close(FILE);
    
    my $perc = $temp_count/$plab_count;
    print "Num nodes now: $temp_count and $perc\n";
    if ($perc >= 0.9) {
	print "NOW: $temp_count THEN:$plab_count\n";
	$done = 1;
    }
    else {
	sleep(30);
	$sleep_count++;
	if ($sleep_count > 20) { #10mts
	    print "Slept for $sleep_count\n";
	    $done = 1;
	}
    }
}

