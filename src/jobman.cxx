/*
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

#include "job/base.hxx"
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
using job::ERR_ABORT;
using job::ERR_AGAIN;
using job::ERR_LOCKED;
using job::ERR_MOVED;
using job::int2str;
using job::str2int;
using job::tim2str;
using job::path;
using job::qqif;
using job::trim;

// CLI options and usage help
enum  {opNONE, opDMN,  opHELP, opLOG,  opNOLK, 
       opQUE,  opMAXR, opPOLL, opROOT, opTPFX, opTTIM, opVERB };
const option::Descriptor usage[] = {
    {opNONE, 0, "",  "",             Arg::None, 
        "Daemon to manage the jobs in one queue.\n"
        "This jobman daemon is typically launched by the queman daemon.\n\n"
        "Usage: jobman [options] queue-name\n\n"
        "Options:" },
    {opDMN,  0, "D", "no-daemonize", Arg::None, "  -D  --no-daemonize Run immediate; do not become a daemon"},
    {opHELP, 0, "h", "help",         Arg::None, "  -h  --help         Show this help message and exit"},
    {opLOG,  0, "l", "log-level",    Arg::Reqd, "  -l  --log-level    Debugging log level (info, verbose, debug...)"},
    {opNOLK, 0, "L", "no-locks",     Arg::None, "  -L  --no-locks     Do not use job file locking ***NYI***"},
    {opQUE,  0, "q", "queue",        Arg::Reqd, "  -q  --queue        Queue to manage"},
    {opMAXR, 0, "r", "run-limit",    Arg::Reqd, "  -r  --run-limit    Max jobs to run in this queue"},
    {opROOT, 0, "R", "root",         Arg::Reqd, "  -R  --root         Set file system root"},
    {opPOLL, 0, "s", "sleep",        Arg::Reqd, "  -s  --sleep        Sleep (poll) time, seconds"},
    {opTTIM, 0, "t", "test-time",    Arg::Reqd, "  -t  --test-time    For testing, exit after time, seconds"},
    {opTPFX, 0, "T", "test-prefix",  Arg::Reqd, "  -T  --test-prefix  For testing, process name prefix"},
    {opVERB, 0, "v", "verbose",      Arg::None, "  -v  --verbose      Show more info"},
    {opNONE, 0, "",  "",             Arg::None, 
        "\n"
        "  There is one jobman daemon for each queue you have defined on your system.\n"
        "  Each jobman daemon manages the jobs in a queue, moving the jobs thru\n"
        "  their states, launching the job's process when they run, and so on.\n"
        "\n"
        },
    {0,0,0,0,0,0}
};

// Eligible job comparison function for sorting jobs to run or clean
static size_t jobcmpofs = 0;    // string offset to the start of the filename part
static bool   jobcompare(std::string a, std::string b) {
    time_t a_time = 0;
    int    a_prio = 0;
    int    amt;
    amt = sscanf(a.c_str()+jobcmpofs, "t%lu.p%d.", &a_time, &a_prio);
    if (amt != 2) return false;
    time_t b_time = 0;
    int    b_prio = 0;
    amt = sscanf(b.c_str()+jobcmpofs, "t%lu.p%d.", &b_time, &b_prio);
    if (amt != 2) return false;
    return ((a_prio == b_prio) && (a_time < b_time))
        ||  (a_prio <  b_prio);
}

// Signal handler flags
static volatile sig_atomic_t run_jobs   = false;    // Run the jobs in queues
static volatile sig_atomic_t check_soon = false;    // Something changed, check again soon

// Misc globals
static std::string     test_prefix;     // Test prefix for process names

// Forward declarations
void notify_user(const std::string & user, const std::string & msg);

// Signal handler to re-check queues
static void signal_handler(int sig) {
    logverbose("Got signal %d...", sig);
    if (sig == SIGHUP) {
        check_soon = true;
        logverbose("  ...Reset check timers");
    }
    else if (sig == SIGTERM) {
        run_jobs = false;
        logverbose("  ...Preparing to exit");
    }
    else {
        logverbose("  ...No special handling for this signal");
    }
}

// Job completion callback
static int try_done(job::launch & pad, void* ua, pid_t cpid, int cstat) {

    // Since a job slot just freed-up, we should look sooner for more pending jobs.
    check_soon = true;

    // Access the job file
    job::file* jf = (job::file*)ua;  // Pointer to job file passed in
    logverbose("Job %d: try done, PID %d, sig:stat=%d:%d", jf->id, cpid, pad.xsig, pad.xstat);
    jf->load();
    if (jf->error) {
        delete &pad;
        logerror("Job %d, cannot load: %s", jf->id, jf->error);
        return EIDRM;  // see below for why this value
    }

    // Do we re-try, or be tied?
    bool retry = (jf->try_count < jf->try_limit) && (((pad.xsig == 0) && (pad.xstat == EAGAIN))
                                                || ((pad.xsig == SIGCONT) && (pad.xstat == 0)));
    bool btied = (jf->try_count < jf->try_limit) && ((pad.xsig == 0) && (pad.xstat == EINPROGRESS));

    // Append result summary of this run
    size_t n = jf->size();
    jf->resize(n+1);
    (*jf)[n]["Section"]     = "result";
    (*jf)[n]["Try-Count"]   = int2str(jf->try_count);
    (*jf)[n]["End-Time"]    = tim2str(time(NULL));
    (*jf)[n]["Exit-Signal"] = int2str(pad.xsig);
    (*jf)[n]["Exit-Status"] = int2str(pad.xstat);
    (*jf)[n]["__BODY__"]   = "";   // none
    jf->closed = !retry;

    // Based on exit signal, exit status, try count and limit,
    //  update job state.
    if (retry) jf->run_time = time(0) + 60*jf->try_count;
    if (btied) { /*XXX create tied job */ }
    jf->state = retry ? job::pend 
              : btied ? job::tied
              : /*-*/   job::done;
    (*jf)[n]["State"] = job::state2str(jf->state);
    if (jf->state == job::done) jf->run_time = time(NULL);  // Easy for housekeeper to find
    jf->write();
    if (jf->error) {
        logerror("Job %d: Cannot update job: %s", jf->id, jf->error);
        // XXX what can we do here???
    }

    // Show what happened
    if (retry)
        loginfo("Job %d: Re-queued on %d:%d.  %d/%d tries.", 
                jf->id, pad.xsig, pad.xstat, jf->try_count, jf->try_limit);
    else if (btied)
        loginfo("Job %d: Tied to remote job", jf->id);
    else if (pad.xsig == 0 && pad.xstat == 0)
        loginfo("Job %d: Complete, success.  %d/%d tries.", jf->id, jf->try_count, jf->try_limit);
    else
        loginfo("Job %d: Failed %d:%d.  %d/%d tries, will not retry", 
                jf->id, pad.xsig, pad.xstat, jf->try_count, jf->try_limit);
    if (jf->notify) {
        std::string msg = "\n" + logmsg + "\n";
        notify_user(jf->submitter, msg);
    }

    // Delete objects
    delete jf;
    jf = NULL;
    delete &pad;

    // Return "Identifier removed" (43) because *we* deleted the pad.
    //  To not do so would cause the job::launch code to zero the
    //  already free'd memory, which ain't so good, I reckon!
    return EIDRM;
}

