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

#include "job/isafe.hxx"
#include "job/seqnum.hxx"
#include <fcntl.h>
#include <linux/version.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
  #define O_CLOEXEC 0   // not available on RHEL5 & earlier
#endif

// constructors
job::seqnum::seqnum() {}
job::seqnum::seqnum(const std::string & seqfile) : value(0), filename(seqfile) {}

// destructor
job::seqnum::~seqnum() {}

// get the current number
uint64_t job::seqnum::curr() {

    // Open file (create if it does not exist)
    int fd = isafe::open(filename.c_str(),
                  O_RDWR | O_CLOEXEC | O_CREAT, // | O_NOATIME,
                  S_IRWXU | S_IRGRP | S_IROTH); // 0744
    if (fd == -1) {
        error.set("open: "+filename, IO_status);
        return value = 0;
    }

    // Exclusive lock on the file
    int ret = isafe::flock(fd, LOCK_EX);
    if (ret == -1) {
        error.set("flock", IO_status);
        isafe::close(fd);
        return value = 0;
    }

    // read prior value.  Because there's a slight chance the read
    //  can be intterrupted by a signal, and even slighter that
    //  the interrupt will be partway thru our minuscule read of 8 bytes
    //  (such as, we get only 4 bytes), we do this in a loop.
    //  Then instead of reading and reassembling the fragments, 
    //  we'll just seek back and try again from the beginning.
    //  This is a lot to do for a infinitesimal chance, but
    //  we want this module rock-solid.
    while (1) {
        ssize_t n = isafe::read(fd, &value, sizeof(value));
        if (n == -1) {
            error.set("read: "+filename, IO_status);
            isafe::close(fd);
            return value = 0;
        }
        if (!n) break;  // empty file (probably was just created)
        off_t o = isafe::lseek(fd, 0, SEEK_SET);   // reset to top
        if (o == -1) {
            error.set("lseek", IO_status);
            isafe::close(fd);
            return value = 0;
        }
        if (n == sizeof(value)) break;
    }

    // close file (unlocks it too)
    ret = isafe::close(fd);
    if (ret == -1) {
        error.set("close", IO_status);
        return value = 0;
    }

    // now serving...
    error = ERR_OK;
    return value;
}

// get the next number
uint64_t job::seqnum::next() {

    // Open file (create if it does not exist)
    int fd = isafe::open(filename.c_str(),
                  O_RDWR | O_CLOEXEC | O_CREAT, // | O_NOATIME,
                  S_IRWXU | S_IRGRP | S_IROTH); // 0744
    if (fd == -1) {
        error.set("open: "+filename, IO_status);
        return value = 0;
    }

    // Exclusive lock on the file
    int ret = isafe::flock(fd, LOCK_EX);
    if (ret == -1) {
        error.set("flock", IO_status);
        isafe::close(fd);
        return value = 0;
    }

    // read prior value.  Because there's a slight chance the read
    //  can be intterrupted by a signal, and even slighter that
    //  the interrupt will be partway thru our minuscule read of 8 bytes
    //  (such as, we get only 4 bytes), we do this in a loop.
    //  Then instead of reading and reassembling the fragments, 
    //  we'll just seek back and try again from the beginning.
    //  [We also seek to 0 here, so the below write() is an overwrite].
    //  This is a lot to do for a infinitesimal chance, but
    //  we want this module rock-solid.
    while (1) {
        ssize_t n = isafe::read(fd, &value, sizeof(value));
        if (n == -1) {
            error.set("read: "+filename, IO_status);
            isafe::close(fd);
            return value = 0;
        }
        if (!n) break;  // empty file (probably was just created)
        off_t o = isafe::lseek(fd, 0, SEEK_SET);   // reset to top
        if (o == -1) {
            error.set("lseek", IO_status);
            isafe::close(fd);
            return value = 0;
        }
        if (n == sizeof(value)) break;
    }

    // increment
    value++;

    // Save the new number.
    //  Like the read above, there's a small chance the
    //  write will fail or be partial.  But this chance is bigger,
    //  because we could be running over a distributed file system
    //  like NFS or Gluster.  A partial write there is more likely
    //  than a fractured read.  Its still a very, very small chance,
    //  but we want to be solid.
    while (1) {
        ssize_t n = isafe::write(fd, &value, sizeof(value));
        if (n == sizeof(value)) break;
        if (n == -1) {
            error.set("write: "+filename, IO_status);
            isafe::close(fd);
            return value = 0;
        }
        off_t o = isafe::lseek(fd, 0, SEEK_SET);   // reset to top
        if (o == -1) {
            error.set("lseek", IO_status);
            isafe::close(fd);
            return value = 0;
        }
    }

    // close file (unlocks it too)
    ret = isafe::close(fd);
    if (ret == -1) {
        error.set("close", IO_status);
        return value = 0;
    }

    // now serving...
    error = ERR_OK;
    return value;
}

