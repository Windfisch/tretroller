#include "color.h"

static const uint8_t gamma8[] = {
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


int clamp_and_gamma(int val)
{
	return gamma8[
		val < 0 ? 0 :
		val > 255 ? 255 :
		val
	];
}


#define RGB(r,g,b) (((r) << 8) | (b) | ((g) << 16))
#define RGBg(r,g,b) RGB(clamp_and_gamma(r), clamp_and_gamma(g), clamp_and_gamma(b))

// hue: 0..3600
// saturation: 0..1000
// value: 0..1000
uint32_t hsv(uint32_t hue, uint32_t saturation, uint32_t value)
{
	hue %= 3600;

	uint32_t hi = (hue / 600); // 0..5
	uint32_t f = (hue % 600); // 0..599

	uint32_t p = value * (1000-saturation) * 255 / 1000000;
	uint32_t q = value * (1000-saturation*f/600) * 255 / 1000000;
	uint32_t t = value * (1000-saturation*(600-f)/600) * 255 / 1000000;
	uint32_t v = value * 255 / 1000;

	switch (hi)
	{
		case 0: return RGBg(v, t*4/5, p);
		case 1: return RGBg(q, v*4/5, p);
		case 2: return RGBg(p, v*4/5, t);
		case 3: return RGBg(p, q*4/5, v);
		case 4: return RGBg(t, p*4/5, v);
		case 5: return RGBg(v, p*4/5, q);
	}
	for(;;); // cannot happen
}


static int abs(int x) { return (x > 0) ? x : -x; }

uint32_t hsv2(uint32_t hue, uint32_t saturation, uint32_t value)
{
	int light_factor = 3200 - abs((int)(hue % 1200) - 600);
	return hsv(hue, saturation, value * (3200-600) / light_factor);
}
