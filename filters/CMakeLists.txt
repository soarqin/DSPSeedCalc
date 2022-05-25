project(filters)

file(GLOB FILTER_SRC_FILES *.cc)
foreach(FILTER_SRC ${FILTER_SRC_FILES})
    get_filename_component(FILTER_PROJ ${FILTER_SRC} NAME_WE)
    add_library(${FILTER_PROJ} SHARED ${FILTER_SRC})
    target_compile_options(${FILTER_PROJ} PRIVATE -fvisibility=hidden)
    target_link_options(${FILTER_PROJ} PRIVATE -Wl,--exclude-all-symbols)
    target_include_directories(${FILTER_PROJ} PRIVATE ../dsp)
    set_target_properties(${FILTER_PROJ} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/filters
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/filters
        PREFIX "")
endforeach()
