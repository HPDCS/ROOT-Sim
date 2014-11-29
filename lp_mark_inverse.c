#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(int argc, char **argv) {

	double z = (double)atoll(argv[1]);
	double w = floor( (sqrt( 8 * z + 1) - 1) / 2.0);
	double t = ( w*w + w ) / 2.0;
	double y = z - t;
	double x = w - y;

	printf("LP: %d, mark: %d\n", (int)x, (int)y);
	return 0;
}
