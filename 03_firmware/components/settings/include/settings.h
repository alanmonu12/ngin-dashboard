#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "sdkconfig.h"

#include "vehicle_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Subir la versión al cambiar el struct; una versión distinta en NVS carga los valores por defecto. */
#define SETTINGS_VERSION 1

typedef struct {
    uint16_t version;
    uint8_t unit;                           /* vehicle_unit_t */
    uint16_t shift_rpm[VEHICLE_GEAR_COUNT]; /* 3000–9000, pasos de 100 (SYS-06) */
    uint16_t bar_start_rpm;
    float gear_ratio[VEHICLE_GEAR_COUNT]; /* RPM / km/h por marcha (SYS-16) */
} app_settings_t;

/* Inicializa NVS y carga los ajustes, o los valores por defecto si no hay. */
esp_err_t settings_init(void);

const app_settings_t *settings_get(void);

/* Valida, guarda en NVS y actualiza la copia en RAM. */
esp_err_t settings_save(const app_settings_t *s);

#if CONFIG_APP_BENCH
/* Solo en el build de banco: guarda sin validar ni actualizar la copia en RAM, para probar SW-703 al
 * reiniciar. */
esp_err_t settings_save_unchecked(const app_settings_t *s);
#endif

#ifdef __cplusplus
}
#endif
