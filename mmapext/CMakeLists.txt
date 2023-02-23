# --- BASIC
cmake_minimum_required(VERSION 3.4)
project(mmapext)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
include(extra_functions)
ex_set_gcc_or_clang()

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/)

set(MSVC_INCREMENTAL_DEFAULT OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ----- OPTIONS
set(BRIDS_DATA_DIR_PATH "" CACHE PATH "Path to the data directory")

# ----- DEPS HEADER DIRS -----

set(sourcedeps_directory "${PROJECT_SOURCE_DIR}/sourcedeps/")

set(sourcedeps_relative_include_dirs
  scaffold/include
  plog/include
  )

ex_prepend_to_each("${sourcedeps_relative_include_dirs}" "${sourcedeps_directory}"
                   all_sourcedeps_include_dirs)

# ----- DEPS SOURCES -----
add_subdirectory(sourcedeps/scaffold)

add_subdirectory(sourcedeps/plog)
add_subdirectory(sourcedeps/linenoise)

add_subdirectory(src)
add_subdirectory(cmd)
