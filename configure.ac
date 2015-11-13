C_PREREQ([2.67])

AC_INIT([ROOT-Sim],[1.0.0],[rootsim@googlegroups.com],[rootsim],[http://www.dis.uniroma1.it/~hpdcs/ROOT-Sim/])
AC_CONFIG_SRCDIR([src])
AM_INIT_AUTOMAKE([-Wno-portability 1.10.1 -Wall foreign subdir-objects])

AC_CONFIG_MACRO_DIR([m4])
AC_USE_SYSTEM_EXTENSIONS

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])



# Debugging symbols or not?
CFLAGS="-O3 -lm -U_FORTIFY_SOURCE"
AC_ARG_ENABLE([debug],
    AS_HELP_STRING([--enable-debug], [Enable debugging of the platform]))

AS_IF([test "x$enable_debug" = "xyes"], [
	CFLAGS="-g3 -Wall -Wextra -O0 -Wswitch-enum -Wcast-align -Wpointer-arith -Wstrict-overflow=5 -Wstrict-prototypes -Winline -Wundef -Wnested-externs -Wshadow -Wunreachable-code -Wfloat-equal -Wredundant-decls -Wold-style-definition -std=c99 -fno-omit-frame-pointer -ffloat-store -fno-common -fstrict-aliasing -fgnu89-inline -rdynamic -lm"
])

AC_ARG_ENABLE([profile],
    AS_HELP_STRING([--enable-profile], [Enable profiling the platform]))

# Extra check flags
AC_ARG_ENABLE(extra-checks,
AC_HELP_STRING([--enable-exrta-checks],
[Enable extra checks on models (kills performance!)]),
[enable_extra_checks=$enableval],[enable_extra_checks=no])
AS_IF([test "x$enable_extra_checks" = "xyes"], [
	AC_DEFINE([EXTRA_CHECKS])
])

AS_IF([test "x$enable_profile" = "xyes"], [
	CFLAGS="-pg $CFLAGS"
])

#----------------------------------------------------------------------------
# Check for languages
#----------------------------------------------------------------------------
AC_LANG([C])


#----------------------------------------------------------------------------
# Checks for programs.
#----------------------------------------------------------------------------
m4_pattern_allow([AM_PROG_AR])
AC_PROG_CC([gcc])
AC_PROG_MAKE_SET
AC_PROG_MKDIR_P
AC_PROG_CC_C_O
AM_PROG_CC_C_O
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AM_PROG_AR
AM_PROG_AS
AC_CHECK_PROGS([HIJACKER], [hijacker])
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
ac_have_as_rdtsc=yes
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
ac_have_as_sse3=yes
], [
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
ac_have_as_ssse3=yes
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
ac_have_as_sse42=yes
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
  ac_enable_ult=yes
])


# If our libtimestretch library is installed, then we can use Preemptive Time Warp
AC_CHECK_LIB(timestretch, ts_start, [libtimestretch=yes], [libtimestretch=no])
ac_preemption=no
if test "x$libtimestretch" = "xyes" -a "x$enable_ult" != "xno"
then
        #
        # We have found everything that we need, but the user can disable the subsystem
        #

	AC_ARG_ENABLE([preemption],
	AS_HELP_STRING([--disable-preemption], [Disable Preemptive Time Warp)]))

	AS_IF([test "x$enable_preemption" != "xno"], [
		AC_DEFINE([HAVE_PREEMPTION])
		ac_have_preemption=yes
		CFLAGS="$CFLAGS -ltimestretch"
	])
fi



# LP binding support
AC_ARG_ENABLE([rebinding],
AS_HELP_STRING([--disable-rebinding], [Disable LPs Rebinding (Enabled by default)]))

AS_IF([test "x$enable_rebinding" != "xno"], [
        AC_DEFINE([HAVE_LP_REBINDING])
        ac_have_lp_rebinding=yes
])

# Custom Parallel Allocator can be disabled, to fallback to standard malloc library
AC_ARG_ENABLE([allocator],
AS_HELP_STRING([--disable-allocator], [Disable Parallel Allocator (Enabled by default)]))

AS_IF([test "x$enable_allocator" != "xno"], [
        AC_DEFINE([HAVE_PARALLEL_ALLOCATOR])
        ac_have_parallel_allocator=yes
])



# If we are on a NUMA system, try to use the NUMA subsystem
AC_CHECK_LIB(numa, numa_num_possible_nodes, [numa_cv_libnuma=yes], [numa_cv_libnuma=no])
AC_CHECK_HEADER(numa.h, [numa_cv_numa_h=yes], [numa_cv_numa_h=no])
AC_CHECK_PROG([numactl],[numactl],[yes],[no])
ac_have_numa=no

if test "x$numa_cv_libnuma" = "xyes" -a "x$numa_cv_numa_h" = "xyes" -a "x$numactl" = "xyes" -a "x$ac_have_parallel_allocator" = "xyes"
then
        #
        # We have found everything that we need, but the user can disable the subsystem
        #

	AC_ARG_ENABLE([numa],
	AS_HELP_STRING([--disable-numa], [Disable NUMA subsystem on NUMA machines (enabled by default)]))

	AS_IF([test "x$enable_numa" != "xno"], [
		AC_DEFINE([HAVE_NUMA])
		ac_have_numa=yes
		CFLAGS="$CFLAGS -lnuma"
	])
fi


