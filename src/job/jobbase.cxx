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

// Coder's Notes:

#include "job/base.hxx"
#include "job/jobbase.hxx"
#include "job/log.hxx"
#include "job/queue.hxx"
#define __STDC_FORMAT_MACROS    // Enable PRI macros

using job::ERR_OK;
using job::int2str;
using std::string;

int job::jobbase::zone = 0;
const char* job::jobbase::BODY_TAG = "__BODY__";

std::string job::state2str(const state_t s) {
    switch (s) {
        case hold: return "hold";
        case pend: return "pend";
        case run:  return "run";
        case tied: return "tied";
        case kill: return "kill";
        case done: return "done";
        case unk:  break;
    }
    return "???";
}

job::state_t job::str2state(const std::string & s) {
    if (s == "hold") return hold;
    if (s == "pend") return pend;
    if (s == "run")  return run;
    if (s == "tied") return tied;
    if (s == "kill") return kill;
    if (s == "done") return done;
    return unk;
}

// Create a new empty job
job::jobbase::jobbase() 
    : queue("batch")
    , sub_time(time(NULL))
    , run_time(JOB_ASAP)
    , priority(5)
    , id(0)
    , mid(0)
    , try_count(0)
    , try_limit(100)
    , state(hold)
    , pid(0)
    , uid(-1)
    , gid(-1)
    , notify(false)
{
    // Ensure we have a section 0
    if (!size()) resize(1);

    // Create a new job ID (job number [+ zone])
//    seqnum q(path.seqfile);
//    id = q.next();
//    if (q.error) {
//        error.set("Sequence number", q.error);
//        return;
//    }
    if ((zone < 0) ||  (zone > 9)) {
        error = "Bad zone value; must be 0 or 1-9";
        return;
    }
    if (zone) id = id*10 + zone;
    error = ERR_OK;
}

// Find and load existing job file, given its ID
job::jobbase::jobbase(const job::id_t & wanted_id) 
    : queue("batch")
    , sub_time(0)
    , run_time(0)
    , priority(0)
    , id(wanted_id)
    , mid(0)
    , try_count(0)
    , try_limit(100)
    , state(hold)
    , pid(0)
    , uid(-1)
    , gid(-1)
    , notify(false)
{
    if (!size()) resize(1); // Ensure we have a section 0

    //TODO
}


job::jobbase::~jobbase() {
}

// Does a tag exist in a section?
bool job::jobbase::exists(const unsigned int sec, 
                          const std::string & tag) {
    return (sec < size()) && ((*this)[sec].count(tag) > 0);
}

// Get a string value, or if not present, use a default
std::string job::jobbase::get(const unsigned int sec, 
                              const std::string & tag, 
                              const std::string & dfl) {
    return exists(sec, tag)
        ? (*this)[sec][lc(tag)]
        : dfl;
        ;
}

job::status job::jobbase::remove() {
    //TODO
    return error = ERR_OK;
}

void job::jobbase::tie_to(std::string node) {
    ties.clear();
    ties[node] = 0;
}

void job::jobbase::tie_to(job::stringlist nodes) {
    ties.clear();
    for (stringlist::iterator it = nodes.begin(); it != nodes.end(); ++it) ties[*it] = 0;
}

job::idlist_t job::jobbase::tied_ids() {
    idlist_t v;
    for (ties_t::iterator it = ties.begin(); it != ties.end(); ++it) v.push_back(it->second);
    return v;
}

job::status job::jobbase::write() {

    // Update section 0 header items

    // -(these come from file pathname attributes: note lowercase)-
    (*this)[0]["job-id"]     = int2str(id);
    (*this)[0]["job-state"]  = state2str(state);                     // varies over life
    (*this)[0]["job-queue"]  = queue;
    (*this)[0]["job-prio"]   = int2str(priority);                    // may vary
    // -(these are from section zero headers)-
    (*this)[0]["Command"]    = command;
    (*this)[0]["Job-MID"]    = int2str(mid);
    (*this)[0]["Job-MNode"]  = mnode;
    (*this)[0]["Job-PID"]    = (state == run)? int2str(pid) : "0";   // varies
    (*this)[0]["Job-Type"]   = type;
    (*this)[0]["TTY-Notify"] = yn2str(notify);
    (*this)[0]["Try-Limit"]  = int2str(try_limit);
    for (size_t i=0; i<args.size(); i++) {
        (*this)[0]["Job-Arg-" + int2str(i+1)] = args[i];
    }
    (*this)[0][BODY_TAG].clear();
    for (ties_t::iterator it = ties.begin(); it != ties.end(); ++it) {
        (*this)[0][BODY_TAG] += "tie " + it->first + " " + int2str(it->second) + "\n";
    }

    // Store it - the whole thing
//TODO

    // Done
    return error = ERR_OK;
}

