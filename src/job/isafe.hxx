#ifndef _JOB_ISAFE_HXX_
#define _JOB_ISAFE_HXX_ 1

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

#include <fcntl.h>      // open(), creat()
#include <sys/file.h>   // flock()
#include <sys/types.h>  // types for system functions
#include <unistd.h>     // read(), write(), lseek(), close()

// Even tho this is part of the job:: subsystem, and would normally be put into
//  the job:: namespace, these read better as part of the isafe:: namespace.
//  For example, isafe::flock() reads as "interrupt-safe flock()".
namespace isafe {
    extern unsigned int busy_try_limit;
    extern unsigned int busy_try_delay;

    int     close(int fd);
    int     creat(const char* pathname, mode_t mode);
    int     dup2(int oldfd, int newfd);
    int     flock(int fd, int operation);
    off_t   lseek(int fd, off_t offset, int whence);
    int     open(const char* pathname, int flags);
    int     open(const char* pathname, int flags, mode_t mode);
    ssize_t read(int fd, void* buf, size_t count);
    int     remove(const char* pathname);
    int     rename(const char* oldpath, const char* newpath);
    int     unlink(const char* pathname);
    pid_t   waitpid(pid_t pid, int* status, int options);
    ssize_t write(int fd, const void* buf, size_t count);
}

/*!
@file job::isafe.hxx
 @brief Interrupt-safe (signal-safe) flavors of system functions.
    The functions simply loop and retry if they return EINTR.
    They are also safe-ish on slow sockets, such as a distributed file 
    system, when the FS returns EBUSY then these functions wait a short
    time (default 0.25s) and retry, up to a retry limit (default 20 tries).
    The externs busy_try_delay and busy_try_limit set these values.

    Why use these instead of setting the SA_RESTART flag (see sigaction(2))?
    Because the details of SA_RESTART vary across UNIX systems; this set
    of function wrappers provide a consistent way of dealing with it.
    Plus, these wrappers give wait-and-retry logic for EBUSY conditions, too.

 @fn    int     close(int fd);
  @brief Interrupt-safe (signal-safe) and busy-retry flavor of the same-named system function.

 @fn    int     creat(const char* pathname, mode_t mode);
  @brief Interrupt-safe (signal-safe) and busy-retry flavor of the same-named system function.

 @fn    int     dup2(int oldfd, int newfd);
  @brief Interrupt-safe (signal-safe) and busy-retry flavor of the same-named system function.

 @fn    int     flock(int fd, int operation);
  @brief Interrupt-safe (signal-safe) and busy-retry flavor of the same-named system function.

 @fn    off_t   lseek(int fd, off_t offset, int whence);
  @brief Interrupt-safe (signal-safe) and busy-retry flavor of the same-named system function.

 @fn    int     open(const char* pathname, int flags);
  @brief Interrupt-safe (signal-safe) and busy-retry flavor of the same-named system function.

 @fn    int     open(const char* pathname, int flags, mode_t mode);
  @brief Interrupt-safe (signal-safe) and busy-retry flavor of the same-named system function.

 @fn    ssize_t read(int fd, void* buf, size_t count);
  @brief Interrupt-safe (signal-safe) and busy-retry flavor of the same-named system function.

 @fn    ssize_t remove(const char* pathname);
  @brief Interrupt-safe (signal-safe) and busy-retry flavor of the same-named system function.

 @fn    ssize_t rename(const char* oldpath, const char* newpath);
  @brief Interrupt-safe (signal-safe) and busy-retry flavor of the same-named system function.

 @fn    ssize_t unlink(const char* pathname);
  @brief Interrupt-safe (signal-safe) and busy-retry flavor of the same-named system function.

 @fn    ssize_t waitpid(pid_t pid, int* status, int options);
  @brief Interrupt-safe (signal-safe) flavor of the same-named system function.
    (This function has no EBUSY retry logic since the system call never returns that).

 @fn    ssize_t write(int fd, const void* buf, size_t count);
  @brief Interrupt-safe (signal-safe) and busy-retry flavor of the same-named system function.

*/

#endif

