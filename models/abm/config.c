#include "config.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <alloca.h> // TODO: get rid of this, find a cleaner way to instantiate new agents

#include "jsmn.h"
#include "jsmn_helper.h"
#include "user.h"
#include "logger.h"

static char json_config[CONFIG_FILE_MAX_LENGTH];
static jsmntok_t tokens[CONFIG_FILE_MAX_LENGTH / 16];
static int tokens_count = -1;

#define root_token (&tokens[0])

// These things here assure we load the file exactly once per machine
// TODO more diversified mutexes could improve performances during initialization
static pthread_mutex_t user_config_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t config_lock = PTHREAD_MUTEX_INITIALIZER;

static bool load_config_done = false;
static config_error_t load_config_ret;

static bool topology_config_done = false;
static config_error_t topology_config_ret;

static void relocate_object_token(jsmntok_t *t_obj, const char **new_base_p, int *old_parent);
static void undo_relocate_object_token(jsmntok_t *t_obj, const char *new_base, int old_parent);
static c_jsmntok_t* get_region_token_by_lp_id(unsigned int me);
static c_jsmntok_t* get_agent_token_by_key_token(c_jsmntok_t *t_key);
static agent_t* get_agent_by_token(c_jsmntok_t *t, unsigned int start_region);
static config_error_t load_config(void);
static config_error_t general_config(void);

config_error_t init_config(void) {

	int ret = load_config();
	if (ret < 0)
		return ret;

	ret = general_config();
	if (ret < 0)
		return ret;

	return CONFIG_SUCCESS;
}

config_error_t get_region_config(unsigned int me, region_t **result) {

	agent_t *agent;
	region_t *region;
	c_jsmntok_t *t, *t_aux, *t_reg;
	jsmntok_t *t_usr;

	*result = NULL;

	t_reg = get_region_token_by_lp_id(me);
	if (t_reg == NULL)
		return CONFIG_BAD_REGION;

	// Verify user_data existence
	t_usr = (jsmntok_t*) get_value_token_by_key(root_token, json_config, t_reg, "data");
	if (t_usr == NULL || t_usr->type != JSMN_OBJECT)
		return CONFIG_BAD_REGION;

	// Retrieve agent list
	t = get_value_token_by_key(root_token, json_config, t_reg, "agents");
	if (t == NULL || t->type != JSMN_ARRAY)
		return CONFIG_BAD_REGION;

	region = new_region(me);
	if (region == NULL)
		return CONFIG_BAD_REGION;

	pthread_mutex_lock(&user_config_lock);
	const char *new_base;
	int old_parent;
	relocate_object_token(t_usr, &new_base, &old_parent);

	if (user_init_region(region, t_usr, new_base) < 0) {
		free_region(region);
		return CONFIG_BAD_USER_REGION;
	}

	undo_relocate_object_token(t_usr, new_base, old_parent);

	pthread_mutex_unlock(&user_config_lock);

	if (!is_obstacle_region_id(me)) {

		struct _gnt_closure_t closure = GNT_CLOSURE_INITIALIZER;

		while ((t_aux = get_next_token(root_token, t, &closure)) != NULL) {

			if (t_aux->type != JSMN_STRING) {
				free_region(region);
				return CONFIG_BAD_REGION;
			}

			t_aux = get_agent_token_by_key_token(t_aux);
			if (t_aux == NULL) {
				free_region(region);
				return CONFIG_BAD_AGENT;
			}

			agent = get_agent_by_token(t_aux, me);
			if (agent == NULL || visit_agent_region(region, agent, 0.0) < 0) {
				free_region(region);
				return CONFIG_BAD_AGENT;
			}
		}
	}

	*result = region;

	return CONFIG_SUCCESS;
}

/**
 * This is a generic function to load and parse a JSON configuration file.
 * It can be re-used verbatim for any ABM simulation model.
 */
