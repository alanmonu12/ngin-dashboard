#include "settings.h"

#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "settings";
static const char *NVS_NAMESPACE = "dash";
static const char *NVS_KEY = "cfg";

#define SHIFT_RPM_MIN  3000
#define SHIFT_RPM_MAX  9000
#define SHIFT_RPM_STEP 100

/*
 * Valores por defecto provisionales: las razones son estimadas y se reemplazan
 * con las medidas en el Forte GT; la RPM_Cambio final depende del limitador (Q14).
 */
static const app_settings_t DEFAULTS = {
    .version = SETTINGS_VERSION,
    .unit = VEHICLE_UNIT_BAR,
    .shift_rpm = {6000, 6000, 6000, 6000, 6000, 6000},
    .bar_start_rpm = 2500,
    .gear_ratio = {120.0f, 70.0f, 48.0f, 37.0f, 30.0f, 25.0f},
};

static app_settings_t current;

static bool valid(const app_settings_t *s)
{
    if (s->version != SETTINGS_VERSION || s->unit >= VEHICLE_UNIT_COUNT) {
        return false;
    }
    for (int g = 0; g < VEHICLE_GEAR_COUNT; g++) {
        uint16_t rpm = s->shift_rpm[g];
        if (rpm < SHIFT_RPM_MIN || rpm > SHIFT_RPM_MAX || rpm % SHIFT_RPM_STEP != 0) {
            return false;
        }
        if (s->gear_ratio[g] < 0.0f) {
            return false;
        }
    }
    return s->bar_start_rpm < SHIFT_RPM_MIN;
}

static esp_err_t nvs_open_flash(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS sin espacio o de otra versión; se borra");
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "erase");
        err = nvs_flash_init();
    }
    return err;
}

esp_err_t settings_init(void)
{
    ESP_RETURN_ON_ERROR(nvs_open_flash(), TAG, "nvs init");
    current = DEFAULTS;

    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "sin ajustes guardados; valores por defecto");
        return ESP_OK;
    }
    app_settings_t stored;
    size_t len = sizeof(stored);
    esp_err_t err = nvs_get_blob(h, NVS_KEY, &stored, &len);
    nvs_close(h);

    if (err == ESP_OK && len == sizeof(stored) && valid(&stored)) {
        current = stored;
    } else {
        ESP_LOGW(TAG, "ajustes guardados inválidos o de otra versión; valores por defecto");
    }
    return ESP_OK;
}

const app_settings_t *settings_get(void)
{
    return &current;
}

static esp_err_t write_nvs(const app_settings_t *s)
{
    nvs_handle_t h;
    ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h), TAG, "open");
    esp_err_t err = nvs_set_blob(h, NVS_KEY, s, sizeof(*s));
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

esp_err_t settings_save(const app_settings_t *s)
{
    ESP_RETURN_ON_FALSE(valid(s), ESP_ERR_INVALID_ARG, TAG, "ajustes fuera de rango");
    ESP_RETURN_ON_ERROR(write_nvs(s), TAG, "write");

    current = *s;
    return ESP_OK;
}

#if CONFIG_APP_BENCH
esp_err_t settings_save_unchecked(const app_settings_t *s)
{
    return write_nvs(s);
}
#endif
