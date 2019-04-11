#include <stdlib.h>     /* exit func */
#include <stdio.h>      /* printf func */
#include <fcntl.h>      /* open syscall */
#include <getopt.h>     /* args utility */
#include <sys/ioctl.h>  /* ioctl syscall*/
#include <unistd.h>	/* close syscall */
#include "hop-ioctl.h"

#define HOP_PATH "/dev/hop"
#define HOP_CTL_DEVICE "/dev/hop/ctl"
#define HOP_DEV_O_MODE 0666

#define ARGS "cnfs:b:pt:"

int main(int argc, char **argv)
{

	/* nothing to do */
	if (argc < 2) goto end;

	int fd, fdt, option, err, val;
	char path[128];
	struct ctl_stats cstats;
	struct tid_stats tstats;

	if ((option = getopt(argc, argv, ARGS)) != -1) {
        	fd = open(HOP_CTL_DEVICE, HOP_DEV_O_MODE);
        	if (fd < 0) goto failure;
		err = 0;
	}

        while(!err && option != -1) {
        	switch(option) {
                case 'c':
                        err = ioctl(fd, HOP_CLEAN_TIDS);
                        break;
        	case 'n':
        		err = ioctl(fd, HOP_PROFILER_ON);
        		break;
        	case 'f':
        		err = ioctl(fd, HOP_PROFILER_OFF);
        		break;
        	case 's':
        		/* if invalid integer the ioctl will fail */
                        val = atoi(optarg);
        		err = ioctl(fd, HOP_SET_SAMPLING, &val);
        		break;
        	case 'b':
        		/* if invalid integer the ioctl will fail */
                        val = atoi(optarg);
        		err = ioctl(fd, HOP_SET_BUF_SIZE, &val);
        		break;
                case 'p':
			/* if invalid integer the ioctl will fail */
			err = ioctl(fd, HOP_CTL_STATS, &cstats);
			if (!err) {
				printf("%u %lu %lu %lu %lu\n", 
					0, cstats.no_ibs, cstats.spurious,
					cstats.requests, cstats.denied);
			}
			break;
                case 't':
                	sprintf(path, "%s/%u", HOP_PATH, atoi(optarg));
	                fdt = open(path, HOP_DEV_O_MODE);
	        	if (fdt < 0) goto failure;

			/* if invalid integer the ioctl will fail */
			err = ioctl(fdt, HOP_TID_STATS, &tstats);
			if (!err) {
				printf("%u %lu %lu %lu %lu\n",
					tstats.tid, tstats.busy, tstats.kernel,
					tstats.memory, tstats.samples);
			}
			close(fdt);
			break;
		default:
			goto end;
	       	}
	       	/* get next arg */
	       	option = getopt(argc, argv, ARGS);
        }

        close(fd);
end:
        exit(EXIT_SUCCESS);
failure:
	exit(EXIT_FAILURE);
}// main
