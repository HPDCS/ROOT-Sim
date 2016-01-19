/**
*			Copyright (C) 2008-2015 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @file binding.c
* @brief Implements load sharing rules for LPs among worker threads
* @author Alessandro Pellegrini
*/

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <arch/atomic.h>
#include <core/core.h>
#include <core/timer.h>
#include <datatypes/list.h>
#include <scheduler/process.h>
#include <scheduler/binding.h>
#include <statistics/statistics.h>
#include <gvt/gvt.h>

#include <mm/allocator.h>
#include <arch/thread.h>


#define REBIND_INTERVAL 10.0


struct lp_cost_id {
	double workload_factor;
	unsigned int id;
};

struct lp_cost_id *lp_cost;



/// A guard to know whether this is the first invocation or not
static __thread bool first_lp_binding = true;

/** Each KLT has a binding towards some LPs. This is the structure used
 *  to keep track of LPs currently being handled
 */
__thread LP_state **LPS_bound = NULL;

/** TODO MN
 *  Each KLT has a binding towards some GLP. This is the structure used 
 *  to keep track of GLPs currently being handled
 */
#ifdef HAVE_GLP_SCH_MODULE
static double *glp_cost;
//For each group it tells us which will be the future core
static unsigned int *new_GLPS_binding;
static GLP_state **new_GLPS;
//For updating the current_group stored inside LP_state according to the new configuration
static unsigned int *new_group_LPS;
static unsigned int empty_value;
#endif
static int binding_acquire_phase = 0;
static __thread int local_binding_acquire_phase = 0;

static unsigned int *new_LPS_binding;
static timer rebinding_timer;
static int binding_phase = 0;
static __thread int local_binding_phase = 0;

static atomic_t worker_thread_reduction;


/**
* Performs a (deterministic) block allocation between LPs and KLTs
*
* @author Alessandro Pellegrini
*/
static inline void LPs_block_binding(void) {
	unsigned int i, j;
	unsigned int buf1;
	unsigned int offset;
	unsigned int block_leftover;

	buf1 = (n_prc / n_cores);
	block_leftover = n_prc - buf1 * n_cores;

	if (block_leftover > 0) {
		buf1++;
	}

	n_prc_per_thread = 0;
	i = 0;
	offset = 0;

	while (i < n_prc) {
		j = 0;
		while (j < buf1) {
			if(offset == tid) {
				LPS_bound[n_prc_per_thread++] = LPS[i];
				LPS[i]->worker_thread = tid;
			}
			i++;
			j++;
		}
		offset++;
		block_leftover--;
		if (block_leftover == 0) {
			buf1--;
		}
	}
}

/**
* Convenience function to compare two elements of struct lp_cost_id.
* This is used for sorting the LP vector in LP_knapsack()
*
* @author Alessandro Pellegrini
*
* @param a Pointer to the first element
* @param b Pointer to the second element
*
* @return The comparison between a and b
*/
static int compare_lp_cost(const void *a, const void *b) {
	struct lp_cost_id *A = (struct lp_cost_id *)a;
	struct lp_cost_id *B = (struct lp_cost_id *)b;

	return ( B->workload_factor - A->workload_factor );
}

