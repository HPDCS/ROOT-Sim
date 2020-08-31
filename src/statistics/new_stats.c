#include <core/core.h>
#include <mm/state.h>
#include <powercap/powercap.h>

#define MICRO_PERIOD_US 	 (micro_period_ms*1000)
#define MICRO_PERIOD_DELAY 	 (micro_period_dly_ms*1000)
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
static __thread int current_executed_events 		= 0;
static __thread int current_sample_id 			= 0;
static  int last_sample_id 			= 0;
static __thread int sampling_enabled 			= 0;

/****************
	REAL STATS
****************/
static __thread int sampled_rollbacks 			= 0;
static __thread int forward_executed_events 		= 0;
static __thread int aborted_events 			= 0;

/****************
	TIME VARS
****************/

static unsigned long long start_macro_time = 0;
static __thread unsigned long long current_time = 0;
static unsigned long long sampling_time = 0;
static unsigned long long delta_time = 0;

typedef struct _new_stats{
	int sampled_rollbacks;
	int aborted_events;
	int forward_executed_events;
	int last_sample_id;
	unsigned long long delta;
	unsigned int pad[10];
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
	evt->sample_id = current_sample_id;
}

/* state.c */
void on_log_state(state_t *log){
	forward_executed_events	+= current_executed_events;
	log->executed_events  	 = current_executed_events;
	log->sample_id 		 = current_sample_id;
	current_executed_events	 = 0;
}

/* state.c (rollback) */
void on_log_restore(){
	sampled_rollbacks	+= sampling_enabled;
	aborted_events		+= current_executed_events;
	forward_executed_events	+= current_executed_events;
	current_executed_events	 = 0;
}

/* state.c (rollback) */
void on_log_discarded(state_t *log){
	if((log->sample_id == current_sample_id) && current_sample_id != 0)	aborted_events+= log->executed_events;	
}

/* state.c (silent_exec) */
void on_process_event_silent(msg_t *evt){
	if(evt->sample_id == current_sample_id && current_sample_id != 0) {
		aborted_events -= 1;
		forward_executed_events -=1;
		current_executed_events +=1;
	}

}

void OnSamplingPeriodBegin(){
	sampling_enabled=1;
	current_sample_id = last_sample_id;
	forward_executed_events = 0;
	aborted_events = 0;
	sampled_rollbacks = 0;
	current_executed_events=0;
}

void OnSamplingPeriodEnd(){
	__sync_fetch_and_add(&stat_collection[tid].forward_executed_events, forward_executed_events);
	__sync_fetch_and_add(&stat_collection[tid].aborted_events, aborted_events);
	__sync_fetch_and_add(&stat_collection[tid].sampled_rollbacks, sampled_rollbacks);
	__sync_fetch_and_add(&stat_collection[tid].last_sample_id, current_sample_id);
	__sync_fetch_and_add(&stat_collection[tid].delta, (current_time - sampling_time));
	current_sample_id = 0;
	sampling_enabled= 0 ;
        forward_executed_events = 0;
        aborted_events = 0;
        sampled_rollbacks = 0;
        current_executed_events=0;
}

/* this must be executed by a unique thread, e.g. by GVT trigger */
double collect_statistics(void){
	current_time = clock_us();
	{
		// collect stats	
		double fee = 0, ae = 0, sr = 0, tm = 0, throughput = 0;
		int i;
		for(i = 0; i < active_threads; i++){
//                	 fee += __sync_lock_test_and_set(&stat_collection[i].forward_executed_events     , 0);
 //               	 ae  += __sync_lock_test_and_set(&stat_collection[i].aborted_events              , 0);
                	 sr  += __sync_lock_test_and_set(&stat_collection[i].sampled_rollbacks           , 0);
                	 printf("[THSTATS] %d %llu %d %d\n", stat_collection[i].last_sample_id, stat_collection[i].delta/1000, stat_collection[i].aborted_events, stat_collection[i].forward_executed_events);

                         fee += __sync_lock_test_and_set(&stat_collection[i].forward_executed_events     , 0);
                         ae  += __sync_lock_test_and_set(&stat_collection[i].aborted_events              , 0);			 __sync_lock_test_and_set(&stat_collection[i].last_sample_id          , 0);
			 tm += __sync_lock_test_and_set(&stat_collection[i].delta           , 0ULL);
   		}

   		throughput = (fee-ae)/((double)delta_time/1000000.0);

  		printf( "[MICRO STATS] "
			"Time: %f Exec: %f, ExecTh:%.2f, E[Th]:%.2f,"
			" Aborted: %f.0, PA: %.2f%, Rollbacks: %f.0, PR: %.2f, "
			"AVG Delta:%.2f, Delta: %.2f\n", 
		((double)delta_time/1000.0), fee, fee/((double)delta_time/1000000.0), throughput, 
		ae, ae*100.0/fee, sr, sr*100.0/fee, 
		(tm/active_threads)/1000.0, delta_time/1000.0);
	 	start_macro_time = 0; // reset computation
		sampling_time = 0;

		return throughput;
	}
}

/* 
also this routine must be executed by a single thread 
This is invoked at the end of the binding	
*/
void start_sampling(){
        if(start_macro_time == 0)       {
                __sync_fetch_and_add(&last_sample_id, 1);
                start_macro_time = clock_us();
        }
}

void process_statistics(){
	current_time = clock_us();
	
	if(!sampling_enabled && start_macro_time != 0){
		if((current_time - start_macro_time) >= MICRO_PERIOD_DELAY 
		&& (current_time - start_macro_time) <= (MICRO_PERIOD_DELAY + MICRO_PERIOD_US)){
			if(master_thread()) 	sampling_time = current_time;		
 			if(sampling_time != 0)	OnSamplingPeriodBegin();
		}
	}

	if(
		sampling_enabled 
		&& ((current_time - sampling_time) >=  (MICRO_PERIOD_US)) ){
		  if(master_thread())	delta_time = current_time - sampling_time;
	 	  OnSamplingPeriodEnd();
	}

}
