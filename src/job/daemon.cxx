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

#include "job/daemon.hxx"
#include "job/isafe.hxx"
#include "job/log.hxx"
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#ifndef WCONTINUED
  #define WCONTINUED 0x00000008
#endif

void job::daemonise() {

    // already a daemon?
    if (getppid() == 1) return;

    // Fork, allowing the parent process to terminate
    pid_t pid = fork();
    if (pid == -1) {
        die("failed to fork while daemonising: (%d) %s",
            SYS_errno, SYS_status);
    }
    else if (pid != 0) {

        // Original parent, just exit
        usleep(470000);   // makes log output nicer so parent output doesn't interfere
        _exit(0);
    }

    // Start a new session for the daemon
    if (setsid() == -1)
        die("failed to become a session leader while daemonising: (%d) %s",
            SYS_errno, SYS_status);

    // Fork again, allowing the step-parent process to terminate
    signal(SIGHUP, SIG_IGN);
    pid = fork();
    if (pid == -1) {
        die("failed to fork while daemonising: (%d) %s",
            SYS_errno, SYS_status);
    }
    else if (pid != 0) {

        // Step-parent
        loginfo("Daemon started, PID %d", pid);

        // Wait a moment, then see if the child is still running
        usleep(470000);
        int sts;
        isafe::waitpid(pid, &sts, WNOHANG|WUNTRACED|WCONTINUED);
        if (kill(pid, 0)) {
            loginfo("Yikes, check the logfile - it seems the daemon has died");
            _exit(1);
        }
        _exit(0);
    }

    // Set the user file creation mask to zero
    umask(0);

    // Close then reopen standard file descriptors
    isafe::close(STDIN_FILENO);

    int fd;
    if ((fd = isafe::open("/dev/null", O_RDONLY)) == -1)
        die("failed to reopen stdin while daemonising: (%d) %s",
            SYS_errno, SYS_status);

    usleep(100000);   // makes loginfo output nicer
    logdebug("Daemon is logging to syslog.");

    isafe::dup2(fd, STDOUT_FILENO);
    isafe::dup2(fd, STDERR_FILENO);
}

void job::change_gid(const std::string & gname) {
    if (gname.empty()) return;
    struct group* pgrp = getgrnam(gname.c_str());
    if (!pgrp)
        die("Could not switch to group %s: (%d) %s",
            gname, SYS_errno, SYS_status);
    if (setgid(pgrp->gr_gid))
        die("Could not switch to group %s: (%d) %s",
            gname, SYS_errno, SYS_status);
    loginfo("Switched to group %s", gname);
}

void job::change_uid(const std::string & uname) {
    if (uname.empty()) return;
    struct passwd* pswd = getpwnam(uname.c_str());
    if (!pswd)
        die("Could not switch to user %s: (%d) %s",
            uname, SYS_errno, SYS_status);
    if (setuid(pswd->pw_uid))
        die("Could not switch to user %s: (%d) %s",
            uname, SYS_errno, SYS_status);
    loginfo("Switched to user %s", uname);
}