/**
* Implements the knapsack load sharing policy in:
*
* Roberto Vitali, Alessandro Pellegrini and Francesco Quaglia
* A Load Sharing Architecture for Optimistic Simulations on Multi-Core Machines
* In Proceedings of the 19th International Conference on High Performance Computing (HiPC)
* Pune, India, IEEE Computer Society, December 2012.
*
*
* @author Alessandro Pellegrini
*/
static inline void LP_knapsack(void) {
	register unsigned int i, j;
	double reference_knapsack = 0;
//	double reference_lvt;
	bool assigned;
	double assignments[n_cores];

	if(!master_thread())
		return;

	// Estimate the reference knapsack
	for(i = 0; i < n_prc; i++) {
		reference_knapsack += lp_cost[i].workload_factor;
	}
	reference_knapsack /= n_cores;

	// Sort the expected times
	qsort(lp_cost, n_prc, sizeof(struct lp_cost_id) , compare_lp_cost);


	// At least one LP per thread
	bzero(assignments, sizeof(double) * n_cores);
	j = 0;
	for(i = 0; i < n_cores; i++) {
		assignments[j] += lp_cost[i].workload_factor;
		new_LPS_binding[i] = j;
		j++;
	}

	// Very suboptimal approximation of knapsack
	for(; i < n_prc; i++) {
		assigned = false;

		for(j = 0; j < n_cores; j++) {
			// Simulate assignment
			if(assignments[j] + lp_cost[i].workload_factor <= reference_knapsack) {
				assignments[j] += lp_cost[i].workload_factor;
				new_LPS_binding[i] = j;
				assigned = true;
				break;
			}
		}

		if(assigned == false)
			break;
	}

	// Check for leftovers
	if(i < n_prc) {
		j = 0;
		for( ; i < n_prc; i++) {
			new_LPS_binding[i] = j;
			j = (j + 1) % n_cores;
		}
	}

	printf("NEW BINDING\n");
	for(j = 0; j < n_cores; j++) {
		printf("Thread %d: ", j);
		for(i = 0; i < n_prc; i++) {
			if(new_LPS_binding[i] == j)
				printf("%d ", i);
		}
		printf("\n");
	}

}


static void post_local_reduction(void) {
	unsigned int i;
	unsigned int lid;

	for(i = 0; i < n_prc_per_thread; i++) {
		lid = LPS_bound[i]->lid;

		lp_cost[lid].id = i;
		lp_cost[lid].workload_factor = list_sizeof(LPS[lid]->queue_in);
		lp_cost[lid].workload_factor *= statistics_get_data(STAT_GET_EVENT_TIME_LP, lid);
		lp_cost[lid].workload_factor /= ( list_tail(LPS[lid]->queue_in)->timestamp - list_head(LPS[lid]->queue_in)->timestamp );
	}
}

#ifndef HAVE_GLP_SCH_MODULE
static void install_binding(void) {
	unsigned int i;

	bzero(LPS_bound, sizeof(LP_state *) * n_prc);
	n_prc_per_thread = 0;

	for(i = 0; i < n_prc; i++) {
		if(new_LPS_binding[i] == tid) {
			LPS_bound[n_prc_per_thread++] = LPS[i];

			if(tid != LPS[i]->worker_thread) {

				#ifdef HAVE_NUMA
				move_request(i, get_numa_node(running_core()));
				#endif

				LPS[i]->worker_thread = tid;
			}
		}
	}
}
#endif

#ifdef HAVE_GLP_SCH_MODULE
/* -------------------------------------------------------------------- */
/* -------------------START MANAGE GROUP------------------------------- */
/* -------------------------------------------------------------------- */

/**
//TODO MN
* Performs a (deterministic) block allocation between GLPs LPs and KLTs
*
* @author Nazzareno Marziale
* @author Francesco Nobilia
*/
static inline void GLPs_block_binding(void) {
	unsigned int i, j, count;
	unsigned int buf1;
	unsigned int offset;
	unsigned int block_leftover;

	empty_value = n_prc + 2;

	buf1 = (n_prc / n_cores);
	block_leftover = n_prc - buf1 * n_cores;

	if (block_leftover > 0) {
		buf1++;
	}

	n_prc_per_thread = 0;
	i = 0;
	offset = 0;
	while (i < n_prc) {
		j = 0;
		while (j < buf1) {
			if(offset == tid) {
				spin_lock_x86(&(GLPS[i]->lock));
				count = 0;
				LP_state **list = GLPS[i]->local_LPS;
				while(count < GLPS[i]->tot_LP){
					LPS_bound[n_prc_per_thread++] = GLPS[i]->local_LPS[count];
					list[count]->worker_thread = tid;
					count++;
				}
				spin_unlock_x86(&(GLPS[i]->lock));
			}
			i++;
			j++;
		}
		offset++;
		block_leftover--;
		if (block_leftover == 0) {
			buf1--;
		}
	}
}

