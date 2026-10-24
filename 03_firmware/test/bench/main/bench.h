#pragma once

#include "app.h"
#include "data_source.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Envuelve la fuente real: deja pasar sus muestras con `bench live` o las reemplaza por valores fijos con
 * `bench fix`. Arranca con valores fijos de ralentí. Regresa NULL si inner es NULL o no hay memoria.
 */
data_source_t *bench_source_create(data_source_t *inner);

/*
 * Consola `bench` por el puerto serial como servicio de app_run: al iniciar registra el comando y empieza a
 * medir la latencia de dibujo; en cada snapshot aplica `bench z2h` y marca las muestras que ese refresco
 * dibuja.
 */
extern const app_service_t bench_service;

#ifdef __cplusplus
}
#endif
