cmake_minimum_required(VERSION 3.21)

project(DSPSeedCalc CXX)

set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CustomCompilerOptions)
fix_compile_flags()
fix_release_flags()
add_static_runtime_option()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(dspugen)

add_executable(${PROJECT_NAME} main.cc filter.cc filter.hh)
target_link_libraries(${PROJECT_NAME} dspugen)
if(WIN32)
    add_subdirectory(getopt)
    add_subdirectory(dlfcn-win32)
    target_link_libraries(${PROJECT_NAME} wingetopt dl)
else()
    target_link_libraries(${PROJECT_NAME} ${CMAKE_DL_LIBS})
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    FOLDER "cli")

add_subdirectory(filters)

add_subdirectory(viewer)
