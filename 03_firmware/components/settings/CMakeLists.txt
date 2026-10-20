idf_component_register(
    SRCS "src/settings.c"
    INCLUDE_DIRS "include"
    REQUIRES vehicle_logic
    PRIV_REQUIRES nvs_flash
)
