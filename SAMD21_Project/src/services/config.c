/* config.c - see config.h.
 *
 * P1: compile-time constants. P5: replace the bodies with NVM reads + a UART
 * CLI to set them; the accessor signatures stay the same so callers don't
 * change.
 */
#include "services/config.h"

/* --- Per-node defaults. Edit per physical node until NVM config lands. --- */
#define CFG_NODE_ID            "node01"
#define CFG_AREA               "extrusion"
#define CFG_FEET_PER_REV_MILLI 1000U   /* 1.000 ft/rev placeholder; calibrate */

const char *config_node_id(void)
{
    return CFG_NODE_ID;
}

const char *config_area(void)
{
    return CFG_AREA;
}

uint32_t config_feet_per_rev_milli(void)
{
    return CFG_FEET_PER_REV_MILLI;
}

/* --- Geometry parameters for the feet_per_rev estimate (see config.h) ------
 * STUB VALUES -- replace with the real gear/roller data when you have it.
 * Kept as integers so the whole calculation stays float-free.
 *
 * CFG_GEOM_CIRCUM_MILLI_IN : effective circumference of the measuring element,
 *   in milli-inches (thousandths of an inch).
 *     - measuring wheel/roller: circumference = pi * diameter. Measure the
 *       circumference directly if you can; it's more accurate than diameter.
 *     - sprocket + chain: pitch * number_of_teeth (pitch circumference) is more
 *       accurate than the physical diameter.
 * CFG_GEOM_DRIVER_TEETH / CFG_GEOM_DRIVEN_TEETH : the gear reduction between the
 *   encoder shaft (driver) and the measuring element (driven). Set BOTH to 1 if
 *   the encoder sits directly on the measuring element (no gearing).
 *     measuring_revs_per_encoder_rev = driver_teeth / driven_teeth
 * CFG_GEOM_CORRECTION_PPT : empirical correction in parts-per-thousand, from
 *   calibrating on the machine (slip/geometry). 1000 = no correction (x1.000);
 *   e.g. 980 trims the estimate down 2%. Start at 1000, refine after a known-
 *   length run. */
#define CFG_GEOM_CIRCUM_MILLI_IN   0U     /* TODO: measuring element circumference, milli-inches */
#define CFG_GEOM_DRIVER_TEETH      1U     /* TODO: encoder-shaft gear teeth (1 if direct) */
#define CFG_GEOM_DRIVEN_TEETH      1U     /* TODO: measuring-element gear teeth (1 if direct) */
#define CFG_GEOM_CORRECTION_PPT    1000U  /* TODO: calibration trim, parts-per-thousand (1000 = none) */

uint32_t config_calc_feet_per_rev_milli(void)
{
    /* feet_per_rev(milli) =
     *     circumference_in / 12            (inches -> feet)
     *   * driver_teeth / driven_teeth      (encoder rev -> measuring-element rev)
     *   * correction / 1000                (empirical trim)
     *
     * Inputs: circumference is milli-inches, we want milli-feet out, so the
     * x1000 scaling is already baked into the input units. Divide by 12 last
     * and keep the big multiplies in uint64_t so we don't overflow or lose
     * precision mid-calculation. Guard against a zero driven_teeth (would be a
     * config error) so we never divide by zero. */
    if (CFG_GEOM_DRIVEN_TEETH == 0U)
    {
        return CFG_FEET_PER_REV_MILLI;   /* bad config: fall back to placeholder */
    }

    uint64_t v = (uint64_t)CFG_GEOM_CIRCUM_MILLI_IN;   /* milli-inches */
    v *= CFG_GEOM_DRIVER_TEETH;
    v *= CFG_GEOM_CORRECTION_PPT;
    v /= CFG_GEOM_DRIVEN_TEETH;
    v /= 1000U;                                        /* undo correction scale */
    v /= 12U;                                          /* inches -> feet */

    return (uint32_t)v;                                /* milli-feet */
}