/**
//TODO MN
* Convenience function to compute the workload of a GRP.
* This is used for preparing the array that will be sorted in GLP_knapsack()
*
* @author Nazzareno Marziale
* @author Francesco Nobilia
*
* @param int Group id
*
* @return The workload of group id
*/
static double compute_workload_GLP(unsigned int id) {
	glp_cost[id] = 0;

	GLP_state *group = new_GLPS[id];

	unsigned int count;
	LP_state **list = group->local_LPS;
	for(count = 0;count < group->tot_LP;count++){
		glp_cost[id] += lp_cost[list[count]->lid].workload_factor;
	}

	return glp_cost[id];
}

/**
//TODO MN
* Convenience function to compare two elements of double.
* This is used for sorting the GROUP vector in GLP_knapsack()
*
* @author Nazzareno Marziale
* @author Francesco Nobilia
*
* @param a Pointer to the first element
* @param b Pointer to the second element
*
* @return The comparison between a and b
*/
static int compare_glp_cost(const void *a, const void *b) {
	return  (*(double*)b - *(double*)a) ;
}

/**
//TODO MN
**/
int LP_change_group(GLP_state **GROUPS_global, unsigned int actual_lp,int n, unsigned int (*statistics)[n], bool force_change){
	unsigned int group_index = statistics[actual_lp][1];
		
	if(force_change || LPS[actual_lp]->current_group != group_index){
		
		//Update old group VERIFICA
		remove_lp_group(GROUPS_global[LPS[actual_lp]->current_group],actual_lp);
					
		//Update new group
		insert_lp_group(GROUPS_global[group_index],actual_lp);
	}

	return group_index;
}

/**
//TODO MN
* Updates all groups configurations according to LPs' statistics
*
* @author Nazzareno Marziale
* @author Francesco Nobilia
*/
static void analise_static_group(int n, unsigned int (*statistics)[n]){
	unsigned int temp_max_access = 0, i=0, j=0;
	ECS_stat *ECS_entry_temp;	
	ECS_stat **actual_lp_table;	

	for(i=0 ; i<n_prc ; i++){
		statistics[i][0] = empty_value;
		statistics[i][1] = empty_value;
		actual_lp_table = LPS[i]->ECS_stat_table;
		// Looking for my new groupmate
		for(j=0; j<n_prc;j++){
                	ECS_entry_temp = actual_lp_table[j];
                	if(ECS_entry_temp->count_access > THRESHOLD_ACCESS_ECS  && ECS_entry_temp->count_access > temp_max_access){
				statistics[i][0] = j;
				temp_max_access =  ECS_entry_temp->count_access;
			}
			// Reset statistics ECS if the my lvt is bigger than last_access plus THRESHOLD
			if(ECS_entry_temp->last_access + THRESHOLD_TIME_ECS < lvt(i)){
				
				ECS_entry_temp->last_access = lvt(i);
				ECS_entry_temp->count_access = 0;
			}	
		}
		// Reset statistics ECS of my new groupmate
		if(statistics[i][0]!=empty_value){
			ECS_entry_temp = actual_lp_table[statistics[i][0]];
			ECS_entry_temp->last_access = lvt(i);
                        ECS_entry_temp->count_access = 0;

		}
						
		temp_max_access = 0;	
	}
}

/**
//TODO MN
* Updates all groups configurations according to LPs' statistics
*
* @author Nazzareno Marziale
* @author Francesco Nobilia
*/
static unsigned int clustering_groups(int n, unsigned int (*statistics)[n], unsigned int lid, unsigned int group){
	if(statistics[lid][1] != empty_value)
		return statistics[lid][1];
	
	if(group != empty_value)
                statistics[lid][1] = group;
	else
		statistics[lid][1] = lid;
		
	if(statistics[lid][0] != empty_value){
		statistics[lid][1] = clustering_groups(DIM_STAT_GROUP,statistics,statistics[lid][0],statistics[lid][1]);
	}
		
	return statistics[lid][1];
}

