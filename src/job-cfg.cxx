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

// TODO:  Add a syntax to delete a key or entire sections.
//          Perhaps !sec:key   or    !sec:
//          Doing   !key   would delete that key from every section where its found.

// TODO:  The --pretty format needs work.  It's mostly a reminder...

#include "config.hpp"
#include "getopt.hpp"
#include "log.hpp"
#include "path.hpp"
#include "string.hpp"
#include <fnmatch.h>

using std::string;

// CLI options and usage help
enum {opNONE, opCFG, opHELP, opNICE, opROOT, opVERB, opVER};
const option::Descriptor usage[] = {
    {opNONE, 0, "",   "",           Arg::None, "Usage: rda-cfg [options] sec:key[=val]...\n\nOptions:"},
    {opCFG,  0, "c", "config-file", Arg::Reqd, "  -c  --config-file  Use this configuration file"},
    {opHELP, 0, "h", "help",        Arg::None, "  -h  --help         Print usage and exit"},
    {opNICE, 0, "p", "pretty",      Arg::None, "  -p  --pretty       Pretty output"},
    {opROOT, 0, "R", "root",        Arg::Reqd, "  -R  --root         Set file system root"},
    {opVERB, 0, "v", "verbose",     Arg::None, "  -v  --verbose      Show more info"},
    {opVER,  0, "V", "version",     Arg::None, "  -V  --version      Print version and exit"},
    {opNONE, 0, "",   "",           Arg::None, "\nGet or set RDA configuration values."},
    {0, 0, 0, 0, 0, 0}
};

void _seckey(const std::string& token,
             std::string & sec,
             std::string & key) {
    rda::stringlist parts = rda::split(token, ":");
    if (parts.size() == 0) {
        sec = "";
        key = "";
    }
    else if (parts.size() == 1) {
        sec = "";
        key = parts[0];
    }
    else {
        sec = rda::join(parts, ':', parts.size()-1);        // a:b:c:d => a:b:c
        key = parts[parts.size()-1];                        // a:b:c:d => d
    }
    if (sec == "") sec = "*";
    if (key == "") key = "*";
    return;
}

bool _match_wild(const string & pat, const string & str) {
    return 0 == fnmatch(pat.c_str(), str.c_str(), FNM_CASEFOLD);
}

// Main entry point
int main(int argc, const char* argv[]) {

    // Parse CLI options
    rda::getopt cli(usage, opNONE, opHELP);
    argc -= (argc > 0);
    argv += (argc > 0); // skip prog name argv[0] if present
    int pstat = cli.parse(argc, argv);
    if (pstat != rda::ERR_OK) return pstat;
    if (cli.opts[opHELP]) return rda::ERR_OK;
    if (cli.opts[opVER]) {
        say("RDA Version %s", rda::version);
        return rda::ERR_OK;
    }
    if (cli.opts[opVERB]) rda::logger::set_level("verbose");

    // Set our filesystem root
    std::string root;
    if (cli.opts[opROOT]) root = cli.opts[opROOT].arg;
    sayverbose("Filesystem Root: %s", root);
    rda::rda_path::create(root);

    // Load the config file
    rda::config cfg;
    std::string file = cli.opts[opCFG]? cli.opts[opCFG].arg : rda::path->cfgfile;
    sayverbose("    Config file: %s", file);
    if (cfg.read(file)) die("%s", cfg.error);

    string fmt = cli.opts[opNICE]? "  %10s:%-13s = %s" : "%s:%s=%s";

    // Go thru each arg, getting or setting as indicated
    //  Formats are:
    //      section:key=value   - set this value
    //      section:key         - just display this vallue
    //      key                 - find that key in any section
    //      section:            - display all in this section
    //  The section can have colons in it, like job:push.
    //  Example:
    //      job:push:command is section 'job:push', key 'command'.
    int nerr = 0;
    int nset = 0;
    for (size_t i=0; i<cli.rest.size(); i++) {

        string item = cli.rest[i];

        // Set or get?
        if (item.find('=') != item.npos) {

            // Set - Split tag from value
            rda::stringlist parts = rda::split(item, "=", 2);
            std::string & tag = parts[0];
            std::string & val = parts[1];
            if (tag.find_first_of("*?") != tag.npos) {
                ++nerr;
                sayerror("*** Tag cannot contain * or ? characters: %s", tag);
                continue;
            }

            // Split tag into section & key
            std::string sec;
            std::string key;
            _seckey(tag, sec, key);
            if ((sec == "*") || (key == "*")) {
                ++nerr;
                sayerror("*** Tag must have both section and key parts, given as section:key (%s)", tag);
                continue;
            }

            // Set it
            cfg.set(sec, key, val);
            if (cli.opts[opVERB]) say(fmt, sec, key, cfg.get(sec, key));
            ++nset;
            continue;
        }

        // Get
        std::string sec;
        std::string key;
        _seckey(item, sec, key);
        sayverbose("\nSeeking %s:%s", sec, key);
        rda::stringlist secs = cfg.sections();
        for (size_t s=0; s<secs.size(); s++) {
            if (!_match_wild(sec, secs[s])) continue;
            rda::stringlist keys = cfg.keys(secs[s]);
            for (size_t t=0; t<keys.size(); t++) {
                if (!_match_wild(key, keys[t])) continue;
                say(fmt, secs[s], keys[t], cfg.get(secs[s], keys[t]));
            }
        }
    }

    // If any set, write the config back
    if (nset && cfg.write()) {
        sayerror(cfg.error);
        ++nerr;
    }

    return nerr > 255? 255 : nerr;
}
