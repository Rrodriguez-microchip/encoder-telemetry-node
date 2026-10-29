/* lcd_hd44780.c - see lcd_hd44780.h
 *
 * Transport: Adafruit #292 backpack = MCP23008 I2C expander, pins wired
 * (same as Adafruit's own library):
 *     GP1 = RS   GP2 = E   GP3..GP6 = D4..D7   GP7 = backlight (1 = on)
 *
 * One HD44780 nibble = three expander GPIO writes: data, data|E, data.
 * Data is set up a byte before E rises and held a byte after it falls; the
 * MCP23008 updates its outputs at each byte's ACK, so at 400 kHz every step
 * is ~22 us apart -- far beyond the HD44780's ns-level setup/E-width needs.
 *
 * Why IOCON.SEQOP = 1: by default the MCP23008 auto-increments its register
 * pointer, so a multi-byte write starting at GPIO (0x09) would spill into
 * OLAT (0x0A) and then wrap to IODIR (0x00) -- turning our outputs into
 * inputs mid-character. With SEQOP set, every data byte of a transfer lands
 * on GPIO, so a whole character (both nibbles, all E edges) is ONE I2C write.
 *
 * No busy flag (RW grounded): an LCD instruction needs 37 us after its last
 * E edge. The I2C framing gives that for free -- the next transfer can't
 * raise E until START + address + register + setup byte have gone out
 * (>= ~90 us at 400 kHz, more at 100 kHz). Only CLEAR/HOME need 1.5 ms, and
 * those are issued only during the blocking lcd_init().
 */
#include <stdbool.h>
#include <string.h>
#include "drivers/lcd_hd44780.h"
#include "services/delay.h"
#include "services/timebase.h"
#include "services/log.h"
#include "definitions.h"            /* SERCOM4_I2C_* from MCC */

/* MCP23008 */
#define MCP_ADDR            0x20U   /* A2..A0 jumpers open */
#define MCP_IODIR           0x00U
#define MCP_IOCON           0x05U
#define MCP_GPIO            0x09U
#define MCP_IOCON_SEQOP     0x20U   /* 1 = no address auto-increment */

#define PIN_RS              0x02U   /* GP1 */
#define PIN_E               0x04U   /* GP2 */
#define DATA_SHIFT          3U      /* D4..D7 on GP3..GP6 */
#define PIN_BL              0x80U   /* GP7 */

/* HD44780 commands */
#define CMD_CLEAR           0x01U
#define CMD_ENTRY_INC       0x06U   /* cursor moves right, no display shift */
#define CMD_DISPLAY_OFF     0x08U
#define CMD_DISPLAY_ON      0x0CU   /* display on, cursor off, blink off */
#define CMD_FUNC_4BIT_2LINE 0x28U   /* 4-bit bus, 2 lines, 5x8 font */
#define CMD_SET_DDRAM       0x80U

#define ROW1_DDRAM_OFFSET   0x40U

/* A 7-byte transfer is ~180 us at 400 kHz; anything past this is a stuck bus
 * (SDA held low, no pull-ups) -- abort rather than wait forever. */
#define XFER_TIMEOUT_US     5000U
/* After a failed runtime transfer, hold off before trying again so an
 * unplugged display doesn't flood the bus/log every loop pass. */
#define RETRY_HOLDOFF_MS    1000U

static bool     s_present;              /* backpack ACKed at init */
static bool     s_in_flight;            /* async transfer owns s_tx */
static uint32_t s_xfer_start_us;
static uint32_t s_holdoff_start_ms;
static bool     s_holdoff;

/* What we want on screen vs. what the LCD is believed to show. A mismatch
 * is sent by lcd_task(). s_shown uses 0 as "unknown" (never a real char we
 * write, since lcd_write_line pads with spaces), forcing a resend. */
static char     s_want[LCD_ROWS][LCD_COLS];
static char     s_shown[LCD_ROWS][LCD_COLS];
static uint8_t  s_cur_row, s_cur_col;   /* LCD's DDRAM cursor (auto-incs) */
static bool     s_cur_valid;

