#ifndef _JOB_LAUNCH_HXX_
#define _JOB_LAUNCH_HXX_ 1

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

#include "job/status.hxx"
#include "job/string.hxx"
#include <map>
#include <signal.h>
#include <string>
#include <sys/types.h>      // uid_t, gid_t, pid_t, etc...

namespace job {

class launch {
  public:

    // child process state type
    enum state_t {
        NEW = 0,
        RUN,
        DONE,
        FAIL
    };

    status error;

    launch();
    ~launch();
    status start();
    status kill(const int sig = SIGKILL);
    status wait(const int duration = -1, const char* msgfmt = NULL);

    std::string command;                // You must set this before start()
    state_t     state;                  // READONLY: process state
    int         xsig;                   // signal that terminated the child
    int         xstat;                  // exit status of child
    int         niceness;               // priority adjust (+ is lower/worse/nicer). See nice(1)(2)
    pid_t       pid;                    // my process ID
    uid_t       uid;                    // User ID to use for child
    gid_t       gid;                    // Group ID to use for child
    std::string logfile;                // name of our logfile
    std::string procname;               // process name to give to launched child
    char**      envp;                   // environment array to pass; if null, inherits it
    bool        append;                 // append to log insted of wiping it out
    bool        kill_kids;              // ...when I die

    typedef int (*callback)(launch & pad, void* ua, pid_t cpid, int cstat);
    callback    term_cb;                // child termination callback
    void*       term_ua;                // callback user arg 

    static void     dump_table();       // Dump process map table - for debugging
    static void     reap_zombies();     // Call frequently to check kids
    static size_t   running();          // Number of NEW or RUN launched processes
    static void     set_process_name(const std::string & name);
                                        // Set our process name.  Caller MUST set
    static int      ac;                 //      job::launch::ac = argc;
    static char**   av;                 //      job::launch::av = (char**)argv;
                                        // IMPORTANT: Alters original argc/argv, so ref's taken
                                        //   to them are no longer reliable; such as cli.parse()!

    // Map of child processes we've launched (may not all be running, check .state)
    typedef std::map<pid_t, launch*> finmap_t;
    static finmap_t finmap;

  private:
    static int                   _count;                 // number of active launchers
    static struct sigaction      _new_action;
    static struct sigaction      _old_action;
    static bool                  _sigchld_handler_set;   // current state of sigchld handler
    static volatile sig_atomic_t needs_reaping;

    static void                  child_exit_handler(int sig);
};
}

/*! @file
@class job::launch
  @brief Launch a command in the background

  Executes a command as a child process.
  The command is NOT passed through a shell, it is exec'd directly.
  This avoids the extra process for the shell.
  If you really need the shell, then put that as part of your command too.
  Command arguments are split out in the standard shell-way, but not via a shell.
  Control is returned to the parent while the child executes - we're asynchronous.

  Output from the child process - both stdout and stderr - is redirected 
  to the logfile specified by the caller.

  The parent may .wait() for the child to complete - a blocking operation;
  or it may periodically check the .state() of the child to see if it completed;
  or it can register a completion callback function .term_cb that will be invoked
  when the child terminates (good or bad).  The callback can be passed a arg .term_ua .

  The parent may obtain the exit status code from when the child completes.

  The parent may .kill() a running child process.

  This launch class will automatically reap the child process upon
  termination of the child, even if the specific launch object instance
  has already been destroyed.

  By default, a child process will continue to run if the parent process dies.
  To guarantee that the children will die when the parent dies for any reason,
  set the public member variable .kill_kids to true.

  You may pass an complete environment into the child process, it will replace
  the environment that would by default be inherited from the parent.
  To do this, set the public member variable .envp to an array of environment strings.
  See execvpe() for details.


@typedef typedef int(*job::launch::callback )(launch & lau, void* ua, pid_t cpid, int cstat)
  @brief Function signature for the child termination callback function.

  The signature for the callback function that is invoked when a child process terminates.
  This function should return 0, however when the callback itself deletes the launch
  object (delete &lau), then it must return EIDRM (43) "Identifier removed".

  @param lau     a reference to the launch object that started the child process
  @param ua      pointer to an arbitrary user data set at callback registration time
  @param cpid    process ID of the child the completed
  @param cstat   exit status of the child process

  @code
    int mycallback(launch & lau, void* ua, pid_t cpid, int cstat) {
        std::string* arg = (std::string*)ua;
        ...
    }
    ...
    std::string* myarg = new std::string("Some user data");
    job::launch lpad;
    lpad.command = "/usr/bin/my-app -c blah";
    lpad.logfile = "/var/log/my-app.log";
    lpad.term_cb = mycallback;
    lpad.term_ua = myarg;
    lpad.start();
    if (lpad.error) ...
     ...
  @endcode

@fn job::launch::launch()
  @brief Construct a launch object
  The object is just constructed; you should then set appropriate values
  (at least .command and .logfile) and invoke start() to run the command.

*/

#endif
