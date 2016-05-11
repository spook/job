/*  LGPL 2.1+

    job - the Linux Batch Facility
    (c) Copyright 2014-2016 Hewlett Packard Enterprise Development LP
    Created by Uncle Spook <spook(at)MisfitMountain(dot)org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
    USA
*/

#include "job/config.hxx"
#include "job/daemon.hxx"
#include "job/file.hxx"
#include "job/getopt.hxx"
#include "job/isafe.hxx"
#include "job/launch.hxx"
#include "job/log.hxx"
#include "job/path.hxx"
#include "job/queue.hxx"
#include "job/status.hxx"
#include "job/string.hxx"
#include <algorithm>        // std::sort
#include <errno.h>          // EAGAIN etc
#include <fcntl.h>          // open(2), close(2), etc
#include <pwd.h>            // getpwuid()
#include <signal.h>         // SIGCONT, kill(), sig_atomic_t, etc
#include <stdio.h>          // snprintf(), etc
#include <stdlib.h>         // setenv()
#include <sys/stat.h>       // open(2), close(2), etc
#include <sys/types.h>      // types for kill(), open() etc
#include <unistd.h>         // sleep()
#include <utmp.h>           // getutent() etc


using job::ERR_OK;
using job::ERR_LOCKED;
using job::ERR_MOVED;
using job::int2str;
using job::str2int;
using job::tim2str;
using job::path;
using job::qqif;
using job::trim;

// CLI options and usage help
enum  {opNONE, opDMN,  opHELP, opLOG, 
       opPOLL, opROOT, opTPFX, opTTIM, opVERB };
const option::Descriptor usage[] = {
    {opNONE, 0, "",  "",             Arg::None, 
        "Queue manager daemon; watches for queues to come and go,\n"
        "and launches or kills the individual jobman's for those queues.\n\n"
        "Usage: queman [options]\n\n"
        "Options:" },
    {opDMN,  0, "D", "no-daemonize", Arg::None, "  -D  --no-daemonize Run immediate; do not become a daemon"},
    {opHELP, 0, "h", "help",         Arg::None, "  -h  --help         Show this help message and exit"},
    {opLOG,  0, "l", "log-level",    Arg::Reqd, "  -l  --log-level    Debugging log level (info, verbose, debug...)"},
    {opROOT, 0, "R", "root",         Arg::Reqd, "  -R  --root         Set file system root"},
    {opPOLL, 0, "s", "sleep",        Arg::Reqd, "  -s  --sleep        Sleep (poll) time, seconds"},
    {opTTIM, 0, "t", "test-time",    Arg::Reqd, "  -t  --test-time    For testing, exit after time, seconds"},
    {opTPFX, 0, "T", "test-prefix",  Arg::Reqd, "  -T  --test-prefix  For testing, process name prefix"},
    {opVERB, 0, "v", "verbose",      Arg::None, "  -v  --verbose      Show more info"},
    {opNONE, 0, "",  "",             Arg::None, 
        "\n"
        "  This daemon manages the set of batch queues on your system.\n"
        "  When new queues appear, it launches a jobman daemon for that queue.\n"
        "  When a queue is deleted, it kills off the jobman that was handling\n"
        "  that queue.\n"
        "\n"
        "        init (PID 1) ---> queman\n"
        "                             |\n"
        "                             +--- jobman queue-A\n"
        "                             |\n"
        "                             +--- jobman queue-B\n"
        "                             :\n"
        "                       (and so on...)\n"
        "\n"
        },
    {0,0,0,0,0,0}
};

// Signal handler flags
static volatile sig_atomic_t run_queues = false;    // Run the queue checker
static volatile sig_atomic_t check_soon = false;    // Something changed, check again soon

// Misc globals
static std::string     test_prefix;     // Test prefix for process names
static job::stringlist known_queues;    // Queues known to be running
static job::stringlist failed_queues;   // Queues we tried to start but that failed; ignoring these

// Signal handler to re-check queues
static void signal_handler(int sig) {
    logverbose("Got signal %d...", sig);
    if (sig == SIGHUP) {
        check_soon = true;
        logverbose("  ...Reset check timers");
    }
    else if (sig == SIGTERM) {
        run_queues = false;
        logverbose("  ...Preparing to exit");
    }
    else {
        logverbose("  ...No special handling for this signal");
    }
}

// Job queue manager completion callback
static int q_done(job::launch & pad, void* ua, pid_t cpid, int cstat) {

    std::string* pqname = (std::string*)ua;

    // Show what happened
    loginfo("Queue %s: Manager terminated with %d:%d", 
                *pqname, pad.xsig, pad.xstat);

    // Remove this queue from the list of known queues
    job::remove(known_queues, *pqname);

    // Delete objects
    delete pqname;
    delete &pad;

    // Return "Identifier removed" (43) because *we* deleted the pad.
    //  To not do so would cause the job::launch code to zero the
    //  already free'd memory, which ain't so good, I reckon!
    return EIDRM;
}


