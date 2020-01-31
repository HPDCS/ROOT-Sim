CFLAGS_PRE=-coverage -g -I ./src/
CFLAGS_POST=-L . -lpthread -lm -std=gnu89

.PHONY: dymelor numerical capabilities

dymelor:
	$(CC) -D_GNU_SOURCE -DOS_LINUX $(CFLAGS_PRE) ./src/arch/x86.o ./tests/dymelor.c ./src/mm/dymelor.c ./src/mm/platform.c ./src/mm/segment.c ./src/mm/slab.c ./src/mm/buddy.c ./src/mm/checkpoints.c -o dymelor ./tests/common.c $(CFLAGS_POST)

numerical:
	$(CC) -DOS_LINUX $(CFLAGS_PRE) ./tests/numerical.c ./src/arch/x86.o ./src/lib/numerical.o ./tests/common.c -o numerical $(CFLAGS_POST)

capabilities:
	rootsim-cc ./tests/capabilities.c -o capabilities
