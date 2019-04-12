#pragma once
#include <stdbool.h>
#include <ROOT-Sim.h>

enum _bug_events_t{
	PRODUCE_FOOD = INIT + 1,
	GUY_LEAVE,
	GUY_VISIT,
	KEEP_ALIVE,
	GUY_DELAYED_VISIT,
	_TRAVERSE
};

#define TIME_STEP 1.0
#define AGENT_SPAWN_PROBABILITY 0.5
#define AGENT_THRESHOLD 0.25
#define AGENT_IS_ENGINEER_PROBABILITY 0.4

