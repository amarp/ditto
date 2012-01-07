#!/usr/bin/perl

use ronscripts;
use Getopt::Std;
use strict;
use Env;

if( $#ARGV < 1) { die "missing <hostname> <procname>\n"; }

#&set_user($EXPT_USER);

my $host = $ARGV[0];
my $proc = $ARGV[1];
my $cmd = "ps -C $proc";

my $rc = &run_on_kid($host, $cmd);
my $line =  &kid_output();
#print "$proc -> $line";
if ($line =~ $proc) {
    print "GOOD HOST NAME=\"$host\"\n";
}
else {
    print "BAD HOST NAME=\"$host\"\n";
}
exit($rc) if $rc;

exit(0);
