idf_component_register(
    SRCS "src/app.c"
    INCLUDE_DIRS "include"
    REQUIRES data_source vehicle_logic
    PRIV_REQUIRES bsp settings ui
)
