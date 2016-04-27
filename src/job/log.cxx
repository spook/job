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

#include "job/log.hxx"
#include "job/string.hxx"
#include "job/path.hxx"
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/time.h>
#include <unistd.h>

/// TODO: << un-do the use of streams >>

std::string     job::logger::last         = "";
job::loglevel_t job::logger::cur_level    = LOGINFO;
const char*     job::logger::cur_name     = NULL;
bool            job::logger::show_header  = true;
bool            job::logger::show_newline = true;
bool            job::logger::show_pid     = false;
bool            job::logger::force_log    = false;
int             job::logger::dummy        = 0;
job::logger     loggerobj;

job::logger & job::logger::obj(loglevel_t level, const char* file, int line, const char* format) {
    return loggerobj.set(level, file, line, format);
}

job::logger & job::logger::obj(loglevel_t level, const char* file, int line, const std::string & format) {
    return loggerobj.set(level, file, line, format.c_str());
}

job::logger::logger() 
        : i(0)
        , lvl(job::LOGFATAL)
        , fil("")
        , lin(0)
        , fmt("")
{
    char * ev = getenv("JOB_LOG_LEVEL");
    if (ev) set_level(ev);
    openlog("job", LOG_ODELAY || LOG_PID, LOG_USER);
}

job::logger::~logger() {}

std::string job::logger::head() {

    // Get the string representation of the log level
    const char* lvls = "---";
    if      (lvl == LOGFATAL)   lvls = "FTL";
    else if (lvl == LOGERROR)   lvls = "ERR";
    else if (lvl == LOGWARN)    lvls = "WRN";
    else if (lvl == LOGINFO)    lvls = "INF";
    else if (lvl == LOGVERBOSE) lvls = "VBS";
    else if (lvl == LOGDEBUG)   lvls = "DBG";
    else if (lvl == LOGDUMP)    lvls = "DMP";

    // Strip off leading path info from the file name
    size_t p = fil.rfind('/');
    if (p != std::string::npos) fil = fil.substr(p+1);
    p = fil.rfind('\\');
    if (p != std::string::npos) fil = fil.substr(p+1);

    // Set up the buffer to hold the full header (file:line combo can be 64+9 chars long)
    char   buf[sizeof("LVL [file:line]")+64];
    int    bufsize = sizeof(buf);
    snprintf(buf, bufsize, "%s [%s:%i]", lvls, fil.c_str(), lin);
    return buf;
}

std::string job::logger::str() {
    return fmt;
}

int job::logger::out(const int logtype) {

    FILE* fp       =  logtype == LOGTYPE_LOG? stderr : stdout;
    bool do_header =  logtype == LOGTYPE_LOG;
    bool do_log    = (logtype == LOGTYPE_LOG) || force_log;

    std::string hd = head();
    if (show_header && do_header) {
        struct timeval tv;
        tv.tv_sec  = 0;
        tv.tv_usec = 0;
        gettimeofday(&tv, NULL);    // if error, not much we can do! so don't check
        tm*    t   = gmtime(&tv.tv_sec);
        fprintf(fp, "%2.2d:%2.2d:%2.2d.%3.3dZ ", 
                t->tm_hour, t->tm_min, t->tm_sec, int(tv.tv_usec/1000));
        if (show_pid) fprintf(fp, "%5d ", getpid());
        fprintf(fp, "%s ", hd.c_str());
    }

    fprintf(fp, "%s", fmt.c_str());
    if (show_newline) fprintf(fp, "\n");
    fflush(fp); // Always, even if stderr, 'cuz stderr may be redirected to an unbuffered file

    // Write to syslog
    if (do_log) {
        // Make our levels match the syslog levels
        // LOG_EMERG LOG_ALERT LOG_CRIT LOG_ERR  LOG_WARNING LOG_NOTICE LOG_INFO   LOG_DEBUG    n/a
        //   n/a        n/a    LOGFATAL LOGERROR LOGWARN     LOGINFO    LOGVERBOSE LOGDEBUG  LOGDUMP
        int prio = (int)lvl + 2;
        if (prio > LOG_DEBUG) prio = LOG_DEBUG;
        syslog(LOG_USER || prio, "%s %s", hd.c_str(), fmt.c_str());
    }

    last = fmt;
    return 1;
}

