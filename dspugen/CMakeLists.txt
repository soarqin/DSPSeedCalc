project(dspugen CXX)

add_subdirectory(fmt)

add_library(dspugen STATIC
    galaxy.cc galaxy.hh
    star.cc star.hh
    planet.cc planet.hh
    protoset.cc protoset.hh
    namegen.cc namegen.hh
    vectors.hh
    util/dotnet35random.cc util/dotnet35random.hh
    util/maths.hh util/mempool.hh
)

target_include_directories(dspugen PUBLIC .)
target_link_libraries(dspugen fmt::fmt)
