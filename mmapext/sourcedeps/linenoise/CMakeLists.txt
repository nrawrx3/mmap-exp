cmake_minimum_required(VERSION 3.4)
project(linenoise)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(linenoise linenoise.c linenoise.h)

target_include_directories(linenoise
    PUBLIC
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
)
