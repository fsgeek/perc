
# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([perc],[0.1],[fsgeek@cs.ubc.ca])
AM_INIT_AUTOMAKE([-Wall -Wno-portability foreign])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

#AC_PATH_PROG(CC_PATH, $CC, )
#CC=/opt/intel/system_studio_2018/compilers_and_libraries/linux/bin/intel64/icc
CC=icc
#CXX=icpc

# use gnu extensions
# AC_USE_SYSTEM_EXTENSIONS

LT_INIT([])
LIBTOOL='$(SHELL) $(top_builddir)/libtool --no-warn'
AC_SUBST(LIBTOOL)

# Checks for programs.
AM_PROG_AS
AC_PROG_CC([icc gcc])
AC_PROG_CXX([icpc g++])
LT_INIT

# enable libtool library interdepenency checks
m4_ifdef([LT_OUTPUT], [LT_OUTPUT])
link_all_deplibs=yes
link_all_deplibs_CXX=yes

# check for libraries
AC_CHECK_LIB([pthread], [pthread_create], [:], [AC_MSG_ERROR(libpthread required)])

# check for packages
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.22.4])

# check for header files
AC_CHECK_HEADERS([stdlib.h stdio.h stdint.h fcntl.h unistd.h string.h sys/mman.h immintrin.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRERROR_R
AC_CHECK_FUNCS([clock_gettime dup2 gettimeofday nl_langinfo setlocale strcasecmp strerror strtoul])


AC_CONFIG_FILES([ Makefile munit/Makefile cache/Makefile cache/daxtest/Makefile stream/Makefile alloc/Makefile cpu/Makefile kiss/Makefile kiss/test/Makefile])
#AC_CONFIG_SUBDIRS([])
AC_OUTPUT
