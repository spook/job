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
#include "job/file.hxx"
#include "job/getopt.hxx"
#include "job/log.hxx"
#include "job/path.hxx"
#include "job/queue.hxx"
#include "job/status.hxx"
#include "job/string.hxx"
#include <dirent.h>
#include <linux/limits.h>   // PATH_MAX
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

// CLI options and usage help
enum  {opNONE, opTIME, opGRP,  opHELP, opAFF,  opLOG,  opNTFY, opPRIO,
       opQUE,  opTRY,  opROOT, opSID,  opSUB,  opTYPE, opTPFX, opVERB };
const option::Descriptor usage[] = {
    {opNONE, 0, "",  "",             Arg::None, 
        "Make a new job; i.e. enter a job into a batch queue.\n\n"
        "Usage: mkjob [options] [command [args...]]\n\n"
        "Options:" },
    {opTIME, 0, "a", "at-time",     Arg::Reqd, "  -a  --at-time      Schedule time to run the job;\n"
                                                "                       default is the next minute"},
    {opGRP,  0, "g", "group",       Arg::Reqd, "  -g  --group        Group job - provide comma-separated\n"
                                                "                       list (no spaces) of stations"},
    {opHELP, 0, "h", "help",        Arg::None, "  -h  --help         Show this help message and exit"},
    {opAFF,  0, "i", "affinity",    Arg::Reqd, "  -i  --affinity     Job affinity"},
    {opLOG,  0, "l", "log-level",   Arg::Reqd, "  -l  --log-level    Debugging log level (info, verbose, debug...)"},
    {opNTFY, 0, "n", "notify",      Arg::None, "  -n  --notify       Notify user of job updates on their tty/pts"},
    {opPRIO, 0, "p", "priority",    Arg::Reqd, "  -p  --priority     Job priority 1-9; 1=best, 5=normal, 9=slowest"},
    {opQUE,  0, "q", "queue",       Arg::Reqd, "  -q  --queue        Queue to use"},
    {opTRY,  0, "#", "try-limit",   Arg::Reqd, "  -#  --try-limit    Try limit, default is 100 tries"},
    {opROOT, 0, "R", "root",        Arg::Reqd, "  -R  --root         Set file system root"},
    {opSID,  0, "s", "station-id",  Arg::Reqd, "  -s  --station-id   Station ID of associated device, if a tied job.\n"
                                                "                       Not valid to use on an endpoint device"},
    {opTYPE, 0, "t", "type",        Arg::Reqd, "  -t  --type         Job type (in lieu of command)"},
    {opTPFX, 0, "T", "test-prefix", Arg::Reqd, "  -T  --test-prefix  For testing, process name prefix"},
    {opSUB,  0, "u", "submitter",   Arg::Reqd, "  -u  --submitter    Submitter (email or CN), defaults to this user"},
    {opVERB, 0, "v", "verbose",     Arg::None, "  -v  --verbose      Show more info"},
    {opNONE, 0, "",  "",            Arg::None, 
        "\n"
        "  mkjob submits a new job to a batch queue.\n"
        "  Use --queue to specify the queue; otherwise the default queue is used.\n"
        "  Without --type, the first argument is required and interpreted as the\n"
        "  command to execute, any additional arguments are passed to that command.\n"
        "  With --type, arguments are optional and passed as-is.\n"
        "  Job types must be defined in the queue's configuration before they can\n"
        "  be used.\n"
//        "\n"
//        "  Job affinity can be used to give a scheduling boost when a job is eligible\n"
//        "  to run in a preferred location, so that the job is more\n"
//        "  likely to run there.  For example, -i 'region=apj' says to prefer to run\n"
//        "  the job in the Asia/Pacific region.\n"
        },
    {0,0,0,0,0,0}
};

static std::string     test_prefix;     // Test prefix for process names

// Get the PID of a particular queue's jobman
//  Returns the PID, else 0 if not found
//  TODO: make this a utility function in the C and C++ API
pid_t find_jobman(const std::string & qnam) {

    DIR* dir;
    if (!(dir = opendir("/proc"))) {
        sayerror("*** Cannot open /proc; thus cannot signal jobman for immediate processing\n"
                 "\tJob will wait until next time queue is checked");
        return 0;
    }

    std::string wanted = test_prefix + "jobman " + qnam;
    struct dirent* ent;
    while ((ent = readdir(dir))) {

        // Is it a PID directory? (an integer?)
        char* endptr;
        long pid = strtol(ent->d_name, &endptr, 10);
        if (*endptr != '\0') continue;      // Not entirely a number, skip it

        // Read its cmdline
        char buf[PATH_MAX];
        snprintf(buf, sizeof(buf), "/proc/%ld/cmdline", pid);
        FILE* fp = fopen(buf, "r");
        if (!fp) continue;
        if (fgets(buf, sizeof(buf), fp) && (buf == wanted)) {

            // Found it
            fclose(fp);
            closedir(dir);
            return (pid_t)pid;
        }
        fclose(fp);
    }

    // Not found
    closedir(dir);
    return 0;
}

