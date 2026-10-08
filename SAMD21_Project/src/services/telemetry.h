/* telemetry.h - periodic telemetry as one JSON line.
 *
 * Builds the node's telemetry payload from the encoder API into a caller-
 * supplied buffer, in the exact shape the MQTT payload will use (P4):
 *
 *   {"node":"node01","count":1234,"det":308,"rev":15,"dir":1,
 *    "rpm":42.5,"ft_s":0.710,"btn":0}
 *
 * The build is transport-agnostic on purpose: P1 emits the string over the
 * UART; P4 will hand the same bytes to the MQTT publish on
 * bldg/<area>/<node_id>/telemetry. Nothing here knows about UART or W5500.
 *
 * No %f (XC32 would pull in a large float library, CLAUDE.md s3): rpm is held
 * as an integer x10 and speed as milli-ft/s, formatted as "<n>.<frac>".
 * ft_s = (rpm/60) * feet_per_rev, using the calibrated feet/rev from config.
 */
#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>
#include <stddef.h>

/* Longest line the current payload can produce, incl. the NUL. Generous:
 * int32 fields can be -2147483648 (11 chars), node id is bounded. Bump this
 * if fields are added. */
#define TELEMETRY_MAX_LEN   128U

/* Build the JSON payload into buf (size len). Returns the string length
 * (excluding NUL), or 0 if the buffer was too small. Reads the encoder API;
 * call from the main loop, not an ISR. */
size_t telemetry_build(char *buf, size_t len);

/* Rate-limited emitter: builds the payload and prints it over the UART at
 * ~10 Hz. Call every superloop pass; it self-gates on the timebase. */
void telemetry_task(void);

/* Material-math helpers, shared so the LCD and the JSON payload use ONE
 * implementation of the feet_per_rev conversion and can never drift (same
 * "one builder feeds both" rule as the UART/MQTT payload). Both are integer-
 * only (no %f): split them into whole/frac for display.
 *   speed:  milli-ft/s  = (rpm/60) * feet_per_rev   -> frac is thousandths
 *   total:  milli-ft     = revolutions * feet_per_rev -> frac is thousandths */
uint32_t telemetry_speed_milli_ft_s(void);
int32_t  telemetry_total_milli_ft(void);

#endif /* TELEMETRY_H */
