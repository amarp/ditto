#!/usr/bin/perl -W

use strict 'vars';
my $NUM_AVGS = 3;

if ($#ARGV < 1) { die "perl control-0.pl <list of nodes> <path to results>"; }

open(FILE,"$ARGV[0]") || die "could not open $ARGV[0] for reading" ;
my $sites = "";
while (my $line = <FILE>) {
    $sites .= $line;
}
close(FILE);
my @allhosts = ($sites =~ m|HOST NAME=\"(.*?)\"|sig);

foreach my $h (@allhosts) {

    next if ($h eq $ARGV[2]);

    my $recvr_name;
    if ($ARGV[2] =~ /(.*?)\./) {
        $recvr_name = $1;
    }
    else {
        $recvr_name = $h;
    }

    my $cmd = "./analyze1.rb $ARGV[1]/expt-$recvr_name $ARGV[0] $NUM_AVGS";
    print "$cmd\n";
    system($cmd);
}
