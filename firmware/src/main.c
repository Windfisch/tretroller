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

#include "color.h"
#include "ws2812.h"
#include "tacho.h"
#include "usart.h"
#include "adc.h"
#include "battery.h"

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


#define SHIFT 16
static uint16_t sin_valuetable[1024] =
{
	0, 1, 3, 4, 6, 7, 9, 10, 12, 14, 15, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31,
	32, 34, 36, 37, 39, 40, 42, 43, 45, 47, 48, 50, 51, 53, 54, 56, 58, 59, 61, 62,
	64, 65, 67, 69, 70, 72, 73, 75, 76, 78, 80, 81, 83, 84, 86, 87, 89, 90, 92, 94,
	95, 97, 98, 100, 101, 103, 105, 106, 108, 109, 111, 112, 114, 115, 117, 119,
	120, 122, 123, 125, 126, 128, 130, 131, 133, 134, 136, 137, 139, 140, 142, 144,
	145, 147, 148, 150, 151, 153, 154, 156, 158, 159, 161, 162, 164, 165, 167, 168,
	170, 171, 173, 175, 176, 178, 179, 181, 182, 184, 185, 187, 188, 190, 192, 193,
	195, 196, 198, 199, 201, 202, 204, 205, 207, 209, 210, 212, 213, 215, 216, 218,
	219, 221, 222, 224, 225, 227, 228, 230, 232, 233, 235, 236, 238, 239, 241, 242,
	244, 245, 247, 248, 250, 251, 253, 254, 256, 257, 259, 260, 262, 264, 265, 267,
	268, 270, 271, 273, 274, 276, 277, 279, 280, 282, 283, 285, 286, 288, 289, 291,
	292, 294, 295, 297, 298, 300, 301, 303, 304, 306, 307, 309, 310, 312, 313, 315,
	316, 318, 319, 321, 322, 324, 325, 327, 328, 330, 331, 333, 334, 336, 337, 339,
	340, 342, 343, 344, 346, 347, 349, 350, 352, 353, 355, 356, 358, 359, 361, 362,
	364, 365, 367, 368, 369, 371, 372, 374, 375, 377, 378, 380, 381, 383, 384, 386,
	387, 388, 390, 391, 393, 394, 396, 397, 399, 400, 402, 403, 404, 406, 407, 409,
	410, 412, 413, 414, 416, 417, 419, 420, 422, 423, 424, 426, 427, 429, 430, 432,
	433, 434, 436, 437, 439, 440, 442, 443, 444, 446, 447, 449, 450, 451, 453, 454,
	456, 457, 458, 460, 461, 463, 464, 466, 467, 468, 470, 471, 472, 474, 475, 477,
	478, 479, 481, 482, 484, 485, 486, 488, 489, 491, 492, 493, 495, 496, 497, 499,
	500, 501, 503, 504, 506, 507, 508, 510, 511, 512, 514, 515, 516, 518, 519, 521,
	522, 523, 525, 526, 527, 529, 530, 531, 533, 534, 535, 537, 538, 539, 541, 542,
	543, 545, 546, 547, 549, 550, 551, 553, 554, 555, 557, 558, 559, 561, 562, 563,
	564, 566, 567, 568, 570, 571, 572, 574, 575, 576, 578, 579, 580, 581, 583, 584,
	585, 587, 588, 589, 590, 592, 593, 594, 596, 597, 598, 599, 601, 602, 603, 604,
	606, 607, 608, 609, 611, 612, 613, 615, 616, 617, 618, 620, 621, 622, 623, 625,
	626, 627, 628, 629, 631, 632, 633, 634, 636, 637, 638, 639, 641, 642, 643, 644,
	645, 647, 648, 649, 650, 652, 653, 654, 655, 656, 658, 659, 660, 661, 662, 664,
	665, 666, 667, 668, 670, 671, 672, 673, 674, 675, 677, 678, 679, 680, 681, 683,
	684, 685, 686, 687, 688, 690, 691, 692, 693, 694, 695, 696, 698, 699, 700, 701,
	702, 703, 704, 706, 707, 708, 709, 710, 711, 712, 714, 715, 716, 717, 718, 719,
	720, 721, 722, 724, 725, 726, 727, 728, 729, 730, 731, 732, 734, 735, 736, 737,
	738, 739, 740, 741, 742, 743, 744, 745, 747, 748, 749, 750, 751, 752, 753, 754,
	755, 756, 757, 758, 759, 760, 761, 762, 763, 765, 766, 767, 768, 769, 770, 771,
	772, 773, 774, 775, 776, 777, 778, 779, 780, 781, 782, 783, 784, 785, 786, 787,
	788, 789, 790, 791, 792, 793, 794, 795, 796, 797, 798, 799, 800, 801, 802, 803,
	804, 805, 806, 807, 808, 809, 810, 811, 812, 813, 813, 814, 815, 816, 817, 818,
	819, 820, 821, 822, 823, 824, 825, 826, 827, 828, 828, 829, 830, 831, 832, 833,
	834, 835, 836, 837, 838, 839, 839, 840, 841, 842, 843, 844, 845, 846, 847, 847,
	848, 849, 850, 851, 852, 853, 854, 854, 855, 856, 857, 858, 859, 860, 860, 861,
	862, 863, 864, 865, 865, 866, 867, 868, 869, 870, 870, 871, 872, 873, 874, 875,
	875, 876, 877, 878, 879, 879, 880, 881, 882, 883, 883, 884, 885, 886, 887, 887,
	888, 889, 890, 890, 891, 892, 893, 894, 894, 895, 896, 897, 897, 898, 899, 900,
	900, 901, 902, 903, 903, 904, 905, 906, 906, 907, 908, 908, 909, 910, 911, 911,
	912, 913, 913, 914, 915, 916, 916, 917, 918, 918, 919, 920, 920, 921, 922, 922,
	923, 924, 925, 925, 926, 927, 927, 928, 929, 929, 930, 930, 931, 932, 932, 933,
	934, 934, 935, 936, 936, 937, 938, 938, 939, 939, 940, 941, 941, 942, 943, 943,
	944, 944, 945, 946, 946, 947, 947, 948, 949, 949, 950, 950, 951, 951, 952, 953,
	953, 954, 954, 955, 955, 956, 957, 957, 958, 958, 959, 959, 960, 960, 961, 962,
	962, 963, 963, 964, 964, 965, 965, 966, 966, 967, 967, 968, 968, 969, 969, 970,
	970, 971, 971, 972, 972, 973, 973, 974, 974, 975, 975, 976, 976, 977, 977, 978,
	978, 978, 979, 979, 980, 980, 981, 981, 982, 982, 983, 983, 983, 984, 984, 985,
	985, 986, 986, 986, 987, 987, 988, 988, 988, 989, 989, 990, 990, 990, 991, 991,
	992, 992, 992, 993, 993, 994, 994, 994, 995, 995, 995, 996, 996, 997, 997, 997,
	998, 998, 998, 999, 999, 999, 1000, 1000, 1000, 1001, 1001, 1001, 1002, 1002,
	1002, 1003, 1003, 1003, 1004, 1004, 1004, 1004, 1005, 1005, 1005, 1006, 1006,
	1006, 1006, 1007, 1007, 1007, 1008, 1008, 1008, 1008, 1009, 1009, 1009, 1009,
	1010, 1010, 1010, 1010, 1011, 1011, 1011, 1011, 1012, 1012, 1012, 1012, 1013,
	1013, 1013, 1013, 1014, 1014, 1014, 1014, 1014, 1015, 1015, 1015, 1015, 1015,
	1016, 1016, 1016, 1016, 1016, 1017, 1017, 1017, 1017, 1017, 1017, 1018, 1018,
	1018, 1018, 1018, 1018, 1019, 1019, 1019, 1019, 1019, 1019, 1019, 1020, 1020,
	1020, 1020, 1020, 1020, 1020, 1020, 1021, 1021, 1021, 1021, 1021, 1021, 1021,
	1021, 1021, 1022, 1022, 1022, 1022, 1022, 1022, 1022, 1022, 1022, 1022, 1022,
	1022, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023
};

