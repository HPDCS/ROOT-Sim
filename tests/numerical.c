#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <stdbool.h>

#include "common.h"

#define print(...) printf(__VA_ARGS__); fflush(stdout)


enum _dis {
	RANDOM,
	EXPENT,
	NORMAL,
	GAMMA,
	GAMMA2,
	POISSON,
	ZIPF
};


static double get_sample(enum _dis distr) {
	
	switch(distr) {
		case RANDOM:
			return Random();
		
		case EXPENT:
			return Expent(10);

		case NORMAL:
			return Normal();

		case GAMMA:
			return Gamma(2);

		case GAMMA2:
			return Gamma(7);

		case POISSON:
			return Poisson();

		case ZIPF:
			return Zipf(3.0, 10);

		default:
			print("Error: unknown distribution\n");
			exit(EXIT_FAILURE);
	}
}


/* Perform the Kolmogorov-Smirnov test on the uniform random number
 * generator.
 */
static bool ks_test(uint32_t N, uint32_t nBins, enum _dis distr)
{
	uint32_t i, index, cumulativeSum;
	double rf, ksThreshold, countPerBin;
	uint32_t bins[nBins];
	
	// Fill the bins
	for (i = 0; i < nBins; i++)
		bins[i] = 0;
		
	for (i = 0; i < N; i++) {
		rf = get_sample(distr);
		index = floor(rf * nBins);
		if (index >= nBins) // just in case...
			index = nBins - 1;

		bins[index]++;
	}
	
	// Test the bins
	ksThreshold = 1.358 / sqrt((double)N);
	countPerBin = (double)N / nBins;
	cumulativeSum = 0;
	for (i = 0; i < nBins; i++) {
		cumulativeSum += bins[i];
		if(!((double)cumulativeSum / N - (i + 1) * countPerBin / N < ksThreshold))
			return false;
	}
	return true;
}

static bool aux_ks_test(enum _dis distr) {
	bool passed = true;
	
	passed &= ks_test(1000000, 1000, distr);
	passed &= ks_test(100000, 1000, distr);
	passed &= ks_test(10000, 100, distr);
	passed &= ks_test(1000, 10, distr);
	passed &= ks_test(100, 10, distr);

	return passed;
}

static bool test_random_range(void) {
	bool passed = true;
	int min, max, r, i;

	for(i = 0; i < 1000000; i++) {
		max = INT_MAX * Random();
		min = max * Random();
		r = RandomRange(min, max);

		if(r < min || r > max)
			passed = false;
	}

	return passed;
}

static bool test_random_range_nonuniform(void) {
	bool passed = true;
	int x, min, max, r, i;

	for(i = 0; i < 1000000; i++) {
		x = INT_MAX * Random();
		max = INT_MAX * Random();
		min = max * Random();
		r = RandomRangeNonUniform(x, min, max);

		if(r < min || r > max)
			passed = false;
	}

	return passed;
}

#define do_test(desc, function, ...) do {\
					print(desc);	\
					passed = function(__VA_ARGS__); \
					if(passed) { \
						print("passed\n"); \
					} else { \
						print("failed\n"); \
						ret = 1; \
					} \
				} while(0)

int main(void)
{
	bool passed = true;
	int ret = 0;
	
	current = &context;
	current->numerical.seed = 7319936632422683443ULL;

	do_test("Kolmogorov-Smirnov test on Random()... ", aux_ks_test, RANDOM);
	do_test("Functional test on RandomRange()... ", test_random_range);
	do_test("Functional test on RandomRangeNonUniform()... ", test_random_range_nonuniform);
	do_test("Kolmogorov-Smirnov test on Expent()... ", aux_ks_test, EXPENT);
	do_test("Kolmogorov-Smirnov test on Normal()... ", aux_ks_test, NORMAL);
	do_test("Kolmogorov-Smirnov test on Gamma()... ", aux_ks_test, GAMMA);
	do_test("Kolmogorov-Smirnov test on Gamma()... ", aux_ks_test, GAMMA2);
	do_test("Kolmogorov-Smirnov test on Poisson()... ", aux_ks_test, POISSON);
	do_test("Kolmogorov-Smirnov test on Zipf()... ", aux_ks_test, ZIPF);
	
	return ret;
}
