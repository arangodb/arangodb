# Copyright 2018, 2019 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

include(BoostTest OPTIONAL RESULT_VARIABLE HAVE_BOOST_TEST)

if(NOT HAVE_BOOST_TEST)
  return()
endif()

file(GLOB tests RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp)

set(BOOST_TEST_LINK_LIBRARIES Boost::move Boost::config Boost::container Boost::core Boost::static_assert)

foreach(test IN LISTS tests)

  boost_test(SOURCES ${test})

endforeach()
