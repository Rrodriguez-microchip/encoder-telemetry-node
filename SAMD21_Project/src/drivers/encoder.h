/* encoder.h - public encoder API, backend-agnostic.
 *
 * app.c, telemetry and the LCD only ever include THIS header. They never see
 * EIC, pins, or the quadrature table. Swapping the KY-040 bench knob for an
 * industrial 600 PPR encoder (quadrature or an SPI counter such as an LS7366R)
 * means writing a new backend .c file; nothing above this line changes.
 *
 * Units are deliberately shaft-only: count / detents / revolutions / rpm /
 * direction / button. The feet-per-minute conversion lives one layer up
 * (telemetry), because FEET_PER_REV is a calibrated, per-machine value stored
 * in NVM (see PROJECT_OVERVIEW.md "Measurement math"); it has no business
 * being baked into a sensor driver.
 *
 * RPM is reported as an integer scaled by 10 ("deci-RPM") to avoid pulling in
 * XC32's float library (no %f, per CLAUDE.md): 425 means 42.5 RPM.
 */
#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>
#include <stdbool.h>

/* Register EIC callbacks / configure the button pin and start counting.
 * Call once, after SYS_Initialize() and timebase_init() (edges are stamped
 * with timebase_us()). */
void encoder_init(void);

/* Run housekeeping: poll+debounce the button at 1 ms and recompute RPM.
 * Call every superloop pass; it is cheap and self-rate-limiting. */
void encoder_task(void);

/* Signed total since init. One full quadrature count; always a multiple of
 * the backend's counts/detent when the knob rests on a detent. */
int32_t  encoder_get_count(void);

/* Signed detents and revolutions, floor-divided so negatives round toward
 * -inf (C truncates toward zero, so a plain / would make -1 detent read 0). */
int32_t  encoder_get_detents(void);
int32_t  encoder_get_revolutions(void);

/* Magnitude of shaft speed, in RPM x 10 (deci-RPM). 0 within ~1 s of the
 * shaft stopping. */
uint32_t encoder_get_rpm_x10(void);

/* -1 / 0 / +1. 0 when stopped: a dashboard showing a direction on a stopped
 * shaft is misleading (CLAUDE.md decisions log). Sign of +1 is whichever way
 * makes count increase; map it to physical direction during bring-up. */
int8_t   encoder_get_direction(void);

/* Debounced button: true while held. */
bool     encoder_get_button(void);

/* Cumulative count of debounced press events (released -> pressed edges).
 * One physical press increments this by exactly one. */
uint32_t encoder_get_button_events(void);

/* Count of illegal quadrature transitions (both A and B appearing to change
 * between two samples) seen since init. Should stay 0 on clean wiring; a
 * rising value means missed edges / excessive bounce / too-fast rotation. */
uint32_t encoder_get_invalid_count(void);

#endif /* ENCODER_H */
