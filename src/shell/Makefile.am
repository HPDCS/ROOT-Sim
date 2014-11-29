CC = gcc
ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -I$(top_srcdir)/src/simulator -I$(top_srcdir)/src/simulator/subsystems
BUILT_SOURCES = yy_bison.h
AM_YFLAGS = -d -p yy
AM_LFLAGS = -o$(LEX_OUTPUT_ROOT).c
AM_MAKEFLAGS = --no-print-directory

CLEANFILES = yy_bison.c yy_bison.h yy_flex.c

bin_PROGRAMS = rootsim
rootsim_SOURCES = execute.c yy_flex.l yy_bison.y shell.c
