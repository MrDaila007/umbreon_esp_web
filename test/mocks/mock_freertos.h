#pragma once
/* Minimal FreeRTOS type stubs for host-side compilation.
 * Only types — no function implementations needed for Tier 1 tests. */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Tick type */
typedef uint32_t TickType_t;
#define portTICK_PERIOD_MS 1
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define portMAX_DELAY 0xFFFFFFFFUL

/* Queue / Semaphore handles (opaque pointers) */
typedef void *QueueHandle_t;
typedef void *SemaphoreHandle_t;

/* Base type */
typedef long BaseType_t;
#define pdTRUE  1
#define pdFALSE 0
#define pdPASS  1

/* FreeRTOS result stubs — not called in Tier 1, just need to compile */
static inline BaseType_t xQueueSend(QueueHandle_t q, const void *item, TickType_t wait)
    { (void)q; (void)item; (void)wait; return pdTRUE; }
static inline BaseType_t xQueueReceive(QueueHandle_t q, void *item, TickType_t wait)
    { (void)q; (void)item; (void)wait; return pdFALSE; }
static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t wait)
    { (void)s; (void)wait; return pdTRUE; }
static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t s)
    { (void)s; return pdTRUE; }
static inline uint32_t xTaskGetTickCount(void) { return 0; }
