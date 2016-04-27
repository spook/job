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

#include "job/base.hxx"
#include "job/isafe.hxx"
#include "job/launch.hxx"
#include "job/log.hxx"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wordexp.h>

// Character to send to the child process for sync purposes
#define SYNC_ACK    6       // ASCII ACK
// The time interval (in microseconds) that the child process will check for the 'go-ahead' from the parent
#define SYNC_TMO 100000     // 0.1 sec
// The maximum number of times that child will check for the 'go-ahead' from the parent
#define SYNC_MAX 7

// Statics
extern  char** environ;
int     job::launch::ac     = 0;
char**  job::launch::av     = NULL;
int     job::launch::_count = 0;
struct sigaction      job::launch::_new_action;
struct sigaction      job::launch::_old_action;
job::launch::finmap_t job::launch::finmap;
bool                  job::launch::_sigchld_handler_set = false;
volatile sig_atomic_t job::launch::needs_reaping        = 0;

// Constructor
job::launch::launch()
    : state(NEW)
    , xsig(0)
    , xstat(0)
    , niceness(0)
    , pid(0)
    , uid(0)
    , gid(0)
    , envp(NULL)
    , append(false)
    , kill_kids(false)
    , term_cb(NULL)
    , term_ua(NULL)
{
    // Setup signal handler for child process
    if (!_count++ && !_sigchld_handler_set) {
        _new_action.sa_handler = child_exit_handler;
        sigemptyset(&_new_action.sa_mask);
        _new_action.sa_flags = 0;
        int err = sigaction(SIGCHLD, &_new_action, &_old_action);
        if (err) logwarn("Could not set child signal handler: %s", SYS_errno);
        _sigchld_handler_set = !err;
    }
}

job::launch::~launch() {
    if (state == RUN) {
        logwarn("Destroying launcher for running child %d; child now unmanaged", pid);
    }
    finmap.erase(pid);
    --_count;
    logdebug("Removed child %d from process table", pid);
}

void job::launch::dump_table() {
    say("\njob::launch process map\n-----------------------\n%d items", finmap.size());
    for (finmap_t::iterator it = finmap.begin(); it != finmap.end(); it++) {
        say("  child PID %5d: %p", it->first, it->second);
    }
}

job::status job::launch::kill(const int sig) {
    if (state != RUN) return error = ERR_BADSTATE;
    if (!pid)         return error = ESRCH;    // No such process
    int err = ::kill(pid, sig);
    state = FAIL;
    return error = err ? SYS_status : ERR_OK;
}

