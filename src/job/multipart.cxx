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
#include "job/multipart.hxx"
#include <assert.h>
#include <fcntl.h>          // open
#include <stdio.h>          // fileno etc...
#include <string.h>         // strncmp
#include <sys/stat.h>       // open
#include <sys/types.h>      // open
#include <unistd.h>         // read

#define isid(c) (isalnum(c) || (c == '-') || (c == '_') || (c == '.'))
#define LINE_MAXLEN 4096
#define BOUND_MAXLEN 80
#define FIELD_MINLEN 13

using job::ERR_OK;
using job::int2str;
using job::lc;

const char* job::multipart::BODY_TAG = "__BODY__";

// Constructor (inherits from vector-of-map, so most of the work is done there)
job::multipart::multipart() 
    : closed(true) {
}

// Does a tag exist in a section?
bool job::multipart::exists(const unsigned int sec, 
                            const std::string & tag) {
    return (sec < size()) && ((*this)[sec].count(tag) > 0);
}

// Get a string value, or if not present, use a default
std::string job::multipart::get(const unsigned int sec, 
                                const std::string & tag, 
                                const std::string & dfl) {
    return exists(sec, tag)
        ? (*this)[sec][lc(tag)]
        : dfl;
        ;
}

// Get a random UUID
std::string job::multipart::get_uuid() {
    int fd = isafe::open("/proc/sys/kernel/random/uuid", O_RDONLY);
    if (fd < 0) return "bad-open-0000-0000-0000-000000000000";
    char uuid[36];
    ssize_t amt = isafe::read(fd, uuid, sizeof(uuid));
    isafe::close(fd);
    if (amt < (ssize_t)sizeof(uuid)) return "bad-read-0000-0000-0000-000000000000";
    return std::string(uuid, sizeof(uuid));
}


