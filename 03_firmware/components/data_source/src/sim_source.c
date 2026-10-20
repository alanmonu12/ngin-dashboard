#include <stdlib.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "data_source.h"
#include "sim_model.h"

static const char *TAG = "sim";

#define SIM_TASK_CORE  0
#define SIM_TASK_PRIO  5
#define SIM_TASK_STACK 3072

typedef struct {
    data_source_t base;
    TaskHandle_t task;
    data_source_sample_cb_t cb;
    void *ctx;
} sim_source_t;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void sim_task(void *arg)
{
    sim_source_t *self = arg;
    sim_model_t model;
    sim_model_init(&model);

    TickType_t last_wake = xTaskGetTickCount();
    for (uint32_t tick = 0;; tick++) {
        sim_model_step(&model, SIM_MODEL_TICK_MS / 1000.0f);
        vehicle_sample_t s = sim_model_sample(&model, now_ms(), tick);
        self->cb(self->ctx, &s);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SIM_MODEL_TICK_MS));
    }
}

static esp_err_t sim_start(data_source_t *base, data_source_sample_cb_t cb, void *ctx)
{
    sim_source_t *self = (sim_source_t *)base;
    ESP_RETURN_ON_FALSE(cb && !self->task, ESP_ERR_INVALID_STATE, TAG, "ya iniciado o sin callback");
    self->cb = cb;
    self->ctx = ctx;
    BaseType_t ok = xTaskCreatePinnedToCore(sim_task, "sim_source", SIM_TASK_STACK, self, SIM_TASK_PRIO,
                                            &self->task, SIM_TASK_CORE);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "tarea");
    ESP_LOGI(TAG, "simulador iniciado");
    return ESP_OK;
}

data_source_t *sim_source_create(void)
{
    sim_source_t *self = calloc(1, sizeof(*self));
    if (!self) {
        return NULL;
    }
    self->base = (data_source_t){
        .name = "simulador",
        .gear_ratios = SIM_MODEL_RATIO,
        .start = sim_start,
    };
    return &self->base;
}
