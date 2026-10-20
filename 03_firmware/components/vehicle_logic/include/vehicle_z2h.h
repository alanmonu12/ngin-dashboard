#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VEHICLE_Z2H_IDLE = 0, /* esperando estar detenido */
    VEHICLE_Z2H_ARMED,    /* detenido el tiempo suficiente; listo para medir */
    VEHICLE_Z2H_RUNNING,
    VEHICLE_Z2H_DONE,
    VEHICLE_Z2H_ABORTED,
} vehicle_z2h_state_t;

typedef struct {
    uint32_t arm_ms;     /* tiempo a 0 km/h para armar */
    uint32_t timeout_ms; /* una medición más larga se aborta */
    float target_kmh;    /* 100 */
} vehicle_z2h_cfg_t;

typedef struct {
    vehicle_z2h_state_t state;
    bool has_last;
    uint32_t last_t_ms;
    float last_kmh;
    uint32_t stopped_since_ms;
    uint32_t stop_t_ms;  /* última muestra a 0 km/h antes de arrancar */
    uint32_t first_t_ms; /* primera muestra en movimiento */
    float first_kmh;
    bool start_estimated;
    uint32_t start_ms;  /* instante estimado en que dejó 0 km/h */
    uint32_t result_ms; /* último resultado; se conserva al volver a armar */
} vehicle_z2h_t;

void vehicle_z2h_reset(vehicle_z2h_t *z);

/* Avanza la máquina de estados con una muestra de velocidad (SYS-07, SYS-08). */
vehicle_z2h_state_t vehicle_z2h_update(vehicle_z2h_t *z, const vehicle_z2h_cfg_t *cfg, uint32_t t_ms,
                                       float speed_kmh);

/* Parcial mientras corre; en cualquier otro estado, el último resultado (0 si aún no hay). */
uint32_t vehicle_z2h_elapsed_ms(const vehicle_z2h_t *z, uint32_t now_ms);

#ifdef __cplusplus
}
#endif
