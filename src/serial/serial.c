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
#include <mm/dymelor.h>
#include <datatypes/calqueue.h>
#include <statistics/statistics.h>

#ifdef EXTRA_CHECKS
#include <queues/xxhash.h>
#endif


static bool serial_simulation_complete = false;
static void **serial_states;
static bool *serial_completed_simulation;


void SerialSetState(void * state) {
	serial_states[lid_to_int(current_lp)] = state;
}

void SerialScheduleNewEvent(unsigned int rcv, simtime_t stamp, unsigned int event_type, void *event_content, unsigned int event_size) {
	GID_t receiver;
	msg_t *event;

	// Sanity checks
	if(unlikely(stamp < current_lvt)) {
		rootsim_error(true, "LP %d is trying to send events in the past. Current time: %f, scheduled time: %f\n", current_lp, current_lvt, stamp);
	}

	// Populate the message data structure
	set_gid(receiver, rcv);
	size_t size = sizeof(msg_t) + event_size;
	event = rsalloc(size);
	bzero(event, size);
	event->sender = LidToGid(current_lp);
	event->receiver = receiver;
	event->timestamp = stamp;
	event->send_time = current_lvt;
	event->type = event_type;
	event->size = event_size;
	memcpy(event->event_content, event_content, event_size);

	// Put the event in the Calenda Queue
	calqueue_put(stamp, event);
}

void serial_init(void) {
	register unsigned int t;

	// Initialize the calendar queue
	calqueue_init();

	// TODO: qua è necessario inizializzare il sottosistema delle statistiche, utilizzando il nuovo approccio (da pensare)

	// Initialize the per LP variables
	serial_states = rsalloc(sizeof(void *) * n_prc_tot);
	serial_completed_simulation = rsalloc(sizeof(bool) * n_prc_tot);
	bzero(serial_states, sizeof(void *) * n_prc_tot);
	bzero(serial_completed_simulation, sizeof(bool) * n_prc_tot);

	// Sanity check on the number of LPs
	if(unlikely(n_prc_tot == 0)) {
		rootsim_error(true, "You must specify the total number of Logical Processes\n");
	}

	// Generate the INIT events for all the LPs
	for (t = 0; t < n_prc_tot; t++) {
		SerialScheduleNewEvent(t, 0.0, INIT, NULL, 0);
	}

	// No LP is scheduled now
	current_lp = idle_process;
}


void serial_simulation(void) {
	timer serial_event_execution;
	timer serial_gvt_timer;
	msg_t *event;
	unsigned int completed = 0;
	void *content = NULL;

	#ifdef EXTRA_CHECKS
        unsigned long long hash1, hash2;
	hash1 = hash2 = 0;
        #endif

	timer_start(serial_gvt_timer);

	statistics_post_other_data(STAT_SIM_START, 0.0);

	while(!serial_simulation_complete) {

		event = (msg_t *)calqueue_get();
		if(unlikely(event == NULL)) {
			rootsim_error(true, "No events to process!\n");
		}

		#ifdef EXTRA_CHECKS
		if(event->size > 0) {
	                hash1 = XXH64(event->event_content, event->size, current_lp);
		}
                #endif

		current_lp = GidToLid(event->receiver);
		current_lvt = event->timestamp;

		content = event->size > 0 ? event->event_content : NULL;

		timer_start(serial_event_execution);

		ProcessEvent_light(lid_to_int(current_lp), current_lvt, event->type, content, event->size, serial_states[lid_to_int(current_lp)]);

		statistics_post_lp_data(current_lp, STAT_EVENT_TIME, timer_value_seconds(serial_event_execution) );
		statistics_post_lp_data(current_lp, STAT_EVENT, 1.0);

		#ifdef EXTRA_CHECKS
		if(event->size > 0) {
                	hash2 = XXH64(event->event_content, event->size, current_lp);
		}

                if(hash1 != hash2) {
			printf("hash1 = %llu, hash2= %llu\n", hash1, hash2);
                        rootsim_error(true, "Error, LP %d has modified the payload of event %d during its processing. Aborting...\n", current_lp, event->type);
                }
                #endif

		current_lp = idle_process;

		// Termination detection can happen only after the state is initialized
		if(likely(serial_states[gid_to_int(event->receiver)] != NULL)) {
			// Should we terminate the simulation?
			if(!serial_completed_simulation[gid_to_int(event->receiver)] && OnGVT_light(gid_to_int(event->receiver), serial_states[gid_to_int(event->receiver)])) {
				completed++;
				serial_completed_simulation[gid_to_int(event->receiver)] = true;
				if(unlikely(completed == n_prc_tot)) {
					serial_simulation_complete = true;
				}
			}
		}

		// Termination detection on reached LVT value
		if(rootsim_config.simulation_time > 0 && event->timestamp >= rootsim_config.simulation_time) {
			serial_simulation_complete = true;
		}

		// Simulate the execution of GVT protocol
	        if (timer_value_milli(serial_gvt_timer) > (int)rootsim_config.gvt_time_period) {
	                timer_restart(serial_gvt_timer);
	                printf("TIME BARRIER: %f\n", current_lvt);
	                statistics_post_other_data(STAT_GVT, current_lvt);
		}

		rsfree(event);
	}

	simulation_shutdown(EXIT_SUCCESS);
}
