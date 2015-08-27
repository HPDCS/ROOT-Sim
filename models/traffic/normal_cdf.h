/*----------------------------------------------------------------------
  File    : normal.h
  Contents: normal distribution functions
  Author  : Christian Borgelt
  History : 2002.11.04 file created
            2008.03.14 set of functions extended
            2008.03.15 cumulative distribution function added
----------------------------------------------------------------------*/
#ifndef __NORMAL__
#define __NORMAL__

#include <math.h>

// Define some constants
#define SQRT_2      1.41421356237309504880168872421	/* \sqrt(2) */
#define _1_SQRT_2PI 0.39894228040143267793994605993	/* 1/\sqrt(2\pi) */

// Compute the probability in a given contour according to a normal distribution
extern double contourcdf(double min, double max, double mean, double var);
extern double normcdf(double x, double mean, double var);


#endif
