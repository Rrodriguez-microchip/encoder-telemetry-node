/* encoder_core.c - see encoder_core.h.
 *
 * Pure logic, zero hardware. Compiles on the host (gcc + Unity) so the
 * quadrature table, floor division, RPM math and button debounce can be
 * tested without a board (CLAUDE.md s3, s7 acceptance tests).
 */
#include "drivers/encoder_core.h"

/* ---- Quadrature state table -------------------------------------------
 *
 * State = (A<<1 | B). A valid step changes exactly one of A/B, giving a
 * Gray-code sequence. We index a 16-entry table by (prev<<2 | curr):
 *
 *   +1  for a forward step        00->01->11->10->00
 *   -1  for a reverse step        00->10->11->01->00
 *    0  when nothing changed      (prev == curr)
 *    2  = INVALID: both bits flipped at once (00<->11, 01<->10). Can't happen
 *        on a clean quadrature signal in one sample; means a missed edge or
 *        bounce. We count it and emit 0 (hold position) rather than guess.
 *
 * Full 4x decoding: 4 counts per quadrature cycle, so 4 counts/detent on the
 * KY-040 (which has one full cycle per detent).
 */
#define INV ENCODER_CORE_INVALID_STEP   /* sentinel, != +/-1/0; see header */

static const int8_t QUAD_TABLE[16] = {
    /* prev=00 */  0,  +1,  -1, INV,
    /* prev=01 */ -1,   0, INV,  +1,
    /* prev=10 */ +1, INV,   0,  -1,
    /* prev=11 */ INV, -1,  +1,   0,
};

/* Floor division / modulo for signed numerator, positive divisor. C's / and %
 * truncate toward zero, so -1 / 80 == 0; we need -1 to floor to -1 detent. */
static int32_t floor_div(int32_t n, int32_t d)
{
    int32_t q = n / d;
    if ((n % d != 0) && ((n < 0) != (d < 0)))
    {
        q--;
    }
    return q;
}

void encoder_core_init(encoder_core_t *e, const encoder_core_cfg_t *cfg)
{
    e->cfg            = *cfg;
    e->count          = 0;
    e->ab_prev        = 0;
    e->invalid        = 0U;
    e->direction      = 0;
    e->rpm_x10        = 0U;
    e->edge_head      = 0U;
    e->edge_fill      = 0U;
    for (uint8_t i = 0U; i < ENCODER_CORE_EDGE_SLOTS; i++)
    {
        e->edge_us[i] = 0U;
    }
    e->last_edge_us   = 0U;
    e->have_edge      = false;
    e->btn_raw_prev   = false;
    e->btn_stable     = false;
    e->btn_candidate  = false;
    e->btn_count_ms   = 0U;
    e->btn_events     = 0U;
}

/* Seed A/B so the first real edge doesn't register a bogus step. Call once
 * after init with the current pin levels. */
void encoder_core_seed(encoder_core_t *e, uint8_t a, uint8_t b)
{
    e->ab_prev = (uint8_t)(((a & 1U) << 1) | (b & 1U));
}

/* Feed one (A,B) sample taken at timestamp us. Called from the backend's
 * pin-change path (ISR on hardware, test harness on host). Updates count,
 * direction and the edge-time ring; must stay short and allocation-free. */
void encoder_core_on_sample(encoder_core_t *e, uint8_t a, uint8_t b, uint32_t us)
{
    uint8_t curr = (uint8_t)(((a & 1U) << 1) | (b & 1U));
    int8_t  step = QUAD_TABLE[(e->ab_prev << 2) | curr];
    e->ab_prev   = curr;

    if (step == ENCODER_CORE_INVALID_STEP)
    {
        /* Both bits flipped between samples: one quadrature state was skipped
         * (contact bounce, or sampling slower than the shaft turns). The shaft
         * really moved 2 positions; dropping it loses counts and drifts the
         * total. Motion is continuous, so assume it continued in the last
         * known direction and apply +/-2. The only time this errs is a
         * reversal landing exactly on a skip (+/-2, self-corrects at the next
         * detent) -- far better than silently losing every skip. */
        e->invalid++;
        if (e->direction == 0)
        {
            return;             /* no prior direction to extend; hold */
        }
        step = (int8_t)(2 * e->direction);
    }
    else if (step == 0)
    {
        return;                 /* no change (e.g. a repeated sample) */
    }

    e->count    += step;
    e->direction = (step > 0) ? +1 : -1;

    /* Record the edge time for the RPM estimator. */
    e->edge_us[e->edge_head] = us;
    e->edge_head = (uint8_t)((e->edge_head + 1U) % ENCODER_CORE_EDGE_SLOTS);
    if (e->edge_fill < ENCODER_CORE_EDGE_SLOTS)
    {
        e->edge_fill++;
    }
    e->last_edge_us = us;
    e->have_edge    = true;
}

