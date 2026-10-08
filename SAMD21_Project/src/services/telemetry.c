/* telemetry.c - see telemetry.h. */
#include <stdio.h>
#include "services/telemetry.h"
#include "services/timebase.h"
#include "services/config.h"
#include "services/log.h"
#include "drivers/encoder.h"

#define TELEMETRY_PERIOD_MS   100U   /* ~10 Hz, matches the MQTT cadence */

/* Linear speed in milli-ft/s from shaft RPM and the calibrated feet/rev.
 *   ft/s = (rpm / 60) * feet_per_rev
 * with rpm held x10 and feet_per_rev held x1000:
 *   milli_ft_s = (rpm_x10 * fpr_milli) / (60 * 10)
 * Done in 64-bit: rpm_x10 (<~1e5) * fpr_milli (~1e3) stays well within range,
 * but 64-bit keeps headroom if either grows. */
uint32_t telemetry_speed_milli_ft_s(void)
{
    uint64_t n = (uint64_t)encoder_get_rpm_x10() * config_feet_per_rev_milli();
    return (uint32_t)(n / 600U);
}

/* Total material extruded since init, in milli-feet.
 *   total_ft = revolutions * feet_per_rev
 * revolutions is signed; feet_per_rev_milli is already x1000, so the product
 * is milli-feet directly. 64-bit intermediate so a long run can't overflow
 * before we cast back (revolutions * ~1e3 stays in range for any real run). */
int32_t telemetry_total_milli_ft(void)
{
    int64_t n = (int64_t)encoder_get_revolutions()
                * (int64_t)config_feet_per_rev_milli();
    return (int32_t)n;
}

size_t telemetry_build(char *buf, size_t len)
{
    uint32_t r10 = encoder_get_rpm_x10();
    uint32_t sp  = telemetry_speed_milli_ft_s();

    /* rpm and speed printed as "<whole>.<frac>" from their scaled integers
     * (no %f). rpm frac is tenths (x10); speed frac is thousandths (x1000). */
    int n = snprintf(buf, len,
                     "{\"node\":\"%s\",\"count\":%ld,\"det\":%ld,\"rev\":%ld,"
                     "\"dir\":%d,\"rpm\":%lu.%lu,\"ft_s\":%lu.%03lu,\"btn\":%d}",
                     config_node_id(),
                     (long)encoder_get_count(),
                     (long)encoder_get_detents(),
                     (long)encoder_get_revolutions(),
                     (int)encoder_get_direction(),
                     (unsigned long)(r10 / 10U),
                     (unsigned long)(r10 % 10U),
                     (unsigned long)(sp / 1000U),
                     (unsigned long)(sp % 1000U),
                     (int)(encoder_get_button() ? 1 : 0));

    if ((n < 0) || ((size_t)n >= len))
    {
        return 0U;              /* truncated: report failure, not a half line */
    }
    return (size_t)n;
}

void telemetry_task(void)
{
    static uint32_t t_last;
    static bool     started;

    uint32_t now = timebase_ms();
    if (started && ((now - t_last) < TELEMETRY_PERIOD_MS))
    {
        return;
    }
    t_last  = started ? (t_last + TELEMETRY_PERIOD_MS) : now;
    started = true;

    char line[TELEMETRY_MAX_LEN];
    if (telemetry_build(line, sizeof line) != 0U)
    {
        log_line(line);         /* non-blocking: a stalled UART can't hang us */
    }
}
