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

#include "job/base.hxx"
#include "job/string.hxx"
#include <algorithm>
#include <cctype>
#include <functional> 
#include <locale>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>
#include <string>
#include <time.h>
#include <wordexp.h>

#define STRINGIFY(arg)      #arg
#define VERSION_STRING(ver) STRINGIFY(ver)
const char* job::version = VERSION_STRING(RELEASE);

// === String Conversion: some type to string ===

std::string job::boo2str(const bool b) {
    return b ? "true" : "false";
}

std::string job::int2str(const int i) {
    char tmp[33];
    snprintf(tmp, 33, "%d", i);
    std::string ret = tmp;
    return ret;
}

std::string job::siz2str(const size_t i) {
    char tmp[256];
    snprintf(tmp, sizeof tmp, "%" PRI_size_t, i);
    std::string ret = tmp;
    return ret;
}

std::string job::tim2str(const time_t & t, const bool human) {
    char buf[sizeof "1953-06-05T13:14:15Z"];
    if (human) strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%SZ", gmtime(&t));
    else       strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
    return buf;
}

std::string job::yn2str(const bool b) {
    return b ? "yes" : "no";
}

// String Parsing: string to some type

bool job::str2boo(const std::string & s) {
    if (!s.size()) return false;
    return (s[0] == 'y') || (s[0] == 't') || (s == "1");
}

int job::str2int(const std::string & s) {
    return ::atoi(s.c_str());
}

size_t job::str2siz(const std::string & s) {
    // TODO:  don't use streams (think like an old C program)
    if (!s.size()) return 0;
    std::istringstream f(s);
    size_t ret;
    f >> ret;
    return ret;
}

time_t job::str2tim(const std::string & s) {

    // Try a bunch of formats
    const char* formats[] = 
            {"%Y-%m-%d"
            ,"%Y-%m-%d %H:%M"
            ,"%Y-%m-%d %H:%M:%S"
            ,"%Y-%m-%dt%H:%M"
            ,"%Y-%m-%dt%H:%M:%S"
            ,"%Y-%m-%dT%H:%M"
            ,"%Y-%m-%dT%H:%M:%S"
            ,"%H:%M"
            ,"%H:%M:%S"
            ,"%Y%m%d"
            ,"%Y%m%d%H%M"
            ,"%Y%m%d%H%M%S"
            };
    struct tm t;
    bool match = false;
    for (size_t i=0; i < (sizeof(formats)/sizeof(formats[0])); i++) {
        memset(&t, '\0', sizeof(t));
        char* end  = strptime(s.c_str(), formats[i], &t);
        size_t got = end - (char*)s.c_str();
        if (got == s.size()) {
            match = true;
            break;
        }
    }
    if (!match) return -1;

    // If we don't have a date, use today
    if (!t.tm_year) {
        time_t tnow = time(NULL);
        struct tm* now = gmtime(&tnow);     // TODO: use localtime() unless ends with 'Z'
        t.tm_year = now->tm_year;
        t.tm_mon  = now->tm_mon;
        t.tm_mday = now->tm_mday;
    }

    // Return as a time_t
    return mktime(&t);
}

// ==== String utility functions ===

std::string job::join(const stringlist & list, const char d) {
    std::string str("");
    for (unsigned i=0; i<list.size(); i++) {
        if (i) str += d;
        str += list[i];
    }
    return str;
}

std::string job::join(const stringlist & list, const char d, const unsigned int n) {
    std::string str("");
    for (unsigned i=0; i<list.size() && i<n; i++) {
        if (i) str += d;
        str += list[i];
    }
    return str;
}

std::string job::join(const stringlist & list, const std::string & d) {
    std::string str("");
    for (unsigned i=0; i<list.size(); i++) {
        if (i) str += d;
        str += list[i];
    }
    return str;
}

job::stringlist job::split(const std::string & s, const std::string & d, const int parts) {
    stringlist ret;
    size_t b = 0;
    if( !d.empty() ) {
        size_t e = 0;
        while ((e = s.find(d, b)) != s.npos) {
            ret.push_back(s.substr(b,e-b));
            b = e+d.size();
            if ((parts) && (ret.size() == (unsigned int)(parts - 1))) break;
        }
    }
    ret.push_back(s.substr(b,s.npos));
    return ret;
}

