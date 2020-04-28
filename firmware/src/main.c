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


const uint8_t gamma8[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

static inline int clamp_and_gamma(int val)
{
	return gamma8[
		val < 0 ? 0 :
		val > 255 ? 255 :
		val
	];
}

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

void tim2_isr(void)
{
	static int slow_warning = 120;
	if (slow_warning > 0) slow_warning--;

	static int t = 0;
	t++;

	static int distance = 0; // increases by 60000 per wheel revolution
	distance += frequency_millihertz;

	static int batt_percent = 50;
	int batt_empty = 1;// batt_percent <= 0;

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
	int batt_empty_color = batt_empty_flash ? 0x0000ff : 0x000100;

	for (int i=0; i<N_SIDE; i++)
	{
		int highlight = i <= (batt_percent / 10);

		if (batt_empty)
		{
			led_data[i] = (i%3==0)?batt_empty_color:0;
			led_data[N_SIDE+N_FRONT+N_SIDE-1-i] = (i%3==0) ? batt_empty_color : 0;
		}
		else
		{
			led_data[i] = highlight ? 0x008844 : 0x000022;
			led_data[N_SIDE+N_SIDE+N_FRONT-1-i] = highlight ? 0x448800 : 0x220000;
		}
	}
	for (int i=0; i<N_FRONT; i++)
	{
		int show_cell = 0;
		switch (batt_cells)
		{
			case -1: show_cell = 0; break;
			case 1: show_cell = i==2; break;
			case 2: show_cell = (i==1 || i==3); break;
			case 3: show_cell = (i==0 || i==2 | i==4); break;
			default: show_cell = (i<batt_cells);
		}

		if (batt_empty)
			led_data[i+N_SIDE] = (show_cell ? batt_empty_color : 0);
		else
			led_data[i+N_SIDE] = ((t%60)>30 ? 20 : 0) | (slow_warning<<9) | (show_cell ? 0x808080 : 0);
	}
		//led_data[i+N_SIDE] = 0x00ff00;

	for (int i=0; i<N_BOTTOM; i++)
	{
		fixed_t pos = (i << SHIFT) + pos0;
		fixed_t r = 128 << SHIFT;
		fixed_t g = 128 << SHIFT;
		fixed_t b = 128 << SHIFT;

		/*for (unsigned w=0; w<WAVE_COUNT; w++)
		{
			fixed_t wavepos = (waves[w].speed *t) / FPS;
			int val = sini((SIN_PERIOD * (pos - wavepos)) / waves[w].wavelength);

			r += val * waves[w].r;
			g += val * waves[w].g;
			b += val * waves[w].b;
		}
		*/

		switch ( (((((pos/30)>>SHIFT) % 3) + 3) % 3) )
		{
			case 0: r=g=0; b=255; break;
			case 1: r=b=0; g=255; break;
			case 2: g=b=0; r=255; break;
			default:
				r=g=b=64;
		}

		if (frequency_millihertz > 10000)
		{
			int bla = frequency_millihertz - 9000;
			if (bla > 10000) bla = 10000;
			r = min(255, bla*255 / 3333);
			g = min(255, bla*255 / 6666);
			b = min(255, bla*255 / 10000);
		}
		r <<= SHIFT;
		g <<= SHIFT;
		b <<= SHIFT;
		
		fixed_t mp = (pos / 50) % (1<<SHIFT);
		//int mask = ((mp < (1 << SHIFT) / 5) ?
		//	sini( ((SIN_PERIOD * mp * 5)>>SHIFT) + SIN_PERIOD*1/4) / 2 + (1<<SHIFT)/2 :
		//	(1<<SHIFT)) * 256 >> SHIFT;
		int mask = 256;
		//int dist = 1 + min(i, N_BOTTOM-1-i);
		//if (dist < 3) mask = 256 * dist / 4; else mask = 256;

		r = r * mask / 256;
		g = g * mask / 256;
		b = b * mask / 256;

		const int dim = 1;
		r = clamp_and_gamma((r/dim) >> SHIFT);
		g = clamp_and_gamma((g/dim) >> SHIFT);
		b = clamp_and_gamma((b/dim) >> SHIFT);

		if (batt_empty)
			r=g=b=0;
		
		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM-1-i] = (r << 8) | (g<<16) | b;
		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM+i] = (r << 8) | (g<<16) | b;
	}



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
	adc_init();

	animation_init();

	int i=0;
	while (1) {
		__asm__("wfe");
	}
}
