cmake_minimum_required(VERSION 3.28)
if(EMSCRIPTEN)
    add_project(viewer EXECUTABLE
        viewer.cc
        web_shell.html
        star_surface_web.vs
        star_surface_web.fs
        star_spot_web.vs
        star_spot_web.fs
        star_black_web.vs
        star_black_web.fs
        galaxy_star_point_web.vs
        galaxy_star_point_web.fs
        bloom_web.vs
        bloom_prefilter_web.fs
        bloom_downsample_first_web.fs
        bloom_downsample_web.fs
        bloom_upsample_web.fs
        bloom_composite_web.fs
        LANGUAGES CXX
        FOLDER "gui")
else()
    add_project(viewer EXECUTABLE
        viewer.cc
        star_surface.vs
        star_surface.fs
        star_spot.vs
        star_spot.fs
        star_black.vs
        star_black.fs
        galaxy_star_point.vs
        galaxy_star_point.fs
        bloom.vs
        bloom_prefilter.fs
        bloom_downsample_first.fs
        bloom_downsample.fs
        bloom_upsample.fs
        bloom_composite.fs
        LANGUAGES CXX
        FOLDER "gui")
endif()

set(RAYLIB_VERSION 6.0)
set(RAYLIB_SHA256SUM 2b3ee1e2120c7a0796b33062c7e9a694dd8a8caa56a96319ac8c8ecf54a90d0b)
set(DSP_VIEWER_FONT "" CACHE FILEPATH "External TTF/OTF font for the viewer")
if(DSP_VIEWER_FONT)
    if(EMSCRIPTEN)
        target_compile_definitions(${PROJECT_NAME} PRIVATE DSP_VIEWER_DEFAULT_FONT="/viewer-font.ttf")
    else()
        target_compile_definitions(${PROJECT_NAME} PRIVATE DSP_VIEWER_DEFAULT_FONT="viewer-font.ttf")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DSP_VIEWER_FONT}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/viewer-font.ttf")
    endif()
endif()
if(NOT EMSCRIPTEN)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Prototypes"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/Prototypes/ThemeProtoSet.json"
            "${CMAKE_SOURCE_DIR}/Prototypes/ItemProtoSet.json"
            "${CMAKE_SOURCE_DIR}/Prototypes/VeinProtoSet.json"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Prototypes"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/star_surface.vs"
            "${CMAKE_CURRENT_SOURCE_DIR}/star_surface.fs"
            "${CMAKE_CURRENT_SOURCE_DIR}/star_spot.vs"
            "${CMAKE_CURRENT_SOURCE_DIR}/star_spot.fs"
            "${CMAKE_CURRENT_SOURCE_DIR}/star_black.vs"
            "${CMAKE_CURRENT_SOURCE_DIR}/star_black.fs"
            "${CMAKE_CURRENT_SOURCE_DIR}/galaxy_star_point.vs"
            "${CMAKE_CURRENT_SOURCE_DIR}/galaxy_star_point.fs"
            "${CMAKE_CURRENT_SOURCE_DIR}/bloom.vs"
            "${CMAKE_CURRENT_SOURCE_DIR}/bloom_prefilter.fs"
            "${CMAKE_CURRENT_SOURCE_DIR}/bloom_downsample_first.fs"
            "${CMAKE_CURRENT_SOURCE_DIR}/bloom_downsample.fs"
            "${CMAKE_CURRENT_SOURCE_DIR}/bloom_upsample.fs"
            "${CMAKE_CURRENT_SOURCE_DIR}/bloom_composite.fs"
            "${CMAKE_CURRENT_SOURCE_DIR}/LensDirt00.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/galaxy_select_lut.bin"
            "${CMAKE_CURRENT_SOURCE_DIR}/galaxy_star_point.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/galaxy_star_point_vertices.bin"
            "${CMAKE_CURRENT_SOURCE_DIR}/galaxy_star_point_uv.bin"
            "${CMAKE_CURRENT_SOURCE_DIR}/galaxy_star_point_indices.bin"
            "${CMAKE_CURRENT_SOURCE_DIR}/star_mass_vertices.bin"
            "${CMAKE_CURRENT_SOURCE_DIR}/star_mass_normals.bin"
            "${CMAKE_CURRENT_SOURCE_DIR}/star_mass_indices.bin"
            "${CMAKE_CURRENT_SOURCE_DIR}/star_black_vertices.bin"
            "${CMAKE_CURRENT_SOURCE_DIR}/star_black_indices.bin"
            "${CMAKE_CURRENT_SOURCE_DIR}/star_spot_vertices.bin"
            "${CMAKE_CURRENT_SOURCE_DIR}/star_spot_uv.bin"
            "${CMAKE_CURRENT_SOURCE_DIR}/star_spot_indices.bin"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
endif()
if(EMSCRIPTEN)
    set(PLATFORM Web CACHE STRING "raylib target platform" FORCE)
    set(GRAPHICS GRAPHICS_API_OPENGL_ES3 CACHE STRING "raylib graphics API" FORCE)
    set(raylib_FOUND FALSE)
else()
    find_package(raylib ${RAYLIB_VERSION} QUIET)
endif()
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

if(EMSCRIPTEN)
    target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_WEB)
    target_link_options(${PROJECT_NAME} PRIVATE
        "-sUSE_GLFW=3"
        "-sWASM=1"
        "-sSINGLE_FILE=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "--preload-file=${CMAKE_SOURCE_DIR}/Prototypes@Prototypes"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/star_surface_web.vs@star_surface.vs"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/star_surface_web.fs@star_surface.fs"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/star_spot_web.vs@star_spot.vs"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/star_spot_web.fs@star_spot.fs"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/star_black_web.vs@star_black.vs"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/star_black_web.fs@star_black.fs"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/galaxy_star_point_web.vs@galaxy_star_point.vs"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/galaxy_star_point_web.fs@galaxy_star_point.fs"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/bloom_web.vs@bloom.vs"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/bloom_prefilter_web.fs@bloom_prefilter.fs"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/bloom_downsample_first_web.fs@bloom_downsample_first.fs"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/bloom_downsample_web.fs@bloom_downsample.fs"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/bloom_upsample_web.fs@bloom_upsample.fs"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/bloom_composite_web.fs@bloom_composite.fs"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/LensDirt00.png@LensDirt00.png"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/galaxy_select_lut.bin@galaxy_select_lut.bin"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/galaxy_star_point.png@galaxy_star_point.png"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/galaxy_star_point_vertices.bin@galaxy_star_point_vertices.bin"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/galaxy_star_point_uv.bin@galaxy_star_point_uv.bin"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/galaxy_star_point_indices.bin@galaxy_star_point_indices.bin"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/star_mass_vertices.bin@star_mass_vertices.bin"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/star_mass_normals.bin@star_mass_normals.bin"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/star_mass_indices.bin@star_mass_indices.bin"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/star_black_vertices.bin@star_black_vertices.bin"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/star_black_indices.bin@star_black_indices.bin"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/star_spot_vertices.bin@star_spot_vertices.bin"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/star_spot_uv.bin@star_spot_uv.bin"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/star_spot_indices.bin@star_spot_indices.bin"
        "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web_shell.html")
    if(DSP_VIEWER_FONT)
        target_link_options(${PROJECT_NAME} PRIVATE
            "--preload-file=${DSP_VIEWER_FONT}@/viewer-font.ttf")
    endif()
    set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_NAME "index"
        SUFFIX ".html")
endif()
