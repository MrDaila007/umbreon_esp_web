#pragma once

/* Declaration of the embedded HTML/CSS/JS single-page application.
 * The definition is in main/web_ui.cpp (C++ raw string literal).
 * The string is placed in flash rodata — not in RAM. */
extern const char PAGE_HTML[];
