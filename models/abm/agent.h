#ifndef ABM_AGENT_H_
#define ABM_AGENT_H_
// TODO DOCUMENT THIS STUFF
#include <ROOT-Sim.h>

/**
 * This header file and his related .c file are mostly unaware of threading and distributed systems issues:
 * these issues are addressed in the config.h and config.c logic.
 * Calling one of these function with a NULL argument instead of a required pointer argument, results in
 * undefined behaviour except where noted otherwise.
 */

/* TYPEDEFS AND MACROS */

typedef unsigned int agent_action_t;

#define ACTION_INVALID UINT_MAX
#define ACTION_START UINT_MAX-1
#define ACTION_TRAVERSE UINT_MAX-2

#define ACTION_STR_START "Start"
#define ACTION_STR_TRAVERSE "Traverse"


typedef struct _agent_t agent_t;
typedef struct _region_t region_t;
typedef struct _region_data_t region_data_t;
typedef struct _neighbour_state_t neighbour_state_t;
typedef struct _agent_data_t agent_data_t;


/*****************************************************************************/
/**************************TOPOLOGY FUNCTIONS*********************************/
/*****************************************************************************/

/**
 * This sets the topology for the lifetime of the simulation.
 * This needs to be set before doing anything else in this header.
 * @param topology the topology you wish to set
 * @return 0 on success, -1 on failure.
 * Currently supported topologies are TOPOLOGY_HEXAGON and TOPOLOGY_SQUARE.
 * Failure happens if you tried to set the topology more than once or the specified topology is not supported.
 */
int					set_topology			(topology_t topology);


/*****************************************************************************/
/****************************REGION FUNCTIONS*********************************/
/*****************************************************************************/

/**
 * This sets the region id as obstacle (notice this requires simply the LP id instead of the full featured struct
 * so that machines who don't have access to specific regions could still mark them as obstacles).
 * @param lp_id the LP id (which SHOULD BE in a one to one relation with regions) you want to mark as obstacle.
 * @return 0 on success, -1 on failure.
 * Failure happens if you tried to mark a non existent region.
 */
int					set_obstacle_region_id	(unsigned int lp_id);

/**
 * This checks if the region with id lp_id is an obstacle (notice this requires simply the LP id instead of the full featured struct
 * so that machines who don't have access to specific regions could still verify the obstacles).
 * @param lp_id the LP id (which SHOULD BE in a one to one relation with regions) you want to check.
 * @return true if lp_id is indeed the id of an obstacle region, false in all the other cases (included failure!).
 * Failure happens if you tried to query a non existent region.
 */
bool				is_obstacle_region_id	(unsigned int lp_id);

/**
 * This instantiates the region with lp_id as id.
 * @param lp_id the LP id which is instantiating the region.
 * @return a new instantiated region, NULL on failure.
 * It is expected (and enforced somewhere else) that LPs are in a one to one relation with regions.
 * Failure happens if you tried to create a region for a non existing LP id or if the machine is catching fire.
 */
region_t*			new_region				(unsigned int lp_id);

/**
 * This checks if the region is an obstacle.
 * @param region a pointer to a region struct instantiated by new_region
 * @return true if region is indeed an obstacle region, false otherwise.
 */
bool 				is_obstacle_region		(const region_t *region);

/**
 * This returns the user custom data associated to the region.
 * @param region a pointer to a region struct instantiated by new_region
 * @return a pointer to a region_data struct, lookup the definition at user.h
 */
region_data_t*  	data_region				(const region_t *region);

/**
 * This returns the computation status related to the region.
 * @param region a pointer to a region struct instantiated by new_region
 * @return true if the region has completed his computation, false if it has other work to do.
 * Note that this is not a necessarily stable predicate. A region could possibly return true,
 * and then false at some point in the future (for example, a new busy agent visit this region).
 */
bool				done_region				(const region_t *region);

/**
 * This frees up the resources held by target region together with all agents currently visiting it.
 * Semantic is similar to stdlib free().
 * @param region a pointer to a region struct instantiated by new_region or NULL
 */
