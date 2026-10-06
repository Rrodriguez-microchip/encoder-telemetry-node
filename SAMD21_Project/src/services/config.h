/* config.h - per-node configuration values.
 *
 * One home for the values that differ between physical nodes / machines, so
 * telemetry and (later) MQTT read them from here instead of hard-coding.
 * For now they are compile-time constants; P5 will load them from NVM and let
 * the UART CLI change them (see PROJECT_OVERVIEW.md phase plan). Keeping the
 * accessors stable now means that swap won't touch any caller.
 *
 * FEET_PER_REV is the calibrated feet of material per shaft revolution. It is
 * NOT derived from the sprocket formula (chain geometry + slip make that only
 * an estimate, CLAUDE.md decisions log): calibrate it on the real machine and
 * store it here. Held as an integer x1000 (milli-feet) to avoid floats
 * (no %f in XC32). The POC KY-040 default of 1000 (= 1.000 ft/rev) is a
 * placeholder from the worked example; replace after calibration.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

/* Node identity, used in the MQTT topic bldg/<area>/<node_id>/... (P4) and
 * echoed in telemetry so a viewer can tell nodes apart. */
const char *config_node_id(void);
const char *config_area(void);

/* Calibrated feet per shaft revolution, x1000 (milli-feet). */
uint32_t config_feet_per_rev_milli(void);

#endif /* CONFIG_H */
