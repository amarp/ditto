#!/usr/bin/perl -W

use strict 'vars';

my @chunks = ("8K", "16K", "32K");
my @combs = ("dot-ohear", "proxy-mhear");

if ($#ARGV < 1) { die "perl control-0.pl <recvr list (PL style)> <path to results>"; }

open(FILE,"$ARGV[0]") || die "could not open $ARGV[0] for reading" ;
my $sites = "";
while (my $line = <FILE>) {
    $sites .= $line;
}
close(FILE);
my @allhosts = ($sites =~ m|HOST NAME=\"(.*?)\"|sig);

my %op = ();

foreach my $h (@allhosts) {
    
    my $dir = "$ARGV[1]/$h/";
    
    my $key;
    if ($h =~ /(.*?)(\d+)/) {
	$key = $2;
	print "$h --> $key\n";
    }
    else {
	print "No key\n";
    }
    
    foreach my $c (@combs) {
	foreach my $s (@chunks) {
	    
	    my $now = "$dir/$s/$c/xput.dat";
	    open(FILE, "$now") || die ("Unable to open file $now");
	    # read file into an array
	    my $line = <FILE>;
	    chomp $line;
	    my @data = split(/ +/, $line);
	    
	    print "Parsing $now\n";
	    
	    #median
	    my $num = sprintf("%.2f", $data[$#data]);
	    push(@{$op{$key}}, $num);
	    #max
	    $num = sprintf("%.2f", $data[$#data-2]);
	    push(@{$op{$key}}, $num);
	    #min
	    $num = sprintf("%.2f", $data[$#data-3]);
	    push(@{$op{$key}}, $num);
	    close(FILE);

	    
	}
    }
    
    print "@{$op{$key}}\n";
}

open(OP, ">$ARGV[1]/xput.dat") || die ("Unable to open file $ARGV[1]/xput.dat");
foreach my $k (sort {$a<=>$b} keys %op) {
    print OP "$k @{$op{$k}}\n";
}
close(OP);

foreach my $c (@combs) {
    my @prev = ();
    
    foreach my $s (@chunks) {
	
	open(OP, ">$ARGV[1]/$c-$s") || die ("Unable to open file $c-$s");
	open(OP1, ">$ARGV[1]/$c-$s-diff") || die ("Unable to open file $c-$s");
	my $track = 0;

	foreach my $h (@allhosts) {
	    my $dir = "$ARGV[1]/$h/";
	    my $now = "$dir/$s/$c/xput.dat";
	    open(FILE, "$now") || die ("Unable to open file $now");
	    # read file into an array
	    my $line = <FILE>;
	    chomp $line;
	    my @data = split(/ +/, $line);
	    close(FILE);
	    
	    print "Parsing $now\n";	    
	    for (my $i = $#data-4; $i >= 0; $i--) {
		my $num = sprintf("%.2f", $data[$i]);
		print OP "$num\n";
	    }

	    if ($s eq $chunks[0]) {
		#median
		my $num = sprintf("%.2f", $data[$#data]);
		push(@prev, $num);
		#max
		$num = sprintf("%.2f", $data[$#data-2]);
		push(@prev, $num);
		#min
		$num = sprintf("%.2f", $data[$#data-3]);
		push(@prev, $num);
	    }
	    else {
		#median
		my $num = sprintf("%.2f", $data[$#data]);
		if ($num > 0 && $prev[$track] > 0) {
		    print "Computing $num $prev[$track]\n";
		    my $diff = (100*($num-$prev[$track]))/$prev[$track];
		    print OP1 "$diff\n";
		}
		$track++;
		#max
		$num = sprintf("%.2f", $data[$#data-2]);
		if ($num > 0 && $prev[$track] > 0) {
		    print "Computing $num $prev[$track]\n";
		    my $diff = (100*($num-$prev[$track]))/$prev[$track];
		    print OP1 "$diff\n";
		}
		$track++;
		#min
		$num = sprintf("%.2f", $data[$#data-3]);
		if ($num > 0 && $prev[$track] > 0) {
		     print "Computing $num $prev[$track]\n";
		    my $diff = (100*($num-$prev[$track]))/$prev[$track];
		    print OP1 "$diff\n";
		}
		$track++;
	    }
	}
	close(OP);
	close(OP1);
    }
}

#####################
my @line_styles = ();
push(@line_styles, "set style line 10 lt 1 lw 4 pt 8");
push(@line_styles, "set style line 11 lt 2 lw 4 pt 2");
push(@line_styles, "set style line 12 lt 3 lw 4 pt 6");
push(@line_styles, "set style line 13 lt 4 lw 4 pt 1");
push(@line_styles, "set style line 14 lt 5 lw 4 pt 1");

sub GnuFile {
    
    my ($name, $title, $xscale, $yscale, $xlabel, $ylabel, $middle_part, $log, $size, $keypos) = @_;

    my $eps;
    my $dir;
    if ($name =~ /^(.*)\/(.*)$/)
    {
        $dir = $1; $eps = $2;
        print "$dir $eps\n";
    }

    open (GP, ">$name.gnu") || die "could not open $name.gnu for reading\n";
    
    if ($title ne "0") {
        print GP "set title '$title'\n";
    }
    print GP "set grid\nset terminal table\nset yrange $yscale\nset xrange $xscale\n\n";
    if ($log == 1) {
        print GP "set logscale x\n";
    }
    print GP "set size $size\nset ylabel '$ylabel'\nset xlabel '$xlabel'\n\n";
    print GP "set pointsize 2\nset key $keypos\n\n";

    foreach my $l (@line_styles) {
        print GP "$l\n";
    }
    print GP "\n";

    print GP "$middle_part";

    
    print GP "replot\n\n";
    print GP "set terminal postscript eps noenhanced color solid defaultplex \"Helvetica\" 18\n";
    #print GP "set terminal postscript \"Helvetica\" 14\n";
    print GP "set output '$eps.eps'\n";
    print GP "replot\n";
    
    close(GP);
    
    my $cmd = "cd $dir; gnuplot $eps.gnu > /dev/null; epstopdf $eps.eps ; cd -\n";
    print $cmd;
    system($cmd);
}

my $xscale = "[1:40]";
my $xlabel = "Node number";
my $yscale = "[0:5000]";
my $ylabel = "Throughput (Kbps)";
my $size = "2*4/5., 1.4*3/3.";
my $keypos = "right top";

#assuming harcoded chunk sizes and combinations
my $middle_part = "plot 'xput.dat' using 1:2:3:4 t '8K' with yerrorbars linestyle 10\n";
$middle_part = "$middle_part replot 'xput.dat' using 1:5:6:7 t '16K' with yerrorbars linestyle 11\n";
$middle_part = "$middle_part replot 'xput.dat' using 1:8:9:10 t '32K' with yerrorbars linestyle 12\n";
GnuFile("$ARGV[1]/xput-dot-ohear", "dot-ohear", $xscale, $yscale, $xlabel, $ylabel, $middle_part, 0, $size, $keypos);

$middle_part = "plot 'xput.dat' using 1:11:12:13 t '8K' with yerrorbars linestyle 10\n";
$middle_part = "$middle_part replot 'xput.dat' using 1:14:15:16 t '16K' with yerrorbars linestyle 11\n";
$middle_part = "$middle_part replot 'xput.dat' using 1:17:18:19 t '32K' with yerrorbars linestyle 12\n";
GnuFile("$ARGV[1]/xput-proxy-mhear", "proxy-mhear", $xscale, $yscale, $xlabel, $ylabel, $middle_part, 0, $size, $keypos);


$xscale = "[0:5000]";
$xlabel = "Throughput (Kbps)";	
$yscale = "[0:100]";
$ylabel = "% of runs";
$keypos = "right bottom";

foreach my $c (@combs) {
    $middle_part = "";
    my $count = 10;
    foreach my $s (@chunks) {
	
	my $cmd = "./cdfgen $ARGV[1]/$c-$s 100 0 $ARGV[1]/$c-$s-cdf > /dev/null\n";
	print $cmd;
	system($cmd);

	if ($s eq $chunks[0]) {
	    $middle_part = "plot '$c-$s-cdf' using 1:2 t '$s' with lines linestyle $count\n";
	}
	else {
	    $middle_part = "$middle_part replot '$c-$s-cdf' using 1:2 t '$s' with lines linestyle $count\n";
	}
	$count++;
    }
    GnuFile("$ARGV[1]/$c", "$c", $xscale, $yscale, $xlabel, $ylabel, $middle_part, 0, $size, $keypos);
}

$xscale = "[-50:200]";
$xlabel = "% change in throughput";	
$yscale = "[0:100]";
$ylabel = "% of runs";
$keypos = "right bottom";

foreach my $c (@combs) {
    $middle_part = "";
    my $count = 10;
    foreach my $s (@chunks) {
	
	next if ($s eq $chunks[0]);
	    
	my $cmd = "cat $ARGV[1]/$c-$s-diff | ./make-cdf > $ARGV[1]/$c-$s-diff-cdf \n";
	print $cmd;
	system($cmd);

	if ($count == 10) {
	    $middle_part = "plot '$c-$s-diff-cdf' using 1:(\$2*100) t '$s' with lines linestyle $count\n";
	}
	else {
	    $middle_part = "$middle_part replot '$c-$s-diff-cdf' using 1:(\$2*100) t '$s' with lines linestyle $count\n";
	}
	$count++;
    }
    GnuFile("$ARGV[1]/$c-diff", "$c", $xscale, $yscale, $xlabel, $ylabel, $middle_part, 0, $size, $keypos);
}

