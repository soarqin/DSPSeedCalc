if(NOT DEFINED VIEWER_SOURCE_DIR OR NOT DEFINED VIEWER_RUNTIME_DIR OR NOT DEFINED VIEWER_STAGE_ENTRIES)
    message(FATAL_ERROR "Viewer staging paths are required")
endif()
string(REPLACE "," ";" VIEWER_STAGE_ENTRIES "${VIEWER_STAGE_ENTRIES}")

file(REMOVE_RECURSE
    "${VIEWER_RUNTIME_DIR}/assets"
    "${VIEWER_RUNTIME_DIR}/Prototypes")
file(REMOVE "${VIEWER_RUNTIME_DIR}/viewer-font.ttf")
file(MAKE_DIRECTORY
    "${VIEWER_RUNTIME_DIR}/assets"
    "${VIEWER_RUNTIME_DIR}/assets/shaders"
    "${VIEWER_RUNTIME_DIR}/Prototypes")
foreach(entry IN LISTS VIEWER_STAGE_ENTRIES)
    string(REPLACE "|" ";" fields "${entry}")
    list(GET fields 0 source)
    list(GET fields 1 runtime)
    set(source_path "${VIEWER_SOURCE_DIR}/${source}")
    set(runtime_path "${VIEWER_RUNTIME_DIR}/${runtime}")
    if(IS_DIRECTORY "${source_path}")
        file(MAKE_DIRECTORY "${runtime_path}")
        file(COPY "${source_path}/" DESTINATION "${runtime_path}")
    else()
        get_filename_component(runtime_directory "${runtime_path}" DIRECTORY)
        file(MAKE_DIRECTORY "${runtime_directory}")
        file(COPY_FILE "${source_path}" "${runtime_path}" ONLY_IF_DIFFERENT)
    endif()
endforeach()

if(DEFINED VIEWER_FONT AND NOT VIEWER_FONT STREQUAL "")
    file(COPY_FILE "${VIEWER_FONT}"
        "${VIEWER_RUNTIME_DIR}/viewer-font.ttf"
        ONLY_IF_DIFFERENT)
endif()
