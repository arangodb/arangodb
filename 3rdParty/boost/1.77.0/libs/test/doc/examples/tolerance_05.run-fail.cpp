//  Copyright (c) 2015 Boost.Test team
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE tolerance_05
#include <boost/test/included/unit_test.hpp>
#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>

BOOST_AUTO_TEST_CASE(test, * boost::unit_test::tolerance(0.02))
{
  double                  d1 = 1.00, d2 = 0.99;
  boost::optional<double> o1 = 1.00, o2 = 0.99;

  BOOST_TEST(d1 == d2);   // with tolerance    (double vs. double)
  BOOST_TEST(o1 == o2);   // without tolerance (optional vs. optional)
  BOOST_TEST(o1 == d2);   // without tolerance (optional vs. double)
  BOOST_TEST(*o1 == *o2); // with tolerance    (double vs. double)
}
//]
