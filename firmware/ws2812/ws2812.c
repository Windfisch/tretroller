/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2009 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2013 Stephen Dwyer <scdwyer@ualberta.ca>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/usart.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>


// maximum is at about 4000
#define LED_COUNT 10 //0x200
// minimum ID offset is 0x100 (first ID byte mustn't be 0x00)
#define ID_OFFSET 0xA000

// allow printf() to use the USART
int _write(int file, char *ptr, int len)
{
	int i;

	if (file == STDOUT_FILENO || file == STDERR_FILENO) {
		for (i = 0; i < len; i++) {
			if (ptr[i] == '\n') {
				usart_send_blocking(USART1, '\r');
			}
			usart_send_blocking(USART1, ptr[i]);
		}
		return i;
	}
	errno = EIO;
	return -1;
}


static void clock_setup(void)
{
	rcc_clock_setup_in_hse_8mhz_out_72mhz();

	/* Enable clocks for GPIO ports */
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOC);
	rcc_periph_clock_enable(RCC_AFIO);

	/* Enable TIM3 Periph */
	rcc_periph_clock_enable(RCC_TIM3);

	/* Enable DMA1 clock */
	rcc_periph_clock_enable(RCC_DMA1);

	rcc_periph_clock_enable(RCC_USART1);
}

static void uart_setup(void)
{
	/* setup GPIO: tx on PA9 */
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO9); // TX pin

        /* Setup UART parameters. */
	usart_set_baudrate(USART1, 115200);
	usart_set_databits(USART1, 8);
	usart_set_stopbits(USART1, USART_STOPBITS_1);
	usart_set_parity(USART1, USART_PARITY_NONE);
	usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);
	usart_set_mode(USART1, USART_MODE_TX); // no tx for now

	/* Finally enable the USART. */
	usart_enable(USART1);

	printf("Hello world!\n");
}

#define TICK_NS (1000/72)
#define WS0 (350 / TICK_NS)
#define WS1 (800 / TICK_NS)
#define WSP (1300 / TICK_NS)
#define WSL (20000 / TICK_NS)

#define DMA_BANK_SIZE 40 * 8 * 3
#define DMA_SIZE (DMA_BANK_SIZE*2)
static uint8_t dma_data[DMA_SIZE];
static volatile uint32_t led_data[LED_COUNT];
static volatile uint32_t led_cur = 0;


static uint32_t frequency_millihertz = 0;

static void pwm_setup(void) {
	/* Configure GPIOs: OUT=PA7 */
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,
	    GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_TIM3_CH2 );

	rcc_periph_reset_pulse(RST_TIM3);

	timer_set_mode(TIM3, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);

	timer_disable_oc_output(TIM3, TIM_OC2);
	timer_set_oc_mode(TIM3, TIM_OC2, TIM_OCM_PWM1);
	timer_disable_oc_clear(TIM3, TIM_OC2);
	timer_set_oc_value(TIM3, TIM_OC2, 0);
	timer_enable_oc_preload(TIM3, TIM_OC2);
	timer_set_oc_polarity_high(TIM3, TIM_OC2);
	timer_enable_oc_output(TIM3, TIM_OC2);

	timer_set_dma_on_update_event(TIM3);
	timer_enable_irq(TIM3, TIM_DIER_UDE); // in fact, enable DMA on update

	timer_enable_preload(TIM3);
	timer_continuous_mode(TIM3);
	timer_set_period(TIM3, WSP);

	timer_enable_counter(TIM3);
}

static void populate_dma_data(uint8_t *dma_data_bank) {
	for(int i=0; i<DMA_BANK_SIZE;) {
		led_cur = led_cur % (LED_COUNT+3);
		if(led_cur < LED_COUNT) {
			uint32_t v = led_data[led_cur];
			for(int j=0; j<24; j++) {
				dma_data_bank[i++] = (v & 0x800000) ? WS1 : WS0;
				v <<= 1;
			}
		} else {
			for(int j=0; j<24; j++) {
				dma_data_bank[i++] = 0;
			}
		}
		led_cur++;
	}
}

