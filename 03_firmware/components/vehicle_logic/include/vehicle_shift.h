#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float latency_ms; /* latencia OBD a compensar (~50–100 ms) */
    float rate_alpha; /* suavizado exponencial de la tasa de RPM, 0..1 */
} vehicle_shift_cfg_t;

typedef struct {
    bool has_last;
    uint32_t last_t_ms;
    float last_rpm;
    float rate_rpm_s; /* tasa de subida suavizada */
} vehicle_shift_t;

typedef struct {
    float bar_fraction; /* longitud de la Barra_RPM, 0..1 (SYS-18) */
    bool alarm;         /* activar la Alarma_Cambio (SYS-05) */
} vehicle_shift_out_t;

void vehicle_shift_reset(vehicle_shift_t *s);

/*
 * Actualiza con una muestra de RPM. La alarma se adelanta con la tasa de
 * subida: se dispara cuando rpm + tasa × latencia >= shift_rpm.
 */
vehicle_shift_out_t vehicle_shift_update(vehicle_shift_t *s, const vehicle_shift_cfg_t *cfg, uint32_t t_ms,
                                         float rpm, float start_rpm, float shift_rpm);

#ifdef __cplusplus
}
#endif
