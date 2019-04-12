CC = rootsim-cc
CFLAGS = -Wall -Wextra
TARGET = sensors

SRCS = application.c  forwarding_engine.c  link_estimator.c  link_layer.c  physical_layer.c  routing_engine.c parameters.c
OBJS = $(SRCS:.c=.o)

.PHONY: clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o $(TARGET)
