#!/usr/bin/perl -W

use strict 'vars';

if ($#ARGV < 4) { die "perl control-1.pl <list of nodes> <testbed> <gateway hostname> <file size in KB> <recvr list>"; }

#setup the testbed
my $cmd = "perl setup/setup.pl $ARGV[0] $ARGV[1]";
print "$cmd\n";
#system($cmd);

#generate traffic
open(FILE,"$ARGV[4]") || die "could not open $ARGV[4] for reading" ;
my $sites = "";
while (my $line = <FILE>) {
    $sites .= $line;
}
close(FILE);
my @allhosts = ($sites =~ m|HOST NAME=\"(.*?)\"|sig);

#generate file and oid
my $sender_name;
if ($ARGV[2] =~ /(.*?)\./) {
    $sender_name = $1;
}
else {
    $sender_name = $ARGV[2];
}
$cmd = "dd if=/dev/urandom of=file-$sender_name bs=1024 count=$ARGV[3]";
print "$cmd\n";
system($cmd);

$cmd = "openssl sha1 file-$sender_name";
print "$cmd\n";
open(IP, "$cmd | ");
my $oid;
my $line = <IP>; chomp $line;
close(IP);
#print "$line\n";
if ($line =~ /SHA1.*?\=\s+(.*)/) {
    $oid = $1;
}
else {
    print "Bad oid\n";
    die;
}
print "$oid\n";

foreach my $h (@allhosts) {

    next if ($h eq $ARGV[2]);

    my $recvr_name;
    if ($h =~ /(.*?)\./) {
	$recvr_name = $1;
    }
    else {
	$recvr_name = $h;
    }

    print "PERRECVR $recvr_name\n";
    system("date");
    
    $cmd = "perl control-1.pl $ARGV[0] expt-$recvr_name $ARGV[2]:file-$sender_name:$h:$oid";
    print "$cmd\n";
    system($cmd);

    sleep(60);

    print "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    print "copying data to moo\n";
    system("date");
    system("scp -r expt* aphanish\@moo.cmcl.cs.cmu.edu:/disk/agami1/aphanish/share/ditto_logs/$ARGV[1]/");
    system("sudo rm -rf expt*");
    print "-----------------------------------------------------\n";
    print "-----------------------------------------------------\n";
    print "-----------------------------------------------------\n";
}


