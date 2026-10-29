/* lcd_hd44780.h - 16x2 HD44780 LCD behind an Adafruit #292 I2C backpack.
 *
 * Wiring: one STEMMA QT cable, SDA=PA12 / SCL=PA13 (SERCOM4 I2C master,
 * 400 kHz), backpack VIN = 3.3 V (its boost makes the LCD's 5 V; powering it
 * from 3.3 V keeps the backpack's 10k SDA/SCL pull-ups at 3.3 V -- the SAMD21
 * is not 5 V tolerant). Address jumpers open -> MCP23008 at 0x20.
 *
 * The backpack is an MCP23008 I/O expander wired to the LCD in 4-bit mode
 * (RW grounded, so no busy flag). Every LCD nibble is an I2C write to the
 * expander's GPIO register, so the LCD is now slow-ish to talk to: a whole
 * 32-char refresh is ~5 ms of bus time. Blocking for that would starve the
 * 1 ms encoder poll in the superloop, so updates are buffered:
 *
 *   lcd_write_line() only copies text into a 2x16 shadow buffer (instant).
 *   lcd_task()       sends at most ONE short I2C transfer per loop pass --
 *                    the next character that differs from what the LCD
 *                    shows -- and returns immediately.
 *
 * If the backpack doesn't ACK at init, the driver disables itself (one log
 * line) and the node runs on without a display.
 */
#ifndef LCD_HD44780_H
#define LCD_HD44780_H

#include <stdint.h>

#define LCD_COLS  16U
#define LCD_ROWS  2U

void lcd_init(void);                  /* ~60 ms, blocking; call before the loop */
void lcd_task(void);                  /* call every superloop pass */
void lcd_clear(void);                 /* blank both rows (buffered) */
/* Overwrite a whole row, space-padded to 16 chars (buffered; only changed
 * characters are actually sent, so no flicker). */
void lcd_write_line(uint8_t row, const char *s);

#endif /* LCD_HD44780_H */
