#include "app.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "bsp.h"
#include "settings.h"
#include "ui.h"

static const char *TAG = "app";

/* Estado compartido: la fuente de datos escribe, la UI copia un snapshot. */
static vehicle_t vehicle;
static SemaphoreHandle_t vehicle_mutex;
static const app_config_t *app_cfg;

static void on_sample(void *ctx, const vehicle_sample_t *sample)
{
    (void)ctx;
    xSemaphoreTake(vehicle_mutex, portMAX_DELAY);
    vehicle_on_sample(&vehicle, sample);
    xSemaphoreGive(vehicle_mutex);
}

static void snapshot(void *ctx, vehicle_outputs_t *out)
{
    (void)ctx;
    xSemaphoreTake(vehicle_mutex, portMAX_DELAY);
    *out = vehicle.out;
    xSemaphoreGive(vehicle_mutex);
    for (size_t i = 0; i < app_cfg->service_count; i++) {
        if (app_cfg->services[i]->on_snapshot) {
            app_cfg->services[i]->on_snapshot(out);
        }
    }
}

/* Las razones de la fuente mandan si las conoce (simulador); si no, las calibradas en los ajustes. */
static vehicle_cfg_t vehicle_cfg_from(const app_settings_t *s, const data_source_t *src)
{
    vehicle_cfg_t cfg;
    vehicle_cfg_set_defaults(&cfg);
    cfg.bar_start_rpm = s->bar_start_rpm;
    const float *ratios = src && src->gear_ratios ? src->gear_ratios : s->gear_ratio;
    for (int g = 0; g < VEHICLE_GEAR_COUNT; g++) {
        cfg.gear.ratio[g] = ratios[g];
        cfg.shift_rpm[g] = s->shift_rpm[g];
    }
    return cfg;
}

static void start_services(void)
{
    for (size_t i = 0; i < app_cfg->service_count; i++) {
        const app_service_t *svc = app_cfg->services[i];
        if (svc->start && svc->start() != ESP_OK) {
            ESP_LOGE(TAG, "no se pudo iniciar el servicio %s", svc->name);
        }
    }
}

void app_run(const app_config_t *cfg)
{
    app_cfg = cfg;

    /* 1–2. Base: sin ajustes o sin pantalla no tiene sentido seguir. */
    ESP_ERROR_CHECK(settings_init());
    ESP_ERROR_CHECK(bsp_display_start());
    ui_show_splash();

    /* 3–4. Fuente de datos y lógica, conectadas aquí. */
    vehicle_mutex = xSemaphoreCreateMutex();
    data_source_t *src = cfg->create_source();
    vehicle_cfg_t vcfg = vehicle_cfg_from(settings_get(), src);
    vehicle_init(&vehicle, &vcfg);

    /* 5. UI antes de arrancar la fuente, para poder mostrar su error. */
    const ui_config_t ui_cfg = {
        .snapshot = snapshot,
        .unit = (vehicle_unit_t)settings_get()->unit,
    };
    ESP_ERROR_CHECK(ui_start(&ui_cfg));
    /* 6. Antes de la fuente: un servicio sirve también cuando la fuente falla (SW-904). */
    start_services();

    /* 7. Una falla de la fuente degrada: la pantalla sigue y avisa. */
    if (!src || data_source_start(src, on_sample, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "no se pudo iniciar la fuente de datos");
        ui_show_error("Sin datos");
        return;
    }
    ESP_LOGI(TAG, "fuente de datos: %s", src->name);
}
