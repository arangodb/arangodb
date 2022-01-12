# Copyright 2018, 2019 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

include(BoostTest OPTIONAL RESULT_VARIABLE HAVE_BOOST_TEST)

if(NOT HAVE_BOOST_TEST)
  return()
endif()

enable_language(C)

set(tests_common arithmetic array comparison control debug facilities list logical punctuation selection seq slot stringize tuple variadic isempty)
set(tests_common_fail isempty_variadic_standard_failure isempty_variadic_standard_failure2)
set(tests_c ${tests_common})
set(tests_cpp ${tests_common} iteration repetition checkempty vaopt quick)

set(BOOST_TEST_LINK_LIBRARIES Boost::preprocessor)

include_directories(../../..) # for `include <libs/preprocessor/...>` to work

foreach(test IN LISTS tests_c)

  boost_test(TYPE compile SOURCES ${test}.c)

endforeach()

foreach(test IN LISTS tests_cpp)

  boost_test(TYPE compile SOURCES ${test}.cpp)

endforeach()

foreach(test IN LISTS tests_common_fail)

  boost_test(TYPE compile-fail SOURCES ${test}.c)
  boost_test(TYPE compile-fail SOURCES ${test}.cpp)

endforeach()

boost_test(TYPE run SOURCES config_info.cpp)
