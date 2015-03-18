#include "application.h"
//#include "coordinator_f.c"
#include "delay_f.c"




static aircraft_object_t* removeLastAircraft(aircraft_queue_element **head){
    aircraft_queue_element*curr = *head;
    aircraft_queue_element *t;
    //printf("Remove last Aircraft method \n");
    if(curr == NULL){
      //  printf("No other aircraft \n");
        return NULL;
    }
    if(curr->next_aircraft == NULL){
        t = curr;
        *head = NULL;
        return t->aircraft;
    }
    while(curr->next_aircraft->next_aircraft != NULL){
        curr = curr->next_aircraft;
    }
    t = curr->next_aircraft;
    curr->next_aircraft = NULL;
    return t->aircraft;



}


static int getLP_By_Position(int i,int j){
    return (MATRIX_DIMENSION *j)+i+AIRPORT_NUMBER;
}


static int computeNextSectorBasedOnDestination(position_t current,position_t destination){

    int s_x,s_y,d_x,d_y;
    s_x = current.x;
    s_y = current.y;
    d_x = destination.x;
    d_y = destination.y;
    if((d_x < s_x) && (d_y < s_y)){
        return getLP_By_Position(s_x-1,s_y-1);
    }
    else if((d_x < s_x) && (d_y == s_y)){
        return getLP_By_Position(s_x-1,s_y);
    }
    else if((d_x == s_x) && (d_y < s_y)){
        return getLP_By_Position(s_x,s_y-1);
    }
    else if((s_x < d_x) && (s_y>d_y)){
        return getLP_By_Position(s_x+1,s_y-1);
    }
    else if((s_x > d_x) && (s_y<d_y)){
        return getLP_By_Position(s_x-1,s_y+1);
    }
    else if ((d_x == s_x) && (d_y == s_y)){
        return getLP_By_Position(d_x,d_y);
    }
    else if((d_x > s_x) && (d_y > s_y)){
        return getLP_By_Position(s_x+1,s_y+1);
    }
    else if((d_x > s_x) && (d_y == s_y)){
        return getLP_By_Position(s_x+1,s_y);
    }
    else if((d_x == s_x) && (d_y > s_y)){
        return getLP_By_Position(s_x,s_y+1);
    }
    return -1;

}

static bool comparePosition(position_t *p1,position_t *p2){
    if(p1->x == p2->x && p1->y == p2->y){
        return true;
    }
    return false;
}


