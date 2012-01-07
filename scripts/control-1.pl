#!/usr/bin/perl -W

use strict 'vars';

#my @combs = ("dot-ohear", "proxy-ohear", "proxy-mhear");
my @combs = ("proxy-mhear", "dot-ohear");
#my @chunks = ("8K", "16K", "32K");
my @chunks = ("8K");

my $PUT_TAR_FILE = "all.tar.gz";
my $DOT_PATH = "/tmp/";

my $TESTBED = "map";
#my $TESTBED = "emulab";
#if map choose wireless interface between ath0 and wlan0
my $WIRELESS_INTERFACE = "ath0";

my $IP;
my $FIRST_OCTET;
if ($TESTBED eq "map") {
    if ($WIRELESS_INTERFACE eq "ath0") {
	$IP = "1\\.0\\.0";
        $FIRST_OCTET = "1";
    }
    elsif ($WIRELESS_INTERFACE eq "wlan0") {
	$IP = "2\\.0\\.0";
        $FIRST_OCTET = "2";
    }
}
elsif ($TESTBED eq "emulab") {
    $IP = "10\\.1\\.1";
    $FIRST_OCTET = "10";
}

my $NUM_AVGS = 3;
#for tcsh
my $REDIRECT = ">&";
#for bash
#my $REDIRECT = "2>&1 | tee";

my %allinterfaces = ();
my %allips = ();

my $bad_attempts = 0;

sub get_interface {
    my ($name) = @_;

    if ($TESTBED eq "map") {
	return $WIRELESS_INTERFACE;
    }
    
    if ($TESTBED eq "emulab") {
	my $cmd = "ssh $name 'ifconfig | grep ath0 | wc -l'";
	print "$cmd\n";
	open(IP, "$cmd | ");
	my $linecount = <IP>; chomp $linecount;
	print "$linecount\n";
	close(IP);
	if ($linecount == 1) {
	    return "ath0";
	}
	else  {
	    return "ath1";
	}
    }
}

sub get_ip {
    my ($name, $i) = @_;
    
    my $ip;
    my $cmd = "ssh $name \"/sbin/ifconfig | grep '$IP' | awk '{print \$2}'\"";
    print "$cmd\n";
    open(IP, "$cmd | ");
    my $linecount = <IP>; chomp $linecount;
    if ($linecount =~ /.*?addr\:(\d+\.\d+\.\d+\.\d+)/ ) {
	$ip = $1;
    }
    close(IP);
    print "$name -----> $linecount -----> $ip\n";
    return($ip);
}

sub run_kill {
    
    my ($file, $s) = @_;

    my $dot_exe = "gtcd_$s";
    my $sniff_exe = "sniffTcp_$s";

    my $cmd = "time ./do-perhost.pl -t 120 -c 200 -f $file \"ssh \@HOST\@ 'sudo killall -9 $dot_exe gcp $sniff_exe && rm -f /tmp/dot/out '\"";
    #&& rm -f /tmp/dot/*.log && rm -f /tmp/dot/*.log.gz*'\"";
    print "$cmd\n";
    system($cmd);

    # no -9 here please, else tcpdump's log fie will not be created.
    $cmd = "time ./do-perhost.pl -t 120 -c 200 -f $file \"ssh \@HOST\@ 'sudo killall tcpdump'\"";
    print "$cmd\n";
    system($cmd);
}

sub put_stuff {
    my ($file) = @_;

    my $cmd = " export WORK_PATH=$DOT_PATH ; time ./do-perhost.pl -t 600 -c 25 -f $file \"./copy-stuff.pl \@HOST\@ all.tar.gz\" ";
    print "$cmd\n";
    system($cmd);

    $cmd = "time ./do-perhost.pl -t 120 -c 200 -f $file \"ssh \@HOST\@ 'rm -rf $DOT_PATH/dot && cd $DOT_PATH && tar -zxf $DOT_PATH/$PUT_TAR_FILE >& /dev/null'\"";
    print "$cmd\n";
    system($cmd);
	
}

