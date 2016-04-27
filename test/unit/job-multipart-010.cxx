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

//  Basic tests for the job::multipart class.

#include "job/multipart.hxx"
#include "job/path.hxx"
#include "job/string.hxx"
#include "tap++/tap++.hxx"
#include "tap-extra.hxx"

#include <stdio.h>          // fopen, ...
#include <stdlib.h>         // mkstemps()
#include <string>
#include <string.h>         // memset()
#include <linux/limits.h>   // PATH_MAX
#include <unistd.h>         // unlink(), write(), readlink()

using namespace std;
using namespace TAP;

// Test helper class - makes the file with the given content; deletes it when out of scope.
class multifile {
    public:
        static string tmpdir;
        string filename;

        multifile(const string & content) {
            string tpl = tmpdir + "/job-multi-010-XXXXXX.tmp";
            int fd = mkstemps((char*)tpl.c_str(), 4); // 4 = len of suffix ".tmp"
            if (fd < 0) 
                bail_out("Cannot create temporary file, template: "+tpl);
            ssize_t amt = write(fd, content.c_str(), content.size());
            if (amt < 0) 
                bail_out("Cannot write temporary file: "+job::int2str(errno));
            if (amt < (int)content.size()) 
                bail_out("Cannot write whole content");
            string fdlink = "/proc/self/fd/" + job::int2str(fd);
            char filepath[PATH_MAX];
            memset(filepath, '\0', PATH_MAX);   // keep valgrind quiet
            if (readlink(fdlink.c_str(), filepath, sizeof(filepath)) < 0) 
                bail_out("Cannot get path of temporary file");
            filename = filepath;
            close(fd);
        }

        ~multifile() {
            if (filename.size()) unlink(filename.c_str());
        }
};
string multifile::tmpdir = "";

