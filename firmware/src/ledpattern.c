#include "math.h"
#include "ledpattern.h"
#include "color.h"
#include "noise.h"
#include <stdio.h>

static int snake_value(int currpos, int snakehead, int snakelen, int fadeout, int full)
{
	if (snakehead <= currpos && currpos <= snakehead+snakelen)
	{
		int dist = min(currpos - snakehead, snakehead+snakelen-currpos);
		return min( (full*dist/fadeout) >> SHIFT  , full);
	}
	else
		return 0;
}

/* requires exactly 5 front lights */
static int show_cell(int batt_cells, int i)
{
	int result = 0;
	switch (batt_cells)
	{
		case -1: result = 0; break;
		case 1: result = i==2; break;
		case 2: result = (i==1 || i==3); break;
		case 3: result = (i==0 || i==2 || i==4); break;
		default: result = (i<batt_cells);
	}
	return result;
}

void ledpattern_bat_empty(volatile uint32_t led_data[], int t, int batt_cells)
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

static void ledpattern_front_bat_and_slow_info_brightness(volatile uint32_t led_data[], int t, int batt_cells, int batt_percent, int slow_warning, int brightness)
{
	/* use this many LEDs for battery display on the side strips */
	#define N_BAT_LEDS N_SIDE
	
	/* smooth the battery percentage over time */
	static int batt_percent_buf = 0; // hundreths of a percent.
	batt_percent_buf = batt_percent_buf + (batt_percent * 100 - batt_percent_buf) / 20;

	/* battery state on the sides */
	for (int i=0; i<N_SIDE; i++)
	{
		int value = clamp(batt_percent_buf / 10 * N_BAT_LEDS - i*1000, 0, 1000) * brightness / 1000;
		led_data[i] = hsv2(-t*30+i*300, 1000, value);
		led_data[N_SIDE+N_SIDE+N_FRONT-1-i] = hsv2(-t*30+i*300 + 1800, 1000, value);
	}

	/* battery cells and slowness warning on the front */
	for (int i=0; i<N_FRONT; i++)
	{ // TODO brightness
		if (show_cell(batt_cells, i))
			led_data[i+N_SIDE] = hsv2(0,0, brightness/2);
		else
			led_data[i+N_SIDE] = hsv2(2400 + 1200 * slow_warning / 120, (t%60)>30 ? 1000 : 0, brightness / 4);
	}
}

void ledpattern_front_bat_and_slow_info(volatile uint32_t led_data[], int t, int batt_cells, int batt_percent, int slow_warning)
{
	ledpattern_front_bat_and_slow_info_brightness(led_data, t, batt_cells, batt_percent, slow_warning, 1000);
}
static void ledpattern_front_bat_and_slow_info2(volatile uint32_t led_data[], int t, int batt_cells, int batt_percent, int slow_warning)
{
	ledpattern_front_bat_and_slow_info_brightness(led_data, t, batt_cells, batt_percent, slow_warning, 750);
}
static void ledpattern_front_bat_and_slow_info3(volatile uint32_t led_data[], int t, int batt_cells, int batt_percent, int slow_warning)
{
	ledpattern_front_bat_and_slow_info_brightness(led_data, t, batt_cells, batt_percent, slow_warning, 500);
}
static void ledpattern_front_bat_and_slow_info4(volatile uint32_t led_data[], int t, int batt_cells, int batt_percent, int slow_warning)
{
	ledpattern_front_bat_and_slow_info_brightness(led_data, t, batt_cells, batt_percent, slow_warning, 250);
}

