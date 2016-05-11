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

# Spec file to make RPM kits for the 'job' subsystem

%define __os_install_post %{nil}
%define _builddir    ./
%define _rpmdir      ./dist
%define _rpmfilename %%{NAME}-%%{VERSION}-%%{RELEASE}-{BuildPlatform}.%%{ARCH}.rpm
%define _topdir      /tmp/rpm
%define _tmppath     %{_topdir}/tmp

%define name      job
%define version   {BuildVersion}
%define release   {BuildRelease}
%define packager  The Linux Batch Job subsystem.
%define vendor    Uncle Spook Software
%define license   LGPL 2.1+
%define group     System Environment/Daemons
%define summary   The Linux Batch Job subsystem
%define url       https://github.com/spook/job
%define buildroot %{_topdir}/%{name}-root

Name:      %{name}
Version:   %{version}
Release:   %{release}
Packager:  %{packager}
Vendor:    %{vendor}
License:   %{license}
Summary:   %{summary}
Group:     %{group}
Source:    %{source}
URL:       %{url}
Prefix:    /
Buildroot: %{buildroot}
Requires:  perl >= 5.0
AutoReqProv: yes
Distribution: RedHat Linux

%description
The Linux Batch Job subsystem.


# -- Prep: pre-build --
%prep
exit 0

# -- Build (runs between prep and install) --
%build
exit 0

# Install the software on the build system --
%install
install  -m744 -d $RPM_BUILD_ROOT/etc/init.d
install  -m744 -d $RPM_BUILD_ROOT/etc/job
install  -m755 -d $RPM_BUILD_ROOT/etc/job/qdefs
install  -m755 -d $RPM_BUILD_ROOT/usr/bin
install  -m755 -d $RPM_BUILD_ROOT/usr/share/doc/job
install  -m755 -d $RPM_BUILD_ROOT/usr/share/man
install  -m755 -d $RPM_BUILD_ROOT/usr/share/man/man5
install  -m755 -d $RPM_BUILD_ROOT/usr/share/man/man7
install  -m755 -d $RPM_BUILD_ROOT/usr/share/man/man8
install  -m755 -d $RPM_BUILD_ROOT%{_libdir}
install  -m755 -d $RPM_BUILD_ROOT/var/lib/job
install  -m755 -d $RPM_BUILD_ROOT/var/log/job
install  -m755 -d $RPM_BUILD_ROOT/var/spool/job

