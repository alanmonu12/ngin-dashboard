#include "bench.h"

#if CONFIG_APP_BENCH

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "esp_attr.h"
#include "esp_check.h"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "bsp.h"
#include "settings.h"
#include "vehicle_boost.h"

static const char *TAG = "bench";

#define FIXED_PERIOD_MS  50 /* mismo ritmo que el simulador para RPM y velocidad */
#define FIXED_TASK_CORE  0
#define FIXED_TASK_PRIO  5
#define FIXED_TASK_STACK 3072
#define LOCK_TIMEOUT_MS  1000

#define LATENCY_WINDOW    1000 /* actualizaciones que resume `bench latency` (SW-501) */
#define Z2H_OFF           UINT32_MAX
#define CORRUPT_SHIFT_RPM 2900 /* fuera del rango de SW-307 */

/* Valores fijos al arrancar: ralentí, detenido y en vacío, así el Boost_Pico parte de 0. */
#define IDLE_RPM      800.0f
#define IDLE_MAP_KPA  32.0f
#define IDLE_BARO_KPA 78.0f

/* Sobrevive a esp_restart(): `bench fail_source` hace fallar el siguiente arranque de la fuente (SW-904). */
#define FAIL_SOURCE_MAGIC 0xBE7C4FA1u
static RTC_NOINIT_ATTR uint32_t fail_source_flag;

typedef struct {
    data_source_t base;
    data_source_t *inner;
    data_source_sample_cb_t cb;
    void *ctx;
    TaskHandle_t task;
} bench_source_t;

static struct {
    portMUX_TYPE lock;
    bool fixed;
    vehicle_sample_t fixed_sample;
    uint32_t z2h_ms;
    /* Latencia: la muestra más vieja que la UI todavía no copió, y la que copió y aún no se dibuja. */
    bool pending, drawing;
    uint32_t pending_t_ms, drawing_t_ms;
    uint16_t latency_ms[LATENCY_WINDOW];
    size_t latency_count, latency_next;
} bench = {
    .lock = portMUX_INITIALIZER_UNLOCKED,
    .fixed = true,
    .fixed_sample =
        {
            .fields = VEHICLE_HAS_RPM | VEHICLE_HAS_SPEED | VEHICLE_HAS_MAP | VEHICLE_HAS_BARO,
            .rpm = IDLE_RPM,
            .map_kpa = IDLE_MAP_KPA,
            .baro_kpa = IDLE_BARO_KPA,
        },
    .z2h_ms = Z2H_OFF,
};

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void deliver(bench_source_t *self, const vehicle_sample_t *s)
{
    taskENTER_CRITICAL(&bench.lock);
    if (!bench.pending) {
        bench.pending = true;
        bench.pending_t_ms = s->t_ms;
    }
    taskEXIT_CRITICAL(&bench.lock);
    self->cb(self->ctx, s);
}

static void inner_sample(void *ctx, const vehicle_sample_t *s)
{
    if (!bench.fixed) {
        deliver(ctx, s);
    }
}

static void fixed_task(void *arg)
{
    bench_source_t *self = arg;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        if (bench.fixed) {
            taskENTER_CRITICAL(&bench.lock);
            vehicle_sample_t s = bench.fixed_sample;
            taskEXIT_CRITICAL(&bench.lock);
            s.t_ms = now_ms();
            deliver(self, &s);
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(FIXED_PERIOD_MS));
    }
}

static esp_err_t bench_source_start(data_source_t *base, data_source_sample_cb_t cb, void *ctx)
{
    bench_source_t *self = (bench_source_t *)base;
    if (fail_source_flag == FAIL_SOURCE_MAGIC) {
        fail_source_flag = 0;
        ESP_LOGW(TAG, "falla provocada de la fuente (bench fail_source)");
        return ESP_FAIL;
    }
    self->cb = cb;
    self->ctx = ctx;
    BaseType_t ok = xTaskCreatePinnedToCore(fixed_task, "bench_fixed", FIXED_TASK_STACK, self,
                                            FIXED_TASK_PRIO, &self->task, FIXED_TASK_CORE);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "tarea");
    return data_source_start(self->inner, inner_sample, self);
}

data_source_t *bench_source_create(data_source_t *inner)
{
    if (!inner) {
        return NULL;
    }
    bench_source_t *self = calloc(1, sizeof(*self));
    if (!self) {
        return NULL;
    }
    self->inner = inner;
    self->base = (data_source_t){
        .name = "banco",
        .gear_ratios = inner->gear_ratios,
        .start = bench_source_start,
    };
    return &self->base;
}

