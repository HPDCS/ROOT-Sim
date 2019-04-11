CC=gcc
CFLAGS_PRE=-Wall -O3 -I../../include
CFLAGS_POST=-lpthread

hot_pages:
	$(CC) $(CFLAGS_PRE) main.c dict.c $(CFLAGS_POST) -o hot_page 


clean:
	rm hot_page
