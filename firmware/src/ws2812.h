// the ws2812 interfacing code has been taken from
// https://github.com/hwhw/stm32-projects (hans-werner hilse <hwhilse@gmail.com>)

#pragma once
#include <stdint.h>

/* WS2812 driver.
 *
 * Resources used:
 *  - TIM3
 *  - DMA1, Channel 3
 *  - GPIO PA7
 *
 * Usage:
 *  - Connect the DIN pin of the wWS2812 strip to PA7.
 *  - Call ws2812_init();
 *  - Then update the led_data array as needed
 *  - Data format: (red << 8) | (green << 16) | (blue)
 */

// maximum is at about 4000
#define LED_COUNT 50 //0x200

extern volatile uint32_t led_data[LED_COUNT];

void ws2812_init(void);
