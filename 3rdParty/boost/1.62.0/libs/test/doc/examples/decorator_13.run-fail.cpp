//  (C) Copyright Andrzej Krzemienski 2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE decorator_13
#include <boost/test/included/unit_test.hpp>

namespace utf = boost::unit_test;
namespace fpc = boost::test_tools::fpc;

BOOST_AUTO_TEST_CASE(test1, * utf::tolerance(0.0005))
{
  BOOST_TEST( 0.001 == 0.000 );
  BOOST_TEST( 1.100 == 1.101 );
}

BOOST_AUTO_TEST_CASE(test2, * utf::tolerance(0.005))
{
  BOOST_TEST( 0.001 == 0.000 );
  BOOST_TEST( 1.100 == 1.101 );
}

BOOST_AUTO_TEST_CASE(test3, * utf::tolerance(0.05F))
{
  BOOST_TEST( 0.001 == 0.000 );
  BOOST_TEST( 1.100 == 1.101 );
}

BOOST_AUTO_TEST_CASE(test4, 
  * utf::tolerance(fpc::percent_tolerance(0.05)))
{
  BOOST_TEST( 0.001 == 0.000 );
  BOOST_TEST( 1.100 == 1.101 );
}

BOOST_AUTO_TEST_CASE(test5, 
  * utf::tolerance(fpc::percent_tolerance(0.5)))
{
  BOOST_TEST( 0.001 == 0.000 );
  BOOST_TEST( 1.100 == 1.101 );
}
//]