// Break a group job into individual jobs
job::status breaking_up_is_hard_to_do(job::file* jf) {

    // TODO:  check if tied list has duplicates... (do we already handle this in the jobfile?)
    // TODO:  figure out what to do if we can't create all the child jobs,
    //          and if we fail to create any!

    logverbose("Job %d: (Group) Splitting into child jobs", jf->id);
    for (job::ties_t::iterator it = jf->ties.begin(); it != jf->ties.end(); ++it) {

        // Create new jobs
        job::file kidjob;
        if (kidjob.error) {
            logerror("  Fail creating local child job: %s", kidjob.error);
            // TODO: what else???
            continue;
        }
        kidjob.copy(*jf);
        kidjob.mid   = jf->id;
        kidjob.uid   = jf->uid;
        kidjob.gid   = jf->gid;
        kidjob.mnode = "";
        kidjob.tie_to(it->first);
        kidjob.state = job::hold;       // always start in hold state
        kidjob.write();
        if (!kidjob.error) {
            kidjob.state = job::pend;   // ...then move into pending
            kidjob.repath();
        }
        if (kidjob.error) {
            logerror("  Fail to create local child job: %s", kidjob.error);
            kidjob.remove();            // If it fails, too bad, above error covers it
            continue;
        }
        else {
            jf->ties[it->first] = kidjob.id;
            logverbose("  Tied '%s' to local child job %d", it->first, kidjob.id);
        }
    }

    loginfo("Job %d: (Group) Split into %d child jobs", jf->id, jf->ties.size());
    if (jf->notify) {
        std::string msg = "\n" + logmsg + "\n";
        notify_user(jf->submitter, msg);
    }
    jf->state  = job::tied;
    jf->closed = true;
    jf->write();
    if (jf->error) {
        logerror("Job %d: (Group) Cannot write job file: %s", jf->id, jf->error);
        // TODO: add error completion to job
        jf->state = job::done;
        jf->repath();    // XXX but can we even do this here???
        delete jf;
        jf = NULL;
        return ERR_ABORT;
    }

    // We have jobs to do, perhaps there's some available slots?
    check_soon = true;

    // Done with group job
    delete jf;
    jf = NULL;
    return ERR_OK;
}

