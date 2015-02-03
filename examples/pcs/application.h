#include <ROOT-Sim.h>



/* DISTRIBUZIONI TIMESTAMP */
#define UNIFORM		0
#define EXPONENTIAL	1
#define DISTRIBUTION	1


#define CHECK_FADING_TIME	10
#define COMPLETE_CALLS		5000
#ifndef TA
#define TA			1.2
#endif
#define TA_DURATION		120
#define CHANNELS_PER_CELL	1000
#define TA_CHANGE		300.0

#define	CELL_CHANGE_DISTRIBUTION	EXPONENTIAL
#define DURATION_DISTRIBUTION		EXPONENTIAL


/* Channel states */
#define CHAN_BUSY	1
#define CHAN_FREE	0

/* EVENT TYPES - PCS */
#define START_CALL	20
#define END_CALL	21
#define HANDOFF_LEAVE	30
#define HANDOFF_RECV	31
#define FADING_RECHECK	40
#define DUMMY_READ	60
#define DUMMY_WRITE	61

#define FADING_RECHECK_FREQUENCY	300	// Every 5 Minutes

#define MSK 0x1
#define SET_CHANNEL_BIT(B,K) ( B |= (MSK << K) )
#define RESET_CHANNEL_BIT(B,K) ( B &= ~(MSK << K) )
#define CHECK_CHANNEL_BIT(B,K) ( B & (MSK << K) )

#define BITS (sizeof(int) * 8)

#define CHECK_CHANNEL(P,I) ( CHECK_CHANNEL_BIT(						\
	((unsigned int*)(((lp_state_type*)P)->channel_state))[(int)((int)I / BITS)],	\
	((int)I % BITS)) )
#define SET_CHANNEL(P,I) ( SET_CHANNEL_BIT(						\
	((unsigned int*)(((lp_state_type*)P)->channel_state))[(int)((int)I / BITS)],	\
	((int)I % BITS)) )
#define RESET_CHANNEL(P,I) ( RESET_CHANNEL_BIT(						\
	((unsigned int*)(((lp_state_type*)P)->channel_state))[(int)((int)I / BITS)],	\
	((int)I % BITS)) )


// Message exchanged among LPs
typedef struct _event_content_type {
	unsigned int cell;
	unsigned int from;
	simtime_t sent_at;
	int channel;
	simtime_t   call_term_time;
} event_content_type;

#define CROSS_PATH_GAIN		0.00000000000005
#define PATH_GAIN		0.0000000001
#define MIN_POWER		3
#define MAX_POWER		3000
#define SIR_AIM			10

// Taglia di 16 byte
typedef struct _sir_data_per_cell{
    double fading;
    double power;
} sir_data_per_cell;

// Taglia di 16 byte
typedef struct _channel{
	int channel_id;
	sir_data_per_cell *sir_data;
	struct _channel *next;
	struct _channel *prev;
} channel;


typedef struct _lp_state_type{
	unsigned int channel_counter;
	unsigned int arriving_calls;
	unsigned int complete_calls;
	unsigned int blocked_on_setup;
	unsigned int blocked_on_handoff;
	unsigned int leaving_handoffs;
	unsigned int arriving_handoffs;
	unsigned int cont_no_sir_aim;
	unsigned int executed_events;

	simtime_t lvt;

	double ta;
	double ref_ta;
	double ta_duration;
	double ta_change;

	int channels_per_cell;
	int total_calls;

	bool check_fading;
	bool fading_recheck;
	bool variable_ta;

	unsigned int *channel_state;
	struct _channel *channels;
} lp_state_type;




double recompute_ta(double ref_ta, simtime_t now);
double generate_cross_path_gain(lp_state_type *state);
double generate_path_gain(lp_state_type *state);
void deallocation(unsigned int lp, lp_state_type *state, int channel, event_content_type *, simtime_t);
int allocation(lp_state_type *state);


extern int channels_per_cell;