install  -m755 kit/etc/init.d/job    $RPM_BUILD_ROOT/etc/init.d
install  -m755 kit/etc/job/job.conf  $RPM_BUILD_ROOT/etc/job
install  -m755 kit/usr/bin/queman    $RPM_BUILD_ROOT/usr/bin
install  -m755 kit/usr/bin/jobman    $RPM_BUILD_ROOT/usr/bin
install  -m755 kit/usr/bin/catjob    $RPM_BUILD_ROOT/usr/bin
install  -m755 kit/usr/bin/edjob     $RPM_BUILD_ROOT/usr/bin
install  -m755 kit/usr/bin/edjobq    $RPM_BUILD_ROOT/usr/bin
install  -m755 kit/usr/bin/lsjob     $RPM_BUILD_ROOT/usr/bin
install  -m755 kit/usr/bin/lsjobq    $RPM_BUILD_ROOT/usr/bin
install -m4755 kit/usr/bin/mkjob     $RPM_BUILD_ROOT/usr/bin
install  -m755 kit/usr/bin/mkjobq    $RPM_BUILD_ROOT/usr/bin
install  -m755 kit/usr/bin/mvjob     $RPM_BUILD_ROOT/usr/bin
install  -m755 kit/usr/bin/mvjobq    $RPM_BUILD_ROOT/usr/bin
install  -m755 kit/usr/bin/rmjob     $RPM_BUILD_ROOT/usr/bin
install  -m755 kit/usr/bin/rmjobq    $RPM_BUILD_ROOT/usr/bin
install  -m644 package/LICENSE       $RPM_BUILD_ROOT/usr/share/doc/job/
install  -m755 man/job.conf.5.gz     $RPM_BUILD_ROOT/usr/share/man/man5
install  -m755 man/job.7.gz          $RPM_BUILD_ROOT/usr/share/man/man7
install  -m755 man/jobman.8.gz       $RPM_BUILD_ROOT/usr/share/man/man8
install  -m755 man/catjob.8.gz       $RPM_BUILD_ROOT/usr/share/man/man8
install  -m755 man/edjob.8.gz        $RPM_BUILD_ROOT/usr/share/man/man8
install  -m755 man/edjobq.8.gz       $RPM_BUILD_ROOT/usr/share/man/man8
install  -m755 man/lsjob.8.gz        $RPM_BUILD_ROOT/usr/share/man/man8
install  -m755 man/lsjobq.8.gz       $RPM_BUILD_ROOT/usr/share/man/man8
install  -m755 man/mkjob.8.gz        $RPM_BUILD_ROOT/usr/share/man/man8
install  -m755 man/mkjobq.8.gz       $RPM_BUILD_ROOT/usr/share/man/man8
install  -m755 man/rmjob.8.gz        $RPM_BUILD_ROOT/usr/share/man/man8
install  -m755 man/rmjobq.8.gz       $RPM_BUILD_ROOT/usr/share/man/man8
install  -m755 kit/usr/lib/libjob.so.1.0.0   $RPM_BUILD_ROOT%{_libdir}
ln -sf                     libjob.so.1.0.0   $RPM_BUILD_ROOT%{_libdir}/libjob.so
ln -sf                     libjob.so.1.0.0   $RPM_BUILD_ROOT%{_libdir}/libjob.so.1
install  -m755 test/load/job-pump            $RPM_BUILD_ROOT/usr/bin

exit 0

# -- Define files in the package --
%files
%defattr(-,root,root,-)
/etc/init.d/job
/usr/bin/*
/usr/share/man/man5/*
/usr/share/man/man7/*
/usr/share/man/man8/*
%{_libdir}/*
/var/lib/job
/var/spool/job
%config(noreplace) /etc/job/job.conf
%doc package/LICENSE
%dir /etc/job/qdefs
%dir /var/log/job

# -- Cleanup after a package build --
%clean
exit 0

# -- Pre-install --
%pre
[ -x /etc/init.d/job ] && service job stop_wait
exit 0

# -- Post-install --
%post
RPM_INSTALL_PREFIX=`echo "$RPM_INSTALL_PREFIX" | sed 's#/*$##'`
BINDIR=$RPM_INSTALL_PREFIX/usr/bin
LIBDIR=$RPM_INSTALL_PREFIX%{_libdir}
CFGDIR=$RPM_INSTALL_PREFIX/etc/job
QUEDIR=$RPM_INSTALL_PREFIX/var/spool/job
ldconfig
mkjobq -f batch
service job start
exit 0

# -- Pre-uninstall --
%preun
RPM_INSTALL_PREFIX=`echo "$RPM_INSTALL_PREFIX" | sed 's#/*$##'`
BINDIR=$RPM_INSTALL_PREFIX/usr/bin
LIBDIR=$RPM_INSTALL_PREFIX%{_libdir}
CFGDIR=$RPM_INSTALL_PREFIX/etc/job
QUEDIR=$RPM_INSTALL_PREFIX/var/spool/job
if [ "$1" = 0 ]; then # this is the last uninstall
    [ -x /etc/init.d/job ] && service job stop_wait
    chkconfig job off
    chkconfig job --del
    killall jobman
fi
exit 0

# -- Post-uninstall --
%postun
if [ "$1" = 0 ]; then # this is the last uninstall
    RPM_INSTALL_PREFIX=`echo "$RPM_INSTALL_PREFIX" | sed 's#/*$##'`
    rm -rf $RPM_INSTALL_PREFIX/etc/job
    rm -rf $RPM_INSTALL_PREFIX/var/lib/job
    rm -rf $RPM_INSTALL_PREFIX/var/spool/job
    ldconfig
fi
exit 0

