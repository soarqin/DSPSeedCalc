# Plugin ABI contract shared by the host, the filters and the benchmark plugins.
# Header-only on purpose: plugins are SHARED libraries, and linking the static
# dspugen into each one would give every plugin its own copy of the Prototype
# globals. Only the include paths may propagate.
add_library(dsp_plugin_abi INTERFACE)
target_include_directories(dsp_plugin_abi INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/dspugen")

# Plugins inline vectors.hh and util/maths.hh, so they must honour the same strict
# floating-point contract as dspugen; they cannot inherit it by linking, because
# linking the static library would duplicate the Prototype globals per plugin.
if(MSVC)
    target_compile_options(dsp_plugin_abi INTERFACE /fp:strict)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(dsp_plugin_abi INTERFACE -fno-fast-math -ffp-contract=off)
endif()

add_project(DSPSeedCalc EXECUTABLE
    main.cc filter.cc filter.hh
    FOLDER "cli"
    LANGUAGES CXX)

target_link_libraries(DSPSeedCalc dspugen)
target_compile_features(DSPSeedCalc PRIVATE cxx_std_17)

if(WIN32)
    target_link_libraries(DSPSeedCalc wingetopt dl)
else()
    target_link_libraries(DSPSeedCalc ${CMAKE_DL_LIBS})
endif()
