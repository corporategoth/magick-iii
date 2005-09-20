dnl Magick IRC Services
dnl
dnl (c) 2005 The Neuromancy Society <info@neuromancy.net>
dnl
dnl The above copywright may not be removed under any circumstances,
dnl however it may be added to if any modifications are made to this
dnl file.  All modified code must be clearly documented and labelled.
dnl
dnl This code is released under the GNU General Public License v2.0 or better.
dnl The full text of this license should be contained in a file called
dnl COPYING distributed with this code.  If this file does not exist,
dnl it may be viewed here: http://www.neuromancy.net/license/gpl.html
dnl
dnl =======================================================================
dnl $Id$
dnl =======================================================================
dnl
dnl For official changes (by The Neuromancy Society), please
dnl check the ChangeLog* files that come with this distribution.
dnl
dnl Third Party Changes (please include e-mail address):
dnl
dnl N/A
dnl
dnl =======================================================================

dnl Standard stuff
dnl ==============

AC_INIT(Magick IRC Services, 3.0.0, magick-bugs@neuromancy.net)
AC_CONFIG_SRCDIR(src/main.cpp)
AM_INIT_AUTOMAKE
AC_CONFIG_FILES([	Makefile po/Makefile.in m4/Makefile
					include/Makefile
					src/Makefile
					python/Makefile
					doc/Makefile
				])
AM_CONFIG_HEADER(include/config.h)

