#include "application.h"
#include "config.h"
#include "logger.h"

enum events {
	AGENT_IN = INIT + 1, AGENT_OUT, AGENT_CHANGE_DEST, HEARTBEAT, UPDATE_NEIGHBOUR
};

static int send_updates(simtime_t now, region_t* region);

void ProcessEvent(unsigned int me, simtime_t now, int event_type, void *event_content, int event_size, region_t *state) {
	agent_t *agent;
	unsigned long long uuid;
	config_error_t conf_err = 0;

	abm_log(ABM_LOG_TRACE, "[LP %u, time %.3f] :: Started processing event %d at time %.7f", me, now, event_type, now);

	switch (event_type) {

	case INIT:
		// Read the config file
		conf_err = init_config();
		if (conf_err < 0) {
			abm_log(ABM_LOG_FATAL, "[LP %u, time %.3f] :: Can't load the configuration: %s", me, now, error_msg_config(conf_err));
			exit(-1);
		}
		abm_log(ABM_LOG_DEBUG, "[LP %u, time %.3f] :: Configuration successfully loaded", me, now);

		// Initialize simulation state
		conf_err = get_region_config(me, &state);
		if (conf_err < 0) {
			abm_log(ABM_LOG_FATAL, "[LP %u, time %.3f] :: Can't load the region: %s", me, now, error_msg_config(conf_err));
			exit(-1);
		}
		abm_log(ABM_LOG_DEBUG, "[LP %u, time %.3f] :: Region successfully instantiated", me, now);

		if (count_agent_region(state) != 0 && is_obstacle_region(state)) {
			abm_log(ABM_LOG_FATAL, "[LP %u, time %.3f] ::  Obstacle is hosting agents!!!", me, now);
			exit(-1);
		}

		SetState(state);
		abm_log(ABM_LOG_DEBUG, "[LP %u, time %.3f] :: State successfully set", me, now);

		// Schedule a leave event for all generated agents
		agent = NULL;
		while (iterate_agent_region(state, &agent)) {
			if (is_chilling_agent(agent))
				continue;
			uuid = uuid_agent(agent);
			abm_log(ABM_LOG_TRACE, "[LP %u, time %.3f] :: Dispatching agent '%llu'", me, now, uuid);
			ScheduleNewEvent(me, now + residence_time_region_agent(state, agent, now), AGENT_OUT, &uuid, sizeof(uuid));
		}
		abm_log(ABM_LOG_INFO, "[LP %u, time %.3f] :: Ready to process events", me, now);
		break;

	case AGENT_OUT:
		uuid = *(unsigned long long *) event_content;

		// This agent is leaving: remove from the list
		agent = leave_agent_by_id_region(state, uuid, now);
		if (!agent) {
			abm_log(ABM_LOG_FATAL, "[LP %u, time %.3f] :: Couldn't find agent '%llu' even if it needed to be removed", me, now, uuid);
			exit(-1);
		}
		abm_log(ABM_LOG_TRACE, "[LP %u, time %.3f] :: Agent '%llu' successfully removed", me, now, uuid);

		// Sanity check: an agent who completed his mission doesn't need to move further
		if (is_chilling_agent(agent)) {
			abm_log(ABM_LOG_ERROR, "[LP %u, time %.3f] :: Agent '%llu' is chilling; he shouldn't try to leave", me, now, uuid);
			break;
		}

		// Send the agent to the destination cell
		ScheduleNewEvent(next_region_agent(agent), now, AGENT_IN, agent, size_agent(agent));
		abm_log(ABM_LOG_DEBUG, "[LP %u, time %.3f] :: Agent '%llu' wants to move in region '%u' towards destination '%u'", me, now,
				uuid, next_region_agent(agent), current_destination_agent(agent));

		// The agent has been copied by the platform into the event's content, now it is possible to free that buffer
		free_agent(agent);
		break;

	case AGENT_IN:
		uuid = uuid_agent(event_content);
		// Sanity check: an obstacle could not host an agent
		if (is_obstacle_region_id(me)) {
			abm_log(ABM_LOG_FATAL, "[LP %u, time %.3f] :: Obstacle is requested to host agent '%llu'", me, now, uuid);
			exit(-1);
		}

		// Get the agent by copying the 'one' provided into the event's payload
		// so that we still work in data separation
		agent = malloc(event_size);
		if (!agent) {
			abm_log(ABM_LOG_FATAL, "[LP %u, time %.3f] :: Unable to allocate %d bytes of memory", me, now, event_size);
			exit(-1);
		}
		memcpy(agent, event_content, event_size);

		// Add the agent to the current list
		if (visit_agent_region(state, agent, now) < 0) {
			abm_log(ABM_LOG_ERROR, "[LP %u, time %.3f] :: Agent '%llu' is unable to visit this region", me, now, uuid);
		}
		abm_log(ABM_LOG_DEBUG, "[LP %u, time %.3f] :: Agent '%llu' visited this region", me, now, uuid);

		if (is_chilling_agent(agent)) {
			abm_log(ABM_LOG_INFO, "[LP %u, time %.3f] :: Agent '%llu' is chilling", me, now, uuid);
			// This agent doesn't need to travel anymore
			break;
		}

		if (compute_path_agent(&agent, state) == UINT_MAX) {
			abm_log(ABM_LOG_ERROR, "[LP %u, time %.3f] :: Unable to plan a new tour for the agent '%llu'", me, now, uuid);
		}

		// Schedule a leave event
		ScheduleNewEvent(me, now + residence_time_region_agent(state, agent, now), AGENT_OUT, &uuid, sizeof(uuid));

		break;

	case AGENT_CHANGE_DEST:
		uuid = *(unsigned long long *) event_content;

		// Find the agent identified by the UUId provided in the event's content
		agent = find_agent_by_id_region(state, uuid);

		// Compute a new destination cell for that agent
		if (compute_path_agent(&agent, state) == UINT_MAX) {
			abm_log(ABM_LOG_ERROR, "[LP %u, time %.3f] :: Unable to determine a new destination for the agent '%llu'", me, now, uuid);
		}

		// Schedule the event to leave the current cell towards the destination one
		ScheduleNewEvent(me, now + residence_time_region_agent(state, agent, now), AGENT_OUT, &uuid, sizeof(uuid));

		break;

	case UPDATE_NEIGHBOUR:
		abm_log(ABM_LOG_DEBUG, "[LP %u, time %.3f] :: We got an update from a neighbour", me, now);
		apply_update_region(state, event_content);
		break;

	case HEARTBEAT:
		// We don't do anything
		break;

	default:
		// huh?
		abm_log(ABM_LOG_FATAL, "[LP %u, time %.3f] :: Unsupported event %d", me, now, event_type);
		exit(-1);
	}

	abm_log(ABM_LOG_TRACE, "[LP %u, time %.3f] :: Finished processing event %d at time %.7f", me, now, event_type, now);
	
	/**
	 * XXX: Without the heartbeats the simulation doesn't complete,
	 * are these necessary or am I noob and fucked up somewhere else?
	 */
	if(done_region(state))
		ScheduleNewEvent(me, now+10, HEARTBEAT, NULL, 0);

	// we check whether we need to update our neighbours
	if (need_refresh_region(state, now)){
		abm_log(ABM_LOG_DEBUG, "[LP %u, time %.3f] :: sending updates!", me, now);
		send_updates(now, state);
	}
}

// Commodity function to throw updates at neighbours
static int send_updates(simtime_t now, region_t* region) {
	unsigned int neighbour_id = UINT_MAX;
	void *update = NULL;
	while (iterate_neighbour_update_region(region, &neighbour_id, &update)) {
		ScheduleNewEvent(neighbour_id, now, UPDATE_NEIGHBOUR, update, size_update_region());
	}
	return 0;
}

// TODO revise this stuff
bool OnGVT(unsigned int me, region_t *snapshot) {
	if(count_agent_region(snapshot))
		print_region(snapshot);
	//return true;
	return done_region(snapshot);
}
