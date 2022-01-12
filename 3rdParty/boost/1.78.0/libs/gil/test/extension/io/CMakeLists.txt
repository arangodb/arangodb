#
# Copyright (c) 2017-2020 Mateusz Loskot <mateusz at loskot dot net>
# All rights reserved.
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
message(STATUS "Boost.GIL: Configuring tests in test/extension/io")

add_executable(test_ext_io_simple "simple_all_formats.cpp")
target_link_libraries(test_ext_io_simple
  PRIVATE
    gil_compile_options
    gil_include_directories
    gil_dependencies)
add_test(NAME t_ext_io_simple COMMAND test_ext_io_simple)

# TODO: Add <format>/CMakeLists.txt configuration for each format ~mloskot