AH_TOP([#ifndef _MAGICK_CONFIG_H
#define _MAGICK_CONFIG_H 1])
AH_BOTTOM([#endif // _MAGICK_CONFIG_H])

AC_PROG_CC
AC_PROG_CXX
AC_REQUIRE_CPP
AC_GNU_SOURCE
AC_SYS_LARGEFILE
dnl AC_HEADER_TIME
dnl AC_HEADER_STDC
dnl AC_HEADER_STAT
dnl AC_HEADER_DIRENT
dnl AC_C_BIGENDIAN
AC_LANG_CPLUSPLUS
AC_CANONICAL_HOST
dnl AC_LIBTOOL_DLOPEN
dnl AC_PROG_LIBTOOL
dnl AC_STRUCT_TIMEZONE
AM_PATH_PYTHON(2.2)

dnl Required stuff
dnl ==============

dnl Here is some flags that may need to be changed.
dnl some of these are compiler specific flags, which means
dnl we will probably have to put in some pre-configure script
dnl (ie. build.sh or something) to normalize things ...
dnl Stuff for a later date.  Right now, we'll keep it here.

dnl NOTE: CFLAGS = C only, CXXFLAGS = C++ only, CPPFLAGS = both

dnl Optimisation and Debugging flags
dnl -g3   Increased output to gdb (for macros, etc)
dnl -pg   Profiling output for gprof
dnl -dm   Output memory stats when compile is done
CPPOPT=""
COPT=""
CXXOPT=""

dnl What warnings we will give off
dnl -Wall == -Wno-import -Wchar-subscripts -Wcomment -Wformat
dnl          -Wimplicit-int -Wimplicit-function-declaration
dnl          -Wimplicit -Wmain -Wmissing-braces -Wmultichar
dnl          -Wparentheses -Wsequence-point -Wreturn-type
dnl          -Wswitch -Wtrigraphs -Wunused -Wuninitialized
dnl          -Wreorder -Wunknown-pragmas
dnl REMOVED: -Wshadow -Wcast-qual -Wconversion -Wcast-align
dnl          -Wold-style-cast -Wundef -Wredundant-decls
dnl          -Winline -Wtemplate-debugging -Wpacked
dnl          -Wtraditional -Wstrict-prototypes 
dnl          -Wformat-security -Wundef -Wmissing-prototypes
dnl POSSIBLES: -Wunreachable-code -Wdisabled-optimization
CPPWARN="-W -Wall -Wimport -Wpointer-arith -Wsign-compare -Wwrite-strings"
CWARN="-Wbad-function-cast -Wmissing-declarations -Wnested-externs"
CXXWARN="-Woverloaded-virtual -Wsynth -Wno-deprecated"

CPPFLAGS="$CPPFLAGS $CPPOPT $CPPWARN"
CFLAGS="$CPPFLAGS $CFLAGS $COPT $CWARN"
CXXFLAGS="$CPPFLAGS $CXXFLAGS $CXXOPT $CXXWARN"

CPPFLAGS="$CPPFLAGS -I\$(top_srcdir)/include"

dnl ====================================================

dnl Find out which to use for C++ headers ...
AC_CHECK_HEADERS(mantra/config.h,,
	AC_MSG_ERROR([Magick requires the Mantra library (http://www.neuromancy.net/mantra).]))

AC_CHECK_HEADERS(boost/version.hpp,,
	AC_MSG_ERROR([Magick requires the Boost library (http://www.boost.org).]))

AC_MSG_CHECKING([for multi-threaded compile argument])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <boost/config/requires_threads.hpp>]],
  [[]])], [AC_MSG_RESULT([none required])],
[
  for x in -pthread -pthreads -mthreads -openmp -mt -tWM -D_REENTRANT -D_MT -D_SGI_MP_SOURCE
  do
	OLD_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $x"
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <boost/config/requires_threads.hpp>]],
	  [[]])],[ac_cv_thread_cmdline_arg=yes], [ac_cv_thread_cmdline_arg=no])
	if test "x$ac_cv_thread_cmdline_arg" = "xyes"; then
	  AC_MSG_RESULT([$x])
	  break
	fi
	CPPFLAGS="$OLD_CPPFLAGS"
  done
  if test "x$ac_cv_thread_cmdline_arg" = "xno"; then
	AC_MSG_ERROR([Magick requires multi-thread support.])
  fi
])

dnl ====================================================

LIBS="$LIBS -lmantra"
dnl AC_CHECK_LIB(mantra, _init, ,
dnl		[AC_MSG_ERROR([Mantra library not found, please specify its location.])])
AC_CHECK_LIB(mantra_core_hasher, _init, ,
	[AC_MSG_ERROR([Mantra Hasher library not found, please specify its location.])])
AC_CHECK_LIB(dl, dlopen, ,
	[AC_MSG_ERROR([Dynamic loading libary not found.])])
AM_GNU_GETTEXT([external])

AC_CHECK_LIB(mantra_storage_xml, _init, [echo >/dev/null],
	[AC_MSG_WARN([[Mantra's XML storage mechanism not found, disabled.]])])
AC_CHECK_LIB(mantra_storage_berkeleydb, _init, [echo >/dev/null],
	[AC_MSG_WARN([[Mantra's Berkeley DB storage mechanism not found, disabled.]])])
AC_CHECK_LIB(mantra_storage_postgresql, _init, [echo >/dev/null],
	[AC_MSG_WARN([[Mantra's PostgreSQL storage mechanism not found, disabled.]])])
AC_CHECK_LIB(mantra_storage_mysql, _init, [echo >/dev/null],
	[AC_MSG_WARN([[Mantra's MySQL storage mechanism not found, disabled.]])])
AC_CHECK_LIB(mantra_storage_sqlite, _init, [echo >/dev/null],
	[AC_MSG_WARN([[Mantra's SQLite storage mechanism not found, disabled.]])])
AC_CHECK_LIB(mantra_storage_stage_compress, _init, [echo >/dev/null],
	[AC_MSG_WARN([[Mantra's Compression Stage not found, disabled.]])])
AC_CHECK_LIB(mantra_storage_stage_crypt, _init, [echo >/dev/null],
	[AC_MSG_WARN([[Mantra's Encryption Stage not found, disabled.]])])

AC_CHECK_LIB(python$PYTHON_VERSION, PyRun_String,
	[PYTHON_INCLUDE=`$PYTHON -c 'import sys; print sys.prefix'`/include/python$PYTHON_VERSION
	 AC_SUBST(INC_PYTHON, -I$PYTHON_INCLUDE)
	 AC_SUBST(LIB_PYTHON, -lpython$PYTHON_VERSION)
	 AC_CHECK_HEADERS($PYTHON_INCLUDE/Python.h)],
	[AC_MSG_WARN([[Python not found, the scripting facilities will be disabled.]])])

AC_DEFINE_UNQUOTED(MAGICK_VERSION_MAJOR, 
	`echo $PACKAGE_VERSION | cut -f1 -d.`, [Major revision of Magick.])
AC_DEFINE_UNQUOTED(MAGICK_VERSION_MINOR,
	`echo $PACKAGE_VERSION | cut -f2 -d.`, [Minor revision of Magick.])
AC_DEFINE_UNQUOTED(MAGICK_VERSION_PATCH, 
	`echo $PACKAGE_VERSION | cut -f3 -d.`, [Patch revision of Magick.])

AC_DEFINE(_ISOC99_SOURCE, 1, [Leave this alone.])
dnl AC_DEFINE_UNQUOTED(_POSIX_C_SOURCE, 200112L, [Leave this alone.])
AC_DEFINE_UNQUOTED(_XOPEN_SOURCE, 600, [Leave this alone.])
AC_DEFINE(_XOPEN_SOURCE_EXTENDED, 1, [Leave this alone.])
AC_DEFINE(__EXTENSIONS__, 1, [Solaris trigger for some extensions.])
AC_DEFINE(_REENTRANT, 1, [Define to make threads recursive.])

dnl Other ...
if test "$boost_rw_mutex" = "false"; then
	AC_CHECK_FUNCS([pthread_rwlock_timedrdlock])
fi

AC_CHECK_FUNCS([bindtextdomain])

dnl ====================================================

AC_OUTPUT
