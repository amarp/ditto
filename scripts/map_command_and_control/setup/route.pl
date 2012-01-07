#!/usr/bin/perl -W

use strict 'vars';

my $TESTBED;
my $NODE_LIST;

my $NUM_NODES = 38;
my $WIRELESS_INTERFACE = "ath0";
#my $POWER = 80; # Max power for ath
my $POWER = -1; 

#my $WIRELESS_INTERFACE = "wlan0";

#my $POWER = -1; #change it to something +ve based on testbed and interface
my $DOT_PATH = "/tmp/";
my $GATEWAY = "map11";

#for tcsh                                                                                                                              
my $REDIRECT = ">&";
#for bash 
#my $REDIRECT = "2>&1 | tee";   

my $IP;

my %allinterfaces = ();
my %allips = ();
my @allhosts = ();

sub get_interface {
    my $name = @_;

    if ($TESTBED eq "map") {
        return $WIRELESS_INTERFACE;
    }
    
    if ($TESTBED eq "emulab") {
        my $cmd = "ssh $name 'ifconfig | grep ath0 | wc -l'";
        open(IP, "$cmd | ");
        my $linecount = <IP>; chomp $linecount;
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
    my ($name) = @_;

    my $ip;
    my $cmd = "ssh $name \"/sbin/ifconfig | grep '$IP' | awk '{print \$2}'\"";
    print "$cmd\n";
    open(IP, "$cmd | ");
    my $linecount = <IP>; chomp $linecount;
    if ($linecount =~ /.*?addr\:(\d+\.\d+\.\d+\.\d+)/ ) {
        $ip = $1;
    }
    close(IP);
    print "$name $linecount $ip\n";
    return($ip);
}

sub change_radio {
    my ($hosts, $radio) = @_;

    if ($radio == -1) {
	return;
    }

    if ($WIRELESS_INTERFACE eq "wlan0") {
	return;
    }

    if ($TESTBED eq "map") {
	foreach my $h (@{$hosts}) {
	    my $cmd = "ssh $h 'sudo iwconfig $allinterfaces{$h} ESSID \"AMAP\" && sudo iwpriv $allinterfaces{$h} mode $radio '";
	    print "$cmd\n";
	    system($cmd);
	    
	    $cmd = "ssh $h 'sudo iwconfig $allinterfaces{$h} mode Ad-Hoc && sudo iwpriv $allinterfaces{$h} ibss 0 '";
	    print "$cmd\n";
	    system($cmd);
	
	    my $channel;
	    if ($radio == 1) {
		$channel  = 36;
	    }
	    elsif ($radio == 3) {
		$channel = 1;
	    }

	    $cmd = "ssh $h 'sudo iwconfig $allinterfaces{$h} channel $channel && sudo ifconfig $allinterfaces{$h} up && sudo ifconfig $allinterfaces{$h} $allips{$h} '";
	    print "$cmd\n";
	    system($cmd);
	}
    }

    
}

#restore card behavior to default
sub restore_testbed {

    print "Rebooting testbed\n";
    
    if ($TESTBED eq "map") {
	 my $cmd = "./do-perhost.pl -t 120 -c 200 -f $NODE_LIST \"ssh \@HOST\@ 'sudo cardctl eject && sudo modprobe -r ath_pci'\"";
	 print "$cmd\n";
	 system($cmd);

	 $cmd = "./do-perhost.pl -t 120 -c 200 -f $NODE_LIST \"ssh \@HOST\@ 'sudo cardctl insert && sudo modprobe ath_pci'\"";
	 print "$cmd\n";
	 system($cmd);

	 $cmd = "./do-perhost.pl -t 120 -c 200 -f $NODE_LIST \"ssh \@HOST\@ 'sudo /map/startwlan'\"";
	 print "$cmd\n";
	 system($cmd);
	 
	 $cmd = "./do-perhost.pl -t 120 -c 200 -f $NODE_LIST \"ssh \@HOST\@ 'sudo /map/startoldath'\"";
	 print "$cmd\n";
	 system($cmd);
	 
	 $cmd = "./do-perhost.pl -t 120 -c 200 -f $NODE_LIST \"ssh \@HOST\@ 'sudo iwconfig wlan0 rate auto'\"";
	 print "$cmd\n";
	 system($cmd);

	 $cmd = "./do-perhost.pl -t 120 -c 200 -f $NODE_LIST \"ssh \@HOST\@ 'sudo iwconfig ath0 rate auto'\"";
	 print "$cmd\n";
	 system($cmd);

	 $cmd = "./do-perhost.pl -t 120 -c 200 -f $NODE_LIST \"ssh \@HOST\@ 'sudo /sbin/service smb stop'\"";
	 print "$cmd\n";
	 system($cmd);
	 
    }
    elsif ($TESTBED eq "emulab") {
	print "Figure out emulab...may be you can just reboot the nodes\n";
	die;
    }

    print "-----------------------------------------------\n";
}

sub change_power {

    if ($POWER < 0) {
	print "Not changing power\n";
	return;
    }

    print "Chaging power\n";

    my $cmd;
    if ($TESTBED eq "map") {
	if ($WIRELESS_INTERFACE eq "ath0") {
	    $cmd = "./do-perhost.pl -t 120 -c 200 -f $NODE_LIST \"ssh \@HOST\@ 'sudo iwconfig ath0 txpower $POWER'\"";
	    print "$cmd\n";
	    system($cmd); 
	}
	elsif ($WIRELESS_INTERFACE eq "wlan0") {
	    $cmd = "./do-perhost.pl -t 120 -c 200 -f $NODE_LIST \"ssh \@HOST\@ 'sudo iwpriv wlan0 alc 0'\"";
	    print "$cmd\n";
	    system($cmd); 
	    
	    $cmd = "./do-perhost.pl -t 120 -c 200 -f $NODE_LIST \"ssh \@HOST\@ 'sudo iwpriv wlan0 writemif 62 $POWER'\"";
	    print "$cmd\n";
	    system($cmd); 
	}
	else {
	    print "Problem in interface\n";
	    die;
	}
    }
    elsif ($TESTBED eq "emulab") {
	print "Figure out emulab...\n";
	die;
    }
}

sub cleanup_routing {

    print "Cleanup routing\n";

    my $cmd = "./do-perhost.pl -t 120 -c 200 -f $NODE_LIST \"ssh \@HOST\@ 'sudo killall -9 olsrd-emulab olsrd-map'\"";
    print "$cmd\n";
    system($cmd);
    
    if ($TESTBED eq "map") {
	for (my $i = 1; $i <= $NUM_NODES; $i++) {
	    $cmd = "./do-perhost.pl -t 120 -c 200 -f $NODE_LIST \"ssh \@HOST\@ 'sudo route del -host 1.0.0.$i' \" >& /dev/null";
	    print "$cmd\n";
	    system($cmd);
	    
	    $cmd = "./do-perhost.pl -t 120 -c 200 -f $NODE_LIST \"ssh \@HOST\@ 'sudo route del -host 2.0.0.$i' \" >& /dev/null";
	    print "$cmd\n";
	    system($cmd);
	}
    }
    elsif ($TESTBED eq "emulab") {
	$cmd = "./do-perhost.pl -t 120 -c 200 -f $NODE_LIST \"ssh \@HOST\@ 'sudo route del -net 10.1.0.0 netmask 255.255.255.0'\"";
	print "$cmd\n";
	system($cmd);
    }
    
    print "-----------------------------------------------\n";
}

sub setup_routing {

    print "Setup routing";
    
    my ($status) = @_;

    my $olsr_exe = "olsrd-$TESTBED";
    my $olsr_conf = "olsrd-$TESTBED-$WIRELESS_INTERFACE.conf";
    
    my $cmd = "export WORK_PATH=$DOT_PATH ; ./do-perhost.pl -t 600 -c 25 -f $NODE_LIST \"./copy-stuff.pl \@HOST\@ setup/$olsr_exe\" ";
    print "$cmd\n";
    system($cmd);

    $cmd = "export WORK_PATH=$DOT_PATH ; ./do-perhost.pl -t 600 -c 25 -f $NODE_LIST \"./copy-stuff.pl \@HOST\@ setup/$olsr_conf\" ";
    print "$cmd\n";
    system($cmd);
    
    $cmd = "./do-perhost.pl -t 120 -c 200 -f $NODE_LIST \"ssh \@HOST\@ 'sudo $DOT_PATH/$olsr_exe -f $DOT_PATH/$olsr_conf -d 0'\"";
    print "$cmd\n";
    system($cmd); 

    #print "$status\n";

    sleep(2*60);

    if ($status == 1) {
	$cmd = "./do-perhost.pl -t 120 -c 200 -f $NODE_LIST \"ssh \@HOST\@ 'sudo killall -9 olsrd-map olsrd-emulab'\"";
	print "$cmd\n";
	system($cmd);
    }

    print "-----------------------------------------------\n";
}


sub setup_hard_route {
    #@allhosts --> path from recvr to gateway
    for (my $i = 0; $i < $#allhosts; $i++) {
	my $cmd;
	if ($i != $#allhosts - 1) {
	    $cmd = "ssh $allhosts[$i] 'sudo route add -host $allips{$allhosts[$#allhosts]} gw $allips{$allhosts[$i+1]} dev $allinterfaces{$allhosts[$i]}'";
	}
	else {
	    $cmd = "ssh $allhosts[$i] 'sudo route add -host $allips{$allhosts[$#allhosts]} dev $allinterfaces{$allhosts[$i]}'";
	}
	print "$cmd\n";
	system($cmd);
    }

    #setup from gw to recvr
    for (my $i = $#allhosts; $i > 0; $i--) {
	my $cmd;
	
	$cmd = "ssh $allhosts[$i] 'sudo route add -host $allips{$allhosts[$i-1]} dev $allinterfaces{$allhosts[$i]}'";
	print "$cmd\n";
	system($cmd);
	
	for (my $j = $i-2; $j >= 0; $j--) {
	    $cmd = "ssh $allhosts[$i] 'sudo route add -host $allips{$allhosts[$j]} gw $allips{$allhosts[$i-1]} dev $allinterfaces{$allhosts[$i]}'";
	    print "$cmd\n";
	    system($cmd);
	}
    }
}

sub test_connect {
    
   open(FILE,"$NODE_LIST") || die "could not open $NODE_LIST for reading" ;
   my $sites = "";
   while (my $line = <FILE>) {
       $sites .= $line;
   }
   close(FILE);

   my @hostnames = ($sites =~ m|HOST NAME=\"(.*?)\"|sig);

   my $cmd = "rm -rf ping && mkdir ping && rm -rf iperf && mkdir iperf";
   print "$cmd\n";
   system($cmd);
       
   foreach my $h (@hostnames) {
       my @nbrs = ();
       my @xput = ();
       
       foreach my $j (@hostnames) {
	   next if $h eq $j;
       
	   my $cmd = "ssh $h 'ping -R -i 1 -c 10 $allips{$j}' $REDIRECT ping/$h-$j.log";
	   print "$cmd\n";
	   system($cmd);
	   
	   #compute loss rates
	   $cmd = "grep loss ping/$h-$j.log";
	   open(FILE, "$cmd |") || die "could not open $cmd for reading";
	   my $line = <FILE>;
	   chomp $line;
	   my $p;
	   if ($line =~ m/^(.*?)(\d+)\% packet loss/) {
	       $p = $2/100;
	       #print "$p\n";
	   }
	   else {
	       print "problem in ping $h\n";
	       die;
	   }
	   close(FILE);
	   if ($p >= 0.8) {
	       print "********$h cannot reach $j\n";
	       next;
	   }
	   
	   push(@nbrs, $j);
	   
	   $cmd = "ssh $h 'sudo killall -9 iperf'";
           print "$cmd\n";
           system($cmd);

	   $cmd = "ssh $j 'sudo killall -9 iperf'";
           print "$cmd\n";
           system($cmd);
	   
	   sleep(1);

	   $cmd = "ssh $j 'iperf -s -f k >& /tmp/$j.log &'";
	   print "$cmd\n";
	   system($cmd);

	   sleep(1);

	   $cmd = "./timeout 30 ssh $h 'iperf -t 20 -c $allips{$j} -f k' $REDIRECT iperf/$h-$j.log";
	   print "$cmd\n";
	   system($cmd);

	   $cmd = "ssh $j 'sudo killall -2 iperf'";
	   print "$cmd\n";
	   system($cmd);
	   
	   $cmd = "scp $j:/tmp/$j.log iperf/recv-$h-$j.log";
	   print "$cmd\n";
	   system($cmd);

	   open(FILE, "iperf/recv-$h-$j.log") || die "could not open iperf/$h-$j.log for reading";
	   my $prev;
	   while (my $line = <FILE>) {
	       chomp $line;
	       $prev = $line;
	   }
	   print "TCP $h-$j $prev\n";  
	   
	   my $tempx;
	   if ($prev =~ /^.*?(\d+\.*\d*\s+Kbits\/sec)$/) {
	       $tempx = $1;
	   }
	   else {
	       print "BADDDDD $prev\n";
	       $tempx = -1;
	   }
	   push(@xput, $tempx);

	   print "--------------------------------------------------------\n";
       }
       
       print "STAT $h ";
       for (my $lik = 0; $lik <= $#nbrs; $lik++) {
	   print "$nbrs[$lik] $xput[$lik] || ";
       }
       print "\n";
       print "--------------------------------------------------------\n";
       print "--------------------------------------------------------\n";
   }
}

################

if( $#ARGV < 1 ) { die "perl setup.pl <list of nodes> <testbed type>"; }

$NODE_LIST = shift(@ARGV);

$TESTBED = shift(@ARGV);
die if ($TESTBED ne "map" && $TESTBED ne "emulab");

if ($TESTBED eq "map") {
    if ($WIRELESS_INTERFACE eq "ath0") {
        $IP = "1\\.0\\.0";
    }
    elsif ($WIRELESS_INTERFACE eq "wlan0") {
        $IP = "2\\.0\\.0";
    }
}
elsif ($TESTBED eq "emulab") {
    $IP = "10\\.1\\.1";
}

#get all nodes, ips and interfaces out
open(FILE,"$NODE_LIST") || die "could not open $NODE_LIST for reading" ;
my $sites = "";
while (my $line = <FILE>) {
    $sites .= $line;
}
close(FILE);
@allhosts = ($sites =~ m|HOST NAME=\"(.*?)\"|sig);
foreach my $h (@allhosts) {
    my $int = get_interface($h);
    $allinterfaces{$h} = $int;
    $allips{$h} = get_ip($h);
}

#1. restore test bed
restore_testbed();

# change radio
#change_radio(\@allhosts, -1);
#1 for a and 3 for g

#2. play with power
#change_power();
#print "-----------------------------------------------\n";

#3. set up routing
cleanup_routing();
print "-----------------------------------------------\n";

#pass 0 to leave olsr running and 1 to stop it
#setup_routing(1);
#setup_hard_route();
#print "-----------------------------------------------\n";

#4. test_connectivity
#ping for ping, tcp or iperf for tcp throughput
test_connect();
#if (test_connect("ping") == 0) {
#    print "Network not connected\n";
#    die;
#}



