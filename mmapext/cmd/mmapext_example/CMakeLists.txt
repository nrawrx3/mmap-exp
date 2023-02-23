set(CMAKE_VERBOSE_MAKEFILE ON)

add_executable(mmapext_example mmapext_example.cpp)
target_link_libraries(mmapext_example PUBLIC mmapext plog linenoise scaffold)
