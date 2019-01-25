/*
 * parameters.h
 *
 *  Created on: 01 ago 2018
 *      Author: andrea
 */

#ifndef MODELS_TUBERCOLOSIS_PARAMETERS_H_
#define MODELS_TUBERCOLOSIS_PARAMETERS_H_

#define AGE_GROUPS 11

/* Initial values */
extern const unsigned num_healthy;
extern const unsigned num_infected;
extern const unsigned num_sick;
extern const unsigned num_treatment;
extern const unsigned num_treated;

/* Defines age_groups considered: 0-5, 6-15, ...*/
extern const unsigned age_groups[AGE_GROUPS];

/* Probability of a generic infected/sickened to be on a given age group */
extern const double p_infected_age[AGE_GROUPS-1];
extern const double p_sickened_age[AGE_GROUPS-1];

/* Probabilities of gender and origin */
extern const double p_infected_male;
extern const double p_infected_foreign;
extern const double p_sickened_male;
extern const double p_sickened_foreign;

/* Characteristic times in days */
extern const unsigned t_infected_max;
extern const unsigned t_treatment_min;
extern const unsigned t_treatment_max;
extern const unsigned t_to_healthy; // Time after diagnose to consider healthy.

/* Probabilities of risk factors */
extern const double p_HIV;
extern const double p_diabetes;
extern const double p_smoking;

/* Probability of smear positive case */
extern const double p_smear;

/* Diagnose delay mean and std.
 * Mean has two components [autochton,foreign]
 */
extern const unsigned diagnose_mean[2];
extern const unsigned diagnose_std;

/* Probability to abandon treatment before finishing it */
extern const double p_abandon;
extern const double p_relapse_min;

/* Probability to infect an close individual */
extern const double p_infect;

/* Parameters related to age class for new infected.
 * youngster (0-20), adult (20-60), elder (60-) */

/* Boundaries in days between age classes */
extern const unsigned age_class_boundaries[2];

/* Probability to belong to a certain age class and gender
 * according to characteristics of the infecting case.
 * 			   [youngster] [male adult] [female adult] [elder] (Case)
 * [youngster]
 * [adult]
 * [elder]
 * (Contact)
 */
extern const double p_contacts_age_class[4][3];

/* Age groups in each age class and probability of each one */
extern const double p_contacts_age_groups[3][4];
extern const unsigned contacts_age_groups[3][5];

/* Probability of origin and gender of new infected depending
 * of origin and gender of the infector
 * 					[Native male] [Native female] [Foreign male] [Foreign female] (Case)
 * [Native male]
 * [Native female]
 * [Foreign male]
 * [Foreign female]
 * (Contact)
 */
extern const double p_origin_gender[4][4];

/* Probability to become sick depending on time infected */
extern const double p_sicken[7];

/* Factors that, if present, multiply p_sicken */
extern const double f_sicken_child; // [0,5) years
extern const double f_sicken_young;  // [5,15) years
extern const double f_sicken_HIV;
extern const double f_sicken_diabetes;
extern const double f_sicken_smoking;

/* Superior value of age group and probability to die for each group */
extern const unsigned p_die_age[2];
extern const double p_die[3];
extern const double p_die_sick;


#endif /* MODELS_TUBERCOLOSIS_PARAMETERS_H_ */
