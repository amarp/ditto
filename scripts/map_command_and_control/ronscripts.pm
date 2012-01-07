#!/usr/bin/perl

=pod
=head1 NAME

ronscripts.pm - utility functions for writing scripts that run on all
RON/emulab nodes

=head1 SUMMARY

   &lock_obtain("/tmp/mylockfile", $timeout);
   &lock_release("/tmp/mylockfile");

   &set_user("root");
   &set_identity("/home/root/.ssh/identity");
   &run_on_kid($host, "uptime");
   &rsync_to_kid($host, $remote_dir, $local_files);
   &rsync_from_kid($host, $local_dir, $remote_files);

=head1 SEE ALSO

L<do-perhost.pl|do-perhost.pl>
L<client-rotate.pl|client-rotate.pl>

=cut

package ronscripts;

use strict;
use IO::Handle;
use Fcntl qw(:flock);

use vars (qw(@ISA @EXPORT @EXPORT_OK %EXPORT_TAGS $VERSION));

require Exporter;
$VERSION = '1.00';
@ISA = ('Exporter');
my $_kid_output = "";

@EXPORT = (qw(&rsync_to_kid &rsync_from_kid &run_on_kid),
	   qw(&set_user &set_identity &kid_output),
	   qw(&lock_obtain &lock_release));

my @ssh_args = ("-q", "-P", "-T", "-x", "-a", "-e", "none",
		"-o", "CheckHostIP=no", 
		'-o', "BatchMode=yes", 
		'-o', 'Protocol=2',
		'-o', 'StrictHostKeyChecking=no',
		'-o', 'UserKnownHostsFile=/dev/null'
	       );
my $rsync_rsh = "/usr/bin/ssh " . join(" ", @ssh_args) . "";
my @ssh_args_full = (@ssh_args, "-n");

# Hm.
#my $RSYNC_ARGS = "-q --timeout=${timeout} -az";

my $debug = 0;
my $timeout = 600;  # number of seconds before we abort a transfer

sub set_debug   { $debug   = $_[0]; }
sub set_timeout { $timeout = $_[0]; }

my @ids = ();
my @users = ();

###############################################################
# Remote execution functions

sub re_args {
    @ssh_args_full = (@ssh_args, @users, @ids);
    $rsync_rsh = "/usr/bin/ssh " . join(" ", @ssh_args_full) . "";
    push @ssh_args_full, "-n";
}
    
sub set_user {
    @users = ("-l", $_[0]);
    &re_args();
}

sub set_identity {
    @ids = ("-i", $_[0]);
    &re_args();
}

sub kid_output { return $_kid_output; }

sub rsync_from_kid {
    my ($host, $dest_dir, @files) = @_;
    return &do_kid($host, "/usr/bin/rsync", "-aqz", "-e", $rsync_rsh,
		   "${host}:" . join(' ', @files),
		   $dest_dir);
}

sub rsync_to_kid {
    my ($host, $dest_dir, @files) = @_;
    return &do_kid($host, "/usr/bin/rsync", "-aqz", "-e", $rsync_rsh,
		   @files, "${host}:${dest_dir}");
}

sub run_on_kid {
    my ($host, @commands) = @_;
    return &do_kid($host, "/usr/bin/ssh", @ssh_args_full, 
		   "${host}", @commands);
}

sub do_kid {
    my ($host, @commands) = @_;

    #print "do kidding in $host | @commands\n";
    
    my $kidpid = open(KID, "-|");
    if (!defined($kidpid)) {
	print STDERR "fork failed: $!\n" if $debug;
	return -1;
    }
    if (!$kidpid) {
	close(STDERR);
	exec @commands;
	die "Child fatal error:  Exec failed.  $!";
    } else {
	$_kid_output = "";
	while (<KID>) {
	    $_kid_output .= $_;
	}
	my $kid = wait;
	my $status = $?;
	print "Kid $kid exited with status $status\n" if $debug;
	close(KID);
	return $status;
    }
}

################################################################
# Lock handling

sub lock_file {
    my $lock_file = $_[0];
    my $nolock = 0;
    open(LOCK, ">$lock_file") || return -1;
    my $didlock = flock(LOCK, LOCK_EX | LOCK_NB);
    if ($didlock) {
	open(LOCKPID, ">${lock_file}.pid");
	print LOCKPID "$$\n";
	close(LOCKPID);
	return 0;
    } else {
	# file was locked when we tried.
	return -1;
    }
}

sub lock_obtain {
    my ($lock_file, $lock_timeout) = @_;
    my $status = &lock_file($lock_file);
    if ($status) {
	my @pidstats = stat("${lock_file}.pid");
	my $curtime = time();
	my $mtime = $pidstats[9];

	open(LOCKPID, "${lock_file}.pid");
	my $lock_pid = <LOCKPID>;
	close(LOCKPID);
	my $lock_age = ($mtime - $curtime);
	print "Lock owned by $lock_pid  age $lock_age\n" if $debug;

	if ($lock_age > $lock_timeout) {
	    # Break the lock..
	    kill 'TERM', $lock_pid;
	    sleep(1);
	    kill 9, $lock_pid;
	    $status = &lock_file();
	    if ($status) {
		#die "could not break lockfile pid $lock_pid age $lock_age\n";
		return -1;
	    }
	} else {
	    # Someone else has it
	    #die "lockfile busy pid $lock_pid age $lock_age\n";
	    return -1;
	}
    }
}

sub lock_release {
    my $lock_file = $_[0];
    unlink($lock_file);
    unlink("${lock_file}.pid");
    close(LOCK);
}

1;
