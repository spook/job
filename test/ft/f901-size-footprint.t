#!/usr/bin/perl -w
# Feature test: CLI basics - no jobman's running
#
use strict;
use warnings;
use FindBin qw/$Bin/;
use Test::More tests => 1;
$| = 1;

# Inits
$0 =~ m{/(f\d+)};
my $fid    = $1 || "f000";
my $root   = "$Bin/../../kit";
note "Using root $root";

# Gather component sizes
my @files = (
    "$root/usr/lib/libjob.so.1.0.0",
    "$root/usr/bin/queman",
    "$root/usr/bin/jobman",
    "$root/usr/bin/catjob",
    "$root/usr/bin/edjob",
    "$root/usr/bin/edjobq",
    "$root/usr/bin/lsjob",
    "$root/usr/bin/lsjobq",
    "$root/usr/bin/mkjob",
    "$root/usr/bin/mkjobq",
    "$root/usr/bin/mvjob",
    "$root/usr/bin/mvjobq",
    "$root/usr/bin/rmjob",
    "$root/usr/bin/rmjobq",

    # Note: the man pages do get gzip'd, so they're smaller on disk - but close enuf
    "$root/../man/job.conf.5.gz",
    "$root/../man/job.7.gz",
    "$root/../man/jobman.8.gz",
    "$root/../man/queman.8.gz",
    "$root/../man/catjob.8.gz",
    "$root/../man/edjob.8.gz",
    "$root/../man/edjobq.8.gz",
    "$root/../man/lsjob.8.gz",
    "$root/../man/lsjobq.8.gz",
    "$root/../man/mkjob.8.gz",
    "$root/../man/mkjobq.8.gz",
#    "$root/../man/mvjob.8.gz",
#    "$root/../man/mvjobq.8.gz",
    "$root/../man/rmjob.8.gz",
    "$root/../man/rmjobq.8.gz",
    );
my $total = 0;
foreach my $file (@files) {
    BAIL_OUT "*** Cannot find $file" unless -r $file;
    my $size = -s $file;
    $total += $size;
    $file =~ m{/([^/]+)$};
    my $base = $1 || '--';
    note sprintf("  %15s  %7d B", $base, $size);
}

ok $total <= 500_000, "Total size $total under 1/2 MB";
exit 0;

