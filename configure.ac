C_PREREQ([2.67])

AC_INIT([ROOT-Sim],[1.0.0],[rootsim@googlegroups.com],[rootsim],[http://www.dis.uniroma1.it/~hpdcs/ROOT-Sim/])
AC_CONFIG_SRCDIR([src])
AM_INIT_AUTOMAKE([-Wno-portability 1.10.1 -Wall foreign subdir-objects])

AC_CONFIG_MACRO_DIR([m4])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])


# Debugging symbols or not?
AC_ARG_ENABLE([debug],
    AS_HELP_STRING([--enable-debug], [Enable debugging of the platform]))

AM_CONDITIONAL([DEBUG], [test x$enable_debug = xyes])


# Setup compilation flags
CFLAGS_COMMON="-std=c99 -fno-omit-frame-pointer -ffloat-store -fno-common -fstrict-aliasing -fgnu89-inline -rdynamic -lm -U_FORTIFY_SOURCE"
CFLAGS_WARNINGS="-Wall -Wextra -Wcast-align -Wpointer-arith -Wstrict-overflow=5 -Wstrict-prototypes -Winline -Wundef -Wnested-externs -Wshadow -Wunreachable-code -Wfloat-equal -Wredundant-decls -Wold-style-definition -Wno-switch-enum"

AS_IF([test x$enable_debug = xyes],
      [CFLAGS="-g3 -O0 $CFLAGS_COMMON $CFLAGS_WARNINGS"],
      [CFLAGS="-O3 $CFLAGS_COMMON"]
      AC_DEFINE([NDEBUG]))


AC_ARG_ENABLE([profile],
    AS_HELP_STRING([--enable-profile], [Enable profiling the platform]))

# Extra check flags
AC_ARG_ENABLE(extra-checks,
AS_HELP_STRING([--enable-exrta-checks],[Enable extra checks on models (kills performance!)]),
[enable_extra_checks=$enableval],[enable_extra_checks=no])
AS_IF([test "x$enable_extra_checks" = "xyes"], [
	AC_DEFINE([EXTRA_CHECKS])
])

AS_IF([test "x$enable_profile" = "xyes"], [
	CFLAGS="-pg $CFLAGS"
])


# MPI flags
AC_ARG_WITH([mpi],
            AS_HELP_STRING([--with-mpi], [compile with MPI (parallelization) support. Default: no]),
            [],
            [with_mpi=no])

AS_IF([test x"$with_mpi" != xno],[
	# Find a suitable MPI compiler: from CC and MPICC var or autodiscovery
	# Test the choosen compiler for MPI support.
	AX_PROG_CC_MPI(,,[AC_MSG_FAILURE(["Couldn't find any MPI compiler. Be sure that CC env variable is not set to an incompatible compiler"])])
	AC_DEFINE([HAVE_MPI])
	CFLAGS="$CFLAGS -DHAVE_MPI"
],[
	# MPI was not requested, force gcc compiler
	AC_PROG_CC([gcc])
])


# Don't move this before AX_PROG_CC_MPI as it clashed with that macro
AC_USE_SYSTEM_EXTENSIONS


#----------------------------------------------------------------------------
# Check for languages
#----------------------------------------------------------------------------
AC_LANG([C])


#----------------------------------------------------------------------------
# Checks for programs.
#----------------------------------------------------------------------------
m4_pattern_allow([AM_PROG_AR])

AC_PROG_MAKE_SET
AC_PROG_MKDIR_P
AC_PROG_CC_C_O
AM_PROG_CC_C_O
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AM_PROG_AR
AM_PROG_AS
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
	i?86)
		AC_MSG_RESULT([yes, ${host_cpu}])
		AC_DEFINE([ARCH_X86])
		ARCH_MAX="x86"
		;;
	x86_64)
		AC_MSG_RESULT([yes, ${host_cpu}])
		AC_DEFINE([ARCH_X86_64])
		;;
	*)
		AC_MSG_RESULT([no, ${host_cpu}])
		AC_MSG_ERROR([Unsupported host architecture. Currently ROOT-Sim supports only x86_64 systems.])
		;;
