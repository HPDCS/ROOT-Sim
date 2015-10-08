/**
*                       Copyright (C) 2008-2015 HPDCS Group
*                       http://www.dis.uniroma1.it/~hpdcs
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
* @file group.c 
* @brief //TODO MN
* @author Nazzareno Marziale
* @author Francesco Nobilia
*
* @date September 23, 2015
*/

#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <scheduler/group.h>

/// Number of group of logical processes hosted by the current kernel instance
unsigned int n_grp;


/** 
//TODO MN
* Checks if it is necessary that actual_lp has to change group,
* if true it returns the newer group index otherwise the older one 
*
* @author Nazzareno Marziale
* @author Francesco Nobilia
*
* @param GROUPS_global Pointer to the temporary groups data-structure 
* @param actual_lp The considered LP_state
*
* @return The group index
*/

*/
extern int (*LP_change_group)(GLP_state **GROUPS_global, LP_state actual_lp);
int real_LP_change_group(GLP_state **GROUPS_global, LP_state actual_lp){
	ECS_stat ECS_entry_temp;
	unsigned int i;
	for(i=0 ; i<n_grp ; i++){
		ECS_entry_temp = actual_lp->ECS_stat_table[i];
		if(ECS_entry_temp->count_access > threshold_access){
			
			//Update old group
			//spin_lock(GROUPS_global[actual_lp->current_group]->lock);
			GROUPS_global[actual_lp->current_group]->local_LPS[lp] = NULL;
			GROUPS_global[actual_lp->current_group]->tot_LP--;
			//spin_unlock(GROUPS_global[actual_lp->current_group]->lock);
					
			//Update new group
			//spin_lock(GROUPS_global[i]->lock);
			GROUPS_global[i]->local_LPS[lp] = actual_lp;
			GROUPS_global[i]->tot_LP++;
			//spin_unlock(GROUPS_global[i]->lock);

			return i;
		}
	}

	return actual_lp->current_group;
}

int init_module(void){
	LP_change_group = &real_LP_change_group;
}

void cleanup_module(void){
	LP_change_group = NULL;	
}