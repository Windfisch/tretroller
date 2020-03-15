#pragma once

/* ADC module. Reads and filters the analog-to-digital-converter.
 *
 * Resources:
 *   - ADC1
 *   - PA0
 */

/** The filtered ADC reading. This is the average over 100 samples, because
  * otherwise, the signal would by noise as hell */
extern int adc_value;

/** Must be called periodically. Polls the ADC and updates adc_value */
void adc_poll(void);

/** Initializes the ADC. Must be called before using this module */
void adc_init(void);
