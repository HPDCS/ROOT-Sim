#include "init.h"
#include "application.h"
#include "functions.c"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

/**
 
 @brief: Init all the structures
 
*/


static void pushNeighborToTheEnd(sector_neighbor_t **head,int val){
    if(head == NULL){
        return;
    }
    if(val<-1 || val > (NUMBER_OF_SECTOR+AIRPORT_NUMBER)){
        return;
    }
    if((*head)->lp == -1){
        (*head)->lp = val;
        return;
    }
    sector_neighbor_t * new_neighbor = malloc(sizeof(sector_neighbor_t));
    new_neighbor->lp = val;
    new_neighbor->next = *head;
    *head = new_neighbor;

}
static int getNeighbors(sector_lp_state_type *sector_state){
    
    int i = sector_state->position->x;
    int j = sector_state->position->y;
    sector_state->neighbors = malloc(sizeof(sector_neighbor_t));
    bzero(sector_state->neighbors,sizeof(sector_neighbor_t));
    sector_state->neighbors->lp = -1;
    sector_state->neighbors->next = NULL;

    if((i>-1 && i<MATRIX_DIMENSION) && (j>-1 && j<MATRIX_DIMENSION)){
        int temp = 0;
        if (i+1<MATRIX_DIMENSION) {
            temp++;
            pushNeighborToTheEnd(&(sector_state->neighbors),getLP_By_Position(i+1,j));
        }
        if(i-1>-1){
            temp++;
            pushNeighborToTheEnd(&(sector_state->neighbors),getLP_By_Position(i-1,j));
        }
        if(j+1<MATRIX_DIMENSION){
            temp++;
            pushNeighborToTheEnd(&(sector_state->neighbors),getLP_By_Position(i,j+1));
        }
        if(j-1>-1){
            temp++;
            pushNeighborToTheEnd(&(sector_state->neighbors),getLP_By_Position(i,j-1));
        }
        if((i-1>-1 && i-1<MATRIX_DIMENSION ) && (j-1>-1 &&j-1<MATRIX_DIMENSION)){
            temp++;
            pushNeighborToTheEnd(&(sector_state->neighbors),getLP_By_Position(i-1,j-1));
        }
        if((i-1>-1 && i-1<MATRIX_DIMENSION ) && (j+1>-1 &&j+1<MATRIX_DIMENSION)){
            temp++;
            pushNeighborToTheEnd(&(sector_state->neighbors),getLP_By_Position(i-1,j+1));
        }
        if((i+1>-1 && i+1<MATRIX_DIMENSION ) && (j+1>-1 &&j+1<MATRIX_DIMENSION)){
            temp++;
            pushNeighborToTheEnd(&(sector_state->neighbors),getLP_By_Position(i+1,j+1));
        }
        if((i+1>-1 && i+1<MATRIX_DIMENSION ) && (j-1>-1 &&j-1<MATRIX_DIMENSION)){
            temp++;
            pushNeighborToTheEnd(&(sector_state->neighbors),getLP_By_Position(i+1,j-1));
        }
        return temp;
    }
    else return -1;
    
    
}

