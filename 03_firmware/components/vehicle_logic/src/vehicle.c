#include "vehicle.h"

#include <string.h>

#include "vehicle_boost.h"

void vehicle_cfg_set_defaults(vehicle_cfg_t *cfg)
{
    *cfg = (vehicle_cfg_t){
        .gear = {.tolerance = VEHICLE_DEFAULT_GEAR_TOLERANCE,
                 .hysteresis = VEHICLE_DEFAULT_GEAR_HYSTERESIS,
                 .min_speed_kmh = VEHICLE_DEFAULT_GEAR_MIN_KMH},
        .shift = {.latency_ms = VEHICLE_DEFAULT_OBD_LATENCY_MS, .rate_alpha = VEHICLE_DEFAULT_RPM_RATE_ALPHA},
        .z2h = {.arm_ms = VEHICLE_DEFAULT_Z2H_ARM_MS,
                .timeout_ms = VEHICLE_DEFAULT_Z2H_TIMEOUT_MS,
                .target_kmh = VEHICLE_DEFAULT_Z2H_TARGET_KMH},
    };
}

void vehicle_init(vehicle_t *v, const vehicle_cfg_t *cfg)
{
    /* Todo en cero también deja listos shift y z2h. */
    memset(v, 0, sizeof(*v));
    v->cfg = *cfg;
    v->baro_kpa = VEHICLE_DEFAULT_BARO_KPA;
}

/* Cada entrada en mask ya llegó al menos una vez (los PIDs llegan a ritmos distintos). */
static bool seen_all(const vehicle_t *v, uint32_t mask)
{
    return (v->fields_seen & mask) == mask;
}

static float shift_rpm_for(const vehicle_t *v, int gear)
{
    if (gear >= 1 && gear <= VEHICLE_GEAR_COUNT) {
        return v->cfg.shift_rpm[gear - 1];
    }
    /* Sin marcha conocida se usa la RPM_Cambio más baja: avisar de más es más seguro. */
    float lowest = v->cfg.shift_rpm[0];
    for (int g = 1; g < VEHICLE_GEAR_COUNT; g++) {
        if (v->cfg.shift_rpm[g] < lowest) {
            lowest = v->cfg.shift_rpm[g];
        }
    }
    return lowest;
}

void vehicle_on_sample(vehicle_t *v, const vehicle_sample_t *s)
{
    vehicle_outputs_t *o = &v->out;
    v->fields_seen |= s->fields;

    if (s->fields & VEHICLE_HAS_BARO) {
        v->baro_kpa = s->baro_kpa;
    }
    if (s->fields & VEHICLE_HAS_MAP) {
        v->map_kpa = s->map_kpa;
    }
    /* El barómetro tiene valor por defecto; el MAP no: sin uno real no hay boost. */
    if ((s->fields & (VEHICLE_HAS_MAP | VEHICLE_HAS_BARO)) && seen_all(v, VEHICLE_HAS_MAP)) {
        o->boost_bar = vehicle_boost_bar(v->map_kpa, v->baro_kpa);
        if (o->boost_bar > o->boost_peak_bar) {
            o->boost_peak_bar = o->boost_bar;
        }
    }
    if (s->fields & VEHICLE_HAS_SPEED) {
        o->speed_kmh = s->speed_kmh;
        o->z2h_state = vehicle_z2h_update(&v->z2h, &v->cfg.z2h, s->t_ms, s->speed_kmh);
        o->z2h_ms = vehicle_z2h_elapsed_ms(&v->z2h, s->t_ms);
    }
    if (s->fields & VEHICLE_HAS_RPM) {
        o->rpm = s->rpm;
    }
    /* OBD puede entregar 0x0C y 0x0D en respuestas separadas: se usa el último valor de cada uno. */
    const uint32_t rpm_and_speed = VEHICLE_HAS_RPM | VEHICLE_HAS_SPEED;
    if ((s->fields & rpm_and_speed) && seen_all(v, rpm_and_speed)) {
        o->gear = vehicle_gear_detect(&v->cfg.gear, o->rpm, o->speed_kmh, o->gear);
    }
    if (s->fields & VEHICLE_HAS_RPM) {
        vehicle_shift_out_t sh = vehicle_shift_update(&v->shift, &v->cfg.shift, s->t_ms, s->rpm,
                                                      v->cfg.bar_start_rpm, shift_rpm_for(v, o->gear));
        o->bar_fraction = sh.bar_fraction;
        o->shift_alarm = sh.alarm;
    }
}
