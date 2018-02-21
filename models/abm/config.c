#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include <ROOT-Sim.h>

#include "application.h"
#include "jsmn.h"

// Buffer to store in memory the JSON configuration file.
// This must be a power of 2.
#define JSON_LENGTH	4096

// We rely on per-thread variables since INIT events are
// executed concurrently by the available workers.
// We don't use dynamic memory here as we want to keep the
// configuration out of the simulation state of the LPs and
// to allow for the reuse of the already parsed data.
static __thread char json_config[JSON_LENGTH];
static __thread bool conf_loaded = false;
static __thread jsmntok_t tokens[JSON_LENGTH/16];


// This is a state variable which tells what is the token
// associated with the last agent which has been
// returned by a get_next_agent() call. When the function
// returns NULL to notify that no more agents are present,
// this variable is automatically set to -1
static __thread int last_agent_token = -1;


// This function returns the size of a token's content.
// This is useful for sanity checks, e.g. if you have to
// copy some text into an array of chars.
static size_t get_content_size(jsmntok_t *t) {
	return t->end - t->start;
}

// Copy the content of a token into a given buffer. The destination
// buffer must have enough space.
static void copy_content(char *dest, jsmntok_t *t) {
	strncpy(dest, &json_config[t->start], t->end - t->start);
	dest[t->end - t->start] = '\0';
}

// Dump the content of a token. Useful for debugging your
// initialization
static void dump_content(jsmntok_t *t) {
	printf("%.*s\n", t->end - t->start, &json_config[t->start]);
}

// Dump a whole token information. Useful for debugging
// your initialization.
static void dump_token(jsmntok_t *t) {
	char *type;

	switch(t->type) {
		case JSMN_UNDEFINED:
			type = "undefined";
			break;
		case JSMN_OBJECT:
			type = "object";
			break;
		case JSMN_ARRAY:
			type = "array";
			break;
		case JSMN_STRING:
			type = "string";
			break;
		case JSMN_PRIMITIVE:
			type = "primitive";
			break;
	}
	printf("Token %ld\n", t - tokens);
	printf("\ttype: %s\n", type);
	printf("\tstart: %d\n", t->start);
	printf("\tend: %d\n", t->end);
	printf("\tsize: %d\n", t->size);
	printf("\tparent: %d\n", t->parent);
	printf("\tcontent: ");
	dump_content(t);
}

// Get the range of tokens which belong to a certain parent.
// The parent must exist.
static void get_token_range(int parent, int *start, int *end) {
		int i;
		jsmntok_t *t;

	*start = -1;
	*end = -1;

		for(i = 0; i < JSON_LENGTH/16; i++) {
				t = &tokens[i];

				if(t->type == JSMN_UNDEFINED)
						break;

		if(t->parent != parent && *start == -1)
			continue;
		if(t->parent < parent)
			break;

		if(*start == -1)
			*start = i;
		*end = i;
		}
}

// Find a token by its parent, its type and its content.
// This is useful to match keys.
static int get_token_by_content(int parent, unsigned int type, char *content) {
	int i, ret = -1;
	jsmntok_t *t;

	for(i = 0; i < JSON_LENGTH/16; i++) {
		t = &tokens[i];

		if(t->type == JSMN_UNDEFINED)
			break;

		if(t->type != type || t->parent != parent)
			continue;

		if(strncmp(&json_config[t->start], content, t->end - t->start) == 0) {
			ret = i;
			break;
		}
	}

	return ret;
}

// In a range of tokens, look for named objects.
static int get_token_value_in_range(int start, int end, char *name) {
	int i;
	jsmntok_t *t;

	for(i = start; i <= end; i += 2) {
		t = &tokens[i];

		if(strncmp(&json_config[t->start], name, t->end - t->start) == 0) {
			return i + 1;
		}
	}

	return -1;
}

