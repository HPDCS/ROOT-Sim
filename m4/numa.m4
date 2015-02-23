dnl @synopsis CHECK_NUMA()
dnl
dnl This macro searches for an installed numa library. If nothing was
dnl specified when calling configure, it searches first in /usr/local
dnl and then in /usr. If the --with-numa=DIR is specified, it will try
dnl to find it in DIR/include/numa.h and DIR/lib/libz.a. If
dnl --without-numa is specified, the library is not searched at all.
dnl
dnl If either the header file (numa.h) or the library (libz) is not
dnl found, the configuration exits on error, asking for a valid numa
dnl installation directory or --without-numa.
dnl
dnl The macro defines the symbol HAVE_LIBZ if the library is found. You
dnl should use autoheader to include a definition for this symbol in a
dnl config.h file. Sample usage in a C/C++ source is as follows:
dnl
dnl   #ifdef HAVE_LIBZ
dnl   #include <numa.h>
dnl   #endif /* HAVE_LIBZ */
dnl
dnl @category InstalledPackages
dnl @author Alessandro Pellegrini <pellegrini@dis.uniroma1.it>
dnl @version 20015-02-23
dnl @license GPL

AC_DEFUN([CHECK_NUMA],
AC_LANG_PUSH([C])
AC_CHECK_LIB(numa, numa_num_possible_nodes, [numa_cv_libnuma=yes], [numa_cv_libnuma=no])
AC_CHECK_HEADER(numa.h, [numa_cv_numa_h=yes], [numa_cv_numa_h=no])
AC_CHECK_PROG([numactl],[numactl],[yes],[no])

if test "$numa_cv_libnuma" = "yes" -a "$numa_cv_numa_h" = "yes" -a "x$numactl" = "xyes"
then
        #
        # We have found everything that we need, but the user can disable the subsystem
        #

	AC_ARG_ENABLE([numa],
	AS_HELP_STRING([--disable-numa], [Disable NUMA subsystem on NUMA machines (enabled by default)]))

	AS_IF([test "x$enable_numa" != "xno"], [
		AC_DEFINE([HAVE_NUMA])
 	])
fi

AC_LANG_POP
])
