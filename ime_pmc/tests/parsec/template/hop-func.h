#ifndef _HOP_FUNC_H
#define _HOP_FUNC_H

#include "hop-ioctl.h"  /* used for hop utility */
#include <sys/ioctl.h>  /* ioctl utility */
#include <sys/types.h>
#include <sys/syscall.h>
#include <stdio.h>

/* used for open syscall */
#ifdef  __cplusplus
#include <fcntl.h>	/* compiled by g++ */
#else
#include <stdlib.h>	/* compiled by gcc*/
#endif

#define HOP_CTL_DEVICE "/dev/hop/ctl"
#define HOP_DEV_O_MODE 0666
#define HOP_DEV_PATH "/dev/hop"

struct hop_requester {
	int fd;
	int active;
	int tid;
};

int request_pt(struct hop_requester *req);

void print_stats_pt(struct hop_requester *req);

void free_pt(struct hop_requester *req);

#endif /* _HOP_FUNC_H */