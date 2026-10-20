# Todas las tarjetas exponen el mismo componente "bsp" con la misma API (include/bsp.h).
idf_component_register(
    SRCS "src/bsp.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio
    PRIV_REQUIRES lvgl__lvgl esp_lcd esp_driver_spi esp_driver_i2c espressif__esp_lvgl_port
                  espressif__esp_lcd_st7796 espressif__esp_lcd_touch_ft5x06
)
