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

#include <math.h>
#define M_PI 3.141592654f

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

struct wave_t
{
	int wavelength; // in LED counts
	int speed; // milli LED counts per second
	int r;
	int g;
	int b;
};

struct wave_t waves[] = { {10, 0, 64,0,0}, {17, 000, 0,128,128}, {7, 0, 0,255,0} };
#define WAVE_COUNT (sizeof(waves)/sizeof(*waves))

static inline int clamp(int val)
{
	return val < 0 ? 0 :
	       val > 255 ? 255 :
	       val;
}

#define WHEEL_CIRCUMFERENCE (2.f*M_PI*WHEEL_RADIUS)
#define WHEEL_RADIUS 10 //cm
#define LED_DISTANCE 1.75f //cm

void tim2_isr(void)
{
	static int slow_warning = 120;
	slow_warning--;

	static int t = 0;
	t++;

	static int distance = 0; // increases by 60000 per wheel revolution
	distance += frequency_millihertz;

	timer_clear_flag(TIM2, TIM_SR_UIF);

	printf("x\n");
	gpio_toggle(GPIOC, GPIO13);	/* LED on/off */


	for (int i=1; i<LED_COUNT; i++)
	{
		float pos = i + distance*(1.f/60000.f * WHEEL_CIRCUMFERENCE / LED_DISTANCE);
		int r=128 << 8;
		int g=128 << 8;
		int b=128 << 8;

		for (unsigned w=0; w<WAVE_COUNT; w++)
		{
			float wavepos = waves[w].speed * t / 60000.f;
			int val = 255 * sinf(2.f*M_PI * (pos - wavepos) / waves[w].wavelength);

			r += val * waves[w].r;
			g += val * waves[w].g;
			b += val * waves[w].b;
		}

		const int dim = 5;
		r = clamp(r >> 8) / dim;
		g = clamp(g >> 8) / dim;
		b = clamp(b >> 8) / dim;
		
		led_data[i] = (r << 8) | (g<<16) | b;
	}

	led_data[0] = ((t%60)>30 ? 255 : 0) | (slow_warning<<9);


	if (timer_get_flag(TIM2, TIM_SR_UIF))
	{
		slow_warning = 120;
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

	animation_init();

	int i=0;
	while (1) {
		__asm__("wfe");
		/*tick_leds();
		if (i++ > 40)
		{
			gpio_toggle(GPIOC, GPIO13);
			i=0;
			//printf("%04x\n", gpio_port_read(GPIOA));
			//printf("%d, %d\n", timer_get_counter(TIM1), timer_get_flag(TIM1, TIM_SR_CC1IF));
		}*/
	}
}