/**
//TODO MN
* 
*
* @author Nazzareno Marziale
* @author Francesco Nobilia
*/
static void check_num_group_over_core(int n, unsigned int (*statistics)[n]){
	unsigned int i,count=0;
	for(i=0;i<n_grp;i++)
		if(new_GLPS[i]->tot_LP > 0) count++;
	
	if (count >= n_cores) return;
	
	for(i=0;i<n_prc;i++){
		if(statistics[i][1]!=i && new_GLPS[i]->tot_LP == 0){
			
			//Update old group
			remove_lp_group(new_GLPS[statistics[i][1]],i);

                	//Update new group
			insert_lp_group(new_GLPS[i],i);

			statistics[i][1] = i;
			new_group_LPS[i] = i;	
			count++;
			
			printf("Move %d\n",i);
		}
		
		if (count >= n_cores) return;
	}
	
}


/**
//TODO MN
* 
*
* @author Nazzareno Marziale
* @author Francesco Nobilia
*/
static void set_group_bound(void){
        unsigned int i,j;
	GLP_state *temp_GLPS;
	LP_state *temp_LP;
	msg_t *result_evt;

        for(i=0;i<n_grp;i++){
		if(new_GLPS_binding[i] == tid){
			temp_GLPS = new_GLPS[i];
			for(j=0;j<temp_GLPS->tot_LP;j++){
				temp_LP = temp_GLPS->local_LPS[j];
				
				if(list_next(temp_LP->bound)!=NULL){
					result_evt = list_next(temp_LP->bound);
				}
				else{
					result_evt = temp_LP->bound;
				}
				
				if(temp_GLPS->initial_group_time->timestamp <= result_evt->timestamp){
					update_IGT(temp_GLPS->initial_group_time,result_evt);
					temp_GLPS->lvt = result_evt;
				}
				
			}
			if(D_EQUAL(temp_GLPS->initial_group_time->timestamp,-1.0) && temp_GLPS->tot_LP > 0)
				rootsim_error(true,"Errore IGT");
				
			temp_GLPS->counter_rollback = 0;
			temp_GLPS->counter_synch = 0;
			temp_GLPS->counter_log = 0;
			temp_GLPS->counter_silent_ex = 0;
			temp_GLPS->ckpt_period = temp_GLPS->tot_LP * CKPT_PERIOD_GROUP;
			temp_GLPS->from_last_ckpt = 0;
			temp_GLPS->state = GLP_STATE_WAIT_FOR_GROUP;
		}
	}
}

/**
//TODO MN
* Updates all groups configurations according to LPs' statistics
*
* @author Nazzareno Marziale
* @author Francesco Nobilia
*/
static void update_clustering_groups(void){
	unsigned int i;
	unsigned int statistics[n_prc][DIM_STAT_GROUP];


	//Create a graph thanks to ECS statistics
	analise_static_group(DIM_STAT_GROUP,statistics);

	//Recursive function that update the clustering info
	for(i=0; i<n_prc; i++)
		clustering_groups(DIM_STAT_GROUP,statistics,i,empty_value);
	
	for(i=0; i<n_prc; i++){
		new_group_LPS[i] = LP_change_group(new_GLPS, i, DIM_STAT_GROUP, statistics,false);
	}
	
	//Check if the number of groups is at least the number of cores
	check_num_group_over_core(DIM_STAT_GROUP,statistics);

}

/**
//TODO MN
* Implements the knapsack load sharing policy according to the LP case
*
* @author Nazzareno Marziale
* @author Francesco Nobilia
*/