// Check for dead or abandoned (orphan) jobs.  If any found, move them to pending.
//  They may have been left here because we were killed off,
//  or the system rebooted, or another jobman on another node died, etc.
void bring_out_yer_dead(job::queue & q) {

    logverbose("Resurrecting dead jobs...");
    int n = 0;
    job::stringlist runjobs = q.get_jobs_by_state(job::run);
    for (size_t i=0; i < runjobs.size(); i++) {

        // Grab level-1 info about the job
        job::file jf(runjobs[i]);
        if (jf.error) {
            logerror("Jobfile error: %s", jf.error);
            continue;
        }

        // Is it one of ours?  Skip it...
        bool is_mine = false;
            //go thru job::launch::finmap, (job::file*)second->term_ua is jf, check jf.id
        for (job::launch::finmap_t::iterator it =  job::launch::finmap.begin();
                                             it != job::launch::finmap.end();
                                             ++it) {
            if (!it->second) continue;
            job::file* pj = (job::file*)it->second->term_ua;
            if (!pj) continue;
            if (pj->id == jf.id) {
                is_mine = true;
                break;
            }
        }
        if (is_mine) continue;

        // Poor lil' thang!  Let's take care of it...
        jf.state = job::pend;
        jf.repath();
        if (jf.error) {
            if (jf.error == ERR_MOVED) {
                logverbose("Skip locked job %d; grabbed by another job manager", jf.id);
                continue;
            }
            if (jf.error == ERR_LOCKED) {
                logverbose("Skip locked job %d; locked by another job manager", jf.id);
                continue;
            }
            logerror("Job %s: stuck, cannot repath: %s", jf.id, jf.error);
            if (jf.notify) {
                std::string msg = "\n" + logmsg + "\n";
                notify_user(jf.submitter, msg);
            }
            continue;
        }
        ++n;
        loginfo("Job %d: found dead, resurrected to pending", jf.id);
        if (jf.notify) {
            std::string msg = "\n" + logmsg + "\n";
            notify_user(jf.submitter, msg);
        }
    }
    logverbose("  ...%d dead jobs resurrected", n);
}

