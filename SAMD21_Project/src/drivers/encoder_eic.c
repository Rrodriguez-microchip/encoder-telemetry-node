/* encoder_eic.c - interrupt-driven backend for the encoder API.
 *
 * Implements drivers/encoder.h on top of the pure encoder_core using the
 * SAM D21 EIC (one edge interrupt per channel):
 *   ENC_A = PB02 (EIC EXTINT2), ENC_B = PA04 (EIC EXTINT4), both edges.
 *   ENC_SW = PB03, GPIO input with pull-up, polled + debounced at 1 ms.
 * (Pin map: CLAUDE.md s5.) EIC runs at NVIC priority 1, above TC3's 2.
 *
 * This is the PRODUCTION path: an optical 600 PPR encoder (~16.8k edges/s at
 * 7 ft/s) must be handled by interrupts, not polling. It is the right backend
 * for a clean electrical signal with no contact bounce.
 *
 * NOT suitable for the mechanical KY-040 bench knob: contact bounce lasts
 * milliseconds, far longer than the 1 MHz EIC filter rejects, so the ISR can
 * read the pins mid-bounce and register illegal transitions (invalid count
 * climbs, counts are lost). Use encoder_poll.c for the knob instead. Only ONE
 * encoder backend may be in the build at a time (they define the same API).
 *
 * ISR discipline (CLAUDE.md s3): the EIC callbacks only read the two pins and
 * push a sample into the core -- no printf, no heavy work. The main loop reads
 * aggregates through a short PRIMASK critical section so a multi-word value
 * (count, rpm) can't be torn by an edge landing mid-read.
 */
#include "drivers/encoder.h"
#include "drivers/encoder_core.h"
#include "services/timebase.h"
#include "definitions.h"

#define KY040_COUNTS_PER_DETENT   4U
#define KY040_COUNTS_PER_REV      80U
#define KY040_STOP_TIMEOUT_US     1000000U   /* 1 s with no edge -> stopped */
#define KY040_DEBOUNCE_MS         20U

/* Button is active-low (pressed = pin reads 0). */
#define KY040_BTN_PRESSED()       (ENC_SW_Get() == 0U)

static encoder_core_t s_enc;

/* Both EIC lines land here: re-read both pins and let the core decide the
 * step. Reading both (rather than trusting which line fired) keeps the state
 * table honest even if two edges are close together. */
static void enc_edge_isr(uintptr_t context)
{
    (void)context;
    uint8_t a = (uint8_t)ENC_A_Get();
    uint8_t b = (uint8_t)ENC_B_Get();
    encoder_core_on_sample(&s_enc, a, b, timebase_us());
}

void encoder_init(void)
{
    static const encoder_core_cfg_t cfg =
    {
        .counts_per_detent = KY040_COUNTS_PER_DETENT,
        .counts_per_rev    = KY040_COUNTS_PER_REV,
        .stop_timeout_us   = KY040_STOP_TIMEOUT_US,
        .debounce_ms       = KY040_DEBOUNCE_MS,
    };
    encoder_core_init(&s_enc, &cfg);

    /* Seed with the resting pin levels so the first edge isn't a phantom. */
    encoder_core_seed(&s_enc, (uint8_t)ENC_A_Get(), (uint8_t)ENC_B_Get());

    EIC_CallbackRegister(EIC_PIN_2, enc_edge_isr, (uintptr_t)0);  /* ENC_A */
    EIC_CallbackRegister(EIC_PIN_4, enc_edge_isr, (uintptr_t)0);  /* ENC_B */
    EIC_InterruptEnable(EIC_PIN_2);
    EIC_InterruptEnable(EIC_PIN_4);
}

void encoder_task(void)
{
    /* Button: debounce at 1 ms. btn_count_ms advances one step per call, so
     * call this at ~1 ms cadence. The superloop runs far faster, so gate on
     * the ms tick to keep the debounce window in real milliseconds. */
    static uint32_t t_btn_ms;
    uint32_t now_ms = timebase_ms();
    while ((now_ms - t_btn_ms) != 0U)   /* catch up one tick at a time */
    {
        t_btn_ms++;
        encoder_core_button_tick(&s_enc, KY040_BTN_PRESSED());
    }

    encoder_core_update_rpm(&s_enc, timebase_us());
}

/* ---- Snapshot reads ---------------------------------------------------
 * Mask interrupts briefly so an EIC edge can't change count/direction
 * mid-read. Each is a single 32-bit load so tearing is only a concern across
 * the compound reads (detents/revs call count internally); we keep the mask
 * tight. */

int32_t encoder_get_count(void)
{
    uint32_t pm = __get_PRIMASK();
    __disable_irq();
    int32_t v = s_enc.count;
    __set_PRIMASK(pm);
    return v;
}

int32_t encoder_get_detents(void)
{
    uint32_t pm = __get_PRIMASK();
    __disable_irq();
    int32_t v = encoder_core_detents(&s_enc);
    __set_PRIMASK(pm);
    return v;
}

int32_t encoder_get_revolutions(void)
{
    uint32_t pm = __get_PRIMASK();
    __disable_irq();
    int32_t v = encoder_core_revolutions(&s_enc);
    __set_PRIMASK(pm);
    return v;
}

uint32_t encoder_get_rpm_x10(void)      { return s_enc.rpm_x10; }    /* 32-bit atomic */
int8_t   encoder_get_direction(void)    { return s_enc.direction; }
bool     encoder_get_button(void)       { return s_enc.btn_stable; }
uint32_t encoder_get_button_events(void){ return s_enc.btn_events; }
uint32_t encoder_get_invalid_count(void){ return s_enc.invalid; }
