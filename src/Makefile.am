ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -I$(top_srcdir)/src 
AM_MAKEFLAGS = -s
AUTOMAKE_OPTIONS = subdir-objects

# The following rules tell whether we have to compile kernel modules or not
if NO_KERNEL_MODULES
SUBDIRS = 
else
SUBDIRS = arch/linux/modules
endif
DIST_SUBDIRS = arch/linux/modules

# We need to generate some constants at compile time which
# are used by assembly code. The following lines do exactly that.
# substr() in the awk command is used to remove the leading '$'
# which is used by the assembler to mark a constant. This is important
# because to use a constant, we then write $sizeof_struct, indicating
# that it is an actual constant.
BUILT_SOURCES = src/arch/asm_defines.h
CLEANFILES = src/arch/asm_defines.h
src/arch/asm_defines.h: arch/asm_defines.c scheduler/process.h
	echo "Generating defines for assembly modules..."
	$(COMPILE) -S $< -o - | $(AWK) '($$1 == "->") { print "#define " $$2 " " substr($$3, 2) }' > $(top_srcdir)/$@ 


# From here on, there are main ROOT-Sim build rules
librootsim_include_HEADERS = ROOT-Sim.h
librootsim_includedir = $(includedir)
lib_LIBRARIES = librootsim.a libwrapperl.a libdymelor.a

librootsim_a_SOURCES =	main.c \
			lib/numerical.c \
			lib/topology.c \
			arch/memusage.c \
			arch/thread.c \
			arch/ult.c \
			arch/x86.c \
			arch/linux/jmp.S \
			datatypes/array.c \
			datatypes/calqueue.c \
			datatypes/msgchannel.c \
			datatypes/slab.c \
			datatypes/treiber.c \
			mm/state.c \
			mm/ecs.c \
			mm/ecs_callback.S \
			queues/queues.c \
			queues/xxhash.c \
			core/init.c \
			core/core.c \
			scheduler/binding.c \
			scheduler/control.c \
			scheduler/preempt.c \
			scheduler/preempt_callback.S \
			scheduler/process.c \
			scheduler/stf.c \
			scheduler/scheduler.c \
			serial/serial.c \
			statistics/statistics.c \
			gvt/gvt.c \
			gvt/fossil.c \
			gvt/ccgs.c \
			communication/wnd.c\
			communication/communication.c\
			communication/gvt.c\
			communication/mpi.c\
			arch/parse-x86.c

libwrapperl_a_SOURCES = lib-wrapper/wrapper.c

libdymelor_a_SOURCES = 	mm/checkpoints.c \
			mm/recoverable.c \
			mm/unrecoverable.c \
			mm/platform.c \
			mm/dymelor.c \
			mm/buddy.c \
			mm/segment.c