void				free_region				(region_t *region);


/*****************************************************************************/
/*****************************ACTION FUNCTIONS********************************/
/*****************************************************************************/

/**
 * This registers a named action.
 * @param name a null terminated string, the name of the action you want to register.
 * @return on success returns a proper action code, on failure returns the special error code INVALID_ACTION.
 * Only the reference to the string is kept; don't modify it after registration!
 * Note that there are always 2 pre-registered actions as specified by the macros START_ACTION and TRAVERSE_ACTION
 * Failure occurs if you tried to register the same action name twice or
 * if the number of registered action would exceed MAX_USER_ACTIONS (specified in user.h).
 */
agent_action_t		register_action			(const char *name);

/**
 * This retrieves the action code of a previously registered named action.
 * @param name a not necessarily null terminated string, the name of the action you want to register.
 * @param name_len the count of the first characters to consider in string name
 * @return on success returns a proper action code, on failure returns the special error code INVALID_ACTION.
 * Failure occurs if you tried to retrieve a non registered string.
 * The signature is "accomodated" in order to permit an easier use in config.c
 */
agent_action_t 		retrieve_action			(const char *name, unsigned name_len);

/**
 * This retrieves the action name of a previously registered named action
 * @param action the action code of a previously registered action
 * @return on success returns a null terminated string, on failure returns NULL.
 * Failure occurs if you tried to retrieve a non registered action.
 */
const char*			name_action				(agent_action_t action);


/*****************************************************************************/
/*****************************AGENT FUNCTIONS*********************************/
/*****************************************************************************/

/**
 * This instantiates a new agent structure.
 * @param start_region_id the LP id representing the starting region
 * @param dest_count the count of the regions the agent wants to visit (excluding his starting region)
 * @param destinations a list of the LP ids of the regions to visit
 * @param actions a list of valid action codes which are the actions the agent commits at the destinations
 * @return a fresh instantiated agent on success, NULL on failure
 * Failure happens if you supply non existent regions and/or actions.
 */
agent_t*			new_agent				(unsigned int start_region_id, unsigned int dest_count,
												const unsigned int destinations[dest_count], const agent_action_t actions[dest_count]);

/**
 * This returns the agent unique id.
 * @param agent a pointer to an agent struct instantiated by new_agent
 * @return the unique id associated with the agent
 */
unsigned long long 	uuid_agent				(const agent_t *agent);

/**
 * This returns the agent size in bytes.
 * @param agent a pointer to an agent struct instantiated by new_agent
 * @return the count of bytes currently used by the agent struct
 */
size_t				size_agent				(const agent_t *agent);

/**
 * This returns the action agent would commit in the region he's visiting.
 * @param agent a pointer to an agent struct instantiated by new_agent
 * @return the action which the agent wishes to commit in the region he's visiting
 */
agent_action_t		current_action_agent	(const agent_t *agent);

/**
 * This returns the region which the agent is visiting.
 * @param agent a pointer to an agent struct instantiated by new_agent
 * @return the region expressed as LP id which the agent is visiting.
 */
unsigned int 		current_region_agent	(const agent_t *agent);

/**
 * This returns the region which the agent is willing to visit next.
 * @param agent a pointer to an agent struct instantiated by new_agent
 * @return the region expressed as LP id which the agent will visit next
 */
unsigned int 		next_region_agent		(const agent_t *agent);

/**
 * This returns the region where the agent is heading for the real objective
 * of his current mission.
 * @param agent a pointer to an agent struct instantiated by new_agent
 * @return the region expressed as LP id in which the agent will complete an objective of his mission
 */
unsigned int 		current_destination_agent(const agent_t *agent);

/**
 * This checks if the agent currently has no other missions to accomplish.
 * @param agent a pointer to an agent struct instantiated by new_agent
 * @return true if the agent has no more destinations planned for him, false otherwise.
 */
bool 				is_chilling_agent		(const agent_t *agent);

/**
 * This calculates the agent residence time in a region.
 * @param region a pointer to the region the agent is currently visiting
 * @param agent a pointer to the agent conducting the visit
 * @param now the current simulation time
 * @return a non negative value representing the time the agent wants to stay in the region
 */
