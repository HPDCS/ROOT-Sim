ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -I$(top_srcdir)/src 
AM_MAKEFLAGS = -s
AUTOMAKE_OPTIONS = subdir-objects

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
			datatypes/array.c \
			datatypes/list.c \
			datatypes/calqueue.c \
			mm/state.c \
			queues/queues.c \
			queues/xxhash.c \
			core/init.c \
			core/core.c \
			scheduler/stf.c \
			scheduler/scheduler.c \
			scheduler/control.c \
			serial/serial.c \
			statistics/statistics.c \
			gvt/gvt.c \
			gvt/fossil.c \
			gvt/cancelback.c \
			gvt/ccgs.c \
			communication/window.c \
			communication/communication.c

libwrapperl_a_SOURCES = lib-wrapper/wrapper.c

libdymelor_a_SOURCES = 	mm/checkpoints.c \
			mm/dymelor.c \
			mm/lp-alloc.c \
			mm/malloc.c