static int find_region_config_value(char *name, char *me_str) {
	int idx;
	int start, end;
	int start2, end2;
	int i;
	jsmntok_t *t;

	idx = get_token_by_content(0, JSMN_STRING, "regions");
	get_token_range(idx + 1, &start, &end);
	for(i = start; i < end; i++) {
		if(tokens[i].type == JSMN_OBJECT) {
			get_token_range(i, &start2, &end2);
			idx = get_token_value_in_range(start2, end2, "id");
			t = &tokens[idx];

			if(strncmp(&json_config[t->start], me_str, t->end - t->start) == 0) {
				return get_token_value_in_range(start2, end2, name);
			}
		}
	}

	return -1;
}

static int find_agent_config(char *agent, char *name) {
		int idx;
		int start, end;
		int start2, end2;
		int i;
		jsmntok_t *t;

		idx = get_token_by_content(0, JSMN_STRING, "agents");
		get_token_range(idx + 1, &start, &end);
		for(i = start; i < end; i++) {
				if(tokens[i].type == JSMN_OBJECT) { 
						get_token_range(i, &start2, &end2);
						idx = get_token_value_in_range(start2, end2, "id");
						t = &tokens[idx];

						if(strncmp(&json_config[t->start], agent, t->end - t->start) == 0) {
				return get_token_value_in_range(start2, end2, name);
						}
				}
		}

		return -1;
}


// Get the number of tasks to be performed by an agent
static int agent_get_task_list_size(int idx) {
	int i = 0;
	int parent = idx;
	jsmntok_t *curr = &tokens[idx + 1];
	
	while(curr->parent == parent) {
		i++;
		idx++;
		curr = &tokens[idx];
	}

	return i;
}


// This function returns a malloc'd agent_t for the next agent
// which is born in a certain region. If no such agent exists,
// it returns NULL.
agent_t *get_next_agent(unsigned int me) {
	char me_str[64];
	char name[64];
	char buff[64];
	int i, idx;
	int start, end;
	int task_number;
	int task_list_tok;
	jsmntok_t *t, *agent_name;
	agent_t *new_agent = NULL;

	// Get the tokens which are related to region agents.
	snprintf(me_str, 64, "%u", me);
	idx = find_region_config_value("agents", me_str);
	get_token_range(idx, &start, &end);
	
	if(last_agent_token == -1) {
		// This is the first call to this function by a
		// region initialization.
		last_agent_token = start;	
	}

	if(last_agent_token + 1 > end)
		goto out;

	// Get the actual token describing the agent
	t = &tokens[last_agent_token++];
	strncpy(name, &json_config[t->start], t->end - t->start);
	name[t->end - t->start] = '\0';

	// Get pointers to attributes of interest
	idx = find_agent_config(name, "name");
	agent_name = &tokens[idx];

	// You can add here more of them

	idx = find_agent_config(name, "task-list") + 1;
	task_list_tok = idx;
	printf("Task list token for agent %s is at %d\n", name, task_list_tok);
	task_number = agent_get_task_list_size(idx) + 1;

	// Allocate the agent struct and populate the entries
	new_agent = malloc(sizeof(agent_t) + sizeof(visit_t) * task_number);
	bzero(new_agent, sizeof(agent_t) + sizeof(visit_t) * task_number);
	if(new_agent == NULL) {
		fprintf(stderr, "Error allocating agent_t structure.\n");
		exit(EXIT_FAILURE);
	}

	strncpy(new_agent->name, &json_config[agent_name->start], agent_name->end - agent_name->start);
	new_agent->uuid = GenerateUniqueId();
	new_agent->visited = 1; // We already visited the initial region
	new_agent->visit_list_size = task_number;

	// First visit is the origin region
	new_agent->visit_list[0].time = 0;
	new_agent->visit_list[0].region = me;
	new_agent->visit_list[0].action = START_POSITION;
	
	// Populate the visit list with the tasks that we have to do. It will
	// be later populated with the path to follow to reach the destinations.
	for(i = 1; i < task_number; i++) {
		new_agent->visit_list[i].time = INFTY; // Still have to visit this cell
		// Copy the destination region id
		
		t = &tokens[(task_list_tok + 1) * (i * 2)];
		dump_token(t);
		
		copy_content(buff, &tokens[(task_list_tok + 1) * (i * 2)]);
		new_agent->visit_list[i].region = atoi(buff);

		// Set the action
		t = &tokens[(task_list_tok + 1) * (i * 2) + 1];
		dump_token(t);

		strncpy(buff, &json_config[t->start], t->end - t->start);
		if(strcmp(buff, "Action A") == 0) {
			new_agent->visit_list[i].action = ACTION_A;
		} else if(strcmp(buff, "Action B") == 0) {
			new_agent->visit_list[i].action = ACTION_B;
		} else if(strcmp(buff, "Action C") == 0) {
			new_agent->visit_list[i].action = ACTION_C;
		} else {
			fprintf(stderr, "Unrecognized action: %s\n", buff);
			exit(EXIT_FAILURE);
		}

	}

	out:
	if(new_agent == NULL)
		last_agent_token = -1;

	return new_agent;
}


