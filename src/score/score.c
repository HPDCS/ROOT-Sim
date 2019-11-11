
#include "score.h"
#include <stdio.h>
#include <stdbool.h>

static int score = 0;

///variables for exp moving average
static bool initialized[2] = {0};
static double first_avg[2][TEMPORAL_PERIOD] = {0};
static int index[2] = {0};
static double coefficient = (double) 2 / ((TEMPORAL_PERIOD)+1);
static double previous_EMA[2] = {0};

int get_score(void){
    return score;
}

void reset_score(void){
    score = 0;
}

void modify_score(int modifier){
    score += modifier;
}

void exponential_moving_avg(double value, int id){
    if(initialized[id] == false) {
	first_avg[id][index[id]] = value;
	index[id]++;
	if(index[id] == TEMPORAL_PERIOD) {
		for(int idx = 0; idx < TEMPORAL_PERIOD; idx++) {
			previous_EMA[id] += first_avg[id][idx];
		}
		previous_EMA[id] = previous_EMA[id] / TEMPORAL_PERIOD;
		initialized[id] = true;
	}
    }
    previous_EMA[id] = previous_EMA[id] + coefficient*(value-previous_EMA[id]);
    evaluate_avg(id);
}

void evaluate_avg(int id){
    int modifier;
    double threshold = (id == BUBBLE_TURNAROUND_ID) ? BUBBLE_TURNAROUND_AVG_THRESHOLD : EMPTY_PT_AVG_THRESHOLD;

    if(previous_EMA[id] > threshold){
        //printf("id:%d\n",id);
        modifier = (id == BUBBLE_TURNAROUND_ID) ? BUBBLE_TURNAROUND_MODIFIER : EMPTY_PT_MODIFIER;
        modify_score(modifier);
    }
}

void post_stragglers_percentage(double percentage){
    if(percentage > STRAGGLERS_RATE_THRESHOLD){
        modify_score(STRAGGLERS_RATE_MODIFIER);
    }
}
