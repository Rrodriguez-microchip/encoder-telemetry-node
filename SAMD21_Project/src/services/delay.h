/* delay.h - blocking microsecond/millisecond delays.
 *
 * Uses the Cortex-M0+ SysTick as a free-running 24-bit down-counter at the
 * CPU clock (48 MHz). No interrupt is used, so it does not conflict with the
 * TC3 1 ms tick, and ISRs that fire during a delay only lengthen it.
 *
 * Intended for short, one-off waits (LCD init/strobe timing). The superloop
 * itself must stay non-blocking; scheduling uses the TC3 timebase (later).
 */
#ifndef DELAY_H
#define DELAY_H

#include <stdint.h>

void delay_init(void);          /* call once after SYS_Initialize() */
void delay_us(uint32_t us);
void delay_ms(uint32_t ms);

#endif /* DELAY_H */