int main(int argc, char* argv[]) {

    plan(122);

    // Paths
    multifile::tmpdir = job::path.tsttmp;
    note("Temporary files will be here: " + multifile::tmpdir);

    // --- Empty or small files ---
    {
        note("  -- empty 1 --");
        multifile mf("");
        job::multipart mp;
        mp.load(mf.filename);
        is(mp.error, job::ERR_OK, "no load error");
        is(mp.size(), 0, "section count");
        is(mp.to_string(), "", "to_string()");
    }

    {
        note("  -- empty 2 (blank) --");
        multifile mf(" ");
        job::multipart mp;
        mp.load(mf.filename);
        is(mp.error, job::ERR_OK, "no load error");
        is(mp.size(), 0, "section count");
        is(mp.to_string(), "", "to_string()");
    }

    {
        note("  -- empty 3 (newline) --");
        multifile mf("\n");
        job::multipart mp;
        mp.load(mf.filename);
        is(mp.error, job::ERR_OK, "no load error");
        is(mp.size(), 0, "section count");
        is(mp.to_string(), "", "to_string()");
    }

    {
        note("  -- empty 4 (whitespace) --");
        multifile mf(" \n  \n\t\n");
        job::multipart mp;
        mp.load(mf.filename);
        is(mp.error, job::ERR_OK, "no load error");
        is(mp.size(), 0, "section count");
        is(mp.to_string(), "", "to_string()");
    }

    // Single section head only
    {
        note("  -- small 1 --");
        multifile mf("a:b");
        job::multipart mp;
        mp.load(mf.filename);
        is(mp.error, job::ERR_OK, "no load error");
        is(mp.size(), 1, "section count");
        is(mp[0].size(), 1, "sec 0 item count");
        ok(mp.exists(0, "a"), "item exists");
        ok(mp.exists(0, "A"), "uc item exists");
        is(mp[0]["a"], "b", "value correct");
        is(mp.get(0, "A"), "b", "get() value correct");
        like(mp.to_string(), "a:\\s*b\n", "to_string()");
    }

    {
        note("  -- small 2 --");
        multifile mf(
            "a : b\n"
            "c :ddd\n"
            " e: ffff   \n"     // note trailing spaces
            "# comment\n"
            " gg:\thh ii jj  "  // note tab, trailing spaces and no \n
        );
        job::multipart mp;
        mp.load(mf.filename);
        is(mp.error, job::ERR_OK, "no load error");
        is(mp.size(), 1, "section count");
        is(mp[0].size(), 4, "sec 0 item count");
        ok(mp.exists(0, "a"), "item exists");
        ok(mp.exists(0, "A"), "uc item exists");
        is(mp[0]["a"], "b", "value correct");
        is(mp.get(0, "A"), "b", "get() value correct");
        is(mp[0]["c"], "ddd", "value correct");
        is(mp[0]["e"], "ffff", "value correct");
        is(mp[0]["gg"], "hh ii jj", "value correct");
    }

    // Single section with body
    {
        note("  -- head+body 0 --");
        multifile mf("a:b\n\n\n");
        job::multipart mp;
        mp.load(mf.filename);
        is(mp.error, job::ERR_OK, "no load error");
        is(mp.size(), 1, "section count");
        is(mp[0].size(), 2, "sec 0 item count - incl body");
        ok(mp.exists(0, "a"), "item exists");
        ok(mp.exists(0, "A"), "uc item exists");
        is(mp[0]["a"], "b", "value correct");
        is(mp.get(0, "A"), "b", "get() value correct");
        is(mp[0]["__BODY__"], "\n", "got body");
        like(mp.to_string(), "a:\\s*b\n\n\n", "to_string()");
    }

    {
        note("  -- head+body 1 --");
        multifile mf("a:b\n\ncorpse");
        job::multipart mp;
        mp.load(mf.filename);
        is(mp.error, job::ERR_OK, "no load error");
        is(mp.size(), 1, "section count");
        is(mp[0].size(), 2, "sec 0 item count - incl body");
        ok(mp.exists(0, "a"), "item exists");
        ok(mp.exists(0, "A"), "uc item exists");
        is(mp[0]["a"], "b", "value correct");
        is(mp.get(0, "A"), "b", "get() value correct");
        is(mp[0]["__BODY__"], "corpse", "got body");
        like(mp.to_string(), "a:\\s*b\n\ncorpse", "to_string()");
    }

    {
        note("  -- head+body 2 --");
        multifile mf("a:b\n\ncorpse\n");
        job::multipart mp;
        mp.load(mf.filename);
        is(mp.error, job::ERR_OK, "no load error");
        is(mp.size(), 1, "section count");
        is(mp[0].size(), 2, "sec 0 item count - incl body");
        ok(mp.exists(0, "a"), "item exists");
        ok(mp.exists(0, "A"), "uc item exists");
        is(mp[0]["a"], "b", "value correct");
        is(mp.get(0, "A"), "b", "get() value correct");
        is(mp[0]["__BODY__"], "corpse\n", "got body");
        like(mp.to_string(), "a:\\s*b\n\ncorpse\n", "to_string()");
    }

    {
        note("  -- head+body 3 --");
        multifile mf("a:b\n\n  one\b  two\n  three\n");
        job::multipart mp;
        mp.load(mf.filename);
        is(mp.error, job::ERR_OK, "no load error");
        is(mp.size(), 1, "section count");
        is(mp[0].size(), 2, "sec 0 item count - incl body");
        ok(mp.exists(0, "a"), "item exists");
        ok(mp.exists(0, "A"), "uc item exists");
        is(mp[0]["a"], "b", "value correct");
        is(mp.get(0, "A"), "b", "get() value correct");
        is(mp[0]["__BODY__"], "  one\b  two\n  three\n", "got body");
        like(mp.to_string(), "a:\\s*b\n\n  one\b  two\n  three\n", "to_string()");
    }

    {
        note("  -- head+body 4 --");
        multifile mf("a:b\n\n  one\b  two\n# three");
        job::multipart mp;
        mp.load(mf.filename);
        is(mp.error, job::ERR_OK, "no load error");
        is(mp.size(), 1, "section count");
        is(mp[0].size(), 2, "sec 0 item count - incl body");
        ok(mp.exists(0, "a"), "item exists");
        ok(mp.exists(0, "A"), "uc item exists");
        is(mp[0]["a"], "b", "value correct");
        is(mp.get(0, "A"), "b", "get() value correct");
        is(mp[0]["__BODY__"], "  one\b  two\n# three", "got body");
        like(mp.to_string(), "a:\\s*b\n\n  one\b  two\n# three", "to_string()");
    }

    // Single section body only
    {
        note("  -- body 0 --");
        multifile mf("\nhi");
        job::multipart mp;
        mp.load(mf.filename);
        is(mp.error, job::ERR_OK, "no load error");
        is(mp.size(), 1, "section count");
        is(mp[0].size(), 1, "sec 0 item count - incl body");
        is(mp[0]["__BODY__"], "hi", "got body");
        is(mp.to_string(), "\nhi", "to_string()");
    }

    {
        note("  -- body 1 --");
        multifile mf("\n\nnote leading newline");
        job::multipart mp;
        mp.load(mf.filename);
        is(mp.error, job::ERR_OK, "no load error");
        is(mp.size(), 1, "section count");
        is(mp[0].size(), 1, "sec 0 item count - incl body");
        is(mp[0]["__BODY__"], "\nnote leading newline", "got body");
        is(mp.to_string(), "\n\nnote leading newline", "to_string()");
    }
 
    {
        note("  -- body 2 --");
        multifile mf("\nnote trailing\nnewline\n");
        job::multipart mp;
        mp.load(mf.filename);
        is(mp.error, job::ERR_OK, "no load error");
        is(mp.size(), 1, "section count");
        is(mp[0].size(), 1, "sec 0 item count - incl body");
        is(mp[0]["__BODY__"], "note trailing\nnewline\n", "got body");
        is(mp.to_string(), "\nnote trailing\nnewline\n", "to_string()");
    }
    {
        note("  -- two-parts --");
        multifile mf(
            "a: b\n"
            "Content-Length: 1234\n"
            "Content-Type:   Multipart/Mixed; boundary=MYBOUNDARY123\n"
            "Thing-One: Yo\n"
            "\n"
            "Body for part zero\n"
            "is here"                   // Body zero does not end with \n
            "\n--MYBOUNDARY123\n"
            "Content-Type: text/plain\n"
            "Thing-Two: Mama!\n"
            "\n"
            "The body of\npart one\n"   // Body one ends with \n
            "\n--MYBOUNDARY123--\n"
        );
        job::multipart mp;
        mp.load(mf.filename);
        is(mp.error, job::ERR_OK, "no load error");
        is(mp.size(), 2, "section count");
        is(mp.boundary, "MYBOUNDARY123", "found boundary");

        is(mp[0].size(), 5, "sec 0 item count - incl body");
        is(mp[0]["__BODY__"], "Body for part zero\nis here", "sec 0 got body");
        ok(mp.exists(0, "a"), "sec 0 item exists");
        ok(mp.exists(0, "A"), "sec 0 uc item exists");
        is(mp[0]["a"], "b", "sec 0 value correct");
        is(mp.get(0, "A"), "b", "sec 0 get() value correct");
        ok(mp.exists(0, "thing-one"), "sec 0 item exists");
        ok(mp.exists(0, "Thing-One"), "sec 0 uc item exists");
        is(mp[0]["thing-one"], "Yo", "sec 0 value correct");
        is(mp.get(0, "Thing-One"), "Yo", "sec 0 get() value correct");

        is(mp[1].size(), 3, "sec 1 item count - incl body");
        is(mp[1]["__BODY__"], "The body of\npart one\n", "sec 1 got body");
        ok(mp.exists(1, "thing-two"), "sec 1 item exists");
        ok(mp.exists(1, "Thing-Two"), "sec 1 uc item exists");
        is(mp[1]["Thing-TWO"], "Mama!", "sec 1 value correct");
        is(mp.get(1, "Thing-Two"), "Mama!", "sec 1 get() value correct");

        is(mp.to_string(), 
            "a:             b\n"
            "Content-Length: 1234\n"
            "Content-Type:  Multipart/Mixed; boundary=MYBOUNDARY123\n"
            "Thing-One:     Yo\n"
            "\n"
            "Body for part zero\n"
            "is here"
            "\n--MYBOUNDARY123\n"
            "Content-Type:  text/plain\n"
            "Thing-Two:     Mama!\n"
            "\n"
            "The body of\npart one\n"
            "\n--MYBOUNDARY123--\n", "to_string()");
    }
    {
        note("  -- three-parts --");
        multifile mf(
            // Part Zero - only a single tag, no body
            "Content-Type:   Multipart/Mixed; boundary=XXXXXXXXXXXXX\n"
            "\n--XXXXXXXXXXXXX\n"
            // Part One - no tags, only body
            "\n"
            "The body of\npart one\n"   // Body one ends with \n
            "\n--XXXXXXXXXXXXX\n"
            // Part Two - both tags and body
            "Content-type: text/plain\n"
            "Trust-level:  C+\n"
            "\n"
            "Simple body of part two"
            "\n--XXXXXXXXXXXXX--\n"
        );
        job::multipart mp;
        mp.load(mf.filename);
        is(mp.error, job::ERR_OK, "no load error");
        is(mp.size(), 3, "section count");
        is(mp.boundary, "XXXXXXXXXXXXX", "found boundary");

        is(mp[0].size(), 1, "sec 0 item count - incl body");
        ok(mp.exists(0, "content-type"), "sec 0 item exists");

        is(mp[1].size(), 1, "sec 1 item count - incl body");
        is(mp[1]["__BODY__"], "The body of\npart one\n", "sec 1 got body");

        is(mp[2].size(), 3, "sec 2 item count - incl body");
        ok(mp.exists(2, "content-type"), "sec 2 item exists");
        is(mp.get(2, "trust-Level"), "C+", "sec 2 get() value correct");
        is(mp[2]["__BODY__"], "Simple body of part two", "sec 2 got body");

        is(mp.to_string(), 
            "Content-Type:  Multipart/Mixed; boundary=XXXXXXXXXXXXX\n"
            "\n--XXXXXXXXXXXXX\n"
            "\n"
            "The body of\npart one\n"
            "\n--XXXXXXXXXXXXX\n"
            "Content-type:  text/plain\n"
            "Trust-level:   C+\n"
            "\n"
            "Simple body of part two"
            "\n--XXXXXXXXXXXXX--\n"
                , "to_string()");
    }


    // Simple two-part


    // General-header lines


    // Fini
    return test_end();
}