static position_t* setSectorPosition(int lp_number){

    position_t *pos = malloc(sizeof(position_t));
    int sector_id = lp_number - AIRPORT_NUMBER;
    int mod = MATRIX_DIMENSION;
    int i;
    int temp = 0;
    for(i=0;i<NUMBER_OF_SECTOR;i++){
        if(i<mod){
             //printf("%i -----> (%i,%i) \n",j,j%3,temp);
        }else{
            temp = temp+1;
            //mod=2*mod;
            mod += MATRIX_DIMENSION;
            //printf("%i -----> (%i,%i) \n",j,j%3,temp);
        }
        if(i==sector_id){
            pos->x = i%MATRIX_DIMENSION;
            pos->y = temp;
            break;
        }
    }
    return pos;


}

  
static position_t* convertNameToPosition(char *name){
  
  	position_t *pos = malloc(sizeof(position_t));
  	if(strcmp(name,FIUMICINO)==0){
        pos->x = 5;
        pos->y = 1;
	}else if(strcmp(name,CIAMPINO)==0){
        pos->x = 4;
        pos->y = 3;
	}else if(strcmp(name,FIRENZE)==0){
        pos->x = 2;
		pos->y = 1;
	}else if(strcmp(name,BOLOGNA)==0){
        pos->x = 1;
        pos->y = 5;
	}else if(strcmp(name,NAPOLI)==0){
        pos->x = 6;//2;
        pos->y = 3;//1;
    }else if(strcmp(name,MALPENSA)==0){
        pos->x = 0;//0;
        pos->y = 2;//0;
    }else if(strcmp(name,BRESCIA)==0){
        pos->x = 0;//0;
        pos->y = 4;//0;

    }else if(strcmp(name,BARI)==0){
        pos->x = 5;//0;
        pos->y = 5;//0;

    }else if(strcmp(name,CATANIA)==0){
        pos->x = 9;//0;
        pos->y = 1;//0;

    }else if(strcmp(name,LIVORNO)==0){
        pos->x = 3;//0;
        pos->y = 0;//0;

    }else{
       printf("Invalid position \n");
       pos->x = -1;
       pos->y = -1;
    }
  	return pos;
  
}

 void init_sector_state(int me, sector_lp_state_type *sector_state) {

    sector_state->lp_type = SECTOR_LP;
    char buff[MAX_BUFFER/2];
    sprintf(buff,"logSector%i.txt",me);
    sector_state->fp = fopen(buff,"w");
    sector_state->sector_ID = me;
    sector_state->position = setSectorPosition(me);
    sector_state->nr_neighbors = getNeighbors(sector_state);
    sector_state->aircraft_within_sector = 0;
    sector_state->airport_within_sector = -1;
    sector_state->current_aircrafts = NULL;
    sector_state->number_of_ack = 0;
    sector_state->t_flag = false;
    printf("Sector:%i state correctly initialized.\n",me);
  }

static _time* parseTime(char *time_string){

    char *time_s = time_string;
    char *time_component;
    char *rest;
    int i = 0;
    _time *time_to_fill = malloc(sizeof(_time));
    while((time_component=strtok_r(time_s,":",&rest))!=NULL){
        if(i==0){
            time_to_fill->hour = atoi(time_component);
        }
        else{
            time_to_fill->minute = atoi(time_component);
        }
        i++;
        time_s = rest;
    }
    return time_to_fill;
}

static aircraft_object_t* parseAircraft(char *aircraft){

    aircraft_object_t *ptr = malloc(sizeof(aircraft_object_t));
    bzero(ptr,sizeof(aircraft_object_t));
    char *aircraft_string = aircraft;
    char *aircraft_field;
    char *rest;
    int i = 0;
    while((aircraft_field = strtok_r(aircraft_string,",",&rest))!=NULL){
        switch(i){
             case 0:
                strcpy(ptr->company,aircraft_field);
                //strcpy(aircraft_obj.company,aircraft_field);
                break;
            case 1:
                strcpy(ptr->type,aircraft_field);
                //strcpy(aircraft_obj.type,aircraft_field);
                break;
            case 2:
                ptr->max_passengers = atoi(aircraft_field);
                //aircraft_obj.max_passengers = atoi(aircraft_field);
                break;
            case 3:
                ptr->destination_position = *(convertNameToPosition(aircraft_field));
                strcpy(ptr->leaving,aircraft_field);
                //aircraft_obj.current_position = *(convertNameToPosition(aircraft_field));
                break;
            case 4:
                ptr->departure_hour = *(parseTime(aircraft_field));
                //strcpy(aircraft_obj.company,aircraft_field);
                break;
            case 5:
                ptr->arrival_hour =*(parseTime(aircraft_field));
                break;
        }
        i++;
        aircraft_string = rest;
    }
    return ptr;
}

static void pushAnAircraftToAirport(aircraft_queue_element **head,aircraft_object_t* aircraft){
    //printf("Push an aircraft \n");
    if((*head)->aircraft == NULL && (*head)->next_aircraft == NULL){
      //  printf("NULL case\n");
        (*head)->aircraft = aircraft;
        return;
    }
    aircraft_queue_element* new_aircraft = malloc(sizeof(aircraft_queue_element));
    bzero(new_aircraft,sizeof(aircraft_queue_element));
    new_aircraft->aircraft = aircraft;
    new_aircraft->next_aircraft = *head;
    *head = new_aircraft;
}

