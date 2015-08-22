//
//  application.h
//  Avionic_Sim
//
//  Created by Roberto Palamaro on 11/02/15.
//  Copyright (c) 2015 Roberto Palamaro. All rights reserved.
//
#pragma once
#ifndef _AVIONIC_APPLICATION_H
#define _AVIONIC_APPLICATION_H
#include <ROOT-Sim.h>

#define MAX_BUFFER         256
#define MATRIX_DIMENSION   10
#define NUMBER_OF_SECTOR   MATRIX_DIMENSION*MATRIX_DIMENSION
#define AIRPORT_NUMBER 10

#define NAME_AIRPORT_LENGTH 32
#define NAME_AIRCRAFT_LENTGH 16
#define TYPE_AIRCRAFT_LENTGH 8
#define AIRPORT_CAPACITY	10
/*
 *************  EVENTS DEFINITON ********************
*/
#define REGISTER_TO_SECTOR 1
#define BOARDING_EVENT 2
#define START_DEPARTING_AIRCRAFT 3
#define END_DEPARTING_AIRCRAFT 4
#define ARRIVAL 5
#define CHANGE_SECTOR 6
#define IDLE 7
#define DO_NOTHING 8
#define COMPUTE_DELAYS 9
#define TAKE_OFF 10
#define LANDING 11
#define FLYING 12
#define ACK_ALL_SECTOR 13
#define INFORM_COMPLETE 14

/******************************************************/


typedef struct position
{   int x;
    int y;
} position_t;

typedef struct _Time{
    int hour;
    int minute;
}_time;

typedef struct _neighbor {
    int 			lp;
    struct _neighbor *next;
} sector_neighbor_t;


typedef struct _aircraft_object
{
    char company[MAX_BUFFER];
    char type[MAX_BUFFER];
    int max_passengers;
    int number_of_passengers;
    char startFrom[MAX_BUFFER/2];
    char leaving[MAX_BUFFER/2];
    _time departure_hour;
    _time arrival_hour;
    _time real_departures;
    position_t current_position;
    position_t destination_position;
    
}aircraft_object_t;

typedef struct _aircraft_queue{
    aircraft_object_t *aircraft;
    struct _aircraft_queue* next_aircraft;
}aircraft_queue_element;



typedef struct _event_content_type
{
    _time current_time;
    aircraft_object_t *aircraft;
}event_content_type;

typedef struct _sector_lp_state
{
    simtime_t lvt;
    int lp_type;
    int sector_ID;
    FILE *fp;
    int airport_within_sector;
    int nr_neighbors;
    int aircraft_within_sector;
    bool t_flag;
    int number_of_ack;
    position_t *position;
    sector_neighbor_t *neighbors;
    aircraft_queue_element *current_aircrafts;
    
}sector_lp_state_type;

typedef struct _airport_lp_state{

	simtime_t lvt;
	int lp_type;
	int airport_ID;
	char name[NAME_AIRPORT_LENGTH];
    FILE *fp;
    int departures_scheduled;
    int total_airstrips;
    int busy_airstrips;
    bool ack_flag;
    bool term_flag;
    int num_of_ack;
    int busy_runways;
    int aircraft_on_runway;
    int runway_capacity;
    int number_of_incoming_aircraft;
    position_t *position;
    aircraft_queue_element *aircrafts_departing;
    aircraft_queue_element *aircrafts_arrived;

}airport_lp_state_type;

#endif
