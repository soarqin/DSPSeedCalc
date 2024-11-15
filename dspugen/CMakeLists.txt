add_project(dspugen STATIC
    galaxy.cc galaxy.hh
    star.cc star.hh
    planet.cc planet.hh
    protoset.cc protoset.hh
    namegen.cc namegen.hh
    settings.hh
    vectors.hh
    util/dotnet35random.cc util/dotnet35random.hh
    util/maths.hh util/mempool.hh
    LANGUAGES CXX
    FOLDER "lib"
)

add_subdirectory(fmt)

target_include_directories(dspugen PUBLIC .)
target_link_libraries(dspugen fmt::fmt)
