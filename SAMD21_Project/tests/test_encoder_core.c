/* test_encoder_core.c - Unity host tests for the pure encoder logic.
 *
 * Runs on the PC (gcc + Unity), no hardware. Links directly against
 * ../src/drivers/encoder_core.c, which is hardware-free by design
 * (CLAUDE.md s3, s7, s8). Covers the P1 acceptance list:
 *   - forward/back count, detents, revolutions
 *   - count-at-rest a multiple of counts_per_detent
 *   - floor division across negatives
 *   - invalid-transition +/-2 recovery (and the dir==0 hold)
 *   - RPM measurement and RPM->0 after the stop timeout
 *   - one-press-one-event button debounce
 *
 * Build/run:  make  (in SAMD21_Project/tests/)
 */
#include "unity.h"
#include "drivers/encoder_core.h"

/* KY-040 geometry: 20 detents/rev, 4 counts/detent -> 80 counts/rev. */
#define CPD   4U
#define CPR   80U
#define DETENTS_PER_REV  (CPR / CPD)   /* 20 */

#define STOP_TIMEOUT_US  1000000U      /* 1 s, per the spec */
#define DEBOUNCE_MS      20U

/* ---- fixture ---------------------------------------------------------- */

static encoder_core_t e;

/* The 4x quadrature Gray sequence of (A<<1|B) states, forward order:
 *   00 -> 01 -> 11 -> 10 -> (00)
 * One full pass through the 4 states = one detent = 4 counts. */
static const uint8_t FWD_STATES[4] = { 0x0, 0x1, 0x3, 0x2 };

static uint32_t g_clock_us;   /* monotonic timestamp source for samples */

void setUp(void)
{
    const encoder_core_cfg_t cfg = {
        .counts_per_detent = CPD,
        .counts_per_rev    = CPR,
        .stop_timeout_us   = STOP_TIMEOUT_US,
        .debounce_ms       = DEBOUNCE_MS,
    };
    encoder_core_init(&e, &cfg);
    encoder_core_seed(&e, 0, 0);   /* start parked at state 00 */
    g_clock_us = 0U;
}

void tearDown(void) {}

/* Feed state s = (A<<1|B) at an auto-incrementing timestamp. */
static void feed_state(uint8_t s, uint32_t dt_us)
{
    g_clock_us += dt_us;
    encoder_core_on_sample(&e, (uint8_t)((s >> 1) & 1U), (uint8_t)(s & 1U), g_clock_us);
}

/* Drive n detents forward (dir=+1) or backward (dir=-1), dt_us between each
 * single-step sample. Walks the Gray sequence one state at a time so every
 * transition is a legal single-bit change. */
static void drive_detents(int n, int dir, uint32_t dt_us)
{
    int steps = n * 4;
    static int idx = 0;        /* index into FWD_STATES, persists across calls */
    for (int i = 0; i < steps; i++)
    {
        idx = (idx + (dir > 0 ? 1 : 3)) & 3;   /* +1 fwd, +3 == -1 mod 4 */
        feed_state(FWD_STATES[idx], dt_us);
    }
}

/* ---- quadrature counting --------------------------------------------- */

void test_forward_20_detents(void)
{
    drive_detents(20, +1, 100U);
    TEST_ASSERT_EQUAL_INT32(80, e.count);
    TEST_ASSERT_EQUAL_INT32(20, encoder_core_detents(&e));
    TEST_ASSERT_EQUAL_INT32(1,  encoder_core_revolutions(&e));
    TEST_ASSERT_EQUAL_INT8(+1,  e.direction);
    TEST_ASSERT_EQUAL_UINT32(0, e.invalid);
}

void test_forward_then_back_to_zero(void)
{
    drive_detents(20, +1, 100U);
    drive_detents(20, -1, 100U);
    TEST_ASSERT_EQUAL_INT32(0, e.count);
    TEST_ASSERT_EQUAL_INT32(0, encoder_core_detents(&e));
    TEST_ASSERT_EQUAL_INT32(0, encoder_core_revolutions(&e));
    TEST_ASSERT_EQUAL_INT8(-1, e.direction);   /* last motion was reverse */
    TEST_ASSERT_EQUAL_UINT32(0, e.invalid);
}

void test_count_at_rest_multiple_of_detent(void)
{
    for (int d = 0; d < 7; d++)
    {
        drive_detents(1, +1, 100U);
        TEST_ASSERT_EQUAL_INT32(0, e.count % (int32_t)CPD);
    }
}

/* ---- floor division across negatives --------------------------------- */
/* C's / truncates toward zero; encoder_core uses floor_div so -1 count ->
 * detent -1 (not 0). We exercise it through the public getters by forcing
 * e.count directly (the math under test is the division, not the stepping). */

