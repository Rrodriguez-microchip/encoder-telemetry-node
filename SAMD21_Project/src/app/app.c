/* app.c - top-level application.
 *
 * Bare-metal superloop: no task ever waits, it only checks the clock.
 * Each task owns a "last run" timestamp; the shared timebase never resets.
 *
 * Current stage: P1 encoder bench readout (KY-040 hand knob).
 *   - LED toggles every 500 ms (alive indicator)
 *   - encoder_task() runs every pass (debounce + RPM)
 *   - LCD row 0: count / detents / revs   row 1: rpm / dir / button events
 *   - UART: one dump line at ~10 Hz with every value, for the acceptance tests
 */
#include <stdio.h>
#include "definitions.h"
#include "app/app.h"
#include "services/delay.h"
#include "services/timebase.h"
#include "drivers/lcd_hd44780.h"
#include "drivers/encoder.h"

#define LED_PERIOD_MS       500U
#define LCD_PERIOD_MS       100U     /* readable, no flicker (overwrite+pad) */
#define UART_PERIOD_MS      100U     /* ~10 Hz, same cadence as future JSON */

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
    printf("\r\nP1 encoder bench\r\n");

    uint32_t now   = timebase_ms();
    uint32_t t_led = now;
    uint32_t t_lcd = now;
    uint32_t t_uart = now;

    for (;;)
    {
        now = timebase_ms();

        encoder_task();             /* cheap, self-rate-limiting */

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

            (void)snprintf(line, sizeof line, "rpm%lu.%lu %c ev%lu",
                           (unsigned long)(r10 / 10U),
                           (unsigned long)(r10 % 10U),
                           dir_char(encoder_get_direction()),
                           (unsigned long)encoder_get_button_events());
            lcd_write_line(1U, line);
        }

        if ((now - t_uart) >= UART_PERIOD_MS)
        {
            t_uart += UART_PERIOD_MS;

            uint32_t r10 = encoder_get_rpm_x10();
            printf("count=%ld det=%ld rev=%ld dir=%d rpm=%lu.%lu "
                   "btn=%d ev=%lu inval=%lu\r\n",
                   (long)encoder_get_count(),
                   (long)encoder_get_detents(),
                   (long)encoder_get_revolutions(),
                   (int)encoder_get_direction(),
                   (unsigned long)(r10 / 10U),
                   (unsigned long)(r10 % 10U),
                   (int)encoder_get_button(),
                   (unsigned long)encoder_get_button_events(),
                   (unsigned long)encoder_get_invalid_count());
        }
    }
}
