
//
//  init.h
//  Avionic_Sim
//
//  Created by Roberto Palamaro on 11/02/15.
//  Copyright (c) 2015 Roberto Palamaro. All rights reserved.
//
#pragma once

#include <stdio.h>
#ifndef AIRPORTS_PATH
    #define AIRPORTS_PATH   "./Airports.txt"
#endif

#define AIRPORT_LP 100
#define SECTOR_LP  200

#include "application.h"

#define MALPENSA  "Malpensa"
#define FIRENZE   "Firenze"
#define BRESCIA   "Brescia"
#define BOLOGNA   "Bologna"
#define LIVORNO   "Livorno"
#define FIUMICINO "Fiumicino"
#define CIAMPINO  "Ciampino"
#define NAPOLI 	  "Napoli"
#define BARI      "Bari"
#define CATANIA   "Catania"



extern void init_airport_state(int me,airport_lp_state_type *airport_state);
extern void init_sector_state(int me, sector_lp_state_type *sector_state);


