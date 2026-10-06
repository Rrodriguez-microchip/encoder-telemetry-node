/* app.c - top-level application.
 *
 * Bare-metal superloop: no task ever waits, it only checks the clock.
 * Each task owns a "last run" timestamp; the shared timebase never resets.
 *
 * Current stage: P1 encoder + telemetry + robustness (KY-040 hand knob).
 *   - watchdog kicked every pass (resets the node if the loop ever hangs)
 *   - LED toggles every 500 ms (alive indicator)
 *   - encoder_task() runs every pass (debounce + RPM)
 *   - telemetry_task() emits the ~10 Hz JSON line over UART (MQTT payload shape),
 *     non-blocking so a stalled UART can't hang the loop
 *   - LCD row 0: count / detents / revs   row 1: rpm / dir / button
 */
#include <stdio.h>
#include "definitions.h"
#include "app/app.h"
#include "services/delay.h"
#include "services/timebase.h"
#include "drivers/lcd_hd44780.h"
#include "drivers/encoder.h"
#include "services/telemetry.h"
#include "services/wdt.h"

#define LED_PERIOD_MS       500U
#define LCD_PERIOD_MS       100U     /* readable, no flicker (overwrite+pad) */

static char dir_char(int8_t d)
{
    return (d > 0) ? '+' : (d < 0) ? '-' : '0';
}

void app_run(void)
{
    char line[LCD_COLS + 1U];

    delay_init();
    timebase_init();
    lcd_init();
    encoder_init();                 /* after timebase: edges stamp with us */
    wdt_init();                     /* start the watchdog; kicked every pass */
    printf("\r\nP1 encoder bench\r\n");

    uint32_t now   = timebase_ms();
    uint32_t t_led = now;
    uint32_t t_lcd = now;

    for (;;)
    {
        now = timebase_ms();

        wdt_kick();                 /* pet the dog: loop is alive */
        encoder_task();             /* cheap, self-rate-limiting */
        telemetry_task();           /* ~10 Hz JSON over UART, non-blocking */

        if ((now - t_led) >= LED_PERIOD_MS)
        {
            t_led += LED_PERIOD_MS;
            LED0_Toggle();
        }

        if ((now - t_lcd) >= LCD_PERIOD_MS)
        {
            t_lcd += LCD_PERIOD_MS;

            int32_t  cnt = encoder_get_count();
            int32_t  det = encoder_get_detents();
            int32_t  rev = encoder_get_revolutions();
            uint32_t r10 = encoder_get_rpm_x10();

            (void)snprintf(line, sizeof line, "C%ld D%ld R%ld",
                           (long)cnt, (long)det, (long)rev);
            lcd_write_line(0U, line);

            /* Clamp the whole part to 4 digits so the row always fits 16 cols
             * (the compiler can't prove the uint is small; a real sensor won't
             * reach 9999 RPM but the display must not silently truncate). */
            uint32_t rpm_whole = r10 / 10U;
            if (rpm_whole > 9999U)
            {
                rpm_whole = 9999U;
            }
            (void)snprintf(line, sizeof line, "rpm %lu.%lu %c",
                           (unsigned long)rpm_whole,
                           (unsigned long)(r10 % 10U),
                           dir_char(encoder_get_direction()));
            lcd_write_line(1U, line);
        }
    }
}
