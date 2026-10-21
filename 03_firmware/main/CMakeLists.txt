# bench.c solo compila su contenido con CONFIG_APP_BENCH (build de banco).
idf_component_register(
    SRCS "app_main.c" "bench.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES bsp data_source settings ui vehicle_logic console esp_timer lvgl__lvgl
)
