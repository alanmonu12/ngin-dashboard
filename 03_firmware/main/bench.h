#pragma once

#include "esp_err.h"
#include "sdkconfig.h"

#include "data_source.h"
#include "vehicle.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_APP_BENCH

/*
 * Envuelve la fuente real: deja pasar sus muestras con `bench live` o las reemplaza por valores fijos con
 * `bench fix`. Arranca con valores fijos de ralentí. Regresa NULL si inner es NULL o no hay memoria.
 */
data_source_t *bench_source_create(data_source_t *inner);

/* Se llama con cada snapshot de la UI: aplica `bench z2h` y marca las muestras que ese refresco dibuja. */
void bench_on_snapshot(vehicle_outputs_t *out);

/* Registra el comando `bench` en una consola por el puerto serial y empieza a medir la latencia de dibujo. */
esp_err_t bench_start(void);

#endif

#ifdef __cplusplus
}
#endif
