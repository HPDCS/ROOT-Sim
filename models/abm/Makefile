# Makefile for the generic ROOT-Sim ABM model

CC=rootsim-cc
CFLAGS=-g -Wall -Wextra
DEPS = agent.h application.h config.h jsmn.h jsmn_helper.h logger.h user.h
OBJS = agent.o application.o config.o jsmn.o jsmn_helper.o logger.o user.o
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