//
// Main entry point
//
using job::ERR_OK;
using job::str2int;
using job::join;
using job::path;
int main(int argc, const char* argv[]) {

    // Inits
    job::logger::set_level();

    // Parse CLI options
    job::getopt cli(usage, opNONE, opHELP);
    argc -= (argc>0); argv += (argc>0);         // skip prog name argv[0] if present
    int pstat = cli.parse(argc, argv, false);   // set gnu-mode to false, allows like: mkjob ls -al
    if (pstat != ERR_OK)  return pstat;
    if (cli.opts[opHELP]) return ERR_OK;

    // Set the log level from the CLI as soon as possible, do it again from the config later.
    if (cli.opts[opLOG])       job::logger::set_level(cli.opts[opLOG].arg);
    else if (cli.opts[opVERB]) job::logger::set_level("verbose");

    // Set our filesystem root
    if (cli.opts[opROOT]) path.set_root(cli.opts[opROOT].arg);

    // Check test prefix
    test_prefix = cli.opts[opTPFX] ? cli.opts[opTPFX].arg : "";
    if (test_prefix.size()) test_prefix += '-';

    // Load the config file
    job::config cfg(path.cfgfile);
    if (cfg.error) quit("*** Cannot load config: %s", cfg.error);

    // Get & check initial values (from config first, CLI overrides)
    if (!cli.opts[opLOG] && !cli.opts[opVERB] && cfg.exists("jobs", "log-level"))
        job::logger::set_level(cfg.get("jobs", "log-level", "info"));
    std::string qnam = cli.opts[opQUE]
                            ? cli.opts[opQUE].arg 
                            : cfg.get("job", "default-queue", "batch")
                            ;
    job::queue jq(qnam);
    if (!jq.exists()) {
        if (!jq.error) quit("*** No such queue '%s'", qnam);
        quit("*** Queue %s: %s", qnam, jq.error);
    }

    int prio = cli.opts[opPRIO]
                    ? str2int(cli.opts[opPRIO].arg)
                    : job::PRIORITY_DEFAULT
                    ;
    if ((prio < job::PRIORITY_MIN) || (prio > job::PRIORITY_MAX))
        quit("*** Bad --priority, must be %d to %d", job::PRIORITY_MIN, job::PRIORITY_MAX);
    if (!cli.opts[opTYPE] && (cli.args.size() == 0))
        quit("*** Provide a command or use --type");
    string command;
    if (!cli.opts[opTYPE]) {
        command = cli.args[0];
        cli.args.erase(cli.args.begin());
    }

    std::string submitter = cli.opts[opSUB]
                    ? cli.opts[opSUB].arg
                    : (getenv("USER") ? getenv("USER") : "")  // gotta guard against NULL
                    ;

    time_t rtime = cli.opts[opTIME] ? job::str2tim(cli.opts[opTIME].arg) : JOB_ASAP;
    if (rtime < 0)
        quit("*** Bad run time");
    int try_limit = cli.opts[opTRY] ? job::str2int(cli.opts[opTRY].arg)  : 100;
    if (try_limit < 1)
        quit("*** Bad try limit");

    // Set our job zone
    job::file::zone = cfg.geti("job", "zone"); 

    // Create the job
    job::file jf;
    if (jf.error) quit(jf.error);
    jf.run_time  = rtime;
    jf.queue     = qnam;
    jf.priority  = prio;
    jf.try_limit = try_limit;
    jf.submitter = submitter;
    jf.command   = command;
    jf.type      = cli.opts[opTYPE] ? cli.opts[opTYPE].arg : "";
    jf.args      = cli.args;
    jf.notify    = cli.opts[opNTFY];
    if (cli.opts[opGRP]) jf.tie_to(job::split(cli.opts[opGRP].arg, ","));
    jf.closed    = true;
    jf.uid       = getuid();    // Use caller's read UID
    jf.gid       = getgid();    // Use caller's read GID
    jf.state     = job::hold;   // Start in hold
    jf.write();                 // Create the file
    if (jf.error) die(jf.error);
    jf.state     = job::pend;   // Move back into pending
    jf.repath();                // Then move into place, under lock
        // Note: we must start the job in 'hold' state, create the file,
        //  and then move it into 'pend' state with repath() (which uses
        //  a lock).  If we do not, the jobman can pickup the job just
        //  as it is created -- empty -- and try to scheule it, before
        //  the write() finishes to populate the job info.
        //  This has been demonstrated in testing; it really occurs!
        //  So the above trick fixes this.
    if (jf.error) {
        job::status moverr = jf.error;
        std::string second = "  Please resubmit";
        jf.remove();
        if (jf.error) second = "  Cannot cleanup job file: " + jf.name()
                           + "\n  " + std::string(jf.error)
                           + "\n  Please manually remove job file.";
        quit("Failed to move job into queue: %s\n%s", moverr, second);
    }

    // Signal the job manager to check its queue again
    pid_t pid = find_jobman(qnam);
    if (pid) {
        int err = kill(pid, SIGHUP);
        err ? sayerror("*** Cannot signal jobman: %s", SYS_status)
            : saydebug("Sent SIGHUP to jobman at PID %d", pid);
    }

    // Ciao!
    say("Job %u: Submitted", jf.id);
    sayverbose("Run Time: %s", cli.opts[opTIME] ? job::tim2str(jf.run_time, true) : "-(soon)-");
    sayverbose("   Queue: %s", qnam);
    sayverbose("Job File: %s", jf.name());
    if (cli.opts[opTYPE])  sayverbose("Job Type: %s", jf.type);
    if (!cli.opts[opTYPE]) sayverbose(" Command: %s", jf.command);
    sayverbose("    Args: %s", join(jf.args));
    return ERR_OK;
}
