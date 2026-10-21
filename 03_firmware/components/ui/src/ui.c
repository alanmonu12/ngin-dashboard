#include "ui.h"

#include <math.h>
#include <stdio.h>

#include "esp_check.h"
#include "esp_log.h"
#include "lvgl.h"

#include "bsp.h"
#include "vehicle_boost.h"

static const char *TAG = "ui";

#define LOCK_TIMEOUT_MS 1000
#define REFRESH_MS      20 /* 50 Hz: la espera a la copia más el dibujo cabe en los 50 ms de SW-501 */
/* Alarma_Cambio a 4.2 Hz (4–8 Hz en el glosario), múltiplo del refresco. Más lento deja ver menos el tearing:
 * el panel no expone su señal TE a ningún GPIO (probado 2026-10-21) y no se puede sincronizar con su barrido.
 */
#define BLINK_HALF_MS           120
#define BOOST_ARC_STEPS_PER_BAR 100 /* el arco avanza en centésimas de bar */
#define BOOST_ARC_MAX           150 /* escala de 0 a 1.5 bar */
#define SCREEN_ANIM_MS          150

/* Barra_RPM como fila de LEDs, al estilo de los shift lights de tablero o volante. */
#define LED_COUNT        15
#define LED_GREEN_COUNT  6
#define LED_YELLOW_COUNT 5 /* el resto, rojos */
#define LED_DIAMETER     24
#define LED_MARGIN_X     16
#define LED_Y            28
#define LED_OFF_MIX      45 /* brillo del LED apagado, de 255 */

#define COLOR_BG     lv_color_black()
#define COLOR_TEXT   lv_color_white()
#define COLOR_QUIET  lv_color_hex(0x8a8a8a)
#define COLOR_GREEN  lv_color_hex(0x2ecc71)
#define COLOR_YELLOW lv_color_hex(0xf1c40f)
#define COLOR_RED    lv_color_hex(0xe74c3c)
#define COLOR_TRACK  lv_color_hex(0x303030) /* fondo del arco */

enum { SCREEN_BOOST = 0, SCREEN_RPM, SCREEN_Z2H, SCREEN_COUNT };

static struct {
    ui_config_t cfg;
    lv_obj_t *screens[SCREEN_COUNT];
    int active;
    lv_obj_t *error_label;

    lv_obj_t *boost_arc, *boost_value, *boost_unit, *boost_peak;
    lv_obj_t *led_row;
    lv_obj_t *leds[LED_COUNT];
    lv_color_t led_on[LED_COUNT], led_off[LED_COUNT];
    lv_obj_t *rpm_gear, *rpm_value, *rpm_speed;
    lv_obj_t *z2h_state, *z2h_time;

    /* Último valor dibujado: solo se redibuja lo que cambia, el SPI es el cuello de botella. */
    int last_boost, last_peak, last_arc, last_rpm, last_speed, last_gear, last_z2h_ms;
    lv_color_t last_led_color[LED_COUNT];
    bool led_cache_valid;
    vehicle_z2h_state_t last_z2h_state;
#if CONFIG_APP_BENCH
    /* Build de banco: tiempos del último cambio de pantalla (SW-505 a SW-507), medidos desde el gesto. */
    uint32_t switch_tick;
    bool switch_values_pending, switch_loading;
#endif
} ui;

static int decimal_scale(int decimals)
{
    return decimals == 2 ? 100 : (decimals == 1 ? 10 : 1);
}

/* Formatea con decimales fijos sin printf de punto flotante. */
static void fmt_fixed(char *buf, size_t len, float v, int decimals)
{
    int scale = decimal_scale(decimals);
    long n = lroundf(v * (float)scale);
    if (decimals == 0) {
        snprintf(buf, len, "%ld", n);
    } else {
        snprintf(buf, len, "%ld.%0*ld", n / scale, decimals, labs(n % scale));
    }
}

static lv_obj_t *make_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    lv_obj_set_style_text_color(scr, COLOR_TEXT, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    return scr;
}

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_text(l, text);
    return l;
}

static lv_color_t led_on_color(int i)
{
    if (i < LED_GREEN_COUNT) {
        return COLOR_GREEN;
    }
    return i < LED_GREEN_COUNT + LED_YELLOW_COUNT ? COLOR_YELLOW : COLOR_RED;
}