// Check group job completion
void group_hug(job::queue & q) {
    logverbose("Checking tied jobs for completion...");

    // Find tied jobs
    int ndone  = 0;
    int ngroup = 0;
    job::stringlist tied_jobs = q.get_jobs_by_state(job::tied);

    // Build map to hold job states
    for (size_t i=0; i<tied_jobs.size(); ++i) {

        // Parse out the job ID from the file name
        job::file jf(tied_jobs[i]);
        if (!jf.error) jf.load();   // load it to get ties
        if (jf.error) {
            logverbose("Job %d: load: %s", jf.id, jf.error);
            continue;
        }
        ++ngroup;
        logdebug("Job %d: (Group) Has %d child jobs", jf.id, jf.ties.size());

        // Build statemap for tied jobs
        job::queue::statemap_t smap;
        for (job::ties_t::iterator it = jf.ties.begin();
                                   it != jf.ties.end();
                                   ++it) {
            // Note: if tied to job 0, that means it didn't get tied.
            //  So should we leave the parent group job open?
            ////if (!it->second) continue;  // Job not tied yet
            smap[it->second] = job::unk;
        }

        // Lookup job child job states
        q.get_states_of_jobs(smap);
        if (q.error) {
            logerror("get_states_of_jobs: %s", q.error);
            return;
        }

        // If all are done, then the tied job is also done
        //  Note: a child job on hold also holds-up the group job
        //     (the group job stays tied while the child is held)
        bool all_done = true;
        for (job::queue::statemap_t::iterator it = smap.begin();
                                              it != smap.end();
                                              ++it) {
            if (it->second != job::done) {
                all_done = false;
                break;
            }
        }
        if (!all_done) {
            logverbose("Job %d: (Group) Not all child jobs done yet", jf.id);
            continue;
        }

        jf.state = job::done;
        jf.run_time = time(NULL);
        jf.repath();
        if (jf.error && (jf.error != ERR_MOVED)) {
            logerror("Job %d: (Group) Cannot move to done: %s", jf.id, jf.error);
            continue;
        }
        ++ndone;
        loginfo("Job %d: (Group) Done (all child jobs done)", jf.id);
        if (jf.notify) {
            std::string msg = "\n" + logmsg + "\n";
            notify_user(jf.submitter, msg);
        }
    }
    logverbose("  ...%d/%d tied jobs now complete", ndone, ngroup);
}

// Housekeeping by Kelly
void kellys_kleaning_kompany(job::queue & q, const int age_clean) {

    loginfo("Begin housekeeping: max age %d secs...", age_clean);
    int n = 0;
    string donedir = q.dir_path(job::done);
    DIR* dirp = opendir(donedir.c_str());
    if (!dirp) {
        logerror("opendir: %s", SYS_status);
        return;
    }

    bool timed_out = false;
    time_t now = time(NULL);
    time_t run_limit = now + 3;     // Give ourselves only a few secs for cleaning
        // TODO: parameterized above 2s value
    struct dirent* d = NULL;
    while ((d = readdir(dirp))) {
        logdump("  Checking age of %s", d->d_name);

        // Outta time?
        if (time(NULL) > run_limit) {
            timed_out = true;
            break;
        }

        // Is it a job file?
        time_t      run_time;
        int         priority;
        job::id_t   id;
        std::string submitter;
        status e = job::file::parse(d->d_name, run_time, priority, id, submitter);
        if (e) {
            logdump("    not a job file: %s", e);
            continue;
        }

        // Old enough to purge?
        string path = donedir + d->d_name;
        if ((run_time + age_clean) < now) {
            int err = isafe::unlink(path.c_str());
            if (err) {
                logerror("Cannot cleanup job file %s: %s", path, IO_status);
            }
            else {
                ++n;
                logverbose("Purged old jobfile %s", path);
            }
        }
    }
    if (closedir(dirp)) logerror("Cannot close dir %s: %s", donedir, IO_status);
    if (timed_out) check_soon = true;  // Come back sooner!
    loginfo("  ...%d old job files purged%s", n, timed_out? " (maybe more to do later)" : "");
}

// Notify a user
void notify_user(const std::string & user, const std::string & msg) {

    struct utmp* up;
    setutent();     // Start at top of utmp file
    while ((up = getutent())) {
        if (!up->ut_user[0]) continue;
#ifdef USER_PROCESS
        if (up->ut_type != USER_PROCESS) continue;
#endif
        if (up->ut_line[0] == ':') continue;    // /etc/X11/wdm/ makes tty's like :0, so skip these
        if (user != up->ut_user) continue;      // Not my submitter

        // got a device for our user, write message to it
        char dev[6+UT_LINESIZE];                              // +5 for /dev/ and +1 for \0
        snprintf(dev, 5+UT_LINESIZE, "/dev/%s", up->ut_line); // ut_line may NOT be null terminated,
                                                              // hence we use snprintf() to limit it
        int tty;
	    if ((tty = isafe::open(dev, O_WRONLY|O_NONBLOCK, 0)) < 0) {
		    if (IO_errno == EBUSY || IO_errno == EACCES) continue;  // silent ignore these
            logwarn("Cannot notify user %s on TTY %s: %s", user, dev, IO_status);
            continue;
	    }
        int err = isafe::write(tty, msg.c_str(), msg.size());
        if (err && (IO_errno == EWOULDBLOCK)) {
            // TODO: fork() it, and let the child try for a little while before giving up
        }
        isafe::close(tty);
    }
    endutent(); // close the utmp file
}

