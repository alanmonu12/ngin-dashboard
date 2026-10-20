cmake_minimum_required(VERSION 3.22)

# Tarjeta = subcarpeta de bsp/. Cada una trae un componente "bsp" con la misma API.
#   idf.py -DBOARD=<tarjeta> build
set(BOARD "wt32sc01" CACHE STRING "Tarjeta: subcarpeta de bsp/")
if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/bsp/${BOARD}/bsp/CMakeLists.txt")
    message(FATAL_ERROR "No existe bsp/${BOARD}/bsp")
endif()
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/bsp/${BOARD}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_dashboard)
