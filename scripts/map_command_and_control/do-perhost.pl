#!/usr/bin/perl

##
# Run a command for each RON node
# (with ability to limit max concurrency)
##
=pod
BEGIN {
    my $file = "/etc/udplatscripts.conf";
    unless (my $return = do $file) {
        warn "couldn't parse $file: $@" if $@;
        warn "couldn't do $file: $!"    unless defined $return;
        warn "couldn't run $file"       unless $return;
    }
    push @INC, '.';  # Get taint mode to stop complaining
    push (@INC, "$NETUTILDIR/plib/");
}
=cut

use Getopt::Std;
use strict;
use POSIX ":sys_wait_h";
use IO::Handle;

# Unbuffer stdout...
select(STDOUT); $| = 1;

my $debug = 0;
my $max_concurrency = 50;
my $timeout = 1200;  # number of seconds before we abort a transfer
my $visual = 0;     # No verbose output
my $plab_hosts_file = "CURRENT.TXT";

## The different stages of the thingy
my $STATE_BEGIN = 0;  # Just starting out, nothing done
my $STATE_RUNNING = 1;
my $STATE_DONE = 2;
my $STATE_FAILED = 3;

my @STATES = ("begin", "running", "done", "failed");

my %opts = ();
getopts('vdt:c:f:', \%opts);

$debug = 1 if $opts{'d'};
$max_concurrency = $opts{'c'} if $opts{'c'};
$timeout = $opts{'t'} if $opts{'t'};
$visual = 1 if $opts{'v'};
$plab_hosts_file = $opts{'f'} if $opts{'f'};

my $kidcount = 0;
my $spawned = 0;
my @active_hosts = ();   # The list of probed hosts
my %STATE;               # State of each host
my %PID_TO_HOST;
my %CMD_START;

my $global_cmd = $ARGV[0];

my $current_time = time();

#CHANGE - gather all planetlab nodes here
#push @active_hosts, $host;
#$STATE{$host} = $STATE_BEGIN;

open(FILE,"$plab_hosts_file") || die "could not open $plab_hosts_file for reading" ;
my $sites = "";
while (my $line = <FILE>) {
    $sites .= $line;
}
@active_hosts = ($sites =~ m|HOST NAME=\"(.*?)\"|sig);
close(FILE);
foreach my $a (@active_hosts) {
    $STATE{$a} = $STATE_BEGIN;
}

#@active_hosts = ('plab2.eece.ksu.edu', 'planet3.cc.gt.atl.ga.us');
#$STATE{$active_hosts[0]} = $STATE_BEGIN;
#$STATE{$active_hosts[1]} = $STATE_BEGIN;

my @remaining_hosts = @active_hosts;

my $this_work = 1;

while ($#remaining_hosts >= 0) {
    # Read any available children
    
    if ($kidcount > $max_concurrency) {
	&kill_timeouts();
	print "Blocking:  @remaining_hosts\n" if $debug;
	while (wait_kid(0)) {
	    # Fixes the blocking problem, even if it's kind of ugly.
	    select(undef, undef, undef, 0.25);
	    &kill_timeouts();
	}
	#&wait_kid(1);  # Do at least one blocking wait
	&display_status_line();
    }
    # Clean up everyone that exited, nonblockingly
    while (!&wait_kid(0)) { &display_status_line(); }

    my $next_host = shift @remaining_hosts;
    &do_kid($next_host, $global_cmd);
    &display_status_line();
}

while ($kidcount > 0) {
    select(undef, undef, undef, 0.25);
    &display_status_line();
    &kill_timeouts();
    while (!&wait_kid(0)) { &display_status_line(); }
    # We're out of nonblocking waits;  simulate a time-limited
    # blocking wait via sleep. :-)
}

# And show some nice output
print "\n";
foreach my $host (@active_hosts) {
    if ($STATE{$host} != $STATE_DONE) {
	printf("%10.10s  %s\n", $host, $STATES[$STATE{$host}]);
    }
}

exit(0);

sub display_status_line {
    if (!$visual) { return; }
    my $bs = sprintf("%c", 0x08);
    print "$bs" x 80;
    print " " x 80;
    print "$bs" x 80;
    my %COUNT;
    foreach my $host (@active_hosts) {
	$COUNT{$STATE{$host}}++;
    }
    printf("Hosts: %d   Pending:  %d  Running: %d  Done:  %d  Failed:  %d",
	   $#active_hosts + 1,
	   $COUNT{$STATE_BEGIN}, $COUNT{$STATE_RUNNING},
	   $COUNT{$STATE_DONE}, $COUNT{$STATE_FAILED});
}

# Throw things into the state machine

sub kill_timeouts {
    my $curtime = time();
    while (my ($pid, $host) = each %PID_TO_HOST) {
	my $cmd_start = $CMD_START{$host};
	my $dur = $curtime - $cmd_start;
	if ($cmd_start && ($dur > $timeout)) {
	    kill 'TERM', $pid;
	    print "ERROR HOST NAME=\"$host\"\n";
	}
	if ($dur > ($timeout + 3)) {
	    kill 'KILL', $pid; # DIE DIE DIE!
	}
    }
}

sub cmd_finished {
    my ($host, $status) = @_;
    my $state = $STATE{$host};
    print "$host finished state $state with status $status\n" if $debug;
    if ($status == 0) {
	$STATE{$host} = $STATE_DONE;
    } else {
	$STATE{$host} = $STATE_FAILED;
    }
}

sub wait_kid {
    my $blocking = $_[0];
    my $kid;
    if ($blocking) {
	$kid = wait;
    } else {
	$kid = waitpid(-1, &WNOHANG);
    }
    if ($kid <= 0) { return -1; }  # Nobody's ready
    my $status = $?;
    my $host = $PID_TO_HOST{$kid};
    print "$kid exited with status $status\n" if $debug;
    $kidcount--;
    delete $PID_TO_HOST{$kid};
    delete $CMD_START{$host};
    &cmd_finished($host, $status);
    return 0;
}

sub do_kid {
    my ($host, $cmdstr) = @_;
    # Open the child, keep its output in some little hash.
    
    my $kidpid = fork();
    if (!defined($kidpid) || $kidpid < 0) {
	$STATE{$host} = $STATE_FAILED;
	return -1;
    }
    if (!$kidpid) { # I'm a little kid.  Get some work done.
	$cmdstr =~ s/\@HOST\@/${host}/g;
	print "Exec:  $cmdstr\n";
	exec $cmdstr;
    } else {
	$PID_TO_HOST{$kidpid} = $host;
	$CMD_START{$host} = time();
	$STATE{$host} = $STATE_RUNNING;
	$kidcount++;
	return 0;
    }
}
