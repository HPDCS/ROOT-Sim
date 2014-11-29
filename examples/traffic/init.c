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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "application.h"
#include "init.h"




static int total_model_road = 0;


static int find_lp_by_name(char *name, simulation_objects *objs) {
	int i;

	for(i = 0; i < objs->intersections; i++) {
		if(strcmp(name, objs->LPs[i]->name) == 0) {
			return i;
		}
	}

	fprintf(stderr, "Unable to find intersection named %s\n", name);
	fflush(stderr);
	exit(EXIT_FAILURE);
}



static void connect_lps(int lp1, int lp2, simulation_objects *objs) {
	neighbour_t *n;

	// Create nodes for the neighbours.
	// n1 is for lp1, n2 is for lp2
	neighbour_t *n1 = malloc(sizeof(neighbour_t));
	n1->lp = lp2;
	n1->next = NULL;

	neighbour_t *n2 = malloc(sizeof(neighbour_t));
	n2->lp = lp1;
	n2->next = NULL;


	// Connect n1 to lp1
	if(objs->LPs[lp1]->neighbours == NULL) {
		objs->LPs[lp1]->neighbours = n1;
	} else {
		n = objs->LPs[lp1]->neighbours;
		while(n->next != NULL) {
			n = n->next;
		}
		n->next = n1;
	}

	// Connect n2 to lp2
	if(objs->LPs[lp2]->neighbours == NULL) {
		objs->LPs[lp2]->neighbours = n2;
	} else {
		n = objs->LPs[lp2]->neighbours;
		while(n->next != NULL) {
			n = n->next;
		}
		n->next = n2;
	}

	// Each lp has one neighbour more now
	objs->LPs[lp1]->num_neighbours++;
	objs->LPs[lp2]->num_neighbours++;
}



static void tokenize_intersection(char *line, simulation_objects *objs) {
	char *token;
	char *source = line;
	int pass = 0;	// This is the tokenization pass

	lp_t *lp = malloc(sizeof(lp_t));

	while((token = strtok(source, ", \t")) != NULL) { // space is there to make the parser ignore them

		switch(pass) {

			case 0: // Intersection name
				strcpy(lp->name, token);
				source = NULL; // this will make strtok continue parsing the same line
				break;

			case 1: // Car leaving probability
				lp->enter_prob = parseDouble(token);
				break;

			case 2: // Car entering probability
				lp->leave_prob = parseDouble(token);
				break;

			default:
				fprintf(stderr, "Too many parameters in intersection %s at pass %d\n", lp->name, pass);
				fflush(stderr);
				exit(EXIT_FAILURE);
		}

		pass++;
	}

	// It's a junction
	lp->lp_type = JUNCTION;

	objs->LPs[objs->curr_lp++] = lp;
	objs->intersections++;
}


static void tokenize_route(char *line, simulation_objects *objs) {
	int i;
	char *token;
	char *source = line;
	int pass = 0;	// This is the tokenization pass
	lp_t *lp;
	int first_lp_in_segment;

	// A route can be split into more than one LP, so we have to store the
	// information here and create LP information later
	char name[NAME_LENGTH];
	char to[NAME_LENGTH];
	char from[NAME_LENGTH];
	double length;
	int num_lps;

	// Did we have to add one LP due to rounding in the previous step?
	static int extra_lp = 0;

	while((token = strtok(source, ", \t")) != NULL) { // space and newline are there to ignore them

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
				length = parseDouble(token);
				break;

			default:
				fprintf(stderr, "Too many parameters in junction %s at pass %d\n", name, pass);
				fflush(stderr);
				exit(EXIT_FAILURE);
		}

		pass++;
	}

	// How many LPs do we need to simulate this segment?
	// TODO: Qualcosa non mi torna nei sanity check, eppure funziona...
	num_lps = (int)rint((double)length / objs->road_per_lp);
	if(num_lps > 1 && extra_lp == 1) {
		num_lps -= 1;
		extra_lp = 0;
	}
	if(num_lps == 0) {
		num_lps = extra_lp = 1;
	}

	// FIXME: note that if one segment comes out to require exactly x.5 LPs, we might generate an inaccurate number of LPs

	// Store the first LP in the segment
	first_lp_in_segment = objs->curr_lp;

	// Generate LPs
	for(i = 0; i < num_lps; i++) {

		lp = malloc(sizeof(lp_t));
		bzero(lp, sizeof(lp_t));
		lp->lp_type = SEGMENT;
		sprintf(lp->name, "%s#%d", name, i);

		objs->LPs[objs->curr_lp++] = lp;

		// we have to connect edges starting from the second one
		if(i > 0) {
			connect_lps(objs->curr_lp - 2, objs->curr_lp - 1, objs); // off-by-one: curr_lp was incremented already
		}
	}

	// Connect edges to intersections
	i = find_lp_by_name(from, objs);
	connect_lps(i, first_lp_in_segment, objs);
	i = find_lp_by_name(to, objs);
	connect_lps(i, objs->curr_lp - 1, objs); // off-by-one again
}


static void parse_topology_line(char *line, int *state, simulation_objects *objs) {

	// Skip empty lines
	if(line[0] == '\0') // The only \n was overwritten with a \0 // FIXME: what if there are leftover spaces?
		return;

	// Skip comments
	if(line[0] == '#')
		return;

	// Change to intersection state
	if(strcmp(line, INTERSECT_STR) == 0) {
		*state = INTERSECT_S;
		return;
	}

	// Change to route state. At this point we know how many LPs are left, so that we can
	// derive the per-LP road length
	if(strcmp(line, ROUTES_STR) == 0) {
		*state = ROUTES_S;

		objs->road_per_lp = ((double)total_model_road / (double)(n_prc_tot - objs->curr_lp + 1));

		// Sanity check
		if(objs->road_per_lp == 0) {
			fprintf(stderr, "Error: too many LPs for the given routes. One LP must at least simulate one unit of length!\n");
			fflush(stderr);
			exit(EXIT_FAILURE);
		}


		return;
	}
	
	// Parse the line and create the LP information
	switch(*state) {
		case INTERSECT_S:
			tokenize_intersection(line, objs);
			break;

		case ROUTES_S:
			tokenize_route(line, objs);
			break;

		case NORMAL_S:
		default:
			fprintf(stderr, "Parsing topology in an unexpected state line: %s\n", line);
			fflush(stderr);
			exit(EXIT_FAILURE);
	}

}


