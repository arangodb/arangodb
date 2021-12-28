//  (C) Copyright Raffi Enficiaud 2014.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE dataset_example59
#include <boost/test/included/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

namespace bdata = boost::unit_test::data;

BOOST_DATA_TEST_CASE( test1, bdata::xrange(5) )
{
  std::cout << "test 1: " << sample << std::endl;
  BOOST_TEST((sample <= 4 && sample >= 0));
}

BOOST_DATA_TEST_CASE( 
  test2, 
  bdata::xrange<int>( (bdata::begin=1, bdata::end=10, bdata::step=3)) )
{
  std::cout << "test 2: " << sample << std::endl;
  BOOST_TEST((sample <= 4 && sample >= 0));
}
//]