esac




#----------------------------------------------------------------------------
# Checks for C header files and libraries
#----------------------------------------------------------------------------
AC_CHECK_HEADER([valgrind/helgrind.h], [AC_DEFINE([HAVE_HELGRIND_H])])


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
        AC_MSG_RESULT([yes, ${host_os}])
        AC_DEFINE([OS_LINUX])
        ac_on_linux=yes
	;;
  cygwin*)
        AC_MSG_RESULT([yes, ${host_os}])
        AC_DEFINE([OS_WINDOWS])
        AC_DEFINE([HAVE_CYGWIN])
	;;
  mingw*)
        AC_MSG_RESULT([yes, ${host_os}])
        AC_DEFINE([OS_WINDOWS])
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



#------------------------------------------------------------
# Linux specific subsystems.
# On other architectures all these checks are just skipped
#------------------------------------------------------------
ac_preemption=no
ac_have_numa=no
ac_have_cross_state=no
ac_kernel_headers=no
ac_disable_modules=yes
ac_system_map=no

if test "x$ac_on_linux" = "xyes"
then

	# If our libtimestretch library is installed, then we can use Preemptive Time Warp
	AC_CHECK_LIB(timestretch, ts_start, [libtimestretch=yes], [libtimestretch=no])
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

	# If we are on a NUMA system, try to use the NUMA subsystem
	AC_CHECK_LIB(numa, numa_num_possible_nodes, [numa_cv_libnuma=yes], [numa_cv_libnuma=no])
	AC_CHECK_HEADER(numa.h, [numa_cv_numa_h=yes], [numa_cv_numa_h=no])
	AC_CHECK_PROG([numactl],[numactl],[yes],[no])

	if test "x$numa_cv_libnuma" = "xyes" -a "x$numa_cv_numa_h" = "xyes" -a "x$numactl" = "xyes" -a "x$ac_have_parallel_allocator" = "xyes"
	then
			#
			# We have found everything that we need, but the user can disable the subsystem
			#

		AC_ARG_ENABLE([numa],
		AS_HELP_STRING([--enable-numa], [Enable NUMA subsystem on NUMA machines]))

		AS_IF([test "x$enable_numa" = "xno"], [
			AC_DEFINE([HAVE_NUMA])
			ac_have_numa=yes
			CFLAGS="$CFLAGS -lnuma"
		])
	fi


	# Test for linux headers, to compile kernel modules
	ac_kernel_headers=no
	AC_CHECK_HEADERS([linux/ioctl.h],
		[ac_kernel_headers=yes])

	if test "x$ac_kernel_headers" = "xyes"
	then
		# Configure kernel module paths
		AC_SUBST([with_kernel], [`uname -r`])
		AC_SUBST([with_kernel_mod], [/lib/modules/$with_kernel/extra])
		AC_SUBST([KERNEL_SRC], [/lib/modules/$with_kernel/build])
		AC_SUBST([KERNEL_MOD], [$with_kernel_mod])
	fi

	# We might want to disable all kernel modules explicitly. This is useful, for example,
	# when installing the simulator in a per-user space, in a system where the modules
	# were already installed. Using the --disable-modules won't compile nor try to
	# install any module.
	AC_ARG_ENABLE([modules],
		AS_HELP_STRING([--enable-modules], [Enable compilation/installation of kernel modules]))

	AS_IF([test "x$enable_modules" = "xyes"], [
		ac_disable_modules=no
		ac_kernel_headers=yes
	])


	# Check if System.map is available. Some kernel modules require access to special symbols
	# which are not exported, but are available in System.map.
	AC_CHECK_FILE("/boot/System.map", [ac_system_map=yes])

	# Per-module checks. To be run only if we are on Linux and kernel headers are available
	if test "x$ac_kernel_headers" = "xyes"
	then

		# Here go modules which require System.map
		if test "x$ac_system_map" = "xyes"
		then

			# Get finish_task_switch in the current kernel
			AC_MSG_CHECKING([for finish_task_switch in the current kernel])
			fts_line=$($GREP -n finish_task_switch /boot/System.map-$(uname -r) | $SED 's/:.*//')
			finish_task_switch=$($SED "${fts_line}q;d" /boot/System.map-$(uname -r) | $SED 's/ .*//')
			if test -z "$finish_task_switch"; then
				AC_MSG_ERROR([Address of finish_task_switch not found in kernel map])
			fi
			AC_MSG_RESULT([found at 0x$finish_task_switch])
			AC_SUBST([FTS_ADDR], [0x$finish_task_switch])

			# Get function after finish_task_switch in the current kernel
			AC_MSG_CHECKING([the address of function next to finish_task_switch in the current kernel])
			let fts_line=$fts_line+1
			finish_task_switch_next=$($SED "${fts_line}q;d" /boot/System.map-$(uname -r) | $SED 's/ .*//')
			if test -z "$finish_task_switch_next"; then
				AC_MSG_ERROR([Address of function after finish_task_switch not found in kernel map])
			fi
			AC_MSG_RESULT([found at 0x$finish_task_switch_next])
			AC_SUBST([FTS_ADDR_NEXT], [0x$finish_task_switch_next])

		fi # closes the System.map modules check

	fi # closes the global Linux kernel headers availability check

	# Event Cross State dependency manager
	AC_ARG_ENABLE([ecs],
	AS_HELP_STRING([--enable-ecs], [Enable Event Cross-State Management]))

	AS_IF([test "x$enable_ecs" = "xyes"], [
		AC_DEFINE([HAVE_CROSS_STATE])
		ac_have_cross_state=yes
	])

