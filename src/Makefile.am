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
			scheduler/stf.c \
			scheduler/scheduler.c \
			scheduler/group.c \
			serial/serial.c \
			statistics/statistics.c \
			gvt/gvt.c \
			gvt/fossil.c \
			gvt/ccgs.c \
			communication/window.c \
			communication/communication.c

libwrapperl_a_SOURCES = lib-wrapper/wrapper.c

libdymelor_a_SOURCES = 	mm/checkpoints.c \
			mm/recoverable.c \
			mm/unrecoverable.c \
			mm/platform.c \
			mm/dymelor.c \
			mm/allocator.c \
			mm/allocator_ecs.c \
			mm/bh-manager.c \
			mm/mapmove.c
