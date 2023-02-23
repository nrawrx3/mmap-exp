cmake_minimum_required(VERSION 3.4)

add_subdirectory(benchmark)

add_compile_options("-std=c++17" "-Wall")

include_directories(${PROJECT_SOURCE_DIR}/include)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/benchmark/include)

add_executable(hash_bench hash_bench.cpp)
target_link_libraries(hash_bench benchmark scaffold)

add_executable(rbt_bench rbt_bench.cpp)
target_link_libraries(rbt_bench benchmark scaffold)