// Run a job - a single one or a group
job::status run_a_job(const std::string & jobfilename, job::config & quecfg) {

    // Load job
    logverbose("Grabbed pending job '%s'", jobfilename);
    job::file* jf = new job::file(jobfilename);
    if (jf->error) {
        logerror("Job %d: Cannot construct %s: %s", jf->id, jobfilename, jf->error);
        // TODO: add error completion to job
        jf->state = job::done;   // XXX Can we even do this here?
        jf->repath();            // XXX ditto
        // TODO: if (error)...
        delete jf;
        jf = NULL;
        return ERR_AGAIN;
    }

    // Pre-lock before we start using it
    jf->lock();
    if (jf->error) {
        if (jf->error == ERR_MOVED) {
            logverbose("...but skipping, another job manager grabbed it first", jf->id);
            return jf->error;
        }
        if (jf->error == ERR_LOCKED) {
            logverbose("...but skipping, another job manager has it locked", jf->id);
            return jf->error;
        }
        logerror("Job %d: Cannot lock: %s", jf->id, jf->error);
        // TODO: add error completion to job
        jf->state = job::done;   // XXX Can we even do this here?
        jf->repath();            // XXX ditto
        // TODO: if (error)...
        delete jf;
        jf = NULL;
        return ERR_AGAIN;
    }

    // Slurp-in the job file
    jf->load();
    if (jf->error) {
        logerror("Job %d: Cannot load: %s", jf->id, jf->error); // TODO: really a warning
        // TODO: add error completion to job
        jf->state = job::done;
        jf->repath();
        // TODO: if (error)...
        delete jf;
        jf = NULL;
        return ERR_AGAIN;
    }

    // Group job?  Break it out here and return...
    if (jf->ties.size() >= 2) {
        return breaking_up_is_hard_to_do(jf);
    }

    // If the last section was 'output', not 'result', then it
    //  must have been a killed job, or the job manager died.
    //  So, insert a result section.
    size_t m = jf->size();
    if ((m > 1) && ((*jf)[m-1]["Section"] == "output")) {
        jf->resize(m+1);
        (*jf)[m]["Section"]     = "result";
        (*jf)[m]["State"]       = job::state2str(jf->state);    // Should always be run!
        (*jf)[m]["Try-Count"]   = int2str(jf->try_count);
        (*jf)[m]["End-Time"]    = tim2str(time(NULL));
        (*jf)[m]["Exit-Note"]   = "Job manager died or system restart";
        (*jf)[m]["Exit-Signal"] = int2str(99);
        (*jf)[m]["Exit-Status"] = int2str(EOWNERDEAD);   // 130
        (*jf)[m]["__BODY__"]    = "";           // none
    }

    // Get/Build command
    string cmd;
    if (!jf->type.size()) {

        // Build the command plus any arguments
        cmd = trim(jf->command);
        for (unsigned int a = 1; ; ++a) {
            string key = "Job-Arg-" + int2str(a);
            if (!jf->exists(0, key)) break;
            cmd += " " + qqif(jf->get(0, key));
        }
    }
    else {
        // Look up the command from the type
        cmd = quecfg.get("type:" + jf->type, "command");
        trim(cmd);
        if (cmd.empty()) {
            logerror("Job %d: type '%s' undefined", jf->id, jf->type);
            if (jf->notify) {
                std::string msg = "\n" + logmsg + "\n";
                notify_user(jf->submitter, msg);
            }
            // TODO: add error completion to job
            jf->state = job::done;
            jf->repath();
            // TODO: if (error)...
            delete jf;
            jf = NULL;
            return ERR_AGAIN;
        }

        // TODO: arg substitution
    }
    trim(cmd);
    if (cmd.empty()) {
        logerror("Job %d: Empty command", jf->id);
        // TODO: add error completion to job
        jf->state = job::done;
        jf->repath();
        if (jf->error) {
            logerror(jf->error);
        }
        delete jf;
        jf = NULL;
        return ERR_AGAIN;
    }

    // Start next header section in job file
    //  so it's ready to have output appended to it.
    // TODO: make this a func in job::file, so it appends to the file, not rewrites the whole thing
    size_t n = jf->size();
    jf->resize(n+1);
    (*jf)[n]["Section"]    = "output";
    (*jf)[n]["Try-Count"]  = int2str(++jf->try_count);
    (*jf)[n]["Start-Time"] = tim2str(time(NULL));
    (*jf)[n]["__BODY__"]   = "\n";
    jf->closed = false;

    // Setup job environment
    //  Note: we do it here, instead of in the envp, so the command expansion (wordexp())
    //  also has the environment available.  It's ok to do it here b/c we replace this 
    //  whole set every time thru.
    // Note also that we (jobman) *should* be launched as a daemon,
    //  so we'll already have a reduced set of envvars.
    setenv("JOB_FILE",      jf->name().c_str(), 1);
    setenv("JOB_ID",        int2str(jf->id).c_str(), 1);
    setenv("JOB_MASTER_ID", jf->mid ? int2str(jf->mid).c_str() : "", 1);
    setenv("JOB_PRIORITY",  int2str(jf->priority).c_str(), 1);
    setenv("JOB_QUEUE",     jf->queue.c_str(), 1);
    setenv("JOB_RUN_AT",    tim2str(jf->run_time).c_str(), 1);
    setenv("JOB_STATE",     state2str(jf->state).c_str(), 1);    // will always be "run" here!
    setenv("JOB_SUBMITTER", jf->submitter.c_str(), 1);
    setenv("JOB_SUBSTATUS", jf->substatus.c_str(), 1);
    setenv("JOB_TRY_COUNT", int2str(jf->try_count).c_str(), 1);
    setenv("JOB_TRY_LIMIT", int2str(jf->try_limit).c_str(), 1);
    setenv("JOB_TYPE",      jf->type.c_str(), 1);
        // TODO: lots more, such as:
        // JOB_ARG_n (is it sufficient that it's passed in the argv[] array??)
        // JOB_COMMAND (see above, it would be argv[0], but does set process name bork this? TODO)

    // Additional environment stuff, based on the user who submitted this job
    struct passwd* pwinfo = getpwuid(jf->uid);
SUPPRESS_DIAGNOSTIC_START(-Wunused-result)
    if (pwinfo) {
        chdir(pwinfo->pw_dir);
        setenv("HOME",  pwinfo->pw_dir, 1);
        setenv("PWD",   pwinfo->pw_dir, 1);
        setenv("SHELL", pwinfo->pw_shell, 1);
        setenv("USER",  pwinfo->pw_name, 1);
    }
    else {
        chdir("/tmp");
        setenv("HOME",  "/tmp", 1);
        setenv("PWD",   "/tmp", 1);
        setenv("SHELL", "", 1);
        setenv("USER",  "", 1);
    }
SUPPRESS_DIAGNOSTIC_END

    // Move to RUN state
    jf->state = job::run;
    jf->write();    // remember, write() does a repath() too if needed
    if (jf->error) {
        logerror("Job %d: Cannot write job file: %s", jf->id, jf->error);
        // TODO: add error completion to job
        jf->state = job::done;
        jf->repath();    // XXX but can we even do this here???
        delete jf;
        jf = NULL;
        return ERR_ABORT;
    }

    // Now that the jobfile is written, we no longer need to keep
    //  the loaded part of the file in memory.  But we do want to 
    //  keep the job::file object since it has fd's and such that
    //  _do_ matter.  It's the vector of maps we don't need anymore.
    jf->clear();

    // Launch it
    job::launch* pad = new job::launch; // deleted in child completion handler
    pad->command   = cmd;
    pad->niceness  = jf->priority;      // Maps nicely, eh? ;-)
    pad->logfile   = jf->name();        // append to our own job file
    pad->procname  = "job " + int2str(jf->id);
    pad->append    = true;
    pad->kill_kids = true;
    pad->term_cb   = try_done;
    pad->term_ua   = jf;                // deleted in child handler
    pad->uid       = jf->uid;
    pad->gid       = jf->gid;

    pad->start();
    if (pad->error) {
        logerror("Job %d: Cannot launch: %s\n\tCommand: %s", jf->id, pad->error, cmd);
        if (jf->notify) {
            std::string msg = "\n" + logmsg + "\n";
            notify_user(jf->submitter, msg);
        }
        // TODO: add error completion to job
        jf->state = job::done;
        jf->repath();
        //XXX if (jf->error) ...
        delete jf;
        jf = NULL;
        delete pad;
        pad = NULL;
        return ERR_ABORT;
    }
    jf->pid = pad->pid;
    loginfo("Job %d: Started as PID %d", jf->id, pad->pid);
    if (jf->notify) {
        std::string msg = "\n" + logmsg + "\n";
        notify_user(jf->submitter, msg);
    }
    logdebug("  Command: %s", cmd);
    return ERR_OK;
}

