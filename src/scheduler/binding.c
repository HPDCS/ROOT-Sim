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
#include <gvt/gvt.h>
#include <scheduler/process.h>
#include <scheduler/binding.h>
//#include <scheduler/group.h>
#include <statistics/statistics.h>

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
static unsigned int *new_GLPS_binding;
static GLP_state **new_GLPS;
static unsigned int *new_group_LPS;
#endif

static timer rebinding_timer;

static unsigned int *new_LPS_binding;


static int binding_phase = 0;
static __thread int local_binding_phase = 0;

static int binding_acquire_phase = 0;
static __thread int local_binding_acquire_phase = 0;

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
	double reference_lvt;
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
	unsigned int i, j, y, count;
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
				spin_lock_x86(&(GLPS[i]->lock));
				count = 0;
				y=0;
				while(count < GLPS[i]->tot_LP){
					if(GLPS[i]->local_LPS[y] != NULL){
						LPS_bound[n_prc_per_thread++] = LPS[i];
						LPS[i]->worker_thread = tid;
						count++;
					}
					y++;
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
static double compute_workload_GLP(int id) {
	glp_cost[id] = 0;

	GLP_state *group = new_GLPS[id];
	//Lock is required since there may be a WT that is updating the local_LP and the correlated tot_LP count
	spin_lock_x86(&(group->lock));

	int count = 0;
	int i=0;
	while(count < group->tot_LP){
		if(group->local_LPS[i] != NULL){
			glp_cost[id] += lp_cost[i].workload_factor;
			count++;
		}
		
		i++;
	}

	spin_unlock_x86(&(group->lock));

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
int LP_change_group(GLP_state **GROUPS_global, unsigned int actual_lp,unsigned int **statistics){
	unsigned int group_index = statistics[actual_lp][1];
		
	if(LPS[actual_lp]->current_group != group_index){
		
		//Update old group
		//spin_lock(GROUPS_global[actual_lp->current_group]->lock);
		GROUPS_global[LPS[actual_lp]->current_group]->local_LPS[actual_lp] = NULL;
		GROUPS_global[LPS[actual_lp]->current_group]->tot_LP--;
		//spin_unlock(GROUPS_global[actual_lp->current_group]->lock);
					
		//Update new group
		//spin_lock(GROUPS_global[i]->lock);
		GROUPS_global[group_index]->local_LPS[actual_lp] = LPS[actual_lp];
		GROUPS_global[group_index]->tot_LP++;
		//spin_unlock(GROUPS_global[i]->lock);
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
static void analise_static_group(unsigned int **statistics){
	unsigned int temp_max_access = -1, i=0, j=0;
	ECS_stat *ECS_entry_temp;	
	ECS_stat **actual_lp_table;	

	for(i=0 ; i<n_prc ; i++){
		statistics[i][0] = -1;
		statistics[i][1] = -1;
		actual_lp_table = LPS[i]->ECS_stat_table;
		for(j=0; j<n_prc;j++){
                	ECS_entry_temp = actual_lp_table[j];
                	if(ECS_entry_temp->count_access > THRESHOLD_ACCESS_ECS  && ECS_entry_temp->count_access > temp_max_access){
				statistics[i][0] = j;
				temp_max_access =  ECS_entry_temp->count_access;
			}	
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
static unsigned int clustering_groups(unsigned int **statistics, unsigned int lid, unsigned int group){
	if(statistics[lid][1] != -1)
		return statistics[lid][1];
	
	if(group != -1)
                statistics[lid][1] = group;
	else
		statistics[lid][1] = lid;
		
	if(statistics[lid][0] != -1){
		statistics[lid][1] = clustering_groups(statistics,statistics[lid][0],statistics[lid][1]);
	}
	
	if(lid != n_prc && group == -1)
		clustering_groups(statistics,lid+1,-1);
	
	return statistics[lid][1];
		
	
}

/**
//TODO MN
* Updates all groups configurations according to LPs' statistics
*
* @author Nazzareno Marziale
* @author Francesco Nobilia
*/
static void update_clustering_groups(){
	unsigned int i;
	unsigned int statistics[n_prc][2];
	
	//Create a graph thanks to ECS statistics
	analise_static_group(statistics);

	//Recursive function that update the clustering info
	clustering_groups(statistics,0,-1);
	
	for(i=0; i<n_prc; i++){
		new_group_LPS[i] = LP_change_group(new_GLPS, i, statistics);
	}
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
	double reference_lvt;
	bool assigned;
	double assignments[n_cores];

	if(!master_thread())
		return;

	// Clustering group
	//TODO MN only if LastGVT > GVT + deltaT 
	update_clustering_groups();

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
	for(i = 0; i < n_cores; i++) {
		assignments[j] += glp_cost[i];
		new_GLPS_binding[i] = j;
		j++;
	}

	// Very suboptimal approximation of knapsack
	for(; i < n_grp; i++) {
		assigned = false;
		for(j = 0; j < n_cores; j++) {
			// Simulate assignment
			if(assignments[j] + glp_cost[i] <= reference_knapsack) {
				assignments[j] += glp_cost[i];
				new_GLPS_binding[i] = j;
				assigned = true;
				break;
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

	printf("NEW BINDING\n");
	for(j = 0; j < n_cores; j++) {
		printf("Thread %d: ", j);
		for(i = 0; i < n_prc; i++) {
			if(new_GLPS_binding[i] == j)
				printf("%d ", i);
		}
		printf("\n");
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

	for(i = 0; i < n_prc; i++) {
		if(new_GLPS_binding[LPS[i]->current_group] == tid){
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
static void switch_GLPS(){
	unsigned int i;
	for (i = 0; i < n_grp; i++) {
		memcpy(GLPS[i]->local_LPS, new_GLPS[i]->local_LPS, n_prc * sizeof(LP_state *));
		GLPS[i]->tot_LP = new_GLPS[i]->tot_LP;
	}

	for (i = 0; i < n_prc; i++)
		LPS[i]->current_group = new_group_LPS[i];


	//TODO MN
	//	If there exist one LP that changes group, then we need to force checkpoint for storing the new group's image.
}

/* -------------------------------------------------------------------- */
/* ---------------------END MANAGE GROUP------------------------------- */
/* -------------------------------------------------------------------- */
#endif

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

		//TODO MN
		// Without HAVE_LP_REBINDING but we want to use groups' module
		#ifdef HAVE_GLP_SCH_MODULE
			printf("Rebind_LP_GROUP... ");
			GLPs_block_binding();
			printf("Done\n");
			
			int j=0;
		#else	
			LPs_block_binding();
		#endif


		timer_start(rebinding_timer);

		if(master_thread()) {
			//TODO MN
			#ifdef HAVE_GLP_SCH_MODULE
				glp_cost = rsalloc(sizeof(double)* n_prc);
				new_group_LPS = rsalloc(sizeof(int) * n_prc);

				new_GLPS_binding = rsalloc(sizeof(int) * n_grp);

				new_GLPS = (GLP_state **)rsalloc(n_grp * sizeof(GLP_state *));
				unsigned int i;
				for (i = 0; i < n_grp; i++) {
					new_GLPS[i] = (GLP_state *)rsalloc(sizeof(GLP_state));
					bzero(new_GLPS[i], sizeof(GLP_state));

					new_GLPS[i]->local_LPS = (LP_state **)rsalloc(n_prc * sizeof(LP_state *));
					unsigned int j;
					for (j = 0; j < n_prc; j++) {
						new_GLPS[i]->local_LPS[j] = (LP_state *)rsalloc(sizeof(LP_state));
						bzero(new_GLPS[i]->local_LPS[j], sizeof(LP_state));
					}

					//Initialise GROUPS
					spinlock_init(&new_GLPS[i]->lock);
					new_GLPS[i]->local_LPS[i] = LPS[i];
					new_GLPS[i]->tot_LP = 1;
				}
			#else
				new_LPS_binding = rsalloc(sizeof(int) * n_prc);
			#endif

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
		} else if(atomic_read(&worker_thread_reduction) == 0) {
			
			//TODO MN
			#ifdef HAVE_GLP_SCH_MODULE
				printf("GLP_knapsack\n");
				GLP_knapsack();
			#else
				LP_knapsack();
			#endif

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
		
		//TODO MN
		#ifdef HAVE_GLP_SCH_MODULE
			install_GLPS_binding();
		#else
			install_binding();
		#endif

		#ifdef HAVE_PREEMPTION
		reset_min_in_transit(tid);
		#endif

		if(thread_barrier(&all_thread_barrier)) {
			atomic_set(&worker_thread_reduction, n_cores);
		}

		//TODO MN
		#ifdef HAVE_GLP_SCH_MODULE
		if(master_thread()) {
			switch_GLPS();
		}
		thread_barrier(&all_thread_barrier);
		
		#endif

	}
#endif
}


