#
# Process this file with autoconf to produce a configure script.
#

AC_PREREQ([2.59])

# Use 'VERSION' files to get version.
m4_define([JOB_VERSION_DATA], m4_include([VERSION]))

# Strip off newline characters.
m4_define([JOB_VERSION], m4_substr(JOB_VERSION_DATA, [0], m4_decr(m4_len(JOB_VERSION_DATA))))

AC_INIT(job, JOB_VERSION, job@linux.example.com)
AM_INIT_AUTOMAKE([foreign -Wall subdir-objects])
##AC_CONFIG_SRCDIR([src/app/rda-midway/rda-midway.cpp])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_DISABLE_STATIC
#AM_PROG_AR

# library version
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html#Updating-version-info
AC_SUBST([JOB_LIB_VERSION],[1:0:0],[libtool version of joblib])

# Checks for programs.
#CXXFLAGS="--coverage"
AC_PROG_CXX
AC_PROG_CC
AC_PROG_LIBTOOL

# Checks for libraries.
##AC_CHECK_LIB([crypto], [X509_new])
##AC_CHECK_LIB([ssl], [SSL_library_init])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_C_RESTRICT
AC_TYPE_SIZE_T
# These macros are not defined until automake 2.60; RHEL5 has 2.59
#AC_TYPE_SSIZE_T
#AC_TYPE_UINT32_T
#AC_TYPE_UINT64_T
#AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MKTIME

AC_FUNC_WAIT3

AC_CONFIG_FILES([Makefile test/unit/Makefile])
AC_OUTPUT