/* Pending update, committed to s_shown/cursor only once the transfer ACKs */
static bool     s_pend_is_char;
static uint8_t  s_pend_row, s_pend_col;

/* Must stay valid while the interrupt-driven PLIB transfer is running */
static uint8_t  s_tx[7];

/* ---- byte packing -------------------------------------------------------- */

/* Fill 3 GPIO bytes for one nibble: setup, E high, E low (falling edge latches) */
static uint8_t *pack_nibble(uint8_t *p, uint8_t nibble, bool rs)
{
    uint8_t v = (uint8_t)(((nibble & 0x0FU) << DATA_SHIFT) | PIN_BL | (rs ? PIN_RS : 0U));
    *p++ = v;
    *p++ = (uint8_t)(v | PIN_E);
    *p++ = v;
    return p;
}

/* Build a full byte (both nibbles) into s_tx; returns transfer length (7) */
static uint32_t pack_byte(uint8_t value, bool rs)
{
    uint8_t *p = s_tx;
    *p++ = MCP_GPIO;
    p = pack_nibble(p, (uint8_t)(value >> 4), rs);
    p = pack_nibble(p, value, rs);
    return (uint32_t)(p - s_tx);
}

/* ---- blocking path (init only) ------------------------------------------- */

static bool xfer_blocking(uint32_t len)
{
    if (!SERCOM4_I2C_Write(MCP_ADDR, s_tx, len))
    {
        return false;
    }
    uint32_t t0 = timebase_us();
    while (SERCOM4_I2C_IsBusy())
    {
        if ((timebase_us() - t0) >= XFER_TIMEOUT_US)
        {
            SERCOM4_I2C_TransferAbort();
            return false;
        }
    }
    return SERCOM4_I2C_ErrorGet() == SERCOM_I2C_ERROR_NONE;
}

static bool mcp_write_reg(uint8_t reg, uint8_t value)
{
    s_tx[0] = reg;
    s_tx[1] = value;
    return xfer_blocking(2U);
}

static bool init_nibble(uint8_t nibble)
{
    s_tx[0] = MCP_GPIO;
    (void)pack_nibble(&s_tx[1], nibble, false);
    return xfer_blocking(4U);
}

static bool init_command(uint8_t cmd)
{
    bool ok = xfer_blocking(pack_byte(cmd, false));
    delay_us(50U);                          /* 37 us typ */
    if (cmd == CMD_CLEAR)
    {
        delay_ms(2U);                       /* 1.52 ms typ */
    }
    return ok;
}

void lcd_init(void)
{
    s_present   = false;
    s_in_flight = false;
    s_holdoff   = false;
    s_cur_valid = false;
    (void)memset(s_want,  ' ', sizeof s_want);
    (void)memset(s_shown, 0,   sizeof s_shown);

    /* Power-on: wait > 40 ms after Vcc reaches 4.5 V (backpack boost included) */
    delay_ms(50U);

    /* First transaction doubles as the presence probe. */
    if (!mcp_write_reg(MCP_IOCON, MCP_IOCON_SEQOP))
    {
        log_line("lcd: I2C backpack not found at 0x20 -- display disabled");
        return;
    }

    bool ok = true;
    ok = ok && mcp_write_reg(MCP_IODIR, 0x00U);      /* GP0..GP7 outputs */
    ok = ok && mcp_write_reg(MCP_GPIO, PIN_BL);      /* backlight on, E/RS low */

    /* "Initialization by instruction" (HD44780 datasheet fig. 24).
     * Works whether the controller woke up in 8-bit or is half-way
     * through a 4-bit transfer (e.g. after an MCU-only reset). */
    ok = ok && init_nibble(0x03U);
    delay_ms(5U);                                    /* > 4.1 ms */
    ok = ok && init_nibble(0x03U);
    delay_us(150U);                                  /* > 100 us */
    ok = ok && init_nibble(0x03U);
    delay_us(150U);
    ok = ok && init_nibble(0x02U);                   /* switch to 4-bit */
    delay_us(150U);

    ok = ok && init_command(CMD_FUNC_4BIT_2LINE);
    ok = ok && init_command(CMD_DISPLAY_OFF);
    ok = ok && init_command(CMD_CLEAR);
    ok = ok && init_command(CMD_ENTRY_INC);
    ok = ok && init_command(CMD_DISPLAY_ON);

    if (!ok)
    {
        log_line("lcd: I2C error during init -- display disabled");
        return;
    }

    /* CLEAR left the panel blank with the cursor at (0,0). */
    (void)memset(s_shown, ' ', sizeof s_shown);
    s_cur_row   = 0U;
    s_cur_col   = 0U;
    s_cur_valid = true;
    s_present   = true;
}