#define SIN_PERIOD 4096
static int sini(unsigned val)
{
	val = val % 4096;

	//return ((val < 2048) ? (val << SHIFT >> 10) : ((4096-val) << SHIFT >> 10)) - (1<<SHIFT);

	if (val < 2048) // positive half-wave
	{
		if (val < 1024)
			return sin_valuetable[val] << SHIFT >> 10;
		else
			return sin_valuetable[2047-val] << SHIFT >> 10;
	}
	else
	{ // negative half-wave
		if (val < 3072)
			return -sin_valuetable[val-2048] << SHIFT >> 10;
		else
			return -sin_valuetable[4095-val] << SHIFT >> 10;
	}
}

int clamp(int val, int lo, int hi)
{
	if (val < lo) return lo;
	if (val > hi) return hi;
	return val;
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

typedef int64_t fixed_t;
struct wave_t
{
	fixed_t wavelength; // in LED counts
	fixed_t speed; // LED counts per second
	int r; // range: 0..255
	int g;
	int b;
};

struct wave_t waves[] = { {10<<SHIFT, 0<<SHIFT, 64,0,0}, {17<<SHIFT, 1<<SHIFT, 0,64,64}, {7<<SHIFT, -(1<<SHIFT), 0,64,0} };
//struct wave_t waves[] = {{17 << SHIFT, 10 << SHIFT, 128,128,128}};
#define WAVE_COUNT (sizeof(waves)/sizeof(*waves))


const double WHEEL_RADIUS_MM = 105.;
const double WHEEL_CIRCUMFERENCE_MM = WHEEL_RADIUS_MM * 2 * 3.141592654;
const double LED_DISTANCE_MM = 17.5;



const fixed_t WHEEL_CIRCUMFERENCE_LEDUNITS = (1<<SHIFT) * WHEEL_CIRCUMFERENCE_MM / LED_DISTANCE_MM;


#define N_SIDE 11
#define N_FRONT 5
#define N_BOTTOM 26

#define FPS 60
#define FREQUENCY_FACTOR 1000 // frequency_millihertz / FREQUENCY_FACTOR = wheel frequency in hertz

static int min(int a, int b)
{
	return a>b?b:a;
}

/* requires exactly 5 front lights */
int show_cell(int batt_cells, int i)
{
	int result = 0;
	switch (batt_cells)
	{
		case -1: result = 0; break;
		case 1: result = i==2; break;
		case 2: result = (i==1 || i==3); break;
		case 3: result = (i==0 || i==2 | i==4); break;
		default: result = (i<batt_cells);
	}
	return result;
}

void ledpattern_bat_empty(volatile uint32_t led_data[], int t)
{
	/* "batt empty" flash pattern:
	      1      2     N=3
	   --X-X----X-X----X-X-------------------------------------X-X----X-X----X-X--...
	     \____________________________________________________/
	     \_____/                     BATT_FLASH_PERIOD_LONG
	BATT_FLASH_PERIOD_SHORT
	*/
	#define BATT_FLASH_PERIOD_LONG 2000
	#define BATT_FLASH_PERIOD_SHORT 100
	#define BATT_FLASH_N 2
	int batt_empty_flash = 0;
	for (int i=0; i<BATT_FLASH_N; i++)
	{
		int t0 = (t - i*BATT_FLASH_PERIOD_SHORT) % BATT_FLASH_PERIOD_LONG;
		int t1 = (t - i*BATT_FLASH_PERIOD_SHORT - 8) % BATT_FLASH_PERIOD_LONG;
		if (t0 < 1 || t1 < 2)
			batt_empty_flash = 1;
	}
	int batt_empty_color = batt_empty_flash ? 0xff0000 : 0x000100; // bright blue flash / dim red glow

	for (int i=0; i<N_SIDE; i++)
	{
		led_data[i] = (i%3==0)?batt_empty_color:0;
		led_data[N_SIDE+N_FRONT+N_SIDE-1-i] = (i%3==0) ? batt_empty_color : 0;
	}
	for (int i=0; i<N_FRONT; i++)
	{
		led_data[i+N_SIDE] = (show_cell(batt_cells, i) ? batt_empty_color : 0);
	}
	for (int i=0; i<N_BOTTOM; i++)
	{
		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM-1-i] = 0;
		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM+i] = 0;
	}
}



