CC = rootsim-cc
CFLAGS = -Wall -Wextra
TARGET = tuberculosis_alt

SRCS = application.c guy.c guy_init.c parameters.c
OBJS = $(SRCS:.c=.o)

.PHONY: clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o $(TARGET)
