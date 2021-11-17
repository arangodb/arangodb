# Copyright (C) 2019 T. Zachary Laine
#
# Distributed under the Boost Software License, Version 1.0. (See
# accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
include_directories(${CMAKE_HOME_DIRECTORY})

macro(add_snippets name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} yap)
    if (clang_on_linux)
        target_link_libraries(${name} c++)
    endif ()
endmacro()
