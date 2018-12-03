#include "application.h"


void ProcessEvent(int me, simtime_t now, int event_type, void *event_content, int event_size, cell_state_t *state) {

	bug_node_t *node;
	int receiver;
	int count;
	int i;

	switch(event_type){

		case INIT:
			if(me == 0){
				state = (cell_state_t *)malloc(sizeof(cell_state_t));
				if(state == NULL){
					printf("Unable to allocate memory for state!\n");
					exit(EXIT_FAILURE);
				}

				node = malloc(sizeof(bug_node_t));
				node->next = NULL;
				node->bug = malloc(sizeof(bug_t) + sizeof(int));
				//how to generate unique ids? for now, there's only 1 bug in cell 0 at the beginning, with ID = 0
				node->bug->uuid = 0;
				node->bug->visited = 1;
				node->bug->cells[node->bug->visited - 1] = me;
				state->bugs = 1;
				state->bug_list = node;
				state->bugs = 0;

				SetState(state);

				for(i = 0; i < 4; i++){
					state->neighbours[i] = 0;
				}
			}

			ScheduleNewEvent(me, now + Expent(TIME_STEP), CELL_OUT, &(node->bug->uuid),sizeof(int));
			break;
		case CELL_IN:
			
			while(state->bug_list->next != NULL){
				if(state->bug_list->bug->uuid == *((int *)event_content))
					node = state->bug_list;
				state->bug_list = state->bug_list->next;
			}
			state->bugs--;
		   	//choose a random direction to take, and get the corresponding c  ell ID (receiver)
			count = 0;
			do{	
				i = (int) RandomRange(0,3);
				receiver = GetReceiver(TOPOLOGY_TORUS, i);
				count++;
			}while(state->neighbours[i] >= BUG_PER_CELL && count < 5);

			ScheduleNewEvent(receiver, now, CELL_IN, node, sizeof(bug_t) + node->visited * sizeof(int));
			free(node);
			free(state->bug_list);


			break;
		case CELL_OUT:

			break;
		default:
			printf("Unrecognized event type\n");
			exit(EXIT_FAILURE);
			break;

	}

}

int OnGVT(unsigned int me, cell_state_t *snapshot){
	return 0;
}
