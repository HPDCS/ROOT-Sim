#include <core/core.h>
#include <mm/state.h>

#define MACRO_PERIOD_US 1000000
#define MICRO_PERIOD_US 100000
#define MICRO_PERIOD_DELAY 1000
#define CLOCKS_PER_US 2200


/// This overflows if the machine is not restarted in about 50-100 years (on 64 bits archs)
#define CLOCK_READ() ({ \
			unsigned int lo; \
			unsigned int hi; \
			__asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi)); \
			(unsigned long long)(((unsigned long long)hi) << 32 | lo); \
			})

#define clock_us() (CLOCK_READ()/CLOCKS_PER_US)

/****************
	TMP STATS
****************/
static __thread unsigned int current_executed_events 	= 0;
static __thread unsigned int current_sample_id 			= 0;
static __thread unsigned int last_sample_id 			= 0;
static __thread unsigned int sampling_enabled 			= 0;

/****************
	REAL STATS
****************/
static __thread unsigned int sampled_rollbacks 			= 0;
static __thread unsigned int forward_executed_events 	= 0;
static __thread unsigned int aborted_events 			= 0;

/****************
	TIME VARS
****************/

static __thread unsigned long long begin_time = 0;
static __thread unsigned long long start_macro_time = 0;
static __thread unsigned long long current_time = 0;

typedef struct _new_stats{
	unsigned int sampled_rollbacks;
	unsigned int aborted_events;
	unsigned int forward_executed_events;
	unsigned int pad[13];
} new_stats;

/****************
 COLLECT VARS
****************/

static new_stats* stat_collection;

/* statistics.c */
void init_new_statistics(){
	stat_collection = rsalloc(n_cores*sizeof(new_stats));
	bzero(stat_collection , n_cores*sizeof(new_stats));
}

/* scheduler.c */
void on_process_event_forward(msg_t *evt){
	current_executed_events	+= sampling_enabled;
	evt->sample_id 			 = current_sample_id;
}

/* state.c */
void on_log_state(state_t *log){
	forward_executed_events 	 +=current_executed_events;
	log->executed_events  = current_executed_events;
	log->sample_id 		  = current_sample_id;
	current_executed_events		  = 0;
}

/* state.c (rollback) */
void on_log_restore(){
	sampled_rollbacks		+=sampling_enabled;
	aborted_events			+= current_executed_events;
	forward_executed_events	+=current_executed_events;
	current_executed_events	 =0;
}

/* state.c (rollback) */
void on_log_discarded(state_t *log){
	if(log->sample_id == current_sample_id)
		aborted_events+= log->executed_events;	
}

/* state.c (silent_exec) */
void on_process_event_silent(msg_t *evt){
		aborted_events-=evt->sample_id == current_sample_id;
}

void OnSamplingPeriodBegin(){
//	printf("BEGIN\n");
	sampling_enabled=1;
	current_sample_id = last_sample_id+1;
	forward_executed_events = 0;
	aborted_events = 0;
	sampled_rollbacks = 0;
	current_executed_events=0;

}

void OnSamplingPeriodEnd(){
//	printf("END\n");
	sampling_enabled=0;
	last_sample_id = current_sample_id;
	current_sample_id = 0;

	forward_executed_events+=current_executed_events;
	// TODO atomic update


//	printf("Exec: %u, Aborted: %u, Rollbacks: %u\n", forward_executed_events, aborted_events, sampled_rollbacks);
	__sync_fetch_and_add(&stat_collection[tid].forward_executed_events, forward_executed_events);
	__sync_fetch_and_add(&stat_collection[tid].aborted_events, aborted_events);
	__sync_fetch_and_add(&stat_collection[tid].sampled_rollbacks, sampled_rollbacks);

}

void process_statistics(){
	current_time = clock_us();
	if(start_macro_time == 0)	{begin_time = start_macro_time = current_time;}
	
	if(!sampling_enabled){
		if((current_time - start_macro_time) >= MACRO_PERIOD_US){

			if(master_thread()){
				double fee = 0, ae = 0, sr = 0;
				int i;
				for(i = 0; i < active_threads; i++){

					fee += __sync_lock_test_and_set(&stat_collection[i].forward_executed_events	, 0);
					ae  += __sync_lock_test_and_set(&stat_collection[i].aborted_events			, 0);
					sr  += __sync_lock_test_and_set(&stat_collection[i].sampled_rollbacks		, 0);
				}
				printf("Time: %llu Exec: %f.0, ExecTh:%.2f, Aborted: %f.0, PA: %.2f%, Rollbacks: %f.0, PR: %.2f%\n", current_time-begin_time,
					fee, fee/((double)MICRO_PERIOD_US/1000000), ae, ae*100.0/fee, sr, sr*100.0/fee);
			}
			//	print forward_executed_events AbortedEvents
			start_macro_time = current_time;	
		}	
		if((current_time - start_macro_time) >= MICRO_PERIOD_DELAY) OnSamplingPeriodBegin();
	}

	if(sampling_enabled && ((current_time - start_macro_time) >=  (MICRO_PERIOD_DELAY + MICRO_PERIOD_US)) ) OnSamplingPeriodEnd();
	
}
