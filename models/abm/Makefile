# Makefile for the generic ROOT-Sim ABM model

CC=rootsim-cc
CFLAGS=-g -Wall -Wextra
DEPS = application.h jsmn.h
OBJS = application.c functions.c jsmn.c config.c
MODEL = abm

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: $(MODEL)

$(MODEL): $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(OBJS) $(MODEL)
