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
#include "tacho.h"

volatile uint32_t frequency_millihertz = 0;
static bool overflow = true;

/** Tacho rising edge interrupt */
void tim1_cc_isr(void)
{
	timer_clear_flag(TIM1, TIM_SR_CC1IF);
	//printf("tim1_cc_isr %d %d\n", TIM1_CCR1, timer_get_flag(TIM1, TIM_SR_CC1OF));
	if (!overflow)
		frequency_millihertz = 1000*65536 / (uint32_t)TIM1_CCR1;
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

