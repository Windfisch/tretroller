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
#include <stdio.h>
#include <string.h>

#include "ws2812.h"
#include "tacho.h"
#include "usart.h"

// maximum is at about 4000
#define LED_COUNT 50 //0x200
// minimum ID offset is 0x100 (first ID byte mustn't be 0x00)
#define ID_OFFSET 0xA000


static void clock_setup(void)
{
	rcc_clock_setup_in_hse_8mhz_out_72mhz();

	rcc_periph_clock_enable(RCC_GPIOC);
}


static void tick_leds(void)
{
	for (int i=0; i<LED_COUNT; i++)
	{
		int val = frequency_millihertz - i*10000/LED_COUNT;
		val = val < 0 ? 0 : val;
		val = val > 150 ? 150 : val;
		val = val * 255 / 150;
		led_data[i] = (val << 8) | (val<<16) | val;
	}
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

	int i=0;
	while (1) {
		__asm__("wfe");
		tick_leds();
		if (i++ > 40)
		{
			gpio_toggle(GPIOC, GPIO13);	/* LED on/off */
			i=0;
			//printf("%04x\n", gpio_port_read(GPIOA));
			//printf("%d, %d\n", timer_get_counter(TIM1), timer_get_flag(TIM1, TIM_SR_CC1IF));
		}
	}
}
