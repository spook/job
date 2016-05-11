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

//  Basic tests for the job::file class.

#include "job/file.hxx"
#include "job/path.hxx"
#include "job/string.hxx"
#include "tap++/tap++.hxx"
#include "tap-extra.hxx"

//#include "rda/utils.hpp"

#include <stdio.h>          // fopen, ...
#include <stdlib.h>         // mkstemps()
#include <string>
#include <linux/limits.h>   // PATH_MAX
#include <unistd.h>       // unlink(), write(), readlink()

using namespace std;
using namespace TAP;


int main(int argc, char* argv[]) {

    plan(85);
    job::path.set_root("./kit");

    // Inits
    unlink(job::path.seqfile.c_str());

    // Utilities
    is(job::state2str(job::hold),        "hold", "state2str(hold)");
    is(job::state2str(job::pend),        "pend", "state2str(pend)");
    is(job::state2str(job::run),         "run",  "state2str(run)");
    is(job::state2str(job::tied),        "tied", "state2str(tied)");
    is(job::state2str(job::kill),        "kill", "state2str(kill)");
    is(job::state2str(job::done),        "done", "state2str(done)");
    is(job::state2str((job::state_t)83), "???",  "state2str(-?-)");

    is((int)job::str2state("hold"), (int)job::hold, "str2state(hold)");
    is((int)job::str2state("pend"), (int)job::pend, "str2state(pend)");
    is((int)job::str2state("run"),  (int)job::run,  "str2state(run)");
    is((int)job::str2state("tied"), (int)job::tied, "str2state(tied)");
    is((int)job::str2state("kill"), (int)job::kill, "str2state(kill)");
    is((int)job::str2state("done"), (int)job::done, "str2state(done)");
    is((int)job::str2state(""),     (int)job::unk,  "str2state() - empty");
    is((int)job::str2state(" "),    (int)job::unk,  "str2state( ) - blank");
    is((int)job::str2state("???"),  (int)job::unk,  "str2state( ) - ???");
    is((int)job::str2state("blah"), (int)job::unk,  "str2state(blah) - random word");
    is((int)job::str2state("Done"), (int)job::unk,  "str2state(Done) - wrong case");


    // Basics
    {
        note("  -- basic 1 --");
        job::file jf;
        isok(jf, "default constructor");
        like(jf.name(), "/batch/hold/t0946684799.p5.j0000001.$", "file name");
        jf.write();
        isok(jf, "write()");
        jf.state = job::pend;
        jf.repath();
        isok(jf, "repath(pend(same))");

        jf.submitter = "holder";
        jf.state = job::hold;
        jf.priority = 4;
        jf.repath();
        isok(jf, "repath(hold)");
        like(jf.name(), "/batch/hold/t0946684799.p4.j0000001.holder$", "file name");

        jf.submitter = "fast@sprinting.biz";
        jf.pid = 3333333;
        jf.state = job::run;
        jf.priority = 6;
        jf.repath();
        isok(jf, "repath(run)");
        like(jf.name(), "/batch/run/t0946684799.p6.j0000001.fast@sprinting.biz$", "file name");

        jf.submitter = "12.34.56.78:9000";
        jf.state = job::tied;
        jf.priority = 1;
        jf.repath();
        isok(jf, "repath(tied)");
        like(jf.name(), "/batch/tied/t0946684799.p1.j0000001.12.34.56.78:9000$", "file name");

        jf.submitter = "[fe80::ea37:34ff:fe3c:b876]:12345";
        jf.state = job::done;
        jf.priority = 9;
        jf.repath();
        isok(jf, "repath(done)");
        like(jf.name(), "/batch/done/t0946684799.p9.j0000001.\\[fe80::ea37:34ff:fe3c:b876]:12345$", "file name");

        note("Job file contents:");
        jf.command = "mklove";
        jf.mid     = 777;
        jf.mnode   = "mars";
        jf.type    = "nasa";
        jf.notify  = true;
        jf.args.push_back("--not");
        jf.args.push_back("java");
        jf.write();
        isok(jf, "write()");
        is(jf.to_string(), 
            "Command:       mklove\n"
            "Job-Arg-1:     --not\n"
            "Job-Arg-2:     java\n"
            "job-id:        1\n"
            "Job-MID:       777\n"
            "Job-MNode:     mars\n"
            "Job-PID:       0\n"
            "job-prio:      9\n"
            "job-queue:     batch\n"
            "job-state:     done\n"
            "Job-Type:      nasa\n"
            "Try-Limit:     100\n"
            "TTY-Notify:    yes\n",
                "to_string()");
        jf.remove();
        isok(jf, "remove()");
    }

    // In a zone
    {
        note("  -- zone 2 --");
        job::file::zone = 2;
        job::file jf;
        jf.submitter = "zoned.out";
        isok(jf, "default constructor");
        like(jf.name(), "/batch/hold/t0946684799.p5.j0000022.zoned.out$", "file name");
    }

    // Name parsing - bad cases
    note("  -- Name parsing: black smoke --");
    {
        job::file jf("");
        is((std::string)jf.error, "Bad jobfile path", "jf() blank");
    }
    {
        job::file jf("hold/t0946684799.p5.j0000022.joe");
        like((std::string)jf.error, "^Bad jobfile path: ", "jf() missing queue");
    }
    {
        job::file jf("/hold/t0946684799.p5.j0000022.joe");
        like((std::string)jf.error, "^Bad queue: ", "jf() empty queue");
    }
    {
        job::file jf("test//hold/t0946684799.p5.j0000022.joe");
        like((std::string)jf.error, "^Bad queue: ", "jf() empty queue leading");
    }
    {
        job::file jf("t0946684799.p5.j0000022.joe");
        like((std::string)jf.error, "^Bad jobfile path", "jf() missing queue and state");
    }
    {
        job::file jf("batch//t0946684799.p5.j0000022.joe");
        like((std::string)jf.error, "^Bad state: ", "jf() empty state");
    }
    {
        job::file jf("test/batch//t0946684799.p5.j0000022.joe");
        like((std::string)jf.error, "^Bad state: ", "jf() empty state deeper");
    }
    {
        job::file jf("batch/xxxx/t0946684799.p5.j0000022.joe");
        like((std::string)jf.error, "^Bad state: ", "jf() bad state");
    }
    {
        job::file jf("test/batch/hold/t0946684799.p5.j0000022");
        like((std::string)jf.error, "^Bad jobfile format: ", "jf() missing submitter dot");
    }
    {
        job::file jf("test/batch/hold/t0946684799.p5.j0000022.");
        like((std::string)jf.error, "^Bad jobfile format: ", "jf() missing submitter");
    }
    {
        job::file jf("test/batch/hold/t.p5.j0000022.joe");
        like((std::string)jf.error, "^Bad jobfile format: ", "jf() missing time");
    }
    {
        job::file jf("test/batch/hold/t0946684799.p.j0000022.joe");
        like((std::string)jf.error, "^Bad jobfile format: ", "jf() missing prio");
    }
    {
        job::file jf("test/batch/hold/t0946684799.p0.j0000022.joe");
        like((std::string)jf.error, "^Bad priority: ", "jf() zero prio");
    }
    {
        job::file jf("test/batch/hold/t0946684799.p10.j0000022.joe");
        like((std::string)jf.error, "^Bad priority: ", "jf() big prio");
    }
    {
        job::file jf("test/batch/hold/t0946684799.p5.j.joe");
        like((std::string)jf.error, "^Bad jobfile format: ", "jf() missing job id");
    }

    note("  -- Name parsing: white smoke --");
    {
        job::file jf("test/batch/tied/t0.p1.j0.a");
        isok(jf, "minimal values");
        is(jf.queue,     "batch",   "  queue");
        is(jf.state,     job::tied, "  state");
        is(jf.run_time,  0,         "  run time");
        is(jf.priority,  1,         "  priority");
        is(jf.id,        0,         "  job id");
        is(jf.submitter, "a",       "  submitter");
    }
    {
        job::file jf("test/q/run/t1.p9.j1.z@z.zork");
        isok(jf, "low fencepost");
        is(jf.queue,     "q",        "  queue");
        is(jf.state,     job::run,   "  state");
        is(jf.run_time,  1,          "  run time");
        is(jf.priority,  9,          "  priority");
        is(jf.id,        1,          "  job id");
        is(jf.submitter, "z@z.zork", "  submitter");
    }
    {
        job::file jf("test/big-things/pend/t0946684799.p7.j1234567890.1.2.3.4:55555");
        isok(jf, "typical defaults");
        is(jf.queue,     "big-things",      "  queue");
        is(jf.state,     job::pend,         "  state");
        is(jf.run_time,  946684799,         "  run time");
        is(jf.priority,  7,                 "  priority");
        is(jf.id,        1234567890,        "  job id");
        is(jf.submitter, "1.2.3.4:55555",   "  submitter");
    }
    {
        job::file jf("test/batch/hold/t1111111111.p2.j0001234.[fe80::ea39:35ff:fe3c:b886]:65432");
        isok(jf, "typical two");
        is(jf.queue,     "batch",           "  queue");
        is(jf.state,     job::hold,         "  state");
        is(jf.run_time,  1111111111,        "  run time");
        is(jf.priority,  2,                 "  priority");
        is(jf.id,        1234,              "  job id");
        is(jf.submitter, "[fe80::ea39:35ff:fe3c:b886]:65432",   "  submitter");
    }
    {
        job::file jf("test/batch/done/t2147483647.p9.j4294967295.mary");
        isok(jf, "hi fencepost");
        is(jf.queue,     "batch",           "  queue");
        is(jf.state,     job::done,         "  state");
        is(jf.run_time,  2147483647,        "  run time");
        is(jf.priority,  9,                 "  priority");
        is(jf.id,        4294967295,        "  job id");
        is(jf.submitter, "mary",            "  submitter");
    }

    // Fini
    return test_end();
}

