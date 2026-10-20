#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pantalla en horizontal; el panel ST7796S es nativo 320×480 en vertical. */
#define BSP_LCD_H_RES 480
#define BSP_LCD_V_RES 320

/* Pines del CAN en el header de 2×20 (pines 6 y 5). */
#define BSP_CAN_TX GPIO_NUM_32
#define BSP_CAN_RX GPIO_NUM_33

/* Inicializa LCD, touch, backlight y LVGL. */
esp_err_t bsp_display_start(void);

/* LVGL solo se toca con este lock tomado (fuera de los callbacks de LVGL). */
bool bsp_display_lock(uint32_t timeout_ms);
void bsp_display_unlock(void);

#ifdef __cplusplus
}
#endif
