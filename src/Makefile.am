ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -I$(top_srcdir)/src 
AM_MAKEFLAGS = -s
AUTOMAKE_OPTIONS = subdir-objects

if NO_KERNEL_MODULES
SUBDIRS = 
else
SUBDIRS = arch/linux/modules
endif
DIST_SUBDIRS = arch/linux/modules

librootsim_include_HEADERS = ROOT-Sim.h
librootsim_includedir = $(includedir)
lib_LIBRARIES = librootsim.a libwrapperl.a libdymelor.a

librootsim_a_SOURCES =	main.c \
			lib/numerical.c \
			lib/parseparam.c \
			lib/topology.c \
			arch/memusage.c \
			arch/thread.c \
			arch/ult.c \
			arch/x86.c \
			arch/linux/jmp.S \
			datatypes/array.c \
			datatypes/list.c \
			datatypes/calqueue.c \
			datatypes/slab.c \
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
			mm/bh.c \
			mm/segment.c \
			mm/numa.c 
