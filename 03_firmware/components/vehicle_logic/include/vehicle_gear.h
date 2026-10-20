#pragma once

#include "vehicle_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float ratio[VEHICLE_GEAR_COUNT]; /* RPM / km/h calibrada de 1.ª a 6.ª; 0 = sin calibrar */
    float tolerance;                 /* fracción, p. ej. 0.05 = 5 % (SYS-15) */
    float hysteresis;                /* margen extra para conservar la marcha anterior */
    float min_speed_kmh;             /* debajo de esta velocidad no se calcula (SYS-14) */
} vehicle_gear_cfg_t;

/*
 * Marcha_Engranada de 1 a 6, o VEHICLE_GEAR_NONE si ninguna razón calibrada
 * cae dentro de la tolerancia (clutch pisado, neutral, cambio en curso).
 * prev_gear permite conservar la marcha anterior cerca del límite.
 */
int vehicle_gear_detect(const vehicle_gear_cfg_t *cfg, float rpm, float speed_kmh, int prev_gear);

#ifdef __cplusplus
}
#endif
