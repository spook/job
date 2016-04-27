#ifndef _JOB_LOG_HPP_
#define _JOB_LOG_HPP_ 1

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
#include <stdlib.h>
#include <string>

// Useful macros for generating logs with filename and line number details
//   say* / quit goes to stdout w/newline, without a debugging header, and no syslog.
//   log* / die  goes to stderr w/newline, has a debugging header, and goes to syslog.

#define LOGTYPE_SAY 0
#define LOGTYPE_LOG 1

#ifndef DOXYGEN_SKIP
  #define sayany(lvl, fmt, args...)   (job::logger::obj(lvl, __FILE__, __LINE__, fmt), ##args).out(LOGTYPE_SAY)
  #define sayifany(lvl, fmt, args...) ((job::logger::cur_level >= lvl) && sayany(lvl, fmt, ##args))
  #define logany(lvl, fmt, args...)   (job::logger::obj(lvl, __FILE__, __LINE__, fmt), ##args).out(LOGTYPE_LOG)
  #define logifany(lvl, fmt, args...) ((job::logger::cur_level >= lvl) && logany(lvl, fmt, ##args))
#endif

#define say(fmt, args...)             sayall(fmt, ##args)
#define sayall(fmt, args...)          job::logger::dummy = sayifany(job::LOGALWAYS,  fmt, ##args)
#define sayfatal(fmt, args...)        job::logger::dummy = sayifany(job::LOGFATAL,   fmt, ##args)
#define sayerror(fmt, args...)        job::logger::dummy = sayifany(job::LOGERROR,   fmt, ##args)
#define saywarn(fmt, args...)         job::logger::dummy = sayifany(job::LOGWARN,    fmt, ##args)
#define sayinfo(fmt, args...)         job::logger::dummy = sayifany(job::LOGINFO,    fmt, ##args)
#define sayverbose(fmt, args...)      job::logger::dummy = sayifany(job::LOGVERBOSE, fmt, ##args)
#define saydebug(fmt, args...)        job::logger::dummy = sayifany(job::LOGDEBUG,   fmt, ##args)
#define saydump(fmt, args...)         job::logger::dummy = sayifany(job::LOGDUMP, fmt, ##args)
#define sayassert(cond, fmt, args...) if (cond) job::logger::dummy = 1; else (job::logger::obj(job::LOGFATAL, __FILE__, __LINE__, fmt), ##args).out_n_die(EXIT_FAILURE, LOGTYPE_SAY)
#define quit(fmt, args...)          (job::logger::obj(job::LOGFATAL, __FILE__, __LINE__, fmt), ##args).out_n_die(EXIT_FAILURE, LOGTYPE_SAY)

#define logall(fmt, args...)          job::logger::dummy = logifany(job::LOGALWAYS,  fmt, ##args)
#define logfatal(fmt, args...)        job::logger::dummy = logifany(job::LOGFATAL,   fmt, ##args)
#define logerror(fmt, args...)        job::logger::dummy = logifany(job::LOGERROR,   fmt, ##args)
#define logwarn(fmt, args...)         job::logger::dummy = logifany(job::LOGWARN,    fmt, ##args)
#define loginfo(fmt, args...)         job::logger::dummy = logifany(job::LOGINFO,    fmt, ##args)
#define logverbose(fmt, args...)      job::logger::dummy = logifany(job::LOGVERBOSE, fmt, ##args)
#define logdebug(fmt, args...)        job::logger::dummy = logifany(job::LOGDEBUG,   fmt, ##args)
#define logdump(fmt, args...)         job::logger::dummy = logifany(job::LOGDUMP,    fmt, ##args)
#define logassert(cond, fmt, args...) if (cond) job::logger::dummy = 1; else (job::logger::obj(job::LOGFATAL, __FILE__, __LINE__, fmt), ##args).out_n_die(EXIT_FAILURE, LOGTYPE_LOG)
#define die(fmt, args...)             (job::logger::obj(job::LOGFATAL, __FILE__, __LINE__, fmt), ##args).out_n_die(EXIT_FAILURE, LOGTYPE_LOG)
#define logstr(fmt, args...)          (job::logger::obj(job::LOGDEBUG, __FILE__, __LINE__, fmt), ##args).str()

#define logmsg job::logger::last

#define LOGGER_LEVEL_VERBOSE (job::logger::cur_level >= job::LOGVERBOSE)
#define LOGGER_LEVEL_DEBUG   (job::logger::cur_level >= job::LOGDEBUG)
#define LOGGER_LEVEL_DUMP    (job::logger::cur_level >= job::LOGDUMP)

