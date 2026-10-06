/* wdt.c - see wdt.h.
 *
 * This CSP has no WDT PLIB: MCC configures the watchdog entirely through the
 * config fuses in initialization.c (WDT_ENABLE = ENABLED, WDT_PER = CYC2048),
 * so the WDT is already running by the time app code starts. There is nothing
 * to initialize in software; we only need to pet it.
 *
 * The kick writes the 0xA5 CLEAR key to the WDT_CLEAR register. We first wait
 * out any pending synchronization (the WDT runs on the ~1 kHz GCLK2, so a
 * clear takes a few WDT clock cycles to sync across the clock domain). Writing
 * CLEAR while SYNCBUSY is set is ignored by hardware and, worse, writing a
 * non-key value would reset the chip -- so the key and the sync wait matter.
 *
 * Clock: GCLK2 = OSCULP32K / 31 ~= 1.057 kHz; PER = 2048 cycles ~= 1.9 s.
 */
#include "services/wdt.h"
#include "definitions.h"

void wdt_init(void)
{
    /* Nothing to do: the fuse enables and clocks the WDT before main().
     * Kept so the app's init sequence reads uniformly and so a future CSP
     * that does expose a WDT_Enable() has a home for it. */
}

void wdt_kick(void)
{
    /* Skip if a previous clear is still synchronizing; it will complete on
     * its own. Petting again next loop pass (microseconds later) is fine. */
    if ((WDT_REGS->WDT_STATUS & WDT_STATUS_SYNCBUSY_Msk) != 0U)
    {
        return;
    }
    WDT_REGS->WDT_CLEAR = (uint8_t)WDT_CLEAR_CLEAR_KEY;
}
