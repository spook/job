#ifndef TAP_EXTRA_HXX
#define TAP_EXTRA_HXX 1

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

#include "tap++/tap++.hxx"
#include <stdio.h>
#include <string>

// Tests if a status object is OK, using the string compare,
//  so if not OK the test emits the text error which is more useful than the int error.
#define isok(e,t)  is((std::string)(e).error, "OK", t)
#define isaok(e,t) isok(e,t); if ((e).error) bail_out("Cannot continue")

// Add regex matching tests
namespace TAP {
    bool like(const std::string & str, const std::string & pat, std::string comment = "") throw();
    bool not_like(const std::string & str, const std::string & pat, std::string comment = "") throw();
    bool has(std::string & s, const char* b, const char* msg);
}

// Capture print output
class test_capture_output {
  public:
    test_capture_output(const int fd = STDERR_FILENO);
    std::string & output();
    ~test_capture_output();
  private:
    int         orig_fd;    // Original file descriptor
    int         back_fd;    // Backup of original fd
    std::string fnam;
    std::string out;
};

// Misc utility functions
int test_end();

#endif