// Get the tail-end of a string.  Like substring but works back from the end
std::string job::tail(const std::string & s, const unsigned int amt) {
    if (amt > s.length()) return s;
    return s.substr(s.length() - amt);
}

// Test the head of a string
bool job::has_head(const std::string & s, const std::string & match) {
    return s.substr(0, match.length()) == match;
}

// Test the tail of a string
bool job::has_tail(const std::string & s, const std::string & match) {
    return tail(s, match.length()) == match;
}

// lowercase the string - changes it in-place, and returns it
std::string & job::lc(std::string & s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

// lowercase the string - do not change in-place, just return it
std::string job::lc(const std::string & _s) {
    std::string s = _s;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

// trim start of string - changes it in-place, and returns it
std::string & job::ltrim(std::string & s) {
    s.erase(s.begin(),
            std::find_if(s.begin(), 
                         s.end(), 
                         std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}

// trim start of string - do not change in-place, just return it
std::string job::ltrim(const std::string & _s) {
    std::string s = _s;
    return ltrim(s);
}

// trim end of string - changes it in-place, and returns it
std::string & job::rtrim(std::string & s) {
    s.erase(std::find_if(s.rbegin(), 
                         s.rend(), 
                         std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}

// trim end of string - do not change in-place, just return it
std::string job::rtrim(const std::string & _s) {
    std::string s = _s;
    return rtrim(s);
}

// trim both ends of string - changes it in-place, and returns it
std::string & job::trim(std::string & s) {
    rtrim(s);
    return ltrim(s);
}

// trim both ends of string - do not change in-place, just return it
std::string job::trim(const std::string & _s) {
    std::string s = _s;
    return trim(s);
}

// replace all occurrances of 'from' with 'to', in 'str'.
//  n=0 does all, n>0 limits to that many substitutions.
void job::replace_all(      std::string & str, 
                      const std::string & from, 
                      const std::string & to,
                      const unsigned int n) {
    if (from.empty()) return;
    unsigned int m = n;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        if (m) {m--; if (!m) return;}
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}

// Quote a string so its safe for /bin/sh
std::string job::qq(const std::string & str) {
    std::string ret = str;
    replace_all(ret, "'", "'\"'\"'"); // Trust me.... he said with a grin
    ret.insert(0, "'");
    ret.append("'");
    return ret;
}

// Conditionally quote a string so its safe for /bin/sh
std::string job::qqif(const std::string & str) {
    return str.find_first_of(" \t\n\v\b\r\f\a|<>[]{}()&'\\*?`;") != std::string::npos
        ? qq(str)
        :    str;
}

// String list functions

// List difference:  A - B  returns anything in A that is not in B.
//  Or put another way, any element common to both is removed from the A list, 
//  and the result returned (A is not modified)
job::stringlist job::diff(const stringlist & a, const stringlist & b) {
    stringlist c;
    for (size_t i=0; i < a.size(); i++) {
        if (!has(b, a[i])) c.push_back(a[i]);
    }
    return c;
}

// Does list a have string s?
bool job::has(const stringlist & a, const std::string & s) {
    for (size_t i=0; i < a.size(); i++) {
        if (a[i] == s) return true;
    }
    return false;
}

// Removes string s from list a (a is modified) - first instance only
//  Returns true if string found and removed.
bool job::remove(stringlist & a, const std::string & s) {
    for (size_t i=0; i<a.size(); i++) {
        if (a[i] == s) {
            a.erase(a.begin()+i);
            return true;
        }
    }
    return false;
}

// Case-insensitive functions for maps, etc...
bool job::caseless::operator()(const std::string & s1, const std::string & s2) const {
    return std::lexicographical_compare(s1.begin(), s1.end(),   // source range
                                        s2.begin(), s2.end(),   // dest range
                                        nocase_compare());      // comparison
};

bool job::caseless::nocase_compare::operator()(
    const unsigned char & c1,
    const unsigned char & c2) const {
    return tolower(c1) < tolower(c2); 
};

