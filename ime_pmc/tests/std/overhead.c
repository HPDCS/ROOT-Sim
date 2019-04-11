#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <time.h>

#include <fcntl.h>

#include "hop-ioctl.h"

#define LENGTH	4096
#define CYCLES	2048

void swap(int *array, int i, int j)
{
	int t;
	t = array[i];
	array[i] = array[j];
	array[j] = t;
}// swap

void shuffle(int *array)
{
	int i, r, s; 
	for (i = 0, r = rand(), s = rand(); i < CYCLES; ++i, r = rand(), s = rand()) {
		swap(array, r % LENGTH, s % LENGTH);
	}
}// shuffle

void sort(int *array)
{
	int i;
	int ck = 1;

	while(ck)
		for (ck = 0, i = 0; i < LENGTH - 1; ++i)
			if (array[i] > array[i + 1]) {
				swap(array, i, i + 1);
				ck = 1;
			}
}// sort

const char *device = "/dev/hop/ctl";

int main(int argc, char const *argv[])
{
	int err = 0;

	if (argc < 3) {
		printf("Usage: overhead #test_times #profiler_active\n");
		goto end;
	}

	int number = atoi(argv[1]);
	int active = atoi(argv[2]);
	int fd;
	if (!!active) {
		fd = open(device, 0666);

		if (fd < 0) {
			printf("Error, cannot open %s, exit\n", device);
			goto end;
		}
	}

	pid_t my_pid = getpid();

	printf("Profiling(%u) PID: %u [%u times]\n", !!active, my_pid, number);

	if (!!active) {
		if ((err = ioctl(fd, HOP_ADD_TID, &my_pid))) {
			printf("ERROR HOP_ADD_TID %u, exit\n", my_pid);
			goto close;
		}

		if ((err = ioctl(fd, HOP_PROFILER_ON))) {
			printf("ERROR HOP_PROFILER_ON, exit\n");
			goto close;		
		}
	}

	int array [LENGTH];
	int i;
	srand(time(0));

	for (i = 0; i < LENGTH; ++i)
		array[i] = i;

	i = number;
	do {
		shuffle(array);
		sort(array);
	} while (i--);

	if (!!active) {
		if ((err = ioctl(fd, HOP_PROFILER_OFF))) {
			printf("ERROR HOP_PROFILER_OFF, prey!\n");
			goto close;		
		}

		/* start reading statistics*/
		char my_dev[64];
		sprintf(my_dev, "%s/%u", "/dev/hop", my_pid);
		
		int mfd = open(my_dev, 0666);
		char my_buf[1024];
		if (!mfd) {
			printf("Cannot open %s\n", my_dev);
//			goto del;
		}

		if (!read(mfd, my_buf, 1024))
			printf("Cannot read from %s\n",my_dev);
		else
			printf("DATA: %s\n",my_buf);
		close(mfd);

//del:
//		if ((err = ioctl(fd, HOP_DEL_TID, &my_pid))) {
//			printf("ERROR HOP_DEL_TID %u, exit\n", my_pid);
//			goto close;
//		}
	}

	printf("PID: %u profiled (%u), done\n", my_pid, !!active);

close:
	if (!!active) close(fd);
end:
	return err;
}// main
