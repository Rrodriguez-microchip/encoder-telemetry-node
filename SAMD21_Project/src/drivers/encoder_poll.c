/* encoder_poll.c - 1 ms polled backend for the encoder API.
 *
 * Implements drivers/encoder.h on top of the pure encoder_core by SAMPLING
 * A/B on a fixed 1 ms cadence instead of taking edge interrupts. This is the
 * right backend for the mechanical KY-040 bench knob:
 *
 *   Contact bounce on a mechanical encoder lasts milliseconds. Sampling once
 *   per ms (rather than taking edge interrupts) lets most chatter settle
 *   between reads. Whatever skips still slip through -- a state read mid-
 *   bounce, or two states crossed within one 1 ms sample -- are recovered by
 *   the core's invalid-step handling (it applies +/-2 in the last direction
 *   instead of dropping the motion), so the count tracks position without
 *   drift. See encoder_core_on_sample().
 *
 * This backend is NOT for the production optical encoder (~16.8k edges/s):
 * that needs the interrupt path in encoder_eic.c. Only ONE backend may be in
 * the build.
 *
 * Pins (CLAUDE.md s5): ENC_A=PB02, ENC_B=PA04, ENC_SW=PB03 (active-low).
 * The EIC can stay configured in MCC; we simply never enable its interrupts
 * here, so the pins read fine as plain inputs.
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
    encoder_core_seed(&s_enc, (uint8_t)ENC_A_Get(), (uint8_t)ENC_B_Get());
    /* No EIC interrupts: we poll in encoder_task(). */
}

void encoder_task(void)
{
    /* Advance one 1 ms step at a time so a late loop pass catches up without
     * collapsing the debounce window. On each tick: sample A/B into the core
     * (quadrature) and tick the button debouncer. */
    static uint32_t t_ms;
    uint32_t now_ms = timebase_ms();

    while ((now_ms - t_ms) != 0U)
    {
        t_ms++;
        encoder_core_on_sample(&s_enc,
                               (uint8_t)ENC_A_Get(),
                               (uint8_t)ENC_B_Get(),
                               timebase_us());
        encoder_core_button_tick(&s_enc, KY040_BTN_PRESSED());
    }

    encoder_core_update_rpm(&s_enc, timebase_us());
}

/* ---- Snapshot reads ---------------------------------------------------
 * encoder_task() mutates s_enc from the main-loop context, so these run in
 * the same context and need no masking. We keep the same signatures as the
 * EIC backend; the compound reads (detents/revs) are consistent because
 * nothing preempts them here. */

int32_t  encoder_get_count(void)        { return s_enc.count; }
int32_t  encoder_get_detents(void)      { return encoder_core_detents(&s_enc); }
int32_t  encoder_get_revolutions(void)  { return encoder_core_revolutions(&s_enc); }
uint32_t encoder_get_rpm_x10(void)      { return s_enc.rpm_x10; }
int8_t   encoder_get_direction(void)    { return s_enc.direction; }
bool     encoder_get_button(void)       { return s_enc.btn_stable; }
uint32_t encoder_get_button_events(void){ return s_enc.btn_events; }
uint32_t encoder_get_invalid_count(void){ return s_enc.invalid; }
