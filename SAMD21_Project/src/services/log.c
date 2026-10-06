/* log.c - see log.h. */
#include "services/log.h"
#include "definitions.h"

/* Per-byte spin budget. If the transmitter isn't ready within this many
 * polls we give up on the line rather than block the loop. At 115200 baud one
 * byte clears in ~87 us; a few thousand polls is far longer than that yet
 * still a bounded, tiny stall. */
#define LOG_SPIN_BUDGET   4000U

static uint32_t s_dropped;

/* Write one byte if the TX becomes ready within the budget. Returns false if
 * it timed out (caller should abandon the rest of the line). */
static bool put_byte(char c)
{
    uint32_t spins = 0U;
    while (!SERCOM5_USART_TransmitterIsReady())
    {
        if (++spins >= LOG_SPIN_BUDGET)
        {
            return false;
        }
    }
    /* TX is ready (DRE set), so WriteByte won't block. */
    SERCOM5_USART_WriteByte((int)(uint8_t)c);
    return true;
}

void log_line(const char *s)
{
    while (*s != '\0')
    {
        if (!put_byte(*s))
        {
            s_dropped++;
            return;                 /* abandon the line; don't hang the loop */
        }
        s++;
    }
    if (!put_byte('\r') || !put_byte('\n'))
    {
        s_dropped++;
    }
}

uint32_t log_dropped(void)
{
    return s_dropped;
}