static void ledpattern_front_knightrider_brightness(volatile uint32_t led_data[], int t, int batt_cells, int batt_percent, int slow_warning, int brightness)
{
	(void) batt_cells;
	(void) batt_percent;
	(void) slow_warning;

	const int N_SLOTS = N_SIDE + 1 + N_SIDE;
	const int FRONT_LED = 0;

	for (int i=0; i<N_FRONT; i++)
		if (i != FRONT_LED)
			led_data[i+N_SIDE] = 0;

	const int fulllength = ((2*N_SLOTS)<<SHIFT);
	const int snakelen = 7 << SHIFT;
	int snakehead = (15 * ( ((fixed_t)t) << SHIFT) / FPS) % fulllength;

	for (int i=0; i<N_SLOTS; i++)
	{
		int led;
		if (i < N_SIDE)
			led = i;
		else if (i == N_SIDE)
			led = N_SIDE+FRONT_LED;
		else
			led = N_SIDE+N_FRONT + i - N_SIDE-1;

		int curr_pos = i << SHIFT;

		int value = max( max(
			snake_value(curr_pos, snakehead, snakelen, 3, 1000),
			snake_value( ((2*N_SLOTS)<<SHIFT) - curr_pos, snakehead, snakelen, 3, 1000) ),
			snake_value(curr_pos, snakehead-fulllength, snakelen, 3, 1000)
			);
		int saturation = 1000 - (max(0, value-500))/3;

		led_data[led] = hsv2(0, saturation, value * brightness / 1000);
	}
}

void ledpattern_front_knightrider(volatile uint32_t led_data[], int t, int batt_cells, int batt_percent, int slow_warning)
{
	ledpattern_front_knightrider_brightness(led_data, t, batt_cells, batt_percent, slow_warning, 1000);
}
void ledpattern_front_knightrider2(volatile uint32_t led_data[], int t, int batt_cells, int batt_percent, int slow_warning)
{
	ledpattern_front_knightrider_brightness(led_data, t, batt_cells, batt_percent, slow_warning, 750);
}
void ledpattern_front_knightrider3(volatile uint32_t led_data[], int t, int batt_cells, int batt_percent, int slow_warning)
{
	ledpattern_front_knightrider_brightness(led_data, t, batt_cells, batt_percent, slow_warning, 500);
}
void ledpattern_front_knightrider4(volatile uint32_t led_data[], int t, int batt_cells, int batt_percent, int slow_warning)
{
	ledpattern_front_knightrider_brightness(led_data, t, batt_cells, batt_percent, slow_warning, 250);
}

void ledpattern_bottom_dots(volatile uint32_t led_data[], int t, fixed_t pos0, fixed_t velocity, int brightness)
{
	(void) velocity;

	const int DOT_DISTANCE = (15 << SHIFT) + (17<<SHIFT)/41;
	const int DOT_SIZE = 7 << SHIFT;
	const int DOT_FADEOUT = 3;
	const int FADEOUT_ZONE = 7;
	
	static fixed_t velo_smooth = 0;
	velo_smooth += (velocity - velo_smooth) / 10;

	fixed_t wobble_amount = ONE - clamp(velo_smooth, 0, ONE);

	for (int i=0; i<N_BOTTOM; i++)
	{
		fixed_t pos = (i << SHIFT) + pos0;
		int hue = ((pos / DOT_DISTANCE) * 1481) % 3600;
		fixed_t wobble = (wobble_amount * sini( (  ((long long int)t) * SIN_PERIOD * (7000+hue)/100000 / FPS)%SIN_PERIOD )*2/ONE);
		fixed_t pos_wrapped = pos % DOT_DISTANCE;
		int value = max(
			snake_value(pos_wrapped, -wobble, DOT_SIZE, DOT_FADEOUT, 1000),
			snake_value(pos_wrapped -wobble+ DOT_DISTANCE, 0, DOT_SIZE, DOT_FADEOUT, 1000)
		);

		value = value * snake_value(i<<SHIFT, FADEOUT_ZONE, (N_BOTTOM<<SHIFT)-2*FADEOUT_ZONE, FADEOUT_ZONE, 1000) / 1000;

		uint32_t color = hsv2(hue, 700, value*brightness/1000 );

		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM-1-i] = color;
		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM+i] = color;
	}
	
}

void ledpattern_bottom_3color(volatile uint32_t led_data[], int t, fixed_t pos0, fixed_t velocity, int brightness)
{
	(void) t; // unused
	(void) velocity;

	for (int i=0; i<N_BOTTOM; i++)
	{
		fixed_t pos = (i << SHIFT) + pos0;
		int r,g,b;
		switch ((((pos/30)>>SHIFT) % 3) )
		{
			case 0: r=g=0; b=255*brightness/1000; break;
			case 1: r=b=0; g=255*brightness/1000; break;
			case 2: g=b=0; r=255*brightness/1000; break;
			default:
				r=g=b=64;
		}

		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM-1-i] = RGBg(r,g,b);
		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM+i] = RGBg(r,g,b);
	}
}

