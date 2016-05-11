#!perl -w
#   LGPL 2.1+
#
#   job - the Linux Batch Facility
#   (c) Copyright 2014-2016 Hewlett Packard Enterprise Development LP
#   Created by Uncle Spook <spook(at)MisfitMountain(dot)org>
#
#   This library is free software; you can redistribute it and/or
#   modify it under the terms of the GNU Lesser General Public
#   License as published by the Free Software Foundation; either
#   version 2.1 of the License, or any later version.
#
#   This library is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#   Lesser General Public License for more details.
#
#   You should have received a copy of the GNU Lesser General Public
#   License along with this library; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
#   USA

use v5.10.0;
use strict;
use warnings;

use FindBin qw($Bin);
use Getopt::Long qw(:config no_ignore_case bundling);
use Pod::Usage;

$| = 1;

my $opts = {};
GetOptions($opts, "small-per-day|n=i",
                  "large-per-day|N=i",
                  "small-size|s=i",
                  "large-size|S=i",
                  "numdays|d=i",
                  "midway|m=s",
                  "numsp|M=i",
                  "spname|t=s",
                  "time-variation|q=i",
                  "size-variation|r=i",
                  "verbose|v",
                  "watcher|w",
                  "help|h",
) or pod2usage(2);
pod2usage(1) if $opts->{help};
exit be_the_script() if $opts->{"script"};

# Inits
my $ROOT    = $opts->{root}    || "/";
my $numdays = $opts->{numdays} || 3; # days
my $initial = $opts->{initial} || 100; # jobs
my $jrate   = $opts->{jrate}   || 1000; # jobs/hour
my $jrvar   = $opts->{jrvary}  || 10; # percent
my $grate   = $opts->{grate}   || 1; # jobs/hour
my $gsize   = $opts->{gsize}   || 1000; # jobs
my $grvary  = $opts->{grvary}  || 10; # percent
my $gsvary  = $opts->{gsvary}  || 10; # percent
my $srate   = $opts->{srate}   || 90; # percent
my $sfail   = $opts->{sfail}   || 20; # percent
my $stime   = $opts->{stime}   || 60; # seconds
my $svary   = $opts->{svary}   || 80; # percent

my $MKJOB  = "$ROOT/usr/bin/mkjob";
my $JOBMAN = "$ROOT/usr/bin/jobman";



if ($opts->{"verbose"}) {
    say "     Days: $d";
    say "     Root: $ROOT";
    say "";
}

# Create initial job batch


exit 0;

__END__

=head1 NAME

job-pump - Pump in a bunch of batch jobs.  To load-test the job subsystem.

=head1 SYNOPSIS

 job-pump [options]

 Options:
  -h  --help            Usage summary
  -M  --no-jobman       Don't start the jobman (perhaps you already have it running)
  -R  --root            Kit root.  Default is /
  -v  --verbose         Print more output

  -d  --numdays         Number of days to run.  Default is 3 days.
  -i  --initial         Pre-load this number of starter jobs before invoking jobman
  -j  --jrate           Job rate; hourly creation rate of single jobs
      --jrvary          Job rate variation, in percent.  Default is +/- 10%.
  -g  --grate           Hourly creation rate of group job bundles; default is 1
  -G  --gsize           Number of jobs in a group, default is 1000
      --grvary          Group rate variation, in percent.  Default is +/-10%
      --gsvary          Group size variation, in percent.  Default is +/-10%

  -s  --srate           Script non-retry rate, chance job will succeed on a try; default 90%
  -S  --sfail           Script fail-vs-retry rate, percent that fail; default is 20%
  -t  --stime           Script run time (implemented via sleep-ish code); default 60 seconds
  -T  --svary           Script run time variation, in percent.  Default is +/-80%

  -k  --script          Run instead as the script for the jobs we submit.
                          Makes us become the thing we run.  -s, -t and -T apply here too.
=head1 DESCRIPTION

Generates bunches of jobs to load-test the job subsystem.
More TBS...

=cut

