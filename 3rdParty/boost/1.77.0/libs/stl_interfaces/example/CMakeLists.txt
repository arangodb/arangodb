# Copyright (C) 2019 T. Zachary Laine
#
# Distributed under the Boost Software License, Version 1.0. (See
# accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
include_directories(${CMAKE_HOME_DIRECTORY})

include(CTest)

enable_testing()

add_custom_target(run_examples COMMAND ${CMAKE_CTEST_COMMAND} -VV -C ${CMAKE_CFG_INTDIR})

macro(add_sample name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} stl_interfaces)
    set_property(TARGET ${name} PROPERTY CXX_STANDARD ${CXX_STD})
    add_test(${name} ${CMAKE_CURRENT_BINARY_DIR}/${name})
    if (clang_on_linux)
        target_link_libraries(${name} c++)
    endif ()
endmacro()

add_sample(repeated_chars_iterator)
add_sample(filtered_int_iterator)
add_sample(node_iterator)
add_sample(random_access_iterator)
add_sample(interoperability)
add_sample(zip_proxy_iterator)
add_sample(back_insert_iterator)
add_sample(reverse_iterator)

add_sample(drop_while_view)

add_sample(static_vector)