void ledpattern_bat_and_slow_info(volatile uint32_t led_data[], int t, int batt_cells, int batt_percent, int slow_warning)
{
	/* use this many LEDs for battery display on the side strips */
	#define N_BAT_LEDS N_SIDE
	
	/* smooth the battery percentage over time */
	static int batt_percent_buf = 0; // hundreths of a percent.
	batt_percent_buf = batt_percent_buf + (batt_percent * 100 - batt_percent_buf) / 20;

	/* battery state on the sides */
	for (int i=0; i<N_SIDE; i++)
	{
		int value = clamp(batt_percent_buf / 10 * N_BAT_LEDS - i*1000, 0, 1000);
		led_data[i] = hsv2(-t*30+i*300, 1000, value);
		led_data[N_SIDE+N_SIDE+N_FRONT-1-i] = hsv2(-t*30+i*300 + 1800, 1000, value);
	}

	/* battery cells and slowness warning on the front */
	for (int i=0; i<N_FRONT; i++)
	{
		led_data[i+N_SIDE] = ((t%60)>30 ? 20 : 0) | (slow_warning<<9) | (show_cell(batt_cells, i) ? 0x808080 : 0);
	}
}

void ledpattern_bottom_3color(volatile uint32_t led_data[], int t, fixed_t pos0)
{
	for (int i=0; i<N_BOTTOM; i++)
	{
		fixed_t pos = (i << SHIFT) + pos0;
		int r,g,b;
		switch ((((pos/30)>>SHIFT) % 3) )
		{
			case 0: r=g=0; b=255; break;
			case 1: r=b=0; g=255; break;
			case 2: g=b=0; r=255; break;
			default:
				r=g=b=64;
		}

		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM-1-i] = RGBg(r,g,b);
		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM+i] = RGBg(r,g,b);
	}
}


