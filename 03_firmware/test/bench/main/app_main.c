#include "app.h"
#include "bench.h"
#include "data_source.h"

/*
 * Composition root de la aplicación de banco: el mismo arranque que la del auto (components/app), con el
 * simulador envuelto por la fuente del banco y la consola bench como servicio.
 */

static data_source_t *create_source(void)
{
    return bench_source_create(sim_source_create());
}

static const app_service_t *const services[] = {&bench_service};

static const app_config_t app_config = {
    .create_source = create_source,
    .services = services,
    .service_count = sizeof(services) / sizeof(services[0]),
};

void app_main(void)
{
    app_run(&app_config);
}
