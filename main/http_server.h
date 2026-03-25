#pragma once

/* Create and start the HTTP server task (port 80, PRI_HTTP, STACK_HTTP).
 * Serves PAGE_HTML (from web_ui.h) for any GET request. */
void http_server_start(void);
