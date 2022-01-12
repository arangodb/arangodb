# Copyright 2021 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

include(BoostTest OPTIONAL RESULT_VARIABLE HAVE_BOOST_TEST)

if(NOT HAVE_BOOST_TEST)
  return()
endif()

set(BOOST_TEST_LINK_LIBRARIES Boost::rational)

boost_test(TYPE run SOURCES rational_example.cpp)
boost_test(TYPE run SOURCES rational_test.cpp LINK_LIBRARIES Boost::unit_test_framework)
boost_test(TYPE run SOURCES constexpr_test.cpp COMPILE_FEATURES cxx_constexpr)

boost_test(TYPE compile-fail SOURCES expected_fail_01.cpp)
boost_test(TYPE compile-fail SOURCES expected_fail_02.cpp)
boost_test(TYPE compile-fail SOURCES expected_fail_03.cpp)
boost_test(TYPE compile-fail SOURCES expected_fail_04.cpp)
boost_test(TYPE compile-fail SOURCES expected_fail_05.cpp)
boost_test(TYPE compile-fail SOURCES expected_fail_06.cpp)
boost_test(TYPE compile-fail SOURCES expected_fail_07.cpp)
boost_test(TYPE compile-fail SOURCES expected_fail_08.cpp)
boost_test(TYPE compile-fail SOURCES expected_fail_09.cpp)
boost_test(TYPE compile-fail SOURCES expected_fail_10.cpp)
boost_test(TYPE compile-fail SOURCES expected_fail_11.cpp)

boost_test(TYPE compile SOURCES expected_compile_12.cpp)
