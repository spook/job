# Autotools makefile for 'job' - the Linux Batch Facility

#   LGPL 2.1+
#
#   job - the Linux Batch Facility
#   (c) Copyright 2014-2016 Hewlett Packard Enterprise Development LP
#   Created by Uncle Spook <spook(at)MisfitMountain(dot)org>
#
#   This library is free software; you can redistribute it and/or
#   modify it under the terms of the GNU Lesser General Public
#   License as published by the Free Software Foundation; either
#   version 2.1 of the License, or any later version.
#
#   This library is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#   Lesser General Public License for more details.
#
#   You should have received a copy of the GNU Lesser General Public
#   License along with this library; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
#   USA

#==========================================================================
# Compiler & Linker flags
#==========================================================================

AM_CPPFLAGS         = -I$(srcdir)/src -DRELEASE=${PACKAGE_VERSION}
ACLOCAL_AMFLAGS     = -I m4
noinst_LIBRARIES    = 
lib_LTLIBRARIES     = libjob.la


#==========================================================================
# libjob shared library
#==========================================================================

libjob_la_LDFLAGS   = -version-info ${JOB_LIB_VERSION}

libjob_la_SOURCES   = src/job/config.cxx \
                      src/job/daemon.cxx \
                      src/job/file.cxx \
                      src/job/getopt.cxx \
                      src/job/isafe.cxx \
                      src/job/jobbase.cxx \
                      src/job/launch.cxx \
                      src/job/log.cxx \
                      src/job/multipart.cxx \
                      src/job/path.cxx \
                      src/job/queue.cxx \
                      src/job/seqnum.cxx \
                      src/job/status.cxx \
                      src/job/string.cxx


#==========================================================================
# Main programs
#==========================================================================

bin_PROGRAMS  = catjob \
                jobman \
                queman \
                mkjob 

LDADD = libjob.la

catjob_LDADD    = $(LDADD)
jobman_LDADD    = $(LDADD)
queman_LDADD    = $(LDADD)
mkjob_LDADD     = $(LDADD)

catjob_SOURCES  = src/catjob.cxx
jobman_SOURCES  = src/jobman.cxx
queman_SOURCES  = src/queman.cxx
mkjob_SOURCES   = src/mkjob.cxx

#==========================================================================
# man pages
#==========================================================================
# By the way, what the heck is this: "../`x=$@;echo $${x%.*}`" below?
#   It's my way of removing the .gz suffix, so we don't end up with mkjob.8.gz.gz
#   if we just use the target $@ going into gzip.  Recall that gzip always
#   adds .gz, and the target is already something.number.gz.  
#   Use of basename would be easier, except it also removes the directories.
#   So what you see here is some bash shell variable magic.
#   FYI, in make, $$ is the escaped literal '$' so it gets thru to the shell.
# IMPORTANT: it must be a TAB, not spaces, prefixing the following rule commands.

SUFFIXES = .pod .5.gz .7.gz .8.gz

.pod.5.gz:
	podchecker $<
	pod2man -c "job - the Linux Batch Facility" -d "2015-03-13" -n "`basename $< .pod`" -r "${PACKAGE_VERSION}" -s 5 $< > "../`x=$@;echo $${x%.*}`"
	gzip -f "../`x=$@;echo $${x%.*}`"

.pod.7.gz:
	podchecker $<
	pod2man -c "job - the Linux Batch Facility" -d "2015-03-13" -n "`basename $< .pod`" -r "${PACKAGE_VERSION}" -s 7 $< > "../`x=$@;echo $${x%.*}`"
	gzip -f "../`x=$@;echo $${x%.*}`"

.pod.8.gz:
	podchecker $<
	pod2man -c "job - the Linux Batch Facility" -d "2015-03-13" -n "`basename $< .pod`" -r "${PACKAGE_VERSION}" -s 8 $< > "../`x=$@;echo $${x%.*}`"
	gzip -f "../`x=$@;echo $${x%.*}`"

man_MANS = man/job.7.gz \
           man/job.conf.5.gz \
           man/jobman.8.gz \
           man/queman.8.gz \
           man/edjobq.8.gz \
           man/lsjobq.8.gz \
           man/mkjobq.8.gz \
           man/rmjobq.8.gz \
           man/catjob.8.gz \
           man/edjob.8.gz \
           man/lsjob.8.gz \
           man/mkjob.8.gz \
           man/rmjob.8.gz

