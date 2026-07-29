cmake_minimum_required(VERSION 3.28)

set(RAYLIB_VERSION 6.0)
set(RAYLIB_SHA256SUM 2b3ee1e2120c7a0796b33062c7e9a694dd8a8caa56a96319ac8c8ecf54a90d0b)
set(DSP_VIEWER_FONT "" CACHE FILEPATH "External TTF/OTF font for the viewer")

set(DSP_VIEWER_SOURCES
    src/app_resources.cc
    src/app_resources.hh
    src/main.cc
    src/viewer_app.cc
    src/viewer_app.hh
    src/camera_controller.cc
    src/camera_controller.hh
    src/material_catalog.cc
    src/material_catalog.hh
    src/post_process.cc
    src/post_process.hh
    src/render_resources.cc
    src/render_resources.hh
    src/scene_renderer.cc
    src/scene_renderer.hh
    src/viewer_ui.cc
    src/viewer_ui.hh
    src/view_model.cc
    src/view_model.hh
    src/view_model_builder.cc
    src/view_model_builder.hh)
set(DSP_VIEWER_ASSET_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/assets/manifest.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${DSP_VIEWER_ASSET_MANIFEST}")
file(READ "${DSP_VIEWER_ASSET_MANIFEST}" DSP_VIEWER_ASSET_MANIFEST_JSON)
string(JSON DSP_VIEWER_STAGE_COUNT LENGTH "${DSP_VIEWER_ASSET_MANIFEST_JSON}" staging)
math(EXPR DSP_VIEWER_STAGE_LAST "${DSP_VIEWER_STAGE_COUNT} - 1")
if(EMSCRIPTEN)
    set(DSP_VIEWER_PLATFORM web)
else()
    set(DSP_VIEWER_PLATFORM desktop)
endif()
set(DSP_VIEWER_STAGE_FILES "${DSP_VIEWER_ASSET_MANIFEST}")
set(DSP_VIEWER_STAGE_ENTRIES)
foreach(index RANGE ${DSP_VIEWER_STAGE_LAST})
    string(JSON stage_source GET "${DSP_VIEWER_ASSET_MANIFEST_JSON}" staging ${index} source)
    string(JSON stage_runtime GET "${DSP_VIEWER_ASSET_MANIFEST_JSON}" staging ${index} runtime)
    string(JSON stage_platform GET "${DSP_VIEWER_ASSET_MANIFEST_JSON}" staging ${index} platform)
    if(stage_platform STREQUAL "all" OR stage_platform STREQUAL DSP_VIEWER_PLATFORM)
        set(stage_source_path "${CMAKE_CURRENT_SOURCE_DIR}/${stage_source}")
        if(IS_DIRECTORY "${stage_source_path}")
            file(GLOB_RECURSE stage_files CONFIGURE_DEPENDS LIST_DIRECTORIES false "${stage_source_path}/*")
            list(APPEND DSP_VIEWER_STAGE_FILES ${stage_files})
        else()
            list(APPEND DSP_VIEWER_STAGE_FILES "${stage_source_path}")
        endif()
        list(APPEND DSP_VIEWER_STAGE_ENTRIES "${stage_source}|${stage_runtime}")
    endif()
endforeach()
string(REPLACE ";" "," DSP_VIEWER_STAGE_ENTRIES_ARG "${DSP_VIEWER_STAGE_ENTRIES}")
set(DSP_VIEWER_STAGE_INVENTORY "${CMAKE_CURRENT_BINARY_DIR}/viewer-assets.inventory")
string(JOIN "\n" DSP_VIEWER_STAGE_INVENTORY_CONTENT ${DSP_VIEWER_STAGE_FILES})
file(CONFIGURE
    OUTPUT "${DSP_VIEWER_STAGE_INVENTORY}"
    CONTENT "${DSP_VIEWER_STAGE_INVENTORY_CONTENT}\n"
    @ONLY)

add_project(viewer EXECUTABLE
    ${DSP_VIEWER_SOURCES}
    ${DSP_VIEWER_STAGE_FILES}
    "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/galaxy_line_texture.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/starmap_orbit_disk_texture.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_assets.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_shader_pairs.py"
    LANGUAGES CXX
    FOLDER "gui"
    OUTPUT_SUBDIR "viewer")

if(DSP_VIEWER_FONT)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DSP_VIEWER_DEFAULT_FONT="viewer-font.ttf")
endif()