sub run_dot {
    
    my ($file, $hosts, $c, $s) = @_;
    my $cmd;

    my $DUMP = 1;
    my $SNIFFER = 1;

    my $dot_exe = "$DOT_PATH/dot/build/gtcd/gtcd_$s";
    my $sniff_exe = "$DOT_PATH/dot/build/sniffTcp_$s";

    if ($c eq "dot-ohear") {
	$cmd = "time ./do-perhost.pl -t 120 -c 200 -f $file \"ssh \@HOST\@ '$dot_exe -n -m $FIRST_OCTET -f $DOT_PATH/dot/conf/dot.conf $REDIRECT $DOT_PATH/dot/dot.log &'\"";
    }
    elsif ($c eq "proxy-ohear") {
	$cmd = "time ./do-perhost.pl -t 120 -c 200 -f $file \"ssh \@HOST\@ '$dot_exe -m $FIRST_OCTET -f $DOT_PATH/dot/conf/dot.conf $REDIRECT $DOT_PATH/dot/dot.log &'\"";
    }
    elsif ($c eq "proxy-mhear"){
	$cmd = "time ./do-perhost.pl -t 120 -c 200 -f $file \"ssh \@HOST\@ '$dot_exe -m $FIRST_OCTET -f $DOT_PATH/dot/conf/dot.conf $REDIRECT $DOT_PATH/dot/dot.log &'\"";
    }
    print "$cmd\n";
    system($cmd);
    
    if ($DUMP == 1) {
	#perform tcpdump instead of sniffTcp
	#in a for loop because we have to change the arg (ip addr) for each host
	foreach my $h (@{$hosts}) {
	    $cmd = "ssh $h 'sudo tcpdump -i $allinterfaces{$h} -w $DOT_PATH/dot/tcpdump.log -s 1600 \'tcp and not port 22 and not dst host $allips{$h}\' >& /dev/null &'";
            print "$cmd\n";
            system($cmd);
	}
    }

    if ($SNIFFER == 1) {
	# in a for loop because we have to change the arg (ip addr) for each host
	foreach my $h (@{$hosts}) {
	    my $param = 0;
	    if ($c eq "proxy-mhear") {
		$param = 1;
	    }
	    
	    $cmd = "ssh $h 'sudo $sniff_exe 1000 $allinterfaces{$h} /tmp/gtcd_sniff.sock 1 $allips{$h} $param $REDIRECT $DOT_PATH/dot/sniffer.log &'";
	    print "$cmd\n";
	    system($cmd);
	}
    }
}

sub run_sender {

    my ($s, $f) = @_;

    for (my $i = 0; $i <= $#{$s}; $i++) {
	my $cmd = "export WORK_PATH=$DOT_PATH/dot/ ; ./copy-stuff.pl ${$s}[$i] ${$f}[$i]";
	print "$cmd\n";
	system($cmd);	

	$cmd = "ssh ${$s}[$i] '$DOT_PATH/dot/build/gcp/gcp -f -P $DOT_PATH/dot/${$f}[$i]'";
	print "$cmd\n";
	system($cmd);
    }
}

sub run_recvr {

    my ($r, $o, $s) = @_;

    for (my $i = 0; $i <= $#{$s}; $i++) {

	my $cmd = "ssh ${$r}[$i] '$DOT_PATH/dot/build/gcp/gcp -f dot://${$o}[$i]:$allips{${$s}[$i]}:12000:1 $DOT_PATH/dot/out $REDIRECT $DOT_PATH/dot/gcp.log &'";
	print "$cmd\n";
	system($cmd);
    }
}

sub check_recvrs {
    
    my ($r, $o, $b) = @_;

    for (my $i = 0; $i <= $#{$r}; $i++) {

        my $cmd = "ssh ${$r}[$i] 'openssl sha1 $DOT_PATH/dot/out'";
        print "$cmd\n";
        
	open(IP, "$cmd | ");
	my $hash;
	my $line = <IP>; chomp $line;
	close(IP);

	my $tbad = 0;
	if ($line =~ /SHA1.*?\=\s+(.*)/) {
	    $hash = $1;
	    if (!defined $hash ||
		$hash ne ${$o}[$i]) {
		$tbad = 1;
		print "**********************Bad hash not matched $hash ${$o}[$i] ${$r}[$i]\n";
	    }
	    else {
		print "Good ${$o}[$i] $hash\n";
	    }
	}
	else {
	    $tbad = 1;
	    print "**********************Bad no hash ${$r}[$i]\n";
	}
	
	push(@{$b}, $tbad);
    }
}