static inline void GLP_knapsack(void) {
	register unsigned int i, j;
	double reference_knapsack = 0;
	bool assigned;
	double assignments[n_cores];

	if(!master_thread())
		return;
	
	// Clustering group
//	if(need_clustering()){
		update_clustering_groups();
//	}	

	// Estimate the reference knapsack
	for(i = 0; i < n_grp; i++) {
		reference_knapsack += compute_workload_GLP(i);
	}
	reference_knapsack /= n_cores;

	// Sort the expected times
	qsort(glp_cost, n_grp, sizeof(double) , compare_glp_cost);


	// At least one GLP per thread
	bzero(assignments, sizeof(double) * n_cores);
	j = 0;
	for(i = 0; i < n_grp; i++) {
		if(new_GLPS[i]->tot_LP > 0){
			assignments[j] += glp_cost[i];
			new_GLPS_binding[i] = j;
			j++;
		
			if(j==n_cores){
				i++;
                        	break;
			}
		}
	}

	if(j!=n_cores)
		rootsim_error(true,"Number of group less than number of cores. Aborting...");

	// Very suboptimal approximation of knapsack
	for(; i < n_grp; i++) {
		assigned = false;
		for(j = 0; j < n_cores; j++) {
			// Simulate assignment
			if(assignments[j] + glp_cost[i] <= reference_knapsack) {
				assignments[j] += glp_cost[i];
				new_GLPS_binding[i] = j;
				assigned = true;
				i++;

				if(i==n_grp)	break;
			}
		}

		if(assigned == false)
			break;
	}

	// Check for leftovers
	if(i < n_grp) {
		j = 0;
		for( ; i < n_grp; i++) {
			new_GLPS_binding[i] = j;
			j = (j + 1) % n_cores;
		}
	}

	//NOTE: Populating here the new_LPS_binging is not secure since there may be a WT that after the update of 
	//      new_LPS_bingind moves a LP from one group towards another one
	//      TODO in install_binding

	printf("NEW GROUP BINDING\n");
	for(j = 0; j < n_cores; j++) {
		printf("Thread %d: ", j);
		for(i = 0; i < n_grp; i++) {
			if(new_GLPS_binding[i] == j && new_GLPS[i]->tot_LP>0)
				printf("%d ", i);
		}
		printf("\n");
	}
	PRINT_DEBUG_GLP{
	for(j = 0; j < n_prc; j++) {
                printf("LP[%d]-G[%d]\n",j,new_group_LPS[j]);
        }
	}


}


/** 
//TODO MN
* Send control message to groupmate in order to syncronize the start of group
*
* @author Nazzareno Marziale
* @author Francesco Nobilia
*/
static void send_control_group_message(void) {
	unsigned int i,j,lp_index = 0;
	GLP_state *temp_GLPS;
	LP_state **list;
        msg_t control_msg;	

		
	for(i=0;i<n_grp;i++){
		if(new_GLPS_binding[i] == tid){
			temp_GLPS = new_GLPS[i];
			list = temp_GLPS->local_LPS;
		
			// TODO This check is to avoid that a SYNCH_GROUP message has a timestamp
			// bigger than the group end time. Check if it is possible to avoid this 
			// situation in another way.
			if(temp_GLPS->initial_group_time->timestamp > future_end_group())
				continue;

			for(j=0;j<temp_GLPS->tot_LP;j++){
				lp_index = list[j]->lid;
				
				// Diretcly place the control message in the target bottom half queue
				bzero(&control_msg, sizeof(msg_t));
				control_msg.sender = LidToGid(i);
				control_msg.receiver = LidToGid(lp_index);
				control_msg.type = SYNCH_GROUP;
				control_msg.timestamp = temp_GLPS->initial_group_time->timestamp;
				control_msg.send_time = temp_GLPS->initial_group_time->timestamp;
				control_msg.message_kind = positive;
				control_msg.mark = generate_mark(i);
				Send(&control_msg);
			
				if(lp_index == temp_GLPS->initial_group_time->receiver){
					update_IGT(temp_GLPS->initial_group_time,&control_msg);
				}		
				PRINT_DEBUG_GLP{	
					printf("SENDED SYNCH MESSAGE TO %d timestamp:%f FAG:%f \n",lp_index,temp_GLPS->initial_group_time->timestamp,future_end_group());
				}

				//Useful to take a log at the end the group execution otherwise an ECS may be executed in silent mode
				bzero(&control_msg, sizeof(msg_t));
				control_msg.sender = LidToGid(i);
				control_msg.receiver = LidToGid(lp_index);
				control_msg.type = CLOSE_GROUP;
				control_msg.timestamp = get_last_gvt() + get_delta_group();
				control_msg.send_time = get_last_gvt() + get_delta_group();
				control_msg.message_kind = positive;
				control_msg.mark = generate_mark(i);
				Send(&control_msg);
				
				lp_index++;
			}
			lp_index=0;	
		}	
	}
	
	
}