namespace job {
    enum loglevel_t {
            LOGSILENT = -2,
            LOGALWAYS = -1, 
            LOGFATAL = 0,
            LOGERROR,
            LOGWARN,
            LOGINFO,
            LOGVERBOSE,
            LOGDEBUG,
            LOGDUMP
    };


    class logger {
    public:
        static void         set_level(std::string level = "info");
        static loglevel_t   fromstring(std::string level);
        static std::string  last;

#ifndef DOXYGEN_SKIP
        static logger &     obj(loglevel_t level, const char* file, int line, const char* format);
        static logger &     obj(loglevel_t level, const char* file, int line, const std::string & format);

        static loglevel_t   cur_level;
        static const char*  cur_name;
        static bool         force_log;
        static bool         show_header;
        static bool         show_newline;
        static bool         show_pid;
        static int          dummy;

        logger();
        ~logger();
        std::string         str();
        int                 out(const int logtype);
        void                out_n_die(int xstat, const int logtype);
        logger & operator,(const job::status & x);
        logger & operator,(const char* x);
        logger & operator,(const std::string & x);
        logger & operator,(long long x);
        logger & operator,(long long unsigned x);
        logger & operator,(size_t x);
        logger & operator,(long x);
        logger & operator,(int x);
        logger & operator,(bool x);
        logger & operator,(char x);
        logger & operator,(double x);
        logger & operator,(void* x);
#endif
    private:
        bool        get_chunk(char & f, std::string & ff);
        std::string head();
        logger &    set(loglevel_t level, const char* file, int line, const char* format);

        size_t      i;
        loglevel_t  lvl;
        std::string fil;
        int         lin;
        std::string fmt;
    };
}

