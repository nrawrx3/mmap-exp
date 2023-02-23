cmake_minimum_required(VERSION 3.4)

# Compile flags
if(msvc)
  add_compile_options("-Wall")
    add_compile_options(/W1 /wd4100 /wd4201 /wd4189 /wd4514 /wd4820 /wd4710 /wd4571 /wd5027 /wd5045 /wd4625 /wd4626 /wd5026 /wd4623)
    add_compile_options(/wd4365)
    add_compile_options(/wd4324)
    add_compile_options(/wd4774)
    add_compile_options(/wd4996)
    add_compile_options(/wd5039)
    add_compile_options(/wd4668)
    add_compile_options(/wd4127)
    add_compile_options(/wd4061)
    add_compile_options(/wd4200)
else()
  add_compile_options("-Wall" "-Wextra" "-Werror" "-Wno-implicit-fallthrough")
endif()

# sanitizer
option(USE_MEMORY_SANITIZER "use memory sanitizer" off)
if(USE_MEMORY_SANITIZER)
  add_compile_options("-fsanitize=memory")
endif()

file(GLOB SOURCE_FILES
          *.cpp
          ${PROJECT_SOURCE_DIR}/include/scaffold/*.h
          ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

if (${BUILD_SHARED_LIBS})
  message("Building scaffold as a shared library")
  if (${MSVC})
    add_compile_options("-DSCAFFOLD_API=__declspec(dllexport)")
  else()
    add_compile_options("-DSCAFFOLD_API=__attribute__((visibility(\"default\")))")
  endif()

  add_library(scaffold SHARED ${SOURCE_FILES})
else()
  add_library(scaffold ${SOURCE_FILES})
endif()

# Allow automatic discovery of the header directories when our lib is linked
# using target_linked_libraries
target_include_directories(scaffold
    PUBLIC
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
)

if (BUILD_SHARED_LIBS)
  target_compile_definitions(scaffold PRIVATE SCAFFOLD_API_EXPORT INTERFACE SCAFFOLD_API_IMPORT)
endif()

if (${SCAFFOLD_USE_ASAN})
  target_link_libraries(scaffold -lasan)
endif()
