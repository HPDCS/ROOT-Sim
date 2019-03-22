#include <stdlib.h>     /* exit func */
#include <stdio.h>      /* printf func */
#include <fcntl.h>      /* open syscall */
#include <getopt.h>     /* args utility */
#include <sys/ioctl.h>  /* ioctl syscall*/
#include <unistd.h>	/* close syscall */

#include "iso_ioctl.h"

#define ISO_DEVICE "/dev/isoctl"
#define ISO_DEV_O_MODE 0666

#define ARGS "x:"

int main(int argc, char **argv)
{
	/* nothing to do */
	//if (argc < 2) goto end;

	int fd, fdt, option, err, val, idx;
    //optarg is char*
	fd = open(ISO_DEVICE, ISO_DEV_O_MODE);

	if (fd < 0) {
		printf("Error, cannot open %s\n", ISO_DEVICE);
		return -1;
	}

	unsigned long long arg;
	char cmd[256];
	
	int pid = fork();
	if (pid == 0) {
		int i;
		ioctl(fd, ISO_ADD_TID, getpid());
		for(i = 0; i < 10; i++)printf("This is the child process. My pid is %d and my parent's id is %d.\n", getpid(), getppid());
		//char *args[]={argv[1], NULL};
		//execvp(args[0],args); 
		return 0;
	}
	else {
		printf("This is the parent process. My pid is %d and my parent's id is %d.\n", getpid(), pid);
	}

	// arg = strtoull(optarg, (char **)NULL, 16);

	close(fd);
end:
	exit(EXIT_SUCCESS);
failure:
	exit(EXIT_FAILURE);
}// main