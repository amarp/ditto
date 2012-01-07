#!/usr/bin/perl -W

use strict 'vars';

my @line_styles = ();
push(@line_styles, "set style line 10 lt 1 lw 4 pt 8");
push(@line_styles, "set style line 11 lt 2 lw 4 pt 4");
push(@line_styles, "set style line 12 lt 4 lw 4 pt 6");
push(@line_styles, "set style line 13 lt 7 lw 4 pt 9 ps 1.0");
push(@line_styles, "set style line 14 lt 7 lw 4 pt 5 ps 1.0");
push(@line_styles, "set style line 15 lt 7 lw 4 pt 7 ps 1.0");

my @chunks = ("8K", "16K", "32K");
my @combs = ("proxy-ohear", "proxy-mhear", "dot-ohear");
#my @combs = ("proxy-ohear-offline", "proxy-mhear-offline", "dot-ohear");

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


if ($#ARGV < 2) { die "perl control-0.pl <recvr list (PL style)> <path to results> <route file>"; }

open(FILE,"$ARGV[0]") || die "could not open $ARGV[0] for reading" ;
my $sites = "";
while (my $line = <FILE>) {
    $sites .= $line;
}
close(FILE);
my @allhosts = ($sites =~ m|HOST NAME=\"(.*?)\"|sig);

foreach my $h (@allhosts) {
    
    my $dir = "$ARGV[1]/$h/";

    my $xscale = "[1:40]";
    my $xlabel = "Node number";
    my $yscale = "[0:110]";
    my $ylabel = "% chunks reconstructed";
    my $size = "1*4/5., 0.7*3/3.";
    #my $size = "2.5*4/5., 2.0*3/3.";
    my $keypos = "right top";

    #assuming harcoded chunk sizes and combinations
    my $middle_part = "plot '8K/proxy-mhear-offline/chunk_percent_analysis.dat' using 1:8:5:6 t '8K-mhear' with yerrorbars linestyle 10\n";
    $middle_part = "$middle_part replot '16K/proxy-mhear-offline/chunk_percent_analysis.dat' using 1:8:5:6 t '16K-mhear' with yerrorbars linestyle 11\n";
    $middle_part = "$middle_part replot '32K/proxy-mhear-offline/chunk_percent_analysis.dat' using 1:8:5:6 t '32K-mhear' with yerrorbars linestyle 12\n";

    $middle_part = "$middle_part replot '8K/proxy-ohear-offline/chunk_percent_analysis.dat' using 1:8:5:6 t '8K-ohear' with yerrorbars linestyle 13\n";
    $middle_part = "$middle_part replot '16K/proxy-ohear-offline/chunk_percent_analysis.dat' using 1:8:5:6 t '16K-ohear' with yerrorbars linestyle 14\n";
    $middle_part = "$middle_part replot '32K/proxy-ohear-offline/chunk_percent_analysis.dat' using 1:8:5:6 t '32K-ohear' with yerrorbars linestyle 15\n";
    
    GnuFile("$ARGV[1]/$h/hear", "Hear", $xscale, $yscale, $xlabel, $ylabel, $middle_part, 0, $size, $keypos);

    $middle_part = "plot '8K/dot-ohear/chunk_percent_analysis.dat' using 1:8:5:6 t '8K-dot' with yerrorbars linestyle 10\n";
    $middle_part = "$middle_part replot '16K/dot-ohear/chunk_percent_analysis.dat' using 1:8:5:6 t '16K-dot' with yerrorbars linestyle 11\n";
    $middle_part = "$middle_part replot '32K/dot-ohear/chunk_percent_analysis.dat' using 1:8:5:6 t '32K-dot' with yerrorbars linestyle 12\n";

    #$middle_part = "plot '8K/proxy-ohear-offline/chunk_percent_analysis.dat' using 1:8:5:6 t '8K-ohear' with yerrorbars linestyle 13\n";
    #$middle_part = "$middle_part replot '16K/proxy-ohear-offline/chunk_percent_analysis.dat' using 1:8:5:6 t '16K-ohear' with yerrorbars linestyle 14\n";
    #$middle_part = "$middle_part replot '32K/proxy-ohear-offline/chunk_percent_analysis.dat' using 1:8:5:6 t '32K-ohear' with yerrorbars linestyle 15\n";
    GnuFile("$ARGV[1]/$h/ideal", "Ideal", $xscale, $yscale, $xlabel, $ylabel, $middle_part, 0, $size, $keypos);

}

my %route = ();
open(FILE, "$ARGV[2]") || die ("Unable to open file $ARGV[2]");
while (my $line = <FILE>) {
    chomp $line;
    my @r = split(/ +/, $line);
    for (my $i = 0; $i <= $#r; $i++){
	my $index;
	if ($r[$i] =~ /.*?(\d+)/) {
	    $index = $1;
	}
	else {
	    print "Bad node name $r[$i]\n";
	    die;
	}
	${$route{$r[$#r]}}{$index} = 1;
    }
}
close(FILE);

foreach my $c (@combs) {
    foreach my $s (@chunks) {
	open(OP, ">$ARGV[1]/$c-$s") || die ("Unable to open file $c-$s");
	foreach my $h (@allhosts) {
	    my $dir = "$ARGV[1]/$h/$s/$c/";
	    open(FILE, "$dir/chunk_percent_analysis.dat") || die ("Unable to open file $c-$s");
	    while (my $line = <FILE>) {
		chomp $line;
		my @data = split(/ +/, $line);

		#ignore on-path caching
		if (defined ${$route{$h}}{$data[0]}) {
		    print "Ignoring $h $data[0]\n";
		    next;
		}
		
		for (my $i = 1; $i <= $#data-4; $i++) {
		    print OP "$data[$i]\n";
		}
	    }
	    close(FILE);
	}
	close(OP);
    }
}

print "--------------------------------------------------\n";
print "--------------------------------------------------\n";

my $xscale = "[1:100]";
my $xlabel = "% chunks reconstructed";
my $yscale = "[0:100]";
my $ylabel = "% of samples";
my $size = "1*4/5., 0.7*3/3.";
#my $size = "2.5*4/5., 2.0*3/3.";
my $keypos = "right bottom";

foreach my $s (@chunks){
    my $middle_part = "";
    my $count = 10;
    my $c;
    foreach $c (@combs) {
	my $cmd = "./cdfgen $ARGV[1]/$c-$s 1 0 $ARGV[1]/$c-$s-cdf > /dev/null\n";
	print $cmd;
	system($cmd);
	
	if ($count == 10) {
	    $middle_part = "plot '$c-$s-cdf' using 1:2 t '$c' with lines linestyle $count\n";
	}
	else {
	    $middle_part = "$middle_part replot '$c-$s-cdf' using 1:2 t '$c' with lines linestyle $count\n";
	}
	$count++;
    }
    GnuFile("$ARGV[1]/$s", "$s", $xscale, $yscale, $xlabel, $ylabel, $middle_part, 0, $size, $keypos);
}

print "--------------------------------------------------\n";
print "--------------------------------------------------\n";

foreach my $c (@combs){
    my $middle_part = "";
    my $count = 10;
    my $s;
    foreach $s (@chunks) {
		
	if ($count == 10) {
	    $middle_part = "plot '$c-$s-cdf' using 1:2 t '$s' with lines linestyle $count\n";
	}
	else {
	    $middle_part = "$middle_part replot '$c-$s-cdf' using 1:2 t '$s' with lines linestyle $count\n";
	}
	$count++;
    }
    GnuFile("$ARGV[1]/$c", "$c", $xscale, $yscale, $xlabel, $ylabel, $middle_part, 0, $size, $keypos);
}

print "--------------------------------------------------\n";
print "--------------------------------------------------\n";
#computing diffs

foreach my $s (@chunks) {
    my @prev = ();

    foreach my $c (@combs) {
	open(OP, ">$ARGV[1]/$c-$s-diff") || die ("Unable to open file $c-$s-diff");
	my $track = 0;

	foreach my $h (@allhosts) {
	    my $dir = "$ARGV[1]/$h/$s/$c/";
	    open(FILE, "$dir/chunk_percent_analysis.dat") || die ("Unable to open file $c-$s");
	    while (my $line = <FILE>) {
		chomp $line;
		my @data = split(/ +/, $line);

		#ignore on-path caching
		if (defined ${$route{$h}}{$data[0]}) {
		    print "Ignoring $h $data[0]\n";
		    next;
		}
		
		if ($c eq $combs[0]) {
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
		    if ($num > 0 && $prev[$track] > 0 ||
			$num == 0 && $prev[$track] > 0) {
			my $diff = (100*($num-$prev[$track]))/$prev[$track];
			print "Computing$c-$s $num $prev[$track] $diff\n";
			print OP "$diff\n";
		    }
		    else {
			if ($prev[$track] == 0 && $num > 0) {
			   print OP "1000\n"; 
			}
			
		    }
		    $track++;
		    
		    #max
		    $num = sprintf("%.2f", $data[$#data-2]);
		    if ($num > 0 && $prev[$track] > 0 ||
			$num == 0 && $prev[$track] > 0) {
			my $diff = (100*($num-$prev[$track]))/$prev[$track];
			print "Computing$c-$s $num $prev[$track] $diff\n";
			print OP "$diff\n";
		    }
		    else {
			if ($prev[$track] == 0 && $num > 0) {
			   print OP "1000\n"; 
			}
			
		    }
		    $track++;
		    
		    #min
		    $num = sprintf("%.2f", $data[$#data-3]);
		    if ($num > 0 && $prev[$track] > 0 ||
			$num == 0 && $prev[$track] > 0) {
			my $diff = (100*($num-$prev[$track]))/$prev[$track];
			print "Computing$c-$s $num $prev[$track] $diff\n";
			print OP "$diff\n";
		    }
		    else {
			if ($prev[$track] == 0 && $num > 0) {
			   print OP "1000\n"; 
			}
			
		    }
		    $track++;
		}
	    }
	    close(FILE);
	}
	close(OP);
    }
}

$xscale = "[-50:200]";
$xlabel = "% change in chunks reconstructed";
$yscale = "[0:100]";
$ylabel = "% of samples";
$size = "1*4/5., 0.7*3/3.";
#my $size = "2.5*4/5., 2.0*3/3.";
$keypos = "right bottom";

foreach my $s (@chunks){
    my $middle_part = "";
    my $count = 10;
    my $c;
    foreach $c (@combs) {

	next if ($c eq $combs[0]);

	my $cmd = "cat $ARGV[1]/$c-$s-diff | ./make-cdf > $ARGV[1]/$c-$s-diff-cdf\n";
	print $cmd;
	system($cmd);
	
	if ($count == 10) {
	    $middle_part = "plot '$c-$s-diff-cdf' using 1:(\$2*100) t '$c' with lines linestyle $count\n";
	}
	else {
	    $middle_part = "$middle_part replot '$c-$s-diff-cdf' using 1:(\$2*100) t '$c' with lines linestyle $count\n";
	}
	$count++;
    }
    GnuFile("$ARGV[1]/$s-diff", "$s", $xscale, $yscale, $xlabel, $ylabel, $middle_part, 0, $size, $keypos);
}

print "--------------------------------------------------\n";
print "--------------------------------------------------\n";
