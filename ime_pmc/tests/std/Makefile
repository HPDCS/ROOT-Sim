CC=gcc 
CFLAGS=-I../../include

all: 		job.o overhead.o synthetic.o
		$(CC) -o job job.o
		$(CC) -o overhead overhead.o
		$(CC) -o synthetic synthetic.o -lpthread
clean:
	rm -f job job.o overhead overhead.o synthetic synthetic.o
