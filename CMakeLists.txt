cmake_minimum_required(VERSION 3.21)
project(DSPSeedCalc)

set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(CustomCompilerOptions)
fix_compile_flags()
fix_release_flags()
add_static_runtime_option()

add_executable(DSPSeedCalc main.cc
    dsp/galaxy.cc dsp/galaxy.hh
    dsp/star.cc dsp/star.hh
    dsp/planet.cc dsp/planet.hh
    dsp/protoset.cc dsp/protoset.hh
    dsp/vectors.hh
    util/dotnet35random.cc util/dotnet35random.hh
    util/maths.hh
    )
add_subdirectory(fmt)
target_include_directories(DSPSeedCalc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DSPSeedCalc fmt::fmt)

if(WIN32)
    add_subdirectory(getopt)
    target_link_libraries(DSPSeedCalc wingetopt)
endif()
