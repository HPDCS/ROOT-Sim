CFLAGS=-g -Wall -Wextra -I ../src/ -L ../src/ -lpthread -lm

all:
	gcc dymelor.c -DOS_LINUX ../src/arch/x86.c -o dymelor $(CFLAGS) -std=gnu89 -ldymelor
