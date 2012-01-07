#!/usr/bin/perl -W

use strict 'vars';

my %data = ();

sub ComputeMedian {
    my ($arr_ref) = @_;
    
    my $count = $#{$arr_ref} + 1;
    my @sorted = sort {$a <=> $b} @{$arr_ref};
    
    print "SORT @sorted\n";

    my $middle;
    my $median;
    if ($count%2 != 0)
    {
        $middle = ($count-1)/2;
        $median = $sorted[$middle];
    }
    else
    {
        $middle = $count/2;
        $median = ($sorted[$middle] + $sorted[$middle-1])/2 ;
        #print "middile $middle\n";
        print "Averaging $sorted[$middle] $sorted[$middle-1]\n";
    }

    #print "Median $median\n";

    return($median);
}

if ($#ARGV < 1) { die "perl proximity.pl <sorted data file> <op file>"; }

open(FILE,"$ARGV[0]") || die "could not open $ARGV[0] for reading" ;
while (my $line = <FILE>) {
    chomp $line;
    my @allwords = split(/ +/, $line);
    
    print "$line $allwords[0] $allwords[1] $allwords[2]\n";

    push(@{$data{$allwords[2]}}, $allwords[$#allwords]);
}
close(FILE);

open(OP,">$ARGV[1]") || die "could not open $ARGV[1] for writing" ;
foreach my $k (keys %data) {
    
    my $num;
    if ($k =~ /map(\d+)/) {
	$num = $1;
    }
    else {
	print "Problem in key\n";
	die;
    }
    
    print "$k\n";
    my $med = ComputeMedian($data{$k});
    my @sorted = sort {$a <=> $b} @{$data{$k}};
    
    my $min = $sorted[0];
    my $max = $sorted[$#sorted];

    print "$k $num $med $min $max\n";
    print "------------------------------------------\n";

    print OP "$k \t $num \t $med \t $min \t $max\n";
}
close(OP);