// Watch for queues that come and go; launch individual job managers for each queue
void yearn_for_queues(
        std::string & jobmancmd,
        int next_check,
        int test_end) {

    // Run the checkloop - stay in here unless we're signalled to die
    int    next_reap  = 1;      // every second
    time_t now        = time(NULL);
    time_t when_check = now + 1;
    time_t when_reap  = 0;
    run_queues        = true;
    do {
        time_t now = time(NULL);
        if (test_end && (now > test_end)) break;

        // Signalled to check things again soon?
        if (check_soon) {
            check_soon = false;
            when_reap  = now;
            when_check = now;
        }

        // Reaper
        if (now >= when_reap) {
            when_reap = now + next_reap;
            job::launch::reap_zombies();
        }

        // Check for new or deleted queueus
        if (now >= when_check) {

            logverbose("Checking for changes to queues");
            when_check = now + next_check;

            job::stringlist curr_queues;
            job::status err = job::queue::get_queues(curr_queues);
            if (err) die("*** "+err);

            // What's gone?  gone = known - curr
            job::stringlist gone_queues = job::diff(known_queues, curr_queues);
            if (gone_queues.size())
                logverbose("Gone queues: %s", job::join(gone_queues, ", "));

            // Stop each queue that is now gone
            for (size_t g=0; g < gone_queues.size(); g++) {
                std::string & gone_q = gone_queues[g];
                loginfo("Queue %s: Removed, stopping queue", gone_q);

                // Find the PID for that queue's job manager
                bool found = false;
                for (job::launch::finmap_t::iterator it = job::launch::finmap.begin();
                     it != job::launch::finmap.end();
                     it++) {
                    pid_t        pid = it->first;
                    job::launch* pad = it->second;
                    if (!pad) continue;
                    if (!pad->term_ua) continue;
                    std::string* pqnam = (std::string*)pad->term_ua;
                    if (*pqnam != gone_q) continue;
                    found = true;

                    // Kill this subordinate queue manager; the q_done callback will do the rest
                    int err = kill(pid, SIGKILL);
                    if (err) {
                        logerror("Queue %s: Could not kill queue manager process %d: %s",
                                  gone_q, pid, SYS_status);
                    }
                    else {
                        logverbose("Queue %s: killed queue manager process %d", gone_q, pid);
                    }
                    break;
                }
                if (!found) logverbose("Queue %s: queue manager process already gone", gone_q);
            }

            // If gone, they're no longer on the failed list
            failed_queues = job::diff(failed_queues, gone_queues);

            // What's new?   new = curr - known - failed
            job::stringlist new_queues  = job::diff(job::diff(curr_queues, known_queues), failed_queues);
            if (new_queues.size())
                logverbose("New queues: %s", job::join(new_queues, ", "));

            // Start these new queues
            for (size_t i=0; i < new_queues.size(); i++) {
                std::string qname = new_queues[i];
                std::string qcmd  = jobmancmd  + " " + qname;

                // Launch it
                job::launch* pad = new job::launch;         // deleted in queue completion handler
                pad->command   = qcmd;
                pad->logfile   = path.logdir + "queue:" + qname + ".log";
                pad->kill_kids = true;
                pad->term_cb   = q_done;
                pad->term_ua   = new std::string(qname);    // deleted in queue completion handler
                pad->start();
                if (pad->error) {
                    logerror("Queue %s: Cannot launch: %s\n\tCommand: %s", qname, pad->error, qcmd);
                    failed_queues.push_back(qname);
                    delete pad;
                    pad = NULL;
                    continue;
                }
                known_queues.push_back(qname);
                loginfo("Queue %s: Manager started as PID %d", qname, pad->pid);
                logdebug("  Command: %s", qcmd);
            }
        }

    } while (sleep(1) || run_queues);

    // Cleanup - although we can just exit out, doing so makes
    //  memory testing (valgrind etc) difficult; so we'll be diligent
    //  here and cleanup stuff anyway.
    // -- Kill off all the job managers we started --
    for (job::launch::finmap_t::iterator it =  job::launch::finmap.begin();
                                         it != job::launch::finmap.end();
                                         ++it) {
        if (!it->second) continue;
        it->second->kill();
    }
    sleep(1);                       // Give some time for child proc's to die
    job::launch::reap_zombies();    // reap 'em; this will call the cleanup callback

    // Bye!
    if (test_end) {
        loginfo("Terminating due to test mode timeout");
    }
}

