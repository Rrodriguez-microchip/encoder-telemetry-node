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
