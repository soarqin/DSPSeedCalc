set(CMAKE_WARN_DEPRECATED OFF CACHE INTERNAL "")

cmake_minimum_required(VERSION 3.21)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(GlobalOptions)
add_global_options()
include(CustomCompilerOptions)
fix_compile_flags()
fix_release_flags(STRIP_BINARY ON USE_LTO ON USE_STATIC_CRT ON)
include(ProjectMacros)

option(BUILD_VIEWER "Build viewer application" OFF)

project(DSPSeedCalc CXX)

add_subdirectory(dspugen)

add_project(${PROJECT_NAME} EXECUTABLE
    main.cc filter.cc filter.hh
    FOLDER "cli"
    LANGUAGES CXX)

target_link_libraries(${PROJECT_NAME} dspugen)
if(WIN32)
    add_subdirectory(getopt)
    add_subdirectory(dlfcn-win32)
    target_link_libraries(${PROJECT_NAME} wingetopt dl)
else()
    target_link_libraries(${PROJECT_NAME} ${CMAKE_DL_LIBS})
endif()

add_subdirectory(filters)

if(BUILD_VIEWER)
    add_subdirectory(viewer)
endif()
