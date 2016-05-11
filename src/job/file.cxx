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

// Coder's Notes:
//  - The actual file isn't written or read until needed
//  - The file name is not stored, it's always derived

#include "job/base.hxx"
#include "job/file.hxx"
#include "job/isafe.hxx"
#include "job/log.hxx"
#include "job/path.hxx"
#include "job/queue.hxx"
#include "job/seqnum.hxx"
#define __STDC_FORMAT_MACROS    // Enable PRI macros
#include <inttypes.h>           // PRI macros
#include <limits.h>             // PATH_MAX
#include <stdio.h>              // snprintf(), sscanf()
#include <string.h>             // strtok()
#include <unistd.h>             // close() etc
#include <sys/file.h>           // flock()
#include <sys/stat.h>           // umask()

using job::ERR_OK;
using job::int2str;
using std::string;

int job::file::zone = 0;

std::string job::state2str(const state_t s) {
    switch (s) {
        case hold: return "hold";
        case pend: return "pend";
        case run:  return "run";
        case tied: return "tied";
        case kill: return "kill";
        case done: return "done";
        case unk:  break;
    }
    return "???";
}

job::state_t job::str2state(const std::string & s) {
    if (s == "hold") return hold;
    if (s == "pend") return pend;
    if (s == "run")  return run;
    if (s == "tied") return tied;
    if (s == "kill") return kill;
    if (s == "done") return done;
    return unk;
}

// Parse a job filename to get the embedded attributes.
//  Do not include any directory paths in the name; just the basename part only.
job::status job::file::parse(const std::string & fnam,
                             time_t & run_time,
                             int & priority,
                             id_t & id, 
                             std::string & submitter) {
    char subm[PATH_MAX];
    int amt = sscanf(fnam.c_str(), 
                     "t%" PRI_time_t".p%d.j%" PRI_id_t ".%s", 
                     &run_time, &priority, &id, subm);
    if (amt != 4)
        return status("Bad jobfile format", fnam); 
    if ((priority < PRIORITY_MIN) || (priority > PRIORITY_MAX))
        return status("Bad priority", fnam);
    submitter = subm;
    if (submitter.size() < 1)
        return status("Missing submitter", fnam);
    return ERR_OK;
}

// Create a new empty job file
job::file::file() 
    : queue("batch")
    , sub_time(time(NULL))
    , run_time(JOB_ASAP)
    , priority(5)
    , id(0)
    , mid(0)
    , try_count(0)
    , try_limit(100)
    , state(hold)
    , pid(0)
    , uid(-1)
    , gid(-1)
    , notify(false)
    , use_locks(true)
    , lockfd(-1)
{
    // Ensure we have a section 0
    if (!size()) resize(1);

    // Create a new job ID (job number [+ zone])
    seqnum q(path.seqfile);
    id = q.next();
    if (q.error) {
        error.set("Sequence number", q.error);
        return;
    }
    if ((zone < 0) ||  (zone > 9)) {
        error = "Bad zone value; must be 0 or 1-9";
        return;
    }
    if (zone) id = id*10 + zone;
    error = ERR_OK;
}

// Find and load existing job file, given its ID
job::file::file(const job::id_t & wanted_id) 
    : queue("batch")
    , sub_time(0)
    , run_time(0)
    , priority(0)
    , id(0)
    , mid(0)
    , try_count(0)
    , try_limit(100)
    , state(hold)
    , pid(0)
    , uid(-1)
    , gid(-1)
    , notify(false)
    , use_locks(true)
    , lockfd(-1)
{
    if (!size()) resize(1); // Ensure we have a section 0

    // Find the job file, if it exists
    oldnam = find(wanted_id);
    if (!oldnam.size()) {
        error.set(logstr("Cannot find nor access job %d", wanted_id));
        return;
    }

    // Start frm this file path
    _init_from_path(oldnam);
}

// Load an existing job from its full file path
job::file::file(const std::string & filepath)
    : queue("batch")
    , sub_time(0)
    , run_time(0)
    , priority(0)
    , id(0)
    , mid(0)
    , try_count(0)
    , try_limit(100)
    , state(hold)
    , pid(0)
    , uid(-1)
    , gid(-1)
    , notify(false)
    , use_locks(true)
    , oldnam(filepath)
    , lockfd(-1)
{
    if (!size()) resize(1);  // Ensure we have a section 0
    _init_from_path(filepath);
}

