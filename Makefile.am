ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -I $(top_srcdir)/src
AM_CCASFLAGS = -I $(top_srcdir)/src
AM_MAKEFLAGS = -s
AUTOMAKE_OPTIONS = subdir-objects
SUBDIRS = scripts man

# The following rules tell whether we have to compile kernel modules or not
if NO_KERNEL_MODULES
else
SUBDIRS += src/arch/x86/linux
endif
DIST_SUBDIRS = src/arch/x86/linux

# We need to generate some constants at compile time which
# are used by assembly code. The following lines do exactly that.
# substr() in the awk command is used to remove the leading '$'
# which is used by the assembler to mark a constant. This is important
# because to use a constant, we then write $sizeof_struct, indicating
# that it is an actual constant.
BUILT_SOURCES = src/arch/asm_defines.h
CLEANFILES = src/arch/asm_defines.h
src/arch/asm_defines.h: src/arch/asm_defines.c
	echo "Generating defines for assembly modules..."
	@$(COMPILE) -DASM_DEFINES -S $< -o - | $(AWK) '($$1 == "->") { print "#define " $$2 " " substr($$3, 2) }' > $(top_srcdir)/$@


# From here on, there are main ROOT-Sim build rules
librootsim_include_HEADERS = src/ROOT-Sim.h
librootsim_includedir = $(includedir)
lib_LIBRARIES = librootsim.a libwrapperl.a libdymelor.a

librootsim_a_SOURCES =	src/main.c \
			src/arch/memusage.c \
			src/arch/thread.c \
			src/arch/ult.c \
			src/arch/x86.c \
			src/arch/x86/disassemble.c \
			src/arch/x86/jmp.S \
			src/arch/x86/ecs_callback.S \
			src/arch/x86/preempt_callback.S \
			src/communication/wnd.c \
			src/communication/communication.c \
			src/communication/gvt.c \
			src/communication/mpi.c \
			src/core/init.c \
			src/core/core.c \
			src/datatypes/calqueue.c \
			src/datatypes/hash_map.c \
			src/datatypes/msgchannel.c \
			src/gvt/gvt.c \
			src/gvt/fossil.c \
			src/gvt/ccgs.c \
			src/lib/topology/topology.c \
			src/lib/topology/topology_costs.c \
			src/lib/topology/topology_obstacles.c \
			src/lib/topology/topology_probabilities.c \
			src/lib/numerical.c \
			src/lib/abm_layer.c \
			src/lib/jsmn_helper.c \
			src/lib/jsmn.c \
			src/mm/state.c \
			src/mm/ecs.c \
			src/queues/queues.c \
			src/queues/xxhash.c \
			src/scheduler/binding.c \
			src/scheduler/control.c \
			src/scheduler/preempt.c \
			src/scheduler/process.c \
			src/scheduler/stf.c \
			src/scheduler/scheduler.c \
			src/serial/serial.c \
			src/statistics/statistics.c

libwrapperl_a_SOURCES = src/lib-wrapper/wrapper.c

libdymelor_la_LDFLAGS = -mgeneral-regs-only
libdymelor_a_SOURCES = 	src/mm/checkpoints.c \
			src/mm/platform.c \
			src/mm/dymelor.c \
			src/mm/buddy.c \
			src/mm/segment.c \
			src/mm/slab.c


# This is the GCC plugin to trace at runtime memory accesses
lib_LTLIBRARIES = libmemtrace.la

libmemtrace_la_includedir = $(includedir)
libmemtrace_la_CXXFLAGS = -std=c++11 -Wall -fno-rtti -Wno-literal-suffix -I$(GCCPLUGINDIR)/include
libmemtrace_la_LDFLAGS = -version-info $(GCCPLUGVERS)
libmemtrace_la_SOURCES = src/plugin/memtrace.cc

