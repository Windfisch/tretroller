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

#include <stdint.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "tacho.h"

volatile uint32_t frequency_millihertz = 0;
static bool overflow = true;


/* CONFIGURATION SECTION
 * change these values to account for your magnet configuration. see learn.py and the README */
#define N_MAGNETS 5
static const uint32_t DISTANCES[N_MAGNETS] = {69340993, 61923605, 64606495, 65695792, 66113112};
/* end of configuration section */

static uint32_t backlog[N_MAGNETS];
static int phase = 0;

static void put_backlog(uint32_t value)
{
	for (int i=1; i<N_MAGNETS; i++)
		backlog[i-1] = backlog[i];
	backlog[N_MAGNETS-1] = value;

	/*printf("backlog: ");
	for (int i=0; i<N_MAGNETS; i++)
		printf("%ld, ", backlog[i]);
	printf("\n");*/
}

static int detect_phase(void)
{
	uint32_t timestamps[N_MAGNETS] = {0};
	for (int i=1; i<N_MAGNETS; i++)
		timestamps[i] = timestamps[i-1] + backlog[i-1];

	int best_offset = -2;
	int best_reldiff = INT_MAX;
	int secondbest_offset = -2;
	int secondbest_reldiff = INT_MAX;


	for (int phase_offset=0; phase_offset<N_MAGNETS; phase_offset++)
	{
		//printf("with phase_offset = %d: ", phase_offset);
		int32_t freqs[N_MAGNETS]; // millihertz
		for (int i=0; i<N_MAGNETS; i++)
			freqs[i] = DISTANCES[(i+phase_offset)%N_MAGNETS] / backlog[i];

		int32_t reldiff_max = 0;
		for (int i=0; i<N_MAGNETS; i++)
		{
			int32_t diff = freqs[i] - (freqs[0] + ((int64_t)timestamps[i]) * (freqs[N_MAGNETS-1] - freqs[0]) / timestamps[N_MAGNETS-1]);
			int32_t reldiff = 1000 * diff / freqs[0];
			//printf("%ld, ", reldiff);
			if (abs(reldiff) > reldiff_max)
				reldiff_max = abs(reldiff);
		}
		//printf(" => %ld\n", reldiff_max);

		if (reldiff_max <= best_reldiff)
		{
			secondbest_reldiff = best_reldiff;
			secondbest_offset = best_offset;
			best_reldiff = reldiff_max;
			best_offset = phase_offset;
		}
		else if (reldiff_max <= secondbest_reldiff)
		{
			secondbest_reldiff = reldiff_max;
			secondbest_offset = phase_offset;
		}
	}

	int confidence = 100 * (secondbest_reldiff - best_reldiff) / (best_reldiff+1); // relative difference between best and second best in percent
	//printf("best phase offset: %d, confidence: %d\n", best_offset, confidence);
	if (confidence > 150)
		return (best_offset+N_MAGNETS-1) % N_MAGNETS;
	else
		return -1;
}

/** Tacho rising edge interrupt */
void tim1_cc_isr(void)
{
	timer_clear_flag(TIM1, TIM_SR_CC1IF);
	printf("TIM1_CCR1 = %lu\n", TIM1_CCR1);

	put_backlog(TIM1_CCR1);
	phase = (phase+1) % N_MAGNETS;
	int detected_phase = detect_phase();
	if (detected_phase != -1 && detected_phase != phase)
	{
		//printf("PHASE JUMP DETECTED! expected %d, detected %d\n", phase, detected_phase);
		phase = detected_phase;
	}
	
	//printf("tim1_cc_isr %d %d\n", TIM1_CCR1, timer_get_flag(TIM1, TIM_SR_CC1OF));
	if (!overflow)
		frequency_millihertz = DISTANCES[phase] / (uint32_t)TIM1_CCR1 / N_MAGNETS;
	overflow = false;
	//printf("%d mHz\n", frequency_millihertz);
}

/** Timer overflow interrupt */
void tim1_up_isr(void)
{
	timer_clear_flag(TIM1, TIM_SR_UIF);
	//printf("tim1_up_isr, %d\n", timer_get_counter(TIM1));

	frequency_millihertz = 0;
	overflow = true;
}

void tacho_init(void)
{
	rcc_periph_clock_enable(RCC_TIM1);
	rcc_periph_reset_pulse(RST_TIM1);

	// Configure GPIOs: tacho in = PA8
	gpio_set_mode(GPIOA, GPIO_MODE_INPUT,
	    GPIO_CNF_INPUT_PULL_UPDOWN, GPIO_TIM1_CH1);
	gpio_set(GPIOA, GPIO_TIM1_CH1);

	// Configure the basic timer stuff
	timer_set_mode(TIM1, TIM_CR1_CKD_CK_INT_MUL_4, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
	timer_disable_preload(TIM1);
	timer_continuous_mode(TIM1);
	timer_update_on_overflow(TIM1); // that means: generate update events *only* on overflows,
	                                // not on e.g. reset through the trigger input.
	timer_set_period(TIM1, 0xFFFF); // full scale
	timer_set_prescaler(TIM1, 72000000L / 65536 - 1); // TODO FIXME

	// Configure input capture on channel 1 (=PA8).
	// Every rising edge on PA8 will cause TIM1_CCR1 to be loaded with the current timer value
	// and tim1_cc_isr() to be called.
	timer_ic_set_input(TIM1, TIM_IC1, TIM_IC_IN_TI1);
	timer_ic_set_polarity(TIM1, TIM_IC1, TIM_IC_RISING);
	timer_ic_set_filter(TIM1, TIM_IC1, TIM_IC_CK_INT_N_8);
	timer_ic_enable(TIM1, TIM_IC1);

	// Configure reset on every rising edge of channel 1.
	timer_slave_set_filter(TIM1, TIM_IC_CK_INT_N_8);
	timer_slave_set_trigger(TIM1, TIM_SMCR_TS_TI1FP1); // trigger input := CH1
	timer_slave_set_mode(TIM1, TIM_SMCR_SMS_RM); // timer reset on trigger input

	// Configure the interrupts
	nvic_enable_irq(NVIC_TIM1_UP_IRQ); // Update interrupt (tim1_up_isr) is called on overflow.
	nvic_enable_irq(NVIC_TIM1_CC_IRQ); // Capture/Compare interrupt (tim1_cc_isr) is called on every rising edge.
	nvic_set_priority(NVIC_TIM1_UP_IRQ, 0xf << 4); // Set lowest priority in order to not interfere
	nvic_set_priority(NVIC_TIM1_CC_IRQ, 0xf << 4); // with the timing critical WS2812 driver
	timer_enable_irq(TIM1, TIM_DIER_UIE | TIM_DIER_CC1IE);

	// Start the timer
	timer_enable_counter(TIM1);
}

