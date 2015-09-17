//
//  application.c
//  Avionic_Sim
//
//  Created by Roberto Palamaro on 11/02/15.
//  Copyright (c) 2015 Roberto Palamaro. All rights reserved.
//

#include "application.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "functions.c"



void ProcessEvent(unsigned int me, simtime_t now, int event_type, event_content_type *event_content, size_t size, void * ptr){

    int i;
    event_content_type *new_event_content;
    airport_lp_state_type *airport_state;
    sector_lp_state_type *sector_state;
    switch (event_type) {
        case INIT:
            if(n_prc_tot < (AIRPORT_NUMBER + NUMBER_OF_SECTOR)){
                printf("Number of logical processes must be = (number of airports + number of sectors) = %i \n",AIRPORT_NUMBER+NUMBER_OF_SECTOR);
                fflush(stderr);
                exit(EXIT_FAILURE);
            } else if(n_prc_tot > (AIRPORT_NUMBER + NUMBER_OF_SECTOR)){
                printf("Number of logical processes must be = (number of airports + number of sectors) = %i \n",AIRPORT_NUMBER+NUMBER_OF_SECTOR);
                fflush(stderr);
                exit(EXIT_FAILURE);
            }
            if(me<AIRPORT_NUMBER){
                airport_state = (airport_lp_state_type*)ptr;
                airport_state = (airport_lp_state_type*)malloc(sizeof(airport_lp_state_type));
                bzero(airport_state,sizeof(airport_lp_state_type));
                airport_state->airport_ID = (int)me;
                airport_state->lvt = 0;
                init_airport_state(me,airport_state);
                SetState(airport_state);
                simtime_t ts;
                fprintf(airport_state->fp,"*********************************************Airport LP:%i********************************************************************************* \n",me);
                fprintf(airport_state->fp,"Airport name: %s, Airstrips Capacity:%i, Run way Capacity:%i, departues scheduled: %i \n",
                        airport_state->name,
                        airport_state->total_airstrips,
                        airport_state->runway_capacity,
                        airport_state->departures_scheduled);
                aircraft_queue_element *el = airport_state->aircrafts_departing;
                while(el){
                    printReadableAircraft(el->aircraft,airport_state->fp);
                    el = el->next_aircraft;
                }
                registerAirportToSector(me,now,airport_state);
                ts = now + 10*Random();
                ScheduleNewEvent(me,ts, START_DEPARTING_AIRCRAFT, NULL,0);
		ScheduleNewEvent(me, now + Expent(KEEP_ALIVE_MEAN), KEEP_ALIVE, NULL, 0);

            }else{
                sector_state = (sector_lp_state_type*)ptr;
                sector_state = (sector_lp_state_type *)malloc(sizeof(sector_lp_state_type));
                bzero(sector_state, sizeof(sector_lp_state_type));
                sector_state->lvt = now;
                init_sector_state(me, sector_state);
                SetState(sector_state);
                fprintf(sector_state->fp,"***********************************************Sector LP:%i**********************************************************************************\n",me);
                printReadableNeighborsFromSector(me,sector_state->neighbors,sector_state->fp);
                ScheduleNewEvent(me, (simtime_t)(now + 10*Random()), IDLE, NULL,0);
            }
			break;
        case REGISTER_TO_SECTOR:
            sector_state = (sector_lp_state_type*)ptr;
            sector_state->airport_within_sector = (int)event_content->current_time.hour;
            fprintf(sector_state->fp,">>> Sector %i has airport with LP number = %i \n \n \n",me,sector_state->airport_within_sector);
            fprintf(sector_state->fp,"************************************************FLYING HISTORY*********************************************************************************\n");
            sector_state->lvt = now;
            ScheduleNewEvent(me, (simtime_t)(now + 10*Random()), IDLE, NULL,0);
            break;
        case START_DEPARTING_AIRCRAFT:
            airport_state = (airport_lp_state_type*)ptr;
            airport_state->lvt = now;
            startDepartingAircraftProcedure(me,now,airport_state);
            break;
        case END_DEPARTING_AIRCRAFT:
            airport_state = (airport_lp_state_type*)ptr;
            airport_state->lvt = now;
            endDepartingProcedure(me,now,airport_state,event_content);
            airport_state->departures_scheduled -=1;
            break;
        case TAKE_OFF:
            sector_state = (sector_lp_state_type*)ptr;
            sector_state->lvt = now;
            fprintf(sector_state->fp,"SectorID:%i(TAKING OFF) Aircraft:%s,from %s -> destination:%s launched at current time (%i:%i) sector_time:%lu \n",
                    me,
                    event_content->aircraft->company,
                    event_content->aircraft->startFrom,
                    event_content->aircraft->leaving,
                    event_content->current_time.hour,event_content->current_time.minute,(long unsigned int)now);
            ScheduleNewEvent(me,(simtime_t)(now + Expent(SECTOR_WITH_AIRPORT_DELAY)), FLYING, event_content,sizeof(event_content_type));
        break;
        case FLYING:
            sector_state = (sector_lp_state_type*)ptr;
            sector_state->lvt = now;
            handleFlying(sector_state,event_content);
            break;
        case LANDING:
            airport_state = (airport_lp_state_type*)ptr;
            airport_state->lvt = now;
            fprintf(airport_state->fp,"\n \t \t Airport%i..Trying to landing Aircraft:%s type %s, from %s\n",me,event_content->aircraft->company,event_content->aircraft->type,event_content->aircraft->startFrom);
            if(airport_state->busy_airstrips == airport_state->total_airstrips){
                fprintf(airport_state->fp,"AirportID:%i, name %s all the airstrips are busy...Try later..resend to sector %i\n",me,airport_state->name,getLP_By_Position(airport_state->position->x,airport_state->position->y));
                ScheduleNewEvent(getLP_By_Position(airport_state->position->x,airport_state->position->y),(simtime_t)now+5*Random(),FLYING,event_content,sizeof(event_content_type));
                ScheduleNewEvent(me, (simtime_t)(now + Expent(airport_state->total_airstrips/(airport_state->total_airstrips+airport_state->runway_capacity))), IDLE, NULL,0);
                break;
            }
            airport_state->busy_airstrips +=1;
            event_content->current_time = addLandingDelay(airport_state,event_content->current_time);
            fprintf(airport_state->fp,"\n******************************************************************************************************************************** \n");
            fprintf(airport_state->fp,"AirportID:%i=%s..ok to landing..Busy_Airstrips=%i,Total Airstrip:%i, Aircraft:%s arrived at (%i:%i) scheduled (%i:%i) \n\n",
                   me,
                   airport_state->name,
                   airport_state->busy_airstrips,
                   airport_state->total_airstrips,
                   event_content->aircraft->company,
                   event_content->current_time.hour,
                   event_content->current_time.minute,
                   event_content->aircraft->arrival_hour.hour,
                   event_content->aircraft->arrival_hour.minute);
                   //computeDelay(event_content->current_time,event_content->aircraft->arrival_hour));
            fprintf(airport_state->fp,"********************************************************************************************************************************\n\n");
            airport_state->number_of_incoming_aircraft -= 1;
            ScheduleNewEvent(me, (simtime_t)(now + Expent(airport_state->total_airstrips/(airport_state->total_airstrips+airport_state->runway_capacity))), IDLE, NULL,0);
            break;
        case IDLE:
            if(me<AIRPORT_NUMBER){
                airport_state  = (airport_lp_state_type*)ptr;
                airport_state->lvt = now;
                if(airport_state->busy_airstrips > 0){
                    airport_state->busy_airstrips -=1;
                }
                if(airport_state->departures_scheduled==0 && airport_state->number_of_incoming_aircraft==0 && airport_state->ack_flag == false){
                    int k;
                    for (k=0;k<(AIRPORT_NUMBER);k++){
                        ScheduleNewEvent(k,(simtime_t)(now + 10*Random()),INFORM_COMPLETE,NULL,0);
                    }
                    airport_state->ack_flag = true;
                    break;
                }
            }else{
                sector_state = (sector_lp_state_type*)ptr;
                sector_state->lvt = now;
            }
            ScheduleNewEvent(me, (simtime_t)now + 10*Random(), IDLE, NULL,0);
            break;
        case ACK_ALL_SECTOR:
            sector_state = (sector_lp_state_type*)ptr;
            sector_state->lvt = now;
            sector_state->number_of_ack +=1;
            sector_state->t_flag = true;
            ScheduleNewEvent(me, (simtime_t)now + 10*Random(), IDLE, NULL,0);
            break;
        case INFORM_COMPLETE:
            airport_state  = (airport_lp_state_type*)ptr;
            airport_state->lvt = now;
            airport_state->num_of_ack += 1;
            ScheduleNewEvent(me, (simtime_t)now + 10*Random(), IDLE, NULL,0);
            break;

	case KEEP_ALIVE:

		#ifdef GLOBVARS
		#else
		for(i = 0; i < AIRPORT_NUMBER; i++) {
            		ScheduleNewEvent(me, (simtime_t)now + 0.5*Random(), UPDATE_DELAYS, NULL,0);
		}
		#endif

		ScheduleNewEvent(me, now + Expent(KEEP_ALIVE_MEAN), KEEP_ALIVE, NULL, 0);
	    	break;

	case UPDATE_DELAYS:
		

		break;

	default:
		abort();

    }
}