#define NUM(x) (((fixed_t)x)<<SHIFT)

void ledpattern_bottom_lava(volatile uint32_t led_data[], int t, fixed_t pos0, fixed_t velocity, int brightness)
{
	(void) pos0;
	(void) velocity;

	for (int i=0; i<N_BOTTOM; i++)
	{
		// lava
		int hue = 400 + ((200 * fractal_noise( NUM(i) / 7, NUM(t)/60, ONE/2, ONE/4, ONE/8 )) >> SHIFT);
		int value = 600 + ((400*fractal_noise( ((i+41)<<SHIFT) / 20, (t<<SHIFT)/300, (1<<SHIFT)/2, (1<<SHIFT)/4, (1<<SHIFT)/8 )) >> SHIFT);
		int saturation = 900 + ((100*fractal_noise( ((i+129)<<SHIFT) / 9, (t<<SHIFT)/150, (1<<SHIFT)/2, (1<<SHIFT)/4, (1<<SHIFT)/8 )) >> SHIFT);
		
		// water
		//int hue = 2100 + ((600 * fractal_noise( NUM(i) / 7, NUM(t)/60, ONE/2, ONE/4, ONE/8 )) >> SHIFT);
		//int value = 750 + ((250*fractal_noise( ((i+41)<<SHIFT) / 20, (t<<SHIFT)/300, (1<<SHIFT)/2, (1<<SHIFT)/4, (1<<SHIFT)/8 )) >> SHIFT);
		//int saturation = 500 + ((500*fractal_noise( ((i+129)<<SHIFT) / 9, (t<<SHIFT)/150, (1<<SHIFT)/2, (1<<SHIFT)/4, (1<<SHIFT)/8 )) >> SHIFT);

		// begin to desaturate the color at a speed of 50 leds/sec. Fully desaturate at 50+50 leds/sec.
		uint32_t color = hsv2(hue, saturation, value*brightness/1000);
		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM-1-i] = color;
		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM+i] = color;
	}
}

void ledpattern_bottom_water(volatile uint32_t led_data[], int t, fixed_t pos0, fixed_t velocity, int brightness)
{
	(void) pos0;
	(void) velocity;

	for (int i=0; i<N_BOTTOM; i++)
	{
		// lava
		//int hue = 400 + ((200 * fractal_noise( NUM(i) / 7, NUM(t)/60, ONE/2, ONE/4, ONE/8 )) >> SHIFT);
		//int value = 600 + ((400*fractal_noise( ((i+41)<<SHIFT) / 20, (t<<SHIFT)/300, (1<<SHIFT)/2, (1<<SHIFT)/4, (1<<SHIFT)/8 )) >> SHIFT);
		//int saturation = 900 + ((100*fractal_noise( ((i+129)<<SHIFT) / 9, (t<<SHIFT)/150, (1<<SHIFT)/2, (1<<SHIFT)/4, (1<<SHIFT)/8 )) >> SHIFT);
		
		// water
		int hue = 2100 + ((600 * fractal_noise( NUM(i) / 7, NUM(t)/60, ONE/2, ONE/4, ONE/8 )) >> SHIFT);
		int value = 750 + ((250*fractal_noise( ((i+41)<<SHIFT) / 20, (t<<SHIFT)/300, (1<<SHIFT)/2, (1<<SHIFT)/4, (1<<SHIFT)/8 )) >> SHIFT);
		int saturation = 500 + ((500*fractal_noise( ((i+129)<<SHIFT) / 9, (t<<SHIFT)/150, (1<<SHIFT)/2, (1<<SHIFT)/4, (1<<SHIFT)/8 )) >> SHIFT);

		// begin to desaturate the color at a speed of 50 leds/sec. Fully desaturate at 50+50 leds/sec.
		uint32_t color = hsv2(hue, saturation, value*brightness/1000);
		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM-1-i] = color;
		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM+i] = color;
	}
}