fi # Closes che global check for Linux availability

# This instructs the Makefile to build kernel modules only when possible
AM_CONDITIONAL([NO_KERNEL_MODULES], [test "x$ac_disable_modules" = "xyes"])

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
AC_CONFIG_FILES([Makefile
		 man/Makefile
		 scripts/Makefile
		 src/Makefile
		 src/arch/linux/modules/Makefile])
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

# Compose the message regarding mpi
if test x"$with_mpi" != xno
then
	enable_mpi="Enabled (compiler: $CC)"
else
	enable_mpi="Disabled (enable with --with-mpi)"
fi

# Compose the message regarding the Parallel Allocator
if test "x$ac_have_parallel_allocator" = "xyes"
then
	ac_parallel_allocator="Enabled (By default, use --disable-allocator if not wanted)"
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


# Compose the message regarding ECS
if test "x$ac_have_cross_state" = "xyes"
then
	ac_ecs="Enabled)"
else
	ac_ecs="Disabled"
	if test "x$ac_on_linux" = "xyes"
	then
		if test "x$ac_disable_modules" = "xno"
		then
			if test "x$ac_kernel_headers" = "xno"
			then
				ac_ecs="${ac_ecs} (Linux Headers not found)"
			fi

			if test "x$ac_system_map" = "xno"
			then
				ac_ecs="${ac_ecs} (System.map not present)"
			fi
		else
			ac_ecs="${ac_ecs} (Requires Linux Kernel Modules)"
		fi
	else
		ac_ecs="${ac_ecs} (requires linux)"
	fi
fi


# Compose message to tell whether modules are disabled
ac_modules="Enabled"
if test "x$ac_disable_modules" = "xyes"
then
	ac_modules="Disabled"
	if test "x$ac_on_linux" = "xyes"
	then
		ac_modules="${ac_modules} (enable with --enable-modules)"
	else
		ac_modules="${ac_modules} on a non-Linux system"
	fi
fi


cat<<EOF


   * ROOT-Sim Configuration on Current Machine *

CPU Architecture.......... : ${current_cpu}
Operating System.......... : ${host_os}
Debugging Support......... : ${enable_debug}
MPI....................... : ${enable_mpi}
User-Level Threads........ : ${ac_enable_ult}
Parallel Allocator........ : ${ac_parallel_allocator}
NUMA Subsystem............ : ${numa_subsystem}
LP Preemption Support..... : ${ac_preemption}
LP Rebinding.............. : ${ac_lp_rebinding}
Linux Kernel Modules...... : ${ac_modules}
Event Cross State......... : ${ac_ecs}

EOF