simtime_t			residence_time_region_agent(region_t *region, agent_t *agent, simtime_t now);

/**
 * This returns the user custom data associated to the agent.
 * @param agent a pointer to a region struct instantiated by new_region
 * @return a pointer to a agent_data struct, lookup the definition at user.h
 */
agent_data_t*		data_agent				(const agent_t *agent);

/**
 * This computes the tour for the agent's next destination.
 * The agent then "knows" where to go until he reaches the next destination.
 * The tour chosen is the shortest (no further guarantees are given regarding the choice
 * between several minimum length paths).
 * Since the recomputing requires a reallocation of the agent memory, the pointer to the region
 * currently visited must be provided.
 * @param agent a pointer to the agent we want to compute the tour for
 * @param agent a pointer to the region the agent is currently visiting
 * @return the expected number of steps required to reach the next destination or UINT_MAX if the next destination is unreachable
 */
unsigned int 		compute_path_agent		(agent_t **agent_p, region_t *region);

/**
 * This frees up the resources held by target agent.
 * Semantic is similar to stdlib free().
 * @param region a pointer to a agent struct instantiated by new_agent or NULL
 */
void				free_agent				(agent_t *agent);

/*****************************************************************************/
/***********************AGENT INTO REGION FUNCTIONS***************************/
/*****************************************************************************/

/**
 * This makes the agent visit the region at time now
 * @param region the region the agent wants to visit
 * @param agent the agent subject
 * @param now the current simulation time
 * @return a non-negative value in case of success, a negative value otherwise.
 * Failure happens if the region isn't in the agent planned tour
 * (agents don't go on vacations during missions :))
 * or if the user decides so.
 */
int 				visit_agent_region		(region_t *region, agent_t *agent, simtime_t now);

/**
 * This makes the agent ready to leave the region
 * @param region the region the agent wants to leave
 * @param agent the agent uuid
 * @param now the current simulation time
 * @return the pointer to the agent struct, NULL in case of failure
 * Failure happens if the agent couldn't be retrieved or if the user decides so.
 */
agent_t*			leave_agent_by_id_region(region_t *region, unsigned long long uuid, simtime_t now);

agent_t*			find_agent_by_id_region	(region_t *region, unsigned long long uuid);
size_t				count_agent_region		(const region_t *region);
bool				iterate_agent_region	(region_t *region, agent_t **agent_p);
bool				iterate_c_agent_region	(const region_t *region, const agent_t **agent_p);


bool				iterate_neighbour_data_region(const region_t *region, const neighbour_state_t **neighbour_p, unsigned int *neighbour_id, direction_t *direction);

/*****************************************************************************/
/***********************MANAGING NEIGHBOURS UPDATES***************************/
/*****************************************************************************/

/**
 * This checks if the region wants to send new data to its neighbours.
 * @param region the region to be checked
 * @param now the logical time when the check is made
 * @return true if this region needs to throw a round of updates, false otherwise
 */
bool				need_refresh_region		(region_t *region, simtime_t now);

/**
 * This iterates over the updates the region has prepared for its neighbours
 * @param region the region to be checked.
 * @param[out] neighbour_id the LP id of the intended recipient of the current update.
 * @param[out] update the pointer to the update data, this needs be set to a variable pointing to NULL at first.
 * @return true if there are more updates to be thrown, false otherwise.
 */
bool				iterate_neighbour_update_region(region_t *region, unsigned int *neighbour_id, void **update);

/**
 * This notifies the region of an update it received.
 * @param region the region which received an update
 * @param update the update data sent to the region
 */
void				apply_update_region		(region_t *region, void *update);

/**
 * This returns the size in bytes of the update
 * @param update the update data sent to the region
 * @return the size expressed in number of bytes, of the update
 */
size_t				size_update_region		(void);

/**
 * This prints on stdout a very scarce summary of the region's state
 * @param region the region to be printed
 */
void 				print_region			(region_t *region);

#endif /* ABM_AGENT_H_ */