static void handleFlying(sector_lp_state_type *sector_state,event_content_type *event){

    aircraft_object_t *aircraft = event->aircraft;
    position_t *current_position = sector_state->position;
    simtime_t timestamp;
    int sector_dest;
    fprintf(sector_state->fp,"\nSector::%i(FLYING) ...Flying,we are in pos:(%i,%i),destination is at (%i,%i) for Aircraft:%s from:%s to %s at time (%i:%i).\n",
               sector_state->sector_ID,
               sector_state->position->x,
               sector_state->position->y,
               aircraft->destination_position.x,
               aircraft->destination_position.y,
               aircraft->company,
               aircraft->startFrom,
               aircraft->leaving,
               event->current_time.hour,
               event->current_time.minute);
    if(comparePosition(current_position,&(aircraft->destination_position))){
        fprintf(sector_state->fp,"Sector::%i(LANDING MODE)..........We are arrived at destination! \n",sector_state->sector_ID);
        if(sector_state->airport_within_sector!=-1){
            fprintf(sector_state->fp,"Checking..if there is an airport...Yes..airportID is %i\n",sector_state->airport_within_sector);
            //timestamp = (sector_state->lvt + SECTOR_WITH_AIRPORT_DELAY*Random());
            timestamp = (simtime_t)(sector_state->lvt + Expent(SECTOR_WITH_AIRPORT_DELAY));
            fprintf(sector_state->fp,"Current Time -->(%i:%i)  \n",
                   event->current_time.hour,
                   event->current_time.minute);
            event->current_time = addDelayToTime(event->current_time,SECTOR_WITH_AIRPORT_DELAY);
            fprintf(sector_state->fp,"Adding this delay is %i -->(%i:%i)  \n \n \n",
                   SECTOR_WITH_AIRPORT_DELAY,
                   event->current_time.hour,
                   event->current_time.minute);
            ScheduleNewEvent(sector_state->airport_within_sector,timestamp,LANDING,event,sizeof(event_content_type));
            ScheduleNewEvent(sector_state->sector_ID, timestamp, IDLE, NULL,0);
        }
        else{
             fprintf(sector_state->fp,"Sector::%i..........Not have an airport -----------------------------\n",sector_state->sector_ID);
        }
    }
    else{
        fprintf(sector_state->fp,"Sector::%i(SWITCHING MODE) passing Aircraft:%s flying from:%s to %s to sector %i \n",
                sector_state->sector_ID,
                aircraft->company,
                aircraft->startFrom,
                aircraft->leaving,
                computeNextSectorBasedOnDestination(*(current_position),aircraft->destination_position));
        sector_dest = computeNextSectorBasedOnDestination(*(current_position),aircraft->destination_position);
        //timestamp = (sector_state->lvt + SECTOR_DELAY*Random());
        timestamp = (simtime_t)(sector_state->lvt + Expent(SECTOR_WITH_AIRPORT_DELAY));
        fprintf(sector_state->fp,"Current Time -->(%i:%i)  \n",
               event->current_time.hour,
               event->current_time.minute);
        event->current_time = addDelayToTime(event->current_time,SECTOR_DELAY);
        fprintf(sector_state->fp,"Adding delay.. is %i -->(%i:%i)  \n \n \n",
               SECTOR_DELAY,
               event->current_time.hour,
               event->current_time.minute);
        ScheduleNewEvent(sector_dest,timestamp,FLYING,event,sizeof(event_content_type));
        ScheduleNewEvent(sector_state->sector_ID, timestamp, IDLE, NULL,0);
    }

}




static void startDepartingAircraftProcedure(int me, simtime_t timestamp,airport_lp_state_type *state){
    fprintf(state->fp,"AirportID:%i name = %s:--------------------------->starting departing procedure at time %lu \n",me,state->name,(long unsigned )timestamp);
    aircraft_object_t *aircraft;
    event_content_type *ev;
    simtime_t ts;
    int delay;
    aircraft = removeLastAircraft(&(state->aircrafts_departing));
    if(aircraft == NULL){
        printf("Airport %i has completed all the departures \n",me);
        fprintf(state->fp,"...................................................\n");
        fprintf(state->fp,"AirportID:%i name = %s: No more aircraft to depart.\n ",me,state->name);
        fprintf(state->fp,"...................................................\n");
        ScheduleNewEvent(me, timestamp + 10*Random(), IDLE, NULL,0);
        return;
    }
    ev = malloc(sizeof(event_content_type));
    bzero(ev,sizeof(event_content_type));
    ev->aircraft = aircraft;
    ev->current_time = aircraft->departure_hour;
    delay = computeDelayForEachPassenger(ev->aircraft,state->total_airstrips);
    ev->current_time = addDelayToTime(ev->current_time,delay);
    fprintf(state->fp,"Airport %s:---> CHECK_IN DELAY---->%i, scheduled(%i:%i) current time(%i:%i) \n",state->name,delay,ev->aircraft->arrival_hour.hour,
            ev->aircraft->arrival_hour.minute,
            ev->current_time.hour,ev->current_time.minute);
    ts = timestamp + (delay*Random());
    ScheduleNewEvent(me,ts,END_DEPARTING_AIRCRAFT,ev,sizeof(event_content_type));
}

