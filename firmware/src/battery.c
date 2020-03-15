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

#include "battery.h"
#include <stdio.h>

#define CELL_FULL_MILLIVOLTS 4200
#define MARGIN_MILLIVOLTS 200

static int batt_estimate_cells(int millivolts)
{
	for (int n=1; n<1000; n++)
		if (millivolts < n * CELL_FULL_MILLIVOLTS + MARGIN_MILLIVOLTS)
			return n;
	return -1; // wtf
}

// remaining:               100    95   90    85    80    75    70    65    60    55    50    45    40    35     30    25    20    15     10   5    0
static int voltages[21] = {4200, 4000, 3950, 3890, 3840, 3800, 3760, 3730, 3700, 3675, 3650,  3630, 3610, 3600, 3585, 3550, 3510, 3450, 3380, 3300, 3000};
#define N_VOLTAGES 21
#define N_INTERVALS (N_VOLTAGES-1)

static int batt_calc_percent(int millivolts, int n_cells)
{
	int millivolts_per_cell = millivolts / n_cells;

	if (millivolts_per_cell >= 4200) return 100;
	if (millivolts_per_cell <= 3000) return 0;

	int lo = 0;
	int hi = N_INTERVALS;
	int i;
	while (1)
	{
		i = (lo+hi)/2;
		if (voltages[i+1] <= millivolts_per_cell && millivolts_per_cell <= voltages[i])
			break;
		if (millivolts_per_cell < voltages[i])
			lo = i;
		else // i.e. voltages[i+1] < millivolts_per_cell
			hi = i;
	}

	return 100 - i * 5 - 5 * (voltages[i] - millivolts_per_cell) / (voltages[i] - voltages[i+1]);
}

int batt_cells = -1;

int batt_get_percent(int millivolts)
{
	if (batt_cells < 0)
	{
		batt_cells = batt_estimate_cells(millivolts);
		printf("battery: detected %d cells\n", batt_cells);
	}
	
	return batt_calc_percent(millivolts, batt_cells);
}
