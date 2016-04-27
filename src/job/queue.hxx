#ifndef _JOB_QUEUE_HXX_
#define _JOB_QUEUE_HXX_ 1

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

#include "job/file.hxx"
#include "job/string.hxx"
#include <dirent.h>         // scandir, alphasort
#include <map>
#include <string>
#include <time.h>

using std::string;
namespace job {

class queue {
  public:
    status error;
    string qname;

    // Fill in qlist with list of queue names
    static status get_queues(stringlist & qlist);

    // Constructor
    queue(const string & qnam);

    // Does this queue exist"
    bool        exists();

    // List of job files with state=s in the queue; if t given, must be <= t
    stringlist  get_jobs_by_state(state_t s, time_t t = 0);

    // Fill in the state for each job in the given statemap
    typedef     std::map<job::id_t, job::state_t> statemap_t;
    status      get_states_of_jobs(statemap_t & smap);

    // Like scandir(3), scan all jobs in this queue (of the desired states)
    //  If you want the full job attributes loaded, set full_load to true.
    //  Otherwise in 'jf' you get only: queue, id, run_time, priority, id, state, submitter;
    //  that is, those attributes quickly obtained from the job file's name.
    //  Of course, setting full_load to true slows down the scan considerably.
    //  Scanning continues while the callback returns non-zero.
    typedef     int (*scanfunc)(const job::file & jf, void* ua);
    status      scan_queue(scanfunc sf, 
                           void* ua = NULL, 
                           unsigned int statemask = -1, 
                           bool full_load = false);

    // return the directory path for this state
    string      dir_path(const state_t s) const;

  private:

    // Callback functions and related globals for scandir() use within
    static string time_limit;                           // for time_filter()
    static int    time_filter(const struct dirent* d);  // for scandir() filtering by time

    static size_t      ggot;                            // for job_scanner(), global counter
    static state_t     gstate;                          // for job_scanner(), global state var
    static statemap_t* gpsmap;                          // for job_scanner(), global ptr to smap
    static int    state_scanner(const struct dirent* d);  // for scandir() to match job id's
    static int    cb_scanner(const struct dirent* d);   // for scandir() to match job id's

};
}

#endif

