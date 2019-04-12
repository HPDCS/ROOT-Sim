#pragma once

#include <scheduler/process.h>
#include <core/init.h>

extern __thread struct lp_struct *current;
extern __thread struct lp_struct context;
extern  simulation_configuration rootsim_config;
extern  unsigned int n_prc_tot;
extern  unsigned int n_prc;
extern struct lp_struct **lps_blocks;
extern __thread unsigned int __lp_counter;
