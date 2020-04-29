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

#pragma once
#include <stdint.h>

/* Color util module
 *
 * Resources: none
 */

/** Converts hexagonal HSV to gamma-corrected 32-bit-words suitable for the ws2812 leds.
  * 
  * hue: 0..3599; saturation: 0..999; value: 0..999
  * Output format: 0x00GGRRBB
  * "Hexagonal" HSV means that hsv(600, 1000, 1000) = 0x00FFFF00 (yellow) is brighter than
  *  hsv(0, 1000, 1000) = 0x0000FF00 (red).
  */
uint32_t hsv(uint32_t hue, uint32_t saturation, uint32_t value);

/** Converts circular HSV to gamma-corrected 32-bit-words suitable for the ws2812 leds.
  * 
  * hue: 0..3599; saturation: 0..999; value: 0..999
  * Output format: 0x00GGRRBB
  * "Circular" HSV means that hsv2(600, 1000, 1000) (yellow) should have the same
  * brightness as hsv(0, 1000, 1000) (red).
  */
uint32_t hsv2(uint32_t hue, uint32_t saturation, uint32_t value);


/** Clamps val into 0..255 and applies a gamma correction for correct led brightness */
int clamp_and_gamma(int val);

#define RGB(r,g,b) (((r) << 8) | (b) | ((g) << 16))
#define RGBg(r,g,b) RGB(clamp_and_gamma(r), clamp_and_gamma(g), clamp_and_gamma(b))
