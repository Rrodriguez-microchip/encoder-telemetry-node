/* encoder_core.h - pure quadrature/RPM/button logic, no hardware.
 *
 * The backend (encoder_ky040.c, or a future industrial one) owns an
 * encoder_core_t, feeds it (A,B) samples with timestamps, ticks the button
 * once per ms, and periodically recomputes RPM. All state lives in the struct
 * the caller provides, so this file is testable on the host (gcc + Unity).
 *
 * Not thread/ISR-safe on its own: the backend serializes access (edges arrive
 * in one EIC priority; the main loop takes a critical-section snapshot before
 * reading). See encoder_ky040.c.
 */
#ifndef ENCODER_CORE_H
#define ENCODER_CORE_H

#include <stdint.h>
#include <stdbool.h>

/* Sentinel step value for an illegal quadrature transition. Chosen outside
 * {-1,0,+1} so on_sample() can distinguish it. */
#define ENCODER_CORE_INVALID_STEP   2

/* Edge-time ring depth. Averaging a handful of recent edges smooths low-speed
 * RPM without lagging; 8 is plenty for the KY-040 and fits the production
 * rate. Must be a power of two is NOT required (we use %). */
#define ENCODER_CORE_EDGE_SLOTS     8U

typedef struct
{
    uint16_t counts_per_detent;  /* 4 for a 4x-decoded quadrature encoder */
    uint16_t counts_per_rev;     /* 80 for KY-040 (20 detents x 4) */
    uint32_t stop_timeout_us;    /* no edge for this long -> rpm 0, dir 0 */
    uint16_t debounce_ms;        /* button stability window (e.g. 20) */
} encoder_core_cfg_t;

typedef struct
{
    encoder_core_cfg_t cfg;

    /* Quadrature */
    volatile int32_t  count;
    uint8_t           ab_prev;       /* last (A<<1|B) */
    volatile uint32_t invalid;       /* illegal-transition counter */
    volatile int8_t   direction;     /* -1 / 0 / +1 */

    /* RPM estimator (edge-time ring) */
    volatile uint32_t rpm_x10;
    uint32_t          edge_us[ENCODER_CORE_EDGE_SLOTS];
    uint8_t           edge_head;     /* next write index */
    uint8_t           edge_fill;     /* valid samples, saturates at SLOTS */
    uint32_t          last_edge_us;
    bool              have_edge;

    /* Button debounce */
    bool              btn_raw_prev;
    bool              btn_stable;     /* debounced level, true = pressed */
    bool              btn_candidate;  /* level currently being timed */
    uint16_t          btn_count_ms;
    volatile uint32_t btn_events;     /* released->pressed transitions */
} encoder_core_t;

void    encoder_core_init(encoder_core_t *e, const encoder_core_cfg_t *cfg);
void    encoder_core_seed(encoder_core_t *e, uint8_t a, uint8_t b);
void    encoder_core_on_sample(encoder_core_t *e, uint8_t a, uint8_t b, uint32_t us);
int32_t encoder_core_detents(const encoder_core_t *e);
int32_t encoder_core_revolutions(const encoder_core_t *e);
void    encoder_core_update_rpm(encoder_core_t *e, uint32_t now_us);
void    encoder_core_button_tick(encoder_core_t *e, bool raw_pressed);

#endif /* ENCODER_CORE_H */