// Load a multipart file (adding to whatever is already there)
job::status job::multipart::load(const std::string & fnam) {
    FILE* fp = fopen(fnam.c_str(), "r");
    if (!fp) return error.set("Cannot open " + fnam, SYS_status);
    if (fstat(fileno(fp), &statbuf) < 0) return error.set("Cannot stat " + fnam, SYS_status);
    assert(LINE_MAXLEN > (4+BOUND_MAXLEN)); // Should do a compile-time check of this, not run-time

    closed = false;
    substatus.clear();
    int  lnum = 0;
    char line[LINE_MAXLEN];
    bool got_gap = false;
    bool in_body = false;
    unsigned int sec = 0;
    while (fgets(line, sizeof line, fp)) {
        ++lnum;
        if (!line[0]) continue;
        // "line" comparison note: we're guaranteed to have at the
        //  NUL character at the end of the buffer, due to using fgets().

        // at a boundary and what type?
        bool at_bound  = (line[0] == '-') 
                      && (line[1] == '-')
                      && (0 == boundary.compare(0, boundary.npos, line+2, boundary.size()))
                       ;
        bool mid_bound = at_bound 
                      && (line[2+boundary.size()] == '\n')
                       ;
        bool end_bound = at_bound
                      && (line[2+boundary.size()] == '-')
                      && (line[3+boundary.size()] == '-')
                      && (line[4+boundary.size()] == '\n')
                       ;
                        // We don't have to worry about overflowing line[]
                        //  because the max boundary size is much less than
                        //  the line max; certainly at least 4 more.
        if (end_bound) {

            // The prior \n belongs to the boundary; remove it
            if (sec >= size())
                resize(sec+1);
            if ((*this)[sec].count(BODY_TAG)) {
                size_t s = (*this)[sec][BODY_TAG].size();
                if (s && ((*this)[sec][BODY_TAG][s-1] == '\n')) {
                    (*this)[sec][BODY_TAG].resize(s-1);
                }
            }

            // Done
            closed = true;
            break;
        }
        if (mid_bound) {

            // The prior \n belongs to the boundary; remove it
            if (sec >= size())
                resize(sec+1);
            if ((*this)[sec].count(BODY_TAG)) {
                size_t s = (*this)[sec][BODY_TAG].size();
                if (s && ((*this)[sec][BODY_TAG][s-1] == '\n')) {
                    (*this)[sec][BODY_TAG].resize(s-1);
                }
            }

            // New section
            got_gap = false;
            in_body = false;
            sec = size();
            continue;
        }
        if (in_body) {
            // special sub-status line?
            //  Note - because of the short-circuiting of &&, and the guarantee that 
            //      there is a null character at the end of the line, we're safe
            //      here not-testing the line length.
            if ((line[0] == '#') && (line[1] == '#') && (line[2] == ' ')) {
                substatus = line+3;
                if ((substatus.size() > 0) && (substatus[substatus.size()-1] == '\n'))
                    substatus.resize(substatus.size()-1);   // Remove trailing \n
            }

            if (sec >= size())
                resize(sec+1);
            (*this)[sec][BODY_TAG].append(line);
            continue;
        }
        if (line[0] == '#') continue;   // we allow comments in the header
        if (got_gap) {
            // At start of body
            in_body = true;
            if (sec >= size())
                resize(sec+1);
            (*this)[sec][BODY_TAG] = line;
            continue;
        }
        if (line[0] == '\n') {
            got_gap = true; 
            continue;
        }

        // tag:value line
        size_t n  = (size_t)-1;
        size_t t  = 0;      // start of tag
        size_t tl = 0;      // tag length
        size_t v  = 1;      // start of value
        size_t w  = 0;      // end of value
        enum {WANT_TAG, WANT_TEND, WANT_DELIM, WANT_VAL, WANT_VEND} state = WANT_TAG;
        while (char c = line[++n]) {
            if (c == '\n') break;
            switch (state) {
                case WANT_TAG:
                    if (isblank(c)) continue;
                    if (!isid(c)) return error.set("Bad tag at line " + int2str(lnum));
                    t = n;
                    state = WANT_TEND;
                    break;
                case WANT_TEND:
                    tl++;
                    if (isid(c)) continue;
                    if (c == ':') state = WANT_VAL;
                    else          state = WANT_DELIM;
                    break;
                case WANT_DELIM:
                    if (isblank(c)) continue;
                    if (c != ':') return error.set("No delimiter at line " + int2str(lnum));
                    state = WANT_VAL;
                    break;
                case WANT_VAL:
                    if (isblank(c)) continue;
                    v = w = n;
                    state = WANT_VEND;
                    break;
                case WANT_VEND:
                    if (isblank(c)) continue;
                    w = n;
                    break;
            }
        }
        if (state == WANT_TAG) continue; // just a line with blanks, ignore it
        if ((state == WANT_TEND) || (state == WANT_DELIM)) 
            return error.set("Tag with no value at line " + int2str(lnum));
        std::string tag(line+t, tl);
        std::string val(line+v, w-v+1);
        if (sec >= size()) resize(sec+1);
        (*this)[sec][tag] = val;

        if ((sec == 0)
              && boundary.empty()
              && (lc(tag) == "content-type")
              && (lc(val.substr(0, 26)) == "multipart/mixed; boundary=")) {
            boundary = val.substr(26);
        }
    }

    fclose(fp);
    return error = ERR_OK;
}

job::status job::multipart::store(const std::string & fnam) {
    FILE* fp = fopen(fnam.c_str(), "w");
    if (!fp) return error.set("store: Cannot create/open " + fnam, SYS_status);

    std::string all_of_me = to_string();
    if (EOF == fputs(all_of_me.c_str(), fp))
        return error.set("store: Cannot write " + fnam, SYS_status);

    if (fstat(fileno(fp), &statbuf) < 0)
        return error.set("store: Cannot stat " + fnam, SYS_status);

    fclose(fp);
    return error = ERR_OK;
}

// Dump ourselves into a string
std::string job::multipart::to_string() {
    if ((size() > 1) && boundary.empty()) {
        boundary = get_uuid();
//        if (!size()) resize(1);
        (*this)[0]["Content-Type"] = "multipart/mixed; boundary="+boundary;
    }

    std::string ret;
    typedef std::map<std::string, std::string> umap;
    for (size_t i=0; i<size(); i++) {
        // TODO: header line (maybe - do we need this?)
        std::string body = "";
        for(umap::iterator j = (*this)[i].begin(); j != (*this)[i].end(); ++j) {
            if (j->first == BODY_TAG) {
                body = j->second;  // TODO: not very efficient!
                continue;
            }
            // Add "tag: value\n"
            ret += j->first + ": ";
            for (size_t i=j->first.size(); i<FIELD_MINLEN; ++i) ret += ' '; // pad to min length
            ret += j->second + "\n";
        }
        if (body.size()) {
            ret += "\n";
            ret += body;
        }
        if (boundary.size() && (closed || (i+1) < size())) {
            ret += "\n--" + boundary;
            ret += ((i+1) == size())? "--\n" : "\n";
        }
    }

    return ret;
}

