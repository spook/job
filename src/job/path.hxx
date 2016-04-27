#ifndef _JOB_PATH_HXX_
#define _JOB_PATH_HXX_ 1

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

#include <string>

namespace job {

class paths {
  public:

    // Singleton lazy initializer
    static paths & me() {
        static paths myself;
        return myself;
    }

    // Set the root whenever needed
    void set_root(const std::string & root);
    void dump() const;

    // Directories - const references so they're readonly for our users
    const std::string & rootdir;    // Root dir
    const std::string & bindir;     // Executable files     /usr/bin/
    const std::string & cfgdir;     // Config files         /etc/job/
    const std::string & qcfdir;     // Queue config files   /etc/job/qdefs/
    const std::string & jobdir;     // Queues and jobs      /var/spool/job
    const std::string & logdir;     // Log files            /var/log/job
    const std::string & tmpdir;     // Temporary files      /tmp/job/
    const std::string & vlbdir;     // Persistent cache files, variable state /var/lib/job

    // Files
    const std::string & cfgfile;    // Configuraton file    /etc/job/job.conf
    const std::string & seqfile;    // Job number sequence  /var/lib/job/job.seq

    // Test Locations
    const std::string & tsttmp;     // Test temporary files

  private:
    paths()                         // hide constructor
        : rootdir(_rootdir)         // initialize const references
        , bindir(_bindir)           //  . . .
        , cfgdir(_cfgdir)
        , qcfdir(_qcfdir)
        , jobdir(_jobdir)
        , logdir(_logdir)
        , tmpdir(_tmpdir)
        , vlbdir(_vlbdir)
        , cfgfile(_cfgfile)
        , seqfile(_seqfile)
        , tsttmp(_tsttmp)
    {
        set_root("/");              // start with this, the user can change it later
    }
    paths(paths const &);           // hide copy constructor
    void operator=(paths const &);  // hide assignment

    // The actual storage
    std::string _rootdir;
    std::string _bindir;
    std::string _cfgdir;
    std::string _qcfdir;
    std::string _jobdir;
    std::string _logdir;
    std::string _tmpdir;
    std::string _vlbdir;

    std::string _cfgfile;
    std::string _seqfile;

    std::string _tsttmp;
};

// The public singleton instance called job::path
//  Use like job::path.seqfile or job::path.set_root(...)
extern job::paths & path;
}

#endif