void job::logger::out_n_die(int xstat, const int logtype) {
    out(logtype);
    exit(xstat);
}

job::loglevel_t job::logger::fromstring(std::string level) {
    lc(level);
    if (level[0] == 'f')           return LOGFATAL;
    if (level[0] == 'e')           return LOGERROR;
    if (level[0] == 'w')           return LOGWARN;
    if (level[0] == 'i')           return LOGINFO;
    if (level[0] == 'v')           return LOGVERBOSE;
    if (level.substr(0,2) == "db") return LOGDEBUG;     // dbg
    if (level.substr(0,2) == "de") return LOGDEBUG;
    if (level.substr(0,2) == "dm") return LOGDUMP;      // dmp
    if (level.substr(0,2) == "du") return LOGDUMP;
    if (level[0] == 'a')           return LOGALWAYS;
    if (level[0] == 's')           return LOGSILENT;    // Suppresses all logging
    return LOGWARN;
}

void job::logger::set_level(std::string level) {
    cur_level = fromstring(level);
}

job::logger & job::logger::set(loglevel_t level, const char* file, int line, const char* format) {
    i = 0;
    lvl = level;
    fil = file;
    lin = line;
    fmt = format;
    return *this;
}

job::logger & job::logger::operator,(const job::status & x) {
    char f;
    std::string ff;
    if (!get_chunk(f, ff)) return *this;
    char buf[4096];
    if (f == 's') 
        snprintf(buf, 4096, ff.c_str(), ((std::string)x).c_str());
    else if ((f == 'd') || (f == 'u')) 
        snprintf(buf, 256, ff.c_str(), (int)x);
    else if (f == 'c') 
        snprintf(buf, 256, ff.c_str(), (int)x? '!' : ' ');
    else 
        strcpy(buf, std::string(ff.size(), '#').c_str());
    fmt.replace(i, ff.size(), buf);
    i += strlen(buf);
    return *this;
}

job::logger & job::logger::operator,(const char* x) {
    char f;
    std::string ff;
    if (!get_chunk(f, ff)) return *this;
    char buf[8192];
    if (f == 's') 
        snprintf(buf, 8192, ff.c_str(), x);
    else if ((f == 'd') || (f == 'u')) 
        snprintf(buf, 256, ff.c_str(), strlen(x));
    else if (f == 'c') 
        snprintf(buf, 256, ff.c_str(), x[0]);
    else 
        strcpy(buf, std::string(ff.size(), '#').c_str());
    fmt.replace(i, ff.size(), buf);
    i += strlen(buf);
    return *this;
}

job::logger & job::logger::operator,(const std::string & x) {
    char f;
    std::string ff;
    if (!get_chunk(f, ff)) return *this;
    char buf[8192];
    if (f == 's') 
        snprintf(buf, 8192, ff.c_str(), x.c_str());
    else if ((f == 'd') || (f == 'u')) 
        snprintf(buf, 256, ff.c_str(), x.size());
    else if (f == 'c') 
        snprintf(buf, 256, ff.c_str(), x[0]);
    else if (f == 'x') {
        std::string tmp;
        for (size_t i=0; (i<x.size() && (tmp.size() < 1024)); ++i) {
            snprintf(buf, 256, "%02x", (unsigned char)x[i]);
            if (tmp.size()) tmp += " ";
            tmp += buf;
        }
        snprintf(buf, 8192, "%s", tmp.c_str());
    }
    else 
        strcpy(buf, std::string(ff.size(), '#').c_str());
    fmt.replace(i, ff.size(), buf);
    i += strlen(buf);
    return *this;
}

