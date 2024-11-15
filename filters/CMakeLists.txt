project(filters)

file(GLOB FILTER_SRC_FILES *.cc)
foreach(FILTER_SRC ${FILTER_SRC_FILES})
    get_filename_component(FILTER_PROJ ${FILTER_SRC} NAME_WE)
    add_project(${FILTER_PROJ} SHARED ${FILTER_SRC} INLINE_TARGET FOLDER "cli/filters" OUTPUT_SUBDIR filters)
    target_link_libraries(${FILTER_PROJ} PRIVATE fmt::fmt)
    target_include_directories(${FILTER_PROJ} PRIVATE ..)
    set_target_properties(${FILTER_PROJ} PROPERTIES
        PREFIX ""
        FOLDER "cli/filters")
endforeach()
