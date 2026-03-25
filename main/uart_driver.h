#pragma once

#include <stddef.h>

/* Initialise UART0 at UART_BAUD, install the driver, and start the
 * uart_task (PRI_UART, STACK_UART).
 * Must be called before wifi_manager_init() so boot messages go out. */
void uart_driver_init(void);

/* Write a null-terminated string to UART0. Thread-safe (uses driver lock). */
void uart_send_str(const char *s);
