#!/usr/bin/perl -w
use strict;
use Getopt::Std;
use Env;
use File::Basename;
use File::Path;
use FindBin qw($Bin);

$| = 1;

### Don't use the test harness - for some reason our tests that use pipes sometimes
###   cause the harness to hang when exiting an individual test.  May have to do with
###   an unclosed stderr???
###use Test::Harness;
###runtests(glob "test/*.t");

#$ENV{ftp_proxy}   = "";
#$ENV{http_proxy}  = "";
#$ENV{https_proxy} = "";
#$ENV{FTP_PROXY}   = "";
#$ENV{HTTP_PROXY}  = "";
#$ENV{HTTPS_PROXY} = "";

my %opt;
my @files = ();
my $start = time;
my $opt1;
my $opt2;

if ($^O ne 'MSWin32') {
    getopts("rxtsuvVoz", \%opt);
}
else {
    $opt1 = $ARGV[0];
    $opt2 = $ARGV[1];
}

my ($reset, $good, $bad, $fair) = (q{}, q{}, q{}, q{});
if ($opt{z}) {
    require Term::ANSIColor;
    $reset = Term::ANSIColor::color('reset');
    $good  = Term::ANSIColor::color('green');
    $bad   = Term::ANSIColor::color('red');
    $fair  = Term::ANSIColor::color('blue');
}

if ($^O ne 'MSWin32') {
    die "*** system has an imported digital badge B (in ~/.rda) this would cause many tests to fail"
        if (-d $ENV{"HOME"} . "/.rda");

    my $ntp_running = qx{ps -ef | grep ntp | grep -v grep};
    if (length $ntp_running <= 0) {
        print "********** Ntp is not running. Ensure ntp is running**********\n";
    }

    $ENV{LD_LIBRARY_PATH} = "$Bin/../kit/usr/lib";
    my $lsb = qx{lsb_release -a};
    $ENV{PERLLIB} = "$Bin/../test/integration/rhel5"
        if (($lsb =~ m/Distributor ID:\s+RedHat/i) && ($lsb =~ m/Release:\s+5/i));
}    # 'MSWin32'

# Return the valgrind command we'll use
sub get_valgrind_cmd {
    my ($tfile) = @_;
    my $f = "valgrind --leak-check=full --free-fill=0xff --suppressions=./build/valgrind.supp ";
    if ($opt{V}) {
        $f .= "--xml=yes --xml-file=valgrind/" . basename($tfile) . ".memcheck ";
        mkpath("valgrind") unless (-d "valgrind");
    }
    $f .= $tfile;
    return $f;
}

sub shuffle {
    my $array = shift;    # $array is a reference to an array
    my $i    = @$array;
    while ($i--) {
        my $j = int rand($i + 1);
        @$array[$i, $j] = @$array[$j, $i];
    }
}

my $ntests  = 0;
my $nfailed = 0;

if ($^O ne 'MSWin32') {
    # Determine the list of tests to run
    if ($opt{t}) {
        push(@files, sort glob("$Bin/integration/*.t"));
        push(@files, sort glob("$Bin/integration/src/bin/*.tx"));
    }
    elsif ($opt{s}) {
        push(@files, sort glob("$Bin/system/*.t"));
        push(@files, sort glob("$Bin/system/src/bin/*.tx"));
    }
    elsif ($opt{u}) {
        push(@files, sort glob("$Bin/ft/*.t"));
        push(@files, sort glob("$Bin/ft/src/bin/*.tx"));
    }
    elsif ($opt{x} || $opt{v}) {
        push(@files, sort glob("$Bin/unit/bin/*.tx"));
    }
    elsif (@ARGV) {
        foreach my $pat (@ARGV) {
            push(@files, sort glob($pat));
        }
    }
    else {
        push(@files, sort glob("$Bin/unit/bin/*.tx"));
        push(@files, sort glob("$Bin/integration/*.t"));
        push(@files, sort glob("$Bin/integration/src/bin/*.tx"));
        push(@files, sort glob("$Bin/ft/*.t"));
    }
    if ($opt{o}) {
        open(RESULT_OUT, '>', "$Bin/testall.out") or die "Could not open file 'testall.out' $!";
    }
}
else {

    # Determine the list of tests to run on Windows
    if ($opt1 && $opt2 && $opt1 eq '-x' && $opt2 eq 'x86') {
        my $ut_x86 = "$Bin/unit/bin/x86/unit-tests.exe";
        # check if the unit test binary exists
        if (-e $ut_x86) {
            my @files_tmp = qx($ut_x86);
            chomp(@files_tmp);
            push(@files, sort @files_tmp);
        }
    }
    elsif ($opt1 && $opt2 && $opt1 eq '-x' && $opt2 eq 'x64') {
        my $ut_x64 = "$Bin/unit/bin/x64/unit-tests.exe";
        # check if the unit test binary exists
        if (-e $ut_x64) {
            my @files_tmp = qx($ut_x64);
            chomp(@files_tmp);
            push(@files, sort @files_tmp);
        }
    }
    elsif ($opt1 && $opt2 && $opt1 eq '-x' && $opt2 eq 'all') {
        my $ut_x86 = "$Bin/unit/bin/x86/unit-tests.exe";
        my $ut_x64 = "$Bin/unit/bin/x64/unit-tests.exe";

        if (-e $ut_x86) {
            my @files_tmp = qx($ut_x86);
            chomp(@files_tmp);
            push(@files, sort @files_tmp);
        }

        if (-e $ut_x64) {
            my @files_tmp = qx($ut_x64);
            chomp(@files_tmp);
            push(@files, sort @files_tmp);
        }
    }
    else {
        print "\nUsage:
     -o  save test output to test/testall.out file
     -x  run unit tests
     -t  run integration tests
     -s  run system tests only (unit tests will not be executed!)
     -u  run feature tests
     -z  colorize output
     -v  run basic valgrind
     -V  run valgrind with memcheck output
     -r  randomize order of test\n";
        exit;
    }
}