sub figure_out {
    
    my ($r, $o, $b, $s, $expt_dir) = @_;
    
    my $bad = 0;

    for (my $i = 0; $i <= $#{$r}; $i++) {

	my $tbad = ${$b}[$i];
	
	if ($tbad == 1) {
	    #see if connectivity problem
	    my $cmd = "zcat $expt_dir/gcp/${$r}[$i]-gcp.log.gz | grep 'could not connect' >& con.crap ";
	    print "$cmd\n";
	    system($cmd);
	    
	    open(IP, "con.crap");
	    my $line = <IP>; 
	    if (!defined $line) {
		chomp $line;
		print "Looks like bad throughput\n";
	    }
	    else {
		print "Connect problems\n";
		if ($bad_attempts == 0) {
                    $cmd = "ssh ${$s}[$i] 'ping -i 1 -c 50 -R $allips{${$r}[$i]}'";
                    print "$cmd\n";
                    system($cmd);

		    $cmd = "ssh ${$r}[$i] 'ping -i 1 -c 50 -R $allips{${$s}[$i]}'";
		    print "$cmd\n";
		    system($cmd);
		}
	    }
	    $bad = 1;
	}
    }    
    
    if ($bad == 1) {
	$bad_attempts++;
    }
}

sub make_list {
    my ($r) = @_;
    
    open(FILE,">recvrs.crap") || die "could not open recvrs.crap for writing\n" ;
    foreach my $e (@{$r}) {
	print FILE "NODE_ID=\"10490\" HOST NAME=\"$e\" IP=\"$e\" BWLIMIT=\"1\" SITE_ID=\"10141\"\n";
    }
    close(FILE);
    
    return("recvrs.crap");
}

sub monitor {
    
    my ($file) = @_;

    my $cmd = "perl monitor.pl $file gcp 0";
    print "$cmd\n";
    system($cmd);
}

sub get_stuff {
    my ($file, $expt_dir) = @_;
    
    my $cmd = "rm -rf $expt_dir ; mkdir -p $expt_dir/dot $expt_dir/sniffer $expt_dir/gcp";
    print "$cmd\n";
    system($cmd);

    print "-----> fetching gtcd logs from all nodes \n";
    $cmd = "time ./do-perhost.pl -t 600 -c 25 -f $file \"ssh  \@HOST\@ 'gzip -f $DOT_PATH/dot/*.log '\" ";                      
    print "$cmd\n";
    system($cmd);                                                                                                                           

    $cmd = "export WORK_PATH=$DOT_PATH/dot ; time ./do-perhost.pl -t 600 -c 25 -f $file './get-stuff.pl \@HOST\@ dot.log.gz $expt_dir/dot/' ";
    print "$cmd\n";
    system($cmd);

    print "-----> fetching sniffer logs from all nodes \n";
    $cmd = "export WORK_PATH=$DOT_PATH/dot ; time ./do-perhost.pl -t 600 -c 25 -f $file './get-stuff.pl \@HOST\@ sniffer.log.gz $expt_dir/sniffer/' ";
    print "$cmd\n";
    system($cmd);

    print "-----> fetching gcp logs from all nodes \n";
    $cmd = "export WORK_PATH=$DOT_PATH/dot ; time ./do-perhost.pl -t 600 -c 25 -f $file './get-stuff.pl \@HOST\@ gcp.log.gz $expt_dir/gcp/' ";
    print "$cmd\n";
    system($cmd);

    print "-----> fetching tcpdump logs from all nodes \n";
    $cmd = "export WORK_PATH=$DOT_PATH/dot ; time ./do-perhost.pl -t 600 -c 25 -f $file './get-stuff.pl \@HOST\@ tcpdump.log.gz $expt_dir/sniffer/' ";
    print "$cmd\n";
    system($cmd);

    $cmd = "time ./do-perhost.pl -t 120 -c 200 -f $file \"ssh \@HOST\@ 'sudo rm -f /tmp/dot/*.log && rm -f /tmp/dot/*.log.gz'\""; 
    print "$cmd\n";
    system($cmd);

}

