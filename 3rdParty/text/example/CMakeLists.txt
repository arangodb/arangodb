include_directories(${CMAKE_HOME_DIRECTORY})

macro(add_sample name)
    add_executable(${name} ${name}.cpp)
    target_compile_options(${name} PRIVATE -Wall)
    target_link_libraries(${name} text boost)
    if (ICU_FOUND)
        target_link_libraries(${name} ${ICU_LIBRARIES})
    endif ()
    set_property(TARGET ${name} PROPERTY CXX_STANDARD ${CXX_STD})
    if (clang_on_linux)
        target_link_libraries(${name} c++)
    endif ()
endmacro()

add_sample(code_point_to_utf8)

add_sample(transcoding)
add_sample(normalize)
add_sample(breaks)
add_sample(case_mapping)
add_sample(collation)
add_sample(collation_search)
add_sample(bidirectional)
add_sample(graphemes)
add_sample(segmented_vector)
add_sample(trie)


if (BUILD_EDITOR)
    find_package(Curses)
    if (NOT CURSES_FOUND)
        message("-- Could not find libcurses; editor will not be built.")
        set(BUILD_EDITOR false)
    else()
        if (UNIX AND NOT APPLE) # Linux
            set(CURSES_LIBRARIES ncursesw)
        endif()
    endif()
endif ()


if (BUILD_EDITOR)
    add_executable(
        rope_editor
        editor/main.cpp
        editor/app_state.cpp
        editor/curses_interface.cpp
        editor/key_mappings.cpp
    )
    target_compile_options(rope_editor PRIVATE -Wall -D_XOPEN_SOURCE_EXTENDED)
    if (CMAKE_COMPILER_IS_GNUCXX)
        target_compile_options(rope_editor PRIVATE -Wno-terminate)
    endif ()
    target_link_libraries(rope_editor text boost ${Boost_LIBRARIES} ${CURSES_LIBRARIES})
    if (ICU_FOUND)
        target_link_libraries(rope_editor ${ICU_LIBRARIES})
    endif ()
    set_property(TARGET rope_editor PROPERTY CXX_STANDARD ${CXX_STD})
    if (clang_on_linux)
        target_link_libraries(rope_editor c++)
    endif ()
endif ()
