/* wdt.h - watchdog timer: reset the node if the superloop ever hangs.
 *
 * An unattended factory node must not sit silently dead if the firmware wedges
 * (a stuck peripheral wait, a runaway loop, a corrupted state). The WDT is a
 * hardware countdown that resets the chip unless the main loop keeps petting
 * it. Normal operation pets it every pass; a hang stops the petting and the
 * chip reboots into a known-good state within the timeout.
 *
 * Configured in MCC via the WDT config fuses (this CSP has no WDT PLIB):
 * WDT_ENABLE = ENABLED, WDT_PER = CYC2048, window mode off. The watchdog is
 * therefore already running before main(); wdt_init() is a no-op and wdt_kick()
 * pets it with a direct CLEAR-key write (see wdt.c). See CLAUDE.md for the
 * clock routing (GCLK2 = OSCULP32K / 31 ~= 1.06 kHz -> ~1.9 s timeout).
 *
 * The superloop runs in microseconds, so a ~1.9 s leash never trips in normal
 * operation but catches a real hang quickly. Note: all init (incl. lcd_init's
 * ~60 ms) must reach the first kick inside ~1.9 s -- it easily does.
 */
#ifndef WDT_H
#define WDT_H

/* No-op: the fuse already enabled the WDT before main(). Kept for a uniform
 * init sequence and in case a future CSP exposes a software enable. */
void wdt_init(void);

/* "Pet" / "kick" the dog: reset its countdown. Call every superloop pass.
 * Missing this for longer than the configured timeout resets the chip. */
void wdt_kick(void);

#endif /* WDT_H */