void job::file::_init_from_path(const std::string & filename) {
    // No need to do the realpath() I/O, plus that makes locking tricky.
    // Instead we'll require that the given filename starts at least with the queue
    // (has at least three parts).
    //
    //    // Derive info from the name alone (don't check for the file here)
    //    char fullpath[PATH_MAX+1];
    //    char* fnam = realpath(filename.c_str(), fullpath);
    //    if (!fnam) {error.set("realpath", IO_errno); return;}

    // Split apart the path
    stringlist pieces = split(filename, "/");
    size_t i = pieces.size();
    if (i < 3) {error.set("Bad jobfile path", filename); return;}
    queue = pieces[i-3];
    if (queue.empty()) {error.set("Bad queue", filename); return;}
    state = str2state(pieces[i-2]);
    if (state == unk) {error.set("Bad state", filename); return;}
    string jfbase = pieces[i-1];

    // Split apart the basename
    status e = parse(jfbase, run_time, priority, id, submitter);
    error = e ? e : ERR_OK;
}

job::file::~file() {
    if (lockfd >= 0) isafe::close(lockfd);
    lockfd = -1;
}

// Copy some attributes ... not all
//  Not copied:  id, mid, mnode, try_count, state, pid, ties, nor any loaded content (in [*][*])
job::status job::file::copy(const job::file & jf) {
    args      = jf.args;
    command   = jf.command;
    notify    = jf.notify;
    priority  = jf.priority;
    queue     = jf.queue;
    run_time  = jf.run_time;
    sub_time  = jf.sub_time;
    submitter = jf.submitter;
    try_limit = jf.try_limit;
    type      = jf.type;
    use_locks = jf.use_locks;
    uid       = jf.uid;
    gid       = jf.gid;
    return error = ERR_OK;
}

// Find and load existing job file; returns empty if cannot find (missing, permissions, etc)
static std::string found_filename;

    // Callback for following function
    static int _finder_cb(const job::file & jf, void* ua) {
        id_t wanted_id = *(id_t*)ua;
        if (jf.id != wanted_id) return 1;
        found_filename = jf.name();
        return 0;   // We found it, we're done!
    }

std::string job::file::find(const id_t & wanted_id) {

    // For all queues...
    found_filename.erase();
    stringlist qlist;
    status e = job::queue::get_queues(qlist);
    if (e) return error = e;
    for (size_t i=0; i<qlist.size(); i++) {
        job::queue q(qlist[i]);
        q.scan_queue(_finder_cb, (void*)&wanted_id);
        if (found_filename.size()) break;
    }
    return found_filename;
}

// Load a job file into ourselves
job::status job::file::load() {

    // Load by our derived name
    oldnam = name();
    job::multipart::load(oldnam);
    if (error) return error;
    uid = statbuf.st_uid;   // statbuf is in our multipart base object, filled in by load() above
    gid = statbuf.st_gid;   // statbuf is in our multipart base object, filled in by load() above

    // Extract out header section values
    if (!size()) resize(1);
    sub_time  = statbuf.st_ctime;
    command   =          (*this)[0]["Command"];
    mnode     =          (*this)[0]["Job-MNode"];
    mid       =  str2int((*this)[0]["Job-MID"]);
    pid       =  str2int((*this)[0]["Job-PID"]);
    type      =          (*this)[0]["Job-Type"];
    notify    =  str2boo((*this)[0]["TTY-Notify"]);
    try_limit =  str2int((*this)[0]["Try-Limit"]);
    try_count = exists(size()-1, "Try-Count") ? str2int((*this)[size()-1]["Try-Count"]) : 0;

    // Build the args list
    args.clear();
    for (unsigned int a = 1; ; ++a) {
        string key = "Job-Arg-" + int2str(a);
        if (!exists(0, key)) break;
        args.push_back(get(0, key));
    }


    // Read tie-lines from the body in section 0.
    //  Example tie line is "tie GAMMA 1234"
    //  The first item is the Station ID (or account number, whetever) that came in
    //    through the --groups option.  It's one of the things listed there.
    //  The second item is our job number for the child job.
    ties.clear();
    for (char* body = (char*)(*this)[0][BODY_TAG].c_str();
         char* line = strtok(body, "\n"); 
         body = NULL) {
        stringlist parts = split(line, " ");
        if (parts.size() != 3) continue;
        if (parts[0] != "tie") continue;
        ties[parts[1]] = str2int(parts[2]);
    }
    return error = ERR_OK;   
}

#ifndef O_CLOEXEC
  #define O_CLOEXEC 0
#endif
#ifndef O_NOATIME
  #define O_NOATIME 0