static void print_adjacencies(int me, topology_t *t, char *n) {
	int i;

	printf("%s (%04d): ", n, me);
	
	for(i = 0; i < t->num_neighbours; i++) {
		printf("%d, ", t->neighbours[i]);
	}

	printf("\n");
	fflush(stdout);
}



static simulation_objects *allocate_LPs(void) {
	
	simulation_objects *objs = malloc(sizeof(simulation_objects));
	bzero(objs, sizeof(simulation_objects));

	objs->LPs = malloc(sizeof(lp_t *) * n_prc_tot);
	bzero(objs->LPs, sizeof(lp_t *) * n_prc_tot);

	return objs; 
}


static void deallocate_LPs(simulation_objects *objs) {
	int i;
	neighbour_t *n1, *n2;

	for(i = 0; i < n_prc_tot; i++) {
		n1 = objs->LPs[i]->neighbours;

		while(n1 != NULL) {
			n2 = n1->next;
			free(n1);
			n1 = n2;
		}
	}

	free(objs->LPs);
	free(objs);
}


static void compute_total_road(FILE *f) {
	char line[LINE_LENGTH];
	void *ret;
	char *token;
	char *source;
	int i;

	bzero(line, LINE_LENGTH);
	
	// Skip lines until we get to the [ROUTES] section
	do {
		ret = fgets(line, LINE_LENGTH, f);
		line[strlen(line) - 1] = '\0'; // Remove trailing '\n'
	} while(ret != NULL && strcmp(line, ROUTES_STR) != 0);

	if(ret == NULL) {
		fprintf(stderr, "Error: unable to find the %s section in the file topology file\n", ROUTES_STR);
		fflush(stderr);
		exit(EXIT_FAILURE);
	}

	// Parse remaining lines and sum the length arguments
	while(fgets(line, LINE_LENGTH, f) != NULL) {

		line[strlen(line) - 1] = '\0'; // Remove trailing '\n'		

		// Check comments and blank lines
		if(line[0] == '#' || line[0] == '\0') {
			continue;
		}

		// Get to the length argument in the line
		source = line;
	
		for(i = 0; i < LENGTH_ARG; i++) {

			token = strtok(source, ", \t");

			if(token == NULL) {
				fprintf(stderr, "Error: too few arguments for the %s entry: %s\n", ROUTES_STR, line);
				fflush(stderr);
				exit(EXIT_FAILURE);
			}

			if(source == line) {
				source = NULL; // To continue tokenizing the string
			}
		}

		// sum it up
		total_model_road += parseInt(token);		

	}
	
}


void init_my_state(int me, lp_state_type *sym_state) {
	int i;
	char line[LINE_LENGTH];
	int state = NORMAL_S;
	simulation_objects *objs = allocate_LPs();
	lp_t *my_lp;
	topology_t *topology;
	neighbour_t *n;

	
	// Open the file
	FILE *f = fopen(FILENAME, "r");
	if(f == NULL) {
		perror(FILENAME);
		fflush(stderr);
		exit(EXIT_FAILURE);
	}

	// Compute the total amount of road to be simulated
	total_model_road = 0;
	compute_total_road(f);

	if(me == 0) {
		printf("Total Model's road length = %d\n", total_model_road);
	}

	// Now build the topology
	fseek(f, 0L, SEEK_SET);
	while(fgets(line, LINE_LENGTH, f) != NULL) {
		line[strlen(line) - 1] = '\0'; // Remove trailing '\n'
		parse_topology_line(line, &state, objs);
		bzero(line, LINE_LENGTH);
	}

	fclose(f);

	// Now convert the information into the one used by the simulation states

	my_lp = objs->LPs[me];

	topology = malloc(sizeof(topology_t));
	topology->num_neighbours = my_lp->num_neighbours;

	if(my_lp->num_neighbours > 0) { // sanity check!
		topology->neighbours = malloc(sizeof(int) * my_lp->num_neighbours);

		i = 0;
		n = my_lp->neighbours;
		while(n != NULL) {
			topology->neighbours[i++] = n->lp;
			n = n->next;
		}
	}

	if(me == 0) {
		printf("Initialized %d LPs\n", objs->curr_lp);
	}


	// Store the information into the simulation state
	sym_state->topology = topology;
	strcpy(sym_state->name, my_lp->name);
	sym_state->lp_type = my_lp->lp_type;
	sym_state->enter_prob = my_lp->enter_prob;
	sym_state->leave_prob = my_lp->leave_prob;

	if(sym_state->lp_type == JUNCTION) {
		sym_state->enter_prob = my_lp->enter_prob;
		sym_state->leave_prob = my_lp->leave_prob;
	} else if(sym_state->lp_type == SEGMENT) {
		sym_state->segment_length = objs->road_per_lp;
	} else {
		fprintf(stderr, "Error: unknown LP type\n");
		fflush(stderr);
		exit(EXIT_FAILURE);
	}
	

	// Print information on the current LP's topology
	//print_adjacencies(me, topology, my_lp->name);


	// free a lot of memory!
	deallocate_LPs(objs);

}
