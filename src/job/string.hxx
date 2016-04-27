#ifndef _JOB_STRING_HXX_
#define _JOB_STRING_HXX_ 1

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

//  Additional types and functions for dealing with strings.

// Implementer's note:  Why do we use 'const' on fundamental parameter types, like
//  void dosomething(const int x), when the x is passed as a copy anyway?
//  There's good reason, see http://stackoverflow.com/questions/2452132/does-it-ever-make-sense-to-make-a-fundamental-non-pointer-parameter-const
//  Or just ask Bjarne Stroustrup ;-)

#include <map>
#include <stdint.h>
#include <string>
#include <time.h>
#include <vector>

namespace job {

    // our version string
    extern const char* version;

    // Simple, intuitive type(s) go here
    typedef std::vector<std::string>        stringlist;

    // Use caseless for a case-insensitive hash
    //  example:
    //      typedef std::map<std::string, buffer, caseless> mycihash;
    struct caseless {
        struct nocase_compare : public std::binary_function<unsigned char, unsigned char, bool> {
            bool operator()(const unsigned char & c1, const unsigned char & c2) const;
        };
        bool operator()(const std::string & s1, const std::string & s2) const;
    };

    // String Conversion: some type to string
    std::string boo2str(const bool b);
    std::string int2str(const int i);
    std::string siz2str(const size_t i);
    std::string tim2str(const time_t & t, const bool human = false);
    std::string yn2str(const bool b);

    // String Parsing: string to some type
    bool          str2boo(const std::string & s);
    int           str2int(const std::string & s);
    size_t        str2siz(const std::string & s);
    time_t        str2tim(const std::string & s);

    // String utility functions
    std::string join(const stringlist & list, const char d = ' ');
    std::string join(const stringlist & list, const std::string & d);
    std::string join(const stringlist & list, const char d, const unsigned int n);
    stringlist split(const std::string & s, const std::string & d, const int parts = 0);
    std::string tail(const std::string & s, const unsigned int amt);
    bool has_head(const std::string & s, const std::string & match);
    bool has_tail(const std::string & s, const std::string & match);
    std::string & lc(std::string & s);
    std::string   lc(const std::string & s);
    std::string & ltrim(std::string & s);
    std::string   ltrim(const std::string & s);
    std::string & rtrim(std::string & s);
    std::string   rtrim(const std::string & s);
    std::string & trim(std::string & s);
    std::string   trim(const std::string & s);

    void replace_all(      std::string & str, 
                     const std::string & from, 
                     const std::string & to,
                     const unsigned int  n = 0);

    // Quote a string so its safe for /bin/sh
    std::string   qq(const std::string & str);      // always
    std::string   qqif(const std::string & str);    // conditionally

    // String list functions
    stringlist diff(const stringlist & a, const stringlist & b);    // Returns a - b
    bool       has(const stringlist & a, const std::string & s);    // Does list a have string s?
    bool       remove(stringlist & a, const std::string & s);       // Removes string s from list a
}

/*!
@file

@typedef stringlist - A list (std::vector) of text objects.
@typedef buffer   - An object to hold BLOB data (std::string).
@typedef hash     - An object to hold a set of name/value pairs.
@typedef cihash   - Same as job::hash except that keys are case-insensitive

@class   job::ciless *  @brief Used by job::cihash for facilitating case-insensitive keys.

 @fn std::string job::dbl2str(const double d)
  @brief double precision to string

 @fn std::string job::int2str(const int i)
  @brief integer to string

 @fn std::string unt2si(const uint64_t n, const int digits = 3, const char* suffix = "")
   @brief unsigned 64bit integer to string with SI-suffix.
   Returns the number with the SI suffix; 1234 => 1.23k, 4567890 => 4.57M, 45678901 => 45.7M
   'n' is the number to convert.
   'digits' is how many digits to display, 0=all possible; note < 3 may still be 3.
   'suffix' is any units suffix such as 'm' for meters, 'Bps' for bytes per second, etc.

 @fn std::string job::join(const stringlist & list, const char d = ' ')
   @brief concatinates the parts of a string list into a single string

 @fn std::string job::join(const stringlist & list, const std::string & d)
   @brief concatinates the parts of a string list into a single string

 @fn std::string join(const hash& hash, const std::string & mapping = " -> ", const std::string & delimiter = " " )
   @brief concatinates a map to a string

 @fn stringlist job::split(const std::string & s, const std::string & d, const int parts = 0)
   @brief splits a string into parts at the specified string delimiter

 @fn std::string job::tail(const std::string & s, const unsigned int amt)
   @brief get the tail-end of a string
   Like substring but works back from the end

 @fn bool job::is_head(const std::string & s, const std::string & head)
   @brief check the head of a string

 @fn std::string & job::lc(std::string & s)
   @brief lowercase the string
   @note Modifies the original string.

 @fn std::string & job::uc(std::string & s)
   @brief uppercase the string
   @note Modifies the original string.

 @fn std::string & job::ltrim(std::string & s)
   @brief trim start of string
   @note Modifies the original string.

 @fn std::string & job::rtrim(std::string & s)
   @brief trim end of string
   @note Modifies the original string.

 @fn std::string & job::trim(std::string & s)
   @brief trim both ends of string
   @note Modifies the original string.

 @fn std::string qq(const std::string & str)
   Quotes the string (make it safe for use on the shell /bin/bash).
   Quotes the string whether it needs it or not.
   For conditional protection, see qqif()

 @fn std::string qqif(const std::string & str)
   Quotes the string (make it safe for use on the shell /bin/bash),
   only if shell metacharacters are found in the string.
   This is the conditional form of qq().
   To always quote the string, use qq().

 @fn stringlist::const_iterator find_string(const stringlist & input, const std::string & strToFind);
   @brief Finds the string inside a list

 @fn bool contains_string(const stringlist & input, const std::string & strToFind);
   @brief Tests for a string presence inside a list

*/

#endif
