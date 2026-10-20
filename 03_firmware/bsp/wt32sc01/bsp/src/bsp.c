#include "bsp.h"

#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7796.h"
#include "esp_lcd_touch_ft5x06.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

static const char *TAG = "bsp";

/* Pines según la hoja de datos de la WT32-SC01 (Wireless-Tag). */
#define LCD_SPI_HOST  SPI2_HOST
#define LCD_PIN_SCLK  GPIO_NUM_14
#define LCD_PIN_MOSI  GPIO_NUM_13
#define LCD_PIN_CS    GPIO_NUM_15
#define LCD_PIN_DC    GPIO_NUM_21
#define LCD_PIN_RST   GPIO_NUM_22
#define LCD_PIN_BL    GPIO_NUM_23
#define TOUCH_PIN_SDA GPIO_NUM_18
#define TOUCH_PIN_SCL GPIO_NUM_19

/* Los pines son IO_MUX de HSPI: 80 MHz según la hoja de datos de la WT32-SC01. Si hay basura en pantalla,
 * bajar a 40 MHz. */
#define LCD_PIXEL_CLOCK_HZ (80 * 1000 * 1000)

/* Orientación y color: confirmar en la tarjeta y ajustar aquí si la imagen sale girada o con colores
 * invertidos. */
#define LCD_SWAP_XY  true
#define LCD_MIRROR_X false
#define LCD_MIRROR_Y false
#define LCD_INVERT   false
#define LCD_BGR      true

/* Buffers parciales en RAM interna: el DMA de SPI del ESP32 no lee PSRAM. */
#define LVGL_BUF_LINES 40

static esp_err_t lcd_init(esp_lcd_panel_io_handle_t *out_io, esp_lcd_panel_handle_t *out_panel)
{
    const spi_bus_config_t bus = {
        .sclk_io_num = LCD_PIN_SCLK,
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = BSP_LCD_H_RES * LVGL_BUF_LINES * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO), TAG, "spi bus");

    const esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = LCD_PIN_CS,
        .dc_gpio_num = LCD_PIN_DC,
        .spi_mode = 0,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_cfg, out_io),
                        TAG, "panel io");

    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_BGR ? LCD_RGB_ELEMENT_ORDER_BGR : LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7796(*out_io, &panel_cfg, out_panel), TAG, "st7796");

    esp_lcd_panel_handle_t panel = *out_panel;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), TAG, "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), TAG, "init");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(panel, LCD_INVERT), TAG, "invert");
    /* La orientación la aplica lvgl_port_add_disp() con disp_cfg.rotation. */
    return esp_lcd_panel_disp_on_off(panel, true);
}

static esp_err_t backlight_on(void)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << LCD_PIN_BL,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "bl gpio");
    return gpio_set_level(LCD_PIN_BL, 1);
}

static esp_err_t touch_init(esp_lcd_touch_handle_t *out_touch)
{
    const i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = TOUCH_PIN_SDA,
        .scl_io_num = TOUCH_PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &bus), TAG, "i2c bus");

    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    esp_lcd_panel_io_handle_t io;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(bus, &io_cfg, &io), TAG, "touch io");

    /* El touch reporta en la orientación nativa del panel: se aplica la misma rotación. */
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = BSP_LCD_V_RES,
        .y_max = BSP_LCD_H_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .flags =
            {
                .swap_xy = LCD_SWAP_XY,
                .mirror_x = LCD_MIRROR_X,
                .mirror_y = LCD_MIRROR_Y,
            },
    };
    return esp_lcd_touch_new_i2c_ft5x06(io, &tp_cfg, out_touch);
}

esp_err_t bsp_display_start(void)
{
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_handle_t panel;
    ESP_RETURN_ON_ERROR(lcd_init(&io, &panel), TAG, "lcd");
    ESP_RETURN_ON_ERROR(backlight_on(), TAG, "backlight");

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_affinity = 1; /* el core 0 queda para la tarea OBD */
    ESP_RETURN_ON_ERROR(lvgl_port_init(&port_cfg), TAG, "lvgl port");

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = BSP_LCD_H_RES * LVGL_BUF_LINES,
        .double_buffer = true,
        .hres = BSP_LCD_H_RES,
        .vres = BSP_LCD_V_RES,
        .rotation =
            {
                .swap_xy = LCD_SWAP_XY,
                .mirror_x = LCD_MIRROR_X,
                .mirror_y = LCD_MIRROR_Y,
            },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags =
            {
                .buff_dma = true,
                .swap_bytes = true,
            },
    };
    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);
    ESP_RETURN_ON_FALSE(disp, ESP_FAIL, TAG, "lvgl display");

    esp_lcd_touch_handle_t touch;
    esp_err_t err = touch_init(&touch);
    if (err == ESP_OK) {
        const lvgl_port_touch_cfg_t touch_cfg = {.disp = disp, .handle = touch};
        if (!lvgl_port_add_touch(&touch_cfg)) {
            ESP_LOGW(TAG, "no se pudo registrar el touch en LVGL");
        }
    } else {
        /* Sin touch la pantalla sigue siendo útil para ver datos. */
        ESP_LOGW(TAG, "touch no disponible: %s", esp_err_to_name(err));
    }

    return ESP_OK;
}

bool bsp_display_lock(uint32_t timeout_ms)
{
    return lvgl_port_lock(timeout_ms);
}

void bsp_display_unlock(void)
{
    lvgl_port_unlock();
}
