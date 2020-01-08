/*
 * approximate_rollback.c
 *
 *  Created on: Dec 19, 2019
 *      Author: andrea
 */
#ifdef HAVE_APPROXIMATED_ROLLBACK

#include <scheduler/scheduler.h>
#include <mm/dymelor.h>


void CoreMemoryMark(void *address)
{
	switch_to_platform_mode();
	int chunk;
	malloc_area *m_area = malloc_area_get(address, &chunk);
	if(m_area == NULL){
		rootsim_error(true, "Please, supply an allocated address and range to the CoreMemoryMark API");
	}

	malloc_state *m_state = current->mm->m_state;
	size_t chk_size = UNTAGGED_CHUNK_SIZE(m_area);

	if(!bitmap_check(m_area->coredata_bitmap, chunk)){
		bitmap_set(m_area->coredata_bitmap, chunk);
		m_state->approximated_log_size += chk_size;
	}

	switch_to_application_mode();
}

void CoreMemoryUnmark(void *address)
{
	switch_to_platform_mode();
	int chunk;
	malloc_area *m_area = malloc_area_get(address, &chunk);
	if(m_area == NULL){
		rootsim_error(true, "Please, supply an allocated address and range to the CoreMemoryMark API");
	}

	malloc_state *m_state = current->mm->m_state;
	size_t chk_size = UNTAGGED_CHUNK_SIZE(m_area);

	if(bitmap_check(m_area->coredata_bitmap, chunk)){
		bitmap_reset(m_area->coredata_bitmap, chunk);
		m_state->approximated_log_size -= chk_size;
	}

	switch_to_application_mode();
}

bool CoreMemoryCheck(void *address)
{
	switch_to_platform_mode();
	int chunk;
	malloc_area *m_area = malloc_area_get(address, &chunk);
	if(m_area == NULL){
		rootsim_error(true, "Please, supply an allocated address and range to the CoreMemoryUnmark API");
	}
	bool ret = bitmap_check(m_area->coredata_bitmap, chunk);
	switch_to_application_mode();
	return ret;
}

void RollbackModeSet(bool want_approximated)
{
	switch_to_platform_mode();
	current->mm->m_state->is_approximated = want_approximated;
	switch_to_application_mode();
}

void restore_approximated()
{
	// TODO call the user supplied function for restoring the stuff
}

void event_approximation_mark(struct lp_struct *lp, msg_t *event)
{
	event->is_approximated = lp->mm->m_state->is_approximated;
}

#endif
