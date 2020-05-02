#pragma once

#include "common.h"

void noise_init(void);
fixed_t noise(fixed_t x, fixed_t y);
fixed_t fractal_noise(fixed_t x, fixed_t y, fixed_t amp1, fixed_t amp2, fixed_t amp3);