// Start (launch) the child command.
//  Wel use fork to create a new child process, and then execvp*() to launch it.
//  We setup a signal handler for the child process so we know when it dies.
//  We add the child process id to our process ids table to track it.
//  In testing, we noticed that the child can exit before the parent has a chance
//  to add it to the process id table.  
//      (This may occur, especially if /proc/sys/kernel/sched_child_runs_first
//      is set true; but a multiprocessor system can run both simultaneously.)
//  Thus we use a pipe to synchronise the parent and child.
//  The parent will write to the pipe after updating the table;
//  the child won't launch the application until it has read from the pipe.
job::status job::launch::start() {
    error = ERR_OK;

    // Pipe to sync with the child
    int sync_fd[2];
    int err = pipe(sync_fd);
    if (err) return error.set("synch pipe", SYS_status);

    // Fork our child
    pid = fork();
    if (pid < 0) {
        pid = 0;
        return error.set("fork", SYS_status);
    }
    if (pid == 0) {

        // *** We are the child process ***

        // Set the death signal we'll get if our parent dies
        if (kill_kids) {
            int err = prctl(PR_SET_PDEATHSIG, SIGKILL);
            if (err) die("child prctl(): %s", SYS_status);
        }

        // Lets be nice and lower our priority
        //  If we can't do it (get an EPERM), keep going anyway -- ignore the return value
        if (niceness) {
            int ret __attribute__((unused)) = nice(niceness);   // This trick quiets the compiler
        }

        // Set our process name here.  Note we'll do it again for the exec*() below.
        //  But it's done here in case the child gets stuck, so we can still see it.
        if (procname.size()) set_process_name(procname);

        // Create/open the log file
        int flags = O_RDWR | O_CREAT;
        if (append)  flags |= O_APPEND;
        if (!append) flags |= O_TRUNC;
        int logfd = isafe::open(logfile.c_str(), flags, S_IRUSR | S_IWUSR);
        if (logfd < 0) die("child cannot open logfile %s: %s", logfile, IO_status);

        // Redirect stdout, stderr to the logfile or pipe
        int err = isafe::dup2(logfd, /*stdout*/1);
        if (err >= 0) err = isafe::dup2(logfd, /*stderr*/2);
        if (err < 0) die("child cannot redirect to %s: %s", logfile, IO_status);
        isafe::close(logfd);   // redirected, no longer need this fd

        // Wait for the parent to give the go ahead.
        char pipe_buf;
        int count = 0;
        isafe::close(sync_fd[1]);  // Close the write end of the sync pipe
        while (count < SYNC_MAX) {
            int n_read = isafe::read(sync_fd[0], &pipe_buf, 1);
            if ((n_read > 0) && (pipe_buf == SYNC_ACK)) break;
            usleep(SYNC_TMO);
            count++;
        }
        isafe::close(sync_fd[0]);
        if (count >= SYNC_MAX) die("child - timeout of sync from parent");

        // Set our user and group
SUPPRESS_DIAGNOSTIC_START(-Wunused-result)
        if (uid) setuid(uid);
        if (gid) setgid(gid);
SUPPRESS_DIAGNOSTIC_END

        // Build the args array for execvp(), from wordexp() results.
        //  wordexp() gives us almost what we need for execv*(),
        //  but it doesn't include the final null pointer.  Darn.
        wordexp_t parts;
        int status = wordexp(command.c_str(), &parts, 0/*flags*/);
        if (status == WRDE_BADCHAR) die("wordexp: bad characters in command");
        if (status == WRDE_NOSPACE) die("wordexp: out of memory");
        if (status == WRDE_SYNTAX)  die("wordexp: command syntax error");
        if (status)                 die("wordexp: undefined error %d", status);
        const char** argv = new const char* [parts.we_wordc+1];
        for (size_t i = 0; i < parts.we_wordc; i++) argv[i] = parts.we_wordv[i];
        argv[parts.we_wordc] = NULL;

        // To allow the process name to live thru the exec(), we'll put it in argv[0]
        std::string bin = argv[0];
        std::string pnm;
        if (procname.size()) {
            pnm = procname + ": " + argv[0];
            argv[0] = pnm.c_str();
        }

        // Replace our mind with the new binary, pass environment if given.
        envp ? execvpe(bin.c_str(), (char**)argv, envp)
             : execvp (bin.c_str(), (char**)argv);
        die("execvp*(%s): %s", argv[0],  SYS_status);   // only get here on error :-(
    }

    // *** Parent process continues here ***
    isafe::close(sync_fd[0]);      // Close the read end of the sync pipe
    finmap[pid] = this;     // Add to process table
    logdebug("Child PID %d added to process table", pid);
    state = RUN;

    // Send the ACK to the waiting child so it will run
    char buf[1];
    buf[0] = SYNC_ACK;
    ssize_t ret = isafe::write(sync_fd[1], buf, 1);
    if (ret == -1) {
        logerror("write ACK: %s", SYS_status);
        kill();
        return error.set(logmsg);
    }
    isafe::close(sync_fd[1]);     // done with pipe
    logdebug("Child PID %d given go-ahead", pid);

    return error = ERR_OK;
}

// Wait for completion, at most for the given duration.  -1 = infinite.
//  Display optional message (with the seconds count as the only param)
job::status job::launch::wait(const int duration,
                              const char* msgfmt) {
    int secs = 0;
    reap_zombies();

    while (((secs++ < duration) || (duration < 0)) && (state == RUN)) {
        if (msgfmt) {
            printf(msgfmt, secs);
            fflush(stdout);
        }
        sleep(1);
        reap_zombies();
    }
    return state == DONE ? error.set(ERR_OK)
         : state == RUN  ? error.set(ERR_TIMEOUT)
         :                 error.set(ERR_ABORT)
         ;
}

