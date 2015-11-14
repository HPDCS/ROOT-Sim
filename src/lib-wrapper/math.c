#include <scheduler/scheduler.h>

// Definitions to functions which will be wrapped by the linker
double __real_pow(double x, double y);
float __real_powf(float x, float y);
long double __real_powl(long double x, long double y);


double __wrap_pow(double x, double y) {
	double ret;
	switch_to_platform_mode();
	ret = __real_pow(x, y);
	switch_to_application_mode();
	return ret;
}

float __wrap_powf(float x, float y) {
	float ret;
	switch_to_platform_mode();
	ret = __real_powf(x, y);
	switch_to_application_mode();
	return ret;
}

long double __wrap_powl(long double x, long double y) {
	long double ret;
	switch_to_platform_mode();
	ret = __real_powl(x, y);
	switch_to_application_mode();
	return ret;
}

