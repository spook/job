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

#include "job/log.hxx"
#include "job/path.hxx"
#include "job/queue.hxx"
#include <stdio.h>          // sscanf()
#include <stdlib.h>         // free()
#include <sys/stat.h>       // stat()
#include <sys/types.h>      // stat()
#include <unistd.h>         // stat()


// Returns the list of queues on the system
job::status job::queue::get_queues(stringlist & qlist) {
    qlist.clear();
    struct dirent** namelist;
    int n = scandir(path.jobdir.c_str(), &namelist, NULL, alphasort);
    if (n < 0)
        return status("scandir", SYS_errno);
    for (int i=0; i<n; i++) {
        if (namelist[i]->d_name[0] != '.') {
            qlist.push_back(namelist[i]->d_name);
        }
        free(namelist[i]);
    }
    free(namelist);
    return ERR_OK;
}

job::queue::queue(const std::string & qnam) 
    : qname(qnam) {
}

bool job::queue::exists() {
    string qtop = path.jobdir + qname + "/";
    struct stat sbuf;
    int err = stat(qtop.c_str(), &sbuf);
    error = ERR_OK;
    if (!err) return true;
    if (IO_errno != ENOENT) error = IO_status;
    return false;
}

std::string job::queue::time_limit;
int job::queue::time_filter(const struct dirent* d) {
    return d->d_name <= time_limit;
}

string job::queue::dir_path(const state_t s) const {
    return path.jobdir + qname + "/" + state2str(s) + "/";
}

// Return a list of jobfiles in the given state, with a runtime at or earlier than given,
//  and return it in sorted order (priority, time, etc)...  No quantity limit, you get 'em all!
job::stringlist job::queue::get_jobs_by_state(state_t s, time_t t) {
    stringlist jobfiles;
    struct dirent** namelist;
    if (t) time_limit = "t" + int2str(t) + ".zzz";  // zzz ensures equal times pass compare
                                                    // because the filename parts after this vary
    string qdir = dir_path(s);
    int n = scandir(qdir.c_str(), &namelist, t? time_filter : NULL, alphasort);
    if (n < 0) {
        error.set("scandir", SYS_errno);
        return jobfiles;
        }
    for (int i=0; i<n; i++) {
        if (namelist[i]->d_name[0] != '.')
            jobfiles.push_back(qdir + namelist[i]->d_name);
        free(namelist[i]);
    }
    free(namelist);
    return jobfiles;
}

// Get the states of all the jobs in the state map
job::status job::queue::get_states_of_jobs(statemap_t & smap) {

    // Clear all states to unknown
    for (statemap_t::iterator it = smap.begin();
                              it != smap.end();
                              ++it) {
        it->second = job::unk;
    }

    // Go thru all states, until we have all the requested jobs
    ggot = 0;
    size_t numwant = smap.size();
    gpsmap = &smap;
    for (int s = job::hold; s <= job::done; ++s) {
        if (ggot >= numwant) break;
        gstate = (state_t)s;
        struct dirent** namelist;
        std::string qdir = dir_path(gstate);
        int n = scandir(qdir.c_str(), &namelist, state_scanner, NULL);
        if (n < 0) return error.set("scandir("+qdir+")", SYS_errno);
        if (n > 0) return error.set("get_states_of_jobs: unexpected scandir() list");
    }
    return ERR_OK;
}

    // job match function for the above
    size_t                  job::queue::ggot = 0;
    job::state_t            job::queue::gstate;
    job::queue::statemap_t* job::queue::gpsmap = NULL;
    int job::queue::state_scanner(const struct dirent* d) {

        // Parse-out the job ID from the file name
        time_t      run_time;
        int         priority;
        id_t        id;
        std::string submitter;
        status e = job::file::parse(d->d_name, run_time, priority, id, submitter);
        if (e) return 0;

        // Interested in this ID?  Then store it's state
        if (gpsmap->count(id)) {
            ++ggot;
            (*gpsmap)[id] = gstate;
        }
        return 0;   // always, we just want the side-effect of calling this function
    }

// Calls the supplied callback function for every job in the queue.
//  This can be used, for example, to find all jobs with userargs
//  that match a some list.  The callback function would manage the
//  building of the list, in this case.
//  Callback profile: int scanfunc(const job::file & jf, void* ua);
// TODO: add bitmask or set for the states to scan; I imagine we often want
//  to ignore done jobs, or just check running jobs, etc.
static job::queue::scanfunc _sf;
static bool                 _sfdone;
static void*                _ua;
static string               _qdir;
job::status job::queue::scan_queue(
        scanfunc sf, 
        void* ua, 
        unsigned int statemask, 
        bool full_load
) {
    // Save for scandir()'s callback's use
    _ua     = ua;
    _sf     = sf;
    _sfdone = false;

    // For each state...
    for (int s = job::hold; s <= job::done; ++s) {

        if (!(statemask && (1<<s))) continue;
        state_t st = (state_t)s;
        _qdir   = dir_path(st);

        struct dirent** namelist;
        std::string qdir = dir_path(st);
        int n = scandir(qdir.c_str(), &namelist, cb_scanner, NULL);
        if (n < 0) return error.set("scandir", SYS_errno);
        if (n > 0) return error.set("get_states_of_jobs: unexpected scandir() list");
        if (_sfdone) break;
    }
    return ERR_OK;
}

    // callback function for the above
    int job::queue::cb_scanner(const struct dirent* d) {

        // Call the user's callback from within _this_ callback
        std::string fullpath = _qdir + d->d_name;
        job::file jf(fullpath);
        if (jf.error) return 0; // skip
        int ret = (_sf)(jf, _ua);
        if (!ret) _sfdone = true;
        return 0;   // always
    }


