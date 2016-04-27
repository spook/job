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

#include "job/getopt.hxx"
#include <iostream>         // used by "optionparser", not us!

using namespace std;

// Constructor; parses the given argc/argv according to usage
job::getopt::getopt(const option::Descriptor* usg, int opNone, int opHelp) 
    : op_none(opNone)
    , op_help(opHelp)
    , usage(usg)
    , opbuf(0)
    , opts(0)
{}

// Destructor
job::getopt::~getopt() {
    delete[] opbuf;
    delete[] opts;
}

// Parser - this does the work
int job::getopt::parse(int argc, const char* argv[], bool gnu) {
    option::Stats stats(usage, argc, argv);
    
    opts  = new option::Option[stats.options_max];
    // Add on a safety margin to the buffer size as our current
    // use of the parser ctor is configured with default parameter
    // bufsize == -1 (meaning large enough)
    opbuf = new option::Option[stats.buffer_max + 16];
    
    // Passing boolean true as the first argument (gnu) will make the parser
    //   tolerate non-option arguments in arbitrary postiions of the token list.
    //   This flexibility is nice, so we make it the default.   However, a command
    //   like mkjob(8) wants to stop at the first no-option argument, so we allow
    //   the caller to set it to false.
    option::Parser parser(gnu, usage, argc, argv, opts, opbuf);   // this does the work
    if (parser.error()) { // the parser blew chunks
        option::printUsage(std::cout, usage);
        return ERR_BADCLI;
    }
    if (op_help && opts[op_help]) {
        option::printUsage(std::cout, usage);
        return ERR_OK;
    }

    // Capture the left-overs
    for (int i = 0; i < parser.nonOptionsCount(); ++i) {
        args.push_back(parser.nonOption(i));
    }

    // Check for bad syntax
    for (option::Option* opt = opts[op_none]; opt; opt = opt->next())
        fprintf(stderr, "Unknown option: %s\n", opt->name);
    if (opts[op_none]) {
        option::printUsage(std::cerr, usage);
        return ERR_BADCLI;
    }

    // All good
    return ERR_OK;
}

