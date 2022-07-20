project(viwer)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(glfw)
add_subdirectory(imgui)

add_executable(${PROJECT_NAME} viewer.cc
    ../dsp/galaxy.cc ../dsp/galaxy.hh
    ../dsp/star.cc ../dsp/star.hh
    ../dsp/planet.cc ../dsp/planet.hh
    ../dsp/protoset.cc ../dsp/protoset.hh
    ../dsp/namegen.cc ../dsp/namegen.hh
    ../dsp/vectors.hh
    ../util/dotnet35random.cc ../util/dotnet35random.hh
    ../util/maths.hh ../util/mempool.hh
    )
target_include_directories(${PROJECT_NAME} PRIVATE ..)
target_link_libraries(${PROJECT_NAME} fmt::fmt imgui glfw opengl32)
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    FOLDER "gui")
