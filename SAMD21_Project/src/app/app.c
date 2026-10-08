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
#include "drivers/w5500_port.h"
#include "services/telemetry.h"
#include "services/net_mqtt.h"
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

    /* P3 Ethernet bring-up (milestone 1): reset the W5500 and read VERSIONR.
     * A healthy chip always returns 0x04 -- that single byte proves the SPI
     * wiring, clock and chip are good. Then load the static IP so the Pi can
     * ping us (milestone 2). One-shot at startup; no superloop task yet. */
    if (w5500_port_init())
    {
        uint8_t ver = w5500_version();
        printf("W5500 VERSIONR = 0x%02X (expect 0x04)\r\n", ver);
        if (ver == 0x04U)
        {
            printf("W5500 net up: %s\r\n", w5500_net_up() ? "ok" : "FAILED");

            /* P4: MQTT connect is NOT done here. A blocking connect in init
             * runs before the superloop, so the WDT (~1.9 s) isn't petted --
             * an unreachable broker would reset the node in a loop. Instead
             * net_mqtt_task() connects lazily from inside the loop and retries
             * forever, so the node tolerates a broker that's down at boot or
             * that comes and goes. */
        }
    }
    else
    {
        printf("W5500 init FAILED\r\n");
    }

    uint32_t now   = timebase_ms();
    uint32_t t_led = now;
    uint32_t t_lcd = now;

    for (;;)
    {
        now = timebase_ms();

        wdt_kick();                 /* pet the dog: loop is alive */
        encoder_task();             /* cheap, self-rate-limiting */
        telemetry_task();           /* ~10 Hz JSON over UART, non-blocking */
        net_mqtt_task();            /* ~1 Hz JSON over MQTT (no-op if down) */

        if ((now - t_led) >= LED_PERIOD_MS)
        {
            t_led += LED_PERIOD_MS;
            LED0_Toggle();
        }

        if ((now - t_lcd) >= LCD_PERIOD_MS)
        {
            t_lcd += LCD_PERIOD_MS;

            /* Operator-facing linear-material view (count/detents are dev-only
             * and no longer shown). Row 0: speed ft/s + direction. Row 1:
             * total feet extruded. Both come from telemetry's shared material
             * math so the LCD and the JSON payload never disagree. */
            uint32_t sp_milli  = telemetry_speed_milli_ft_s();   /* milli-ft/s */
            int32_t  tot_milli = telemetry_total_milli_ft();     /* milli-ft   */

            /* Speed: "<whole>.<2 frac> ft/s <dir>". Thousandths -> hundredths
             * for the display (plenty of resolution on a 16-col row). */
            (void)snprintf(line, sizeof line, "%lu.%02lu ft/s %c",
                           (unsigned long)(sp_milli / 1000U),
                           (unsigned long)((sp_milli % 1000U) / 10U),
                           dir_char(encoder_get_direction()));
            lcd_write_line(0U, line);

            /* Total: "Tot <whole>.<2 frac> ft". Signed (a reversed shaft can
             * net negative); print the sign and work in magnitude so the
             * fractional split is correct for negatives too. */
            long     tot_whole = (long)(tot_milli / 1000);
            uint32_t tot_frac  = (uint32_t)((tot_milli < 0 ? -tot_milli : tot_milli)
                                            % 1000) / 10U;
            (void)snprintf(line, sizeof line, "Tot %ld.%02lu ft",
                           tot_whole, (unsigned long)tot_frac);
            lcd_write_line(1U, line);
        }
    }
}
