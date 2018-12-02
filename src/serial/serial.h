#pragma once

#include <ROOT-Sim.h>

extern void SerialSetState(void *);
extern void SerialScheduleNewEvent(unsigned int, simtime_t, unsigned int,
				   void *, unsigned int);

extern void serial_init(void);
extern void serial_simulation(void) __attribute__((noreturn));