bool OnGVT(unsigned int me,void *snapshot) {


    if(me<AIRPORT_NUMBER){
        airport_lp_state_type *state  = (airport_lp_state_type*)snapshot;
    if(state->lvt > TERMINATION_TIME)
	return true;
    return false;
        if(state->number_of_incoming_aircraft == 0 && state->departures_scheduled == 0){ // && state->num_of_ack == AIRPORT_NUMBER){
            printf("Airport %i ok to end \n",me);
            if(me==0){
                int k;
//                for (k=AIRPORT_NUMBER;k<(NUMBER_OF_SECTOR+AIRPORT_NUMBER);k++){
//                    ScheduleNewEvent(k,(simtime_t)(state->lvt + 10*Random()),ACK_ALL_SECTOR,NULL,0);
//                }
            }
            fprintf(state->fp,"Airport %i....Shutting Down \n",me);
//            fclose(state->fp);
            return true;
        }else{
            return false;
        }

    }else{
        sector_lp_state_type *state = (sector_lp_state_type*)snapshot;
    if(state->lvt > TERMINATION_TIME)
	return true;
    return false;
        if(state->t_flag == true){
             printf("Sector %i..ok to end\n",me);
//             fclose(state->fp);
             return true;
        }else{
            return false;
        }

    }

}