void bench_on_snapshot(vehicle_outputs_t *out)
{
    taskENTER_CRITICAL(&bench.lock);
    if (bench.z2h_ms != Z2H_OFF) {
        out->z2h_ms = bench.z2h_ms;
    }
    if (bench.pending && !bench.drawing) {
        bench.drawing = true;
        bench.drawing_t_ms = bench.pending_t_ms;
        bench.pending = false;
    }
    taskEXIT_CRITICAL(&bench.lock);
}

/* Fin del render del cuadro: el último bloque puede tardar unos ms más en salir por SPI. Con el monitor de
 * rendimiento de LVGL activo (sdkconfig.defaults) el refresco corre en cada periodo aunque nada cambie. */
static void refr_ready_cb(lv_event_t *e)
{
    (void)e;
    taskENTER_CRITICAL(&bench.lock);
    if (bench.drawing) {
        bench.drawing = false;
        uint32_t latency = now_ms() - bench.drawing_t_ms;
        bench.latency_ms[bench.latency_next] = latency > UINT16_MAX ? UINT16_MAX : (uint16_t)latency;
        bench.latency_next = (bench.latency_next + 1) % LATENCY_WINDOW;
        if (bench.latency_count < LATENCY_WINDOW) {
            bench.latency_count++;
        }
    }
    taskEXIT_CRITICAL(&bench.lock);
}

/* --- Comandos --- */

static int cmd_fix(int argc, char **argv)
{
    vehicle_sample_t s;
    taskENTER_CRITICAL(&bench.lock);
    s = bench.fixed_sample;
    taskEXIT_CRITICAL(&bench.lock);

    for (int i = 2; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        if (!eq) {
            printf("esperaba campo=valor: %s\n", argv[i]);
            return 1;
        }
        *eq = '\0';
        float v = strtof(eq + 1, NULL);
        if (strcmp(argv[i], "rpm") == 0) {
            s.rpm = v;
        } else if (strcmp(argv[i], "kmh") == 0) {
            s.speed_kmh = v;
        } else if (strcmp(argv[i], "map") == 0) {
            s.map_kpa = v;
        } else if (strcmp(argv[i], "baro") == 0) {
            s.baro_kpa = v;
        } else {
            printf("campo desconocido: %s (rpm, kmh, map, baro)\n", argv[i]);
            return 1;
        }
    }

    taskENTER_CRITICAL(&bench.lock);
    bench.fixed_sample = s;
    bench.fixed = true;
    taskEXIT_CRITICAL(&bench.lock);
    printf("fijo: rpm=%.2f kmh=%.2f map=%.1f baro=%.1f\n", s.rpm, s.speed_kmh, s.map_kpa, s.baro_kpa);
    return 0;
}

static int cmd_z2h(int argc, char **argv)
{
    if (argc < 3) {
        printf("uso: bench z2h <ms>|off\n");
        return 1;
    }
    uint32_t ms = strcasecmp(argv[2], "off") == 0 ? Z2H_OFF : (uint32_t)strtoul(argv[2], NULL, 10);
    taskENTER_CRITICAL(&bench.lock);
    bench.z2h_ms = ms;
    taskEXIT_CRITICAL(&bench.lock);
    return 0;
}

static void print_settings(const app_settings_t *s)
{
    printf("unidad=%s bar_start=%u shift=", vehicle_unit_name((vehicle_unit_t)s->unit), s->bar_start_rpm);
    for (int g = 0; g < VEHICLE_GEAR_COUNT; g++) {
        printf("%u%s", s->shift_rpm[g], g + 1 < VEHICLE_GEAR_COUNT ? "," : "\n");
    }
}

static bool parse_unit(const char *name, uint8_t *unit)
{
    for (int u = 0; u < VEHICLE_UNIT_COUNT; u++) {
        if (strcasecmp(name, vehicle_unit_name((vehicle_unit_t)u)) == 0) {
            *unit = (uint8_t)u;
            return true;
        }
    }
    return false;
}