// Look for jobs to run
void solicit_on_the_street(job::queue & q, const size_t maxjobs, job::config & quecfg) {
    logverbose("Soliciting queue %s for work...", q.qname);

    // Do we have room to take on work?
    int nrun = job::launch::running();
    size_t need = maxjobs - nrun;
    if (need <= 0) {
        logverbose("  Queue %s: %d/%d running jobs", q.qname, nrun, maxjobs);
        return;
    }

    // Job selection - go thru pending jobs, find most eligible
    job::stringlist pendjobs = q.get_jobs_by_state(job::pend, time(0));
    logverbose("  Queue %s: %d/%d slots running, %d waiting to run", 
                q.qname, nrun, maxjobs, pendjobs.size());
    if (pendjobs.empty()) return;

    // Sort by priority, then take what we need off the top
    jobcmpofs = q.dir_path(job::pend).size();
    std::sort(pendjobs.begin(), pendjobs.end(), jobcompare);
    for (size_t i=0; (i < pendjobs.size()) && (i < need); i++) {

        // Let's go to work...
        int err = run_a_job(pendjobs[i], quecfg);
        if ((err == ERR_MOVED) ||
            (err == ERR_LOCKED) ||
            (err == ERR_AGAIN)) {
            // grabbed by another jobman, so we can take another job
            need++;
        }
    }
}

