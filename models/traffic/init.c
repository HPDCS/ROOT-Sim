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
* @file init.c
* @brief This module implements the initialization functions
* @author Alessandro Pellegrini
* @date January 12, 2012
*/

#define _POSIX_SOURCE // for strtok_r
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>

#include "application.h"
#include "init.h"

#define parseDouble(s) ({\
			double __value;\
			char *__endptr;\
			__value = strtod(s, &__endptr);\
			if(!(*s != '\0' && *__endptr == '\0')) {\
				fprintf(stderr, "%s:%d: Invalid conversion value: %s\n", __FILE__, __LINE__, s);\
			}\
			__value;\
		       })

static int find_lp_by_name(char *name, FILE *f) {
	char line[LINE_LENGTH];
	char *saveptr;
	int count = 0;

	rewind(f);

	while(fgets(line, LINE_LENGTH, f) != NULL) {
		line[strlen(line) - 1] = '\0'; // Remove trailing '\n'
		if(line[0] == '\0') // The only \n was overwritten with a \0
			continue;

		if(line[0] == '#')
			continue;

		if(strcmp(line, ROUTES_STR) == 0)
			continue;

		if(strcmp(line, INTERSECT_STR) == 0)
			continue;

		if(strcmp(strtok_r(line, ", \t", &saveptr), name) == 0) {
			return count;
		}

		count++;
	}

	fprintf(stderr, "Unable to find intersection named %s\n", name);
	fflush(stderr);
	exit(EXIT_FAILURE);
}


static void tokenize_intersection(char *line, lp_state_type *state) {
	char *token;
	char *source = line;
	char *saveptr;
	int pass = 0;	// This is the tokenization pass

	while((token = strtok_r(source, ", \t", &saveptr)) != NULL) { // space is there to make the parser ignore them

		switch(pass) {

			case 0: // Intersection name
				strcpy(state->name, token);
				source = NULL; // this will make strtok continue parsing the same line
				break;

			case 1: // Car leaving probability
				state->enter_prob = parseDouble(token);
				break;

			case 2: // Car entering probability
				state->leave_prob = parseDouble(token);
				break;

			default:
				fprintf(stderr, "Too many parameters in intersection %s at pass %d\n", state->name, pass);
				fflush(stderr);
				exit(EXIT_FAILURE);
		}

		pass++;
	}
}


static void tokenize_route(char *line, lp_state_type *state, char *name, char from[NAME_LENGTH], char to[NAME_LENGTH]) {
	char *token;
	char *source = line;
	char *saveptr;
	int pass = 0;	// This is the tokenization pass

	while((token = strtok_r(source, ", \t", &saveptr)) != NULL) { // space and newline are there to ignore them

		switch(pass) {

			case 0: // Intersection name
				strcpy(name, token);
				source = NULL; // this will make strtok continue parsing the same line
				break;

			case 1: // To
				strcpy(to, token);
				break;

			case 2: // From
				strcpy(from, token);
				break;

			case 3: // Length
				state->segment_length = parseDouble(token);
				break;

			default:
				fprintf(stderr, "Too many parameters in junction %s at pass %d\n", state->name, pass);
				fflush(stderr);
				exit(EXIT_FAILURE);
		}

		pass++;
	}
}


static bool parse_topology_line(unsigned int me, unsigned int *line_counter, char *line, int *state, lp_state_type *sim_state, FILE *f) {
	char to[NAME_LENGTH];
	char from[NAME_LENGTH];

	// Skip empty lines
	if(line[0] == '\0') // The only \n was overwritten with a \0
		return false;

	// Skip comments
	if(line[0] == '#')
		return false;

	// Change to intersection state
	if(strcmp(line, INTERSECT_STR) == 0) {
		*state = INTERSECT_S;
		return false;
	}

	// Change to route state. At this point we know how many LPs are left, so that we can
	// derive the per-LP road length
	if(strcmp(line, ROUTES_STR) == 0) {
		*state = ROUTES_S;
		return false;
	}

	if(me == *line_counter) {
		// Determine whether we are a route or an interserction
		if(*state == INTERSECT_S) {
			sim_state->lp_type = JUNCTION;
			tokenize_intersection(line, sim_state);
		} else {
			sim_state->lp_type = SEGMENT;
			tokenize_route(line, sim_state, sim_state->name, from, to);

			// A route has only 2 neighbours, one source and one destination
			sim_state->topology = malloc(sizeof(topology_t));
			sim_state->topology->num_neighbours = 2;
			sim_state->topology->neighbours = malloc(sizeof(int) * 2);

			// Find the id of the to/from neighbours
			sim_state->topology->neighbours[0] = find_lp_by_name(from, f);
			sim_state->topology->neighbours[1] = find_lp_by_name(to, f);
		}

		return true;
	}

	// Switch to the next LP
	(*line_counter)++;

	return false;
}


