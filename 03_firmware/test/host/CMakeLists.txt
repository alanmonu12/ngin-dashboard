# Tests en la PC: unitarios e integración, en un solo ejecutable con su propio main.
#   cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure
#   ./build/host_tests -v            todos, con detalle
#   ./build/host_tests -g gear       solo un grupo
cmake_minimum_required(VERSION 3.16)
project(dashboard_host_tests C)

# Unity: por defecto la copia de ESP-IDF; en CI se pasa -DUNITY_DIR con un clon de la misma versión.
set(UNITY_DIR "" CACHE PATH "Raíz de Unity (ThrowTheSwitch/Unity)")
if(NOT UNITY_DIR)
    if(NOT DEFINED ENV{IDF_PATH})
        message(FATAL_ERROR "Sin UNITY_DIR ni IDF_PATH: activa ESP-IDF (get_idf) o pasa -DUNITY_DIR=...")
    endif()
    set(UNITY_DIR "$ENV{IDF_PATH}/components/unity/unity")
endif()
option(COVERAGE "Instrumentar el código de producción con gcov" OFF)
set(FW_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra -Werror)

# Código de producción que se prueba: solo C puro, sin ESP-IDF.
# Todo src/ del componente: un archivo nuevo entra solo, sin mantener dos listas.
file(GLOB VEHICLE_LOGIC_SRCS CONFIGURE_DEPENDS ${FW_DIR}/components/vehicle_logic/src/*.c)
add_library(vehicle_logic STATIC ${VEHICLE_LOGIC_SRCS})
target_include_directories(vehicle_logic PUBLIC ${FW_DIR}/components/vehicle_logic/include)
target_link_libraries(vehicle_logic PUBLIC m)

add_library(sim_model STATIC ${FW_DIR}/components/data_source/src/sim_model.c)
target_include_directories(sim_model PUBLIC ${FW_DIR}/components/data_source/src)
target_link_libraries(sim_model PUBLIC vehicle_logic)

add_library(unity STATIC
    ${UNITY_DIR}/src/unity.c
    ${UNITY_DIR}/extras/fixture/src/unity_fixture.c
    ${UNITY_DIR}/extras/memory/src/unity_memory.c
)
target_include_directories(unity PUBLIC
    ${UNITY_DIR}/src
    ${UNITY_DIR}/extras/fixture/src
    ${UNITY_DIR}/extras/memory/src
)
target_compile_options(unity PRIVATE -w)

add_executable(host_tests
    main.c
    unit/vehicle_logic/test_boost.c
    unit/vehicle_logic/test_gear.c
    unit/vehicle_logic/test_shift.c
    unit/vehicle_logic/test_z2h.c
    unit/vehicle_logic/test_vehicle.c
    integration/test_sim_vehicle.c
)
target_link_libraries(host_tests PRIVATE vehicle_logic sim_model unity)

if(COVERAGE)
    foreach(lib vehicle_logic sim_model)
        target_compile_options(${lib} PRIVATE --coverage -O0)
    endforeach()
    target_link_options(host_tests PRIVATE --coverage)
endif()

# Un test de CTest por grupo, para ver de un vistazo cuál falla.
enable_testing()
foreach(group boost gear shift z2h vehicle sim_vehicle)
    add_test(NAME ${group} COMMAND host_tests -g ${group})
endforeach()
