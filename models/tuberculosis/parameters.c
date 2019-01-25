/*
 * parameters.c
 *
 *  Created on: 01 ago 2018
 *      Author: andrea
 */

#include "parameters.h"

/* Initial values */
const unsigned num_healthy = 1529469;
const unsigned num_infected = 68491;
const unsigned num_sick = 39;
const unsigned num_treatment = 161;
const unsigned num_treated = 1840;

/* Defines age_groups considered: 0-4, 5-14, ...*/
const unsigned age_groups[AGE_GROUPS] = {0, 5, 15, 25, 35, 45, 55, 65, 75, 85, 90};

/* Probability of a generic infected/sickened to be on a given age group */
const double p_infected_age[AGE_GROUPS-1] = { 0.0220, 0.0778, 0.1979, 0.4501, 0.7277, 0.9104, 0.9679, 0.9865, 0.9983,
		1.0 };
const double p_sickened_age[AGE_GROUPS-1] = { 0.0270, 0.0585, 0.1978, 0.4977, 0.7202, 0.8371, 0.9090, 0.9472, 0.9865,
		1.0 };

/* Probabilities of gender and origin */
const double p_infected_male = 0.6802;
const double p_infected_foreign = 0.7357;
const double p_sickened_male = 0.6978;
const double p_sickened_foreign = 0.7157;

/* Characteristic times in days */
const unsigned t_infected_max = 7 * 365;
const unsigned t_treatment_min = 15;
const unsigned t_treatment_max = 180;
const unsigned t_to_healthy = 720; // Time after diagnose to consider healthy.

/* Probabilities of risk factors */
const double p_HIV = 0.0042;
const double p_diabetes = 0.056;
const double p_smoking = 0.241;

/* Probability of smear positive case */
const double p_smear = 0.22;

/* Diagnose delay mean and std.
 * Mean has two components [autochton,foreign]
 */
const unsigned diagnose_mean[2] = { 42, 33 };
const unsigned diagnose_std = 4;

/* Probability to abandon treatment before finishing it */
const double p_abandon = 0.022;
const double p_relapse_min = 0.01;

/* Probability to infect an close individual */
const double p_infect = 0.06;

/* Parameters related to age class for new infected.
 * youngster (0-20), adult (20-60), elder (60-) */

/* Boundaries in days between age classes */
const unsigned age_class_boundaries[2] = { 20 * 365, 60 * 365 };

/* Probability to belong to a certain age class and gender
 * according to characteristics of the infecting case.
 * 			   [youngster] [male adult] [female adult] [elder] (Case)
 * [youngster]
 * [adult]
 * [elder]
 * (Contact)
 */
const double p_contacts_age_class[4][3] = {{0.30, 0.95, 1.0}, {0.05, 0.99, 1.00}, {0.12, 0.97, 1.00}, {0.05, 0.50, 1.00}};

/* Age groups in each age class and probability of each one */
const double p_contacts_age_groups[3][4] = {{0.1970, 0.4091, 0.6970, 1.0000}, {0.2581, 0.6138, 0.8557, 1.0000}, {0.6667, 0.8788, 1.0000, 1.0000}};
const unsigned contacts_age_groups[3][5] = {{0, 5, 10, 15, 20}, {20, 30, 40, 50, 60}, {60, 70, 80, 90, 90}};

/* Probability of origin and gender of new infected depending
 * of origin and gender of the infector
 * 					[Native male] [Native female] [Foreign male] [Foreign female] (Case)
 * [Native male]
 * [Native female]
 * [Foreign male]
 * [Foreign female]
 * (Contact)
 */
const double p_origin_gender[4][4] = {{0.40, 0.68, 0.87, 1.00}, {0.35, 0.68, 0.84, 1.00}, {0.11, 0.14, 0.79, 1.00}, {0.08, 0.14, 0.63, 1.00}};

/* Probability to become sick depending on time infected */
const double p_sicken[7] = { 0.028, 0.022, 0.0168, 0.0123, 0.0085, 0.0055, 0.00320 };

/* Factors that, if present, multiply p_sicken */
const double f_sicken_child = 16; // [0,5) years
const double f_sicken_young = 5;  // [5,15) years
const double f_sicken_HIV = 18.5;
const double f_sicken_diabetes = 2;
const double f_sicken_smoking = 1.2;

/* Superior value of age group and probability to die for each group */
const unsigned p_die_age[2] = { 10 * 365, 65 * 365 };
const double p_die[3] = { 6.877e-7, 4.65e-6, 1.2299e-4 };
const double p_die_sick = { 2.192e-4 };
