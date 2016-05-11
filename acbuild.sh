#!/bin/bash
# Build the 'job' subsystem using autoconf

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

set -e
prog=$0
PATH=/sbin:/usr/sbin:/bin:/usr/bin

Usage() {
    cat <<EOF >&2
Usage: $prog [options] <type> [args ...]
  where options are:
  -B, --build-dir DIR     Install executables into specified directory (default
                          is $PWD/bin)
  -c, --compile           Compile the code
  -D, --dest-dir DIR      Install executables into specified directory (default
                          is $PWD/kit)
  -d, --debug             Compile with debug symbols
  -g, --coverage          Generate a test coverage report  
  -h, --help              Show this help message and exit
  -k, --clean             Perform "make clean uninstall"
  -m, --multi             Turn off multiprocessing (make -j flag)
  -n, --revision          Use given revision number, no get from SVN
  -p, --purge             Purge the code (and binaries)
  -R, --regen             Regenerate all autotools files
  -r, --reconfigure       Rerun configure
  -s, --ship              Make the appropriate kits to ship as well
  -S, --storage-update    Make the storage update kit
  -t, --test              Execute the perl tests
  -u, --unittest          Execute the unit tests
  -v, --verbose           Verbose build
  -y, --tiny              Optimize for small size; do not use with -d nor -g
  -z, --colorize          Colorize the output
  -#, --rehash            Rehash the certs

EOF
    exit $1
}

# Version string compare.  Ex:  Is 1.15 later than 1.7.8? (yes)
#   Sets $CMP to "=" for equal, ">" for greater, "<" for less
vercomp() {
    if [[ $1 == $2 ]]
    then
        CMP='='
        return
    fi
    local IFS=.
    local i ver1=($1) ver2=($2)
    # fill empty fields in ver1 with zeros
    for ((i=${#ver1[@]}; i<${#ver2[@]}; i++))
    do
        ver1[i]=0
    done
    for ((i=0; i<${#ver1[@]}; i++))
    do
        if [[ -z ${ver2[i]} ]]
        then
            # fill empty fields in ver2 with zeros
            ver2[i]=0
        fi
        if ((10#${ver1[i]} > 10#${ver2[i]}))
        then
            CMP='<'
            return
        fi
        if ((10#${ver1[i]} < 10#${ver2[i]}))
        then
            CMP='>'
            return
        fi
    done
    CMP='='
    return
}

#
# Various initializations
#
false() { return 1; }
true()  { return 0; }
BUILDDIR=${BUILDDIR:-$PWD/bin}
DESTDIR=${DESTDIR:-$PWD/kit}
DISTDIR=${DISTDIR:-$PWD/dist}
LIBDIR=${LIBDIR:-$PWD/kit/usr/lib}
BINDIR=${BINDIR:-$PWD/kit/usr/bin}
COVERAGEDIR=${COVERAGEDIR:-$PWD/coverage}
TESTDIR=${TESTDIR:-$PWD/test/unit}
BE_SMALL=false;
DO_AUTORECONF=false
DO_CLEAN=false
DO_CONFIGURE=false
DO_COVERAGE=false
DO_MAKE=false
DO_PURGE=false
DO_SHIP=false
DO_UNIT_TEST=false
DO_INTG_TEST=false
DO_COLOR=false
WANT_REVISION=
OPTFLAGS="-O3"
LDFLAGS=

#
# Figure out our platform
#
OS=$(lsb_release -is)
BIT=$(getconf LONG_BIT)
case "$OS" in
    RedHatEnterpriseServer | RedHatEnterpriseWorkstation )
        osver=$(lsb_release -rs)
        if [ $osver == "6.5" ]
        then
            buildPlatform=rh65
            OS=el${osver%%.*}-5
        else
            buildPlatform=rh
            OS=el${osver%%.*}
        fi
        ;;
    Ubuntu)
        buildPlatform=ub
        ;;
    Debian)
        buildPlatform=debian
        ;;
    CentOS)
        buildPlatform=ce
        ;;
    *)
        echo "Unsupported OS $OS for building kits" >&2
        ;;
esac

#
# Determine number of processes to use for make.  We'll use at least 2 but no
# more than what are on the system.  The nproc command can tell us this but
# RHEL5 does not have that.  Thus, we count them in /proc/cpuinfo .
#
NJOBS=$(grep processor /proc/cpuinfo | wc -l)
[ $NJOBS -lt 2 ] && NJOBS=2

#
# Parse the command line
#
TEMP=$(getopt -o kcD:dghmn:pRrsStuvyz# \
    --long clean,compile,dest-dir:,debug,help,coverage,multi,revision:,purge,reconfigure,regen,ship,storage-update,test,unittest,verbose,tiny,colorize,rehash -- "$@")
eval set -- "$TEMP"

while true; do
    case "$1" in
        -c|--compile)         DO_MAKE=true;;
        -D|--dest-dir)        shift; DESTDIR=$1;;
        -d|--debug)           OPTFLAGS="-ggdb -O0";;
        -h|--help)            Usage 0;;
        -g|--coverage)        OPTFLAGS="-ggdb -O0 -fprofile-arcs -ftest-coverage --coverage"; LDFLAGS="-lgcov"; DO_COVERAGE=true;; 
        -k|--clean)           DO_CLEAN=true;;
        -m|--multi)           NJOBS=1;;
        -n|--revision)        shift; WANT_REVISION=$1;;
        -p|--purge)           DO_PURGE=true;;
        -R|--regen)           DO_AUTORECONF=true; DO_CONFIGURE=true;;
        -r|--reconfigure)     DO_CONFIGURE=true;;
        -s|--ship)            DO_SHIP=true;;
        -t|--test)            DO_INTG_TEST=true;; 
        -u|--unittest)        DO_UNIT_TEST=true;;
        -v|--verbose)         VERBOSE=true;;
        -y|--tiny)            BE_SMALL=true; OPTFLAGS="-Os";;
        -z|--colorize)        DO_COLOR=true;;
        --)                   shift; break;;
        *)                    Usage 1;;
    esac
    shift
