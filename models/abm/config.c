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


// This function returns the size of a token's content.
// This is useful for sanity checks, e.g. if you have to
// copy some text into an array of chars.
static size_t get_content_size(jsmntok_t *t) {
	return t->end - t->start;
}

// Copy the content of a token into a given buffer. The destination
// buffer must have enough space.
static size_t copy_content(char *dest, jsmntok_t *t) {
	strncpy(dest, &json_config[t->start], t->end - t->start);
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
	printf("Token %d\n", t - tokens);
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
static int get_token_by_content(int parent, int type, char *content) {
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
	int idx, parent;
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

static void find_agent_config(char *agent) {
}


// This is the core configuration function where the logic to setup the
// simulation state should be implemented.
void region_config(unsigned int me) {
	int idx, start, end;
	char me_str[64];

	// Initialize my state using the general configuration information
	idx = get_token_by_content(0, JSMN_STRING, "general");
	get_token_range(idx + 1, &start, &end);

	printf("name: ");
	idx = get_token_value_in_range(start, end, "name");
	if(idx < 0) {
		fprintf(stderr, "Invalid general configuration\n");
		exit(EXIT_FAILURE);
	}
	dump_content(&tokens[idx]);

	// Find a (possibly non-existent) specific configuration for me
	snprintf(me_str, 64, "%u", me);

	idx = find_region_config_value("name", me_str);
	if(idx != -1) {
		printf("name: ");
		dump_content(&tokens[idx]);
	}

	idx = find_region_config_value("", me_str);
	if(idx != -1) {
		printf("name: ");
		dump_content(&tokens[idx]);
	}

	idx = find_region_config_value("name", me_str);
	if(idx != -1) {
		printf("name: ");
		dump_content(&tokens[idx]);
	}
}


// This is a generic function to load and parse a JSON configuration file.
// It can be re-used verbatim for any ABM simulation model. The logic to
// handle different configurations of the LPs should be implemented in the
// setup_config() function above.
void load_config(unsigned int me) {
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
		fprintf(stderr, "Error: %s is too long. Recompile the model setting JSON_LENGTH to a larger size\n");
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

int main(void) {
	load_config(0);
	return 0;
}

