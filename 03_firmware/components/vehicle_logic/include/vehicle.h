#pragma once

#include "vehicle_gear.h"
#include "vehicle_shift.h"
#include "vehicle_types.h"
#include "vehicle_z2h.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    vehicle_gear_cfg_t gear;
    vehicle_shift_cfg_t shift;
    vehicle_z2h_cfg_t z2h;
    float shift_rpm[VEHICLE_GEAR_COUNT]; /* RPM_Cambio por marcha (SYS-06) */
    float bar_start_rpm;                 /* RPM_Inicio_Barra */
} vehicle_cfg_t;

/* Lo que la UI necesita para dibujar; se copia completo en cada refresco. */
typedef struct {
    float rpm;
    float speed_kmh;
    float boost_bar;
    float boost_peak_bar;
    int gear; /* 1..6 o VEHICLE_GEAR_NONE */
    float bar_fraction;
    bool shift_alarm;
    vehicle_z2h_state_t z2h_state;
    uint32_t z2h_ms; /* parcial mientras corre; si no, el último resultado (0 si no hay) */
} vehicle_outputs_t;

typedef struct {
    vehicle_cfg_t cfg;
    vehicle_shift_t shift;
    vehicle_z2h_t z2h;
    float map_kpa;
    float baro_kpa;
    uint32_t fields_seen; /* VEHICLE_HAS_* recibidos alguna vez */
    vehicle_outputs_t out;
} vehicle_t;

/* Barómetro a nivel del mar hasta recibir el PID 0x33. */
#define VEHICLE_DEFAULT_BARO_KPA 101.3f

/*
 * Ajustes por defecto. Provisionales: la tolerancia y la histéresis se revisan tras medir las razones en
 * el Forte GT (SYS-15) y la latencia con los logs de OBD (SYS-05).
 */
#define VEHICLE_DEFAULT_GEAR_TOLERANCE  0.05f /* 5 % */
#define VEHICLE_DEFAULT_GEAR_HYSTERESIS 0.02f /* 2 % extra para conservar la marcha */
#define VEHICLE_DEFAULT_GEAR_MIN_KMH    10.0f /* SYS-14 */
#define VEHICLE_DEFAULT_OBD_LATENCY_MS  75.0f /* mitad del rango esperado de 50–100 ms */
#define VEHICLE_DEFAULT_RPM_RATE_ALPHA  0.3f
#define VEHICLE_DEFAULT_Z2H_ARM_MS      1000u
#define VEHICLE_DEFAULT_Z2H_TIMEOUT_MS  30000u
#define VEHICLE_DEFAULT_Z2H_TARGET_KMH  100.0f

/* Llena cfg con los ajustes por defecto; las razones y RPM_Cambio quedan en 0 y las pone quien llama. */
void vehicle_cfg_set_defaults(vehicle_cfg_t *cfg);

void vehicle_init(vehicle_t *v, const vehicle_cfg_t *cfg);
void vehicle_on_sample(vehicle_t *v, const vehicle_sample_t *s);

#ifdef __cplusplus
}
#endif