static void handle_cmd(uint8_t cmd) {
	static enum {
		STATE_WAITING,
		STATE_GOT_ID1,
		STATE_GOT_ID2,
		STATE_GOT_COL1,
		STATE_GOT_COL2,
		STATE_GOT_COL3,
		STATE_GOT_COL4,
		STATE_GOT_COL5,
	} cmd_state = STATE_WAITING;
	static uint16_t id = 0;
	static uint16_t col_r = 0;
	static uint16_t col_g = 0;
	static uint16_t col_b = 0;

	gpio_toggle(GPIOC, GPIO13);	/* LED on/off */

	switch(cmd_state) {
	case STATE_WAITING:
		id = cmd;
		/* send a bunch of 0x00 to re-sync protocol */
		if(id != 0x00)
			cmd_state = STATE_GOT_ID1;
		break;
	case STATE_GOT_ID1:
		id = (id << 8) + cmd;
		cmd_state = STATE_GOT_ID2;
		break;
	case STATE_GOT_ID2:
		col_r = cmd;
		cmd_state = STATE_GOT_COL1;
		break;
	case STATE_GOT_COL1:
		cmd_state = STATE_GOT_COL2;
		break;
	case STATE_GOT_COL2:
		col_g = cmd;
		cmd_state = STATE_GOT_COL3;
		break;
	case STATE_GOT_COL3:
		cmd_state = STATE_GOT_COL4;
		break;
	case STATE_GOT_COL4:
		col_b = cmd;
		cmd_state = STATE_GOT_COL5;
		break;
	case STATE_GOT_COL5:
		id -= ID_OFFSET;
		if(id < LED_COUNT)
			led_data[id] = (col_g << 16) | (col_r << 8) | (col_b);
		cmd_state = STATE_WAITING;
		break;
	}
}


static void dma_int_enable(void) {
	/* SPI1 TX on DMA1 Channel 3 */
	nvic_set_priority(NVIC_DMA1_CHANNEL3_IRQ, 0);
	nvic_enable_irq(NVIC_DMA1_CHANNEL3_IRQ);
}

/* Not used in this example
static void dma_int_disable(void) {
 	nvic_disable_irq(NVIC_DMA1_CHANNEL3_IRQ);
}
*/

static int timer_dma(uint8_t *tx_buf, int tx_len)
{
	dma_int_enable();	

	/* Reset DMA channels*/
	dma_channel_reset(DMA1, DMA_CHANNEL3);

	/* Set up tx dma */
	dma_set_peripheral_address(DMA1, DMA_CHANNEL3, (uint32_t)&TIM_CCR2(TIM3));
	dma_set_memory_address(DMA1, DMA_CHANNEL3, (uint32_t)tx_buf);
	dma_set_number_of_data(DMA1, DMA_CHANNEL3, tx_len);
	dma_set_read_from_memory(DMA1, DMA_CHANNEL3);
	dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL3);
	dma_set_peripheral_size(DMA1, DMA_CHANNEL3, DMA_CCR_PSIZE_32BIT);
	dma_set_memory_size(DMA1, DMA_CHANNEL3, DMA_CCR_MSIZE_8BIT);
	dma_set_priority(DMA1, DMA_CHANNEL3, DMA_CCR_PL_HIGH);

	dma_enable_circular_mode(DMA1, DMA_CHANNEL3);
	dma_enable_transfer_complete_interrupt(DMA1, DMA_CHANNEL3);
	dma_enable_half_transfer_interrupt(DMA1, DMA_CHANNEL3);
	dma_enable_channel(DMA1, DMA_CHANNEL3);

	return 0;
}

/* SPI transmit completed with DMA */
void dma1_channel3_isr(void)
{
	if ((DMA1_ISR & DMA_ISR_TCIF3) != 0) {
		DMA1_IFCR |= DMA_IFCR_CTCIF3;
		populate_dma_data(&dma_data[DMA_BANK_SIZE]);
	}
	if ((DMA1_ISR & DMA_ISR_HTIF3) != 0) {
		DMA1_IFCR |= DMA_IFCR_CHTIF3;
		populate_dma_data(dma_data);
	}
}

void tim1_cc_isr(void)
{
	timer_clear_flag(TIM1, TIM_SR_CC1IF);
	printf("tim1_cc_isr %d %d\n", TIM1_CCR1, timer_get_flag(TIM1, TIM_SR_CC1OF));
	frequency_millihertz = 1000*65536 / (uint32_t)TIM1_CCR1;
	printf("%d mHz\n", frequency_millihertz);
//timer_get_flag(TIM1, TIM_SR_CC1OF)
}