static config_error_t load_config(void) {

	long int jslen;

	FILE *config;
	jsmn_parser p;

	pthread_mutex_lock(&config_lock);
	if (load_config_done)
		// Done already by some other thread
		goto end;

	/* Prepare parser */
	jsmn_init(&p);

	// Load configuration file
	config = fopen(CONFIG_FILE, "r");
	if (config == NULL) {
		load_config_ret = CONFIG_FILE_CANT_OPEN;
		goto end;
	}

	// Sanity check on the config length
	fseek(config, 0, SEEK_END);
	jslen = ftell(config);
	if (ferror(config)) {
		load_config_ret = CONFIG_FILE_CANT_OPEN;
		goto end;
	}
	rewind(config);
	if (jslen >= CONFIG_FILE_MAX_LENGTH) {
		load_config_ret = CONFIG_FILE_TOO_LARGE;
		goto end;
	}

	// Load the whole JSON file from disk
	if (fread(json_config, (size_t) jslen, 1, config) == 0 || ferror(config)) {
		load_config_ret = CONFIG_FILE_CANT_OPEN;
		goto end;
	}

	// Parse the loaded JSON file
	tokens_count = jsmn_parse(&p, json_config, (size_t) jslen, tokens,
	CONFIG_FILE_MAX_LENGTH / 16);
	if (tokens_count < 0) {
		switch (tokens_count) {
		case JSMN_ERROR_NOMEM:
			load_config_ret = CONFIG_FILE_TOO_LARGE;
			break;
		case JSMN_ERROR_INVAL:
		case JSMN_ERROR_PART:
			load_config_ret = CONFIG_FILE_BAD_FORMAT;
			break;
		default:
			load_config_ret = CONFIG_UNKNOWN_ERROR;
			break;
		}
		goto end;
	}
	load_config_ret = CONFIG_SUCCESS;
	end:
	load_config_done = true;
	pthread_mutex_unlock(&config_lock);
	return load_config_ret;
}

/**
 * This function initializes the topology and the obstacles of
 * the regions grid.
 */
static config_error_t general_config(void) {

	unsigned u_value;
	c_jsmntok_t *t, *t_aux;

	pthread_mutex_lock(&config_lock);
	if (topology_config_done) {
		// Done already by some other thread
		goto end;
	}
	// Get the list of actions
	t = get_value_token_by_key(root_token, json_config, root_token, "actions");
	if (t == NULL || t->type != JSMN_ARRAY) {
		topology_config_ret = CONFIG_BAD_ACTIONS;
		goto end;
	}

	struct _gnt_closure_t closure_ac = GNT_CLOSURE_INITIALIZER;

	while ((t_aux = get_next_token(root_token, t, &closure_ac)) != NULL) {

		if (t_aux->type != JSMN_STRING) {
			topology_config_ret = CONFIG_BAD_ACTIONS;
			goto end;
		}

		// NOTE: this should work safely since we are the only thread here;
		// moreover we only read this stuff once
		json_config[t_aux->end] = '\0';

		if (register_action(&json_config[t_aux->start]) == ACTION_INVALID) {
			topology_config_ret = CONFIG_BAD_ACTIONS;
			goto end;
		}

	}

	// Get the topology value
	t = get_value_token_by_key(root_token, json_config, root_token, "topology");
	if (t == NULL || t->type != JSMN_STRING) {
		topology_config_ret = CONFIG_BAD_TOPOLOGY;
		goto end;
	}
	if (strcmp_token(json_config, t, "HEXAGON")) {
		if (strcmp_token(json_config, t, "SQUARE")) {
			topology_config_ret = CONFIG_BAD_TOPOLOGY;
			goto end;
		} else
			set_topology(TOPOLOGY_SQUARE);
	} else
		set_topology(TOPOLOGY_HEXAGON);

	// Get the list of obstacles
	t = get_value_token_by_key(root_token, json_config, root_token, "obstacles");
	if (t == NULL || t->type != JSMN_ARRAY) {
		topology_config_ret = CONFIG_BAD_OBSTACLES;
		goto end;
	}

	struct _gnt_closure_t closure_ob = GNT_CLOSURE_INITIALIZER;

	while ((t_aux = get_next_token(root_token, t, &closure_ob)) != NULL) {

		if (parse_unsigned_token(json_config, t_aux, &u_value) < 0) {
			topology_config_ret = CONFIG_BAD_OBSTACLES;
			goto end;
		}

		set_obstacle_region_id(u_value);
	}

	topology_config_ret = CONFIG_SUCCESS;
	end:
	topology_config_done = true;
	pthread_mutex_unlock(&config_lock);
	return topology_config_ret;
}

