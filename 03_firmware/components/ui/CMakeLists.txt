idf_component_register(
    SRCS "src/ui.c"
    INCLUDE_DIRS "include"
    REQUIRES vehicle_logic
    PRIV_REQUIRES bsp lvgl__lvgl
)
