#
# Copyright (c) 2018 Mateusz Loskot <mateusz at loskot dot net>
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
foreach(name
  point
  point_concepts)
  add_executable(gil_test_${name} "")
  target_sources(gil_test_${name} PRIVATE ${_boost_gil_headers} ${name}.cpp)
  target_link_libraries(gil_test_${name} PRIVATE Boost::boost)
  add_test(gil.tests.core.${name} gil_test_${name})
endforeach()
