/* app.c - top-level application.
 *
 * Current stage: LCD bring-up. Prints a tick counter on the LCD and the
 * debug UART every 500 ms. This becomes the superloop state machine once
 * the TC3 timebase and the encoder are added.
 */
#include <stdio.h>
#include "definitions.h"
#include "app/app.h"
#include "services/delay.h"
#include "drivers/lcd_hd44780.h"

void app_run(void)
{
    char line[LCD_COLS + 1U];
    uint32_t n = 0U;

    delay_init();
    lcd_init();
    lcd_write_line(0U, "SAMD21 LCD test");
    printf("LCD init done\r\n");

    for (;;)
    {
        LED0_Toggle();
        printf("tick %lu\r\n", (unsigned long)n);

        (void)snprintf(line, sizeof line, "tick %lu", (unsigned long)n);
        lcd_write_line(1U, line);

        n++;
        delay_ms(500U);     /* temporary: replaced by TC3 timebase scheduling */
    }
}
