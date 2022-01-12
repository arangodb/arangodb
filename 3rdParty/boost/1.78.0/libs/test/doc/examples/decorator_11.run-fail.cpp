//  (C) Copyright Andrzej Krzemienski 2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE decorator_11
#include <boost/test/included/unit_test.hpp>
namespace utf = boost::unit_test;

BOOST_AUTO_TEST_CASE(test1, * utf::timeout(2))
{
#ifdef BOOST_SIGACTION_BASED_SIGNAL_HANDLING
  for(;;) {}
  BOOST_TEST(true);
#else
  BOOST_TEST(false);
#endif
}
//]
