#pragma once

/* USART module
 * Usage:
 *   - call uart_setup();
 *   - use printf()
 */

void uart_setup(void);
int _write(int file, char *ptr, int len);
