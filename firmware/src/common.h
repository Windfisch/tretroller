#pragma once

#include <stdint.h>

#define SHIFT 16
typedef int64_t fixed_t;

#define ONE (((fixed_t)1)<<SHIFT)

#define N_SIDE 11
#define N_FRONT 5
#define N_BOTTOM 26

#define FPS 60
#define FREQUENCY_FACTOR 1000 // frequency_millihertz / FREQUENCY_FACTOR = wheel frequency in hertz