/** 
//TODO MN
* Populates the LPS_bound according the group concept
*
* @author Nazzareno Marziale
* @author Francesco Nobilia
*/
static void install_GLPS_binding(void) {
	unsigned int i;

	bzero(LPS_bound, sizeof(LP_state *) * n_prc);
	n_prc_per_thread = 0;

//	send_control_group_message(); //TODO check if it is correct remove from here

	for(i = 0; i < n_prc; i++) {
		if(new_GLPS_binding[new_group_LPS[i]] == tid){
			LPS_bound[n_prc_per_thread++] = LPS[i];

			if(tid != LPS[i]->worker_thread) {

				#ifdef HAVE_NUMA
				move_request(i, get_numa_node(running_core()));
				#endif

				LPS[i]->worker_thread = tid;
			}
		}
	}
}

/** 
//TODO MN
* Copy the informations of new_GLPS into global GLPS
*
* @author Nazzareno Marziale
* @author Francesco Nobilia
*/
static void switch_GLPS(void){
	unsigned int i;
	
	for (i = 0; i < n_grp; i++) {
		if(new_GLPS_binding[i] == tid){
			GLPS[i]->id = new_GLPS[i]->id;
			memcpy(GLPS[i]->local_LPS, new_GLPS[i]->local_LPS, n_prc * sizeof(LP_state *));
			GLPS[i]->tot_LP = new_GLPS[i]->tot_LP;
			update_IGT(GLPS[i]->initial_group_time,new_GLPS[i]->initial_group_time);
			reset_IGT(new_GLPS[i]->initial_group_time);
			GLPS[i]->state = new_GLPS[i]->state;
			new_GLPS[i]->state = GLP_STATE_WAIT_FOR_GROUP;
			GLPS[i]->lvt = new_GLPS[i]->lvt;
			new_GLPS[i]->lvt = NULL;
			GLPS[i]->counter_rollback = new_GLPS[i]->counter_rollback;
			new_GLPS[i]->counter_rollback = 0;
			GLPS[i]->counter_silent_ex = new_GLPS[i]->counter_silent_ex;
			new_GLPS[i]->counter_silent_ex = 0;
			GLPS[i]->from_last_ckpt = new_GLPS[i]->from_last_ckpt;
			new_GLPS[i]->from_last_ckpt = 0;
			GLPS[i]->ckpt_period = new_GLPS[i]->ckpt_period;
			GLPS[i]->counter_synch = new_GLPS[i]->counter_synch;	
			new_GLPS[i]->counter_synch = 0;	
			GLPS[i]->counter_log = new_GLPS[i]->counter_log;	
			new_GLPS[i]->counter_log = 0;
		}	
	}

	for (i = 0; i < n_prc; i++)
		LPS[i]->current_group = new_group_LPS[i];

	update_last_time_group();

}