job::logger & job::logger::operator,(long long x) {
    char f;
    std::string ff;
    if (!get_chunk(f, ff)) return *this;
    char buf[256];
    if ((f == 'd') || (f == 'i') || (f == 'o') || (f == 'u') || (f == 'x') || (f == 'c'))
        snprintf(buf, 256, ff.c_str(), x);
    else if (f == 's') {
        char b[64];
        snprintf(b, 64, "%lld", x);
        snprintf(buf, 256, ff.c_str(), b);
    }
    else if ((f == 'e') || (f == 'f') || (f == 'g') || (f == 'a'))
        snprintf(buf, 256, ff.c_str(), (double)x);
    else if (f == 'p')
        snprintf(buf, 256, ff.c_str(), (void*)x);
    else
        strcpy(buf, std::string(ff.size(), '#').c_str());
    fmt.replace(i, ff.size(), buf);
    i += strlen(buf);
    return *this;
}

job::logger & job::logger::operator,(long long unsigned x) {
    char f;
    std::string ff;
    if (!get_chunk(f, ff)) return *this;
    char buf[256];
    if ((f == 'd') || (f == 'i') || (f == 'o') || (f == 'u') || (f == 'x') || (f == 'c'))
        snprintf(buf, 256, ff.c_str(), x);
    else if (f == 's') {
        char b[64];
        snprintf(b, 64, "%llu", x);
        snprintf(buf, 256, ff.c_str(), b);
    }
    else if ((f == 'e') || (f == 'f') || (f == 'g') || (f == 'a'))
        snprintf(buf, 256, ff.c_str(), (double)x);
    else if (f == 'p')
        snprintf(buf, 256, ff.c_str(), (void*)x);
    else
        strcpy(buf, std::string(ff.size(), '#').c_str());
    fmt.replace(i, ff.size(), buf);
    i += strlen(buf);
    return *this;
}

job::logger & job::logger::operator,(size_t x) {
    char f;
    std::string ff;
    if (!get_chunk(f, ff)) return *this;
    char buf[256];
    if ((f == 'd') || (f == 'i') || (f == 'o') || (f == 'u') || (f == 'x') || (f == 'c'))
        snprintf(buf, 256, ff.c_str(), x);
    else if (f == 's') {
        char b[32];
        snprintf(b, 32, "%lu", (unsigned long)x);
        snprintf(buf, 256, ff.c_str(), b);
    }
    else if ((f == 'e') || (f == 'f') || (f == 'g') || (f == 'a'))
        snprintf(buf, 256, ff.c_str(), (double)x);
    else if (f == 'p')
        snprintf(buf, 256, ff.c_str(), (void*)x);
    else
        strcpy(buf, std::string(ff.size(), '#').c_str());
    fmt.replace(i, ff.size(), buf);
    i += strlen(buf);
    return *this;
}

job::logger & job::logger::operator,(long x) {
    char f;
    std::string ff;
    if (!get_chunk(f, ff)) return *this;
    char buf[256];
    if ((f == 'd') || (f == 'i') || (f == 'o') || (f == 'u') || (f == 'x') || (f == 'c'))
        snprintf(buf, 256, ff.c_str(), x);
    else if (f == 's') {
        char b[32];
        snprintf(b, 32, "%ld", x);
        snprintf(buf, 256, ff.c_str(), b);
    }
    else if ((f == 'e') || (f == 'f') || (f == 'g') || (f == 'a'))
        snprintf(buf, 256, ff.c_str(), (double)x);
    else if (f == 'p')
        snprintf(buf, 256, ff.c_str(), (void*)x);
    else
        strcpy(buf, std::string(ff.size(), '#').c_str());
    fmt.replace(i, ff.size(), buf);
    i += strlen(buf);
    return *this;
}

job::logger & job::logger::operator,(int x) {
    char f;
    std::string ff;
    if (!get_chunk(f, ff)) return *this;
    char buf[256];
    if ((f == 'd') || (f == 'i') || (f == 'o') || (f == 'u') || (f == 'x') || (f == 'c'))
        snprintf(buf, 256, ff.c_str(), x);
    else if (f == 's') {
        char b[32];
        snprintf(b, 32, "%d", x);
        snprintf(buf, 256, ff.c_str(), b);
    }
    else if ((f == 'e') || (f == 'f') || (f == 'g') || (f == 'a'))
        snprintf(buf, 256, ff.c_str(), (double)x);
    else
        strcpy(buf, std::string(ff.size(), '#').c_str());
    fmt.replace(i, ff.size(), buf);
    i += strlen(buf);
    return *this;
}

