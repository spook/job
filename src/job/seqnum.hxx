#ifndef _JOB_SEQNUM_HXX_
#define _JOB_SEQNUM_HXX_ 1

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
#include <stdint.h>
#include <string>

#define PRI_seqnum_t PRIu64   // for use in printf's

namespace job {
class seqnum {

  public:
    // Constructor/destructor
                seqnum();
                seqnum(const std::string & seqfile);
                ~seqnum();

    // Returns the current sequential number; on error 0 is returned and 'error' set.
    uint64_t    curr();

    // Returns the next sequential number; on error 0 is returned and 'error' set.
    uint64_t    next();

    // Public vars
    status      error;     // Error status
    uint64_t    value;     // Last value issued
    std::string filename;  // Name of sequence file

  private:
    // Assignment operator
    seqnum & operator=(const seqnum & other);
};
}

/*! @file
 * @class job::seqnum
 *   @brief Generate a unique sequential number
 *
 *   Returns the next number in a sequence each time next() is called.
 *   A sequence file is used to control generation of unique sequence numbers,
 *   such as when a job ID is needed for some process.  This generator guarantees
 *   that competing processes won't issue the same number when using the same
 *   sequence file.
 *
 *   @code
 *     job::seqnum sn("/tmp/my-sequence/tmp.seq");
 *     uint64_t n = sn.next();
 *      ...
 *   @endcode
 *
 * @fn job::seqnum::seqnum(const std::string & seqfile)
 *   @brief Construct a seuence number object
 *   @param seqfile Filename of the seuence file that will persiste and synchronize number generation.
 *
 * @fn uint64_t job::seqnum::next()
 *   @brief Returns the next number in the sequence, or 0 if an error occurs.
 *
 * @fn job::seqnum::~seqnum()
 *   @brief Destructor.
 *
 */

#endif

