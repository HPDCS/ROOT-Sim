#pragma once

#define TEMPORAL_PERIOD 3

#define SCORE_HIGHER_THRESHOLD 4
#define SCORE_LOWER_THRESHOLD -4


#define UPPER_THRESHOLD_MODIFIER -1
#define LOWER_THRESHOLD_MODIFIER 1


#define BUBBLE_TURNAROUND_ID 0
#define BUBBLE_TURNAROUND_MODIFIER -3
#define BUBBLE_TURNAROUND_AVG_THRESHOLD 5

#define EMPTY_PT_ID 1
#define EMPTY_PT_MODIFIER 2
#define EMPTY_PT_AVG_THRESHOLD 0.4


#define STRAGGLERS_RATE_THRESHOLD 15.0
#define STRAGGLERS_RATE_MODIFIER 2


extern void reset_score(void);
extern void modify_score(int modifier);
extern int get_score(void);
extern void exponential_moving_avg(double value, int id);
extern void evaluate_avg(int id);
extern void post_stragglers_percentage(double percentage);
