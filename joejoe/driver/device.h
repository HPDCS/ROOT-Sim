#ifndef _ISO_DEVICE_H
#define _ISO_DEVICE_H

#define ISO_DEVICE_NAME	"isoctl"
#define ISO_MAX_MINOR	1

extern int setup_metadata(void);

extern int setup_devices(void);

extern void cleanup_devices(void);

#endif