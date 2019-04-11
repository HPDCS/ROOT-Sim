CC=gcc
CFLAGS_PRE=-Wall -g -O3 -I../../main
CFLAGS_POST=-lpthread

hot_pages:
	$(CC) $(CFLAGS_PRE) main.c dict.c $(CFLAGS_POST) -o hot_page 


clean:
	rm hot_page
