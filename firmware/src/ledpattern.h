#pragma once
#include <stdint.h>
#include "common.h"

void ledpattern_bat_empty(volatile uint32_t led_data[], int t, int batt_cells);

void ledpattern_front_bat_and_slow_info(volatile uint32_t led_data[], int t, int batt_cells, int batt_percent, int slow_warning);
void ledpattern_front_knightrider(volatile uint32_t led_data[], int t, int batt_cells, int batt_percent, int slow_warning);

void ledpattern_bottom_3color(volatile uint32_t led_data[], int t, fixed_t pos0, fixed_t velocity);
void ledpattern_bottom_rainbow(volatile uint32_t led_data[], int t, fixed_t pos0, fixed_t velocity);
void ledpattern_bottom_velocity_color(volatile uint32_t led_data[], int t, fixed_t pos0, fixed_t velocity);
void ledpattern_bottom_position_color(volatile uint32_t led_data[], int t, fixed_t pos0, fixed_t velocity);
void ledpattern_bottom_water(volatile uint32_t led_data[], int t, fixed_t pos0, fixed_t velocity);
void ledpattern_bottom_snake(volatile uint32_t led_data[], int t, fixed_t pos0, fixed_t velocity);

typedef void (*ledpattern_bottom_t)(volatile uint32_t[], int, fixed_t, fixed_t);
#define N_BOTTOM_PATTERNS 6
extern ledpattern_bottom_t ledpatterns_bottom[N_BOTTOM_PATTERNS];

typedef void (*ledpattern_front_t)(volatile uint32_t[], int , int, int, int);
#define N_FRONT_PATTERNS 8
extern ledpattern_front_t ledpatterns_front[N_FRONT_PATTERNS];

