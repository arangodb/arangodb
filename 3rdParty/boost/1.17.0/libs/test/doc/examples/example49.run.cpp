//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example49
#include <boost/test/included/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
using namespace boost::unit_test;

BOOST_DATA_TEST_CASE( free_test_function, boost::unit_test::data::xrange(1000) )
{
  // sleep(1);
  BOOST_TEST( true /* test assertion */ );
}
//]
