#pragma once

typedef struct stats{   
    int collector;                     // While set to 1 thread collects data for this round  
    int total_commits;                 // Defined as number of commits for the current round
    int commits;                       // Number of commits in the current round
    int aborts;                        // Number of aborts in the current round
    int nb_tx;                         // Number of total transactions started in this round 
    long start_energy;                 // Value of energy consumption taken at the start of the round, expressed in micro joule
    long end_energy;                   // Value of energy consumption taken at the end of the round, expressed in micro joule
    long start_time;        				   // Start time of the current round
    long end_time;        				     // End time of the current round
  } stats_t;
