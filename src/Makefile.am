ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -I$(top_srcdir)/src 
AM_MAKEFLAGS = -s
AUTOMAKE_OPTIONS = subdir-objects

# The following rules tell whether we have to compile kernel modules or not
if NO_KERNEL_MODULES
SUBDIRS = 
else
SUBDIRS = arch/x86/linux
endif
DIST_SUBDIRS = arch/x86/linux

# We need to generate some constants at compile time which
# are used by assembly code. The following lines do exactly that.
# substr() in the awk command is used to remove the leading '$'
# which is used by the assembler to mark a constant. This is important
# because to use a constant, we then write $sizeof_struct, indicating
# that it is an actual constant.
BUILT_SOURCES = src/arch/asm_defines.h
CLEANFILES = src/arch/asm_defines.h
src/arch/asm_defines.h: arch/asm_defines.c
	echo "Generating defines for assembly modules..."
	$(COMPILE) -S $< -o - | $(AWK) '($$1 == "->") { print "#define " $$2 " " substr($$3, 2) }' > $(top_srcdir)/$@ 


# From here on, there are main ROOT-Sim build rules
librootsim_include_HEADERS = ROOT-Sim.h
librootsim_includedir = $(includedir)
lib_LIBRARIES = librootsim.a libwrapperl.a libdymelor.a

librootsim_a_SOURCES =	main.c \
			arch/memusage.c \
			arch/thread.c \
			arch/ult.c \
			arch/x86.c \
			arch/x86/disassemble.c \
			arch/x86/jmp.S \
			arch/x86/ecs_callback.S \
			arch/x86/preempt_callback.S \
			communication/wnd.c \
			communication/communication.c \
			communication/gvt.c \
			communication/mpi.c \
			core/init.c \
			core/core.c \
			datatypes/array.c \
			datatypes/calqueue.c \
			datatypes/msgchannel.c \
			gvt/gvt.c \
			gvt/fossil.c \
			gvt/ccgs.c \
			lib/numerical.c \
			lib/topology.c \
			mm/state.c \
			mm/ecs.c \
			queues/queues.c \
			queues/xxhash.c \
			scheduler/binding.c \
			scheduler/control.c \
			scheduler/preempt.c \
			scheduler/process.c \
			scheduler/stf.c \
			scheduler/scheduler.c \
			serial/serial.c \
			statistics/statistics.c

libwrapperl_a_SOURCES = lib-wrapper/wrapper.c

libdymelor_a_SOURCES = 	mm/checkpoints.c \
			mm/platform.c \
			mm/dymelor.c \
			mm/buddy.c \
			mm/segment.c \
			mm/slab.c
