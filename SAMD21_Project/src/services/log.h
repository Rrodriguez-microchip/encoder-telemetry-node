/* log.h - non-blocking UART line output.
 *
 * Why not just printf: the generated SERCOM5_USART_Write() blocks forever
 * spinning on the DRE flag (plib_sercom5_usart.c), and printf() routes through
 * it. If the TX ever wedges (hardware fault, a terminal holding flow control,
 * a bad cable), a plain printf would hang the whole superloop -- the encoder
 * would stop being serviced. On an unattended factory node that is a silent
 * death.
 *
 * log_line() instead writes byte-by-byte only while the transmitter reports
 * ready, and DROPS the rest of the line if it isn't. Losing a telemetry line
 * is fine (it is sent again in 100 ms); stalling the loop is not. A dropped-
 * line counter is exposed so the condition is visible rather than hidden.
 */
#ifndef LOG_H
#define LOG_H

#include <stdint.h>

/* Emit s followed by CRLF, non-blocking. Bytes are written only while the
 * UART transmitter is ready; if it stalls mid-line the remainder is dropped
 * and the drop counter increments. Safe to call from the main loop. */
void log_line(const char *s);

/* Number of lines that were truncated/dropped because the TX wasn't ready.
 * Nonzero means the UART is not keeping up or is wedged. */
uint32_t log_dropped(void);

#endif /* LOG_H */
