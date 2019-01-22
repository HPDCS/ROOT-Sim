/**
* @file lib/numerical.c
*
* @brief Numerical Library
*
* Piece-Wise Deterministic Random Number Generators.
*
* @copyright
* Copyright (C) 2008-2019 HPDCS Group
* https://hpdcs.github.io
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; only version 3 of the License applies.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @author Alessandro Pellegrini
*
* @date March 16, 2011
*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <ROOT-Sim.h>

#include <core/init.h>
#include <communication/communication.h>
#include <mm/mm.h>
#include <scheduler/process.h>
#include <scheduler/scheduler.h>
#include <statistics/statistics.h> // To have _mkdir helper function

/**
 * Master seed to initialize local seeds of all LPs. This is
 * taken from the configuration file in ~/.rootsim if available,
 * or from /dev/urand the first time a ROOT-Sim model is run.
 */
static seed_type master_seed;

static double do_random(void)
{
	uint32_t *seed1;
	uint32_t *seed2;

	seed1 = (uint32_t *) & (current->numerical.seed);
	seed2 =
	    (uint32_t *) ((char *)&(current->numerical.seed) +
			  (sizeof(uint32_t)));

	*seed1 = 36969u * (*seed1 & 0xFFFFu) + (*seed1 >> 16u);
	*seed2 = 18000u * (*seed2 & 0xFFFFu) + (*seed2 >> 16u);

	// The magic number below is 1/(2^32 + 2).
	// The result is strictly between 0 and 1.
	return (((*seed1 << 16u) + (*seed1 >> 16u) + *seed2) +
		1.0) * 2.328306435454494e-10;

}

/**
* This function returns a number in between (0,1), according to a Uniform Distribution.
* It is based on Multiply with Carry by George Marsaglia
*
* @return A random number, in between (0,1)
*/
double Random(void)
{
	double ret;
	switch_to_platform_mode();

	ret = do_random();

	switch_to_application_mode();
	return ret;
}

int RandomRange(int min, int max)
{
	double ret;
	switch_to_platform_mode();

	ret = (int)floor(do_random() * (max - min + 1)) + min;

	switch_to_application_mode();
	return ret;
}

int RandomRangeNonUniform(int x, int min, int max)
{
	double ret;
	switch_to_platform_mode();

	ret = (((RandomRange(0, x) | RandomRange(min, max))) % (max - min + 1)) + min;

	switch_to_application_mode();
	return ret;
}

/**
* This function returns a random number according to an Exponential distribution.
* The mean value of the distribution must be passed as the mean value.
*
* @param mean Mean value of the distribution
* @return A random number
*/
double Expent(double mean)
{
	double ret;
	switch_to_platform_mode();

	if (unlikely(mean < 0)) {
		rootsim_error(true,
			      "Expent() has been passed a negative mean value\n");
	}

	ret = (-mean * log(1 - do_random()));

	switch_to_application_mode();
	return ret;
}

/**
* This function returns a number according to a Normal Distribution with mean 0
*
* @return A random number
*/
double Normal(void)
{
	double fac, rsq, v1, v2;
	bool *iset;
	double *gset;

	iset = &current->numerical.iset;
	gset = &current->numerical.gset;

	if (*iset == false) {
		do {
			v1 = 2.0 * Random() - 1.0;
			v2 = 2.0 * Random() - 1.0;
			rsq = v1 * v1 + v2 * v2;
		} while (rsq >= 1.0 || D_EQUAL_ZERO(rsq));

		fac = sqrt(-2.0 * log(rsq) / rsq);

		// Perform Box-Muller transformation to get two normal deviates. Return one
		// and save the other for next time.
		*gset = v1 * fac;
		*iset = true;
		return v2 * fac;
	} else {
		// A deviate is already available
		*iset = false;
		return *gset;
	}
}

/**
* This function returns a number in according to a Gamma Distribution of Integer Order ia,
* a waiting time to the ia-th event in a Poisson process of unit mean.
*
* @author D. E. Knuth
* @param ia Integer Order of the Gamma Distribution
* @return A random number
*/
double Gamma(int ia)
{
	int j;
	double am, e, s, v1, v2, x, y;

	if (unlikely(ia < 1)) {
		rootsim_error(false,
			      "Gamma distribution must have a ia value >= 1. Defaulting to 1...");
		ia = 1;
	}

	if (ia < 6) {
		// Use direct method, adding waiting times
		x = 1.0;
		for (j = 1; j <= ia; j++)
			x *= Random();
		x = -log(x);
	} else {
		// Use rejection method
		do {
			do {
				do {
					v1 = Random();
					v2 = 2.0 * Random() - 1.0;
				} while (v1 * v1 + v2 * v2 > 1.0);

				y = v2 / v1;
				am = (double)(ia - 1);
				s = sqrt(2.0 * am + 1.0);
				x = s * y + am;
			} while (x < 0.0);

			e = (1.0 + y * y) * exp(am * log(x / am) - s * y);
		} while (Random() > e);
	}

	return x;
}

