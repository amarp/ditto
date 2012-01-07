#!/usr/bin/perl -W

use strict 'vars';

my @combs = ("dot", "proxy-mhear", "proxy");
my $chunks = "8K";

my @files = ("dot_snap_20070206.tar.gz"); #, "FILES/gcc2", "FILES/gcc3");
my @oids = ("5c6bac26d0f356b3590b786972a742561644b202"); #, "efgh", "ijkl");

#scenario1
#my @recvrs = ("map18", "map7", "map23");
#my @filenums = ("0", "0", "0");
#time to sleep between reqs
#my @times = ("0", "200", "200");

#scenario2
my @recvrs = ("map2", "map3", "map5", "map7", "map14", "map15", "map18", "map19", "map25", "map27", "map38");
#my @recvrs = ("map34", "map19", "map2", "map22");
my @filenums = ("0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0");
#my @times = ("0", "60", "60", "60", "60", "60", "60", "60", "60", "60", "60");
my @times = ("0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0");

#my @recvrs = ("nodew2", "nodew7");
#my @filenums = ("0", "0");
#my @times = ("0", "30");


my $GATEWAY = "map11";
#my $GATEWAY = "nodew8.ditto.netarch.emulab.net";

my $PUT_TAR_FILE = "all.tar.gz";
my $DOT_PATH = "/tmp/";

my $TESTBED = "map";
#my $TESTBED = "emulab";
#if map choose wireless interface between ath0 and wlan0
my $WIRELESS_INTERFACE = "ath0";
#my $WIRELESS_INTERFACE = "wlan0";

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

my $NUM_AVGS = 5;
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

    my $cmd = "time ./do-perhost.pl -t 120 -c 200 -f $file \"ssh \@HOST\@ 'sudo killall -9 $dot_exe gcp $sniff_exe && rm -f /tmp/dot/out'\"";
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

    if ($c eq "dot") {
	$cmd = "time ./do-perhost.pl -t 120 -c 200 -f $file \"ssh \@HOST\@ '$dot_exe -n -m $FIRST_OCTET -f $DOT_PATH/dot/conf/dot.conf $REDIRECT $DOT_PATH/dot/dot.log &'\"";
    }
    elsif ($c eq "proxy-mhear" || $c eq "proxy"){
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

    if ($SNIFFER == 1 && $c eq "proxy-mhear") {
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

    for (my $i = 0; $i <= $#{$f}; $i++) {
	my $cmd = "export WORK_PATH=$DOT_PATH/dot/ ; ./copy-stuff.pl $s ${$f}[$i]";
	print "$cmd\n";
	system($cmd);	

	$cmd = "ssh $s '$DOT_PATH/dot/build/gcp/gcp -f -P $DOT_PATH/dot/${$f}[$i]'";
	print "$cmd\n";
	system($cmd);
    }
}

sub run_recvr {

    my ($r, $o, $s) = @_;

    #my $cmd = "ssh $r '$DOT_PATH/dot/build/gcp/gcp -f dot://$o:$allips{$s}:12000:1 $DOT_PATH/dot/out $REDIRECT $DOT_PATH/dot/gcp.log &'";
    my $cmd = "ssh $r '$DOT_PATH/dot/build/gcp/gcp -f dot://$o:$allips{$s}:12000:1 $DOT_PATH/dot/out $REDIRECT $DOT_PATH/dot/gcp.log'";
    print "$cmd\n";
    system($cmd);

    print "Completed running $r\n";
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
		$hash ne ${$o}[$filenums[$i]]) {
		$tbad = 1;
		print "**********************Bad hash not matched $hash ${$o}[$filenums[$i]] ${$r}[$i]\n";
	    }
	    else {
		print "Good ${$o}[$filenums[$i]] $hash $line\n";
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
                    $cmd = "ssh $s 'ping -i 1 -c 50 -R $allips{${$r}[$i]}'";
                    print "$cmd\n";
                    system($cmd);

                    $cmd = "ssh ${$r}[$i] 'ping -i 1 -c 50 -R $allips{$s}'";
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

if( $#ARGV < 1 ) { die "perl xput-control-1.pl <list of nodes> <expt-dir>"; }

my $node_list = shift(@ARGV);
my $expt_dir = shift(@ARGV);

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

my $recv_list = make_list(\@recvrs);

run_kill($node_list, $chunks);
print "--------------------------------------------\n";
