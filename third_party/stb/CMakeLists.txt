cmake_minimum_required(VERSION 3.15...4.0)

file(GLOB MYLIB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_library(stb INTERFACE ${MYLIB_SOURCES})

target_include_directories(stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})