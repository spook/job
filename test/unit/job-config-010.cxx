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

// Test script for job::config

//#include "stdafx.hpp"
#include "job/config.hxx"
#include "job/log.hxx"
#include "tap++/tap++.hxx"
#include "tap-extra.hxx"
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace job;
using namespace TAP;
using job::int2str;

// Helper class for doing tests
//  This class takes the given contents and puts it into a temporary file,
//  which is used as the config-file-under-test.
//  Note: This is only suitable for small contents, as we do only one write(2).
class test_config : public job::config 
    {
    public:
        test_config(const std::string & contents) 
        : job::config("this-will-fail-just-ignore-it")
        {
            static int fnum = 0;
            fnam = "test/tmp/job-config-010-" + int2str(++fnum) + ".tmp";
            int fd = open(fnam.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
            if (fd < 0) die("*** Cannot create temporary config file %s: %s", fnam, IO_status);
            ssize_t amt = write(fd, contents.c_str(), contents.size());
            if (amt < 0) die("*** Cannot write temporary config file %s: %s", fnam, IO_status);
            if ((size_t)amt != contents.size())
                die("*** Cannot write full contents, got %d/%d, file %s", amt, contents.size(), fnam);
            close(fd);
            load(fnam);
        }
        ~test_config() {
            unlink(fnam.c_str());
        }

    private:
        std::string fnam;
};

int main(int argc, char* argv[]) {
    plan(88);

    // Missing file test
    {
        job::config cfg("bogus-file.foo");
        is((std::string)cfg.error, 
            "Cannot open bogus-file.foo: No such file or directory", 
            "Try to load a bogus file");
    }

    // Test that we flag bad configs
    {
        test_config cfg0(":123");
        is((std::string)cfg0.error, "Bad tag at line 1", "empty tag");

        test_config cfg1("+z:123");
        is((std::string)cfg1.error, "Bad tag at line 1", "bad identifier char in tag");

    // Any char can be a section name!  Do we care?
    //  test_config cfg2("[a/5]\nx:123\n");
    //  is((std::string)cfg2.error, "", "bad section chars");

        test_config cfg3("[sec\nx:123\n");
        is((std::string)cfg3.error, "No section terminator at line 1", "missing section close");

        test_config cfg4("b+4:123");
        is((std::string)cfg4.error, "No delimiter or bad tag char at line 1", "bad tag chars");

        test_config cfg5("bad");
        is((std::string)cfg5.error, "Tag with no value at line 1", "no ':' delimiter");
    }

    // Load whitespace-flavors
    {
        test_config cfg0("");               isok(cfg0, "Empty file");
        test_config cfg1(" ");              isok(cfg1, "Blank file");
        test_config cfg2("\n");             isok(cfg2, "Newline file");
        test_config cfg3("\t");             isok(cfg3, "Tab file");
        test_config cfg4("\n \n  \n\t \n"); isok(cfg4, "Mixed whitespace file");
    }

    // Load empties with comments
    {
        test_config cfg0("#");                  isok(cfg0, "Comment but Empty file");
        test_config cfg1("# ");                 isok(cfg1, "Comment but Blank file");
        test_config cfg2("#\n");                isok(cfg2, "Comment but Newline file");
        test_config cfg3("#\t");                isok(cfg3, "Comment but Tab file");
        test_config cfg4("#\n# \n#  \n#\t \n#");isok(cfg4, "Comment but Mixed whitespace file");
        test_config cfg5("#\n#  hi\n  #\n\t #");isok(cfg5, "Leading blank comments");
    }


    // Default section only
    {
        test_config cfg(
            "# default section only \n"
            "a:1\n"
            "bb: 22\n"
            "ccc :333\n"
            "dddd : 4444\n"
            " eeeee \t : \t 55555 \n"
            "\tffffff : inner space \t \n"
            "ggggggg:\t  inner\ttab  and \t space \t\t \n"
            " #  and blank values\n"
            "\t zzzz:\n"
            " \tzzz \t: \n"
            "\t zz \t: \t\n"
            " \tz \t:\t  \n"
            " n : 0 \n"
            " nn: 00\n"
            "ooo:-55 \n"
            "\n"
            "i.got_dots-123-dashes$.- : misc, etc\n"
            "final:end"     // no trailing newline
        );
        isok(cfg, "default section with various items");
        is((int)cfg.size(), 1, "one section");
        is(cfg[""]["a"],       "1",     "value a");
        is(cfg[""]["bb"],      "22",    "value bb");
        is(cfg[""]["ccc"],     "333",   "value ccc");
        is(cfg[""]["dddd"],    "4444",  "value dddd");
        is(cfg[""]["eeeee"],   "55555", "value eeeee");
        is(cfg[""]["ffffff"],  "inner space",               "value with inner space");
        is(cfg[""]["ggggggg"], "inner\ttab  and \t space",  "value with inner tabs");

        is(cfg[""]["zzzz"],    "",      "blank value zzzz");
        is(cfg[""]["zzz"],     "",      "blank value zzz");
        is(cfg[""]["zz"],      "",      "blank value zz");
        is(cfg[""]["z"],       "",      "blank value z");

        is(cfg[""]["n"],       "0",     "zero value n");
        is(cfg[""]["nn"],      "00",    "zeros value nn");
        is(cfg[""]["ooo"],     "-55",   "negative value ooo");

        is(cfg[""]["i.got_dots-123-dashes$.-"], "misc, etc", "dots, dashes, and so on");
        is(cfg[""]["I.gOt_dOtS-123-dASheS$.-"], "misc, etc", "dots, dashes, case blind");

        is(cfg[""]["final"],   "end",   "final value");

        is(cfg[""]["A"],       "1",     "value a case blind");
        is(cfg[""]["zZ"],      "",      "value zz case blind");
        is(cfg[""]["CcC"],     "333",   "value ccc case blind");

        ok(!cfg.exists("", ""),     "exists nothing");
        ok(!cfg.exists("", "nada"), "exists missing tag");
        ok(cfg.exists("", "a"),     "exists a");
        ok(cfg.exists("", "A"),     "exists a caseblind");
        ok(cfg.exists("", "zz"),    "exists zz");
        ok(cfg.exists("", "zZ"),    "exists zz caseblind");
        ok(cfg.exists("", "ccc"),   "exists ccc");
        ok(cfg.exists("", "cCc"),   "exists ccc caseblind");
        ok(cfg.exists("", "n"),     "exists n");
        ok(cfg.exists("", "i.got_dots-123-dashes$.-"), "exists misc");
        ok(cfg.exists("", "I.gOt_dOtS-123-dASheS$.-"), "exists misc caseblind");

        is(cfg.get("", ""),             "",     "get nothing");
        is(cfg.get("", "", "hi"),       "hi",   "get nothing w/default");
        is(cfg.get("", "nada"),         "",     "get missing");
        is(cfg.get("", "nada", "yo"),   "yo",   "get missing w/default");
        is(cfg.get("", "final"),        "end",  "get existing");
        is(cfg.get("", "final", "def"), "end",  "get existing w/default");
        is(cfg.get("", "z"),            "",     "get existing blank");
        is(cfg.get("", "z", "def"),     "",     "get existing blank w/default");

        is(cfg.geti("", ""),          0, "geti nothing");
        is(cfg.geti("", "", 7),       7, "geti nothing w/default");
        is(cfg.geti("", "nada"),      0, "geti missing");
        is(cfg.geti("", "nada", 8),   8, "geti missing w/default");
        is(cfg.geti("", "n"),         0, "geti n");
        is(cfg.geti("", "n", 9),      0, "geti n w/default");
        is(cfg.geti("", "nn"),        0, "geti nn");
        is(cfg.geti("", "nn", 6),     0, "geti nn w/default");
        is(cfg.geti("", "ooo"),     -55, "geti n");
        is(cfg.geti("", "ooo", 5),  -55, "geti n w/default");
        is(cfg.geti("", "a"),         1, "geti a");
        is(cfg.geti("", "a"),         1, "geti a w/default");
        is(cfg.geti("", "bb"),       22, "geti bb");
        is(cfg.geti("", "ccc"),     333, "geti ccc");
        is(cfg.geti("", "dddd"),   4444, "geti dddd");
        is(cfg.geti("", "eeeee"), 55555, "geti eeeee");

        is(cfg.geti("", "final"),     0, "geti non-number");
        is(cfg.geti("", "final", 4),  0, "geti non-number w/default");  // should still be 0

        is(cfg.to_string(),
            "# default section only \n"
            "#  and blank values\n"
            "a:           1\n"
            "bb:          22\n"
            "ccc:         333\n"
            "dddd:        4444\n"
            "eeeee:       55555\n"
            "ffffff:      inner space\n"
            "final:       end\n"
            "ggggggg:     inner	tab  and 	 space\n"
            "i.got_dots-123-dashes$.-: misc, etc\n"
            "n:           0\n"
            "nn:          00\n"
            "ooo:         -55\n"
            "z:           \n"
            "zz:          \n"
            "zzz:         \n"
            "zzzz:        \n"
                , "to_string");
    }

    // Named sections, no default section
    {
        test_config cfg(
            "# Test job::confg file\n"
            "# Named sections, no default section\n"
            "[monsters]  \n"
            "  Afrit    : (pl. Afriti) Arabic fire spirit.\n"
            "  Alfar    : (see also Dockalfar and Liosalfar, see Elves): Norse equivalent to Elves.\n"
            "  Amphisbaena : A two headed venomous serpent\n"
            "\n"
            "[ Demons ]\n"  // note spaces around section
            "  Nybbas : obscure, 1863\n"
            "  Yan-gant-y-tan : lesser known\n"
            "  Seraphim : serpent demons\n"
            "#  Zepar : Pseudomonarchia daemonum (commented-out)\n"
        );
        isok(cfg, "Named sections, no default section");

        ok(cfg.exists("demons", "nybbas"), "exists by section");
        is(cfg["demons"]["nybbas"],        "obscure, 1863",                    "section 1 lookup");
        is(cfg["monsters"]["Amphisbaena"], "A two headed venomous serpent",    "section 2 lookup");
        is(cfg.get("demons", "seraphim"),  "serpent demons",                   "get from section 2");
        is(cfg.get("mOnStErS", "AFRIT"),   "(pl. Afriti) Arabic fire spirit.", "get from section 1");

        is(cfg.to_string(),
            "# Test job::confg file\n"
            "# Named sections, no default section\n"
            "\n"
            "[Demons]\n"
            "#  Zepar : Pseudomonarchia daemonum (commented-out)\n"
            "Nybbas:      obscure, 1863\n"
            "Seraphim:    serpent demons\n"
            "Yan-gant-y-tan: lesser known\n"
            "\n"
            "[monsters]\n"
            "Afrit:       (pl. Afriti) Arabic fire spirit.\n"
            "Alfar:       (see also Dockalfar and Liosalfar, see Elves): Norse equivalent to Elves.\n"
            "Amphisbaena: A two headed venomous serpent\n"
                , "to_string");
    }

    // default and named sections with comments
    {
        test_config cfg(
            "# Test job::confg file\n"
            "# Default and named sections\n"
            "#\n"
            "mode: list\n"
            "phase: full\n"
            "\n"
            "  [  pluto  ]  \n"
            "  moons : true\n"
            "# planet status was disputed\n"
            "  planet: false\n"
            "[ mars ]\n"
            " moons : true \n"
            " visited : soon \n"
            "   # This is the mars\n"
            "# multiline comment\n"
        );
        isok(cfg, "Default and named sections with comments");
        is(cfg.get("",      "mode"),  "list", "get from default");
        is(cfg.get("pluto", "moons"), "true", "get from section");
        is(cfg.to_string(),
            "# Test job::confg file\n"
            "# Default and named sections\n"
            "#\n"
            "mode:        list\n"
            "phase:       full\n"
            "\n"
            "[mars]\n"
            "# This is the mars\n"
            "# multiline comment\n"
            "moons:       true\n"
            "visited:     soon\n"
            "\n"
            "[pluto]\n"
            "# planet status was disputed\n"
            "moons:       true\n"
            "planet:      false\n"
                , "to_string");
    }

    return test_end();
}

