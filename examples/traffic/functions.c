/**
*
* TRAFFIC is a simulation model for the ROme OpTimistic Simulator (ROOT-Sim)
* which allows to simulate car traffic on generic routes, which can be
* specified from text file.
*
* The software is provided as-is, with no guarantees, and is released under
* the GNU GPL v3 (or higher).
*
* For any information, you can find contact information on my personal webpage:
* http://www.dis.uniroma1.it/~pellegrini
*
* @file functions.c
* @brief Implementation of functions for supporting the simulation
* @author Alessandro Pellegrini
* @date January 12, 2012
*/

#include <ROOT-Sim.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "application.h"
#include "normal_cdf.h"


/*static*/ double Gaussian(double m, double s)
/* ========================================================================
 * Returns a normal (Gaussian) distributed real number.
 * NOTE: use s > 0.0
 *
 * Uses a very accurate approximation of the normal idf due to Odeh & Evans,
 * J. Applied Statistics, 1974, vol 23, pp 96-97.
 * ========================================================================
 * copied from http://www.cs.wm.edu/~va/software/park/
 */
{
	const double p0 = 0.322232431088;     const double q0 = 0.099348462606;
	const double p1 = 1.0;                const double q1 = 0.588581570495;
	const double p2 = 0.342242088547;     const double q2 = 0.531103462366;
	const double p3 = 0.204231210245e-1;  const double q3 = 0.103537752850;
	const double p4 = 0.453642210148e-4;  const double q4 = 0.385607006340e-2;
	double u, t, p, q, z;

	u = Random();
	if (u < 0.5)
		t = sqrt(-2.0 * log(u));
	else
		t = sqrt(-2.0 * log(1.0 - u));
	p = p0 + t * (p1 + t * (p2 + t * (p3 + t * p4)));
	q = q0 + t * (q1 + t * (q2 + t * (q3 + t * q4)));

	if (u < 0.5)
		z = (p / q) - t;
	else
		z = t - (p / q);

	return (m + s * z);
}


static simtime_t compute_traverse_time(lp_state_type *state, double mean_speed) {
	double traffic;
	double speed, real_speed;
	simtime_t traverse_time;

	// Compute the traffic scaling factor
	traffic = (double)state->queued_elements / (double)state->total_queue_slots;

	// Compute the speed
	speed = MIN_SPEED;
	if(mean_speed > MIN_SPEED) {
		speed += (mean_speed - MIN_SPEED) * traffic;
	}

	do {
		// Compute traverse time according to a Normal distribution
		real_speed = abs(Gaussian(speed, SPEED_SIGMA));
		traverse_time = (simtime_t)(state->segment_length / real_speed);
	} while(isinf(traverse_time));

	// Convert the traverse time in seconds
	traverse_time *= 3600.0;

	return traverse_time;
}


void release_cars(lp_state_type *state) {
	car_t *curr_car;
	
	curr_car = state->queue;
	while(curr_car != NULL) {
		curr_car->accident = false;
		curr_car = curr_car->next;
	}
}

simtime_t enqueue_car(int from, lp_state_type *state) {
	car_t *new_car;
	car_t *curr_car;
	
	// Create the car node
	new_car = malloc(sizeof(car_t));
	new_car->from = from;
	new_car->arrival = state->lvt;
	new_car->leave = state->lvt + compute_traverse_time(state, AVERAGE_SPEED);
	new_car->next = NULL;
	if(state->accident)
		new_car->accident = true;
	
	state->queued_elements++;

	if(state->queue == NULL) {
		state->queue = new_car;
		return new_car->leave;
	}
	
	if(state->queue->leave < new_car->leave) {
		new_car->next = state->queue;
		state->queue = new_car;
		return new_car->leave;
	}
	
	// Insert the car in reverse time order
	curr_car = state->queue;
	while(curr_car->next != NULL && curr_car->next->leave > new_car->leave)
		curr_car = curr_car->next;
	
	new_car->next = curr_car->next;
	curr_car->next = new_car;
	
	return new_car->leave;
}


