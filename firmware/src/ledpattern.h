#pragma once
#include <stdint.h>
#include "common.h"

void ledpattern_bat_empty(volatile uint32_t led_data[], int t, int batt_cells);
void ledpattern_bat_and_slow_info(volatile uint32_t led_data[], int t, int batt_cells, int batt_percent, int slow_warning);
void ledpattern_bottom_3color(volatile uint32_t led_data[], int t, fixed_t pos0);
void ledpattern_bottom_rainbow(volatile uint32_t led_data[], int t, fixed_t pos0, fixed_t velocity);
void ledpattern_bottom_color(volatile uint32_t led_data[], int t, fixed_t pos0, fixed_t velocity);
