#pragma once

/* Create and start the LED task (PRI_LED, STACK_LED).
 * Call after queues and g_led_state are initialised. */
void led_task_start(void);
