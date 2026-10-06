/* delay.c - SysTick-based blocking delays. See delay.h. */
#include "services/delay.h"
#include "definitions.h"            /* CPU_CLOCK_FREQUENCY, CMSIS SysTick */

#define SYSTICK_MAX         0x00FFFFFFU
#define TICKS_PER_US        (CPU_CLOCK_FREQUENCY / 1000000U)   /* 48 */
#define MAX_CHUNK_US        100000U     /* 4.8 M ticks, well below 24-bit wrap */

void delay_init(void)
{
    SysTick->CTRL = 0U;
    SysTick->LOAD = SYSTICK_MAX;
    SysTick->VAL  = 0U;
    /* CPU clock, counter enabled, NO interrupt (TICKINT = 0) */
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
}

static void delay_ticks(uint32_t ticks)
{
    uint32_t start = SysTick->VAL;
    /* Down-counter: elapsed = start - now, modulo 2^24 */
    while (((start - SysTick->VAL) & SYSTICK_MAX) < ticks)
    {
    }
}

void delay_us(uint32_t us)
{
    while (us > MAX_CHUNK_US)
    {
        delay_ticks(MAX_CHUNK_US * TICKS_PER_US);
        us -= MAX_CHUNK_US;
    }
    delay_ticks(us * TICKS_PER_US);
}

void delay_ms(uint32_t ms)
{
    while (ms-- > 0U)
    {
        delay_ticks(1000U * TICKS_PER_US);
    }
}
