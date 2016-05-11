#!/usr/bin/perl -w
# Feature test: CLI basics - no jobman's running
#
use strict;
use warnings;
use Test::More tests => 43;
$| = 1;

# Inits
my $out;
my $MKROOT = "test/tools/mkroot";
$0 =~ m{/(f\d+)};
my $fid    = $1 || "f000";
my $myq    = "q$fid";
my $base   = "test/roots";
my $root   = "$base/$fid";
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
my $MKJOB  = "$root/usr/bin/mkjob  -R $root";
my $RMJOB  = "$root/usr/bin/rmjob  -R $root";

# Make the queue
note "Create queue";
$out = qx($MKJOBQ $myq 2>&1);
is $?,   0,   "exit status";
is $out, q{}, "quiet";
ok -d "$root/var/spool/job/$myq", "queue dir created";
for my $state ((@STATES, "kill")) {
    ok -d "$root/var/spool/job/$myq/$state", "state dir '$state' created";
}

# List queues
note "List queues";
$out = qx($LSJOBQ);
is $?, 0, "exit status";
ok $out =~ m{^batch\s}mg, "default queue listed";
ok $out =~ m{^$myq\s}mg,  "this queue listed";
do_lsq($myq, 0, 0, 0, 0, 0, 0);

# Create a job in this queue
note "Make a job";
$out = qx($MKJOB -q $myq sleep 1);
is $?, 0, "exit status";
ok $out =~ m{^job (\d+): submitted$}i, "job submitted";
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

# Check the queue for the pending job
note "Check if job is pending";
do_lsq($myq, 0, 1, 0, 0, 0, 1);

# Remove the queue
note "Remove queue";
$out = qx($RMJOBQ -y $myq);
is $?, 0, "exit status";
ok !-d "$root/var/spool/job/$myq", "queue dir gone";

# List queues
note "Check listing of queues";
$out = qx($LSJOBQ);
is $?, 0, "exit status";
ok $out =~ m{^batch\s}mg, "default queue listed";
ok $out !~ m{^$myq\s}mg,  "this queue not listed";

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