/**
* This function returns the waiting time to the next event in a Poisson process of unit mean.
*
* @return A random number
*/
double Poisson(void)
{
	return Gamma(1);
}

/**
* This function returns a random sample from a Zipf distribution.
* Based on the rejection method by Luc Devroye for sampling:
* "Non-Uniform Random Variate Generation, page 550, Springer-Verlag, 1986
*
* @param skew The skew of the distribution
* @param limit The largest sample to retrieve
* @return A random number
*/
int Zipf(double skew, int limit)
{
	double a = skew;
	double b = pow(2., a - 1.);
	double x, t, u, v;
	do {
		u = Random();
		v = Random();
		x = floor(pow(u, -1. / a - 1.));
		t = pow(1. + 1. / x, a - 1.);
	} while (v * x * (t - 1.) / (b - 1.) > (t / b) || x > limit);
	return (int)x;
}

/**
* MwC random number generators suffer from bad seeds. Since initial LPs' seeds are derived randomly
* from a random master seed, chances are that we incur in a bad seed. This would create a strong
* bias in the generation of random numbers for certain LPs.
* This function checks whether a randomly generated seed is a bad one, and switch to a different
* safe initial state.
* This generation is deterministic, therefore it is good for PWD executions of different simulations
* using the same initial master seed.
*
* @param cur_seed The current seed which has been generated and must be checked
* @return A sanitized seed
*/
static seed_type sanitize_seed(seed_type cur_seed)
{

	uint32_t *seed1 = (uint32_t *) &(cur_seed);
	uint32_t *seed2 = (uint32_t *) ((char *)&(cur_seed) + (sizeof(uint32_t)));

	// Sanitize seed1
	// Any integer multiple of 0x9068FFFF, including 0, is a bad state
	uint32_t state_orig;
	uint32_t temp;

	state_orig = *seed1;
	temp = state_orig;

	// The following is equivalent to % 0x9068FFFF, without using modulo
	// operation which may be expensive on embedded targets. For
	// uint32_t and this divisor, we only need 'if' rather than 'while'.
	if (temp >= UINT32_C(0x9068FFFF))
		temp -= UINT32_C(0x9068FFFF);
	if (temp == 0) {
		// Any integer multiple of 0x9068FFFF, including 0, is a bad state.
		// Use an alternate state value by inverting the original value.
		temp = state_orig ^ UINT32_C(0xFFFFFFFF);
		if (temp >= UINT32_C(0x9068FFFF))
			temp -= UINT32_C(0x9068FFFF);
	}
	*seed1 = temp;

	// Sanitize seed2
	// Any integer multiple of 0x464FFFFF, including 0, is a bad state.
	state_orig = *seed2;
	temp = state_orig;

	// The following is equivalent to % 0x464FFFFF, without using modulo
	// operation which may be expensive on some targets. For
	// uint32_t and this divisor, it may loop up to 3 times.
	while (temp >= UINT32_C(0x464FFFFF))
		temp -= UINT32_C(0x464FFFFF);
	if (temp == 0) {
		// Any integer multiple of 0x464FFFFF, including 0, is a bad state.
		// Use an alternate state value by inverting the original value.
		temp = state_orig ^ UINT32_C(0xFFFFFFFF);
		while (temp >= UINT32_C(0x464FFFFF))
			temp -= UINT32_C(0x464FFFFF);
	}

	*seed2 = temp;

	return cur_seed;
}

