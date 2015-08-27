#include <math.h>
#include "application.h"

#define PI_GREEK  3.14159265358979323846264338327950288
#define BOARDING_TIME   120 //120 minutes
#define SECTOR_WITH_AIRPORT_DELAY 10
#define SECTOR_DELAY        20
#define AVERAGE_SERVICE_TIME 40 //30 seconds
#define AVERAGE_PERCENTAGE_OF_PASSENGER 0.7 //70%
/*
 *
 * @brief: Function needed to handle aircraft traffic
 *
 *  Hp: boarding time to fill an aircraft : 180 m
 *  Hp: every 120 min all the airports fill their queues
 */
static double Gaussian(double m, double s)
/* ========================================================================
 * Returns a normal (Gaussian) distributed real number.
 * NOTE: use s > 0.0
 *
 * Uses a very accurate approximation of the normal idf due to Odeh & Evans,
 * J. Applied Statistics, 1974, vol 23, pp 96-97.
 * ========================================================================
 * copied from http://www.cs.wm.edu/~va/software/park/
 */
{
    const double p0 = 0.322232431088;     const double q0 = 0.099348462606;
    const double p1 = 1.0;                const double q1 = 0.588581570495;
    const double p2 = 0.342242088547;     const double q2 = 0.531103462366;
    const double p3 = 0.204231210245e-1;  const double q3 = 0.103537752850;
    const double p4 = 0.453642210148e-4;  const double q4 = 0.385607006340e-2;
    double u, t, p, q, z;

    u = Random();
    if (u < 0.5)
        t = sqrt(-2.0 * log(u));
    else
        t = sqrt(-2.0 * log(1.0 - u));
    p = p0 + t * (p1 + t * (p2 + t * (p3 + t * p4)));
    q = q0 + t * (q1 + t * (q2 + t * (q3 + t * q4)));

    if (u < 0.5)
        z = (p / q) - t;
    else
        z = t - (p / q);

    return (m + s * z);
}

static double getPassengerForAircraft(aircraft_object_t *aircraft){

    double r = Random();
    double mean_passenger = 0;
    if(r<0.7){
         mean_passenger = AVERAGE_PERCENTAGE_OF_PASSENGER * (int)aircraft->max_passengers;
    }
    else{
         mean_passenger = r * (int)aircraft->max_passengers;
    }
    double sigma = 1/((aircraft->max_passengers) * (sqrt(2*PI_GREEK)));
    return Gaussian(mean_passenger,sigma);
}


/*
 *
 * Gaussian height: a = 1/(sigma*sqrt(2pi)) ---> sigma = 1/max_passenger*(sqrt(2pi))
 * Gaussian mean: center of gaussian ::  70% of max passenger
 *
 */

/*
 *
 * Exponential Distribution for each passenger  == total delay
 *
 * Hp: num_of_passenger /  3600s
 *
 *
 */
static int computeDelayForEachPassenger(aircraft_object_t *aircraft,int check_in_capacity){

    int num_passenger = (int)getPassengerForAircraft(aircraft);
    aircraft->number_of_passengers = num_passenger;
    double rand;
    //double lambda = (num_passenger/BOARDING_TIME);
    //int ideal_time = num_passenger *TIME_TO_DO_CHECK_IN;
    //int total_delay = 0;
    /*
   */
    double lambda = (double)(num_passenger/((double)BOARDING_TIME));
    lambda/=60;
    double service_rate = (double)(1.0/(double)AVERAGE_SERVICE_TIME);
    //service_rate*=(double)NUMBER_OF_QUEUES;
    service_rate*=(double)check_in_capacity;
    double utilization_factor = (lambda/service_rate);
    double avg_response_time = ((1/service_rate)/(1-utilization_factor));
    int total_delay = 0; //= (num_passenger * avg_response_time)/60;
    int k;
    for(k=0;k<num_passenger;k++){
        total_delay+=avg_response_time;
        rand = Random();  // For security pass check-in
        if(rand>=0.1){
            total_delay+=10;
        }
        else{
            total_delay+=60;
        }
    }
    total_delay = (int)(total_delay/60);
    //printf("Total delay is ----------------------------------> %i minuti ideal time is %i minuti, lamba:%f,service_rate:%f,avg_response_time %f \n",total_delay,BOARDING_TIME,lambda,service_rate,avg_response_time);
    if(total_delay>BOARDING_TIME){
        return total_delay - BOARDING_TIME;
    }
    return 0;

}

static _time addDelayToTime(_time time, int delay){
    if(delay == 0){
        return time;
    }
    if(time.minute+delay>=60){
        time.hour+=1;
    }
    time.minute = (time.minute+delay)%60;
    return time;
}

static void computeDelayForSector(_time time, sector_lp_state_type *sector_state){

    if(sector_state->aircraft_within_sector!=-1){
        // Sector contains aircraft
        time = addDelayToTime(time,SECTOR_WITH_AIRPORT_DELAY);
    }
    else{
        time = addDelayToTime(time,SECTOR_DELAY);
    }
}


static bool compareTime(_time t1,_time t2){
    if(t1.hour == t2.hour && t1.minute == t1.minute){
        return true;
    }
    return false;

}
static _time addLandingDelay(airport_lp_state_type *state,_time time){
    int lambda = (state->runway_capacity + state->total_airstrips)/(state->total_airstrips) ;
    int delay = (int)Expent(lambda);
    time = addDelayToTime(time,delay);
    return time;
}

static int computeDelay(_time end,_time start){
    if((int)end.hour < (int)start.hour){
        printf("Delay.hour cannot be less than start.hour \n");
        return -1;

    }else if(end.hour == start.hour){
        if(end.minute < start.minute){
            printf("Delay and start at same hour, but Delay.min cannot be less than start.min \n");
            return -1;
        }
        return end.minute - start.minute;
    }
    else{
        if(end.minute == start.minute){
            return (end.hour - start.hour)*60;

        }else{
            return (end.minute - start.minute) + 60*(end.hour - start.hour);
        }
    }

}
