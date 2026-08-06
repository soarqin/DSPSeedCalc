add_project(dspugen STATIC
    galaxy.cc galaxy.hh
    star.cc star.hh
    planet.cc planet.hh
    vein/vein_amounts.hh
    protoset.cc protoset.hh
    namegen.cc namegen.hh
    settings.hh
    vectors.hh
    util/dotnet35random.cc util/dotnet35random.hh
    util/instrumentation.cc util/instrumentation.hh
    util/maths.hh util/mempool.hh
    LANGUAGES CXX
    FOLDER "lib"
)

add_subdirectory(fmt)

target_include_directories(dspugen PUBLIC .)
target_link_libraries(dspugen fmt::fmt)
target_compile_features(dspugen PUBLIC cxx_std_17)

if(DSP_ENABLE_INSTRUMENTATION)
    target_compile_definitions(dspugen PUBLIC DSP_ENABLE_INSTRUMENTATION=1)
endif()

# PUBLIC, not PRIVATE: util/maths.hh and vectors.hh are inline headers, so every
# consumer compiles parity-sensitive arithmetic into its own translation unit and
# needs the same strict floating-point contract. Plugins do not link dspugen; they
# get these options from dsp_plugin_abi in cli/CMakeLists.txt.
if(MSVC)
    target_compile_options(dspugen PUBLIC /fp:strict)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(dspugen PUBLIC -fno-fast-math -ffp-contract=off)
endif()
