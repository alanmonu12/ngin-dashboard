#pragma once

#include "vehicle_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VEHICLE_KPA_PER_BAR 100.0f

/* Boost_Relativo en bar: MAP − barómetro, recortado a 0 (SYS-21). */
float vehicle_boost_bar(float map_kpa, float baro_kpa);

/* Convierte bar a la Unidad_Presión elegida (SYS-17). */
float vehicle_pressure_from_bar(float bar, vehicle_unit_t unit);

/* Decimales que dan la resolución de SYS-02/SYS-17: 0.01 bar, 0.1 psi, 1 kPa. */
int vehicle_pressure_decimals(vehicle_unit_t unit);

const char *vehicle_unit_name(vehicle_unit_t unit);

#ifdef __cplusplus
}
#endif