//
// Main entry point
//
int main(int argc, const char* argv[]) {

    // License
    say("Licensed under the LGPL 2.1+\n"
        "job - the Linux Batch Facility\n"
        "(c) Copyright 2014-2016 Hewlett Packard Enterprise Development LP\n"
        "Created by Uncle Spook <spook(at)MisfitMountain(dot)org>\n");

    // Inits
    job::launch::ac = argc;                     // used to change our process name
    job::launch::av = (char**)argv;             // used to change our process name
    job::logger::set_level();                   // start at default level (Info)
    job::logger::show_pid = true;               // because we're a daemon

    // Parse CLI options
    job::getopt cli(usage, opNONE, opHELP);
    argc -= (argc>0); argv += (argc>0);         // skip prog name argv[0] if present
    int pstat = cli.parse(argc, argv);
    if (pstat != ERR_OK)  return pstat;
    if (cli.opts[opHELP]) return ERR_OK;
    logassert(cli.args.size() == 0, "*** Extraneous command tokens");

    // Check test prefix
    test_prefix = cli.opts[opTPFX] ? cli.opts[opTPFX].arg : "";
    if ((test_prefix.size() > 7) ||
        (test_prefix.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                       "abcdefghijklmnopqrstuvwxyz"
                                       "0123456789") != std::string::npos)) {
        die("*** --test-prefix invalid, must match ^[A-Za-z0-9]{1,7}$");
    }
    if (test_prefix.size()) test_prefix += '-';

    // Announce ourselves
    logall("Starting queue manager as u%d:g%d%s", 
            (int)getuid(), (int)getgid(),
            test_prefix.size()? " prefix " + test_prefix : ""
          );

    // Set the log level from the CLI as soon as possible, do it again from the config later.
    if (cli.opts[opLOG])       job::logger::set_level(cli.opts[opLOG].arg);
    else if (cli.opts[opVERB]) job::logger::set_level("verbose");

    // Set our filesystem root
    if (cli.opts[opROOT]) path.set_root(cli.opts[opROOT].arg);

    // Load the overall config file
    job::config jobcfg(path.cfgfile);
    if (jobcfg.error) die("Cannot open config: %s", jobcfg.error);

    // Get & check initial values (from config first, CLI overrides)
    if (!cli.opts[opLOG] && !cli.opts[opVERB] && jobcfg.exists("jobs", "log-level"))
        job::logger::set_level(jobcfg.get("jobs", "log-level", "info"));

    // Setup the SIGHUP handler
    struct sigaction sa;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = signal_handler;
    if (sigaction(SIGHUP, &sa, NULL) == -1)
        die("*** Cannot set SIGHUP signal handler: %s", SYS_status);
    if (sigaction(SIGTERM, &sa, NULL) == -1)
        die("*** Cannot set SIGTERM signal handler: %s", SYS_status);

    // To see if we're already running, we put a lock on the overall config file.
    //  If we get the lock, then we're the only running instance.
    int fd = isafe::open(path.cfgfile.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        die("*** Cannot use '%s' as lockfile: %s", path.cfgfile, SYS_status);
    }
    int err = isafe::flock(fd, LOCK_EX | LOCK_NB);
    if (err && (SYS_errno == EWOULDBLOCK)) {
        die("*** Another jobman appears to be running - I cannot get an exclusive lock");
    }
    else if (err) {
        die("*** Unable to obtain exclusive lock on '%s': %s", path.cfgfile, SYS_status);
    }

    // Build a command to launch each jobman, 
    //  using the command that launched us as the template
    std::string mecmd = job::qqif(job::launch::av[0]);
    job::replace_all(mecmd, "queman", "jobman", 1);
    for (int i=1; i<job::launch::ac; ++i) {
        mecmd += " " + job::qqif(job::launch::av[i]);
    }

    // Get or default the CLI values we'll need when 'yearning'
    int next_check = cli.opts[opPOLL]
                        ? str2int(cli.opts[opPOLL].arg)
                        : jobcfg.geti("job",   "queue-watch-secs", 180);
    if (next_check < 1) next_check = 1; // don't let it be zero or less
    time_t now = time(NULL);
    int test_end = cli.opts[opTTIM]
                        ? now + str2int(cli.opts[opTTIM].arg)
                        : 0;

    // Deamonize
    if (!cli.opts[opDMN]) {
        if (getuid())
            quit("*** Must be root to daemonize.  To run locally, use -D and -R <rootdir>.");
        job::daemonise();
    }

    // Make us easy to find - NOTE: Cannot use cli after this point!
    job::launch::set_process_name(test_prefix + "queman");

    // Watch for new queues or removed queues
    yearn_for_queues(mecmd, next_check, test_end);

    // Done
    isafe::close(fd);
    loginfo("queman normal exit");
    return ERR_OK;
}