/* ---- non-blocking path (superloop) ---------------------------------------- */

/* Called when the in-flight transfer has finished (ok or not). */
static void finish_transfer(bool ok)
{
    s_in_flight = false;
    if (!ok)
    {
        /* Unknown what the LCD latched: resend everything, re-seek cursor. */
        (void)memset(s_shown, 0, sizeof s_shown);
        s_cur_valid        = false;
        s_holdoff          = true;
        s_holdoff_start_ms = timebase_ms();
        return;
    }
    if (s_pend_is_char)
    {
        s_shown[s_pend_row][s_pend_col] = s_want[s_pend_row][s_pend_col];
        s_cur_col = (uint8_t)(s_pend_col + 1U);  /* DDRAM auto-increment */
    }
    else
    {
        s_cur_col = s_pend_col;
    }
    s_cur_row   = s_pend_row;
    s_cur_valid = (s_cur_col < LCD_COLS);        /* past col 15 = not row-wrap */
}

void lcd_task(void)
{
    if (!s_present)
    {
        return;
    }

    if (s_in_flight)
    {
        if (SERCOM4_I2C_IsBusy())
        {
            if ((timebase_us() - s_xfer_start_us) >= XFER_TIMEOUT_US)
            {
                SERCOM4_I2C_TransferAbort();
                finish_transfer(false);
            }
            return;
        }
        finish_transfer(SERCOM4_I2C_ErrorGet() == SERCOM_I2C_ERROR_NONE);
        return;                 /* one transfer per pass keeps the loop short */
    }

    if (s_holdoff)
    {
        if ((timebase_ms() - s_holdoff_start_ms) < RETRY_HOLDOFF_MS)
        {
            return;
        }
        s_holdoff = false;
    }

    /* Next character that differs from the screen, row-major. */
    for (uint8_t row = 0U; row < LCD_ROWS; row++)
    {
        for (uint8_t col = 0U; col < LCD_COLS; col++)
        {
            if (s_want[row][col] == s_shown[row][col])
            {
                continue;
            }

            uint32_t len;
            s_pend_row = row;
            s_pend_col = col;
            if (s_cur_valid && (s_cur_row == row) && (s_cur_col == col))
            {
                s_pend_is_char = true;
                len = pack_byte((uint8_t)s_want[row][col], true);
            }
            else
            {
                /* Seek first; the character goes out on a later pass. */
                uint8_t addr = (row == 0U) ? col : (uint8_t)(ROW1_DDRAM_OFFSET + col);
                s_pend_is_char = false;
                len = pack_byte((uint8_t)(CMD_SET_DDRAM | addr), false);
            }

            if (SERCOM4_I2C_Write(MCP_ADDR, s_tx, len))
            {
                s_in_flight     = true;
                s_xfer_start_us = timebase_us();
            }
            return;
        }
    }
}

void lcd_clear(void)
{
    (void)memset(s_want, ' ', sizeof s_want);
}

void lcd_write_line(uint8_t row, const char *s)
{
    if (row >= LCD_ROWS)
    {
        return;
    }
    uint8_t i = 0U;
    while ((i < LCD_COLS) && (s[i] != '\0'))
    {
        s_want[row][i] = s[i];
        i++;
    }
    while (i < LCD_COLS)
    {
        s_want[row][i] = ' ';
        i++;
    }
}
