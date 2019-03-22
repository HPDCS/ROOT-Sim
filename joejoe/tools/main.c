#include <stdlib.h>     /* exit func */
#include <stdio.h>      /* printf func */
#include <fcntl.h>      /* open syscall */
#include <getopt.h>     /* args utility */
#include <sys/ioctl.h>  /* ioctl syscall*/
#include <unistd.h>	/* close syscall */

#include "iso_ioctl.h"

#define ISO_DEVICE "/dev/isoctl"
#define ISO_DEV_O_MODE 0666

#define ARGS "nfm:s:b:p:t:x:"

int main(int argc, char **argv)
{
	/* nothing to do */
	if (argc < 2) goto end;

	int fd, fdt, option, err, val, idx;

	unsigned long long arg;
	
	char path[128];

	if ((option = getopt(argc, argv, ARGS)) != -1) {
		fd = open(ISO_DEVICE, ISO_DEV_O_MODE);
		if (fd < 0) goto failure;
			err = 0;
	}

	while(!err && option != -1) {
		switch(option) {
		case 'n':
			err = ioctl(fd, ISO_GLOBAL_ON);
			break;
		case 'f':
			err = ioctl(fd, ISO_GLOBAL_OFF);
			break;
		case 'm':
			arg = strtoull(optarg, (char **)NULL, 16);
			printf("Args: %llu\n", arg);
			/* if invalid integer the ioctl should fail */
			err = ioctl(fd, ISO_CPU_MASK_SET, arg);
			break;
		case 'p':
			arg = strtoull(optarg, (char **)NULL, 10);
			printf("Args: %llu\n", arg);
			/* if invalid integer the ioctl should fail */
			err = ioctl(fd, ISO_ADD_TID, arg);
			break;
		}
		option = getopt(argc, argv, ARGS);
	}

	close(fd);
end:
	exit(EXIT_SUCCESS);
failure:
	exit(EXIT_FAILURE);
}// main
