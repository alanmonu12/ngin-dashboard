#include "vehicle_z2h.h"

#include <string.h>

void vehicle_z2h_reset(vehicle_z2h_t *z)
{
    memset(z, 0, sizeof(*z));
}

static void mark_stopped(vehicle_z2h_t *z, uint32_t t_ms)
{
    if (!z->has_last || z->last_kmh > 0.0f) {
        z->stopped_since_ms = t_ms;
    }
}

/*
 * Con resolución de 1 km/h el arranque real cae entre la última muestra a 0
 * y la primera en movimiento. Se extrapola hacia atrás con las dos primeras
 * muestras en movimiento y se acota a ese intervalo.
 */
static uint32_t estimate_start(const vehicle_z2h_t *z, uint32_t t2_ms, float v2_kmh)
{
    /* Solo se trabaja con diferencias en uint32_t: los ms absolutos en float pierden precisión con horas
     * encendido. */
    uint32_t window_ms = z->first_t_ms - z->stop_t_ms;
    uint32_t dt_ms = t2_ms - z->first_t_ms;
    if (v2_kmh <= z->first_kmh || dt_ms == 0) {
        return z->stop_t_ms;
    }
    float back_ms = z->first_kmh * (float)dt_ms / (v2_kmh - z->first_kmh);
    if (back_ms >= (float)window_ms) {
        return z->stop_t_ms;
    }
    return z->first_t_ms - (uint32_t)(back_ms + 0.5f);
}

vehicle_z2h_state_t vehicle_z2h_update(vehicle_z2h_t *z, const vehicle_z2h_cfg_t *cfg, uint32_t t_ms,
                                       float speed_kmh)
{
    switch (z->state) {
    case VEHICLE_Z2H_IDLE:
    case VEHICLE_Z2H_DONE:
    case VEHICLE_Z2H_ABORTED:
        if (speed_kmh <= 0.0f) {
            mark_stopped(z, t_ms);
            if (t_ms - z->stopped_since_ms >= cfg->arm_ms) {
                z->state = VEHICLE_Z2H_ARMED;
            }
        }
        break;

    case VEHICLE_Z2H_ARMED:
        if (speed_kmh > 0.0f) {
            z->state = VEHICLE_Z2H_RUNNING;
            z->stop_t_ms = z->last_t_ms;
            z->first_t_ms = t_ms;
            z->first_kmh = speed_kmh;
            z->start_estimated = false;
            z->start_ms = z->stop_t_ms;
        }
        break;

    case VEHICLE_Z2H_RUNNING:
        if (!z->start_estimated) {
            z->start_ms = estimate_start(z, t_ms, speed_kmh);
            z->start_estimated = true;
        }
        if (speed_kmh <= 0.0f || t_ms - z->start_ms > cfg->timeout_ms) {
            z->state = VEHICLE_Z2H_ABORTED;
            mark_stopped(z, t_ms);
            break;
        }
        if (speed_kmh >= cfg->target_kmh) {
            uint32_t t_end_ms = t_ms;
            if (speed_kmh > z->last_kmh) {
                float frac = (cfg->target_kmh - z->last_kmh) / (speed_kmh - z->last_kmh);
                t_end_ms = z->last_t_ms + (uint32_t)(frac * (float)(t_ms - z->last_t_ms) + 0.5f);
            }
            z->result_ms = t_end_ms - z->start_ms;
            z->state = VEHICLE_Z2H_DONE;
        }
        break;
    }

    z->has_last = true;
    z->last_t_ms = t_ms;
    z->last_kmh = speed_kmh;
    return z->state;
}

uint32_t vehicle_z2h_elapsed_ms(const vehicle_z2h_t *z, uint32_t now_ms)
{
    return z->state == VEHICLE_Z2H_RUNNING ? now_ms - z->start_ms : z->result_ms;
}