static void endDepartingProcedure(int me, simtime_t timestamp,airport_lp_state_type *state,event_content_type *event){
    fprintf(state->fp,"Aiport %s:--------------------------------->finalizing departing procedure at time %lu for aircraft %s\n",state->name,(long unsigned )timestamp,event->aircraft->company);
    /*
     *  Now we add the delay obtained from an exponential distribution given
     */
    int lambda = (state->runway_capacity + state->total_airstrips)/(state->runway_capacity) ;
    int delay = (int)Expent(lambda);
    int sector;
    event->current_time = addDelayToTime(event->current_time,delay);
    fprintf(state->fp,"Airport %s:------> DEPARTING DELAY---------->%i, current time(%i,%i) for aircraft %s \n",
           state->name,
           delay,
           event->current_time.hour,
           event->current_time.minute,
           event->aircraft->company);
    sector = getLP_By_Position(state->position->x,state->position->y);
    simtime_t ts = timestamp + (delay*Random());
    event->aircraft->real_departures = event->current_time;
    fprintf(state->fp,"Aircraft Ready to Take off...Scheduled at:(%i:%i) ---- real was(%i:%i) \n\n\n",
           event->aircraft->departure_hour.hour,
           event->aircraft->departure_hour.minute,
           event->aircraft->real_departures.hour,
           event->aircraft->real_departures.minute);
    ScheduleNewEvent(sector,ts,TAKE_OFF,event,sizeof(event_content_type));
    ScheduleNewEvent(me,ts, START_DEPARTING_AIRCRAFT, NULL,0);
}

static void registerAirportToSector(int me, simtime_t timestamp,airport_lp_state_type *state){

    if(state){
        event_content_type *ev = malloc(sizeof(event_content_type));
        bzero(ev,sizeof(event_content_type));
        ev->current_time.hour = me;
        ev->aircraft = NULL;
        int sector_dest = getLP_By_Position(state->position->x,state->position->y);
        fprintf(state->fp,"\t \t Airport %i name:%s register to Sector %i \n\n\n",me,state->name,sector_dest);
        ScheduleNewEvent(sector_dest,(simtime_t)(timestamp + 10*Random()),REGISTER_TO_SECTOR,ev,sizeof(event_content_type));
    }
}



static bool compareAircraft(aircraft_object_t* a1,aircraft_object_t*a2){
    if(strcmp(a1->company,a2->company)==0 && strcmp(a1->type,a2->type)==0 && a1->max_passengers == a2->max_passengers && a1->number_of_passengers == a2->number_of_passengers
            && comparePosition(&(a1->current_position),&(a2->current_position))){
        return true;
    }
    return false;
}



static void removeAircraftFromList(aircraft_queue_element **head,aircraft_object_t* aircraft){
    aircraft_queue_element *el = *head;
    aircraft_queue_element *temp = NULL;
    if(!aircraft){
        printf("Passed a null aircraft to remove \n");
        return;
    }
    if(el->next_aircraft == NULL && compareAircraft(el->aircraft,aircraft)){
        free(el->aircraft);
        el->aircraft = NULL;
        return;
    }

    while(el){
        if(compareAircraft(el->next_aircraft->aircraft,aircraft)){
            temp = el->next_aircraft;
            break;
        }
        el = el->next_aircraft;
    }
    el->next_aircraft = temp->next_aircraft;
    free(temp);

}

static void printReadableAircraft(aircraft_object_t *aircraft,FILE *fp){
    fprintf(fp,"------------------------------------------------------------------------------------------------------ \n");
    fprintf(fp,"Aircraft name:%s,capacity:%i, current pos:(%i,%i), destination pos:(%i,%i) starting from Airport:%s \n",
           aircraft->company,
           aircraft->max_passengers,
           aircraft->current_position.x,
           aircraft->current_position.y,
           aircraft->destination_position.x,
           aircraft->destination_position.y,
           aircraft->leaving);
    fprintf(fp,"Depart hour:(%i:%i),arrival hour:(%i:%i) \n",
           aircraft->departure_hour.hour,
           aircraft->departure_hour.minute,
           aircraft->arrival_hour.hour,
           aircraft->arrival_hour.minute);
    fprintf(fp,"------------------------------------------------------------------------------------------------------ \n");

}

static void printReadableNeighborsFromSector(int me,sector_neighbor_t *neighbors,FILE *fp){
    sector_neighbor_t *curr = neighbors;
    char buffer[MAX_BUFFER];
    char temp[MAX_BUFFER/2];
    sprintf(buffer,">>>  Neighbors for sector %i:",me);
    while(curr->next != NULL){
        sprintf(temp," %i ",curr->lp);
        strcat(buffer,temp);
        curr = curr->next;
    }
    sprintf(temp," %i ",curr->lp);
    strcat(buffer,temp);
    fprintf(fp,"%s\n",buffer);
}
