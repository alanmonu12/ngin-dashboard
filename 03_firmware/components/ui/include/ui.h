#pragma once

#include "esp_err.h"

#include "vehicle.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Copia el estado actual del vehículo; la UI la llama en cada refresco. */
typedef void (*ui_snapshot_fn_t)(void *ctx, vehicle_outputs_t *out);

typedef struct {
    ui_snapshot_fn_t snapshot;
    void *ctx;
    vehicle_unit_t unit;
} ui_config_t;

/* Las tres funciones toman el lock de LVGL; no llamarlas desde callbacks de LVGL. */
void ui_show_splash(void);
void ui_show_error(const char *msg);
esp_err_t ui_start(const ui_config_t *cfg);

#ifdef __cplusplus
}
#endif
