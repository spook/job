#ifndef _JOB_FILE_HXX_
#define _JOB_FILE_HXX_ 1

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

#include "job/multipart.hxx"
#include "job/status.hxx"
#include "job/string.hxx"
#include <map>
#include <string>
#include <sys/types.h>      // uid_t, gid_t, etc...
#include <vector>

#define JOB_ASAP 946684799  // Fri, 31 Dec 1999 23:59:59 GMT

namespace job {

typedef uint64_t id_t;      // Job ID type      XXX change this to jid_t ???
#define PRI_id_t PRIu64     // How to printf it

typedef enum {unk, hold, pend, run, tied, kill, done} state_t;  // 'kill' is NOT a state, but a dir
typedef std::map<std::string, id_t> ties_t;                     // the ties that bind... ;-)
typedef std::vector<id_t> idlist_t;

const static int PRIORITY_MIN     = 1;
const static int PRIORITY_MAX     = 9;
const static int PRIORITY_DEFAULT = 5;

std::string  state2str(const state_t s);
state_t      str2state(const std::string & s);

class file : public multipart {
  public:

    file();                                 // Create new job file
    file(const id_t & wanted_id);           // Find by ID
    file(const std::string & filepath);     // Start from existing jobfile's full pathname
    ~file();

    static int  zone;           // 0=no zones, 1-9 valid

                                // Letters below indicate where the info is encoded/stored
                                //  P = in path of jobfile
                                //  I = inode of jobfile
                                //  N = in the name of the jobfile
                                //  H = in a header of jobfile, section 0
                                //  B = in bodypart of jobfile, section 0

    std::string queue;          // P: Job queue name
    time_t      sub_time;       // I: When job submitted
    time_t      run_time;       // N: When next eligible to run
    int         priority;       // N: Job priority 1-9; 1=best, 5=default, 9=slowest
    id_t        id;             // N: My job identifier (job number with optional zone)
    id_t        mid;            // H: Job master ID if I'm a child (local or remote)
    std::string mnode;          // H: Job master node if I'm a remote child
    int         try_count;      // H: Current count of run tries
    int         try_limit;      // H: Maximum number of tries
    state_t     state;          // P: Current job state
    pid_t       pid;            // H: If running, the job's PID
    uid_t       uid;            // I: user who owns the file
    gid_t       gid;            // I: group who owns the file
    ties_t      ties;           // B: ties for the job - who its tied to and the child job id's
    std::string submitter;      // N: Who submitted the job (email or CN)
    std::string type;           // H: Job type (a name for a command template)
    std::string command;        // H: Job command
    stringlist  args;           // H: Command arguments
    bool        notify;         // H: notify submitter on their tty/pts
    bool        use_locks;      // Use file locking, eg when multiple nodes share queues

    job::status         copy(const job::file & jf);  // Copy some parts of a job::file
    std::string         find(const id_t & want_id);  // Find job filename by ID
    job::status         load();         // Load the file using the name it should be
    job::status         lock();         // Lock the file (if use_locks is true)
    std::string         name() const;   // Job file path & name
    job::status         remove();       // Remove the job file from the file system
    job::status         unlock();       // Remove the job file lock

    // Rename/move file to its proper name and place in the queue.
    //  Certain attribute changes -- run_time, priority, state --
    //  cause changes to the file path.  Whever these are changed,
    //  the file path should be updated via this function.
    job::status         repath();

    void                tie_to(std::string node);   // Set a single tied station/node
    void                tie_to(stringlist nodes);   // Set new list of tied stations/nodes
    idlist_t            tied_ids();                 // Return list of tied job IDs

    // Store the information back to disk.  Invokes repath() if needed.
    job::status         store();

    // Parse just the filename of a jobfile
    static job::status  parse(const std::string & fnam,
                              time_t & run_time,
                              int    & priority,
                              id_t   & id, 
                              std::string & submitter);
  private:
    std::string oldnam;         // Prior file name, before state/time/prio changes
    int         lockfd;         // fd used to lock the file during transitions and run
    void        _init_from_path(const std::string & filename);
};
}

/*!
 * @class job::file
 *   @brief Manages a job file
 *
 *   This class handles creation, access to, and management of job files.
 *   The job file knows what its name is, how to make itself,
 *   how to move itself between subdirectories when states change, etc.
 *
 *   This class "is-a" job::multipart file, so all those access functions
 *   are available.
 *
*/

#endif

