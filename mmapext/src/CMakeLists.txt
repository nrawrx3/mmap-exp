set(CMAKE_VERBOSE_MAKEFILE ON)

if(gcc_or_clang)
  add_compile_options(-Wall -march=native -fmax-errors=1)
else()
  add_compile_options(-Wall)
endif()

set(header_dir ${PROJECT_SOURCE_DIR}/include/mmapext)
set(header_files_relative
	mmapext.hpp
)

set(library_source_files
	mmapext.cpp
)

add_library(mmapext SHARED ${library_source_files})
target_link_libraries(mmapext plog)

target_compile_definitions(mmapext
	PRIVATE MMAPEXT_API_BEING_BUILT
	PUBLIC MMAPEXT_API_BEING_IMPORTED)

# Allow automatic discovery of the header directories when our lib is linked
# using target_linked_libraries
target_include_directories(mmapext
    PUBLIC
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
)
