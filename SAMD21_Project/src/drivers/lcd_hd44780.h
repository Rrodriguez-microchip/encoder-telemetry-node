/* lcd_hd44780.h - 16x2 HD44780-compatible LCD, 4-bit mode, write-only.
 *
 * Wiring: RS=PB08, E=PB23, D4=PA28, D5=PA05, D6=PA23, D7=PA03, RW tied to GND.
 * Because RW is grounded the busy flag cannot be read, so every command is
 * followed by a fixed worst-case delay (37 us typ -> 50 us; clear/home 2 ms).
 */
#ifndef LCD_HD44780_H
#define LCD_HD44780_H

#include <stdint.h>

#define LCD_COLS  16U
#define LCD_ROWS  2U

void lcd_init(void);                                 /* ~60 ms, blocking */
void lcd_clear(void);                                /* 2 ms, blocking */
void lcd_set_cursor(uint8_t col, uint8_t row);
void lcd_putc(char c);
void lcd_puts(const char *s);
/* Overwrite a whole row, space-padded to 16 chars. Preferred over
 * lcd_clear()+lcd_puts() for periodic updates: no flicker, no 2 ms wait. */
void lcd_write_line(uint8_t row, const char *s);

#endif /* LCD_HD44780_H */
