
#define OBSTACLE_PROB	0.1

#define KEEP_ALIVE	103

#define KEEP_ALIVE_INTERVAL (now + (simtime_t)Expent(50))

void CellProcessEvent(int me, simtime_t now, int event_type, void *event_content, int event_size, void *pointer) {

	cell_state_type *state;
	int i;

	state = (cell_state_type *) pointer;

	switch (event_type) {

	case INIT:
		pointer = malloc(sizeof(cell_state_type));
		if (pointer == NULL) {
			rootsim_error(true, "Error allocating cell %d state!\n", me);
		}
		SetState(pointer);

		bzero(pointer, sizeof(cell_state_type));

		state = (cell_state_type *) pointer;

//                      state->has_obstacles = true;
		// Set the values for neighbours. If one is non valid, then set it to -1
		for (i = 0; i < 6; i++) {
			if (isValidNeighbour(me, i)) {
				// With a random probability, an obstacle
				// prevents any robot from getting there
				if (!state->has_obstacles && Random() < OBSTACLE_PROB) {
					state->has_obstacles = true;
					state->neighbours[i] = -1;
				} else {
					state->neighbours[i] = GetNeighbourId(me, i);
				}
			} else {
				state->neighbours[i] = -1;
			}
		}

		// Allocate the presence bitmap
		state->agents = ALLOCATE_BITMAP(n_prc_tot - num_cells);
		bzero(state->agents, BITMAP_SIZE(n_prc_tot - num_cells));

		states[me] = pointer;
		ScheduleNewEvent(me, KEEP_ALIVE_INTERVAL, KEEP_ALIVE, NULL, 0);

		break;

	case KEEP_ALIVE:
		ScheduleNewEvent(me, KEEP_ALIVE_INTERVAL, KEEP_ALIVE, NULL, 0);
		break;

	default:
		abort();
	}
}

CellOnGVT(unsigned int me, cell_state_type * state) {
	int i;

//      printf("Presence bitmap for %d: ", me);

//      for(i = 0; i < n_prc_tot - num_cells; i++) {
//              printf("%d", (CHECK_BIT(state->agents, i) > 0));
//      }
//      printf("\n");

	return true;
}
