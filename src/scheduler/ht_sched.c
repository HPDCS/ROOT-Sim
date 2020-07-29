#include <scheduler/ht_sched.h>

#include <core/core.h>
#include <core/init.h>
#include <arch/thread.h>
#include <mm/mm.h>

#include <time.h>
#include <limits.h>
#include <stdatomic.h>

static unsigned *p_tids;
static unsigned *i_tids;
static atomic_bool *idle_cmd;
static atomic_bool *idle_rcvd;

static int parse_ht_info(unsigned proc_tids[n_cores], unsigned idle_tids[n_cores])
{
	const unsigned half_cores = get_cores() / 2;
	const char *cmd = "lscpu -p";

	char buf[1024];
	FILE *fp;

	if (n_cores > half_cores) {
		rootsim_error(false, "HT infos are useless since we are running on more than half available threads anyway\n");
		return -1;
	}

	if ((fp = popen(cmd, "r")) == NULL) {
		rootsim_error(false, "Unable to open a pipe\n");
		return -1;
	}

	memset(proc_tids, UCHAR_MAX, sizeof(*proc_tids) * n_cores);

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if(buf[0] == '#')
			continue;

		unsigned i, j;
		sscanf(buf, "%u,%u ", &i, &j);

		if(j >= half_cores) {
			rootsim_error(false, "This machine probably hasn't got SMT\n", cmd);
			return -1;
		}

		if(proc_tids[j] == UINT_MAX)
			proc_tids[j] = i;
		else
			idle_tids[j] = i;
	}

	if(pclose(fp)) {
		rootsim_error(false, "Command %s not found or exited with error status\n", cmd);
		return -1;
	}

	return 0;
}

static void *idle_thread_fnc(void *arg)
{
	unsigned thread_i = *(unsigned *)arg;
	unsigned this_tid = ((unsigned *)arg) - i_tids;

	set_affinity(thread_i);

	while (1) {
		if (atomic_load_explicit(&idle_cmd[this_tid], memory_order_acquire)) {

			// useless but whatever
			atomic_thread_fence(memory_order_seq_cst);

			atomic_store_explicit(&idle_rcvd[this_tid], true, memory_order_release);

			while (atomic_load_explicit(&idle_cmd[this_tid], memory_order_relaxed));

			atomic_store_explicit(&idle_rcvd[this_tid], false, memory_order_release);
		}
		struct timespec req = {.tv_sec = 0, .tv_nsec = 10000};
		nanosleep(&req, NULL);
	}
	return NULL;
}

void idle_thread_activate(void)
{
	if(unlikely(!idle_cmd))
		return;

	unsigned thread_i = local_tid;
	while (atomic_load_explicit(&idle_rcvd[thread_i], memory_order_acquire));
	atomic_store_explicit(&idle_cmd[thread_i], true, memory_order_release);
	while (!atomic_load_explicit(&idle_rcvd[thread_i], memory_order_acquire));
}

void idle_thread_deactivate(void)
{
	if(unlikely(!idle_cmd))
		return;

	unsigned thread_i = local_tid;
	atomic_store_explicit(&idle_cmd[thread_i], false, memory_order_release);
}

void ht_sched_init(void)
{
	// Set the affinity on a CPU core, for increased performance
	if (unlikely(!rootsim_config.core_binding))
		return;

	i_tids = rsalloc(sizeof(*i_tids) * n_cores);
	p_tids = rsalloc(sizeof(*p_tids) * n_cores);

	unsigned i = n_cores;
	if(parse_ht_info(p_tids, i_tids) < 0) {
		// default mapping
		while(i--) {
			p_tids[i] = i;
		}
		return;
	}

	if (rootsim_config.snapshot == SNAPSHOT_HARDINC) {
		idle_cmd = rsalloc(sizeof(*idle_cmd) * n_cores);
		memset(idle_cmd, 0, sizeof(*idle_cmd) * n_cores);

		idle_rcvd = rsalloc(sizeof(*idle_rcvd) * n_cores);
		memset(idle_rcvd, 0, sizeof(*idle_rcvd) * n_cores);

		while(i--) {
			new_thread(idle_thread_fnc, &i_tids[i]);
		}
	}
}

void smart_set_affinity(void)
{
	// Set the affinity on a CPU core, for increased performance
	if (likely(rootsim_config.core_binding))
		set_affinity(p_tids[local_tid]);
}

