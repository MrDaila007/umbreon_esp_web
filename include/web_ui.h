#pragma once

/* Declaration of the embedded HTML/CSS/JS single-page application.
 * The definition is in main/web_ui.cpp (C++ raw string literal).
 * The string is placed in flash rodata — not in RAM.
 *
 * Settings panel keys must stay in sync with Zephyr `$CFG:` / `$SET:` (wifi_cmd.c).
 * Speed-related UI keys include: SPD1, SPD2, SLW (m/s² setpoint slew; 0 = instant),
 * KOP / KOM (start kick: % of ESC span, duration ms; KOP 0 = off). */
extern const char PAGE_HTML[];
