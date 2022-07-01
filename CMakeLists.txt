cmake_minimum_required(VERSION 3.21)

project(DSPSeedCalc CXX)

set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CustomCompilerOptions)
fix_compile_flags()
fix_release_flags()
add_static_runtime_option()

add_subdirectory(fmt)

add_executable(${PROJECT_NAME} main.cc
    filter.cc filter.hh
    dsp/galaxy.cc dsp/galaxy.hh
    dsp/star.cc dsp/star.hh
    dsp/planet.cc dsp/planet.hh
    dsp/protoset.cc dsp/protoset.hh
    dsp/namegen.cc dsp/namegen.hh
    dsp/vectors.hh
    util/dotnet35random.cc util/dotnet35random.hh
    util/maths.hh util/mempool.hh
    )
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} fmt::fmt)
if(WIN32)
    add_subdirectory(getopt)
    add_subdirectory(dlfcn-win32)
    target_link_libraries(${PROJECT_NAME} wingetopt dl)
else()
    target_link_libraries(${PROJECT_NAME} ${CMAKE_DL_LIBS})
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_subdirectory(filters)
