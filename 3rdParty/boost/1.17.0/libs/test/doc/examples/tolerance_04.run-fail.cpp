//  Copyright (c) 2015 Boost.Test team
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE tolerance_04
#include <boost/test/included/unit_test.hpp>
#include <boost/rational.hpp>
namespace utf = boost::unit_test;
namespace tt = boost::test_tools;

namespace boost { namespace math { namespace fpc {

  template <typename I>
  struct tolerance_based< rational<I> > : boost::true_type{};
  
} } }

typedef boost::rational<int> ratio;

BOOST_AUTO_TEST_CASE(test1, * utf::tolerance(ratio(1, 1000)))
{
  ratio x (1002, 100); // 10.02
  ratio y (1001, 100); // 10.01
  ratio z (1000, 100); // 10.00
  
  BOOST_TEST(x == y);  // irrelevant diff by default
  BOOST_TEST(x == y, tt::tolerance(ratio(1, 2000)));
  
  BOOST_TEST(x != z);  // relevant diff by default
  BOOST_TEST(x != z, tt::tolerance(ratio(2, 1000)));
}
//]
