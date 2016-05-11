#ifndef _JOB_GETOPT_HXX_
#define _JOB_GETOPT_HXX_ 1

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

#include "job/status.hxx"
#include "job/string.hxx"
#include "open/optionparser.hxx" // http://optionparser.sourceforge.net, overhead ~11k
#include <stdio.h>

// For the optionparse, define a "Required" argument (too bad this isn't standard)
struct Arg: public option::Arg {
    static option::ArgStatus Reqd(const option::Option & option, bool msg)  {
        if (option.arg != 0) return option::ARG_OK;
        if (msg) fprintf(stderr, "Option '%s' requires an argument\n", option.name);
        return option::ARG_ILLEGAL;
    }
};

namespace job {
  class getopt {
        int                         op_none;    // index of non-op args, usually 0
        int                         op_help;    // index of help option, 0 = no help
        const option::Descriptor*   usage;      // syntax descriptor
        option::Option*             opbuf;      // buffer to hold values

    public:
        getopt(const option::Descriptor* usage, int opNone = 0, int opHelp = 0);
        ~getopt();
        int             parse(int argc,
                              const char* argv[], 
                              bool gnu = true);                 // parser - this does the work
        option::Option* opts;                                   // the parsed options
        stringlist      args;                                   // left-over tokens (surplus, extra)
  };
}

#endif

