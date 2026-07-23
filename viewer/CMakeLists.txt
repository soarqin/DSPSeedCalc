cmake_minimum_required(VERSION 3.28)
add_project(viewer WIN32
    viewer.cc
    LANGUAGES CXX
    FOLDER "gui")

set(RAYLIB_VERSION 6.0)
set(RAYLIB_SHA256SUM 2b3ee1e2120c7a0796b33062c7e9a694dd8a8caa56a96319ac8c8ecf54a90d0b)
find_package(raylib ${RAYLIB_VERSION} QUIET)
if (NOT raylib_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        raylib
        DOWNLOAD_EXTRACT_TIMESTAMP OFF
        URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz
        URL_HASH SHA256=${RAYLIB_SHA256SUM}
        EXCLUDE_FROM_ALL
    )
    set(BUILD_EXAMPLES OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(raylib)
endif()

target_link_libraries(${PROJECT_NAME} dspugen raylib)
if (APPLE)
    target_link_libraries(${PROJECT_NAME} "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()
