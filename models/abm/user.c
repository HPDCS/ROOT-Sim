#include "user.h"

#include <memory.h>

#include "jsmn_helper.h"

int user_init_agent(const agent_t *agent, const jsmntok_t *t_d, const char *base) {
	// Simple example of custom initialization
	agent_data_t *agent_d = data_agent(agent);
	int name_len;
	c_jsmntok_t *t = get_value_token_by_key(t_d, base, t_d, "name");
	if (t == NULL || t->type != JSMN_STRING)
		return -1;

	name_len = t->end - t->start;
	name_len = name_len > NAME_LENGTH - 1 ? NAME_LENGTH - 1 : name_len;
	memcpy(agent_d->name, &base[t->start], name_len * sizeof(char));

	agent_d->name[name_len ? name_len : 0] = '\0';

	/* have fun! */

	return 0;
}

int user_init_region(const region_t *region, const jsmntok_t *t_d, const char *base) {
	// Simple example of custom initialization
	region_data_t *region_d = data_region(region);
	int name_len;
	c_jsmntok_t *t = get_value_token_by_key(t_d, base, t_d, "name");
	if (t == NULL || t->type != JSMN_STRING)
		return -1;

	name_len = t->end - t->start;
	name_len = name_len > NAME_LENGTH - 1 ? NAME_LENGTH - 1 : name_len;
	memcpy(region_d->name, &base[t->start], name_len * sizeof(char));

	region_d->name[name_len ? name_len : 0] = '\0';

	/* have fun! */

	return 0;
}


bool user_done_region(const region_t *region) {
	if(!count_agent_region(region))
		return true;
	const agent_t *agent = NULL;
	while(iterate_c_agent_region(region, &agent)){
		if(!is_chilling_agent(agent))
			return false;
	}
	return true;
}

#define RESIDENCE_TIME 10

simtime_t user_residence_time(const region_t *region, const agent_t *agent, simtime_t now) {
	return Random()*RESIDENCE_TIME + 1; /* eheh STUB */
}

int user_on_visit(const region_t *region, const agent_t *agent, simtime_t now){
	return 0;
}

int user_on_leave(const region_t *region, const agent_t *agent, simtime_t now){
	return 0;
}

int	user_compile_neighbour_state(const region_t *region, neighbour_state_t *neighbour_state, simtime_t now){
	return 0;
}

void user_print_region(const region_t* region, FILE *out_stream){
	fprintf(out_stream, "Hello! I'm region %s\n", data_region(region)->name);
}

void user_print_agent(const agent_t* agent, FILE *out_stream){
	fprintf(out_stream, "Hello! I'm agent %s\n", data_agent(agent)->name);
}
