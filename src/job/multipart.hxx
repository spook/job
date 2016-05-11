#ifndef _JOB_MULTIPART_HXX_
#define _JOB_MULTIPART_HXX_ 1

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
#include <map>
#include <string>
#include <sys/types.h>          // stat()
#include <sys/stat.h>           // stat()
#include <vector>

namespace job {

typedef std::vector< std::map<std::string, std::string, job::caseless> > vecmap_t;
class multipart : public vecmap_t {
  public:
    static const char* BODY_TAG;

    job::status error;
    std::string boundary;
    std::string substatus;
    struct stat statbuf;

    bool        closed;         // Includes final terminating boundary

                multipart();

    bool        exists(const unsigned int sec, 
                       const std::string & tag);
    std::string get(const unsigned int sec, 
                    const std::string & tag, 
                    const std::string & dfl = "");
    int         geti(const unsigned int sec, 
                     const std::string & tag, 
                     const int dfl);
    job::status load(const std::string & fnam);
    job::status store(const std::string & fnam);
//    status      load(read_callback get_chunk,   // callback gets file data
//                    );
    std::string to_string();

  private:
    std::string get_uuid();
};
}

/*! @file
 *   @brief Parser for Job Multipart files (almost the same as HTTP MIME multipart files)
 *
 * @class job::multipart
 *   @brief Fast, small parser for multipart files.
 *
 *   A Job's multipart file is similar to an RFC 1341/RFC 2387 MIME multipart message.
 *   But there are a few differences:
#      - No general header line (GET, POST, etc)
 *     - Line endings are \n, not \r\n
 *     - Header sections can have comments beginning with #
 *     - Only one level of message nesting
 *
 *   These files parse into, for each section:
 *      Multiple tags:values
 *      The body - an arbitrary chunk data
 *   The first section is numbered 0.
 *   The above then can repeat for multiple sections. 
 *   The whole file is parsed into an object of this class, 
 *   which is-a vector of maps, for simple access.
 *   Tags are always stored LOWER CASE.
 *   Examples show it best:
 *
 * Example multipart  message
    Host: www.example.com
    Accept-Encoding: compress, gzip
    Content-Type: multipart/mixed; boundary=gc0p4Jq0M2Yt08jU534c0p

    Preamble stuff here, often ignored by HTTP but maybe used 
    by your code?
    --gc0p4Jq0M2Yt08jU534c0pqr
    Content-type: text/plain; charset=us-ascii 
    X-Job-Thing-One : stuff
    X-Job-Thing-Two : yippie!

    This is explicitly typed plain ASCII text. 
    It DOES end with a linebreak, hence the blank line below.

    --gc0p4Jq0M2Yt08jU534c0pqr
    Content-Type: application/octet-stream
    Content-Length: 4096

    .......stuff
    .....more stuff
    --gc0p4Jq0M2Yt08jU534c0p--


 * @fn job::multipart::load(const std::string & fnam)
 *   @brief Parses and loads a multipart file.
 *   @param fnam File name of the multi file
 *
 *   Parses the multipart file, loading the sections.
 *   Recall that a multi object is-a map of maps.
 *   If a body is found in a section, it's put in the __BODY__ tag for the section.
 *   The first section is the 0-numbered section.
 *   Our error status will indicate success or failure and the reason(s).
 *
 *   @code
 *     job::multipart  mp;
 *     mi.load("/etc/xyz/something.job");
 *     if (mp.error) { ... do something for the error ... }
 *     printf("The foo value in section 2 is %s\n", mi[2]["foo"].c_str());
 *     printf("The first body is:\n%s\n", mi[0]["__BODY__"].c_str());
 *   @endcode
 *
 * @fn job::multipart::store(const std::string & fnam)
 *   @brief Writes the multipart file.
 *   @param fnam File name of the multi file to create/update
 *
 *   Creates or overwrites the named multipart file with our present info.
 *   Our error status will indicate success or failure and the reason(s).
 *
 *   @code
 *     job::multipart  mp;
 *     mp.load("/etc/xyz/something.job");
 *     if (mp.error) { ... do something for the error ... }
 *          // later...
 *     mp.store("/etc/xyz/something.job");
 *     if (mp.error) { ... do something for the error ... }
 *   @endcode
 *
*/

#endif