job::logger & job::logger::operator,(bool x) {
    char f;
    std::string ff;
    if (!get_chunk(f, ff)) return *this;
    char buf[256];
    if (f == 'c')
        snprintf(buf, 256, ff.c_str(), x? 'Y' : 'N');
    else if ((f == 'd') || (f == 'i') || (f == 'o') || (f == 'u') || (f == 'x'))
        snprintf(buf, 256, ff.c_str(), x? 1 : 0);
    else if (f == 's') {
        snprintf(buf, 256, ff.c_str(), x? "true" : "false");
    }
    else
        strcpy(buf, std::string(ff.size(), '#').c_str());
    fmt.replace(i, ff.size(), buf);
    i += strlen(buf);
    return *this;
}

job::logger & job::logger::operator,(char x) {
    char f;
    std::string ff;
    if (!get_chunk(f, ff)) return *this;
    char buf[256];
    if ((f == 'c') || (f == 'd') || (f == 'i') || (f == 'o') || (f == 'u') || (f == 'x'))
        snprintf(buf, 256, ff.c_str(), x);
    else if (f == 's') {
        char b[32];
        snprintf(b, 32, "%c", x);
        snprintf(buf, 256, ff.c_str(), b);
    }
    else if ((f == 'e') || (f == 'f') || (f == 'g') || (f == 'a'))
        snprintf(buf, 256, ff.c_str(), (double)x);
    else
        strcpy(buf, std::string(ff.size(), '#').c_str());
    fmt.replace(i, ff.size(), buf);
    i += strlen(buf);
    return *this;
}

job::logger & job::logger::operator,(double x) {
    char f;
    std::string ff;
    if (!get_chunk(f, ff)) return *this;
    char buf[256];
    if ((f == 'e') || (f == 'f') || (f == 'g') || (f == 'a'))
        snprintf(buf, 256, ff.c_str(), x);
    else if (f == 's') {
        char b[64];
        snprintf(b, 64, "%f", x);
        snprintf(buf, 256, ff.c_str(), b);
    }
    else if ((f == 'd') || (f == 'i') || (f == 'o') || (f == 'u') || (f == 'x') || (f == 'c'))
        snprintf(buf, 256, ff.c_str(), (long)x);
    else
        strcpy(buf, std::string(ff.size(), '#').c_str());
    fmt.replace(i, ff.size(), buf);
    i += strlen(buf);
    return *this;
}

job::logger & job::logger::operator,(void* x) {
    char f;
    std::string ff;
    if (!get_chunk(f, ff)) return *this;
    char buf[256];
    if (f == 'p')
        snprintf(buf, 256, ff.c_str(), x);
    else if ((f == 'd') || (f == 'i') || (f == 'o') || (f == 'u') || (f == 'x') || (f == 'c'))
        snprintf(buf, 256, ff.c_str(), (long)x);
    else if (f == 's') {
        char b[32];
        snprintf(b, 32, "%p", x);
        snprintf(buf, 256, ff.c_str(), b);
    }
    else
        strcpy(buf, std::string(ff.size(), '#').c_str());
    fmt.replace(i, ff.size(), buf);
    i += strlen(buf);
    return *this;
}

// Get the next chunk of the format string
bool job::logger::get_chunk(char & f, std::string & ff) {
    size_t e = 0;
    while (true) {
        i = fmt.find('%', i);
        if (i == fmt.npos) return false;
        if ((i>0) && (fmt[i-1] == '\\')) {++i; continue;}
        e = fmt.find_first_of("sdicouxXeEfFgGaAp%CSnm", i+1);   // printf conversion specifiers
        if (e == fmt.npos) {++i; return false;}
        f = fmt[e];
        if (f == '%') {fmt.replace(i, e-i+1, "\\%"); i += 2; continue;}
        if (f > 'z') f = f - 'A' + 'a';
        ff = fmt.substr(i, e-i+1);
        return true;
    }
}