void ledpattern_bottom_rainbow(volatile uint32_t led_data[], int t, fixed_t pos0, fixed_t velocity)
{
	for (int i=0; i<N_BOTTOM; i++)
	{
		fixed_t pos = (i << SHIFT) + pos0;
		int hue = (pos*120)>>SHIFT;

		// begin to desaturate the color at a speed of 50 leds/sec. Fully desaturate at 50+50 leds/sec.
		int saturation = 1000 - clamp( ((velocity - (50<<SHIFT) ) * 1000 / 50) >> SHIFT, 0, 1000);
		uint32_t color = hsv2(hue, saturation, 1000);
		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM-1-i] = color;
		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM+i] = color;
	}
}

void ledpattern_bottom_color(volatile uint32_t led_data[], int t, fixed_t pos0, fixed_t velocity)
{
	static fixed_t velocity_saved = 0;

	if (velocity > 0) velocity_saved = velocity;

	int base_hue = t*(3600/600) / FPS; // one full color revolution per minute
	int velo_hue = 3600 * (velocity_saved / 200) >> SHIFT; // 200 ledunits per sec makes one full color revolution

	int saturation = 1000 - clamp((500 * velocity_saved / 150) >> SHIFT, 0, 1000);

	//int instant_value = (velocity >> SHIFT) > 5 ? 1000 : 0;
	int instant_value = clamp((1000 * (velocity - (10<<SHIFT)) / 90) >> SHIFT, 0, 1000);
	static int value_smooth = 0;
	value_smooth += (instant_value - value_smooth) / 30;
	
	uint32_t color = hsv2(base_hue + velo_hue, saturation, value_smooth);

	for (int i=0; i<N_BOTTOM; i++)
	{
		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM-1-i] = color;
		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM+i] = color;
	}
}

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

	static int batt_percent = -1;
	int batt_empty = batt_percent <= 0;

	timer_clear_flag(TIM2, TIM_SR_UIF);

	gpio_toggle(GPIOC, GPIO13);	/* LED on/off */

	adc_poll();
	if (t % 100 == 0)
	{
		if (adc_value > 0)
		{
			int batt_millivolts = ADC_VREF_MILLIVOLTS * adc_value * (BAT_R1+BAT_R2) / ADC_MAX / BAT_R1;
			batt_percent = batt_get_percent(batt_millivolts);
			printf("adc value: %d = %d mV -> %d %%\n", adc_value, batt_millivolts, batt_percent);
		}
	}


	fixed_t pos0 = distance * WHEEL_CIRCUMFERENCE_LEDUNITS / (FPS*FREQUENCY_FACTOR);

	if (batt_empty)
	{
		// sets both front/side and bottom leds
		ledpattern_bat_empty(led_data, t);
	}
	else
	{
		// set the front/side leds
		ledpattern_bat_and_slow_info(led_data, t, batt_cells, batt_percent, slow_warning);

		// set the bottom leds
		ledpattern_bottom_rainbow(led_data, t, pos0, velocity);
		//ledpattern_bottom_color(led_data, t, pos0, velocity);
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

	animation_init();

	int i=0;
	while (1) {
		__asm__("wfe");
	}
}
