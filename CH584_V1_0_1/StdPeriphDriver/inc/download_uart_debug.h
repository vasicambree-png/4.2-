#ifndef DOWNLOAD_UART_DEBUG_H
#define DOWNLOAD_UART_DEBUG_H

/* Match the paired firmware: UART3 owns baseboard commands; UART1 is logs.
 * Explicitly set to 1 only for the separate PC protocol test profile. */
#ifndef DOWNLOAD_UART_DEBUG
#define DOWNLOAD_UART_DEBUG 0
#endif

#endif
