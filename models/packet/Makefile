# This is a Makefile which allows to compile a simulation
# model using the HPC ROOT-Sim runtime.

CC = rootsim-cc
CFLAGS = -g -Wall -Wextra
TARGET = model

SRCS = application.c
OBJS = $(SRCS:.c=.o)

.PHONY: clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o $(TARGET)

