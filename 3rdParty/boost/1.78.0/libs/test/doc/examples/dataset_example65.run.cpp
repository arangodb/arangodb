//  (C) Copyright Raffi Enficiaud 2014.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE dataset_example65
#include <boost/test/included/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

namespace bdata = boost::unit_test::data;


BOOST_DATA_TEST_CASE( 
  test1, 
  bdata::make(2),
  singleton)
{
  std::cout 
    << "test 1: " 
    << singleton << std::endl;
  BOOST_TEST(singleton == 2);
}

BOOST_DATA_TEST_CASE( 
  test2, 
  bdata::xrange(3) ^ bdata::make(2),
  xr, singleton)
{
  std::cout 
    << "test 2: " 
    << xr << ", " << singleton << std::endl;
  BOOST_TEST(singleton == 2);
}
//]
