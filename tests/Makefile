CFLAGS_PRE=-coverage -g -I ./src/
CFLAGS_POST=-L . -lpthread -lm -std=gnu99

.PHONY: dymelor numerical capabilities

dymelor:
	$(CC) -D_GNU_SOURCE -DOS_LINUX $(CFLAGS_PRE) ./src/arch/x86.o ./tests/dymelor.c ./src/mm/dymelor.c ./src/mm/platform.c ./src/mm/segment.c ./src/mm/slab.c ./src/mm/buddy.c ./src/mm/checkpoints.c -o dymelor ./tests/common.c $(CFLAGS_POST)

numerical:
	$(CC) -D_GNU_SOURCE -DOS_LINUX $(CFLAGS_PRE) ./src/arch/x86.o ./src/lib/numerical.c ./tests/numerical.c -o numerical ./tests/common.c $(CFLAGS_POST)

capabilities:
	rootsim-cc ./tests/capabilities.c -o capabilities