set(DSP_VIEWER_RUNTIME_DIR "${CMAKE_BINARY_DIR}/bin/viewer")
set(DSP_VIEWER_ASSET_STAMP "${CMAKE_CURRENT_BINARY_DIR}/viewer-assets.stamp")
set(DSP_VIEWER_STAGE_COMMAND
    ${CMAKE_COMMAND}
    "-DVIEWER_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
    "-DVIEWER_RUNTIME_DIR=${DSP_VIEWER_RUNTIME_DIR}"
    "-DVIEWER_STAGE_ENTRIES=${DSP_VIEWER_STAGE_ENTRIES_ARG}")
if(DSP_VIEWER_FONT)
    list(APPEND DSP_VIEWER_STAGE_COMMAND "-DVIEWER_FONT=${DSP_VIEWER_FONT}")
endif()
list(APPEND DSP_VIEWER_STAGE_COMMAND
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/StageViewerAssets.cmake")

set(DSP_VIEWER_STAGE_DEPENDS
    ${DSP_VIEWER_STAGE_FILES}
    "${DSP_VIEWER_STAGE_INVENTORY}"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/StageViewerAssets.cmake")
if(DSP_VIEWER_FONT)
    list(APPEND DSP_VIEWER_STAGE_DEPENDS "${DSP_VIEWER_FONT}")
endif()
add_custom_command(
    OUTPUT "${DSP_VIEWER_ASSET_STAMP}"
    COMMAND ${DSP_VIEWER_STAGE_COMMAND}
    COMMAND ${CMAKE_COMMAND} -E touch "${DSP_VIEWER_ASSET_STAMP}"
    DEPENDS ${DSP_VIEWER_STAGE_DEPENDS}
    VERBATIM)
add_custom_target(viewer_assets DEPENDS "${DSP_VIEWER_ASSET_STAMP}")
add_dependencies(${PROJECT_NAME} viewer_assets)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_custom_target(verify_viewer_assets
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_assets.py"
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_shader_pairs.py"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        VERBATIM)
endif()

if(EMSCRIPTEN)
    set(PLATFORM Web CACHE STRING "raylib target platform" FORCE)
    set(GRAPHICS GRAPHICS_API_OPENGL_ES3 CACHE STRING "raylib graphics API" FORCE)
    set(raylib_FOUND FALSE)
else()
    find_package(raylib ${RAYLIB_VERSION} QUIET)
endif()
if(NOT raylib_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        raylib
        DOWNLOAD_EXTRACT_TIMESTAMP OFF
        URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz
        URL_HASH SHA256=${RAYLIB_SHA256SUM}
        EXCLUDE_FROM_ALL)
    set(BUILD_EXAMPLES OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(raylib)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE dspugen raylib)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()

if(EMSCRIPTEN)
    target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_WEB)
    target_link_options(${PROJECT_NAME} PRIVATE
        "-sUSE_GLFW=3"
        "-sWASM=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "--preload-file=${DSP_VIEWER_RUNTIME_DIR}/assets@/assets"
        "--preload-file=${DSP_VIEWER_RUNTIME_DIR}/Prototypes@/Prototypes"
        "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")
    if(DSP_VIEWER_FONT)
        target_link_options(${PROJECT_NAME} PRIVATE
            "--preload-file=${DSP_VIEWER_RUNTIME_DIR}/viewer-font.ttf@/viewer-font.ttf")
    endif()
    set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_NAME "index"
        SUFFIX ".html")
    set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS
        "${DSP_VIEWER_ASSET_STAMP}"
        "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")
endif()
