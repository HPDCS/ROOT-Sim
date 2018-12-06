CFLAGS_PRE=--coverage -O3 -Wall -Wextra -I ../src/
CFLAGS_POST=-L ../src/ -lpthread -lm

.PHONY: dymelor

dymelor:
	$(CC) -D_GNU_SOURCE -DOS_LINUX $(CFLAGS_PRE) ../src/arch/x86.c dymelor.c -o dymelor -std=gnu89 -ldymelor $(CFLAGS_POST) 
