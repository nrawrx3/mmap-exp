set(CMAKE_VERBOSE_MAKEFILE ON)

add_executable(test_map_large_file test_map_large_file.cpp util.hpp command.hpp)
target_link_libraries(test_map_large_file
        PUBLIC mmapext plog linenoise scaffold)
