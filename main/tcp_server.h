#pragma once

/* Create and start the TCP server task (port 23, PRI_TCP, STACK_TCP).
 * Accepts one client at a time; new connections displace existing ones. */
void tcp_server_start(void);
