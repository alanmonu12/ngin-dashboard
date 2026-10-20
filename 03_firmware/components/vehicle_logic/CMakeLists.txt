# C puro: no debe depender de ningún componente de ESP-IDF, así compila también en host (test/host/).
idf_component_register(
    SRCS "src/vehicle_boost.c" "src/vehicle_gear.c" "src/vehicle_shift.c" "src/vehicle_z2h.c" "src/vehicle.c"
    INCLUDE_DIRS "include"
)
