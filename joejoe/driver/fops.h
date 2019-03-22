/**
 * This file contains the file operations used
 * inside the chrdevs.
 * Actually only read, release and ioctl are ops of interest.
 */
#ifndef _ISO_FOPS_H
#define _ISO_FOPS_H

#include <linux/fs.h>
#include <asm/uaccess.h>

// FUNCTION PROTOTYPES


/**
 * iso_open - open a profiled thread device node instance
 */
int iso_open(struct inode *inode, struct file *file);

/**
 * iso_release - release a profiled thread device node instance
 */
int iso_release(struct inode *inode, struct file *file);

/**
 * iso_read - read HOP samples from the device
 * @count:  number of bytes to read; value must be (1) at least the size of
 *      one entry and (2) no more than the total buffer size
 *
 * This function reads as many HOP samples as possible
 *
 * Returns: Number of bytes read, or negative error code
 */
// ssize_t _read(struct file *filp, char __user *buf, size_t count,
//             loff_t *fpos);

/**
 * iso_ioctl() - perform an ioctl command
 *
 * This function is actually used to read a profiled thread statistics
 */
long iso_ioctl(struct file *filep, unsigned int ioctl, unsigned long arg);


#endif /* _ISO_FOPS_H */