static void addAircrafts(char *file_line,airport_lp_state_type *state){
		
    /*
		aircraft_object_t *aircraft_list;
		int i,z;
		char *single_aircraft;
        char *rest;
        char *single_line = file_line;

        if(state->capacity!=0){
			aircraft_list= malloc(state->capacity*sizeof(aircraft_object_t));
        }
		else{
			printf("Capacity is not setted for airport %i and name %s\n",state->airport_ID,state->name);
			aircraft_list = malloc(50*sizeof(aircraft_object_t));
            bzero(aircraft_list,50*sizeof(aircraft_object_t));
		}
		z = 0;
        while((single_aircraft=strtok_r(single_line,";",&rest))!=NULL){
            //parseAircraft(single_aircraft,aircraft_list[z]);
            aircraft_list[z] = *(parseAircraft(single_aircraft));
            aircraft_list[z].current_position = *(state->position);
            z++;
            single_line = rest;
        }
		if(realloc(aircraft_list,z*sizeof(aircraft_object_t))==(void*)NULL){
			printf("Realloc of aircraft list fails \n");
		}
		state->departures_remained =z;
        state->aircraft_departing = aircraft_list;
        printf("All aircraft has been loaded. \n");*/
    int z = 0;
    char *single_aircraft;
    char *rest;
    char *single_line = file_line;
    state->aircrafts_departing = malloc(sizeof(aircraft_queue_element));
    bzero(state->aircrafts_departing,sizeof(aircraft_queue_element));
    state->aircrafts_departing->next_aircraft = NULL;
    state->aircrafts_departing->aircraft = NULL;
    aircraft_object_t *aircraft;
    while((single_aircraft=strtok_r(single_line,";",&rest))!=NULL){
        aircraft = parseAircraft(single_aircraft);
        aircraft->current_position = *(state->position);
        strcpy(aircraft->startFrom,state->name);
        pushAnAircraftToAirport(&(state->aircrafts_departing),aircraft);
        z++;
        single_line = rest;
    }
    state->departures_scheduled = z;
    fprintf(state->fp,"Airport %i: All aircraft has been loaded \n",state->airport_ID);

 }

static void start_to_parse(FILE *fp,int me,airport_lp_state_type *state){
     char *tokens;
     char *rest;
     int i,z;
     char *line = NULL;
     size_t len = 0;
     ssize_t read;
     i = 0;
     while((read = getline(&line, &len, fp)) != -1){
         if(i==me){
            z=0;
            while((tokens = strtok_r(line, "-",&rest))!=NULL){
                switch (z) {
                    case 0:
                        strcpy(state->name,tokens);
                        state->position = convertNameToPosition(tokens);
                        break;
                    case 1:
                        state->total_airstrips = atoi(tokens);
                        break;
                    case 2:
                        state->runway_capacity = atoi(tokens);
                        break;
                    case 3:
                        state->number_of_incoming_aircraft =  atoi(tokens);
                        break;
                    case 4:
                        addAircrafts(tokens,state);
                        break;
                }
                z++;
                line = rest;
            }
            break;
        }
        i++;
    }
    state->busy_airstrips = 0;
    state->busy_runways = 0;
 }

 void init_airport_state(int me,airport_lp_state_type *state){
    FILE* fp = fopen(AIRPORTS_PATH,"r");
    if(fp==NULL){
        printf("File pointer is null \n");
        fflush(stderr);
        exit(EXIT_FAILURE);
    }
    //state->termination_flag = false;
    char buff[MAX_BUFFER];
    sprintf(buff,"logAirport%i.txt",me);
    state->fp = fopen(buff,"w");
    state->ack_flag = false;
    state->num_of_ack = 0;
    state->term_flag = false;
    start_to_parse(fp,me,state);
    fclose(fp);
    printf("Airport%i state correctly initialized \n",me);
}

