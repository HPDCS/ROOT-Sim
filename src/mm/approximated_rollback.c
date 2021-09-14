/*
 * approximate_rollback.c
 *
 *  Created on: Dec 19, 2019
 *      Author: andrea
 */
#ifdef HAVE_APPROXIMATED_ROLLBACK

#include <core/core.h>
#include <core/init.h>
#include <scheduler/scheduler.h>
#include <mm/dymelor.h>
#include <statistics/statistics.h>

#define AUTONOMIC_EVENTS_THRESHOLD 1000

void CoreMemoryMark(void *address)
{
	if (unlikely(rootsim_config.serial))
	{
		return;
	}
	switch_to_platform_mode();
	int chunk;
	malloc_area *m_area = malloc_area_get(address, &chunk);
	if (m_area == NULL)
	{
		rootsim_error(true, "Please, supply an allocated address and range to the CoreMemoryMark API");
	}

	malloc_state *m_state = current->mm->m_state;
	size_t chk_size = UNTAGGED_CHUNK_SIZE(m_area);

	if (!bitmap_check(m_area->coredata_bitmap, chunk))
	{
		bitmap_set(m_area->coredata_bitmap, chunk);
		m_state->approximated_log_size += chk_size;
	}

	switch_to_application_mode();
}

void CoreMemoryUnmark(void *address)
{
	if (unlikely(rootsim_config.serial))
	{
		return;
	}
	switch_to_platform_mode();
	int chunk;
	malloc_area *m_area = malloc_area_get(address, &chunk);
	if (m_area == NULL)
	{
		rootsim_error(true, "Please, supply an allocated address and range to the CoreMemoryUnmark API");
	}

	malloc_state *m_state = current->mm->m_state;
	size_t chk_size = UNTAGGED_CHUNK_SIZE(m_area);

	if (bitmap_check(m_area->coredata_bitmap, chunk))
	{
		bitmap_reset(m_area->coredata_bitmap, chunk);
		m_state->approximated_log_size -= chk_size;
	}

	switch_to_application_mode();
}

bool CoreMemoryCheck(void *address)
{
	if (unlikely(rootsim_config.serial))
	{
		return true;
	}
	switch_to_platform_mode();
	int chunk;
	malloc_area *m_area = malloc_area_get(address, &chunk);
	if (m_area == NULL)
	{
		return false;
	}
	bool ret = bitmap_check(m_area->coredata_bitmap, chunk);
	switch_to_application_mode();
	return ret;
}

bool IsAllocatedMemoryCheck(void *address)
{
	if (unlikely(rootsim_config.serial))
	{
		return true;
	}
	switch_to_platform_mode();
	int chunk;
	malloc_area *m_area = malloc_area_get(address, &chunk);
	switch_to_application_mode();
	return m_area != NULL;
}

void RollbackModeSet(enum _rollback_mode_t mode)
{
	if (unlikely(rootsim_config.serial))
	{
		return;
	}
	switch_to_platform_mode();
	current->mm->m_state->approximated_mode = mode;
	switch_to_application_mode();
}

bool RollbackModeCheck(void)
{
	if (unlikely(rootsim_config.serial))
	{
		return false;
	}
	switch_to_platform_mode();
	bool ret = current_evt->is_approximated;
	switch_to_application_mode();
	return ret;
}

void updateCheckpointInterval(struct lp_struct *lp, bool approx) {
	if (lp->policy_events_counter > 0)
		return;

	double chi = computeCheckpointInterval(lp, approx);
	set_checkpoint_period(lp, chi);
	lp->policy_events_counter = AUTONOMIC_EVENTS_THRESHOLD;
}

void event_approximation_mark(struct lp_struct *lp, msg_t *event)
{
	bool is_approximated = lp->mm->m_state->is_approximated;
	enum _rollback_mode_t approximated_mode = lp->mm->m_state->approximated_mode;

	switch (approximated_mode) {
		case AUTONOMIC:
			if (lp->policy_events_counter == 0) {
				is_approximated = shouldSwitchApproximationMode(lp);
				lp->policy_events_counter = AUTONOMIC_EVENTS_THRESHOLD;
			}
			break;
		case APPROXIMATED:
			is_approximated = true;
			updateCheckpointInterval(lp, true);
			break;
		case PRECISE:
			is_approximated = false;
			updateCheckpointInterval(lp, false);
			break;
		default:
			return;
	}

	lp->policy_events_counter--;

	if (is_approximated) {
		statistics_post_data(lp, STAT_APPROX_PHASE, 1.0);
	} else {
		statistics_post_data(lp, STAT_PREC_PHASE, 1.0);
	}

	lp->mm->m_state->is_approximated = is_approximated;
	event->is_approximated = is_approximated;
}

#endif
