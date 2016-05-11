#ifndef _JOB_DAEMON_HXX_
#define _JOB_DAEMON_HXX_ 1

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
    void daemonise();
    void change_gid(const std::string & gname);
    void change_uid(const std::string & uname);
}

/*! @file
 *   @brief Utility to daemonise a process.
 *
 * @fn void job::daemonise()
 *   @brief Makes the process a daemon.
 *
 *   Causes the process to be a daemon.
 */

#endif
