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

#include "job/launch.hxx"
#include "job/seqnum.hxx"
#include <tap++/tap++.hxx>
#include "tap-extra.hxx"
#define __STDC_FORMAT_MACROS    // Enable PRI macros
#include <inttypes.h>           // PRI macros
#include <stdio.h>
#include <string>

using namespace job;
using namespace std;
using namespace TAP;
#define TESTDIR "test/tmp/"

int main (int argc, char* argv[]) {

    // Inits
    string seqfile = TESTDIR "seq.tmp";
    seqnum sq(seqfile);

    // sequence generator
    int n;
    if ((argc > 1) && (n = str2int(argv[1]))) {
        for (int i=0; i<n; i++) {
            printf("%" PRI_seqnum_t "\n", sq.next());
            if (sq.error) printf("*** Had error: %s\n", sq.error.c_str());
        }
        exit(0);
    }

    // Inits 2
    plan(32);

    // nuke the sequence file
    unlink(seqfile.c_str());

    // test 10 runs gives 1..10
    for (uint64_t u=1; u<=10; u++) {
        is(sq.next(), u, "initial sequence");
        is(sq.value, u, "exposed value");
        isok(sq, "error status ok");
    }

    // launch a bunch of generators in parallel
    note("Starting parallel generator test");
    #define NGENS 200
    std::string cmd = argv[0]; cmd += " 13000";
    std::string logs[NGENS];
    job::launch* gens[NGENS];
    for (int i=0; i<NGENS; i++) {
        char log[255];
        snprintf(log, sizeof(log), TESTDIR "sq.out.%d", i);
        logs[i] = log;
        gens[i] = new job::launch();
        gens[i]->command   = cmd;
        gens[i]->logfile   = log;
        gens[i]->kill_kids = true;
        gens[i]->start();
    }
    //(wait for them to finish)
    note("Waiting for generators to finish");
    for (int i=0; i<NGENS; i++) {
        gens[i]->wait(20);
        delete gens[i];
    }
    note("Checking results");
    cmd = "sort -n ";
    for (int i=0; i<NGENS; i++) {
        cmd += logs[i];
        cmd += " ";
    }
    cmd += "| uniq -d | wc -l | grep '^0$' 1>/dev/null";
    //note(cmd);
    int ret = system(cmd.c_str());
    is(ret, 0, "All values unique");
    // if test good, nuke the temp files
    if (!ret) {
        ok(system("rm " TESTDIR "sq.out.*") == 0, "removed temp files");
    }

    // Fini
    return test_end();
}
