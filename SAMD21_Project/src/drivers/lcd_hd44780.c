/* lcd_hd44780.c - see lcd_hd44780.h */
#include <stdbool.h>
#include "drivers/lcd_hd44780.h"
#include "services/delay.h"
#include "definitions.h"            /* LCD_xx_Set()/Clear() from MCC pin names */

/* HD44780 commands */
#define CMD_CLEAR           0x01U
#define CMD_HOME            0x02U
#define CMD_ENTRY_INC       0x06U   /* cursor moves right, no display shift */
#define CMD_DISPLAY_OFF     0x08U
#define CMD_DISPLAY_ON      0x0CU   /* display on, cursor off, blink off */
#define CMD_FUNC_4BIT_2LINE 0x28U   /* 4-bit bus, 2 lines, 5x8 font */
#define CMD_SET_DDRAM       0x80U

#define ROW1_DDRAM_OFFSET   0x40U

/* Put a nibble on D7..D4 and strobe E.
 * Timing (HD44780 @ 5 V): address/data setup >= 80 ns, E high >= 450 ns,
 * E cycle >= 1000 ns. 1 us steps give comfortable margin. */
static void lcd_write_nibble(uint8_t nibble)
{
    if ((nibble & 0x01U) != 0U) { LCD_D4_Set(); } else { LCD_D4_Clear(); }
    if ((nibble & 0x02U) != 0U) { LCD_D5_Set(); } else { LCD_D5_Clear(); }
    if ((nibble & 0x04U) != 0U) { LCD_D6_Set(); } else { LCD_D6_Clear(); }
    if ((nibble & 0x08U) != 0U) { LCD_D7_Set(); } else { LCD_D7_Clear(); }

    delay_us(1U);
    LCD_E_Set();
    delay_us(1U);
    LCD_E_Clear();          /* data latched on falling edge */
    delay_us(1U);
}

static void lcd_write_byte(uint8_t value, bool is_data)
{
    if (is_data) { LCD_RS_Set(); } else { LCD_RS_Clear(); }
    lcd_write_nibble((uint8_t)(value >> 4));
    lcd_write_nibble((uint8_t)(value & 0x0FU));
    delay_us(50U);          /* 37 us typ execution time, no busy flag */
}

static void lcd_command(uint8_t cmd)
{
    lcd_write_byte(cmd, false);
    if ((cmd == CMD_CLEAR) || (cmd == CMD_HOME))
    {
        delay_ms(2U);       /* 1.52 ms typ */
    }
}

void lcd_init(void)
{
    LCD_E_Clear();
    LCD_RS_Clear();

    /* Power-on: wait > 40 ms after Vcc reaches 4.5 V */
    delay_ms(50U);

    /* "Initialization by instruction" (HD44780 datasheet fig. 24).
     * Works whether the controller woke up in 8-bit or is half-way
     * through a 4-bit transfer (e.g. after an MCU-only reset). */
    lcd_write_nibble(0x03U);
    delay_ms(5U);           /* > 4.1 ms */
    lcd_write_nibble(0x03U);
    delay_us(150U);         /* > 100 us */
    lcd_write_nibble(0x03U);
    delay_us(150U);
    lcd_write_nibble(0x02U);  /* switch to 4-bit */
    delay_us(150U);

    lcd_command(CMD_FUNC_4BIT_2LINE);
    lcd_command(CMD_DISPLAY_OFF);
    lcd_command(CMD_CLEAR);
    lcd_command(CMD_ENTRY_INC);
    lcd_command(CMD_DISPLAY_ON);
}

void lcd_clear(void)
{
    lcd_command(CMD_CLEAR);
}

void lcd_set_cursor(uint8_t col, uint8_t row)
{
    if (col >= LCD_COLS) { col = LCD_COLS - 1U; }
    uint8_t addr = (row == 0U) ? col : (uint8_t)(ROW1_DDRAM_OFFSET + col);
    lcd_command((uint8_t)(CMD_SET_DDRAM | addr));
}

void lcd_putc(char c)
{
    lcd_write_byte((uint8_t)c, true);
}

void lcd_puts(const char *s)
{
    while (*s != '\0')
    {
        lcd_putc(*s++);
    }
}

void lcd_write_line(uint8_t row, const char *s)
{
    uint8_t i = 0U;
    lcd_set_cursor(0U, row);
    while ((i < LCD_COLS) && (s[i] != '\0'))
    {
        lcd_putc(s[i]);
        i++;
    }
    while (i < LCD_COLS)
    {
        lcd_putc(' ');
        i++;
    }
}
