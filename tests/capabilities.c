#include <ROOT-Sim.h>

void ProcessEvent(unsigned int me, simtime_t now, int type, void *payload, unsigned int size, void *state) {
	(void)me;
	(void)now;
	(void)type;
	(void)payload;
	(void)size;
	(void)state;

	struct capability_info_t info;

	// Repeatedly query the capability subsystem
	CapabilityAvailable(CAP_SCHEDULER, NULL);
	CapabilityAvailable(CAP_CKTRM_MODE, NULL);
	CapabilityAvailable(CAP_LPS_DISTRIBUTION, NULL);
	CapabilityAvailable(CAP_STATS, NULL);
	CapabilityAvailable(CAP_STATE_SAVING, NULL);
	CapabilityAvailable(CAP_THREADS, NULL);
	CapabilityAvailable(CAP_LPS, NULL);
	CapabilityAvailable(CAP_OUTPUT_DIR, NULL);
	CapabilityAvailable(CAP_P, NULL);
	CapabilityAvailable(CAP_GVT, NULL);
	CapabilityAvailable(CAP_SEED, NULL);
	CapabilityAvailable(CAP_VERBOSE, NULL);
	CapabilityAvailable(CAP_NPWD, NULL);
	CapabilityAvailable(CAP_FULL, NULL);
	CapabilityAvailable(CAP_INC, NULL);
	CapabilityAvailable(CAP_A, NULL);
	CapabilityAvailable(CAP_SIMULATION_TIME, NULL);
	CapabilityAvailable(CAP_DETERMINISTIC_SEED, NULL);
	CapabilityAvailable(CAP_SERIAL, NULL);
	CapabilityAvailable(CAP_CORE_BINDING, NULL);
	CapabilityAvailable(CAP_PREEMPTION, NULL);
	CapabilityAvailable(CAP_ECS, NULL);
	CapabilityAvailable(CAP_LINUX_MODULES, NULL);
	CapabilityAvailable(CAP_MPI, NULL);
	CapabilityAvailable(CAP_POWER, NULL);

	CapabilityAvailable(CAP_SCHEDULER, &info);
	CapabilityAvailable(CAP_CKTRM_MODE, &info);
	CapabilityAvailable(CAP_LPS_DISTRIBUTION, &info);
	CapabilityAvailable(CAP_STATS, &info);
	CapabilityAvailable(CAP_STATE_SAVING, &info);
	CapabilityAvailable(CAP_THREADS, &info);
	CapabilityAvailable(CAP_LPS, &info);
	CapabilityAvailable(CAP_OUTPUT_DIR, &info);
	CapabilityAvailable(CAP_P, &info);
	CapabilityAvailable(CAP_GVT, &info);
	CapabilityAvailable(CAP_SEED, &info);
	CapabilityAvailable(CAP_VERBOSE, &info);
	CapabilityAvailable(CAP_NPWD, &info);
	CapabilityAvailable(CAP_FULL, &info);
	CapabilityAvailable(CAP_INC, &info);
	CapabilityAvailable(CAP_A, &info);
	CapabilityAvailable(CAP_SIMULATION_TIME, &info);
	CapabilityAvailable(CAP_DETERMINISTIC_SEED, &info);
	CapabilityAvailable(CAP_SERIAL, &info);
	CapabilityAvailable(CAP_CORE_BINDING, &info);
	CapabilityAvailable(CAP_PREEMPTION, &info);
	CapabilityAvailable(CAP_ECS, &info);
	CapabilityAvailable(CAP_LINUX_MODULES, &info);
	CapabilityAvailable(CAP_MPI, &info);
	CapabilityAvailable(CAP_POWER, &info);
}


bool OnGVT(unsigned int me, void *snapshot) {
	(void)me;
	(void)snapshot;

	return true;
}
