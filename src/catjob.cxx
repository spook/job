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
#include "job/file.hxx"
#include "job/getopt.hxx"
#include "job/log.hxx"
#include "job/path.hxx"
#include "job/queue.hxx"
#include "job/status.hxx"
#include "job/string.hxx"
#include <sys/types.h>
#include <unistd.h>

// CLI options and usage help
enum  {opNONE, opHELP, opLOG,  opOUT, opRES, 
       opROOT, opTRY,  opVERB, opRAW };
const option::Descriptor usage[] = {
    {opNONE, 0, "",  "",          Arg::None, 
        "Dump the output from a batch job.\n\n"
        "Usage: catjob [options] job-id\n\n"
        "Options:" },
    {opHELP, 0, "h", "help",      Arg::None, "  -h  --help         Show this help message and exit"},
    {opLOG,  0, "l", "log-level", Arg::Reqd, "  -l  --log-level    Debugging log level (info, verbose, debug...)"},
    {opOUT,  0, "o", "output",    Arg::None, "  -o  --output       Show output (use with -r)"},
    {opRES,  0, "r", "result",    Arg::None, "  -r  --result       Show result status"},
    {opROOT, 0, "R", "root",      Arg::Reqd, "  -R  --root ROOT    Set file system root"},
    {opTRY,  0, "t", "try",       Arg::Reqd, "  -t  --try NUM      Show output from only this try"},
    {opVERB, 0, "v", "verbose",   Arg::None, "  -v  --verbose      Show more info"},
    {opRAW,  0, "w", "raw",       Arg::None, "  -w  --raw          Dump job file as-is (raw)"},
    {opNONE, 0, "",  "",          Arg::None, 
        "\n"
        "  Display the captured output from a batch job as well as the result(s)\n"
        "  of each run attempt (a 'try').  By default, all tries are shown, but\n"
        "  individual tries may be selected using --try.  The result status may\n"
        "  instead be shown using --result; to see both the output and the result\n"
        "  status use both --output and --result.  Additional details about the\n"
        "  job are shown using --verbose.\n"
        },
    {0,0,0,0,0,0}
};

//
// Main entry point
//
using job::ERR_OK;
using job::str2int;
using job::join;
using job::path;
int main(int argc, const char* argv[]) {

    // Inits
    job::logger::set_level();

    // Parse CLI options
    job::getopt cli(usage, opNONE, opHELP);
    argc -= (argc>0); argv += (argc>0);         // skip prog name argv[0] if present
    int pstat = cli.parse(argc, argv);
    if (pstat != ERR_OK)  return pstat;
    if (cli.opts[opHELP]) return ERR_OK;
    if (cli.args.size() != 1) quit("*** One Job ID required");
    job::id_t id = str2int(cli.args[0]);

    // Set the log level from the CLI as soon as possible, do it again from the config later.
    if (cli.opts[opLOG])       job::logger::set_level(cli.opts[opLOG].arg);
    else if (cli.opts[opVERB]) job::logger::set_level("verbose");

    // Set our filesystem root
    if (cli.opts[opROOT]) path.set_root(cli.opts[opROOT].arg);

    // Load the config file
    job::config cfg(path.cfgfile);
    if (cfg.error) quit("*** Cannot load config: %s", cfg.error);

    // 2nd time: set the log level
    if (!cli.opts[opLOG] && !cli.opts[opVERB] && cfg.exists("jobs", "log-level"))
        job::logger::set_level(cfg.get("jobs", "log-level", "info"));


    // Open the job
    job::file jf(id);
    if (jf.error) quit(jf.error);
    jf.load();
    if (jf.error) quit(jf.error);

    // Raw dump?
    if (cli.opts[opRAW]) {
        if (cli.opts[opVERB]) {
            say("Job-File:      %s", jf.name());
            say("Job-Owner-UID: %d", (int)jf.uid);
            say("Job-Owner-GID: %d", (int)jf.gid);
        }
        fputs(jf.to_string().c_str(), stdout);  // we use fputs() instead of puts() 
                                                //   so to NOT get the \n at the end.
        exit(ERR_OK);
    }

    // Show captured output
    int tri = 0;
    for (size_t s=1; s<jf.size(); s++) {    // Note start at section 1
        if (jf[s]["section"] != "output") continue;
        say("\nTry %d:", ++tri);
        say("%s\n", jf[s][job::file::BODY_TAG]);
    }

    exit(ERR_OK);
}
