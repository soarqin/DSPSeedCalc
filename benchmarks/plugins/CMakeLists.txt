foreach(BENCH_PLUGIN reject_galaxy reject_star reject_planet high_output pose_sink galaxy_digest owned_output)
    add_library(bench_${BENCH_PLUGIN} SHARED ${BENCH_PLUGIN}.cc)
    target_link_libraries(bench_${BENCH_PLUGIN} PRIVATE fmt::fmt dsp_plugin_abi)
    set_target_properties(bench_${BENCH_PLUGIN} PROPERTIES
        PREFIX ""
        OUTPUT_NAME ${BENCH_PLUGIN}
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/benchmark-plugins
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/benchmark-plugins)
endforeach()