void ledpattern_bottom_snake(volatile uint32_t led_data[], int t, fixed_t pos0, fixed_t velocity, int brightness)
{
	const int fulllength = ((2*N_BOTTOM)<<SHIFT);
	const int snakelen = 10 << SHIFT;
	int snakehead = (30 * ( ((fixed_t)t) << SHIFT) / FPS) % fulllength;

	for (int i=0; i<2*N_BOTTOM; i++)
	{
		int hue = 3600 * t / FPS / 60;
		int saturation = 500;

		int currpos = (i<<SHIFT);

		int value = 0;
	
		if (snakehead <= currpos && currpos <= snakehead+snakelen)
			value = snake_value(currpos, snakehead, snakelen, 2, brightness);
		if (snakehead - fulllength <= currpos && currpos <= snakehead+snakelen-fulllength)
			value = snake_value(currpos, snakehead-fulllength, snakelen, 2, brightness);

		uint32_t color = hsv2(hue, saturation, value);
		led_data[N_SIDE+N_FRONT+N_SIDE+i] = color;
	}
}


void ledpattern_bottom_position_color(volatile uint32_t led_data[], int t, fixed_t pos0, fixed_t velocity, int brightness)
{
	(void) t;
	(void) velocity;

	for (int i=0; i<N_BOTTOM; i++)
	{
		int hue = (pos0*12)>>SHIFT;
		// begin to desaturate the color at a speed of 50 leds/sec. Fully desaturate at 50+50 leds/sec.
		uint32_t color = hsv2(hue, 1000, brightness);
		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM-1-i] = color;
		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM+i] = color;
	}
}

void ledpattern_bottom_rainbow(volatile uint32_t led_data[], int t, fixed_t pos0, fixed_t velocity, int brightness)
{
	(void) t; // unused

	for (int i=0; i<N_BOTTOM; i++)
	{
		fixed_t pos = (i << SHIFT) + pos0;
		int hue = (pos*120)>>SHIFT;

		// begin to desaturate the color at a speed of 50 leds/sec. Fully desaturate at 50+50 leds/sec.
		int saturation = 1000 - clamp( ((velocity - (50<<SHIFT) ) * 1000 / 50) >> SHIFT, 0, 1000);
		uint32_t color = hsv2(hue, saturation, brightness);
		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM-1-i] = color;
		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM+i] = color;
	}
}

void ledpattern_bottom_velocity_color(volatile uint32_t led_data[], int t, fixed_t pos0, fixed_t velocity, int brightness)
{
	(void) pos0; // unused

	static fixed_t velocity_saved = 0;

	if (velocity > 0) velocity_saved = velocity;

	int base_hue = t*(3600/600) / FPS; // one full color revolution per 10 minutes
	int velo_hue = 3600 * (velocity_saved / 200) >> SHIFT; // 200 ledunits per sec makes one full color revolution

	int saturation = 1000 - clamp((500 * velocity_saved / 150) >> SHIFT, 0, 1000);

	//int instant_value = (velocity >> SHIFT) > 5 ? 1000 : 0;
	int instant_value = clamp((1000 * (velocity - (10<<SHIFT)) / 90) >> SHIFT, 0, 1000);
	static int value_smooth = 0;
	value_smooth += (instant_value - value_smooth) / 30;
	
	uint32_t color = hsv2(base_hue + velo_hue, saturation, value_smooth * brightness / 1000);

	for (int i=0; i<N_BOTTOM; i++)
	{
		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM-1-i] = color;
		led_data[N_SIDE+N_FRONT+N_SIDE+N_BOTTOM+i] = color;
	}
}

ledpattern_bottom_t ledpatterns_bottom[N_BOTTOM_PATTERNS] = {
	ledpattern_bottom_rainbow,
	ledpattern_bottom_dots,
	ledpattern_bottom_3color,
	ledpattern_bottom_water,
	ledpattern_bottom_lava,
	ledpattern_bottom_snake,
	ledpattern_bottom_position_color,
	ledpattern_bottom_velocity_color
};

ledpattern_front_t ledpatterns_front[N_FRONT_PATTERNS] = {
	ledpattern_front_bat_and_slow_info,
	ledpattern_front_bat_and_slow_info2,
	ledpattern_front_bat_and_slow_info3,
	ledpattern_front_bat_and_slow_info4,
	ledpattern_front_knightrider,
	ledpattern_front_knightrider2,
	ledpattern_front_knightrider3,
	ledpattern_front_knightrider4
};