# If requested, enable reverse computation
if test "x$HIJACKER" = "xhijacker"
then
	AC_ARG_ENABLE([reverse],
	AS_HELP_STRING([--enable-reverse], [Enable reverse computation]))

	AS_IF([test "x$enable_reverse" = "xyes"], [
	        AC_DEFINE([HAVE_REVERSE])
	        ac_have_reverse=yes
	])
fi




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
AC_SUBST([cc_hijacker_conf_lines], [$(wc -l scripts/hijacker-conf.xml | sed 's,\ .*,,g')])
AC_MSG_RESULT([ok])

# Additionally, test whether we have to call hijacker from rootsim-cc
AM_CONDITIONAL([USE_HIJACKER], [test "x$enable_reverse" = "xyes"])


# Generate Makefiles for all these subfolders
#AC_OUTPUT
AC_CONFIG_FILES([Makefile man/Makefile scripts/Makefile src/Makefile])
AC_OUTPUT



#----------------------------------------------------------------------------
# Generate a summary of the configuration adopted on the current machine.
# At this step, we give as well some information on how to enable/disable
# autoconfigured stuff.
#----------------------------------------------------------------------------

# Compose the message regarding the CPU
current_cpu="${host_cpu} (available supports: "
if test "x$ac_have_as_rdtsc" = "xyes"
then
	current_cpu="${current_cpu}rdtcs "
fi

if test "x$ac_have_as_sse3" = "xyes"
then
	current_cpu="${current_cpu}SSE3 "
fi

if test "x$ac_have_as_ssse3" = "xyes"
then
	current_cpu="${current_cpu}SSSE3 "
fi

if test "x$ac_have_as_sse42" = "xyes"
then
	current_cpu="${current_cpu}SSE4.2 "
fi
current_cpu="${current_cpu})"


# Compose the debugging message
if test "x$enable_debug" = "xyes"
then
	enable_debug="Enabled"
else
	enable_debug="Disabled (use --enable-debug if wanted)"
fi

# Compose the ULT message
if test "x$ac_enable_ult" = "xyes"
then
	ac_enable_ult="Enabled (use --disable-ult if not wanted)"
else
	ac_enable_ult="Disabled"
fi


# Compose the message regarding the Parallel Allocator
if test "x$ac_have_parallel_allocator" = "xyes"
then
	ac_parallel_allocator="Enabled (By default, use --disable-parallel-allocator if not wanted)"
else
	ac_parallel_allocator="Disabled (manually excluded)"
fi


# Compose the message regarding LP rebinding
if test "x$ac_have_lp_rebinding" = "xyes"
then
	ac_lp_rebinding="Enabled (By default, use --disable-rebinding if not wanted)"
else
	ac_lp_rebinding="Disabled (manually excluded)"
fi


# Compose the message regarding the NUMA subsystem
if test "x$ac_have_numa" = "xyes"
then
	numa_subsystem="Enabled (by default on NUMA machines, use --disable-numa if not wanted)"
else
	numa_subsystem="Disabled ("
	if test "x$numa_cv_libnuma" = "xno"
	then
		numa_subsystem="${numa_subsystem}libnuma "
	fi
	
	if test "x$numa_cv_numa_h" = "xno"
	then
		numa_subsystem="${numa_subsystem}numa.h "
	fi
	
	if test "x$numactl" = "xno" 
	then
		numa_subsystem="${numa_subsystem}numactl "
	fi
	
	numa_subsystem="${numa_subsystem}not found)"
	
	if test "x$numa_cv_libnuma" = "xyes" -a "x$numa_cv_numa_h" = "xyes" -a "x$numactl" = "xyes" 
	then
		numa_subsystem="Disabled (manually excluded)"
	fi

	if test "x$ac_have_parallel_allocator" != "xyes"
	then
		numa_subsystem="Disabled (Parallel Allocator is required)"

	fi
fi


# Compose the message regarding Preemptive Time Warp
if test "x$ac_have_preemption" = "xyes"
then
	ac_preemption="Enabled (by default with libtimestretch, use --disable-preemption if not wanted)"
else
	ac_preemption="Disabled"
	if test "x$enable_ult" = "xno"
	then
		ac_preemption="${ac_preemption} (User-Level Threads are necessary)"
	else
		if test "x$libtimestretch" = "xyes"
		then
			ac_preemption="${ac_preemption} by the user"
		else
			ac_preemption="${ac_preemption} (libtimestretch not found)"
		fi
	fi
fi


# Compose the message regarding reverse computing
if test "x$ac_have_reverse" = "xyes"
then
	ac_reverse="Enabled"
else
	ac_reverse="Disabled"
	if test "x$enable_reverse" = "xyes"
	then
		ac_reverse="${ac_reverse} (hijacker not found on the system)"
	else
		ac_reverse="${ac_reverse} (use --enable-reverse to activate)"
	fi
fi



cat<<EOF


   * ROOT-Sim Configuration on Current Machine *

CPU Architecture.......... : ${current_cpu}
Operating System.......... : ${host_os}
Debugging Support......... : ${enable_debug}
User-Level Threads........ : ${ac_enable_ult}
Parallel Allocator........ : ${ac_parallel_allocator}
NUMA Subsystem............ : ${numa_subsystem}
LP Preemption Support..... : ${ac_preemption}
LP Rebinding.............. : ${ac_lp_rebinding}
Reverse Computing......... : ${ac_reverse}


EOF
