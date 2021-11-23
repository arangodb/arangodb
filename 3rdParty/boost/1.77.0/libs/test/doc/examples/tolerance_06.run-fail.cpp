//  Copyright (c) 2015 Boost.Test team
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE tolerance_06
#include <boost/test/included/unit_test.hpp>
namespace utf = boost::unit_test;
namespace tt = boost::test_tools;

BOOST_AUTO_TEST_CASE(test1, * utf::tolerance(0.1415 / 3)) // == 0.047166667
{
  double x = 3.141592404915836;
  // x is 'double' which is tolerance based, 3 is 'int' which is arithmetic:
  // tolerance based comparison will be used.
  // Type of tolerance for this comparison will be boost::common_type<double, int>::type == double
  // Value for this tolerance type is set by the decorator.
  BOOST_TEST(x == 3);
}
//]
