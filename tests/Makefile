CFLAGS_PRE=-coverage -g -I ./src/
CFLAGS_POST=-L ./src/ -lpthread -lm

.PHONY: dymelor numerical

dymelor:
	$(CC) -D_GNU_SOURCE -DOS_LINUX $(CFLAGS_PRE) ./src/arch/x86.c ./tests/dymelor.c ./tests/common.c -o dymelor -std=gnu89 -ldymelor $(CFLAGS_POST)

numerical:
	$(CC) -DOS_LINUX $(CFLAGS_PRE) ./tests/numerical.c ./tests/common.c ./src/arch/x86.c ./src/lib/numerical.c -o numerical $(CFLAGS_POST) 2> /dev/null
