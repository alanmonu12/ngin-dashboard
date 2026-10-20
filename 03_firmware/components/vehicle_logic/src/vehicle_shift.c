#include "vehicle_shift.h"

#include <math.h>
#include <string.h>

void vehicle_shift_reset(vehicle_shift_t *s)
{
    memset(s, 0, sizeof(*s));
}

vehicle_shift_out_t vehicle_shift_update(vehicle_shift_t *s, const vehicle_shift_cfg_t *cfg, uint32_t t_ms,
                                         float rpm, float start_rpm, float shift_rpm)
{
    uint32_t dt_ms = t_ms - s->last_t_ms;
    if (s->has_last && dt_ms > 0) {
        float dt_s = (float)dt_ms / 1000.0f;
        float rate = (rpm - s->last_rpm) / dt_s;
        s->rate_rpm_s += cfg->rate_alpha * (rate - s->rate_rpm_s);
    }
    s->has_last = true;
    s->last_t_ms = t_ms;
    s->last_rpm = rpm;

    vehicle_shift_out_t out = {0};
    if (shift_rpm > start_rpm) {
        float f = (rpm - start_rpm) / (shift_rpm - start_rpm);
        out.bar_fraction = fminf(fmaxf(f, 0.0f), 1.0f);
    }

    /* Solo se adelanta mientras las RPM suben; bajando no hay que avisar antes. */
    float lead = s->rate_rpm_s > 0.0f ? s->rate_rpm_s * cfg->latency_ms / 1000.0f : 0.0f;
    out.alarm = rpm + lead >= shift_rpm;
    return out;
}