static c_jsmntok_t* get_region_token_by_lp_id(unsigned int me) {

	c_jsmntok_t *t, *t_aux, *t_arr;
	unsigned u_value;

	t_arr = get_value_token_by_key(root_token, json_config, root_token, "regions");
	if (t_arr == NULL || t_arr->type != JSMN_ARRAY)
		return NULL;

	struct _gnt_closure_t closure = GNT_CLOSURE_INITIALIZER;

	while ((t = get_next_token(root_token, t_arr, &closure)) != NULL) {
		// a valid region object must have a valid id key-value pair
		t_aux = get_value_token_by_key(root_token, json_config, t, "id");
		if (t_aux == NULL || parse_unsigned_token(json_config, t_aux, &u_value) < 0)
			return NULL;

		if (u_value == me)
			return t;
	}
	return NULL;
}

/**
 * This function gets a string token,
 * returns the corresponding object token representing the agent with that ID
 */
static c_jsmntok_t* get_agent_token_by_key_token(c_jsmntok_t *t_key) {

	c_jsmntok_t *t_agents_array, *t_aux, *t_agt;
	const char *cmp_str;
	int cmp_str_len;

	if (t_key->type != JSMN_STRING)
		return NULL;

	cmp_str = &json_config[t_key->start];
	cmp_str_len = t_key->end - t_key->start;

	t_agents_array = get_value_token_by_key(root_token, json_config, root_token, "agents");
	if (t_agents_array == NULL || t_agents_array->type != JSMN_ARRAY)
		return NULL;

	struct _gnt_closure_t closure = GNT_CLOSURE_INITIALIZER;

	while ((t_agt = get_next_token(root_token, t_agents_array, &closure)) != NULL) {

		t_aux = get_value_token_by_key(root_token, json_config, t_agt, "id");
		if (t_aux == NULL || t_aux->type != JSMN_STRING)
			return NULL;

		if (cmp_str_len == (t_aux->end - t_aux->start)
				&& strncmp(cmp_str, &json_config[t_aux->start], (size_t) cmp_str_len) == 0)
			return t_agt;
	}

	return NULL;
}

static agent_t* get_agent_by_token(c_jsmntok_t *t_agt, unsigned int start_region) {

	c_jsmntok_t *t, *t_aux, *t_arr;
	jsmntok_t *t_usr;

	unsigned int *destinations;
	unsigned int dest_count;
	agent_action_t *actions;
	agent_t *agent;

	t_arr = get_value_token_by_key(root_token, json_config, t_agt, "task-list");
	if (t_arr == NULL || t_arr->type != JSMN_ARRAY)
		return NULL;

	destinations = alloca(t_arr->size * (sizeof(unsigned))); // todo alloca is always a bit scary, get rid of it
	actions = alloca(t_arr->size * (sizeof(agent_action_t)));

	struct _gnt_closure_t closure = GNT_CLOSURE_INITIALIZER;

	dest_count = 0;
	while ((t = get_next_token(root_token, t_arr, &closure)) != NULL) {

		t_aux = get_value_token_by_key(root_token, json_config, t, "cell");
		if (t_aux == NULL || parse_unsigned_token(json_config, t_aux, destinations + dest_count) < 0)
			return NULL;

		t_aux = get_value_token_by_key(root_token, json_config, t, "action");
		if (t_aux == NULL || t_aux->type != JSMN_STRING) {
			return NULL;
		}

		actions[dest_count] = retrieve_action(&json_config[t_aux->start], t_aux->end - t_aux->start);
		if (actions[dest_count] == ACTION_INVALID) {
			abm_log(ABM_LOG_DEBUG, "Found a malformed action in an agent with starting region %u", start_region);
			return NULL;
		}
		dest_count++;
	}

	t_usr = (jsmntok_t*) get_value_token_by_key(root_token, json_config, t_agt, "data");
	if (t_usr == NULL || t_usr->type != JSMN_OBJECT) {
		abm_log(ABM_LOG_DEBUG, "Couldn't find custom data field of an agent with starting region %u", start_region);
		return NULL;
	}

	agent = new_agent(start_region, dest_count, destinations, actions);
	if (agent == NULL) {
		abm_log(ABM_LOG_DEBUG, "Couldn't instantiate agent with starting region %u", start_region);
		return NULL;
	}

	pthread_mutex_lock(&user_config_lock);
	const char *new_base;
	int old_parent;

	relocate_object_token(t_usr, &new_base, &old_parent);

	if (user_init_agent(agent, t_usr, new_base) < 0) {
		free_agent(agent);
		abm_log(ABM_LOG_DEBUG, "Failed user initialization for agent in region %u", start_region);
		return NULL;
	}

	undo_relocate_object_token(t_usr, new_base, old_parent);

	pthread_mutex_unlock(&user_config_lock);

	return agent;
}