static void build_boost(lv_obj_t *scr)
{
    lv_obj_align(make_label(scr, &lv_font_montserrat_20, COLOR_QUIET, "BOOST"), LV_ALIGN_TOP_LEFT, 16, 12);

    ui.boost_arc = lv_arc_create(scr);
    lv_obj_set_size(ui.boost_arc, 280, 280);
    lv_obj_align(ui.boost_arc, LV_ALIGN_CENTER, 0, 24);
    lv_arc_set_rotation(ui.boost_arc, 135);
    lv_arc_set_bg_angles(ui.boost_arc, 0, 270);
    lv_arc_set_range(ui.boost_arc, 0, BOOST_ARC_MAX);
    lv_arc_set_value(ui.boost_arc, 0);
    lv_obj_remove_style(ui.boost_arc, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(ui.boost_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(ui.boost_arc, 18, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui.boost_arc, 18, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui.boost_arc, COLOR_TRACK, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ui.boost_arc, COLOR_GREEN, LV_PART_INDICATOR);

    ui.boost_value = make_label(scr, &lv_font_montserrat_48, COLOR_TEXT, "0.00");
    lv_obj_align(ui.boost_value, LV_ALIGN_CENTER, 0, 10);
    ui.boost_unit = make_label(scr, &lv_font_montserrat_20, COLOR_QUIET, vehicle_unit_name(ui.cfg.unit));
    lv_obj_align(ui.boost_unit, LV_ALIGN_CENTER, 0, 54);

    ui.boost_peak = make_label(scr, &lv_font_montserrat_20, COLOR_QUIET, "Pico 0.00");
    lv_obj_align(ui.boost_peak, LV_ALIGN_BOTTOM_MID, 0, -12);
}

static void build_rpm(lv_obj_t *scr)
{
    /* Una fila que ocupa el ancho de la pantalla: el reparto lo hace flex, sin depender de la resolución. */
    ui.led_row = lv_obj_create(scr);
    lv_obj_remove_style_all(ui.led_row);
    lv_obj_set_size(ui.led_row, lv_pct(100), LED_DIAMETER);
    lv_obj_set_pos(ui.led_row, 0, LED_Y);
    lv_obj_set_style_pad_hor(ui.led_row, LED_MARGIN_X, 0);
    lv_obj_set_flex_flow(ui.led_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui.led_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(ui.led_row, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    /* Objetos simples con radio de círculo: se dibujan directo, sin capas intermedias. */
    for (int i = 0; i < LED_COUNT; i++) {
        ui.led_on[i] = led_on_color(i);
        ui.led_off[i] = lv_color_mix(ui.led_on[i], lv_color_black(), LED_OFF_MIX);
        lv_obj_t *led = lv_obj_create(ui.led_row);
        lv_obj_remove_style_all(led);
        lv_obj_set_size(led, LED_DIAMETER, LED_DIAMETER);
        lv_obj_set_style_radius(led, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(led, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(led, ui.led_off[i], 0);
        lv_obj_remove_flag(led, LV_OBJ_FLAG_CLICKABLE);
        ui.leds[i] = led;
    }

    lv_obj_align(make_label(scr, &lv_font_montserrat_20, COLOR_QUIET, "MARCHA"), LV_ALIGN_LEFT_MID, 40, -10);
    ui.rpm_gear = make_label(scr, &lv_font_montserrat_48, COLOR_TEXT, "-");
    lv_obj_align(ui.rpm_gear, LV_ALIGN_LEFT_MID, 70, 40);

    ui.rpm_value = make_label(scr, &lv_font_montserrat_48, COLOR_TEXT, "0");
    lv_obj_align(ui.rpm_value, LV_ALIGN_RIGHT_MID, -40, 0);
    lv_obj_align(make_label(scr, &lv_font_montserrat_20, COLOR_QUIET, "rpm"), LV_ALIGN_RIGHT_MID, -40, 44);

    ui.rpm_speed = make_label(scr, &lv_font_montserrat_20, COLOR_QUIET, "0 km/h");
    lv_obj_align(ui.rpm_speed, LV_ALIGN_BOTTOM_MID, 0, -12);
}

static void build_z2h(lv_obj_t *scr)
{
    lv_obj_align(make_label(scr, &lv_font_montserrat_20, COLOR_QUIET, "0 - 100 km/h"), LV_ALIGN_TOP_LEFT, 16,
                 12);
    ui.z2h_time = make_label(scr, &lv_font_montserrat_48, COLOR_TEXT, "0.00 s");
    lv_obj_align(ui.z2h_time, LV_ALIGN_CENTER, 0, -10);
    ui.z2h_state = make_label(scr, &lv_font_montserrat_28, COLOR_QUIET, "");
    lv_obj_align(ui.z2h_state, LV_ALIGN_CENTER, 0, 60);
}

static void refresh_boost(const vehicle_outputs_t *o)
{
    char buf[24];
    int decimals = vehicle_pressure_decimals(ui.cfg.unit);
    float value = vehicle_pressure_from_bar(o->boost_bar, ui.cfg.unit);
    float peak = vehicle_pressure_from_bar(o->boost_peak_bar, ui.cfg.unit);
    /* Las claves van en la precisión que se muestra: un cambio en un dígito oculto no redibuja. */
    int scale = decimal_scale(decimals);
    int value_key = (int)lroundf(value * (float)scale);
    int peak_key = (int)lroundf(peak * (float)scale);
    int arc = (int)lroundf(o->boost_bar * BOOST_ARC_STEPS_PER_BAR);

    if (value_key != ui.last_boost) {
        ui.last_boost = value_key;
        fmt_fixed(buf, sizeof(buf), value, decimals);
        lv_label_set_text(ui.boost_value, buf);
    }
    if (arc != ui.last_arc) {
        ui.last_arc = arc;
        lv_arc_set_value(ui.boost_arc, arc); /* LVGL lo acota al rango del arco */
    }
    if (peak_key != ui.last_peak) {
        ui.last_peak = peak_key;
        char num[12];
        fmt_fixed(num, sizeof(num), peak, decimals);
        snprintf(buf, sizeof(buf), "Pico %s", num);
        lv_label_set_text(ui.boost_peak, buf);
    }
}

static void refresh_rpm(const vehicle_outputs_t *o)
{
    int rpm = (int)lroundf(o->rpm / 10.0f) * 10;
    if (rpm != ui.last_rpm) {
        ui.last_rpm = rpm;
        lv_label_set_text_fmt(ui.rpm_value, "%d", rpm);
    }
    int speed = (int)lroundf(o->speed_kmh);
    if (speed != ui.last_speed) {
        ui.last_speed = speed;
        lv_label_set_text_fmt(ui.rpm_speed, "%d km/h", speed);
    }
    if (o->gear != ui.last_gear) {
        ui.last_gear = o->gear;
        if (o->gear == VEHICLE_GEAR_NONE) {
            lv_label_set_text(ui.rpm_gear, "-");
        } else {
            lv_label_set_text_fmt(ui.rpm_gear, "%d", o->gear);
        }
    }

    /* SYS-18: LEDs encendidos en proporción a las RPM; SYS-05: al cambio parpadean los 15, cada uno en su
     * color. Solo cambia el brillo, no el color: el tearing (el panel no expone TE) se nota menos. */
    int lit = (int)(o->bar_fraction * LED_COUNT);
    bool blink_on = (lv_tick_get() / BLINK_HALF_MS) % 2 == 0;
    lv_color_t want[LED_COUNT];
    int changed = 0;
    for (int i = 0; i < LED_COUNT; i++) {
        if (o->shift_alarm) {
            want[i] = blink_on ? ui.led_on[i] : ui.led_off[i];
        } else {
            want[i] = i < lit ? ui.led_on[i] : ui.led_off[i];
        }
        if (!ui.led_cache_valid || !lv_color_eq(want[i], ui.last_led_color[i])) {
            changed++;
        }
    }
    /* Los LEDs están separados y LVGL no une sus áreas: con varios cambios se invalida la fila entera
     * primero, así hay un solo render y un solo envío por SPI en lugar de uno por LED. */
    if (changed > 1) {
        lv_obj_invalidate(ui.led_row);
    }
    for (int i = 0; changed && i < LED_COUNT; i++) {
        if (!ui.led_cache_valid || !lv_color_eq(want[i], ui.last_led_color[i])) {
            ui.last_led_color[i] = want[i];
            lv_obj_set_style_bg_color(ui.leds[i], want[i], 0);
        }
    }
    ui.led_cache_valid = true;
}

static void refresh_z2h(const vehicle_outputs_t *o)
{
    if (o->z2h_state != ui.last_z2h_state) {
        ui.last_z2h_state = o->z2h_state;
        static const char *const TEXT[] = {
            [VEHICLE_Z2H_IDLE] = "Detente para armar", [VEHICLE_Z2H_ARMED] = "Listo: acelera",
            [VEHICLE_Z2H_RUNNING] = "Midiendo...",     [VEHICLE_Z2H_DONE] = "Resultado",
            [VEHICLE_Z2H_ABORTED] = "Cancelado",
        };
        lv_label_set_text(ui.z2h_state, TEXT[o->z2h_state]);
    }
    int ms = (int)(o->z2h_ms / 10) * 10;
    if (ms != ui.last_z2h_ms) {
        ui.last_z2h_ms = ms;
        lv_label_set_text_fmt(ui.z2h_time, "%d.%02d s", ms / 1000, (ms % 1000) / 10);
    }
}

static void refresh_cb(lv_timer_t *t)
{
    (void)t;
    vehicle_outputs_t o;
    ui.cfg.snapshot(ui.cfg.ctx, &o);
    switch (ui.active) {
    case SCREEN_BOOST:
        refresh_boost(&o);
        break;
    case SCREEN_RPM:
        refresh_rpm(&o);
        break;
    case SCREEN_Z2H:
        refresh_z2h(&o);
        break;
    }
    /* SW-501: dibujar en este mismo ciclo. El temporizador de refresco de LVGL no está sincronizado con este
     * y, si se le espera, suma hasta un periodo más de latencia (se midió 76 ms en el build de banco). */
    lv_refr_now(NULL);
#if CONFIG_APP_BENCH
    if (ui.switch_values_pending) {
        ui.switch_values_pending = false;
        ESP_LOGI(TAG, "valores en la pantalla nueva: %lu ms", (unsigned long)lv_tick_elaps(ui.switch_tick));
    }
#endif
}

static void invalidate_cache(void)
{
    ui.last_boost = ui.last_peak = ui.last_arc = ui.last_rpm = ui.last_speed = ui.last_z2h_ms = -1;
    ui.led_cache_valid = false;
    ui.last_gear = -1;
    ui.last_z2h_state = (vehicle_z2h_state_t)-1;
}

#if CONFIG_APP_BENCH
static void screen_loaded_cb(lv_event_t *e);
#endif

/* Siguiente pantalla del Ciclo_Pantallas con dir = LV_DIR_LEFT, anterior con LV_DIR_RIGHT. */
static void show_next_screen(lv_dir_t dir)
{
    int next;
    lv_screen_load_anim_t anim;
    if (dir == LV_DIR_LEFT) {
        next = (ui.active + 1) % SCREEN_COUNT;
        anim = LV_SCREEN_LOAD_ANIM_MOVE_LEFT;
    } else if (dir == LV_DIR_RIGHT) {
        next = (ui.active + SCREEN_COUNT - 1) % SCREEN_COUNT;
        anim = LV_SCREEN_LOAD_ANIM_MOVE_RIGHT;
    } else {
        return;
    }
    ui.active = next;
    invalidate_cache();
#if CONFIG_APP_BENCH
    ui.switch_tick = lv_tick_get();
    ui.switch_values_pending = true;
    ui.switch_loading = true;
#endif
    lv_screen_load_anim(ui.screens[next], anim, SCREEN_ANIM_MS, 0, false);
}

/* SYS-19: deslizar cambia de pantalla. */
static void gesture_cb(lv_event_t *e)
{
    (void)e;
    show_next_screen(lv_indev_get_gesture_dir(lv_indev_active()));
}

void ui_show_splash(void)
{
    if (!bsp_display_lock(LOCK_TIMEOUT_MS)) {
        return;
    }
    lv_obj_t *scr = make_screen();
    lv_obj_center(make_label(scr, &lv_font_montserrat_28, COLOR_TEXT, "ESP32 CAN Dashboard"));
    lv_screen_load(scr);
    bsp_display_unlock();
}

void ui_show_error(const char *msg)
{
    if (!bsp_display_lock(LOCK_TIMEOUT_MS)) {
        return;
    }
    if (!ui.error_label) {
        ui.error_label = make_label(lv_layer_top(), &lv_font_montserrat_20, COLOR_RED, "");
        lv_obj_align(ui.error_label, LV_ALIGN_TOP_RIGHT, -16, 12);
    }
    lv_label_set_text(ui.error_label, msg ? msg : "");
    bsp_display_unlock();
}

esp_err_t ui_start(const ui_config_t *cfg)
{
    ESP_RETURN_ON_FALSE(cfg && cfg->snapshot, ESP_ERR_INVALID_ARG, TAG, "sin snapshot");
    ESP_RETURN_ON_FALSE(bsp_display_lock(LOCK_TIMEOUT_MS), ESP_ERR_TIMEOUT, TAG, "lock");

    ui.cfg = *cfg;
    lv_obj_t *splash = lv_screen_active();
    for (int i = 0; i < SCREEN_COUNT; i++) {
        ui.screens[i] = make_screen();
        lv_obj_add_event_cb(ui.screens[i], gesture_cb, LV_EVENT_GESTURE, NULL);
#if CONFIG_APP_BENCH
        lv_obj_add_event_cb(ui.screens[i], screen_loaded_cb, LV_EVENT_SCREEN_LOADED, NULL);
#endif
    }
    build_boost(ui.screens[SCREEN_BOOST]);
    build_rpm(ui.screens[SCREEN_RPM]);
    build_z2h(ui.screens[SCREEN_Z2H]);

    ui.active = SCREEN_BOOST;
    invalidate_cache();
    lv_screen_load(ui.screens[SCREEN_BOOST]);
    lv_obj_delete(splash);
    lv_timer_create(refresh_cb, REFRESH_MS, NULL);

    bsp_display_unlock();
    return ESP_OK;
}

#if CONFIG_APP_BENCH
static void screen_loaded_cb(lv_event_t *e)
{
    (void)e;
    /* La carga de la primera pantalla en ui_start no viene de un gesto: no se mide. */
    if (!ui.switch_loading) {
        return;
    }
    ui.switch_loading = false;
    ESP_LOGI(TAG, "cambio de pantalla: %lu ms", (unsigned long)lv_tick_elaps(ui.switch_tick));
}

static const char *label_text(const lv_obj_t *label)
{
    return label ? lv_label_get_text(label) : "";
}

/* Una letra por LED_Simulado: G, Y o R según su color encendido; '.' si está apagado. */
static int led_pattern(char *out)
{
    int lit = 0;
    for (int i = 0; i < LED_COUNT; i++) {
        lv_color_t c = lv_obj_get_style_bg_color(ui.leds[i], LV_PART_MAIN);
        if (lv_color_eq(c, ui.led_off[i])) {
            out[i] = '.';
            continue;
        }
        lit++;
        out[i] = lv_color_eq(c, COLOR_GREEN) ? 'G' : (lv_color_eq(c, COLOR_YELLOW) ? 'Y' : 'R');
    }
    out[LED_COUNT] = '\0';
    return lit;
}

static void print_screen(void)
{
    switch (ui.active) {
    case SCREEN_BOOST:
        printf("pantalla=boost valor=\"%s\" unidad=\"%s\" pico=\"%s\" arco=%d/%d\n",
               label_text(ui.boost_value), label_text(ui.boost_unit), label_text(ui.boost_peak),
               (int)lv_arc_get_value(ui.boost_arc), BOOST_ARC_MAX);
        break;
    case SCREEN_RPM: {
        char leds[LED_COUNT + 1];
        int lit = led_pattern(leds);
        printf("pantalla=rpm marcha=\"%s\" rpm=\"%s\" velocidad=\"%s\" leds=%s encendidos=%d\n",
               label_text(ui.rpm_gear), label_text(ui.rpm_value), label_text(ui.rpm_speed), leds, lit);
        break;
    }
    case SCREEN_Z2H:
        printf("pantalla=0-100 tiempo=\"%s\" estado=\"%s\"\n", label_text(ui.z2h_time),
               label_text(ui.z2h_state));
        break;
    }
    printf("aviso=\"%s\"\n", label_text(ui.error_label));
}

esp_err_t ui_bench_screen(int dir)
{
    ESP_RETURN_ON_FALSE(bsp_display_lock(LOCK_TIMEOUT_MS), ESP_ERR_TIMEOUT, TAG, "lock");
    if (dir != 0) {
        show_next_screen(dir > 0 ? LV_DIR_LEFT : LV_DIR_RIGHT);
    } else {
        print_screen();
    }
    bsp_display_unlock();
    return ESP_OK;
}
#endif
