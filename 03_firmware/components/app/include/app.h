#pragma once

#include <stddef.h>

#include "esp_err.h"

#include "data_source.h"
#include "vehicle.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pieza opcional que una aplicación agrega al arranque común (p. ej. la consola del build de banco). */
typedef struct {
    const char *name;
    /* Después de la UI y antes de la fuente, así sirve aunque la fuente falle. NULL si no hace falta. */
    esp_err_t (*start)(void);
    /* En cada refresco de la UI, con el snapshot ya copiado. NULL si no hace falta. */
    void (*on_snapshot)(vehicle_outputs_t *out);
} app_service_t;

typedef struct {
    /* Se llama después de iniciar ajustes y pantalla; regresa NULL si no pudo crear la fuente. */
    data_source_t *(*create_source)(void);
    const app_service_t *const *services;
    size_t service_count;
} app_config_t;

/*
 * Arranque de la aplicación, el mismo para todas: ajustes, pantalla, fuente y lógica, UI, servicios y al
 * final la fuente. Sin ajustes o sin pantalla reinicia (ESP_ERROR_CHECK); si la fuente o un servicio fallan,
 * sigue. cfg debe vivir mientras corra la aplicación.
 */
void app_run(const app_config_t *cfg);

#ifdef __cplusplus
}
#endif
