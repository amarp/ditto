#!/usr/bin/perl

use ronscripts;
use Getopt::Std;
use strict;
use Env;

if( $#ARGV < 2 ) { die "perl get-stuff.pl <hostname> <remote file with complete path> <localpath>"; }

my $host = $ARGV[0];

my $rc = &rsync_from_kid($host, "$ARGV[2]/$ARGV[0]-$ARGV[1]", "$WORK_PATH/$ARGV[1]");
print &kid_output();
exit($rc) if $rc;

exit(0);
