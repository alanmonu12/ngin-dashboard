#include "sdkconfig.h"

#include "app.h"
#include "data_source.h"

/* Composition root de la aplicación del auto: elige la fuente; el arranque está en components/app. */

static data_source_t *create_source(void)
{
#if CONFIG_APP_SOURCE_SIMULATOR
    return sim_source_create();
#else
#error "Fuente de datos no implementada todavía"
#endif
}

static const app_config_t app_config = {
    .create_source = create_source,
};

void app_main(void)
{
    app_run(&app_config);
}
