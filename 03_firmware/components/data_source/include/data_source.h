#pragma once

#include "esp_err.h"

#include "vehicle_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Se llama desde la tarea de la fuente, una vez por muestra. */
typedef void (*data_source_sample_cb_t)(void *ctx, const vehicle_sample_t *sample);

typedef struct data_source data_source_t;

/* Interfaz común a OBD real, simulador y replay de log. */
struct data_source {
    const char *name;
    /* Razones RPM / km/h que la fuente ya conoce (el simulador); NULL si hay que usar las calibradas. */
    const float *gear_ratios;
    esp_err_t (*start)(data_source_t *self, data_source_sample_cb_t cb, void *ctx);
};

static inline esp_err_t data_source_start(data_source_t *src, data_source_sample_cb_t cb, void *ctx)
{
    return src->start(src, cb, ctx);
}

/* Simulador interno: ciclos de arranque a fondo, cambios de 1.ª a 6.ª y frenado. */
data_source_t *sim_source_create(void);

#ifdef __cplusplus
}
#endif
