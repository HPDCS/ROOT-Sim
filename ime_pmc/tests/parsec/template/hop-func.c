#include "hop_func.h"

int request_pt(struct hop_requester *req)
{
	if (!req) return -EINVAL;

	req->active = 0;
	req->tid = syscall(__NR_gettid);

	printf("Start profiling ThreadID: %u\n" req->tid);

        req->fd = open(HOP_CTL_DEVICE, HOP_DEV_O_MODE);

        if (req->fd < 0) {
                printf("ERROR, cannot open %s, exit\n", HOP_CTL_DEVICE);
                return -ENOENT;
        }

        if (ioctl(req->fd, HOP_ADD_TID, &req->tid)) {
                printf("ERROR, cannot add TID %u for profiling, exit\n", req->tid);
                return -EINTR;
        }

        req->active = 1;
        return 0;
}// request_pt

void print_stats_pt(struct hop_requester *req)
{
	if (!req) {
		printf("WARNING[print_stats_pt], got a null argument, exit\n");
		return;
	}

	if (!req->active) {
		printf("WARNING[print_stats_pt], profiler not active, exit\n");
		return;		
	}

	/* start reading statistics*/
        char my_dev[64];
        sprintf(my_dev, "%s/%u", "/dev/hop", my_pid);

        int mfd = open(my_dev, HOP_DEV_O_MODE);
        char my_buf[1024];
        if (!mfd) {
                printf("WARNING[print_stats_pt], Cannot open %s, exit\n", my_dev);
                return;
        }
        if (!read(mfd, my_buf, 1024))
                printf("WARNING[print_stats_pt], Cannot read from %s, exit\n", my_dev);
        else
                printf("DATA: %s\n", my_buf);
        close(mfd);
}// print_stats_pt

void free_pt(struct hop_requester *req)
{
	if (!req) {
		printf("WARNING[free_pt], got a null argument, exit\n");
		return;
	}

	if (!req->active) {
		printf("WARNING[free_pt], profiler not active, exit\n");
		return;		
	}

	if (ioctl(req->fd, HOP_DEL_TID, &req->tid)) {
		printf("ERROR, cannot remove TID %u, try manually\n", req->tid);
	}

        if (req->fd > 0) close(req->fd);

	printf("Stop profiling ThreadID: %u\n" req->tid);
}// free_pt                                                                                                                                            210,1         Bot
