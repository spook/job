#ifndef _JOB_BASE_HXX_
#define _JOB_BASE_HXX_ 1

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

// base.hxx:
//      Defines useful base macros and normalizes differences between compilers
//      *Most* modules should include this header.

#include <stdint.h>

// Pragma stuff; GCC's <4.5 don't do diagnostic pragmas
#if defined(__GNUC__)
    #define DO_PRAGMA(x) _Pragma (#x)
#else
    #define DO_PRAGMA(x)
#endif

#if defined (__clang__) || \
    ((__GNUC__)  && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 5))))
    #define SUPPRESS_DIAGNOSTIC_START(d) \
        DO_PRAGMA(GCC diagnostic push) \
        DO_PRAGMA(GCC diagnostic ignored #d)
    #define SUPPRESS_DIAGNOSTIC_END \
        DO_PRAGMA(GCC diagnostic pop)
#else
    #define SUPPRESS_DIAGNOSTIC_START(d)
    #define SUPPRESS_DIAGNOSTIC_END
#endif

// Extend STDC_FORMAT_MACROS with some other popular types
#if __WORDSIZE >= 64
    #define PRI_size_t  "lu"
    #define PRI_ssize_t "ld"
#else
    #define PRI_size_t  "u"
    #define PRI_ssize_t "d"
#endif
#define PRI_time_t  "ld"
#define PRI_gid_t   "d"
#define PRI_uid_t   "d"
#define PRI_pid_t   "d"

#endif  // _JOB_BASE_HXX_

