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

#include "job/path.hxx"
#include <stdio.h>

// Initialize our one global instance reference
job::paths & job::path = job::paths::me();

void job::paths::set_root(const std::string & root) {
    _rootdir = root;
    if (root.empty() || (root[root.size()-1] != '/')) _rootdir += "/";

    _bindir = rootdir + "usr/bin/";
    _cfgdir = rootdir + "etc/job/";
    _qcfdir = rootdir + "etc/job/qdefs/";
    _jobdir = rootdir + "var/spool/job/";
    _logdir = rootdir + "var/log/job/";
    _tmpdir = rootdir + "tmp/job/";
    _vlbdir = rootdir + "var/lib/job/";

    _cfgfile = cfgdir + "job.conf";
    _seqfile = vlbdir + "job.seq";

    _tsttmp = "./test/tmp/";
}

void job::paths::dump() const {
    fprintf(stderr, "rootdir: %s\n", rootdir.c_str());
    fprintf(stderr, "bindir:  %s\n", bindir.c_str());
    fprintf(stderr, "seqfile: %s\n", seqfile.c_str());
    fprintf(stderr, "tsttmp:  %s\n", tsttmp.c_str());
}

