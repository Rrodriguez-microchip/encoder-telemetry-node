/* app.c - top-level application.
 *
 * Bare-metal superloop: no task ever waits, it only checks the clock.
 * Each task owns a "last run" timestamp; the shared timebase never resets.
 *
 * Current stage: timebase bring-up.
 *   - LED toggles every 500 ms
 *   - every 500 ms: tick counter on LCD row 1 + UART line with ms and the
 *     measured us interval (should read ~500000, a check of timebase_us())
 *   - LCD row 0: uptime hh:mm:ss, compare against a stopwatch
 */
#include <stdio.h>
#include "definitions.h"
#include "app/app.h"
#include "services/delay.h"
#include "services/timebase.h"
#include "drivers/lcd_hd44780.h"

#define LED_PERIOD_MS       500U
#define TICK_PERIOD_MS      500U
#define UPTIME_PERIOD_MS    1000U

void app_run(void)
{
    char line[LCD_COLS + 1U];
    uint32_t n = 0U;

    delay_init();
    timebase_init();
    lcd_init();
    printf("\r\nTimebase test\r\n");

    uint32_t now      = timebase_ms();
    uint32_t t_led    = now;
    uint32_t t_tick   = now;
    uint32_t t_uptime = now - UPTIME_PERIOD_MS;   /* draw row 0 immediately */
    uint32_t us_prev  = timebase_us();

    for (;;)
    {
        now = timebase_ms();

        /* += keeps the schedule anchored: a late run doesn't push later ones. */
        if ((now - t_led) >= LED_PERIOD_MS)
        {
            t_led += LED_PERIOD_MS;
            LED0_Toggle();
        }

        if ((now - t_tick) >= TICK_PERIOD_MS)
        {
            t_tick += TICK_PERIOD_MS;

            uint32_t us_now = timebase_us();
            printf("tick %lu  ms=%lu  dus=%lu\r\n",
                   (unsigned long)n, (unsigned long)now,
                   (unsigned long)(us_now - us_prev));
            us_prev = us_now;

            (void)snprintf(line, sizeof line, "tick %lu", (unsigned long)n);
            lcd_write_line(1U, line);
            n++;
        }

        if ((now - t_uptime) >= UPTIME_PERIOD_MS)
        {
            t_uptime += UPTIME_PERIOD_MS;

            uint32_t s = now / 1000U;
            (void)snprintf(line, sizeof line, "up %02lu:%02lu:%02lu",
                           (unsigned long)(s / 3600U),
                           (unsigned long)((s / 60U) % 60U),
                           (unsigned long)(s % 60U));
            lcd_write_line(0U, line);
        }

        /* future: encoder_task(); telemetry_task(); -- run every pass */
    }
}
