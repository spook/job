#ifndef _JOB_CONFIG_HXX_
#define _JOB_CONFIG_HXX_ 1

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

using job::status;

namespace job {
  typedef std::map< std::string, std::map<std::string, std::string, job::caseless>, job::caseless > tmap;
  class config : public tmap {
  public:
    status      error;
    
                config(const std::string & fnam);
    bool        exists(const std::string & sec, 
                       const std::string & tag);
    std::string get(const std::string & sec, 
                    const std::string & tag, 
                    const std::string & dfl = "");
    int         geti(const std::string & sec, 
                     const std::string & tag, 
                     const int dfl = 0);
    status      load(const std::string & fnam);
    status      store(const std::string & fnam);
    std::string to_string();
  };
}

/*! @file
 *   @brief Configuration file class (INI format)
 *
 * @class job::config
 *   @brief Fast, small config file parser and writer

    # Comment, # must be in column 1
    [section.header]
    tag:val
    tag2: value
    tag3 :value
    tag4 : value
      tag : can start indented

    [blah]
    foo: bar
    # Max line length is 255 bytes
    thingy : value must be all on one line and # is a value char too


 * @fn job::config::load(const std::string & fnam) const
 *   @brief Parses and loads a config file file.
 *   @param fnam File name of the config file
 *
 *   Parses the config file and loads the sections, tags, and values into ourselves.
 *   Recall that a tini object is-a hashmap of hashmaps.
 *   Our error status will indicate success or failure and the reason(s).
 *
 *   @code
 *     job::config cfg;
 *     cfg.load("/etc/job/jobs.conf");
 *     if (cfg.error) { ... do something for the error ... }
 *     printf("The blah foo value is %s\n", cfg["blah"]["foo"].c_str());
 *     cfg["queue"]["state"] = "stop";      // update or add a value
 *     cfg.set("queue", "state", "stop");   // Alternate method to set value
 *     cfg.store("/etc/job/jobs.conf");     // write it back
 *     if (cfg.error) { ... do something for the error ... }
 *   @endcode
 *
*/

#endif

