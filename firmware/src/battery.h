#pragma once

/* Battery estimation module.
 *
 * Resources: none
 */

/** Queries the battery state. On its first call, this auto-detects
  * the number of LiPo cells and sets batt_cells accordingly */
int batt_get_percent(int millivolts);

/** Number of battery cells. This value is available 2-3 sec after startup */
extern int batt_cells;
