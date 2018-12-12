CFLAGS_PRE=-coverage -I ./src/
CFLAGS_POST=-L . -lpthread -lm -std=gnu89

.PHONY: dymelor numerical

dymelor:
	$(CC) -D_GNU_SOURCE -DOS_LINUX $(CFLAGS_PRE) ./src/arch/x86.o ./tests/dymelor.c -o dymelor -ldymelor ./tests/common.c $(CFLAGS_POST)

numerical:
	$(CC) -DOS_LINUX $(CFLAGS_PRE) ./tests/numerical.c ./src/arch/x86.o ./src/lib/numerical.o ./tests/common.c -o numerical $(CFLAGS_POST)
