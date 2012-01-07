#!/usr/bin/perl -W

use strict 'vars';

my $DOT_PATH = "/tmp/";
my $TESTBED = "map";
#my $TESTBED = "emulab";

#for tcsh
#my $SHELL = "tcsh";
#my $REDIRECT = ">&";

#for bash
my $SHELL = "bash";
#my $REDIRECT = "2>&1 | tee";
my $REDIRECT = "2>&1 ";


if ($#ARGV < 1) { die "perl offline.pl <dir containing result dirs> <absoulte path to exe file>"; }

my $cmd = "rm -rf $DOT_PATH/dot && cd $DOT_PATH/ && tar -zxf $ARGV[1] > /dev/null";
print "$cmd\n";
system($cmd);

my @thedirs = ();
opendir(IMD, $ARGV[0]) || die "Cannot open directory\n" ;
@thedirs = readdir(IMD);
closedir(IMD);

foreach my $d (@thedirs) {

    next if ($d !~ 'proxy-mhear');
    next if ($d =~ 'offhear');

    print "Extracting chunk size and recvr\n";
    my $chunk_size;
    my $recvr;
    if ($TESTBED eq "map") {
	if ($d =~ /expt-(map\d+)-(\d+K)-proxy-mhear-\d+/) {
	    $recvr = $1;
	    $chunk_size = $2;
	}
	else {
	    print "Problem in chunk size $d\n";
	    die;
	}
    }
    elsif ($TESTBED eq "emulab") {
	if ($d =~ /expt-(nodew\d+)-(\d+K)-proxy-mhear-\d+/) {
            $recvr = $1;
            $chunk_size = $2;
	    $recvr = "$recvr\.ditto.cmu849\.emulab\.net";
        }
        else {
            print "Problem in chunk size $d\n";
            die;
        }
    }

    print "Parsing $d $chunk_size $recvr\n";

    my $d1 = $d;
    $d1 =~ s/proxy-mhear/proxy-mhear-offline/;
    my $d2 = $d;
    $d2 =~ s/proxy-mhear/proxy-ohear-offline/;

    $cmd = "mkdir $ARGV[0]/$d1/ ; mkdir $ARGV[0]/$d1/gcp/ ; mkdir $ARGV[0]/$d1/dot ; mkdir $ARGV[0]/$d1/sniffer";
    print "$cmd\n";
    system($cmd);
    
    $cmd = "mkdir $ARGV[0]/$d2/ ; mkdir $ARGV[0]/$d2/gcp/ ; mkdir $ARGV[0]/$d2/dot ; mkdir $ARGV[0]/$d2/sniffer";
    print "$cmd\n";
    system($cmd);

    my @thefiles = ();
    opendir(IMD, "$ARGV[0]/$d/sniffer") || die "Cannot open directory\n" ;
    @thefiles = readdir(IMD);
    closedir(IMD);

    foreach my $f (@thefiles) {
	next if ($f !~ 'tcpdump');
	
	print "Parsing $f\n";
	
	my $map;
	if ($TESTBED eq "map") {
	    if ($f =~ /(map\d+)\-tcpdump.*/) {
		$map = $1;
	    }
	    else {
		print "Map node unknown $f\n";
		die;
	    }
	}
	elsif ($TESTBED eq "emulab") {
	    if ($f =~ /(nodew\d+.*?)\-tcpdump.*/) {
                $map = $1;
	    }
            else {
                print "Map node unknown $f\n";
                die;
            }
	}

	my @comb = ("offmhear", "offohear");

	my $MV = 0;

	if ($f =~ '\.gz') {
	    #run offline
	    $cmd = "cp -f $ARGV[0]/$d/sniffer/$f dump2.gz";
	    print "$cmd\n";
	    system($cmd);
	    
	    $cmd = "gunzip -f dump2.gz";
	    print "$cmd\n";
	    system($cmd);
	}
	else {
	    $cmd = "mv $ARGV[0]/$d/sniffer/$f dump2";
            print "$cmd\n";
            system($cmd);

	    $MV = 1;
	}

	foreach my $c (@comb) {
	    #run gtcd
	    my $cmd;
	    if ($SHELL eq "tcsh") {
		$cmd = "$DOT_PATH/dot/build/gtcd/gtcd_$chunk_size -f $DOT_PATH/dot/conf/dot.conf $REDIRECT op.crap &";
	    }
	    else {
		$cmd = "$DOT_PATH/dot/build/gtcd/gtcd_$chunk_size -f $DOT_PATH/dot/conf/dot.conf > op.crap $REDIRECT  &";
	    }
	    print "$cmd\n";
	    system($cmd);

	    sleep(1);

	    my $dir;
	    if ($c eq "offmhear") {
		if ($SHELL eq "tcsh") {
		    $cmd = "$DOT_PATH/dot/build/sniffTcp_$chunk_size 1000 lo /tmp/gtcd_sniff.sock 0 128.1.1.1 1 $REDIRECT op1.crap";
		}
		else {
		    $cmd = "$DOT_PATH/dot/build/sniffTcp_$chunk_size 1000 lo /tmp/gtcd_sniff.sock 0 128.1.1.1 1 > op1.crap $REDIRECT";
		}
		$dir = $d1;
	    }
	    elsif ($c eq "offohear") {
		if ($SHELL eq "tcsh") {
		    $cmd = "$DOT_PATH/dot/build/sniffTcp_$chunk_size 1000 lo /tmp/gtcd_sniff.sock 0 128.1.1.1 0 $REDIRECT op1.crap ";
		}
		else {
		    $cmd = "$DOT_PATH/dot/build/sniffTcp_$chunk_size 1000 lo /tmp/gtcd_sniff.sock 0 128.1.1.1 0 > op1.crap $REDIRECT";
		}
		$dir = $d2;
	    }
	    print "$cmd\n";
	    system($cmd);
	    
	    #kill gtcd
	    $cmd = "killall -9 gtcd_$chunk_size";
	    print "$cmd\n";
	    system($cmd);
	    
	    $cmd = "mv op.crap $ARGV[0]/$dir/dot/$map-dot.log";
	    print "$cmd\n";
	    system($cmd);

	    $cmd = "mv op1.crap $ARGV[0]/$dir/sniffer/$map-sniffer.log";
            print "$cmd\n";
            system($cmd);

	    $cmd = "cp gcp.crap $ARGV[0]/$dir/gcp/$recvr-gcp.log";
	    print "$cmd\n";
	    system($cmd);

	}

	if ($MV == 1) {
	    $cmd = "mv dump2 $ARGV[0]/$d/sniffer/$f";
	    print "$cmd\n";
	    system($cmd);
	}
	
	print "--------------------------------------------------\n";
    }
    
    print "-----------------------------------------------------\n";
    print "-----------------------------------------------------\n";
}

