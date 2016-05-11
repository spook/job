#ifndef _JOB_STATUS_HXX_
#define _JOB_STATUS_HXX_ 1

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

// Adds a set value (int) and error string to a class,
//  plus simple typecasts to make error checking easy.

#include <string>
#include <errno.h>

// Convenience macros to make a status object from an errno, etc
#define IO_errno    errno
#define SYS_errno   errno
#define IO_status   job::status(IO_errno)
#define SYS_status  job::status(SYS_errno)

namespace job {
    // Error codes we use
    enum errs_t {ERR_USE_STR  = -1              //  -1  String-supplied error, not a standard value
                ,ERR_OK       = 0               //   0  All good

                ,ERR_AGAIN    = EAGAIN          //  11  Try Again
                ,ERR_ABORT    = ECANCELED       // 125  Operation canceled
                ,ERR_BADSTATE = EBADFD          //  77  File descriptor in bad state (just "bad state" for us)
                ,ERR_TIMEOUT  = ETIME           //  62  Timer expired
                ,ERR_MOVED    = 192             // 192  Job moved/taken by another
                ,ERR_LOCKED   = 193             // 193  Job locked by another
                ,ERR_INITING  = 194             // 194  Job initializing
                ,ERR_PENDING  = 195             // 195  Job pending
                ,ERR_BADCLI   = 199             // 199  Indicates a bad command line option
                };

  class status {
  public:
    status(const int e = ERR_OK, const std::string & prior = "");
    status(const int e,          const status & prior);
    status(const std::string & s, const std::string & prior = "");
    status(const std::string & s, const status & prior);
    virtual ~status();

    // Getters
    virtual operator int() const;
    virtual operator std::string() const;
    virtual const char* c_str() const;      // allows like printf("%s",job::status(errno).c_str());
    virtual bool is_ok() const;
    virtual bool is_error() const;

    // Setters
    //  Note: to clear the error, call set(ERR_OK) or assign with ERR_OK
    status operator = (const int e);
    status operator = (const std::string & s);
    status operator += (const int e);
    status operator += (const std::string & s);
    status operator += (const job::status & p);
    virtual int set(const int e, const std::string & prior = "");
    virtual int set(const int e, const status & prior);
    virtual int set(const std::string & s, const std::string & prior = "");
    virtual int set(const std::string & s, const status & prior);

  protected:
    int errval;
    std::string errstr;

  private:
    const std::string tostr() const;
    static char laststr[1024];
  };
}

#endif