// return count of NEW or RUN child processes
size_t job::launch::running() {
    size_t n = 0;
    for (finmap_t::iterator it = finmap.begin(); it != finmap.end(); it++) {
        launch* pad = it->second;
        if (pad && ((pad->state == NEW) || (pad->state == RUN))) ++n;
    }
    return n;
}

/*! @brief Function to reap the zombies which may arise from child processes
 *
 *  When the application exits, it will send a SIGCHLD signal to the parent.
 *  It is caught by the signal handler, but we can't do the wait in the
 *  signal handler because waitpid is not safe to call within a signal handler.
 *  So we clean up the zombies here.
 */
void job::launch::reap_zombies() {
    if (!needs_reaping) return;

    needs_reaping = 0;
    logdebug("%s", __FUNCTION__);

    int cstat = 0;  // child status (core + signal + exit status)
    while (pid_t cpid = isafe::waitpid(-1, &cstat, WNOHANG)) {
        if (cpid < 0) break;    // ignore, spurious signals are common
        if (cpid == 0) {
            logdebug("Skip child non-exit state change stat=%%x%x", cstat);
            continue;  // normally won't happen with pid==-1, but maybe a child is traced
        }

        // Look for the job::launch object that made this child
        if (finmap.find(cpid) != finmap.end()) {
            job::launch* pad = finmap[cpid];

            if (WIFSIGNALED(cstat)) {
                pad->state = FAIL;
                pad->xsig  = WTERMSIG(cstat);
                pad->xstat = 0;
            }
            else {
                pad->state = DONE;
                pad->xsig  = 0;
                pad->xstat = WEXITSTATUS(cstat);
            }
            logdebug("Child %d exit sig:stat %d:%d", cpid, pad->xsig, pad->xstat);

            // Invoke the child termination handler, if defined (it's a one shot callback)
            callback tcb = pad->term_cb;
            if (tcb) {
                int ret = (tcb)(*pad, pad->term_ua, cpid, cstat);
                if (ret != EIDRM) {
                    pad->term_cb  = NULL;
                    pad->term_ua  = NULL;
                }
            }
        }
        else {
            logwarn("Lost child %d exit with sig:stat %d:%d",
                            cpid, WTERMSIG(cstat), WEXITSTATUS(cstat));
        }
    }
}

/*! @brief Signal Handler to catch when the application (child) exits
 *  @param[in]  sig The signal that we've just caught - it should be a SIGCHLD
 *
 *  When the application exits, it will send a SIGCHLD signal to the parent.
 *  We catch that here and indicate that we need to reap the zombie.
 */
void job::launch::child_exit_handler(int sig) {
    needs_reaping = 1;
}

// atexit() handler to free the duplicate argc/argv space we made in set_process_name().
static void free_environ() {
    unsigned int i = -1;
    while (environ[++i]) {
        free(environ[i]);
    }
    free(environ);
}

// Set our process name - the string we see in top or ps
void job::launch::set_process_name(const std::string & name) {

    if (!av) return;                // Not initialized, nothing we can do
    static unsigned int size = 0;   // Space we can use; also a flag that we moved the env already
    if (!size) {

        // Move the environment vars out of the way so we may steal that space
        int env_len = -1;
        if (environ) while (environ[++env_len]);    // Count to the ending NULL pointer
        if (env_len > 0)
            size = environ[env_len-1] + strlen(environ[env_len-1]) - av[0];
        else
            size = av[ac-1] + strlen(av[ac-1]) - av[0];

        if (environ) {
            char** new_environ = (char**)malloc((env_len+1) * sizeof(char*));
            unsigned int i = -1;
            while (environ[++i]) {
                new_environ[i] = strdup(environ[i]);
            }
            environ = new_environ;
            environ [env_len] = NULL;

            // Setup an exit handler to free what we alloc'd above.
            //  This is done so memory leak testing (valgrind, etc) won't 
            //  report this as lost memory.
            atexit(free_environ);
        }
    }
    if (!size) return;

    // Overwrite the argv + old environment space (for ELF binaries, they're adjacent)
    //  TODO: If not running where we use ELF, this will BLOW UP!
    //  Find some compile-time check, else run-time check.
    char* args = av[0];
    memset(args, '\0', size);
    snprintf(args, size - 1, "%s", name.c_str());
}

