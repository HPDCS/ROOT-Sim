C_PREREQ([2.67])

AC_INIT([ROOT-Sim],[1.0.0],[rootsim@googlegroups.com],[rootsim],[http://www.dis.uniroma1.it/~hpdcs/ROOT-Sim/])
AC_CONFIG_SRCDIR([src])
AM_INIT_AUTOMAKE([1.11 -Wall foreign subdir-objects])

m4_ifndef([AM_SILENT_RULES], [m4_define([AM_SILENT_RULES],[])])
AM_SILENT_RULES([yes]) # make --enable-silent-rules the default.

AC_CONFIG_MACRO_DIR([m4])
AC_USE_SYSTEM_EXTENSIONS




# Debugging symbols or not?
CFLAGS="-O3 -lm"
AC_ARG_ENABLE([debug],
    AS_HELP_STRING([--enable-debug], [Enable debugging of the platform]))

AS_IF([test "x$enable_debug" = "xyes"], [
	CFLAGS="-g3 -pg -Wall -Wextra -O0 -Wswitch-enum -Wcast-align -Wpointer-arith -Wstrict-overflow=5 -Wstrict-prototypes -Winline -Wundef -Wnested-externs -Wshadow -Wunreachable-code -Wfloat-equal -Wredundant-decls -Wold-style-definition -std=c99 -fno-omit-frame-pointer -ffloat-store -fno-common -fstrict-aliasing -fgnu89-inline -rdynamic -lm"
])



#----------------------------------------------------------------------------
# Check for languages
#----------------------------------------------------------------------------
AC_LANG([C])


#----------------------------------------------------------------------------
# Checks for programs.
#----------------------------------------------------------------------------
AC_PROG_CC([gcc])
AC_PROG_MAKE_SET
AC_PROG_MKDIR_P
AC_PROG_CC_C_O
AM_PROG_CC_C_O
LT_INIT



#----------------------------------------------------------------------------
# Check for architecture
#----------------------------------------------------------------------------
#
# We rely on assembly modules, so we can support only the following architectures.
# Perform a test here, and halt if an unsupported architecture is found.

AC_CANONICAL_HOST

AC_MSG_CHECKING([for a supported CPU])

case "${host_cpu}" in

# When we will resupport x86, uncomment these lines!

     i?86)
	AC_MSG_RESULT([yes, ${host_cpu}])
	AC_DEFINE([ARCH_X86])
	ARCH_MAX="x86"
	;;

     x86_64)
        AC_MSG_RESULT([yes, ${host_cpu}])
        AC_DEFINE([ARCH_X86_64])
        ;;

#     *)
#        AC_MSG_RESULT([no, ${host_cpu}])
#        AC_MSG_ERROR([Unsupported host architecture. Currently ROOT-Sim supports only x86_64 systems.])
#        ;;

esac



#----------------------------------------------------------------------------
# Checks for C header files and libraries
#----------------------------------------------------------------------------
AC_CHECK_LIB([m], [log], [true], AC_MSG_FAILURE([libm not found]))

# Check for POSIX thread support
AX_PTHREAD([], [AC_MSG_ERROR([PTHREAD support not found])])

LIBS="$PTHREAD_LIBS $LIBS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
CC="$PTHREAD_CC"

AC_SUBST([LIBS])
AC_SUBST([CFLAGS])
AC_SUBST([CC])



#----------------------------------------------------------------------------
# Check for Operating System
#----------------------------------------------------------------------------
#
# This must go after CPU check, as it relies on AC_CANONICAL_HOST
#

AC_MSG_CHECKING([for supported host Operating System])

case $host_os in
  linux*)
        # Do something specific for linux
        AC_MSG_RESULT([yes, ${host_os}])
        AC_DEFINE([OS_LINUX])
	;;
  cygwin*)
        # Do something specific for cygwin
        AC_MSG_RESULT([yes, ${host_os}])
        AC_DEFINE([OS_CYGWIN])
	;;
    *)
        #Default Case
        AC_MSG_RESULT([no, ${host_os}])
        AC_MSG_ERROR([Your platform is not currently supported])
        ;;
esac




#----------------------------------------------------------------------------
# Check if we can use the tick timer
#----------------------------------------------------------------------------


