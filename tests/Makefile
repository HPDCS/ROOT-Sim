CFLAGS=-g -Wall -Wextra -I ../src/ -L ../src/ -lpthread -lm

.PHONY: dymelor

dymelor:
	gcc -D_GNU_SOURCE -DOS_LINUX ../src/arch/x86.c dymelor.c -o dymelor $(CFLAGS) -std=gnu89 -ldymelor
