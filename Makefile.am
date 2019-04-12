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

EXTRA_DIST =	docs/doxy.conf.in \
		docs/Makefile.in \
		man/Makefile.in \
		src/arch/x86/linux/Makefile \
		scripts/Makefile.in \
		scripts/ld-data1 \
		scripts/ld-data2 \
		scripts/ld-final \
		scripts/rootsim-cc.in \
		src/arch/asm_defines.c

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
			src/lib/topology/costs.c \
			src/lib/topology/obstacles.c \
			src/lib/topology/probabilities.c \
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
			src/statistics/statistics.c \
			src/queues/queues.h \
			src/queues/xxhash.h \
			src/lib/numerical.h \
			src/lib/jsmn_helper.h \
			src/lib/jsmn.h \
			src/lib/abm_layer.h \
			src/lib/topology.h \
			src/ROOT-Sim.h \
			src/mm/dymelor.h \
			src/mm/ecs.h \
			src/mm/state.h \
			src/mm/mm.h \
			src/communication/wnd.h \
			src/communication/gvt.h \
			src/communication/mpi.h \
			src/communication/communication.h \
			src/gvt/ccgs.h \
			src/gvt/gvt.h \
			src/serial/serial.h \
			src/statistics/statistics.h \
			src/datatypes/bitmap.h \
			src/datatypes/array.h \
			src/datatypes/list.h \
			src/datatypes/msgchannel.h \
			src/datatypes/hash_map.h \
			src/datatypes/calqueue.h \
			src/datatypes/heap.h \
			src/arch/thread.h \
			src/arch/ult.h \
			src/arch/memusage.h \
			src/arch/asm_defines.h \
			src/arch/x86/linux/cross_state_manager/cross_state_manager.h \
			src/arch/x86/linux/schedule-hook/ld.h \
			src/arch/x86/linux/schedule-hook/schedule.h \
			src/arch/x86/linux/schedule-hook/lend.h \
			src/arch/x86/linux/intercept/traps-dummy.h \
			src/arch/x86/linux/intercept/my_page_fault.h \
			src/arch/x86/jmp.h \
			src/arch/x86/disassemble.h \
			src/arch/atomic.h \
			src/arch/jmp.h \
			src/arch/x86.h \
			src/core/timer.h \
			src/core/core.h \
			src/core/init.h \
			src/scheduler/binding.h \
			src/scheduler/process.h \
			src/scheduler/scheduler.h \
			src/scheduler/stf.h


libwrapperl_a_SOURCES = src/lib-wrapper/wrapper.c

libdymelor_a_SOURCES = 	src/mm/checkpoints.c \
			src/mm/platform.c \
			src/mm/dymelor.c \
			src/mm/buddy.c \
			src/mm/segment.c \
			src/mm/slab.c
