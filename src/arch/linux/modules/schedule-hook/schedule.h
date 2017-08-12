/**                       Copyright (C) 2014 HPDCS Group
*                       http://www.dis.uniroma1.it/~hpdcs
*
* This is free software;
* You can redistribute it and/or modify this file under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
*
* This file is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* this file; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @file timestretch.h
* @brief This is the header file to configure the timestretch module
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @date November 11, 2014
*/
#pragma once
#ifndef __KERNEL_TIME_SLICE_STRETCH
#define __KERNEL_TIME_SLICE_STRETCH

#include <linux/ioctl.h>

#define TSTRETCH_IOCTL_MAGIC 'T'

#define UNLIKELY_FLAG 0xf0f0f0f0
#define RESET_FLAG    0x00000000

#define MAX_STRETCH 3000 //this is expressed in milliseconds

#define DELAY 1000
#define ENABLE if(1)
//#define MAX_CPUs 32
#define SIBLING_PGD 128 // max number of concurrent memory views (concurrent root-sim worker threads on a node)
#define TS_THREADS (SIBLING_PGD) //max number of concurrent HTM threads with time stretch




#define HTM_THREADS 16 // max number of concurrent threads rnning HTM transactions with timer stretch

#define IOCTL_SETUP_PID _IOW(TSTRETCH_IOCTL_MAGIC, 2, unsigned long )
#define IOCTL_SHUTDOWN_PID _IOW(TSTRETCH_IOCTL_MAGIC, 3, unsigned long )
#define IOCTL_SHUTDOWN_ACK _IOW(TSTRETCH_IOCTL_MAGIC, 4, unsigned long )
#define IOCTL_SHUTDOWN_VIEWS _IOW(TSTRETCH_IOCTL_MAGIC, 5, unsigned long )

#endif /* __KERNEL_TIME_SLICE_STRETCH */