AC_MSG_CHECKING([if assembler knows rdtsc instruction])

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[
  do { 
    unsigned int lo;
    unsigned int hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    ((unsigned long long)hi) << 32 | lo;
  }
  while (0)
]])], [
AC_DEFINE([HAVE_RDTSC])
AC_MSG_RESULT([yes])
], [
AC_MSG_RESULT([no])
])


#----------------------------------------------------------------------------
# Family instructions checks.
# These are used to support a specific version of the memcmp for the
# early check termination.
#----------------------------------------------------------------------------


# does the x86/amd64 assembler understand SSE3 instructions?
AC_MSG_CHECKING([if assembler knows SSE3])

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[
  do { long long int x;
     __asm__ __volatile__("fisttpq (%0)" : :"r"(&x) ); }
  while (0)
]])], [
AC_DEFINE([HAVE_SSE3])
AC_MSG_RESULT([yes])
], [
ac_have_as_sse3=no
AC_MSG_RESULT([no])
])




# does the x86/amd64 assembler understand SSSE3 instructions?
AC_MSG_CHECKING([if assembler knows SSSE3])

save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -msse"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[
  do { long long int x;
   __asm__ __volatile__(
      "pabsb (%0),%%xmm7" : : "r"(&x) : "xmm7" ); }
  while (0)
]])], [
AC_DEFINE([HAVE_SSSE3])
AC_MSG_RESULT([yes])
], [
AC_MSG_RESULT([no])
])
CFLAGS="$save_CFLAGS"





# does the x86/amd64 assembler understand SSE 4.2 instructions?
AC_MSG_CHECKING([if assembler knows SSE4.2])

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[
  do { long long int x;
   __asm__ __volatile__(
      "crc32q %%r15,%%r15" : : : "r15" );
   __asm__ __volatile__(
      "pblendvb (%%rcx), %%xmm11" : : : "memory", "xmm11");
   __asm__ __volatile__(
      "aesdec %%xmm2, %%xmm1" : : : "xmm2", "xmm1"); }
  while (0)
]])], [
AC_DEFINE([HAVE_SSE42])
AC_MSG_RESULT([yes])
], [
AC_MSG_RESULT([no])
])



#----------------------------------------------------------------------------
# COMPILE TIME OPTIONS
#
# From here on, we enable/disable compile time switches for the simulator
#----------------------------------------------------------------------------


# Disable ULT. Everything is run in the same context, in the same stack

AC_ARG_ENABLE([ult],
    AS_HELP_STRING([--disable-ult], [Disable User-Level Threads for LP scheduling]))

AS_IF([test "x$enable_ult" != "xno"], [
  AC_DEFINE([ENABLE_ULT])
])




#----------------------------------------------------------------------------
# Doxygen Documentation
#
# If doxygen is installed on the box, generate a commodity Makefile in the
# doc subfolder to generate the documentation. This must be called explicitly,
# as it is separated from the main Makefile.
#----------------------------------------------------------------------------
AC_CHECK_PROGS([DOXYGEN], [doxygen])
AM_CONDITIONAL([HAVE_DOXYGEN], [test -n "$DOXYGEN"])
AM_COND_IF([HAVE_DOXYGEN], [AC_CONFIG_FILES([doc/doxy.conf])])
AM_COND_IF([HAVE_DOXYGEN], [AC_CONFIG_FILES([doc/Makefile])])



#----------------------------------------------------------------------------
# Check scripts sizes.
# The compiler script is dynamically built in order to create ad compile
# time linker scripts which will be used to produce the final version
# of the executable. We compute here the sizes of those cat'd scripts
# in order to make the compiler behave consistently with the last version
# of the linker scripts provided in the installation.
#----------------------------------------------------------------------------
AC_MSG_CHECKING([compiler and linker scripts sizes])
AC_SUBST([cc_script_lines], [$(wc -l scripts/rootsim-cc.in | sed 's,\ .*,,g')])
AC_SUBST([cc_lddata1_lines], [$(wc -l scripts/ld-data1 | sed 's,\ .*,,g')])
AC_SUBST([cc_lddata2_lines], [$(wc -l scripts/ld-data2 | sed 's,\ .*,,g')])
AC_SUBST([cc_ldfinal_lines], [$(wc -l scripts/ld-final | sed 's,\ .*,,g')])
AC_MSG_RESULT([ok])


# Generate Makefiles for all these subfolders
#AC_OUTPUT
AC_CONFIG_FILES([Makefile man/Makefile scripts/Makefile src/Makefile])
AC_OUTPUT

