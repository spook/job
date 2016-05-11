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
#include "tap-extra.hxx"

#include <fcntl.h>
#include <fstream>      // std::ifstream
#include <sstream>
#include <stdio.h>
#include <sys/types.h>
#include <regex.h>
#include <unistd.h>

using namespace std;
using namespace TAP;
using job::int2str;

// Test against a regex
bool TAP::like(const std::string & str, const std::string & pat, std::string comment) throw() {
    if (getenv("TAP_LIKE")) {
        diag("like:");
        diag(str);
        diag(pat);
    }
    regex_t re;
    int err = regcomp(&re, pat.c_str(), REG_EXTENDED | REG_NOSUB | REG_ICASE | REG_NEWLINE);
    if (!err) {
        err = regexec(&re, str.c_str(), 0/*nmatch*/, NULL/*pmatch*/, 0);
        regfree(&re);
    }
    if (err && (err != REG_NOMATCH)) {
        comment += " (regcomp fail: ";
        char errbuf[256];
        errbuf[0] = '\0';
        regerror(err, &re, errbuf, sizeof(errbuf));
        comment += errbuf;
        comment += ")";
        return ok(false, comment);
    }
    if (err) diag("For test: "+comment+"\n#  Got:  "+str+"\n#  Like: "+pat);
    return ok(err == 0, comment);
}

// Why not call !like(...)?  It won't work.  The "ok" count will be screwed up.
bool TAP::not_like(const std::string & str, const std::string & pat, std::string comment) throw() {
    if (getenv("TAP_LIKE")) {
        diag("not-like:");
        diag(str);
        diag(pat);
    }
    regex_t re;
    int err = regcomp(&re, pat.c_str(), REG_EXTENDED | REG_NOSUB | REG_ICASE | REG_NEWLINE);
    if (!err) {
        err = regexec(&re, str.c_str(), 0/*nmatch*/, NULL/*pmatch*/, 0);
        regfree(&re);
    }
    if (err && (err != REG_NOMATCH)) {
        comment += " (regcomp fail: ";
        char errbuf[256];
        errbuf[0] = '\0';
        regerror(err, &re, errbuf, sizeof(errbuf));
        comment += errbuf;
        comment += ")";
        return ok(false, comment);
    }
    if (err) diag("For test: "+comment+"\n#  Got:  "+str+"\n# !Like: "+pat);
    return ok(err == REG_NOMATCH, comment);
}

// Is 'b' in 's'?
bool TAP::has(std::string & s, const char* b, const char* msg) {
    bool f = s.find(b) != s.npos;
    ok(f, msg);
    if (!f) note("Why: "+s);
    return f;
}

// Capture print output
static int test_n = 0;
test_capture_output::test_capture_output(const int fd) 
    : orig_fd(fd), back_fd(-1) {

    // Make the temporary file name
    char base[32];
    snprintf(base, sizeof(base), "test.%d.%d.tmp", getpid(), test_n++);
    fnam = std::string("/tmp/") + base;  //XXX

    // redirect the output to this file
    back_fd = dup(orig_fd);     // Make a copy of the original
    int temp_fd = open(fnam.c_str(), 
                       O_CREAT | O_WRONLY,
                       S_IRWXU | S_IRWXG);
    if (temp_fd < 0) {
        string why = "*** Failed capture to temp file ";
        why += fnam;
        why += ": ";
        why += (std::string)IO_status;
        bail_out(why);
    }
    dup2(temp_fd, orig_fd);     // take over the output fd
    close(temp_fd);             // close the fd that started the file
}

// End redirection and slurp in the file
std::string & test_capture_output::output() {
    // Already got the output?
    if (orig_fd < 0) return out;

    // We're done with the redirect
    dup2(back_fd, orig_fd);     // Put it back
    close(back_fd);
    orig_fd = -1;
    back_fd = -1;

    // Slurp in the file we just wrote
    int fd = open(fnam.c_str(), O_RDONLY);
    if (fd < 0) {
        string why = "*** Failed to open captured output file ";
        why += fnam;
        why += ": ";
        why += (std::string)IO_status;
        bail_out(why);
    }
    char buf[4096];
    ssize_t amt = 0;
    while ((amt = read(fd, &buf, sizeof(buf))) > 0) out.append(buf, amt);
    close(fd);
    if (amt < 0) {
        string why = "*** Failed to read captured output file ";
        why += fnam;
        why += ": ";
        why += (std::string)IO_status;
        bail_out(why);
    }

    // Nuke the temp file
    unlink(fnam.c_str());

    // Done
    return out;
}

test_capture_output::~test_capture_output() {
    if (orig_fd >= 0) {
        dup2(back_fd, orig_fd);
        close(back_fd);
        unlink(fnam.c_str());
    }
}

int test_end() {
    done_testing();
    int bad = exit_status();
    if (bad == 255) {
        note("Failed to run the planned number of tests");
    }
    else if (bad) {
        std::string num = exit_status() < 254? int2str(exit_status()) : "many";
        note("Looks like you failed " + num 
                             + " of " + int2str(planned()) + " tests");
    }
    else {
        note("Passed all " + int2str(planned()) +" tests");
    }
    return exit_status();
}