/*! @file
 *   @brief Jobs Logging utility.
 *
 *   Generated log messages are of the form:
 *
 *   @code
 *     LVL [FILE:LINE] Message
 *   @endcode
 *
 *   @param LVL is the three character level for the message FTL,ERR,WRN,INF,VBS,DBG,VDB
 *   @param FILE is the name of the source file where the log message is generated 
 *   @param LINE is the line number within the source file where the message is generated
 *   @param Message is the formatted contents of the log message
 *
 * @def logfatal(fmt, args...)
 *   @brief Log a fatal level message.
 *
 *   @param fmt the log message format string, like a format string for printf
 *   @param ... any other arguments for use as defined by the format string
 *
 * @def logerror(fmt, args...)
 *   @brief Log an error level message.
 *
 *   @param fmt the log message format string, like a format string for printf
 *   @param ... any other arguments for use as defined by the format string
 *
 * @def logwarn(fmt, args...)
 *   @brief Log a warning level message.
 *
 *   @param fmt the log message format string, like a format string for printf
 *   @param ... any other arguments for use as defined by the format string
 *
 * @def loginfo(fmt, args...)
 *   @brief Log an informational level message.
 *
 *   @param fmt the log message format string, like a format string for printf
 *   @param ... any other arguments for use as defined by the format string
 *
 * @def logverbose(fmt, args...)
 *   @brief Log a verbose level message.
 *
 *   @param fmt the log message format string, like a format string for printf
 *   @param ... any other arguments for use as defined by the format string
 *
 * @def logdebug(fmt, args...)
 *   @brief Log a debug level message.
 *
 *   @param fmt the log message format string, like a format string for printf
 *   @param ... any other arguments for use as defined by the format string
 *
 * @def logdump(fmt, args...)
 *   @brief Log a more-verbose debug level message; typically debugging that includes 
 *   dumps of data structures or message contents.
 *
 *   @param fmt the log message format string, like a format string for printf
 *   @param ... any other arguments for use as defined by the format string
 *
 * @def logassert(cond, fmt, args...)
 *   @brief Log a fatal level message and exit if the condition is true.
 *
 *   @param cond a true/false condition for the assert
 *   @param fmt  the log message format string, like a format string for printf
 *   @param ...  any other arguments for use as defined by the format string
 *
 *   @note The condition must be able to evaluate to a boolean true or false.
 *
 * @def sayfatal(fmt, args...)
 *   @brief Print to stdout a fatal level message.
 *
 *   @param fmt the log message format string, like a format string for printf
 *   @param ... any other arguments for use as defined by the format string
 *
 * @def sayerror(fmt, args...)
 *   @brief Print to stdout an error level message.
 *
 *   @param fmt the log message format string, like a format string for printf
 *   @param ... any other arguments for use as defined by the format string
 *
 * @def saywarn(fmt, args...)
 *   @brief Print to stdout a warning level message.
 *
 *   @param fmt the log message format string, like a format string for printf
 *   @param ... any other arguments for use as defined by the format string
 *
 * @def sayinfo(fmt, args...)
 *   @brief Print to stdout an informational level message.
 *
 *   @param fmt the log message format string, like a format string for printf
 *   @param ... any other arguments for use as defined by the format string
 *
 * @def sayverbose(fmt, args...)
 *   @brief Print to stdout a verbose level message.
 *
 *   @param fmt the log message format string, like a format string for printf
 *   @param ... any other arguments for use as defined by the format string
 *
 * @def saydebug(fmt, args...)
 *   @brief Print to stdout a debug level message.
 *
 *   @param fmt the log message format string, like a format string for printf
 *   @param ... any other arguments for use as defined by the format string
 *
 * @def saydump(fmt, args...)
 *   @brief Print to stdout a more-verbose debug level message; 
 *   typically debugging that includes 
 *   dumps of data structures or message contents.
 *
 *   @param fmt the log message format string, like a format string for printf
 *   @param ... any other arguments for use as defined by the format string
 *
 * @def sayassert(cond, fmt, args...)
 *   @brief Print to stdout a fatal level message and exit if the condition is true.
 *
 *   @param cond a true/false condition for the assert
 *   @param fmt  the log message format string, like a format string for printf
 *   @param ...  any other arguments for use as defined by the format string
 *
 *   @note The condition must be able to evaluate to a boolean true or false.
 *
 * @def logstr(fmt, args...)
 *   @brief Return the filled in format string.
 *
 *   @param fmt the log message format string, like a format string for printf
 *   @param ... any other arguments for use as defined by the format string
 *
 *   @note The returned string does not have a time stamp or file/line section prepended.
 *
 * @def die(fmt, ec, args...)
 *   @brief Log a fatal level message with the error string appended and exit.
 *
 *   @param fmt the log message format string, like a format string for printf
 *   @param ... any other arguments for use as defined by the format string
 *
 *   @note An error code is given as the first argument for the die, it must be the first argument
 *         after the format string.
 *
 *   @note If the format string has any format placeholders (those % things) and an error code
 *         is not supplied, a zero argument must be used between the format string and the
 *         values for the format controls.
 *
 *   @note A string representation of the error code is appended to the generated log message.
 *         There should be no format control in the format string to account for this.
 *
 * @def quit(fmt, ec, args...)
 *   @brief Print to stdout the error string, and exit.
 *
 *   @param fmt the log message format string, like a format string for printf
 *   @param ... any other arguments for use as defined by the format string
 *
 *   @note An error code is given as the first argument for the die, it must be the first argument
 *         after the format string.
 *
 *   @note If the format string has any format placeholders (those % things) and an error code
 *         is not supplied, a zero argument must be used between the format string and the
 *         values for the format controls.
 *
 *   @note A string representation of the error code is appended to the generated message.
 *         There should be no format control in the format string to account for this.
 *
 * @fn void job::logger(loglevel_t lvl, const char *file, unsigned int line, const char *fmt)
 *   @brief The low level log function called by the level specific macros.
 *
 *   @param lvl the level of this log message
 *   @param file the file this log message is generated from
 *   @param line the line number within the file where this log message is generated
 *   @param fmt the log message format string, like a format string for printf
 *
 * @fn void job::logger::set_level(const std::string & level = "Info")
 *   @brief Sets the level of generated log messages.
 *
 *   When a level is set, all messages at that level and below are generated.
 *
 *   The valid levels names are: Fatal, Error, Warn, Info, Verbose, Debug, VerboseDebug.
 *
 *   Attempting to set the level to an unkown name results in no change to the current level.
 *
 *   @param level the level of log messages to output
 *
 * @var job::logger::last
 *   @brief Contains the message portion of the last generated log message.
 *
 *   @code
 *     logerror("Some error message: %d", return_code);
 *     std::string msg = job::logger::last;
 *   @endcode
 *
 *   @note If the log level is set such that the log message is not generated,
 *     it will NOT set this last generated message string.
 *
 * @enum job::loglevel_t
 *   @brief The supported levels of log information.
 */

#endif