if( $#ARGV < 2 ) { die "perl control-1.pl <list of nodes> <expt-dir> <list of sender:file:recvr:oid>"; }

my $node_list = shift(@ARGV);
my $expt_dir = shift(@ARGV);
my @list = @ARGV;

#get all nodes, ips and interfaces out
open(FILE,"$node_list") || die "could not open $node_list for reading" ;
my $sites = "";
while (my $line = <FILE>) {
    $sites .= $line;
}
close(FILE);

my @allhosts = ($sites =~ m|HOST NAME=\"(.*?)\"|sig);
foreach my $h (@allhosts) {
    my $int = get_interface($h);
    $allinterfaces{$h} = $int;
    $allips{$h} = get_ip($h, $int);
}
print "--------------------------------------------\n";

my @senders;
my @recvrs;
my @files;
my @oids;

foreach my $l (@list) {
    chomp $l;
    if ($l =~ /(.*?)\:(.*?)\:(.*?)\:(.*)/) {
	#print "$1 $2 $3 $4\n";
	push(@senders, $1);
	push(@files, $2);
	push(@recvrs, $3);
	push(@oids, $4);
    }
    else {
	print "Problem";
	die;
    }
}

my $recv_list = make_list(\@recvrs);

foreach my $s (@chunks) {
    run_kill($node_list, $s);
}
print "--------------------------------------------\n";
put_stuff($node_list);
print "--------------------------------------------\n";

foreach my $s (@chunks) {
    $bad_attempts = 0;
    for (my $i = 0; $i < $NUM_AVGS; $i++) {
        foreach my $c (@combs) {

	    print "PERRUN chunk = $s, comb = $c, run = $i\n";
	    system("date");

	    print "-----> Killing processes in all nodes\n";
	    run_kill($node_list, $s);
	    print "--------------------------------------------\n";

	    #one run
	    print "-----> Starting GTCDs and Sniffers on all nodes\n";
	    run_dot($node_list, \@allhosts, $c, $s); # amar:: start sniffTcp on all nodes?
	    print "--------------------------------------------\n";
	    
	    print "-----> Starting sender GCP\n";
	    run_sender(\@senders, \@files);
	    print "--------------------------------------------\n";
	    
	    print "-----> Starting receiver GCP\n";
	    run_recvr(\@recvrs, \@oids, \@senders);
	    print "--------------------------------------------\n";
	    
	    print "-----> Waiting for receiver GCP to die ...\n";
	    monitor($recv_list);
	    print "--------------------------------------------\n";
	    
	    sleep 5;
	
	    print "-----> Checking receivers\n";
	    my @bad = ();
	    check_recvrs(\@recvrs, \@oids, \@bad);
	    print "--------------------------------------------\n";

	    print "-----> Killing processes in all nodes\n";
	    run_kill($node_list, $s);
	    print "--------------------------------------------\n";
	    
	    print "-----> Fetching logs from all nodes\n";
	    get_stuff($node_list, "$expt_dir-$s-$c-$i");
	    print "--------------------------------------------\n";

	    print "-----> Figuring out the problems\n";
	    figure_out(\@recvrs, \@oids, \@bad, \@senders, "$expt_dir-$s-$c-$i");
	    print "--------------------------------------------\n";

	    last if ($bad_attempts >= 2);

#             print "-----> copying data to moo\n";
#             system("date");
#             system("scp -r expt* aphanish\@moo.cmcl.cs.cmu.edu:/disk/agami1/aphanish/share/ditto_logs/emulab/");
#             #system("scp -r expt* aphanish\@moo.cmcl.cs.cmu.edu:/disk/agami2/aphanish/ditto/share/ditto_logs/e2");
#             system("sudo rm -rf expt*");
#             print "-----------------------------------------------------\n";
	    
	} #avgs
	last if ($bad_attempts >= 2);
    } #combs
} #chunk sizes

