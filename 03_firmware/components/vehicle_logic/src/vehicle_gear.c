#include "vehicle_gear.h"

#include <math.h>

static float rel_error(float measured, float calibrated)
{
    return fabsf(measured - calibrated) / calibrated;
}

int vehicle_gear_detect(const vehicle_gear_cfg_t *cfg, float rpm, float speed_kmh, int prev_gear)
{
    if (speed_kmh < cfg->min_speed_kmh || speed_kmh <= 0.0f) {
        return VEHICLE_GEAR_NONE;
    }
    float measured = rpm / speed_kmh;

    if (prev_gear >= 1 && prev_gear <= VEHICLE_GEAR_COUNT) {
        float ratio = cfg->ratio[prev_gear - 1];
        if (ratio > 0.0f && rel_error(measured, ratio) <= cfg->tolerance + cfg->hysteresis) {
            return prev_gear;
        }
    }

    int best = VEHICLE_GEAR_NONE;
    float best_err = cfg->tolerance;
    for (int g = 0; g < VEHICLE_GEAR_COUNT; g++) {
        if (cfg->ratio[g] <= 0.0f) {
            continue;
        }
        float err = rel_error(measured, cfg->ratio[g]);
        if (err <= best_err) {
            best_err = err;
            best = g + 1;
        }
    }
    return best;
}