/**
* This function is used by ROOT-Sim to load the initial value of the seed.
*
* A configuration file is used to store the last master seed, amongst different
* invocations of the simulator. Using the first value stored in the file, a random
* number is generated, which is used as a row-offset in the file istelf, where a
* specified seed will be found.
* The first line is the replaced with the subsequent seed generated by the Random
* function.
*
* @return The master seed
*/
static void load_seed(void)
{

	static bool single_print = false; // To print only once a message about manual initialization
	seed_type new_seed;
	char conf_file[512];
	FILE *fp;

	// Get the path to the configuration file
	sprintf(conf_file, "%s/.rootsim/numerical.conf", getenv("HOME"));

	// Check if the file exists. If not, we have to create configuration
	if ((fp = fopen(conf_file, "r+")) == NULL) {

		// Try to build the path to the configuration folder.
		sprintf(conf_file, "%s/.rootsim", getenv("HOME"));
		_mkdir(conf_file);

		// Create and initialize the file
		sprintf(conf_file, "%s/.rootsim/numerical.conf", getenv("HOME"));
		if ((fp = fopen(conf_file, "w")) == NULL) {
			rootsim_error(true, "Unable to create the numerical library configuration file %s. Aborting...", conf_file);
		}

		// We now initialize the first long random number. Thanks Unix!
		// TODO: THIS IS NOT PORTABLE!
		int fd;
		if ((fd = open("/dev/random", O_RDONLY)) == -1) {
			rootsim_error(true, "Unable to initialize the numerical library configuration file %s. Aborting...", conf_file);

		}
		read(fd, &new_seed, sizeof(seed_type));
		close(fd);
		fprintf(fp, "%llu\n", (unsigned long long)new_seed);	// We cast, so that we get an integer representing just a bit sequence
		fclose(fp);

	}

	// Is seed manually specified?
	if (rootsim_config.set_seed > 0) {

		if (!single_print) {
			single_print = true;
			printf("Manually setting master seed to %llu\n", (unsigned long long)rootsim_config.set_seed);
		}
		master_seed = rootsim_config.set_seed;
	}

	// Load the configuration for the numerical library
	if ((fp = fopen(conf_file, "r+")) == NULL) {
		rootsim_error(true, "Unable to load numerical distribution configuration: %s. Aborting...", conf_file);
	}

	// Load the initial seed
	fscanf(fp, "%llu", (unsigned long long *)&master_seed);

	// Replace the initial seed
	if (rootsim_config.deterministic_seed == false) {
		rewind(fp);
		srandom(master_seed);
		new_seed = random();
		fprintf(fp, "%llu\n", (unsigned long long)new_seed);
	}

	fclose(fp);
}


// TODO: con un (non tanto) alto numero di processi logici (> numero di bit di un intero!!), lo shift
// circolare restituisce a due LP differenti lo stesso seme iniziale. Questo fa sì che, se la logica di
// programma è la stessa e basata soltanto su numeri casuali, due o più nodi eseguano sempre la stessa
// sequenza di eventi agli stessi timestamp!!!

#define RS_WORD_LENGTH (8 * sizeof(seed_type))
#define ROR(value, places) (value << (places)) | (value >> (RS_WORD_LENGTH - places))	// Circular shift
void numerical_init(void)
{
	// Initialize the master seed
	load_seed();

	// Initialize the per-LP seed
	foreach_lp(lp) {
		lp->numerical.seed = sanitize_seed(ROR((int64_t) master_seed, lp->gid.to_int % RS_WORD_LENGTH));
	}

}

#undef RS_WORD_LENGTH
#undef ROR


/**
* Calculates a sum within double variables with bounded approximation error
*
* This implements a variant of the Kahan summation algorithm:
* https://en.wikipedia.org/wiki/Kahan_summation_algorithm
* This is used in the topology module to calculate minimum costs and weighted probabilities
* since the classic iterated floating point sum may result in unbounded errors
* originated from the limited precision of the underlying floating point representation.
*
* @author Andrea Piccione
* @param cnt the number of addendums to sum
* @param addendums a pointer to a sequence of cnt doubles
* @return the sum between the addendums with bounded error
*/
__attribute__((const))
double NeumaierSum(unsigned cnt, double addendums[cnt]) {
	if(!cnt)
		return 0.0;
	double sum = addendums[0];
	double crt = 0.0;
	double tmp;
	while (--cnt) {
		tmp = sum + addendums[cnt];
		if(fabs(sum) >= fabs(addendums[cnt])) {
			crt += (sum - tmp) + addendums[cnt];
		} else {
			crt += (addendums[cnt] - tmp) + sum;
		}
		sum = tmp;
	}
	return sum + crt;
}


/**
* Calculates a partial sum within double variables with bounded approximation error
*
* This has similar utilization to <NeumaierSum>() with the difference that this can be used
* to calculate partial sums, adding doubles one by one.
*
* @author Andrea Piccione
* @param sh a struct _sum_helper_t holding the partial result typically from a previous sum
* @param addendum the double floating point value to add
* @return a struct _sum_helper_t holding the sum between sh and addendum
*/
__attribute__((const))
struct _sum_helper_t PartialNeumaierSum(struct _sum_helper_t sh, double addendum){
	double tmp = sh.sum + addendum;
	if(fabs(sh.sum) >= fabs(addendum)) {
		sh.crt += (sh.sum - tmp) + addendum;
	} else {
		sh.crt += (addendum - tmp) + sh.sum;
	}
	sh.sum = tmp;
	return sh;
}

