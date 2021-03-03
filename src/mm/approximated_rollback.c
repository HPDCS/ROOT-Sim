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
		rootsim_error(true, "Please, supply an allocated address and range to the CoreMemoryCheck API");
	}
	bool ret = bitmap_check(m_area->coredata_bitmap, chunk);
	switch_to_application_mode();
	return ret;
}

bool ask_oracle()
{
	int random_range = Random() * 100;
	if (random_range < 5)
	{
		return true;
	}
	else
	{
		return false;
	}
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

void event_approximation_mark(const struct lp_struct *lp, msg_t *event)
{
	bool is_approximated;
	enum _rollback_mode_t approximated_mode = lp->mm->m_state->approximated_mode;
	if (approximated_mode == AUTONOMIC){
		is_approximated = ask_oracle();
	}
	else{
		is_approximated = approximated_mode == APPROXIMATED ? true : false;
	}
	lp->mm->m_state->is_approximated = is_approximated;
	event->is_approximated = is_approximated;
}

double computeCheckpointInterval(struct lp_struct *lp, bool approx)
{

	double evnt_time = statistics_get_lp_data(lp, STAT_GET_EVENT_TIME); //𝛿_e
	double tot_rollbacks = statistics_get_lp_data(lp, approx ? STAT_GET_ROLLBACK_APPROX : STAT_GET_ROLLBACK);
	double tot_events = statistics_get_lp_data(lp, approx ? STAT_GET_EVENT_APPROX : STAT_GET_EVENT);
	double roll_freq = tot_rollbacks / tot_events; //F_r (or F_r_approx, according to @approx)
	double ckpt_time = statistics_get_lp_data(lp, STAT_GET_CKPT_TIME) -
			   (approx ? statistics_get_lp_data(lp, STAT_GET_CKPT_TIME_APPROX) : 0); //𝛿_s
	return ceil(sqrt((2 * ckpt_time) / (roll_freq * evnt_time)));
}

bool shouldSwitchApproximationMode(struct lp_struct *lp)
{
	double chi = computeCheckpointInterval(lp, false); //χ
	double ckpt_time = statistics_get_lp_data(lp, STAT_GET_CKPT_TIME); //𝛿_s
	double evnt_time = statistics_get_lp_data(lp, STAT_GET_EVENT_TIME); //𝛿_e
	double roll_time = statistics_get_lp_data(lp, STAT_GET_ROLLBACK_TIME); //𝛿_r
	double roll_freq = statistics_get_lp_data(lp, STAT_GET_ROLLBACK) /
			   statistics_get_lp_data(lp, STAT_GET_EVENT); //F_r


	double numerator = (chi * evnt_time) +
			   (ckpt_time / chi) +
			   roll_freq * (roll_time + ((chi - 1) / 2) * evnt_time);

	double chi_approx = computeCheckpointInterval(lp, true); //χ_a
	double alpha = chi / chi_approx; //α
	double ckpt_time_core = ckpt_time -
				statistics_get_lp_data(lp, STAT_GET_CKPT_TIME_APPROX); //𝛿_r_core
	double roll_freq_appr = statistics_get_lp_data(lp, STAT_GET_ROLLBACK_APPROX) /
				statistics_get_lp_data(lp, STAT_GET_EVENT_APPROX); //F_r_approx
	double roll_time_core = roll_time -
				statistics_get_lp_data(lp, STAT_GET_ROLLBACK_TIME_APPROX); //𝛿_r_core
	double rec_time = statistics_get_lp_data(lp, STAT_GET_RECOVERY_TIME); //𝛿_cf

	double denominator = alpha *
			     (chi_approx * evnt_time +
			      ckpt_time_core / chi_approx +
			      roll_freq_appr * (roll_time_core +
						rec_time +
						(((chi_approx - 1) / 2) * evnt_time)));

	return (numerator / denominator) > 1;
}

#endif
