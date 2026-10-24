cmake_minimum_required(VERSION 3.22)

# Build de banco: otra aplicación con los mismos componentes y bsp que la del auto, más la consola bench.
#   idf.py -C test/bench build      (desde 03_firmware)
set(FW_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")
set(BOARD "wt32sc01" CACHE STRING "Tarjeta: subcarpeta de bsp/")
if(NOT EXISTS "${FW_DIR}/bsp/${BOARD}/bsp/CMakeLists.txt")
    message(FATAL_ERROR "No existe bsp/${BOARD}/bsp")
endif()
set(EXTRA_COMPONENT_DIRS "${FW_DIR}/components" "${FW_DIR}/bsp/${BOARD}")

# La misma configuración base que el auto; el sdkconfig se genera en build/ y no se versiona.
set(SDKCONFIG_DEFAULTS "${FW_DIR}/sdkconfig.defaults")
set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_dashboard_bench)
