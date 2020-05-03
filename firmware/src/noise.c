#include <stdlib.h>
#include <stdio.h>
#include "common.h"

#define RESOLUTION_X 16
#define RESOLUTION_Y 64

static fixed_t random_data[RESOLUTION_X][RESOLUTION_Y]; // between -(1<<SHIFT) and (1<<SHIFT)

#define MUL(a,b) ((a*b)>>SHIFT)
#define NUM(x) ((x)<<SHIFT)

void noise_init(void)
{
	for (int i=0; i<RESOLUTION_X; i++)
		for (int j=0; j<RESOLUTION_Y; j++)
			random_data[i][j] = rand() % (2*ONE) - ONE ;

	printf("noise: RAND_MAX = %d, val = %d\n", RAND_MAX, random_data[4][4]);
}

fixed_t noise(fixed_t x, fixed_t y)
{
	const fixed_t wrap_x = RESOLUTION_X << SHIFT;
	const fixed_t wrap_y = RESOLUTION_Y << SHIFT;

	x = ((x % wrap_x) + wrap_x) % wrap_x;
	y = ((y % wrap_y) + wrap_y) % wrap_y;

	int x_int = x >> SHIFT;
	int y_int = y >> SHIFT;
	fixed_t x_frac = x % (1<<SHIFT);
	fixed_t y_frac = y % (1<<SHIFT);

	fixed_t r11 = random_data[x_int][y_int];
	fixed_t r12 = random_data[x_int][(y_int+1)%RESOLUTION_Y];
	fixed_t r21 = random_data[(x_int+1)%RESOLUTION_X][y_int];
	fixed_t r22 = random_data[(x_int+1)%RESOLUTION_X][(y_int+1)%RESOLUTION_Y];

	fixed_t val1 = r11 + (((r12-r11)*y_frac)>>SHIFT);
	fixed_t val2 = r21 + (((r22-r21)*y_frac)>>SHIFT);

	return val1 + (((val2-val1)*x_frac)>>SHIFT);
}

fixed_t fractal_noise(fixed_t x, fixed_t y, fixed_t amp1, fixed_t amp2, fixed_t amp3)
{
	return
		MUL(amp1, noise(x, y)) +
		MUL(amp2, noise(x*2 + NUM(3187), y*2 + NUM(1379))) +
		MUL(amp2, noise(x*4 + NUM(827), y*4 + NUM(2913)));
}