/**
 * This function modifies t_obj and his children token so that they look like
 * the result of parsing a file containing only the object represented by t_obj and his children.
 * This is useful so that we can pass the user's tokens to him without him having to manage the root token and all that stuff
 */
static void relocate_object_token(jsmntok_t *t_obj, const char **new_base_p, int *old_parent) {
	jsmntok_t *t;
	int c_offset, t_offset;

	c_offset = t_obj->start;
	t_offset = t_obj - root_token;

	t = t_obj + 1;
	while (t->end <= t_obj->end) {
		t->start -= c_offset;
		t->end -= c_offset;
		t->parent -= t_offset;
		t++;
	}

	t_obj->start -= c_offset;
	t_obj->end -= c_offset;
	*old_parent = t_obj->parent;
	t_obj->parent = root_token->parent;

	*new_base_p = json_config + c_offset;

	return;
}

static void undo_relocate_object_token(jsmntok_t *t_obj, const char *new_base, int old_parent) {
	jsmntok_t *t;
	int c_offset, t_offset;

	c_offset = new_base - json_config;
	t_offset = t_obj - root_token;

	t_obj->start += c_offset;
	t_obj->end += c_offset;
	t_obj->parent = old_parent;

	t = t_obj + 1;
	while (t->end <= t_obj->end) {
		t->start += c_offset;
		t->end += c_offset;
		t->parent += t_offset;
		t++;
	}

	return;
}

#define MISS_STR " specification is missing or malformed"

const char * const config_error_str[CONFIG_ERROR_COUNT] = {

[CONFIG_SUCCESS] = "success",

[-CONFIG_FILE_CANT_OPEN] = "unable to access the configuration file",

[-CONFIG_FILE_TOO_LARGE] = "the configuration file is too large,"
		" recompile with bigger CONFIG_FILE_MAX_LENGTH",

[-CONFIG_FILE_BAD_FORMAT] = "bad json format in the configuration file",

[-CONFIG_BAD_TOPOLOGY] = "topology" MISS_STR,

[-CONFIG_BAD_OBSTACLES] = "obstacles" MISS_STR,

[-CONFIG_BAD_REGION] = "region" MISS_STR,

[-CONFIG_BAD_AGENT] = "agent" MISS_STR,

[-CONFIG_BAD_ACTIONS] = "actions" MISS_STR,

[-CONFIG_BAD_USER_REGION] = "(user.h) custom region" MISS_STR,

[-CONFIG_BAD_USER_AGENT] = "(user.h) custom agent" MISS_STR,

[-CONFIG_UNKNOWN_ERROR] = "no idea..."

};

#undef MISS_STR

const char* error_msg_config(config_error_t cfg_err) {
	if (cfg_err > 0 || cfg_err <= -CONFIG_ERROR_COUNT)
		return NULL;
	return config_error_str[-cfg_err];
}