// Killing-off (cancel) our own jobs marked with a kill file.
void terminate_with_predjudice(job::queue & q) {

    logverbose("Assassinating marked jobs...");
    int n = 0;
    std::string killdir = q.dir_path(job::kill);    // contains kill files, not job files
    DIR* dirp = opendir(killdir.c_str());
    if (!dirp) {
        logerror("opendir: %s", SYS_status);
        return;
    }

    bool timed_out = false;
    time_t now = time(NULL);
    time_t run_limit = now + 2;     // Give ourselves only 1-2 secs for killing
        // TODO: parameterized above 2s value
    struct dirent* d = NULL;
    while ((d = readdir(dirp))) {

        // Outta time?
        if (time(NULL) > run_limit) {
            timed_out = true;
            break;
        }

        // A kill file's name is just the job number; if not that format then skip
        job::id_t jid = str2int(d->d_name);
        if (!jid) continue;
        logdump("  Found kill order for job %d", jid);

        // It must be one of ours, if not, then skip
        //(go thru job::launch::finmap, (job::file*)second->term_ua is jf, check jf.id)
        pid_t pid = 0;
        for (job::launch::finmap_t::iterator it =  job::launch::finmap.begin();
                                             it != job::launch::finmap.end();
                                             ++it) {
            if (!it->second) continue;
            job::file* pj = (job::file*)it->second->term_ua;
            if (!pj) continue;
            if (pj->id == jid) {
                pid = it->first;
                break;
            }
        }
        if (!pid) {
            logdump("  Job %d: not ours, skipping", jid);
            continue;
        }

        // Remove the kill file
        std::string fullpath = killdir + d->d_name;
        if (isafe::unlink(fullpath.c_str())) {
            logerror("  Cannot cleanup kill file %s: %s", fullpath, IO_status);
        }


        // Hello, My name is Inigo Montoya... Prepare to die
        // Apologies to https://www.youtube.com/watch?v=6JGp7Meg42U
        loginfo("Job %d: Assassinating (PID %d)", jid, pid);
        int err = kill(pid, SIGTERM);
            // Note: this will cause our SIGCHLD handler to finish it off,
            //  so we don't have to.
        if (err) {
            logerror("Job %d: is Na'vi, they are very hard to kill: %s", jid, SYS_status);
            // what else can we do?
        }
        else {
            ++n;
        }
    }
    if (closedir(dirp)) logerror("Cannot close dir %s: %s", killdir, IO_status);
    logverbose("  ...%d jobs singin' to da fishies%s", n, timed_out? " (more to kill later)" : "");
}


