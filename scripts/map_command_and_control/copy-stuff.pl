#!/usr/bin/perl

use ronscripts;
use Getopt::Std;
use strict;
use Env;

if( $#ARGV < 1 ) { die "perl copy-stuff.pl <hostname> <src file with complete path> "; }

my $host = $ARGV[0];
my $rc = &rsync_to_kid($host, "$WORK_PATH", $ARGV[1]);
print &kid_output();
exit($rc) if $rc;

exit(0);
