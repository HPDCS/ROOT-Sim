#ifndef CORE_STRUCTS_H
#define CORE_STRUCTS_H

#include <linux/cdev.h>
#include <linux/hashtable.h>

// #define register_set(val, len, off)	((val & len) << off)
// #define register_get(reg, msk, off)	((val & msk) >> off)

enum core_states {
	CORE_ON 	= 0,
	CORE_OFF	= 1,
	CORE_MAX_STATES,
};

/* minor used to make a chdev, can be reused */
struct minor {
	int min;
	struct list_head node;
};

// mepo device
struct core_dev {
	u8 state; /* BITS_TO_LONGS(IBS_MAX_STATES) */
	u64 counter;
};

#endif /* CORE_STRUCTS_H */