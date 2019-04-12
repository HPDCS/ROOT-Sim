/**
 * @file serial/serial.c
 *
 * @brief Serial scheduler
 *
 * This module implements the sequential execution of simulation models.
 * Here all the routines to support sequential simulations are implemented,
 * except for the event queue which uses the Calendar Queue implemented in
 * calqueue.c.
 *
 * @copyright
 * Copyright (C) 2008-2019 HPDCS Group
 * https://hpdcs.github.io
 *
 * This file is part of ROOT-Sim (ROme OpTimistic Simulator).
 *
 * ROOT-Sim is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; only version 3 of the License applies.
 *
 * ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * @author Alessandro Pellegrini
 */

#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdbool.h>

#include <ROOT-Sim.h>
#include <serial/serial.h>
#include <scheduler/scheduler.h>
#include <core/core.h>
#include <core/init.h>
#include <core/timer.h>
#include <gvt/ccgs.h>
#include <mm/mm.h>
#include <datatypes/calqueue.h>
#include <statistics/statistics.h>

#ifdef EXTRA_CHECKS
#include <queues/xxhash.h>
#endif

static bool serial_simulation_complete = false;
static bool *serial_completed_simulation;

void SerialScheduleNewEvent(unsigned int rcv, simtime_t stamp,
			    unsigned int event_type, void *event_content,
			    unsigned int event_size)
{
	GID_t receiver;
	msg_t *event;

	// Sanity checks
	if (unlikely(stamp < lvt(current))) {
		rootsim_error(true, "LP %d is trying to send events in the past. Current time: %f, scheduled time: %f\n",
			      current->gid.to_int, lvt(current), stamp);
	}
	// Populate the message data structure
	set_gid(receiver, rcv);
	size_t size = sizeof(msg_t) + event_size;
	event = rsalloc(size);
	bzero(event, sizeof(msg_t));
	event->sender = current->gid;
	event->receiver = receiver;
	event->timestamp = stamp;
	event->send_time = lvt(current);
	event->type = event_type;
	event->size = event_size;
	memcpy(event->event_content, event_content, event_size);

	// Put the event in the Calenda Queue
	calqueue_put(stamp, event);
}

void serial_init(void)
{
	// Sanity check on the number of LPs
	if (unlikely(n_prc_tot == 0)) {
		rootsim_error(true, "You must specify the total number of Logical Processes\n");
	}
	// Initialize the calendar queue
	calqueue_init();

	// Initialize the per LP variables
	serial_completed_simulation = rsalloc(sizeof(bool) * n_prc_tot);
	bzero(serial_completed_simulation, sizeof(bool) * n_prc_tot);

	// Generate the INIT events for all the LPs
	foreach_lp(lp) {
		current = lp;
		SerialScheduleNewEvent(current->gid.to_int, 0.0, INIT, NULL, 0);
	}

	// No LP is scheduled now
	current = NULL;
}

void serial_simulation(void)
{
	timer serial_event_execution;
	timer serial_gvt_timer;
	msg_t *event;
	bool new_termination_decision;
	unsigned int completed = 0;

#ifdef EXTRA_CHECKS
	unsigned long long hash1, hash2;
	hash1 = hash2 = 0;
#endif

	timer_start(serial_gvt_timer);

	statistics_start();

	while (!serial_simulation_complete) {

		// Pick an event from the calendar queue and use the
		// receiver as the current lp
		event = (msg_t *) calqueue_get();
		if (unlikely(event == NULL)) {
			rootsim_error(true, "No events to process!\n");
		}

		current = find_lp_by_gid(event->receiver);
		current->bound = event;
		current_evt = event;

#ifdef EXTRA_CHECKS
		if (event->size > 0) {
			hash1 = XXH64(event->event_content, event->size, current);
		}
#endif

		timer_start(serial_event_execution);
		if(&abm_settings){
			ProcessEventABM();
		}else if (&topology_settings){
			ProcessEventTopology();
		}else{
			ProcessEvent_light(current->gid.to_int, event->timestamp,
					event->type, event->event_content,
					event->size, current->current_base_pointer);
		}

		statistics_post_data_serial(STAT_EVENT, 1.0);
		statistics_post_data_serial(STAT_EVENT_TIME, timer_value_seconds(serial_event_execution));

#ifdef EXTRA_CHECKS
		if (event->size > 0) {
			hash2 = XXH64(event->event_content, event->size, current);
		}

		if (hash1 != hash2) {
			printf("hash1 = %llu, hash2= %llu\n", hash1, hash2);
			rootsim_error(true, "Error, LP %d has modified the payload of event %d during its processing. Aborting...\n",
				      current->gid, event->type);
		}
#endif

		// Termination detection can happen only after the state is initialized
		if (likely(current->current_base_pointer != NULL)) {

			// We have just executed a new event at some LP. Depending on the type of requested termination detection,
			// we call or skip the termination detection for the current LP.
			if(rootsim_config.check_termination_mode == CKTRM_INCREMENTAL) {
				// In incremental termination detection we are dealing with stable termination
				// predicates. We can suppose that after that an LP decided to terminate the
				// simulation, it will never change its mind.
				if (!serial_completed_simulation[event->receiver.to_int] && current->OnGVT(event->receiver.to_int, current->current_base_pointer)) {
					completed++;
					serial_completed_simulation[event->receiver.to_int] = true;
					if (unlikely(completed == n_prc_tot)) {
						serial_simulation_complete = true;
					}
				}
			} else {
				// Normal and accurate termination detection policies are the same in sequential simulation.
				// We have to be sure that, at the current time, all the LPs are agreeing on termination.
				// We therefore keep track of past per-LP decisions and increment/decrement the termination counter depending
				// on changed decision.
				new_termination_decision = current->OnGVT(event->receiver.to_int, current->current_base_pointer);

				if(serial_completed_simulation[event->receiver.to_int] != new_termination_decision) {
					if(new_termination_decision) {
						// Changed from false to true
						completed++;
					} else {
						// Changed from true to false
						completed--;
					}
				}

				serial_completed_simulation[event->receiver.to_int] = new_termination_decision;

				if (unlikely(completed == n_prc_tot)) {
					serial_simulation_complete = true;
				}
			}
		}

		// Termination detection on reached LVT value
		if (rootsim_config.simulation_time > 0 && event->timestamp >= rootsim_config.simulation_time) {
			serial_simulation_complete = true;
		}

		// Print the time advancement periodically
		if (timer_value_milli(serial_gvt_timer) > (int)rootsim_config.gvt_time_period) {
			timer_restart(serial_gvt_timer);
			printf("TIME BARRIER: %f\n", lvt(current));
			statistics_on_gvt_serial(lvt(current));
		}

		current = NULL;

		rsfree(event);
	}

	simulation_shutdown(EXIT_SUCCESS);
}