void test_floor_div_negatives(void)
{
    e.count = -1;
    TEST_ASSERT_EQUAL_INT32(-1, encoder_core_detents(&e));      /* -1/4  -> -1 */
    TEST_ASSERT_EQUAL_INT32(-1, encoder_core_revolutions(&e));  /* -1/80 -> -1 */

    e.count = -79;
    TEST_ASSERT_EQUAL_INT32(-20, encoder_core_detents(&e));     /* -79/4 -> -20 */
    TEST_ASSERT_EQUAL_INT32(-1,  encoder_core_revolutions(&e)); /* -79/80 -> -1 */

    e.count = -80;
    TEST_ASSERT_EQUAL_INT32(-20, encoder_core_detents(&e));
    TEST_ASSERT_EQUAL_INT32(-1,  encoder_core_revolutions(&e)); /* exact -> -1 */

    e.count = -81;
    TEST_ASSERT_EQUAL_INT32(-21, encoder_core_detents(&e));
    TEST_ASSERT_EQUAL_INT32(-2,  encoder_core_revolutions(&e)); /* -81/80 -> -2 */

    e.count = 81;
    TEST_ASSERT_EQUAL_INT32(20, encoder_core_detents(&e));      /* positive truncates fine */
    TEST_ASSERT_EQUAL_INT32(1,  encoder_core_revolutions(&e));
}

/* ---- invalid-transition recovery ------------------------------------- */

void test_invalid_recovers_plus2_in_last_direction(void)
{
    /* Establish forward direction with one clean detent. */
    drive_detents(1, +1, 100U);
    TEST_ASSERT_EQUAL_INT8(+1, e.direction);
    int32_t before = e.count;

    /* Current state is 00 (idx wrapped). Jump straight to 11: both bits flip
     * = one skipped state = INVALID. Should apply +2 (last dir) and count it. */
    e.ab_prev = 0x0;
    feed_state(0x3, 100U);
    TEST_ASSERT_EQUAL_INT32(before + 2, e.count);
    TEST_ASSERT_EQUAL_UINT32(1, e.invalid);
    TEST_ASSERT_EQUAL_INT8(+1, e.direction);
}

void test_invalid_with_no_direction_holds(void)
{
    /* Fresh encoder, direction==0. An invalid transition can't guess a way,
     * so it must hold position (count unchanged) and still tally invalid. */
    TEST_ASSERT_EQUAL_INT8(0, e.direction);
    e.ab_prev = 0x0;
    feed_state(0x3, 100U);   /* 00 -> 11, both bits flip */
    TEST_ASSERT_EQUAL_INT32(0, e.count);
    TEST_ASSERT_EQUAL_UINT32(1, e.invalid);
}

/* ---- RPM -------------------------------------------------------------- */

void test_rpm_zero_before_any_edge(void)
{
    encoder_core_update_rpm(&e, 50000U);
    TEST_ASSERT_EQUAL_UINT32(0, e.rpm_x10);
}

void test_rpm_measures_then_zeroes_after_timeout(void)
{
    /* Fill the edge ring with evenly spaced samples. 1000 us between counts
     * -> 1000 counts/s / 80 counts/rev = 12.5 rev/s = 750 rpm -> rpm_x10 7500.
     * Spacing is exact, so the result is exact (no tolerance needed). */
    drive_detents(2, +1, 1000U);          /* 8 counts, 8 edges, dt=1000us */
    encoder_core_update_rpm(&e, g_clock_us);
    TEST_ASSERT_EQUAL_UINT32(7500, e.rpm_x10);        /* 750.0 rpm */

    /* Now advance well past the stop timeout with no new edge. */
    encoder_core_update_rpm(&e, g_clock_us + STOP_TIMEOUT_US + 1U);
    TEST_ASSERT_EQUAL_UINT32(0, e.rpm_x10);
    TEST_ASSERT_EQUAL_INT8(0, e.direction);           /* dir clears when stopped */
}

/* ---- button debounce -------------------------------------------------- */

static void button_hold(bool pressed, uint16_t ms)
{
    for (uint16_t i = 0; i < ms; i++)
    {
        encoder_core_button_tick(&e, pressed);
    }
}

void test_button_one_press_one_event(void)
{
    button_hold(false, 5);
    button_hold(true,  DEBOUNCE_MS + 5);   /* held long enough to accept */
    TEST_ASSERT_EQUAL_UINT32(1, e.btn_events);
    TEST_ASSERT_TRUE(e.btn_stable);

    button_hold(false, DEBOUNCE_MS + 5);   /* release, no new event */
    TEST_ASSERT_EQUAL_UINT32(1, e.btn_events);
    TEST_ASSERT_FALSE(e.btn_stable);
}

void test_button_glitch_shorter_than_window_ignored(void)
{
    button_hold(false, 5);
    button_hold(true,  DEBOUNCE_MS - 1);   /* not long enough */
    button_hold(false, DEBOUNCE_MS + 5);   /* bounces back before accept */
    TEST_ASSERT_EQUAL_UINT32(0, e.btn_events);
    TEST_ASSERT_FALSE(e.btn_stable);
}

/* ---- runner ----------------------------------------------------------- */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_forward_20_detents);
    RUN_TEST(test_forward_then_back_to_zero);
    RUN_TEST(test_count_at_rest_multiple_of_detent);
    RUN_TEST(test_floor_div_negatives);
    RUN_TEST(test_invalid_recovers_plus2_in_last_direction);
    RUN_TEST(test_invalid_with_no_direction_holds);
    RUN_TEST(test_rpm_zero_before_any_edge);
    RUN_TEST(test_rpm_measures_then_zeroes_after_timeout);
    RUN_TEST(test_button_one_press_one_event);
    RUN_TEST(test_button_glitch_shorter_than_window_ignored);
    return UNITY_END();
}