#endif
job::status job::file::lock() {
    if (lockfd < 0) {
        lockfd = isafe::open(oldnam.c_str(), O_RDONLY | O_CLOEXEC | O_NOATIME);
        if (lockfd == -1) {
            if (SYS_errno == ENOENT) return error = ERR_MOVED;      // Grabbed out from under us
            return error.set("repath: lockfd open", SYS_status);
        }

        int err = isafe::flock(lockfd, LOCK_EX | LOCK_NB);
        if ((err) && (SYS_errno == EWOULDBLOCK)) {
            isafe::close(lockfd);
            lockfd = -1;
            return error = ERR_LOCKED;
        }
        if (err) {
            isafe::close(lockfd);
            lockfd = -1;
            return error.set("repath: flock", SYS_status);
        }
    }
    return error = ERR_OK;
}

std::string job::file::name() const {
    char nam[PATH_MAX+1];
    std::string ss = state2str(state);
    snprintf(nam, sizeof(nam), "%s%s/%s/t%10.10" PRI_time_t ".p%d.j%7.7" PRI_id_t ".%s",
                path.jobdir.c_str(),
                queue.c_str(),
                ss.c_str(),
                run_time,
                priority,
                id,
                submitter.c_str());
    return nam;
}

job::status job::file::remove() {
    if (oldnam != "") {
        int err = isafe::remove(oldnam.c_str());
        if (err) return error.set("remove: ", SYS_status);
        oldnam.clear();
    }
    return error = ERR_OK;
}

job::status job::file::repath() {
    if (oldnam.empty()) return ERR_OK;
    std::string newnam = name();
    if (oldnam != newnam) {

        // Grab lock
        lock();
        if (error) return error;

        // Rename (move) the file
        int err = isafe::rename(oldnam.c_str(), newnam.c_str());
        if (err) return error.set("repath: rename\n"
                                  " from: "+oldnam+"\n"
                                  "   to: "+newnam+"\n"
                                  "\t", SYS_status);
        oldnam = newnam;

        // Release lock (if state not run)
        if (state != run) unlock();
    }
    return error = ERR_OK;
}

void job::file::tie_to(std::string node) {
    ties.clear();
    ties[node] = 0;
}

void job::file::tie_to(job::stringlist nodes) {
    ties.clear();
    for (stringlist::iterator it = nodes.begin(); it != nodes.end(); ++it) ties[*it] = 0;
}

job::idlist_t job::file::tied_ids() {
    idlist_t v;
    for (ties_t::iterator it = ties.begin(); it != ties.end(); ++it) v.push_back(it->second);
    return v;
}


job::status job::file::unlock() {
    if (lockfd >= 0) {
        isafe::close(lockfd);
        lockfd = -1;
    }
    return error = ERR_OK;
}

job::status job::file::write() {

    // Update section 0 header items

    // -(these come from file pathname attributes: note lowercase)-
    (*this)[0]["job-id"]     = int2str(id);
    (*this)[0]["job-state"]  = state2str(state);                     // varies over life
    (*this)[0]["job-queue"]  = queue;
    (*this)[0]["job-prio"]   = int2str(priority);                    // may vary
    // -(these are from section zero headers)-
    (*this)[0]["Command"]    = command;
    (*this)[0]["Job-MID"]    = int2str(mid);
    (*this)[0]["Job-MNode"]  = mnode;
    (*this)[0]["Job-PID"]    = (state == run)? int2str(pid) : "0";   // varies
    (*this)[0]["Job-Type"]   = type;
    (*this)[0]["TTY-Notify"] = yn2str(notify);
    (*this)[0]["Try-Limit"]  = int2str(try_limit);
    for (size_t i=0; i<args.size(); i++) {
        (*this)[0]["Job-Arg-" + int2str(i+1)] = args[i];
    }
    (*this)[0][BODY_TAG].clear();
    for (ties_t::iterator it = ties.begin(); it != ties.end(); ++it) {
        (*this)[0][BODY_TAG] += "tie " + it->first + " " + int2str(it->second) + "\n";
    }

    // Store it - the whole thing
    repath();
    if (oldnam.empty()) oldnam = name();
    mode_t oldum = umask(007);  // mode: user & group get all, others get nothing
    store(oldnam);              // write the file
    umask(oldum);               // mode: set it back
    if (error) return error.set("write "+oldnam, std::string(error));   // "error" from multipart

    // Set user and group
    if (uid && gid) {
        int ret = chown(oldnam.c_str(), uid, gid);
        if (ret) return error.set(logstr("write %s: chown(%d, %d): %s", 
                                  oldnam, (int)uid, (int)gid, SYS_status));
    }

    // Done
    return error = ERR_OK;
}