// This is the core configuration function where the logic to setup the
// simulation state should be implemented.
void region_config(lp_state_t *state, unsigned int me) {
	int idx, start, end;
	char me_str[64];

	// Initialize my state using the general configuration information
	idx = get_token_by_content(0, JSMN_STRING, "general");
	get_token_range(idx + 1, &start, &end);

	idx = get_token_value_in_range(start, end, "name");
	if(idx < 0) {
		fprintf(stderr, "Invalid general configuration\n");
		exit(EXIT_FAILURE);
	}
	copy_content(state->name, &tokens[idx]);

	// Find a (possibly non-existent) specific configuration for me
	snprintf(me_str, 64, "%u", me);

	idx = find_region_config_value("name", me_str);
	if(idx != -1) {
		copy_content(state->name, &tokens[idx]);
	}
} 

void initialize_obstacles(obstacles_t **obstacles) {
	int i;
	int start, end;
	int idx;
	jsmntok_t *t;
	char buff[64];

	// Get the list of obstacles
	idx = get_token_by_content(0, JSMN_STRING, "obstacles") + 1;
	get_token_range(idx, &start, &end);
	
		SetupObstacles(obstacles);
		for(i = start; i <= end; i++) {
		t = &tokens[i];
		strncpy(buff, &json_config[t->start], t->end - t->start);
				AddObstacle(*obstacles, atoi(buff));
				printf("Cell %d is a obstacle\n", atoi(buff));
		}
}



// This is a generic function to load and parse a JSON configuration file.
// It can be re-used verbatim for any ABM simulation model. The logic to
// handle different configurations of the LPs should be implemented in the
// setup_config() function above.
void load_config(void) {
	int r;
	size_t jslen = 0;
	FILE *config;
	jsmn_parser p;

	// Load and parse only once for a given thread.
	// This allows all LPs bound to the same worker to
	// reuse the already-parsed information.
	if(conf_loaded == true)
		return;

	/* Prepare parser */
	jsmn_init(&p);

	// Load configuration file
	config = fopen(CONFIG_FILE, "r");
	if(config == NULL) {
		fprintf(stderr, "Error opening configuration file %s: ", CONFIG_FILE);
		perror("");
		exit(EXIT_FAILURE);
	}

	// Sanity check on the config length
	fseek(config, 0, SEEK_END);
	jslen = ftell(config);
	rewind(config);
	if(jslen >= JSON_LENGTH) {
		fprintf(stderr, "Error: config file is too long. Recompile the model setting JSON_LENGTH to a larger size\n");
		exit(EXIT_FAILURE);
	}


	// Load the whole JSON file from disk
	fread(json_config, jslen, 1, config);
	if(ferror(config)) {
		fprintf(stderr, "Error loading the configuration file: ");
		perror("");
		exit(EXIT_FAILURE);
	}

	// Parse the loaded JSON file
	r = jsmn_parse(&p, json_config, jslen, tokens, JSON_LENGTH/16);
	if (r < 0) {
		if (r == JSMN_ERROR_NOMEM) {
			fprintf(stderr, "Error parsing token: out of memory. Recompile the model increasing JSON_LENGTH.\n");
			exit(EXIT_FAILURE);
		}
	}

	conf_loaded = true;
}

