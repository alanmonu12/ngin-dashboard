#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VEHICLE_GEAR_COUNT 6
#define VEHICLE_GEAR_NONE  0 /* se muestra "–" (SYS-15) */

/* Campos presentes en una muestra; OBD los entrega a ritmos distintos. */
enum {
    VEHICLE_HAS_RPM = 1u << 0,
    VEHICLE_HAS_SPEED = 1u << 1,
    VEHICLE_HAS_MAP = 1u << 2,
    VEHICLE_HAS_BARO = 1u << 3,
};

typedef struct {
    uint32_t t_ms;   /* momento de la respuesta de la ECU */
    uint32_t fields; /* combinación de VEHICLE_HAS_* */
    float rpm;
    float speed_kmh;
    float map_kpa;
    float baro_kpa;
} vehicle_sample_t;

typedef enum {
    VEHICLE_UNIT_BAR = 0,
    VEHICLE_UNIT_PSI,
    VEHICLE_UNIT_KPA,
    VEHICLE_UNIT_COUNT,
} vehicle_unit_t;

#ifdef __cplusplus
}
#endif