static void connect_junction(lp_state_type *sim_state, FILE *f) {
	int num_neighbours = 0;
	int i = 0;
	char line[LINE_LENGTH];
	char to[NAME_LENGTH];
	char from[NAME_LENGTH];
	char name[NAME_LENGTH];
	int state = NORMAL_S;

	// Count all LPs which have a to/from as the current name
	rewind(f);
	while(fgets(line, LINE_LENGTH, f) != NULL) {
		line[strlen(line) - 1] = '\0'; // Remove trailing '\n'
		if(line[0] == '\0') // The only \n was overwritten with a \0
			continue;

		if(line[0] == '#')
			continue;

		if(strcmp(line, ROUTES_STR) == 0) {
			state = ROUTES_S;
			continue;
		}

		if(strcmp(line, INTERSECT_STR) == 0)
			continue;

		if(state == ROUTES_S) {
			tokenize_route(line, sim_state, name, from, to);

			if(strcmp(from, sim_state->name) == 0) {
				num_neighbours++;
			}

			if(strcmp(to, sim_state->name) == 0) {
				num_neighbours++;
			}
		}
	}

	// Allocate space
	sim_state->topology = malloc(sizeof(topology_t));
	sim_state->topology->num_neighbours = num_neighbours;
	sim_state->topology->neighbours = malloc(sizeof(int) * num_neighbours);

	// Make the actual connections
	for(i = 0; i < num_neighbours; i++) {

		rewind(f);
		while(fgets(line, LINE_LENGTH, f) != NULL) {
			line[strlen(line) - 1] = '\0'; // Remove trailing '\n'
			if(line[0] == '\0') // The only \n was overwritten with a \0
				continue;

			if(line[0] == '#')
				continue;

			if(strcmp(line, ROUTES_STR) == 0) {
				state = ROUTES_S;
				continue;
			}

			if(strcmp(line, INTERSECT_STR) == 0)
				continue;

			if(state == ROUTES_S) {
				tokenize_route(line, sim_state, name, from, to);

				if(strcmp(from, sim_state->name) == 0) {
					sim_state->topology->neighbours[i++] = find_lp_by_name(name, f);
				}

				if(strcmp(to, sim_state->name) == 0) {
					sim_state->topology->neighbours[i++] = find_lp_by_name(name, f);
				}
			}
		}
	}
}


void init_my_state(int me, lp_state_type *sim_state) {
	int i;
	char line[LINE_LENGTH];
	int state = NORMAL_S;

	// This variable keeps track of the line number
	unsigned int line_count = 0;

	// Open the file
	FILE *f = fopen(FILENAME, "r");
	if(f == NULL) {
		perror(FILENAME);
		fflush(stderr);
		exit(EXIT_FAILURE);
	}

	// Now build the topology
	while(fgets(line, LINE_LENGTH, f) != NULL) {
		line[strlen(line) - 1] = '\0'; // Remove trailing '\n'
		if(parse_topology_line(me, &line_count, line, &state, sim_state, f) == true)
			break;

		if(line_count > n_prc_tot) {
			printf("Error: too few logical processes!\n");
			exit(EXIT_FAILURE);
		}
	}

	// Make connections for junctions (1:N). Segments are already connected.
	if(sim_state->lp_type == JUNCTION) {
		connect_junction(sim_state, f);
	}

	fclose(f);

	printf("LP %d (%s) is a%s with ", me, sim_state->name, ( sim_state->lp_type == JUNCTION ? "n intersection" : " route" ));
	if(sim_state->topology != NULL)
		printf("%d neighbours: ", sim_state->topology->num_neighbours);
	else
		printf("0 neighbours\n");
	for(i = 0; i < sim_state->topology->num_neighbours; i++) {
		printf("%d, ", sim_state->topology->neighbours[i]);
		if(sim_state->topology->neighbours[i] >= n_prc_tot)  {
			printf("\nError: too few logical processes!\n");
			exit(EXIT_FAILURE);
		}
	}
	printf("\n");
}
