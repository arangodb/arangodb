//  (C) Copyright Raffi Enficiaud 2019.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example83 multicontext
#include <boost/test/included/unit_test.hpp>
#include <cmath>

BOOST_AUTO_TEST_CASE(test_multi_context)
{
  for (int level = 0; level < 10; ++level) {
    int rand_value = std::abs(rand()) % 50;
    BOOST_TEST_CONTEXT("With level " << level, "Random value=" << rand_value){
      for( int j = 1; j < rand_value; j++) {
        BOOST_TEST(level < rand_value);
        rand_value /= 2;
      }
    }
  }
}
//]
