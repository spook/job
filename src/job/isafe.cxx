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
#include <errno.h>
#include <sys/wait.h>

#define _BLURT 1
#ifdef _BLURT
  #include <stdio.h>
#endif

unsigned int isafe::busy_try_limit = 20;
unsigned int isafe::busy_try_delay = 250000;    // usec, = 0.25 sec

int isafe::close(int fd) {
    int ret;
    unsigned int num = busy_try_limit;
    for (;;--num) {
        do {
            ret = ::close(fd);
        } while ((ret == -1) && (errno == EINTR));
        if (ret != -1)      return ret;     // it worked
        if (num == 0)       return ret;     // we're out of tries
        if ((errno != EBUSY) &&
            (errno != EIO)) return ret;     // not something to retry
        usleep(busy_try_delay);
#ifdef _BLURT
        fprintf(stderr, "%s BUSYWAIT\n", __FUNCTION__);
#endif
    }
}

int isafe::creat(const char* pathname, mode_t mode) {
    int ret;
    unsigned int num = busy_try_limit;
    for (;;--num) {
        do {
            ret = ::creat(pathname, mode);
        } while ((ret == -1) && (errno == EINTR));
        if (ret != -1)      return ret;     // it worked
        if (num == 0)       return ret;     // we're out of tries
        if (errno != EBUSY) return ret;     // not something to retry
                // Maybe also retry on EMFILE, ENFILE, ENOMEM, ENOSPC, EWOULDBLOCK ?
        usleep(busy_try_delay);
#ifdef _BLURT
        fprintf(stderr, "%s BUSYWAIT\n", __FUNCTION__);
#endif
    }
}

int isafe::dup2(int oldfd, int newfd) {
    int ret;
    unsigned int num = busy_try_limit;
    for (;;--num) {
        do {
            ret = ::dup2(oldfd, newfd);
        } while ((ret == -1) && (errno == EINTR));
        if (ret != -1)      return ret;     // it worked
        if (num == 0)       return ret;     // we're out of tries
        if (errno != EBUSY) return ret;     // not something to retry
        usleep(busy_try_delay);
#ifdef _BLURT
        fprintf(stderr, "%s BUSYWAIT\n", __FUNCTION__);
#endif
    }
}

int isafe::flock(int fd, int operation) {
    int ret;
    unsigned int num = busy_try_limit;
    for (;;--num) {
        do {
            ret = ::flock(fd, operation);
        } while ((ret == -1) && (errno == EINTR));
        if (ret != -1)      return ret;     // it worked
        if (num == 0)       return ret;     // we're out of tries
        if (errno != EBUSY) return ret;     // not something to retry
        usleep(busy_try_delay);
#ifdef _BLURT
        fprintf(stderr, "%s BUSYWAIT\n", __FUNCTION__);
#endif
    }
}

off_t isafe::lseek(int fd, off_t offset, int whence) {
    off_t ret;
    unsigned int num = busy_try_limit;
    for (;;--num) {
        do {
            ret = ::lseek(fd, offset, whence);
        } while ((ret == -1) && (errno == EINTR));
        if (ret != -1)      return ret;     // it worked
        if (num == 0)       return ret;     // we're out of tries
        if (errno != EBUSY) return ret;     // not something to retry
        usleep(busy_try_delay);
#ifdef _BLURT
        fprintf(stderr, "%s BUSYWAIT\n", __FUNCTION__);
#endif
    }
}

int isafe::open(const char* pathname, int flags) {
    int ret;
    unsigned int num = busy_try_limit;
    for (;;--num) {
        do {
            ret = ::open(pathname, flags);
        } while ((ret == -1) && (errno == EINTR));
        if (ret != -1)      return ret;     // it worked
        if (num == 0)       return ret;     // we're out of tries
        if (errno != EBUSY) return ret;     // not something to retry
        usleep(busy_try_delay);
#ifdef _BLURT
        fprintf(stderr, "%s BUSYWAIT\n", __FUNCTION__);
#endif
    }
}

