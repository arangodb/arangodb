//  (C) Copyright Raffi Enficiaud 2018.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

// checks that the master test suite is not copyable (compilation failure)

#define BOOST_TEST_MODULE "master test suite non copyable"
#include <boost/test/included/unit_test.hpp>
#include <exception>
#include <iostream>

BOOST_AUTO_TEST_CASE( check )
{
  using namespace boost::unit_test;
  master_test_suite_t master_copy = framework::master_test_suite();
  std::cerr << "Test called with " << master_copy.argc << " arguments" << std::endl; // to prevent optimization, just in case it compiles...
  throw std::runtime_error("Should not reach here ");
}
