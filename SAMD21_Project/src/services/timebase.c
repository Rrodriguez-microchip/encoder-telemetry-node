/* timebase.c - see timebase.h
 *
 * TC3 (MCC): 16-bit, GCLK0 48 MHz, prescaler 1, MPWM with CC0 = 47999,
 * so the counter runs 0..47999 and overflows exactly every 1 ms.
 * The overflow ISR only increments s_ms. Each count is 1/48 us.
 */
#include "services/timebase.h"
#include "definitions.h"
#include "mqtt_interface.h"     /* MilliTimer_Handler: the MQTT lib's 1 ms tick */

#define TC3_COUNTS_PER_US   (CPU_CLOCK_FREQUENCY / 1000000U)   /* 48 */

static volatile uint32_t s_ms;

/* Runs in TC3 interrupt context (priority 2). Keep it this short. */
static void timebase_tick_isr(TC_TIMER_STATUS status, uintptr_t context)
{
    (void)status;
    (void)context;
    s_ms++;
    MilliTimer_Handler();       /* drives the MQTT client's countdown timers */
}

void timebase_init(void)
{
    s_ms = 0U;
    TC3_TimerCallbackRegister(timebase_tick_isr, (uintptr_t)0);
    TC3_TimerStart();
}

uint32_t timebase_ms(void)
{
    /* 32-bit aligned load is atomic on Cortex-M0+, no lock needed. */
    return s_ms;
}

uint32_t timebase_us(void)
{
    /* Race to handle: TC3 can overflow between reading s_ms and reading the
     * counter, and the ISR may not have run yet (interrupts masked, or we
     * are inside a higher-priority ISR such as the EIC). That would make
     * time jump backwards by up to 1 ms.
     *
     * Fix: freeze s_ms by masking interrupts, then check the OVF flag.
     * If an overflow is pending, the ISR hasn't counted it yet: re-read the
     * counter (now guaranteed post-wrap) and add the missing millisecond. */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    uint32_t ms  = s_ms;
    uint32_t cnt = TC3_Timer16bitCounterGet();
    if ((TC3_REGS->COUNT16.TC_INTFLAG & TC_INTFLAG_OVF_Msk) != 0U)
    {
        cnt = TC3_Timer16bitCounterGet();
        ms++;
    }

    __set_PRIMASK(primask);

    /* ms * 1000 wraps mod 2^32 consistently, so differences stay valid. */
    return (ms * 1000U) + (cnt / TC3_COUNTS_PER_US);
}
