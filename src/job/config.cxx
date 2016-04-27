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

#include "job/config.hxx"
#include <stdio.h>
#include <stdlib.h>         // str2int()

#define isid(c) (isalnum(c) || (c == '-') || (c == '_') || (c == '.') || (c == '$'))

using job::ERR_OK;
using job::str2int;
using job::int2str;

// Constructor with load
job::config::config(const std::string & fnam) {
    load(fnam);
}

// Does a tag exist in a section?
bool job::config::exists(const std::string & sec, 
                         const std::string & tag)
{
    return (this->find(sec) != this->end()) &&
           ((*this)[sec].find(tag) != (*this)[sec].end());
}

// Get a string value, or if not present, use a default
std::string job::config::get(const std::string & sec, 
                             const std::string & tag, 
                             const std::string & dfl) 
{
    return exists(sec, tag)
        ? (*this)[sec][tag]
        : dfl;
        ;
}

// Get integer value
int job::config::geti(const std::string & sec, 
                      const std::string & tag, 
                      const int dfl)
{
    return exists(sec, tag)
        ? str2int((*this)[sec][tag])
        : dfl;
        ;
}

// Parse a config file and load it into us (adding to whatever is already there)
job::status job::config::load(const std::string & fnam) {
    FILE* fp = fopen(fnam.c_str(), "r");
    if (!fp) return error.set("Cannot open " + fnam, SYS_errno);

    int  lnum = 0;
    char line[256];
    std::string sec = "";
    while (fgets(line, sizeof line, fp)) {
        ++lnum;

        // skip to first non-blank character
        size_t n = (size_t)-1;
        while (line[++n] && isblank(line[n]));
        if (!line[n]) continue;
        if (line[n] == '#') {
            (*this)[sec]["#"].append(line+n);   //includes # and \n
            continue;
        }
        if (line[n] == '[') {

            // New section
            while(line[++n] && isblank(line[n]));   // skip blanks after '['
            size_t m = n;       // n indexes to first tag char, m moves thru
            while (line[++m] && !isblank(line[m]) && (line[m] != '\n') && (line[m] != ']'));
            size_t o = m - 1;   // o indexes to last char
            while(line[m] && isblank(line[m])) m++; // skip blanks before ']'
            if (line[m] != ']') {
                fclose(fp);
                return error.set("No section terminator at line " + int2str(lnum)); 
            }
            sec.assign(line+n, o-n+1);
            continue;
        }

        // tag:value line
        size_t t  = 0;      // start of tag
        size_t tl = 0;      // tag length
        size_t v  = 1;      // start of value
        size_t w  = 0;      // end of value
        enum {WANT_TAG, WANT_TEND, WANT_DELIM, WANT_VAL, WANT_VEND} state = WANT_TAG;
        while (char c = line[n++]) {
            if (c == '\n') break;
            switch (state) {
                case WANT_TAG:
                    if (!isid(c)) {
                        fclose(fp);
                        return error.set("Bad tag at line " + int2str(lnum));
                    }
                    t = n - 1;
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
                    if (c != ':') {
                        fclose(fp);
                        return error.set("No delimiter or bad tag char at line " + int2str(lnum));
                    }
                    state = WANT_VAL;
                    break;
                case WANT_VAL:
                    if (isblank(c)) continue;
                    v = w = n - 1;
                    state = WANT_VEND;
                    break;
                case WANT_VEND:
                    if (isblank(c)) continue;
                    w = n - 1;
                    break;
            }
        }
        if (state == WANT_TAG) continue; // just a line with blanks, ignore it
        if ((state == WANT_TEND) || (state == WANT_DELIM)) {
            fclose(fp);
            return error.set("Tag with no value at line " + int2str(lnum));
            }
        std::string tag(line+t, tl);
        std::string val(line+v, w-v+1);
        (*this)[sec][tag] = val;
    }

    fclose(fp);
    return error = ERR_OK;
}

job::status job::config::store(const std::string & fnam) {
    FILE* fp = fopen(fnam.c_str(), "w");
    if (!fp) return error.set("Cannot create/update " + fnam, SYS_status);
    std::string all_of_me = to_string();
    if (EOF == fputs(all_of_me.c_str(), fp))
        return error.set("store: Cannot write " + fnam, SYS_status);
    fclose(fp);
    return error = ERR_OK;
}

// Dump ourselves into a string
std::string job::config::to_string() {
    std::string ret;
    typedef tmap::iterator t_it;
    typedef std::map<std::string, std::string> umap;
    for (t_it it = this->begin(); it != this->end(); it++) {
        std::string section = it->first;
        if (section.size()) ret += "\n[" + section + "]\n";
        for (umap::iterator jt = it->second.begin(); jt != it->second.end(); ++jt) {
            if (jt->first == "#") {
                ret += jt->second;
                continue;
            }
            ret +=  jt->first + ": ";
            for (size_t s=jt->first.size(); s<=10; ++s) ret += " ";
            ret +=  jt->second + "\n";
        }
    }
    return ret;
}

