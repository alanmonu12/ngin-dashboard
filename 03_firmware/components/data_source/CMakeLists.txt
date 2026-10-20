idf_component_register(
    SRCS "src/sim_model.c" "src/sim_source.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES vehicle_logic
    PRIV_REQUIRES esp_timer
)