int isafe::open(const char* pathname, int flags, mode_t mode) {
    int ret;
    unsigned int num = busy_try_limit;
    for (;;--num) {
        do {
            ret = ::open(pathname, flags, mode);
        } while ((ret == -1) && (errno == EINTR));
        if (ret != -1)      return ret;     // it worked
        if (num == 0)       return ret;     // we're out of tries
        if (errno != EBUSY) return ret;     // not something to retry
        usleep(busy_try_delay);
#ifdef _BLURT
        fprintf(stderr, "%s BUSYWAIT\n", __FUNCTION__);
#endif
    }
}

ssize_t isafe::read(int fd, void* buf, size_t count) {
    ssize_t ret;
    unsigned int num = busy_try_limit;
    for (;;--num) {
        do {
            ret = ::read(fd, buf, count);
        } while ((ret == -1) && (errno == EINTR));
        if (ret != -1)      return ret;     // it worked
        if (num == 0)       return ret;     // we're out of tries
        if (errno != EBUSY) return ret;     // not something to retry
        usleep(busy_try_delay);
#ifdef _BLURT
        fprintf(stderr, "%s BUSYWAIT\n", __FUNCTION__);
#endif
    }
}

int isafe::remove(const char* pathname) {
    int ret;
    unsigned int num = busy_try_limit;
    for (;;--num) {
        do {
            ret = ::remove(pathname);
        } while ((ret == -1) && (errno == EINTR));
        if (ret != -1)      return ret;     // it worked
        if (num == 0)       return ret;     // we're out of tries
        if (errno != EBUSY) return ret;     // not something to retry
        usleep(busy_try_delay);
#ifdef _BLURT
        fprintf(stderr, "%s BUSYWAIT\n", __FUNCTION__);
#endif
    }
}

int isafe::rename(const char* oldpath, const char* newpath) {
    int ret;
    unsigned int num = busy_try_limit;
    for (;;--num) {
        do {
            ret = ::rename(oldpath, newpath);
        } while ((ret == -1) && (errno == EINTR));
        if (ret != -1)      return ret;     // it worked
        if (num == 0)       return ret;     // we're out of tries
        if (errno != EBUSY) return ret;     // not something to retry
        usleep(busy_try_delay);
#ifdef _BLURT
        fprintf(stderr, "%s BUSYWAIT\n", __FUNCTION__);
#endif
    }
}

int isafe::unlink(const char* pathname) {
    int ret;
    unsigned int num = busy_try_limit;
    for (;;--num) {
        do {
            ret = ::unlink(pathname);
        } while ((ret == -1) && (errno == EINTR));
        if (ret != -1)      return ret;     // it worked
        if (num == 0)       return ret;     // we're out of tries
        if (errno != EBUSY) return ret;     // not something to retry
        usleep(busy_try_delay);
#ifdef _BLURT
        fprintf(stderr, "%s BUSYWAIT\n", __FUNCTION__);
#endif
    }
}

// waitpid is does not deal with filesystems; no need to retry on EBUSY or similar
pid_t isafe::waitpid(pid_t pid, int* status, int options) {
    pid_t ret;
    do {
        ret = ::waitpid(pid, status, options);
    } while ((ret == -1) && (errno == EINTR));
    return ret;
}

ssize_t isafe::write(int fd, const void* buf, size_t count) {
    ssize_t ret;
    unsigned int num = busy_try_limit;
    for (;;--num) {
        do {
            ret = ::write(fd, buf, count);
        } while ((ret == -1) && (errno == EINTR));
        if (ret != -1)      return ret;     // it worked
        if (num == 0)       return ret;     // we're out of tries
        if (errno != EBUSY) return ret;     // not something to retry
        usleep(busy_try_delay);
#ifdef _BLURT
        fprintf(stderr, "%s BUSYWAIT\n", __FUNCTION__);
#endif
    }
}

