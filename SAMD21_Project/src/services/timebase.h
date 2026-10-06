/* timebase.h - system time from the TC3 1 ms tick.
 *
 * One free-running clock for the whole firmware. Nothing ever resets it;
 * each task keeps its own "last run" timestamp and compares against now:
 *
 *     if (timebase_ms() - t_last >= PERIOD_MS) { t_last += PERIOD_MS; ... }
 *
 * Always compare with unsigned subtraction ("now - then >= period") so the
 * result stays correct across the counter wrap (ms: ~49.7 days,
 * us: ~71.6 minutes). Never use "now >= then + period", and never use ==.
 */
#ifndef TIMEBASE_H
#define TIMEBASE_H

#include <stdint.h>

/* Register the TC3 callback and start TC3. Call once, after SYS_Initialize(). */
void timebase_init(void);

/* Milliseconds since timebase_init(). Safe from main loop and ISRs. */
uint32_t timebase_ms(void);

/* Microseconds since timebase_init(), 1 us resolution (ms tick + TC3 count).
 * Intended for encoder edge timestamps. Safe from main loop and ISRs,
 * including ISRs of higher priority than TC3 (e.g. EIC). Takes ~1-2 us. */
uint32_t timebase_us(void);

/* Milliseconds elapsed since a timestamp previously taken with timebase_ms(). */
static inline uint32_t timebase_elapsed_ms(uint32_t since_ms)
{
    return timebase_ms() - since_ms;
}

#endif /* TIMEBASE_H */
