#pragma once

#include "vehicle_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Modelo del auto para el simulador; sin FreeRTOS para poder probarlo en host. */

/* Ritmos parecidos a un polling OBD real: RPM + velocidad a 20 Hz, MAP a 10 Hz, barómetro cada 30 s. */
#define SIM_MODEL_TICK_MS          50u
#define SIM_MODEL_MAP_EVERY_TICKS  2u
#define SIM_MODEL_BARO_EVERY_TICKS (30000u / SIM_MODEL_TICK_MS)

/* Razones RPM / km/h del modelo; ilustrativas, no medidas en el Forte GT. */
extern const float SIM_MODEL_RATIO[VEHICLE_GEAR_COUNT];

typedef enum {
    SIM_IDLE = 0,
    SIM_LAUNCH, /* clutch patinando en 1.ª */
    SIM_PULL,   /* a fondo en una marcha */
    SIM_SHIFT,  /* clutch pisado entre marchas */
    SIM_DECEL,
} sim_phase_t;

typedef struct {
    sim_phase_t phase;
    float phase_s;
    int gear;
    float speed_kmh;
    float rpm;
    float map_kpa;
    float baro_kpa;
} sim_model_t;

void sim_model_init(sim_model_t *m);
void sim_model_step(sim_model_t *m, float dt_s);

/* Muestra del tick dado con la cadencia y la cuantización de OBD (0x0D en km/h enteros, 0x0C en 1/4 rpm). */
vehicle_sample_t sim_model_sample(const sim_model_t *m, uint32_t t_ms, uint32_t tick);

#ifdef __cplusplus
}
#endif
