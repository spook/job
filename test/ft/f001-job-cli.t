#!/usr/bin/perl -w
# Feature test: job CLI basics
#
use strict;
use warnings;
use Test::More tests => 54;
$| = 1;

# TODO: Move package to own file then use it here
package TestDaemon;
    use Test::More;

sub new { #($root, $prefix)

    # Get class
    my $maker = shift;
    my $class = ref($maker) || $maker;

    # The instance
    my $this = {root => shift, 
                prefix => shift, 
                mypid => $$, 
                pid => 0,
                @_};
    $this->{queman} = "$this->{root}/usr/bin/queman";
    $this->{jobman} = "$this->{root}/usr/bin/jobman";

    # Bless & return
    bless($this, $class);
    return $this;
}

sub start {
    my $this = shift;
    BAIL_OUT "Daemon $this->{queman} doesn't exist" unless -e $this->{queman};
    BAIL_OUT "Daemon $this->{jobman} doesn't exist" unless -e $this->{jobman};

    # Fork a new process for the daemon
    $this->{pid} = fork();
    BAIL_OUT "Fork failed: $!" if !defined $this->{pid};
    if ($this->{pid}) {
        # I am the parent process
        note "New daemon started as PID $this->{pid}";
        return $this;
    }

    # Build args list; split/join in case an arg was passed as single string like "-v -x"
    my @args = ("-D", 
                "-T", $this->{prefix},
                "-R", $this->{root},
                split(/\s+/, join(q{ }, @_)));
    exec $this->{queman}, @args;
    die "*** Could not exec: $!\n\tcmd: $this->{queman} @args\n";
}

sub stop {
    my $this = shift;
    my $pid = $this->{pid};
    $this->{pid} = 0;
    return $this if !$pid;

    # try the nice way first
    kill 'TERM', $pid;
    for (1..5) {
        sleep 1;
        return $this if !kill(0, $pid); # done when it's gone
    }

    # its not stopping nicely, so use the sledgehammer
    kill 'KILL', $pid;
    return $this;
}

sub wait_for {
    my $this = shift;
    my $tmo = shift;
    if ($this->{pid}) {
        for (1..$tmo) {
            sleep 1;
            return $this if kill 'HUP', $this->{pid};
        }
    }
    die "*** Daemon not running\n";
}

sub DESTROY {
    my $this = shift;
    return if $this->{mypid} != $$;     # Because we fork, be sure we're the parent not the child
    $this->stop();
}

package main;

# Inits
my $out;
my $MKROOT = "test/tools/mkroot";
$0 =~ m{/(f\d+)};
my $fid    = $1 || "f000";
my $myq    = "batch";   # Use the default queue
my $base   = "$ENV{PWD}/test/roots";
my $root   = "$base/$fid";
my $tmpdir = "$root/tmp";
my @STATES = qw/hold pend run tied done/;
note "Using root $root";

# Make a clean test root
BAIL_OUT "*** Cannot find $base tree relative to here\n" unless -d $base;
qx(rm -fr $root);
qx($MKROOT $root);
BAIL_OUT "*** Cannot create root" if $?;

# Commands
my $MKJOBQ = "$root/usr/bin/mkjobq -R $root -T $fid";
my $LSJOBQ = "$root/usr/bin/lsjobq -R $root";
my $RMJOBQ = "$root/usr/bin/rmjobq -R $root -T $fid";
my $CATJOB = "$root/usr/bin/catjob -R $root";
my $LSJOB  = "$root/usr/bin/lsjob  -R $root";
my $MKJOB  = "$root/usr/bin/mkjob  -R $root -T $fid";
my $RMJOB  = "$root/usr/bin/rmjob  -R $root";

# Start queue/job managers
note "Starting queue & job managers";
my $daemon = new TestDaemon($root, $fid);
$daemon->start()->wait_for(3);

# Create a job in the default queue
note "Make a job";
my $file1 = "$tmpdir/run1.tmp";
$out = qx($MKJOB touch $file1);
is $?, 0, "exit status";
diag $out;
ok $out =~ m{^job (\d+): submitted$}img, "job submitted";
my $jid = $1;
is $jid, 1, "job ID";

# list the jobs, should be there
note "Check job is listed";
my ($q, $state, $prio, $time, $who) = lsj($jid);
is $q,     $myq,       "  queue";
is $state, "pend",     "  state";
is $prio,  5,          "  prio";
is $time,  '--asap--', "  time";
is $who,   $ENV{USER}, "  submitter";

# Check the queue for the pending job - XXX jobman could grab it by now...
note "Check if job is pending";
do_lsq($myq, 0, 1, 0, 0, 0, 1);

# Was the file created?
note "Wait for job to run";
for (1..5) {
    last if -e $file1;
    sleep 1;
}
ok -e $file1, "Job ran";

# Job should show done now
sleep 1;    # give time for job state to change
note "Check if jobs shows as done";
do_lsq($myq, 0, 0, 0, 0, 1, 1);

# Create second job
note "Make second job";
my $file2 = "$tmpdir/run2.tmp";
$out = qx($MKJOB -q $myq -p 3 touch $file2);
is $?, 0, "exit status";
ok $out =~ m{^job (\d+): submitted$}i, "job submitted";
$jid = $1;
is $jid, 2, "job ID";

note "Check job is listed";
($q, $state, $prio, $time, $who) = lsj($jid);
is $q,     $myq,       "  queue";
is $state, "pend",     "  state";
is $prio,  3,          "  prio";
is $time,  '--asap--', "  time";
is $who,   $ENV{USER}, "  submitter";

# Check the queue for the pending job
note "Check if job 2 is pending";
do_lsq($myq, 0, 1, 0, 0, 1, 2);

# Was the file created?
note "Wait for job to run";
for (1..5) {
    last if -e $file2;
    sleep 1;
}
ok -e $file2, "Job 2 ran";

# Jobs should show done now
sleep 1;    # give time for job state to change
note "Check if jobs show as done";
do_lsq($myq, 0, 0, 0, 0, 2, 2);


exit 0;

# List jobs
sub lsj {
    my $jid = shift;
    my $out = qx($LSJOB -a);
    is $?, 0, "job $jid: lsjob exit status";
    my ($qnam, $state, $prio, $time, $who)
        = $out =~ m{^\s*$jid\s+(\S+)\s+(\S+)\s+(\d)\s+(\S+)\s+(\S+)\s*$}mg;
    ok $qnam, "  job found";
    return ($qnam, $state, $prio, $time, $who);
}

# List queue info: Call like do_lsq("myqueue", 2, 0, 1, 0, 0, 3);
sub do_lsq {
    my $qnam = shift;
    my $out  = qx($LSJOBQ $qnam);
    is $?, 0, "lsjobq '$qnam' exit status";
    my @parts = $out =~ m{^$qnam\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)$}mg;
    ok @parts, "  queue $qnam listed";
    my @titles = @STATES;
    push @titles, "total";
    while (@titles) {
        is shift @parts, shift @_, "  correct " . shift(@titles) . " count";
    }
}
