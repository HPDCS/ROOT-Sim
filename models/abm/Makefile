# Makefile for the generic ROOT-Sim ABM model

CC=rootsim-cc
CFLAGS=-g -Wall -Wextra
DEPS = application.h jsmn.h
OBJS = application.o functions.o jsmn.o config.o
MODEL = abm

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: $(MODEL)

$(MODEL): $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)
	rm *.o.rs

.PHONY: clean

clean:
	rm -f $(OBJS) $(MODEL)