static int cmd_save(int argc, char **argv)
{
    app_settings_t s = *settings_get();
    if (argc == 4 && strcmp(argv[2], "unit") == 0 && parse_unit(argv[3], &s.unit)) {
        /* La UI lee la unidad al arrancar: se ve después de reiniciar. */
    } else if (argc == 4 && strcmp(argv[2], "bar_start") == 0) {
        s.bar_start_rpm = (uint16_t)strtoul(argv[3], NULL, 10);
    } else if (argc == 5 && strcmp(argv[2], "shift") == 0) {
        int gear = atoi(argv[3]);
        if (gear < 1 || gear > VEHICLE_GEAR_COUNT) {
            printf("marcha de 1 a %d\n", VEHICLE_GEAR_COUNT);
            return 1;
        }
        s.shift_rpm[gear - 1] = (uint16_t)strtoul(argv[4], NULL, 10);
    } else {
        printf("uso: bench save unit <bar|psi|kPa> | shift <marcha> <rpm> | bar_start <rpm>\n");
        return 1;
    }
    esp_err_t err = settings_save(&s);
    printf("%s\n", err == ESP_OK ? "OK" : esp_err_to_name(err));
    return err == ESP_OK ? 0 : 1;
}

static int cmd_corrupt(void)
{
    app_settings_t s = *settings_get();
    s.shift_rpm[0] = CORRUPT_SHIFT_RPM;
    esp_err_t err = settings_save_unchecked(&s);
    printf("%s; reinicia para cargarlos\n", err == ESP_OK ? "guardados sin validar" : esp_err_to_name(err));
    return err == ESP_OK ? 0 : 1;
}

static int cmd_latency(void)
{
    uint16_t copy[LATENCY_WINDOW];
    size_t n;
    taskENTER_CRITICAL(&bench.lock);
    n = bench.latency_count;
    memcpy(copy, bench.latency_ms, sizeof(copy));
    taskEXIT_CRITICAL(&bench.lock);

    if (n == 0) {
        printf("sin mediciones todavía\n");
        return 1;
    }
    uint32_t max = 0, sum = 0;
    for (size_t i = 0; i < n; i++) {
        max = copy[i] > max ? copy[i] : max;
        sum += copy[i];
    }
    printf("latencia de muestra a cuadro dibujado, últimas %u: máx %lu ms, media %lu ms\n", (unsigned)n,
           (unsigned long)max, (unsigned long)(sum / n));
    return 0;
}

static int bench_cmd(int argc, char **argv)
{
    const char *sub = argc > 1 ? argv[1] : "";
    if (strcmp(sub, "fix") == 0) {
        return cmd_fix(argc, argv);
    }
    if (strcmp(sub, "live") == 0) {
        bench.fixed = false;
        return 0;
    }
    if (strcmp(sub, "z2h") == 0) {
        return cmd_z2h(argc, argv);
    }
    if (strcmp(sub, "save") == 0) {
        return cmd_save(argc, argv);
    }
    if (strcmp(sub, "settings") == 0) {
        print_settings(settings_get());
        return 0;
    }
    if (strcmp(sub, "corrupt") == 0) {
        return cmd_corrupt();
    }
    if (strcmp(sub, "latency") == 0) {
        return cmd_latency();
    }
    if (strcmp(sub, "fail_source") == 0) {
        fail_source_flag = FAIL_SOURCE_MAGIC;
        esp_restart();
    }
    if (strcmp(sub, "panic") == 0) {
        abort(); /* SW-906: el backtrace sale por el puerto serial antes de reiniciar */
    }
    printf("subcomandos: fix, live, z2h, save, settings, corrupt, latency, fail_source, panic\n");
    return 1;
}

esp_err_t bench_start(void)
{
    ESP_RETURN_ON_FALSE(bsp_display_lock(LOCK_TIMEOUT_MS), ESP_ERR_TIMEOUT, TAG, "lock");
    lv_display_add_event_cb(lv_display_get_default(), refr_ready_cb, LV_EVENT_REFR_READY, NULL);
    bsp_display_unlock();

    esp_console_repl_t *repl;
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.prompt = "bench>";
    const esp_console_dev_uart_config_t uart_cfg = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_console_new_repl_uart(&uart_cfg, &repl_cfg, &repl), TAG, "repl");

    const esp_console_cmd_t cmd = {
        .command = "bench",
        .help = "Pruebas en la tarjeta: fix rpm= kmh= map= baro= | live | z2h <ms>|off | save "
                "unit|shift|bar_start "
                "| settings | corrupt | latency | fail_source | panic",
        .func = bench_cmd,
    };
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&cmd), TAG, "cmd");
    ESP_RETURN_ON_ERROR(esp_console_start_repl(repl), TAG, "start");
    ESP_LOGW(TAG, "build de banco: consola bench activa, fuente con valores fijos");
    return ESP_OK;
}

#endif
