ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -I$(top_srcdir)/src/simulator -I$(top_srcdir)/src/simulator/subsystems
AM_MAKEFLAGS = -s

librootsim_include_HEADERS = ROOT-Sim.h
librootsim_includedir = $(includedir)
lib_LIBRARIES = librootsim.a libwrapperl.a libdymelor.a

librootsim_a_SOURCES =	main.c \
			lib/numerical.c \
			lib/parseparam.c \
			lib/topology.c \
			subsystems/arch/thread.c \
			subsystems/arch/ult.c \
			subsystems/arch/x86.c \
			subsystems/datatypes/array.c \
			subsystems/datatypes/list.c \
			subsystems/datatypes/calqueue.c \
			subsystems/mm/state.c \
			subsystems/queues/queues.c \
			subsystems/core/init.c \
			subsystems/core/core.c \
			subsystems/scheduler/stf.c \
			subsystems/scheduler/scheduler.c \
			subsystems/scheduler/control.c \
			subsystems/serial/serial.c \
			subsystems/statistics/statistics.c \
			subsystems/gvt/gvt.c \
			subsystems/gvt/gvt-nostro.c \
			subsystems/gvt/gvt-fujimoto.c \
			subsystems/gvt/fossil.c \
			subsystems/gvt/ccgs.c \
			subsystems/communication/window.c \
			subsystems/communication/communication.c

libwrapperl_a_SOURCES = subsystems/lib-wrapper/wrapper.c

libdymelor_a_SOURCES = 	subsystems/mm/checkpoints.c \
			subsystems/mm/dymelor.c \
			subsystems/mm/lp-alloc.c \
			subsystems/mm/malloc.c