void rebind_LPs(void) {

	if(first_lp_binding) {
		first_lp_binding = false;

		// Binding metadata are used in the platform to perform
		// operations on LPs in isolation
		LPS_bound = rsalloc(sizeof(LP_state *) * n_prc);
		bzero(LPS_bound, sizeof(LP_state *) * n_prc);

		GLPs_block_binding();

		timer_start(rebinding_timer);

		if(master_thread()) {
			glp_cost = rsalloc(sizeof(double)* n_prc);
			new_group_LPS = rsalloc(sizeof(int) * n_prc);

			new_GLPS_binding = rsalloc(sizeof(int) * n_grp);

			new_GLPS = (GLP_state **)rsalloc(n_grp * sizeof(GLP_state *));
			unsigned int i;
			for (i = 0; i < n_grp; i++) {
				new_GLPS[i] = (GLP_state *)rsalloc(sizeof(GLP_state));
				bzero(new_GLPS[i], sizeof(GLP_state));

				new_GLPS[i]->local_LPS = rsalloc(n_prc * sizeof(LP_state *));

				//Initialise GROUPS
				spinlock_init(&new_GLPS[i]->lock);
				new_GLPS[i]->id = i;
				new_GLPS[i]->local_LPS[0] = LPS[i];
				new_GLPS[i]->tot_LP = 1;
				new_GLPS[i]->initial_group_time = (msg_t *)rsalloc(sizeof(msg_t));
				new_GLPS[i]->counter_rollback = 0;
				new_GLPS[i]->counter_synch = 0;
				new_GLPS[i]->counter_log = 0;
				new_GLPS[i]->lvt = NULL;
				new_GLPS[i]->state = GLP_STATE_READY;
				new_GLPS[i]->from_last_ckpt = 0;
				new_GLPS[i]->ckpt_period = CKPT_PERIOD_GROUP;

				
			}

			lp_cost = rsalloc(sizeof(struct lp_cost_id) * n_prc);

			atomic_set(&worker_thread_reduction, n_cores);
		}

		return;
	}

#ifdef HAVE_LP_REBINDING
if(!gvt_stable()) return;

	if(master_thread()) {

		if(atomic_read(&worker_thread_reduction) == 0) {
			
			GLP_knapsack();

			binding_acquire_phase++;
		}
	}

	if(local_binding_phase < binding_phase) {
		local_binding_phase = binding_phase;
		post_local_reduction();
		atomic_dec(&worker_thread_reduction);
	}

	if(local_binding_acquire_phase < binding_acquire_phase) {
		local_binding_acquire_phase = binding_acquire_phase;
		
		install_GLPS_binding();

		#ifdef HAVE_PREEMPTION
		reset_min_in_transit(tid);
		#endif

		if(thread_barrier(&all_thread_barrier)) {
			atomic_set(&worker_thread_reduction, n_cores);
		}

		//Find maximum timestamp of bound between LPs inside a group
		set_group_bound();

		send_control_group_message(); 	// RENDERLO PER THREAD

		switch_GLPS();			
			
		/*	
		unsigned int k = 0;
		for(;k<n_prc_per_thread;k++)
			printf("[tid:%d] LP:%d G:%d\n",tid,LPS_bound[k]->lid,LPS_bound[k]->current_group);
		*/
		
	}
#endif
}

void force_rebind_GLP(void){
	binding_phase++;
}

/* -------------------------------------------------------------------- */
/* ---------------------END MANAGE GROUP------------------------------- */
/* -------------------------------------------------------------------- */
#endif

#ifndef HAVE_GLP_SCH_MODULE
/**
* This function is used to create a temporary binding between LPs and KLT.
* The first time this function is called, each worker thread sets up its data
* structures, and the performs a (deterministic) block allocation. This is
* because no runtime data is available at the time, so we "share" the load
* as the number of LPs.
* Then, successive invocations, will use the knapsack load sharing policy

* @author Alessandro Pellegrini
*/
void rebind_LPs(void) {

	if(first_lp_binding) {
		first_lp_binding = false;

		// Binding metadata are used in the platform to perform
		// operations on LPs in isolation
		LPS_bound = rsalloc(sizeof(LP_state *) * n_prc);
		bzero(LPS_bound, sizeof(LP_state *) * n_prc);
		
		LPs_block_binding();

		timer_start(rebinding_timer);

		if(master_thread()) {
			new_LPS_binding = rsalloc(sizeof(int) * n_prc);

			lp_cost = rsalloc(sizeof(struct lp_cost_id) * n_prc);

			atomic_set(&worker_thread_reduction, n_cores);
		}

		return;
	}

#ifdef HAVE_LP_REBINDING
	if(master_thread()) {
		if(timer_value_seconds(rebinding_timer) >= REBIND_INTERVAL) {
			timer_restart(rebinding_timer);
			binding_phase++;
		}

		if(atomic_read(&worker_thread_reduction) == 0) {
			
			LP_knapsack();

			binding_acquire_phase++;
		}
	}

	if(local_binding_phase < binding_phase) {
		local_binding_phase = binding_phase;
		post_local_reduction();
		atomic_dec(&worker_thread_reduction);
	}

	if(local_binding_acquire_phase < binding_acquire_phase) {
		local_binding_acquire_phase = binding_acquire_phase;
		
		install_binding();

		#ifdef HAVE_PREEMPTION
		reset_min_in_transit(tid);
		#endif

		if(thread_barrier(&all_thread_barrier)) {
			atomic_set(&worker_thread_reduction, n_cores);
		}

	}
#endif
}

#endif