// Main work loop - periodically do various tasks
void will_work_for_food(job::queue  & q, 
                        job::config & quecfg,
                        int maxjobs,
                        int next_poll,
                        int test_end) {

    // Setup intervals for time-based things, and some limits
    time_t now = time(NULL);
    int age_clean  = 30*86400;      // max thirty days old
    int next_reap  = 1;             // every second
    int next_group = 15;            // four times a minute
    int next_dead  = 180;           // check for dead jobs every 3 mins
    int next_kill  = 30;            // twice a minute
    int next_clean = 12*3600;       // twice a day
    time_t when_reap  = 0;
    time_t when_dead  = now + 1;
    time_t when_poll  = now + 3;
    time_t when_group = now + 4;
    time_t when_kill  = now + 6;
    time_t when_clean = now + 13;

    // Run the work loop - stay in here unless we're signalled to die
    run_jobs = true;
    do {
        time_t now = time(NULL);
        if (test_end && (now > test_end)) break;

        // If we're signalled, then make 'em fire sooner than they normally would
        if (check_soon) {
            check_soon = false;
            when_reap  = now;
            if (when_poll  > (now +    3)) when_poll  = now +    3;
            if (when_group > (now +   10)) when_group = now +   10;
            if (when_kill  > (now +   15)) when_kill  = now +   15;
            if (when_dead  > (now +   59)) when_dead  = now +   59;
            if (when_clean > (now + 1800)) when_clean = now + 1800;
        }

        // Reaper
        if (now >= when_reap) {
            when_reap = now + next_reap;
            job::launch::reap_zombies();
        }

        // Check for dead/abandoned jobs (skipping ours of course).
        if (now >= when_dead) {
            when_dead = now + next_dead;
            bring_out_yer_dead(q);
        }

        // Kill-off cancelled jobs
        if (now >= when_kill) {
            when_kill = now + next_kill;
            terminate_with_predjudice(q);
        }

        // Housekeeper - check every day for done jobs X days old (30?)
        if (now >= when_clean) {
            when_clean = now + next_clean;
            kellys_kleaning_kompany(q, age_clean);
        }

        // Check-up on group jobs - have all child jobs finished?
        if (now >= when_group) {
            when_group = now + next_group;
            group_hug(q);
        }

        // Any room for more jobs?
        if (now >= when_poll) {
            when_poll = now + next_poll;
            solicit_on_the_street(q, maxjobs, quecfg);
        }

    } while (sleep(1) || run_jobs);
    if (test_end) loginfo("Terminating due to test mode timeout");
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
    logassert(cli.args.size() == 1, "*** Missing QUEUE, please provide one queue name");

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
    std::string qname = cli.args[0];
    logall("Starting job manager for queue %s as u%d:g%d%s", 
            qname,
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
    if (jobcfg.error) die("Cannot open job config: %s", jobcfg.error);

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


    // Verify this queue exists
    //  ***TODO***

    // Per-queue inits
    std::string qcf = path.qcfdir + qname + ".conf";    // qcf = queue config file
    job::config quecfg(qcf);
    if (quecfg.error) die("Cannot open queue config: %s", quecfg.error);
    job::queue q(qname);

    // Get CLI values or defaults
    time_t now = time(NULL);
    int maxjobs = cli.opts[opMAXR]
                        ? str2int(cli.opts[opMAXR].arg)
                        : quecfg.geti("queue", "run-limit", 
                          jobcfg.geti("job",   "run-limit", 
                          10));
    int next_poll = cli.opts[opPOLL]
                        ? str2int(cli.opts[opPOLL].arg)
                        : quecfg.geti("queue", "poll-secs", 
                          jobcfg.geti("job",   "poll-secs", 
                          60));
    if (next_poll < 1) next_poll = 1; // don't let it be zero or less
    int test_end = cli.opts[opTTIM]
                        ? now + str2int(cli.opts[opTTIM].arg)
                        : 0;

    // Make us easy to find - NOTE: Cannot use cli after this point!
    job::launch::set_process_name(test_prefix + "jobman " + qname);

    // Run the work loop
    will_work_for_food(q, quecfg, maxjobs, next_poll, test_end);

    // Ciao!
    loginfo("Jobman %s normal exit", qname);
    return ERR_OK;
}
