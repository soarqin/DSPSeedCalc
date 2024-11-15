macro(fix_compile_flags)
    add_compile_options($<$<OR:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>:/utf-8>)
endmacro()

include(CMakeParseArguments)

macro(fix_release_flags)
    cmake_parse_arguments(default "" "STRIP_BINARY;USE_LTO;USE_STATIC_CRT" "" ${ARGN})
    option(RELEASE_STRIP_BINARY "Strip binaries for release configs" ${default_STRIP_BINARY})
    option(RELEASE_USE_LTO "Use link-time optimization for release configs" ${default_USE_LTO})
    option(RELEASE_USE_STATIC_CRT "Use static C/C++ runtime for release configs" ${default_USE_STATIC_CRT})

    if (RELEASE_STRIP_BINARY)
        add_link_options(
            $<$<AND:$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>>,$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>>:-s>
        )
    endif ()
    if (RELEASE_USE_LTO)
        add_compile_options(
            $<$<AND:$<C_COMPILER_ID:MSVC>,$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>>:/GL>
        )
        add_compile_options(
            $<$<AND:$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>>,$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>>:-flto>
        )
        add_link_options(
            $<$<AND:$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>>,$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>>:-flto>
            $<$<AND:$<C_COMPILER_ID:MSVC>,$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>>:/LTCG>
        )
    endif ()
    if (RELEASE_USE_STATIC_CRT)
        add_link_options(
            $<$<AND:$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>>,$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>>:-static>
        )
        add_compile_options(
            $<$<AND:$<C_COMPILER_ID:GNU>,$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>>:-static-libgcc\;-static-libstdc++>
        )
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif ()
endmacro()