void inject_new_cars(lp_state_type *state, int me) {
	simtime_t timestamp;
	event_content_type new_evt;

	// If the node has an interarrival frequency == -1, then no car can enter the node (i.e., a junction
	// with no actual ramps)
	if(D_EQUAL(state->enter_prob, -1.0)) {
		return;
	}

	// Entering timestamps ditributed according to an Erlang distribution
	timestamp = state->lvt + (simtime_t)(Expent(JUNCTION_TRAVERSE_TIME));

	// Send me the inject event
	new_evt.from = me;
	new_evt.injection = 1;
	ScheduleNewEvent(me, timestamp, ARRIVAL, &new_evt, sizeof(event_content_type));

}



void cause_accident(lp_state_type *state, int me) {
	double min;
	double max;
	double mean;
	double var;
	double prob;
	double coin;
	simtime_t duration;

	// if there is already an accident, don't cause another one
	if(state->accident) {
		return;
	}

	// An accident happens depending on the number of cars.
	// Accident probability is normal wrt the number of cars.
	// Derive a discrete probability from a normal distribution
	if(state->queued_elements == 0) {
		min = 0.0;
		max = 0.5;
	} else if(state->queued_elements == state->total_queue_slots) {
		min = (double)state->total_queue_slots - 0.5;
		max = (double)state->total_queue_slots;
	} else {
		min = (double)state->queued_elements - 0.5;
		max = (double)state->queued_elements + 0.5;
	}

	// TODO come calcolare qui la varianza?!
	mean = (double)state->total_queue_slots / 2.0; // When there are many cars but not that much, accidents are more likely to occur
	var = RandomRange(0, 100);

	prob = contourcdf(min, max, mean, var);
	prob *= (double)ACCIDENT_PROBABILITY;

	// Toss a coin to check whether an accident occured or not
	coin = Random();
	
	// If there is an accident, set the parameters accordingly and determine how long the accident will last
	if(coin <= prob) {

		state->accident = true;

		// Compute when the road will be freed, according to the node's accident duration
		do {
			duration = (simtime_t)(Gaussian(ACCIDENT_DURATION, ACCIDENT_SIGMA));
		} while(duration <= 0);
		
		ScheduleNewEvent(me, state->lvt + duration, FINISH_ACCIDENT, NULL, 0);

		printf("(%d) Accident at node %s at time %f, until %f\n", me, state->name, state->lvt, state->lvt + duration);
	}
}




int check_car_leaving(lp_state_type *state, int from, int me) {
	double coin;

	// If the node has a leaving probability == -1, then no car can leave the node
	if(D_EQUAL(state->leave_prob, -1.0)) {
		return 0;
	}

	if(state->lp_type == JUNCTION) {

		// If this is an end node and the car did not join the HW from here...
		if(state->topology->num_neighbours == 1 && from != me) {
			// Then the car has left
			return 1;
		}

		// If the car arrived here (and did not join from here) then check if it's leaving
		if(from != me) {
			coin = Random();
			if(coin <= state->leave_prob) {
				// The car has left
				return 1;
			}
		}
	}

	return 0;

}


car_t *car_dequeue(lp_state_type *state, simtime_t now) {
	car_t *curr_car, *curr_car_prev = NULL;
	
	curr_car = state->queue;
	
	while(curr_car != NULL && curr_car->leave - now > DBL_EPSILON) {
		curr_car_prev = curr_car;
		curr_car = curr_car->next;
	}
	
	if(curr_car_prev == NULL) {
		state->queue = curr_car->next;
	} else {
		curr_car_prev->next = curr_car->next;
	}
	
	state->queued_elements--;
	
	curr_car->next = NULL;
	return curr_car;
}


car_t *car_dequeue_conditional(lp_state_type *state, simtime_t now) {
	car_t *car = car_dequeue(state, now);
	
	if(car->accident == true)
		return NULL;
	return car;
}