int32_t encoder_core_detents(const encoder_core_t *e)
{
    return floor_div(e->count, (int32_t)e->cfg.counts_per_detent);
}

int32_t encoder_core_revolutions(const encoder_core_t *e)
{
    return floor_div(e->count, (int32_t)e->cfg.counts_per_rev);
}

/* Recompute RPM from the edge-time ring. Call periodically with "now" in us.
 *
 * Averaging over the ring (not just the last gap) smooths low-speed jitter;
 * us timestamps give the resolution needed at 600 PPR, where edges arrive
 * ~60 us apart (CLAUDE.md decisions log). If no edge has arrived within
 * stop_timeout_us, speed is 0 and direction clears to 0 (don't show a
 * direction on a stopped shaft).
 */
void encoder_core_update_rpm(encoder_core_t *e, uint32_t now_us)
{
    if (!e->have_edge || ((now_us - e->last_edge_us) >= e->cfg.stop_timeout_us))
    {
        e->rpm_x10   = 0U;
        e->direction = 0;
        return;
    }

    if (e->edge_fill < 2U)
    {
        return;                 /* need two edges to measure an interval */
    }

    /* Oldest and newest timestamps in the ring span (edge_fill-1) intervals. */
    uint8_t newest = (uint8_t)((e->edge_head + ENCODER_CORE_EDGE_SLOTS - 1U)
                               % ENCODER_CORE_EDGE_SLOTS);
    uint8_t oldest = (e->edge_fill < ENCODER_CORE_EDGE_SLOTS)
                     ? 0U
                     : e->edge_head;   /* ring full: head points at the oldest */

    uint32_t span_us   = e->edge_us[newest] - e->edge_us[oldest];
    uint32_t intervals = (uint32_t)(e->edge_fill - 1U);
    if (span_us == 0U)
    {
        return;                 /* implausible; keep previous value */
    }

    /* avg us per count = span / intervals.
     * rev/s      = 1e6 / (counts_per_rev * us_per_count)
     * rpm        = rev/s * 60
     * rpm_x10    = rpm * 10
     *            = 60e7 * intervals / (counts_per_rev * span_us)
     * 600000000 * intervals: at the production worst case (16.8k edges/s,
     * ~15 intervals over the ring) this is ~9e9, so compute in 64-bit. */
    uint64_t num = (uint64_t)600000000U * intervals;
    uint64_t den = (uint64_t)e->cfg.counts_per_rev * span_us;
    e->rpm_x10 = (uint32_t)(num / den);
}

/* Debounce the button. Call once per ms with the raw level (true = pressed;
 * the KY-040 switch is active-low, so the backend inverts before calling).
 * A new level must hold for debounce_ms before it's accepted; a 0->1 accepted
 * edge counts one press event. */
void encoder_core_button_tick(encoder_core_t *e, bool raw_pressed)
{
    if (raw_pressed != e->btn_candidate)
    {
        e->btn_candidate = raw_pressed;    /* level changed: restart the timer */
        e->btn_count_ms  = 0U;
    }
    else if (e->btn_candidate != e->btn_stable)
    {
        if (e->btn_count_ms < e->cfg.debounce_ms)
        {
            e->btn_count_ms++;
        }
        if (e->btn_count_ms >= e->cfg.debounce_ms)
        {
            e->btn_stable = e->btn_candidate;      /* accept the new level */
            if (e->btn_stable)                     /* released -> pressed */
            {
                e->btn_events++;
            }
        }
    }
}
