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

// This class adds an int 'errno' like value to objects, and a string reason text.
//  If the errval is set, that overrides the string text (the string is derived from the int).
//  If the errstr is set, then the string is used instead.

#include "job/status.hxx"
#include <cstring>
#include <netdb.h>        // for gai_strerror()
#include <sys/types.h>    // for gai_strerror()
#include <sys/socket.h>   // for gai_strerror()

job::status::status(const int e, const std::string & prior) {
    set(e, prior);
}

job::status::status(const int e, const status & prior) {
    set(e, prior);
}

job::status::status(const std::string & s, const std::string & prior) {
    set(s, prior);
}

job::status::status(const std::string & s, const status & prior) {
    set(s, prior);
}

job::status::~status() {
}

// Getters
job::status::operator int() const {
    return errval;
}

job::status::operator std::string() const {
    return tostr();
}

char job::status::laststr[1024]; // Needed for the next function

const char* job::status::c_str() const {
    strncpy(laststr, tostr().c_str(), sizeof laststr);
    laststr[sizeof(laststr)-1] = 0x0;
    return laststr;
}

bool job::status::is_ok() const {
    return errval == ERR_OK;
}

bool job::status::is_error() const {
    return errval != ERR_OK;
}

// Setters
job::status job::status::operator = (const int e) {
    this->set(e);
    return *this;
}

job::status job::status::operator = (const std::string & s) {
    this->set(s);
    return *this;
}

job::status job::status::operator += (const int e) {
    if (errval) {
        errstr.append("; ");    // Note a semicolon to imply a list, not a colon
        errstr.append(job::status(e));
    }
    else
        this->set(e);
    return *this;
}

job::status job::status::operator += (const std::string & s) {
    if (errval) {
        errstr.append("; ");    // Note a semicolon to imply a list, not a colon
        errstr.append(s);
    }
    else
        this->set(s);
    return *this;
}

job::status job::status::operator += (const job::status & p) {
    if (errval) {
        errstr.append("; ");    // Note a semicolon to imply a list, not a colon
        errstr.append((std::string)p);
    }
    else {
        errval = p.errval;
        errstr = p.errstr;
    }
    return *this;
}

int job::status::set(const int e, const std::string & cause) {
    errval = e;
    errstr.clear();
    if (cause.length()) errstr = cause;
    return errval;
}

int job::status::set(const int e, const status & prior) {
    errval = e;
    errstr.clear();
    if (&prior) errstr = prior.tostr();
    return errval;
}

int job::status::set(const std::string & s, const std::string & cause) {
    errval = ((s != "") && (s != "OK"))? ERR_USE_STR : ERR_OK;
    errstr = s;
    if (cause.length()) {
        errstr.append(": ");
        errstr.append(cause);
    }
    return errval;
}

int job::status::set(const std::string & s, const status & prior) {
    errval = ((s != "") && (s != "OK"))? ERR_USE_STR : ERR_OK;
    errstr = s;
    if (&prior) {
        errstr.append(": ");
        errstr.append(prior);
    }
    return errval;
}

// Stringifier
const std::string job::status::tostr() const {
    if (errval == ERR_OK)
        return "OK";
    else if (errval == ERR_USE_STR)
        return errstr;

    // Lookup message for this errval
    //  For some, strerror() gives a confusing message, so we provide our own instead
    std::string s = "";
         if (errval == ERR_LOCKED)   s = "Job locked by another";
    else if (errval == ERR_MOVED)    s = "Job moved by another";
    else if (errval == ERR_INITING)  s = "Job initialising";
    else if (errval == ERR_PENDING)  s = "Job pending";
    #ifndef WIN32
    else if (errval < 0)             s = gai_strerror(errval);  // n/a on win - codes not negative.
    #endif
    else                             s = strerror(errval);

    // Any prior causing reason?  Append it.
    if (errstr.length()) {
        if ((errstr.length() >= 2) && (errstr.substr(0,2) != "; ")) s.append(": ");
        s.append(errstr);
    }
    return s;
}

