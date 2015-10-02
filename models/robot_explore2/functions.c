#include "application.h"
#include "agent.h"
#include "region.h"

/**
 * Computes the diff between first and second visit map; therefore
 * it exchanges the computed differences among the two agents.
 *
 * @param agent_a Pointer to the first agent's state
 * @param agent_b Pointer to the second agent's state
 */
void map_diff_exchange(agent_state_type * agent_a, agent_state_type * agent_b) {
	unsigned int index;
	unsigned char *map_a;
	unsigned char *map_b;

	// Iterates all over the possible regions
	for (index = 0; index < number_of_regions; index++) {
		// Checks if there is one region such that is visited
		// by one agent and not by the other

		map_a = agent_a->visit_map;
		map_b = agent_b->visit_map;

		// Exchange map informations and recomputes current frontier
		if (BITMAP_CHECK_BIT(map_a, index) && BITMAP_CHECK_BIT(map_b, index) == 0) {
			BITMAP_SET_BIT(map_b, index);
			agent_b->visited_cells++;
			agent_b->target_cell = closest_frontier(agent_b, -1);
		}
		if (BITMAP_CHECK_BIT(map_a, index) == 0 && BITMAP_CHECK_BIT(map_b, index)) {
			BITMAP_SET_BIT(map_a, index);
			agent_a->visited_cells++;
			agent_a->target_cell = closest_frontier(agent_a, -1);
		}

	}
}
/*void map_diff_exchange(agent_state_type *agent_a, agent_state_type *agent_b) {
	unsigned int index;
	map_t *map_a;
	map_t *map_b;

	// Iterates all over the possible regions
	for (index = 0; index < number_of_regions; index++) {
		// Checks if there is one region such that is visited
		// by one agent and not by the other
		
		map_a = &(agent_a->visit_map[index]);
		map_b = &(agent_b->visit_map[index]);

		// Exchange map informations and recomputes current frontier
		if (map_a->visited && !map_b->visited) {
			memcpy(map_a->neighbours, map_b->neighbours, sizeof(int) * CELL_EDGES);
			map_b->visited = true;
			agent_b->visited_cells++;
			agent_b->target_cell = closest_frontier(agent_b, -1);
			
		} else if (!map_a->visited && map_b->visited) {
			memcpy(map_b->neighbours, map_a->neighbours, sizeof(int) * CELL_EDGES);
			map_a->visited = true;
			agent_a->visited_cells++;
			agent_a->target_cell = closest_frontier(agent_a, -1);
		}

	}
}*/

void dump_agent_knowledge(agent_state_type * agent) {
	int index;

	for (index = 0; index < number_of_regions; index++) {
		printf("%#3d ", index);
	}
	printf("\n");

	for (index = 0; index < number_of_regions; index++) {
		printf("%#3d ", BITMAP_CHECK_BIT(agent->visit_map, index) >> (index % 8));
	}
	printf("\tVisited %d regions\n", agent->visited_cells);
}

void dump_a_star_map(agent_state_type * agent) {
	int index;

	for (index = 0; index < number_of_regions; index++) {
		printf("%#3d ", index);
	}
	printf("\n");

	for (index = 0; index < number_of_regions; index++) {
		printf("%#3d ", BITMAP_CHECK_BIT(agent->a_star_map, index) >> (index % 8));
	}
	printf("\n");
}

void hexdump(void *addr, int len) {
	int i;
	int count;
	unsigned char buff[17];
	unsigned char *pc = (unsigned char *)addr;

	if (len <= 0) {
		return;
	}

	printf("       Address                     Hexadecimal values                      Printable\n");
	printf("   ----------------  ------------------------------------------------  ------------------\n");

	// Process every byte in the data.
	count = (((int)(len / 16) + 1) * 16);
	for (i = 0; i < count; i++) {

		// Multiple of 8 means mid-line (add a mid-space)
		if ((i % 8) == 0) {
			if (i != 0)
				printf(" ");
		}

		if (i < len) {
			// Multiple of 16 means new line (with line offset).
			if ((i % 16) == 0) {
				// Just don't print ASCII for the zeroth line.
				if (i != 0)
					printf(" |%s|\n", buff);

				// Output the offset.
				printf("   (%5d) %08x ", i, i);
			}
			// Now the hex code for the specific character.
			printf(" %02x", pc[i]);

			// And store a printable ASCII character for later.
			if ((pc[i] < 0x20) || (pc[i] > 0x7e))
				buff[i % 16] = '.';
			else
				buff[i % 16] = pc[i];
			buff[(i % 16) + 1] = '\0';
		}
		// Pad out last line if not exactly 16 characters.
		else {
			printf("   ");
			buff[i % 16] = '.';
			buff[(i % 16) + 1] = '\0';
		}
	}

	// And print the final ASCII bit.
	printf("  |%s|\n", buff);
}
