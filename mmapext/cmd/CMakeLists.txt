set(CMAKE_VERBOSE_MAKEFILE ON)

if(gcc_or_clang)
  add_compile_options(-Wall -march=native -fmax-errors=1)
else()
  add_compile_options(-Wall)
endif()

find_package(Catch2 REQUIRED)

add_subdirectory(mmapext_example)
add_subdirectory(test_map_large_file)
