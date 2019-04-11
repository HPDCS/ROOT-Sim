#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

#include <fcntl.h>

#define LENGTH	4096
#define CYCLES	2048

void swap(int *array, int i, int j)
{
	int t;
	t = array[i];
	array[i] = array[j];
	array[j] = t;
}// swap

void shuffle(int *array)
{
	int i, r, s; 
	for (i = 0, r = rand(), s = rand(); i < CYCLES; ++i, r = rand(), s = rand()) {
		swap(array, r % LENGTH, s % LENGTH);
	}
}// shuffle

void sort(int *array)
{
	int i;
	int ck = 1;

	while(ck)
		for (ck = 0, i = 0; i < LENGTH - 1; ++i)
			if (array[i] > array[i + 1]) {
				swap(array, i, i + 1);
				ck = 1;
			}
}// sort

int main(int argc, char const *argv[])
{
	int array [LENGTH];
	int i;
	srand(time(0));

	printf("\nPID: %u\n", getpid());
	printf("Parent PID: %u\n", getppid());

	for (i = 0; i < LENGTH; ++i)
		array[i] = i;

	do {
		shuffle(array);
		sort(array);
	} while (1);
	
	return 0;
}// main