# Run each test
shuffle(\@files) if $opt{r};
my $at = 0;
my $of = @files;
foreach my $tfile (@files) {
    print "Test ".++$at."/$of $tfile - ";
    $tfile = get_valgrind_cmd($tfile) if (($tfile =~ m/\.tx$/) && ($opt{v} || $opt{V}));
    my @output = qx($tfile 2>&1);
    if ($opt{o}) {
        print RESULT_OUT "\n\n---------- Test $tfile ----------\n\n";
        print RESULT_OUT @output;
    }
    my @ok   = grep {$_ =~ /^ok/} @output;
    my @skip = grep {$_ =~ /^ok \d+ # skip /i} @output;
    my @fail = grep {$_ =~ /^(?!.*# TODO)^not ok/}
    @output;    # Emulate Test::Harness by ignoring TODO failures
    if (@fail) {
        my $n = @ok + @fail;
        my $f = @fail;
        my $p = int((($n - $f) / $n) * 10000) / 100;
        print "$bad$p\%$reset\n";
        print "\t" . join("\t", @fail) . "\n";
        ++$nfailed;
    }
    elsif ($?) {
        print "$bad*** crashed: $?, $!\n$reset\n";
        ++$nfailed;
    }
    elsif (@skip) {
        print "${fair}(skip) 100%$reset\n";
    }
    else {
        if ($opt{v}) {
            my @err = grep {
                $_
                    =~ /( lost: \d+ bytes in \d+ blocks)|( ERROR SUMMARY: \d+ errors from \d+ contexts)/
            } @output;
            @err = grep {$_ !~ /lost: 0 bytes in 0 blocks/} @err;
            @err = grep {$_ !~ / ERROR SUMMARY: 0 errors from 0 contexts/} @err;
            map {$_ =~ s/^==\d+==/valgrind - /} @err;
            map {$_ =~ s/ERROR SUMMARY/     ERROR SUMMARY/} @err;
            map {$_ =~ s/ \(suppressed: \d+ from \d+\)//} @err;
            if (@err) {
                my $n = @ok + @err;
                my $f = @err;
                my $p = int((($n - $f) / $n) * 10000) / 100;
                print "$bad$p\%$reset\n";
                print "\t" . join("\t", @err);
                ++$nfailed;
            }
            else {
                print "${good}100%$reset\n";
            }
        }
        else {
            print "${good}100%$reset\n";
        }
    }
    ++$ntests;
}
if ($opt{o}) {close RESULT_OUT}

# Summary
my $duration = time - $start;
die "\nNo tests to run\n" if (!$ntests);
if ($nfailed) {
    my $pcnt = int((($ntests - $nfailed) / $ntests) * 10000) / 100;
    die "\n${bad}Failed $nfailed out of $ntests tests ($duration s): $pcnt$reset\%\n";
}
print "\n${good}All $ntests tests passed ($duration s): 100%$reset\n";
exit 0;