void tim1_up_isr(void)
{
	timer_clear_flag(TIM1, TIM_SR_UIF);
	printf("tim1_up_isr, %d\n", timer_get_counter(TIM1));
	// overflow

	frequency_millihertz = 0;
}

/*void tick_leds(void)
{
	static uint64_t frame = 0;
	frame++;

	uint64_t speed[] = {10,8,11,14,10,12,7,9,11,8,13,10};

	for (int i=0; i<10; i++)
	{
		int col_r = 255;
		int col_b = 0;
		int col_g = 65 + (int)(
			sin(i + frame/6000.0) * 40.0  +  
			sin(frame * speed[i] / 10.0 / 4000.0 * 5.13) * 25.0
		);
		col_b = pow((fmax(0,sin(frame * speed[i] / 200000.0) - 0.7) / 0.3),2) * col_g / 2.0;
		//col_r /= 10;
		//col_g /= 10;
		//col_b /= 10;
		led_data[i] = (col_g << 16) | (col_r << 8) | (col_b);
	}

}*/

void tick_leds(void)
{
	for (int i=0; i<10; i++)
	{
		int val = frequency_millihertz - i*1000;
		val = val < 0 ? 0 : val;
		val = val > 150 ? 150 : val;
		val = val * 255 / 150;
		led_data[i] = val << 8;
	}
}

static void timer_setup(void)
{
	rcc_periph_clock_enable(RCC_TIM1);
	rcc_periph_reset_pulse(RST_TIM1);

	/* Configure GPIOs: tacho in = PA8 */
	gpio_set_mode(GPIOA, GPIO_MODE_INPUT,
	    GPIO_CNF_INPUT_PULL_UPDOWN, GPIO_TIM1_CH1);
	gpio_set(GPIOA, GPIO_TIM1_CH1);

	timer_set_mode(TIM1, TIM_CR1_CKD_CK_INT_MUL_4, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
	timer_disable_preload(TIM1);
	timer_continuous_mode(TIM1);
	timer_update_on_overflow(TIM1); // that means: generate update events *only* on overflows,
	                                // not on e.g. reset through the trigger input.


	timer_set_period(TIM1, 0xFFFF);
	timer_set_prescaler(TIM1, 78000000L / 65536 - 1);

	timer_ic_set_input(TIM1, TIM_IC1, TIM_IC_IN_TI1);
	timer_ic_set_polarity(TIM1, TIM_IC1, TIM_IC_RISING);
	timer_ic_set_filter(TIM1, TIM_IC1, TIM_IC_CK_INT_N_8);//TIM_IC_DTF_DIV_32_N_8);
	timer_ic_enable(TIM1, TIM_IC1);

	timer_slave_set_filter(TIM1, TIM_IC_CK_INT_N_8);
	timer_slave_set_trigger(TIM1, TIM_SMCR_TS_TI1FP1); // trigger input := CH1
	timer_slave_set_mode(TIM1, TIM_SMCR_SMS_RM); // timer reset on trigger input

	nvic_enable_irq(NVIC_TIM1_UP_IRQ);
	nvic_enable_irq(NVIC_TIM1_CC_IRQ);
	timer_enable_irq(TIM1, TIM_DIER_UIE | TIM_DIER_CC1IE); // in fact, enable DMA on update

	timer_enable_counter(TIM1);

	nvic_set_priority(NVIC_TIM1_UP_IRQ, 0xf << 4); // Set lowest priority in order to not interfere
	nvic_set_priority(NVIC_TIM1_CC_IRQ, 0xf << 4); // with the timing critical WS2812 driver
}

int main(void)
{
	clock_setup();

	// LED
	gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
	gpio_toggle(GPIOC, GPIO13);	/* LED on/off */

	uart_setup();


	memset(dma_data, 0, DMA_SIZE);
	memset((void*)led_data, 0, LED_COUNT*sizeof(*led_data));
	for (int i=0; i<LED_COUNT; i++)
	{
		//int col_r = (i*30) % 255;
		//int col_g = (255-col_r);
		//int col_b = 127;
		int col_b = 0;
		int col_r = 255;
		int col_g = 127 - 255/2 + ((i*400) % 251)/2;
		led_data[i] = (col_g << 16) | (col_r << 8) | (col_b);
	}
	//populate_dma_data(dma_data);
	//populate_dma_data(&dma_data[DMA_BANK_SIZE]);

	timer_dma(dma_data, DMA_SIZE);
	pwm_setup();

	timer_setup();
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
