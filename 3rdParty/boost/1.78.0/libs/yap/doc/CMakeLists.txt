include_directories(${CMAKE_HOME_DIRECTORY})

macro(add_snippets name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} yap)
    if (clang_on_linux)
        target_link_libraries(${name} c++)
    endif ()
endmacro()

add_snippets(user_macros_snippets)
add_snippets(other_snippets)