done

# Colorize by piping ourselves thru a colorizer script
if $DO_COLOR; then
    OPTIONS=`echo $TEMP | sed "s/--colorize //g" | sed "s/-z //g" | sed "s/--$//g"`
    COLORCMND=`echo $0 | sed "s/acbuild.sh/\/accolor/g"`
    $0 $OPTIONS 2>&1 | $COLORCMND
    exit 0
fi

#
# Create file named VERSION to hold our version number.  Only update the VERSION
# file if it doesn't exist or the version has changed.  Recreating VERSION will
# cause configure to be run the next time make is run.
#
if [ -z $WANT_REVISION ]; then
    rev=$(svnversion -c)
    if [ "$rev" == "exported" ]; then
        rev="EXP"
    elif [ "$rev" == "Unversioned directory" ]; then
        rev=$(git svn info | sed -n -e '/Revision/{s/.*: *//p}')
    fi
else
    rev=${WANT_REVISION//[^0-9]/}   # take only digits
fi
VERSION=${VERSION:-$(date +%y.%m-${rev##*:})}
echo $VERSION > VERSION.tmp
if [ -f VERSION ] && cmp -s VERSION VERSION.tmp; then
    rm VERSION.tmp
else
    echo "Creating new file VERSION" >&2
    mv VERSION.tmp VERSION
fi
echo "================ Building version $VERSION" >&2

#
# Make sure automake is at least the version that Dwight Eisenhower used.
#
AM_VERSION=$(automake --version | perl -e '$a = <>; $a =~ / (\d+\.\d+.*)$/; print "$1"')
vercomp "$AM_VERSION" "1.7"
if [[ "$CMP" != "<" ]]; then
    echo "automake must be at least version 1.7; installed version is $AM_VERSION" >&2
    exit 1
fi
echo "automake version $AM_VERSION is OK" >&2

echo "Using $NJOBS jobs for make" >&2

#
# Clean the build area
#
if $DO_CLEAN || $DO_PURGE; then
    echo "
================ Uninstalling and cleaning
" >&2
    if [ -f $BUILDDIR/Makefile ] ; then
        make -C $BUILDDIR DESTDIR=$DESTDIR uninstall clean
    else
        echo "No makefile found in \"$BUILDDIR\"" >&2
    fi
fi

#
# Purge the build area
#
if $DO_PURGE; then
    echo "
================ Purging old Makefiles
" >&2
    rm -f Makefile ./bin/test/unit/Makefile ./bin/Makefile
    echo "
================ Purging files autogenerated by autoreconf
" >&2
    names=($BINDIR/'*.tx*' $PWD/'aclocal.m4'  $PWD/'autom4te.cache/' $PWD/'config.guess'
        $PWD/'config.h.in' $PWD/'config.sub'  $PWD/'depcomp'         $PWD/'install-sh' 
        $PWD/'.libs/'      $PWD/'ltmain.sh'   $PWD/'Makefile.in'     $PWD/'configure'
        $PWD/'missing')
    for filename in ${names[@]}
    do
        rm -rf $filename
    done
    echo "
================ Purging directory $BUILDDIR
" >&2
    rm -rf $BUILDDIR
    echo "
================ Purging directory $DISTDIR
" >&2
    rm -f $DISTDIR/*
    echo "
================ Purging directory $LIBDIR
" >&2
    rm -rf $LIBDIR
    echo "
================ Purging directory $TESTDIR/bin
" >&2
    rm -rf $TESTDIR/bin/*
    echo "
================ Purging directory $COVERAGEDIR
" >&2
    rm -rf $COVERAGEDIR
    echo "
================ Purging coverage files
" >&2
    rm -rf coverage.info*
fi

if $DO_AUTORECONF || ($DO_CONFIGURE && ! [ -f configure ]) ; then
    echo "
================ Running autoreconf
" >&2
    command -v autoconf >/dev/null 2>&1 || { echo "autoconf is not installed. Aborting." >&2; exit 1; }
    command -v libtool >/dev/null 2>&1  || { echo "libtool is not installed. Aborting." >&2;  exit 1; }
    autoreconf --force --install
fi

mkdir -pv $BUILDDIR
# TODO: If BUILDDIR has no slashes, it's relative so let's make TOPDIR
# be "..".  This will reduce clutter on commands.
TOPDIR=$PWD

#
# Everything from here is done in BUILDDIR
#
if $DO_CONFIGURE; then
    echo "
================ Running configure
" >&2
    (cd $BUILDDIR; $TOPDIR/configure --prefix=/usr)
fi

if $DO_MAKE || $DO_SHIP; then

    echo "
================ Running make install
" >&2
    make -C $BUILDDIR -j$NJOBS install DESTDIR=$DESTDIR CFLAGS="-Wall $OPTFLAGS" CXXFLAGS="-Wall $OPTFLAGS" LDFLAGS="$LDFLAGS" 
    make -C $BUILDDIR/test/unit -j$NJOBS install DESTDIR=$TESTDIR prefix="" CFLAGS="-Wall $OPTFLAGS" CXXFLAGS="-Wall $OPTFLAGS" LDFLAGS="$LDFLAGS"
fi

if $DO_SHIP; then
    echo "
================ Building kits
" >&2
    # When built for shipping, there should be no M in the version string
    case $VERSION in
        *M*)    echo "WARNING: Danger Will Robinson! Danger! There should be no 'M'" >&2;;
        *)      
    esac

    # Tarball for sources
    TARBALL=dist/job-$VERSION.tgz
    tar -czf $TARBALL                   \
        --transform 's,^,job/,'         \
        --exclude-vcs                   \
        --exclude-backups               \
        --exclude='*~'                  \
        --exclude='kit/etc/job/qdefs/*' \
        --exclude='kit/var/spool/job/*' \
        --exclude='dist/*'              \
        --exclude='m4/*'                \
        --exclude='test/unit/*.tx'      \
        --exclude='test/unit/*.in'      \
        --exclude='test/roots/*'        \
        --exclude='test/tmp/*'          \
        --exclude='kit/usr/lib/*'       \
        --exclude='kit/usr/bin/catjob'  \
        --exclude='kit/usr/bin/mkjob'   \
        --exclude='kit/usr/bin/queman'  \
        --exclude='kit/usr/bin/jobman'  \
        --exclude='kit/var/lib/job/*'   \
        --exclude='kit/var/log/job/*'   \
        --exclude='kit/usr/share/man/man5/*'  \
        --exclude='kit/usr/share/man/man7/*'  \
        --exclude='kit/usr/share/man/man8/*'  \
        acbuild.sh      \
        accolor         \
        configure.ac    \
        Makefile.am     \
        CONTRIBUTING.md \
        IMPLEMENTATION  \
        LICENSE         \
        README          \
        TODO            \
        VERSION         \
        dist/           \
        kit/            \
        m4/             \
        man/*.pod       \
        package/        \
        src/            \
        test/
    echo "Source tarball: $TARBALL"

    # Debian or RedHad kit...
    mkdir -p dist/
    if [ $buildPlatform == "rh" ]
    then
        make -f package/mkrpm KIT=job RPM_PLAT=$OS
    elif [ $buildPlatform == "ce" ]
    then
        make -f package/mkrpm KIT=job RPM_PLAT=$buildPlatform$BIT
    elif [ $buildPlatform == "ub" ]
    then
        sh package/mkdeb $buildPlatform$BIT $VERSION
    elif [ $buildPlatform == "debian" ]
    then
        sh package/mkdeb $buildPlatform$BIT $VERSION
    else
        echo "Package build - Unsupported OS $OS" >&2
    fi
fi

if $BE_SMALL; then
    echo "
================ Stripping executables
" >&2
    for fn in kit/usr/bin/*; do
        elf=$(file $fn | grep ELF | wc -l)
        if [ $elf -ne 0 ]; then
            strip $fn 2>/dev/null
        fi
    done
    strip kit/usr/lib/* 2>/dev/null
    echo "done"
fi

if $DO_UNIT_TEST; then
    echo "
================ Running unit tests
" >&2
    test/testall.pl -x -o
fi

if $DO_INTG_TEST; then
    echo "
================ Running integration tests
" >&2
    test/testall.pl -t
fi

if $DO_COVERAGE; then
    echo "
================ Generating a test coverage report
" >&2
    build/coverage.py
fi

