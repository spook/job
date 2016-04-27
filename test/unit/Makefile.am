# Makefile for unit tests

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

AM_CPPFLAGS = -I$(srcdir)/../../src/ -I$(srcdir)/../src/
LDADD       = ../../libjob.la

bin_PROGRAMS = \
    job-config-010.tx \
    job-file-010.tx \
    job-multipart-010.tx \
    job-seqnum-010.tx 

TEST_CODE   = ../src/tap-extra.cxx ../src/tap++/tap++.cxx

job_file_010_tx_SOURCES         = job-file-010.cxx $(TEST_CODE)
job_multipart_010_tx_SOURCES    = job-multipart-010.cxx $(TEST_CODE)
job_seqnum_010_tx_SOURCES       = job-seqnum-010.cxx $(TEST_CODE)
job_config_010_tx_SOURCES       = job-config-010.cxx $(TEST_CODE)
