//  (C) Copyright Raffi Enficiaud 2014.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE dataset_example64
#include <boost/test/included/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

namespace bdata = boost::unit_test::data;


BOOST_DATA_TEST_CASE( 
  test1, 
  bdata::xrange(2) * bdata::xrange(3), 
  xr1, xr2)
{
  std::cout << "test 1: " << xr1 << ", " << xr2 << std::endl;
  BOOST_TEST((xr1 <= 2 && xr2 <= 3));
}

BOOST_DATA_TEST_CASE( 
  test2, 
  bdata::xrange(3)
  *
  ( bdata::random( 
      bdata::distribution=std::uniform_real_distribution<float>(1, 2)) 
    ^ bdata::xrange(2) 
  ),  
  xr, random_sample, index)
{
  std::cout << "test 2: " 
    << xr << " / " 
    << random_sample << ", " << index
    << std::endl;
  BOOST_TEST(random_sample < 1.7); // 30% chance of failure
}
//]
