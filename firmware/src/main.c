/* Copyright (c) 2020 Florian Jung
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/cortex.h>
#include <stdio.h>
#include <string.h>

#include "noise.h"
#include "color.h"
#include "ws2812.h"
#include "tacho.h"
#include "usart.h"
#include "adc.h"
#include "battery.h"
#include "common.h"
#include "math.h"
#include "ledpattern.h"

#define ADC_MAX 4095
#define ADC_VREF_MILLIVOLTS 3300
#define BAT_R1 1
#define BAT_R2 10

// minimum ID offset is 0x100 (first ID byte mustn't be 0x00)
#define ID_OFFSET 0xA000


static void clock_setup(void)
{
	rcc_clock_setup_in_hse_8mhz_out_72mhz();
	rcc_periph_clock_enable(RCC_GPIOC); // LED
}


static void animation_init(void)
{
	rcc_periph_clock_enable(RCC_TIM2);
	rcc_periph_reset_pulse(RST_TIM2);

	// Configure the basic timer stuff
	timer_set_mode(TIM2, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
	timer_disable_preload(TIM2);
	timer_continuous_mode(TIM2);
	timer_set_period(TIM2, 0xFFFF); // full scale
	timer_set_prescaler(TIM2, 72000000LL / (60 * 65536LL) - 1); // 60 fps update rate FIXME

	// Configure the interrupts
	nvic_enable_irq(NVIC_TIM2_IRQ);
	nvic_set_priority(NVIC_TIM2_IRQ, 0xf << 4); // Set lowest priority in order to not interfere with ws2812
	timer_enable_irq(TIM2, TIM_DIER_UIE);

	// Start the timer
	timer_enable_counter(TIM2);
}



const double WHEEL_RADIUS_MM = 105.;
const double WHEEL_CIRCUMFERENCE_MM = WHEEL_RADIUS_MM * 2 * 3.141592654;
const double LED_DISTANCE_MM = 17.5;



const fixed_t WHEEL_CIRCUMFERENCE_LEDUNITS = (1<<SHIFT) * WHEEL_CIRCUMFERENCE_MM / LED_DISTANCE_MM;


void tim2_isr(void)
{
	/* slow_warning is usually 0. It's set to >0, when the ISR hasn't finished in time */
	static int slow_warning = 120;
	if (slow_warning > 0) slow_warning--;

	/* frame counter */
	static int t = 1000; // the offset does not really matter. however, we're subtracting from t at some places, and we don't want these calculations to become negative.
	t++;

	cm_disable_interrupts();
	uint32_t frequency_millihertz_copy = frequency_millihertz;
	cm_enable_interrupts();

	static int distance = 0; // increases by 60000 per wheel revolution
	distance += frequency_millihertz_copy;

	fixed_t velocity = ((fixed_t)frequency_millihertz_copy) * WHEEL_CIRCUMFERENCE_LEDUNITS / FREQUENCY_FACTOR; // = ledunits per second

	static int batt_percent = 1;
	static int batt_empty = 0;
	/* hysteresis to avoid flickering between the normal and the empty state */
	if (!batt_empty)
		batt_empty = batt_percent <= 0;
	else
		batt_empty = batt_percent <= 3;

	timer_clear_flag(TIM2, TIM_SR_UIF);

	gpio_toggle(GPIOC, GPIO13);	/* LED on/off */

	adc_poll();
	if (t % 100 == 0)
	{
		if (adc_value > 0)
		{
			int batt_millivolts = ADC_VREF_MILLIVOLTS * adc_value * (BAT_R1+BAT_R2) / ADC_MAX / BAT_R1;
			batt_percent = batt_get_percent(batt_millivolts);
			//printf("adc value: %d = %d mV -> %d %%\n", adc_value, batt_millivolts, batt_percent);
		}
	}


	fixed_t pos0 = distance * WHEEL_CIRCUMFERENCE_LEDUNITS / (FPS*FREQUENCY_FACTOR);

	if (batt_empty)
	{
		// sets both front/side and bottom leds
		ledpattern_bat_empty(led_data, t, batt_cells);
	}
	else
	{
		// set the front/side leds
		ledpattern_bat_and_slow_info(led_data, t, batt_cells, batt_percent, slow_warning);
		//ledpattern_front_knightrider(led_data, t, batt_cells, batt_percent, slow_warning);

		// set the bottom leds
		ledpattern_bottom_snake(led_data, t, pos0, velocity);
		//ledpattern_bottom_water(led_data, t, pos0, velocity);
		//ledpattern_bottom_rainbow(led_data, t, pos0, velocity);
		//ledpattern_bottom_position_color(led_data, t, pos0, velocity);
	}



	// must be at the end of the ISR
	if (timer_get_flag(TIM2, TIM_SR_UIF))
		slow_warning = 120;
}

int main(void)
{
	clock_setup();

	// LED
	gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
	gpio_toggle(GPIOC, GPIO13);	/* LED on/off */


	uart_setup();
	ws2812_init();
	tacho_init();
	adc_init();
	noise_init();

	animation_init();

	int i=0;
	while (1) {
		__asm__("wfe");
	}
}
